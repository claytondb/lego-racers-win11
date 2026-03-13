#!/usr/bin/env python3
"""
LEGO Racers Online Multiplayer Relay Server
Handles TCP lobby/signaling and UDP game state relay
"""

import asyncio
import logging
import logging.handlers
import time
import sys
import os
import argparse
from typing import Dict, Set, Optional, Tuple
from dataclasses import dataclass, field
from collections import defaultdict
import signal
from datetime import datetime, timedelta
import json

# Configuration
TCP_HOST = "0.0.0.0"
TCP_PORT = 3031
UDP_HOST = "0.0.0.0"
UDP_PORT = 3030
MAX_PLAYERS_PER_ROOM = 6
MIN_PLAYERS_TO_START = 2
ROOM_CLEANUP_TIMEOUT = 60  # seconds
PING_INTERVAL = 30  # seconds
PING_TIMEOUT = 10  # seconds
RATE_LIMIT_PACKETS = 100  # packets per second per client
RATE_LIMIT_WINDOW = 1.0  # seconds

# Setup logging
LOG_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)))
os.makedirs(LOG_DIR, exist_ok=True)

log_formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger('LegoServer')
logger.setLevel(logging.DEBUG)

console_handler = logging.StreamHandler()
console_handler.setLevel(logging.INFO)
console_handler.setFormatter(log_formatter)
logger.addHandler(console_handler)

file_handler = logging.handlers.RotatingFileHandler(
    os.path.join(LOG_DIR, 'server.log'),
    maxBytes=10*1024*1024, backupCount=5
)
file_handler.setLevel(logging.DEBUG)
file_handler.setFormatter(log_formatter)
logger.addHandler(file_handler)


@dataclass
class PlayerState:
    nickname: str
    addr: Tuple[str, int]
    tcp_writer: Optional[asyncio.StreamWriter] = None
    room_id: Optional[str] = None
    is_ready: bool = False
    last_activity: float = field(default_factory=time.time)
    last_ping_sent: float = 0
    ping_acked: bool = True
    packet_count: int = 0
    packet_window_start: float = field(default_factory=time.time)
    created_at: float = field(default_factory=time.time)

    def get_rate(self):
        elapsed = time.time() - self.packet_window_start
        return self.packet_count / elapsed if elapsed > 0 else 0

    def reset_rate_window(self):
        self.packet_count = 0
        self.packet_window_start = time.time()

    def update_activity(self):
        self.last_activity = time.time()


@dataclass
class Room:
    room_id: str
    name: str
    host_id: str
    players: Dict[str, PlayerState] = field(default_factory=dict)
    created_at: float = field(default_factory=time.time)
    started: bool = False
    last_activity: float = field(default_factory=time.time)

    def get_status(self):
        return json.dumps({
            'room_id': self.room_id,
            'name': self.name,
            'host_id': self.host_id,
            'players': list(self.players.keys()),
            'player_count': len(self.players),
            'max_players': MAX_PLAYERS_PER_ROOM,
            'started': self.started,
        })

    def is_empty(self):
        return len(self.players) == 0

    def update_activity(self):
        self.last_activity = time.time()


