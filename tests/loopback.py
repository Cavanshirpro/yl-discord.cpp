# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Real local HTTP/WebSocket fixture: never contacts Discord or loads credentials."""
import base64
import hashlib
import http.server
import json
import socket
import struct
import subprocess
import sys
import threading
import time
import urllib.parse


class State:
    def __init__(self, scenario):
        self.scenario = scenario
        self.errors = []
        self.requests = []
        self.frames = []
        self.posts = 0
        self.connections = 0
        self.heartbeats = 0
        self.last = {}
        self.lock = threading.Lock()

    def check(self, condition, message):
        if not condition:
            self.errors.append(message)


def send_frame(sock, payload, opcode=1, final=True):
    if isinstance(payload, dict):
        payload = json.dumps(payload, separators=(',', ':')).encode()
    elif isinstance(payload, str):
        payload = payload.encode()
    length = len(payload)
    header = bytes([(0x80 if final else 0) | opcode])
    if length < 126:
        header += bytes([length])
    elif length < 65536:
        header += b'\x7e' + struct.pack('!H', length)
    else:
        header += b'\x7f' + struct.pack('!Q', length)
    sock.sendall(header + payload)


def receive_frame(sock):
    def exact(length):
        result = b''
        while len(result) < length:
            chunk = sock.recv(length - len(result))
            if not chunk:
                raise EOFError()
            result += chunk
        return result
    first, second = exact(2)
    if not second & 0x80:
        raise ValueError('Client WebSocket frame is not masked')
    length = second & 0x7f
    if length == 126:
        length = struct.unpack('!H', exact(2))[0]
    elif length == 127:
        length = struct.unpack('!Q', exact(8))[0]
    if length > 4096:
        raise ValueError('Client Gateway payload exceeds the limit')
    mask = exact(4)
    payload = exact(length)
    return first & 15, bytes(value ^ mask[i % 4] for i, value in enumerate(payload))