class LegoRacersServer:
    def __init__(self):
        self.players: Dict[str, PlayerState] = {}
        self.rooms: Dict[str, Room] = {}
        self.player_ids: Dict[Tuple[str, int], str] = {}
        self.udp_transport = None
        self.tcp_server = None
        self.running = True
        self.stats = {
            'total_connections': 0,
            'total_rooms_created': 0,
            'total_packets_relayed': 0,
            'start_time': time.time()
        }

    async def start(self):
        logger.info("Starting LEGO Racers Relay Server...")
        self.tcp_server = await asyncio.start_server(
            self.handle_tcp_connection, TCP_HOST, TCP_PORT)
        logger.info(f"TCP listening on {TCP_HOST}:{TCP_PORT}")

        loop = asyncio.get_event_loop()
        self.udp_transport, _ = await loop.create_datagram_endpoint(
            lambda: LegoUDPProtocol(self),
            local_addr=(UDP_HOST, UDP_PORT))
        logger.info(f"UDP listening on {UDP_HOST}:{UDP_PORT}")

        asyncio.create_task(self.cleanup_empty_rooms())
        asyncio.create_task(self.ping_clients())

        logger.info(f"Server ready. TCP:{TCP_PORT} UDP:{UDP_PORT}")
        async with self.tcp_server:
            await self.tcp_server.serve_forever()

    async def handle_tcp_connection(self, reader, writer):
        addr = writer.get_extra_info('peername')
        player_id = f"{addr[0]}:{addr[1]}"
        self.stats['total_connections'] += 1
        logger.info(f"New connection: {player_id}")
        try:
            while self.running:
                try:
                    data = await asyncio.wait_for(reader.readline(), timeout=90)
                except asyncio.TimeoutError:
                    break
                if not data:
                    break
                msg = data.decode('utf-8', errors='ignore').strip()
                if not msg:
                    continue
                if player_id not in self.players:
                    self.players[player_id] = PlayerState(nickname="Unknown", addr=addr, tcp_writer=writer)
                    self.player_ids[addr] = player_id
                player = self.players[player_id]
                player.tcp_writer = writer
                player.update_activity()
                await self.handle_message(player_id, msg, player, writer)
        except Exception as e:
            logger.error(f"TCP error {player_id}: {e}")
        finally:
            await self.disconnect_player(player_id)

    async def handle_message(self, player_id, message, player, writer):
        parts = message.split('|', 2)
        ptype = parts[0] if parts else ""
        content = parts[1] if len(parts) > 1 else ""

        if ptype == "CONNECT":
            player.nickname = content
            logger.info(f"{player_id} is '{player.nickname}'")
            await self.send(writer, "OK|Connected")
        elif ptype == "DISCONNECT":
            await self.disconnect_player(player_id)
        elif ptype == "CREATE_ROOM":
            await self.create_room(player_id, player, content, writer)
        elif ptype == "JOIN_ROOM":
            await self.join_room(player_id, player, content, writer)
        elif ptype == "ROOM_LIST":
            await self.list_rooms(writer)
        elif ptype == "READY":
            player.is_ready = True
            await self.send(writer, "READY_ACK|")
            if player.room_id:
                await self.broadcast_room_update(player.room_id)
        elif ptype == "START":
            await self.start_race(player_id, player, writer)
        elif ptype == "CHAT":
            if player.room_id:
                await self.broadcast_to_room(player.room_id, f"CHAT|{player.nickname}|{content}")
        elif ptype == "PING":
            player.ping_acked = True
            await self.send(writer, f"PONG|{content}")
        else:
            logger.warning(f"Unknown packet: {ptype}")

    async def create_room(self, player_id, player, name, writer):
        if player.room_id:
            await self.send(writer, "ERROR|Already in a room"); return
        room_id = f"room_{int(time.time()*1000)%1000000}"
        room = Room(room_id=room_id, name=name, host_id=player_id)
        room.players[player_id] = player
        self.rooms[room_id] = room
        player.room_id = room_id
        self.stats['total_rooms_created'] += 1
        logger.info(f"Room created: {room_id} '{name}' by {player.nickname}")
        await self.send(writer, f"ROOM_CREATED|{room_id}")
        await self.broadcast_room_update(room_id)

    async def join_room(self, player_id, player, room_id, writer):
        if player.room_id:
            await self.send(writer, "ERROR|Already in a room"); return
        if room_id not in self.rooms:
            await self.send(writer, "ERROR|Room not found"); return
        room = self.rooms[room_id]
        if room.started:
            await self.send(writer, "ERROR|Race already started"); return
        if len(room.players) >= MAX_PLAYERS_PER_ROOM:
            await self.send(writer, "ERROR|Room full"); return
        room.players[player_id] = player
        player.room_id = room_id
        room.update_activity()
        logger.info(f"{player.nickname} joined {room_id}")
        await self.send(writer, f"JOINED_ROOM|{room_id}")
        await self.broadcast_room_update(room_id)

    async def list_rooms(self, writer):
        rooms_list = []
        for room_id, room in self.rooms.items():
            if not room.started and len(room.players) < MAX_PLAYERS_PER_ROOM:
                rooms_list.append(json.dumps({
                    'room_id': room_id, 'name': room.name,
                    'players': len(room.players), 'max': MAX_PLAYERS_PER_ROOM
                }))
        await self.send(writer, "ROOM_LIST|" + ";".join(rooms_list))

    async def start_race(self, player_id, player, writer):
        if not player.room_id:
            await self.send(writer, "ERROR|Not in a room"); return
        room = self.rooms[player.room_id]
        if room.host_id != player_id:
            await self.send(writer, "ERROR|Only host can start"); return
        if len(room.players) < MIN_PLAYERS_TO_START:
            await self.send(writer, f"ERROR|Need {MIN_PLAYERS_TO_START}+ players"); return
        room.started = True
        logger.info(f"Race started in {player.room_id}")
        await self.broadcast_to_room(room.room_id, "START_RACE|")

    async def broadcast_room_update(self, room_id):
        if room_id not in self.rooms: return
        room = self.rooms[room_id]
        await self.broadcast_to_room(room_id, f"ROOM_UPDATE|{room.get_status()}")

    async def broadcast_to_room(self, room_id, message):
        if room_id not in self.rooms: return
        dead = []
        for pid, p in self.rooms[room_id].players.items():
            try:
                if p.tcp_writer: await self.send(p.tcp_writer, message)
            except:
                dead.append(pid)
        for pid in dead:
            await self.disconnect_player(pid)

    async def send(self, writer, message):
        try:
            writer.write((message + '\n').encode('utf-8'))
            await writer.drain()
        except:
            pass

    async def disconnect_player(self, player_id):
        if player_id not in self.players: return
        player = self.players[player_id]
        logger.info(f"Disconnecting {player.nickname}")
        if player.room_id and player.room_id in self.rooms:
            room = self.rooms[player.room_id]
            if player_id in room.players:
                del room.players[player_id]
            if room.host_id == player_id:
                await self.broadcast_to_room(player.room_id, "ROOM_CLOSED|Host left")
                del self.rooms[player.room_id]
            else:
                await self.broadcast_room_update(player.room_id)
        if player.tcp_writer:
            try: player.tcp_writer.close()
            except: pass
        if player.addr in self.player_ids:
            del self.player_ids[player.addr]
        del self.players[player_id]

    async def cleanup_empty_rooms(self):
        while self.running:
            await asyncio.sleep(ROOM_CLEANUP_TIMEOUT)
            now = time.time()
            for rid in [rid for rid, r in self.rooms.items()
                        if r.is_empty() and now - r.last_activity > ROOM_CLEANUP_TIMEOUT]:
                logger.info(f"Cleaning up empty room {rid}")
                del self.rooms[rid]

    async def ping_clients(self):
        while self.running:
            await asyncio.sleep(PING_INTERVAL)
            now = time.time()
            dead = []
            for pid, p in self.players.items():
                try:
                    if p.tcp_writer:
                        if p.ping_acked:
                            p.ping_acked = False
                            p.last_ping_sent = now
                            await self.send(p.tcp_writer, f"PING|{int(now*1000)}")
                        elif now - p.last_ping_sent > PING_TIMEOUT:
                            dead.append(pid)
                except:
                    dead.append(pid)
            for pid in dead:
                await self.disconnect_player(pid)

    def print_stats(self):
        uptime = int(time.time() - self.stats['start_time'])
        print(f"\n=== LEGO Racers Server Stats ===")
        print(f"Uptime: {uptime//3600}h {(uptime%3600)//60}m {uptime%60}s")
        print(f"Connections: {self.stats['total_connections']}")
        print(f"Rooms created: {self.stats['total_rooms_created']}")
        print(f"UDP packets relayed: {self.stats['total_packets_relayed']}")
        print(f"Active players: {len(self.players)}")
        print(f"Active rooms: {len(self.rooms)}")
        print("================================\n")