def handler_for(state):
    class Handler(http.server.BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass

        def answer(self, code, body, headers=None):
            data = json.dumps(body).encode()
            self.send_response(code)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Content-Length', str(len(data)))
            for key, value in (headers or {}).items():
                self.send_header(key, str(value))
            self.end_headers()
            try:
                self.wfile.write(data)
            except (BrokenPipeError, ConnectionResetError):
                pass

        def do_GET(self):
            if self.headers.get('Upgrade', '').lower() == 'websocket':
                self.websocket()
                return
            state.check(self.headers.get('Authorization') == 'Bot fixture-only-credential', 'Missing bot REST authorization')
            now = time.monotonic()
            state.requests.append(self.path)
            port = self.server.server_address[1]
            if self.path == '/api/v10/gateway/bot':
                self.answer(200, {'url': f'ws://127.0.0.1:{port}/gateway', 'shards': 1,
                                  'session_start_limit': {'remaining': 20, 'total': 1000, 'reset_after': 5000, 'max_concurrency': 1}})
            elif self.path.endswith('/limited'):
                count = state.requests.count(self.path)
                if count == 1:
                    state.last['limited'] = now
                    self.answer(429, {'retry_after': .06, 'global': False}, {'X-RateLimit-Bucket': 'shared'})
                else:
                    state.check(now - state.last['limited'] >= .05, '429 retried before retry_after')
                    if count > 2:
                        state.check(now - state.last['shared'] >= .05, 'Learned shared bucket ignored')
                    state.last['shared'] = now
                    self.answer(200, {}, {'X-RateLimit-Bucket': 'shared', 'X-RateLimit-Remaining': '0', 'X-RateLimit-Reset-After': '.06'})
            elif self.path.endswith('/shared'):
                # Learn the same bucket, then verify a subsequent call uses its reset.
                state.last['shared'] = now
                self.answer(200, {}, {'X-RateLimit-Bucket': 'shared', 'X-RateLimit-Remaining': '0', 'X-RateLimit-Reset-After': '.06'})
            elif self.path.endswith('/global'):
                if state.requests.count(self.path) == 1:
                    state.last['global'] = now
                    self.answer(429, {'retry_after': .06, 'global': True})
                else:
                    state.check(now - state.last['global'] >= .05, 'Global 429 retried early')
                    self.answer(200, {})
            elif self.path.endswith('/after-global'):
                self.answer(200, {})
            elif self.path.endswith('/unauthorized'):
                self.answer(401, {'code': 0, 'message': 'Unauthorized'})
            elif self.path.endswith('/must-not-send'):
                state.errors.append('Request sent after authentication was rejected')
                self.answer(401, {})
            elif self.path.endswith('/slow'):
                time.sleep(.3)
                self.answer(200, {})
            elif self.path.endswith('/long-limit'):
                self.answer(429, {'retry_after': 60, 'global': True})
            else:
                self.answer(404, {})

        def do_POST(self):
            state.check(self.headers.get('Authorization') == 'Bot fixture-only-credential', 'Message authorization missing')
            data = json.loads(self.rfile.read(int(self.headers.get('Content-Length', 0))))
            state.check(data.get('content') == 'Pong! "quoted" \n UTF-8: ü', 'JSON/UTF-8 content changed')
            state.check(data.get('message_reference', {}).get('message_id') in ('100', '101'), 'Reply reference missing')
            state.check(data.get('allowed_mentions') == {'parse': [], 'replied_user': False}, 'Implicit mentions enabled')
            if state.posts == 0:
                # REST is deliberately slower than several Gateway heartbeat intervals.
                time.sleep(.18)
            state.posts += 1
            self.answer(200, {'id': str(200 + state.posts), 'channel_id': '42', 'content': 'reply accepted',
                              'author': {'id': '1', 'username': 'fixture-bot', 'bot': True}})

        def websocket(self):
            query = urllib.parse.parse_qs(urllib.parse.urlsplit(self.path).query)
            state.check(query == {'v': ['10'], 'encoding': ['json']}, 'Gateway version/encoding mismatch')
            key = self.headers.get('Sec-WebSocket-Key', '')
            accept = base64.b64encode(hashlib.sha1((key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').encode()).digest()).decode()
            self.send_response(101)
            self.send_header('Upgrade', 'websocket')
            self.send_header('Connection', 'Upgrade')
            self.send_header('Sec-WebSocket-Accept', accept)
            self.end_headers()
            self.connection.settimeout(4)
            state.connections += 1
            current = state.connections
            try:
                if state.scenario not in ('handshake-timeout', 'close-handshake'):
                    send_frame(self.connection, {'op': 10, 'd': {'heartbeat_interval': 45}})
                command_sent = False
                for _ in range(200):
                    opcode, payload = receive_frame(self.connection)
                    if opcode == 8:
                        return
                    if opcode == 10:
                        continue
                    frame = json.loads(payload)
                    state.frames.append(frame['op'])
                    if frame['op'] == 1:
                        state.heartbeats += 1
                        if state.scenario != 'missing-ack':
                            send_frame(self.connection, {'op': 11})
                        if command_sent and current == 1 and state.scenario == 'live' and state.posts >= 1:
                            send_frame(self.connection, {'op': 7, 'd': None})
                            return
                        continue
                    if frame['op'] in (2, 6):
                        state.check(frame['d']['token'] == 'fixture-only-credential', 'Gateway credential changed')
                        if state.scenario == 'fatal':
                            send_frame(self.connection, struct.pack('!H', 4004), opcode=8)
                            return
                        if frame['op'] == 2:
                            state.check(current == 1 or state.scenario == 'missing-ack', 'Reconnect identified instead of resuming')
                            send_frame(self.connection, {'op': 0, 't': 'READY', 's': 1, 'd': {
                                'user': {'id': '1', 'username': 'fixture-bot', 'bot': True},
                                'session_id': 'fixture-session', 'resume_gateway_url': f'ws://127.0.0.1:{self.server.server_address[1]}/gateway'}})
                        else:
                            state.check(frame['d']['session_id'] == 'fixture-session', 'Resume session ID is incorrect')
                            if state.scenario != 'missing-ack':
                                state.check(frame['d']['seq'] == 2, 'Resume sequence is incorrect')
                            send_frame(self.connection, {'op': 0, 't': 'RESUMED', 's': frame['d']['seq'] + 1, 'd': {}})
                        if state.scenario == 'missing-ack':
                            continue
                        if state.scenario == 'pause-destroy':
                            continue
                        message = {'op': 0, 't': 'MESSAGE_CREATE', 's': 2 if current == 1 else 4, 'd': {
                            'id': '100' if current == 1 else '101', 'channel_id': '42', 'guild_id': '9',
                            'content': '!ping "two words"', 'author': {'id': '7', 'username': 'fixture-user', 'bot': False}}}
                        if state.scenario == 'fragment':
                            encoded = json.dumps(message).encode()
                            send_frame(self.connection, encoded[:20], final=False)
                            send_frame(self.connection, b'probe', opcode=9)
                            send_frame(self.connection, encoded[20:], opcode=0)
                        else:
                            send_frame(self.connection, message)
                        command_sent = True
            except (EOFError, ConnectionResetError, BrokenPipeError):
                pass
            except Exception as error:
                state.errors.append(type(error).__name__ + ': ' + str(error))
            finally:
                self.close_connection = True
    return Handler


def main():
    executable = sys.argv[1]
    for scenario in ('live', 'fragment', 'rest', 'timeout', 'cancel', 'cancel-transfer', 'fatal',
                     'missing-ack', 'handshake-timeout', 'close-handshake', 'pause-destroy'):
        state = State(scenario)
        server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler_for(state))
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        began = time.monotonic()
        try:
            result = subprocess.run([executable, scenario, f'http://127.0.0.1:{server.server_address[1]}/api/v10'],
                                    capture_output=True, text=True, timeout=12, encoding='utf-8')
            print(result.stdout, end='')
            if result.returncode:
                raise RuntimeError(result.stderr)
            if scenario == 'live':
                state.check(state.connections == 2 and state.posts == 2, 'Live reconnect/reply counts incorrect')
                state.check(state.heartbeats >= 3, 'REST blocked Gateway heartbeats')
            if scenario == 'fatal':
                state.check(state.connections == 1, 'Fatal close retried')
            if scenario in ('missing-ack', 'handshake-timeout'):
                state.check(state.connections == 3, 'Missing ACK did not consume bounded reconnect attempts')
            if scenario in ('cancel', 'cancel-transfer', 'close-handshake'):
                state.check(time.monotonic() - began < 2, 'Shutdown did not interrupt rate-limit waiting')
            if state.errors:
                raise RuntimeError('; '.join(state.errors))
        finally:
            server.shutdown()
            server.server_close()
    print('All local HTTP/WebSocket scenarios passed')


if __name__ == '__main__':
    main()