class LegoUDPProtocol(asyncio.DatagramProtocol):
    def __init__(self, server):
        self.server = server
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport

    def datagram_received(self, data, addr):
        try:
            player_id = self.server.player_ids.get(addr)
            if not player_id: return
            player = self.server.players.get(player_id)
            if not player or not player.room_id: return

            # Rate limit
            player.packet_count += 1
            if time.time() - player.packet_window_start > RATE_LIMIT_WINDOW:
                player.reset_rate_window()
            if player.get_rate() > RATE_LIMIT_PACKETS: return

            player.update_activity()
            room = self.server.rooms.get(player.room_id)
            if not room: return

            # Relay to all other players in room
            relay = player_id.encode('utf-8') + b'|' + data
            for other_id, other in room.players.items():
                if other_id != player_id:
                    try:
                        self.transport.sendto(relay, other.addr)
                    except: pass
            self.server.stats['total_packets_relayed'] += 1
        except Exception as e:
            logger.error(f"UDP error from {addr}: {e}")

    def error_received(self, exc):
        logger.error(f"UDP error: {exc}")


async def main():
    parser = argparse.ArgumentParser(description='LEGO Racers Relay Server')
    parser.add_argument('--host', default='0.0.0.0')
    parser.add_argument('--tcp-port', type=int, default=TCP_PORT)
    parser.add_argument('--udp-port', type=int, default=UDP_PORT)
    args = parser.parse_args()

    server = LegoRacersServer()

    try:
        await server.start()
    except KeyboardInterrupt:
        server.print_stats()
        logger.info("Shutting down")


if __name__ == "__main__":
    print("LEGO Racers Relay Server")
    print(f"TCP port: {TCP_PORT}  UDP port: {UDP_PORT}")
    print("Press Ctrl+C to stop\n")
    asyncio.run(main())
