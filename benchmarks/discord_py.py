# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
"""Offline synthetic dispatch/reply fixture. Never logs in or contacts Discord."""
import argparse
import asyncio
import ctypes
import json
import logging
import math
import platform
import sys
import time
from types import SimpleNamespace

import discord
from discord.ext import commands

PINNED_VERSION = '2.7.1'
FIRST_ID = 100_000_000_000_000_000


def peak_memory():
    if sys.platform == 'win32':
        class Counters(ctypes.Structure):
            _fields_ = [('cb', ctypes.c_ulong), ('faults', ctypes.c_ulong)] + [
                (name, ctypes.c_size_t) for name in ('peak_ws', 'ws', 'peak_paged', 'paged',
                                                   'peak_nonpaged', 'nonpaged', 'pagefile', 'peak_pagefile')]
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        psapi = ctypes.WinDLL('psapi', use_last_error=True)
        kernel.GetCurrentProcess.restype = ctypes.c_void_p
        psapi.GetProcessMemoryInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(Counters), ctypes.c_ulong]
        counters = Counters()
        counters.cb = ctypes.sizeof(counters)
        if not psapi.GetProcessMemoryInfo(kernel.GetCurrentProcess(), ctypes.byref(counters), counters.cb):
            raise ctypes.WinError(ctypes.get_last_error())
        return counters.peak_ws
    import resource
    value = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    return value if sys.platform == 'darwin' else value * 1024


def payload(identifier, content, author):
    return {'id': str(identifier), 'channel_id': '42', 'content': content, 'type': 0,
            'author': author, 'attachments': [], 'embeds': [], 'mentions': [], 'mention_roles': [],
            'timestamp': '2026-10-29T00:00:00+00:00', 'edited_timestamp': None,
            'pinned': False, 'tts': False, 'flags': 0}


async def measure(count, batch):
    class FixtureBot(commands.Bot):
        async def on_command_error(self, ctx, error):
            self.failure = error
            self.complete.set()

        async def on_error(self, event, *args, **kwargs):
            self.failure = sys.exc_info()[1]
            self.complete.set()

    bot = FixtureBot(command_prefix='!', intents=discord.Intents(4609), help_command=None)
    starts = [0] * count
    latencies = []
    requests = []
    messages = {}
    bot.failure = None
    bot.complete = asyncio.Event()
    target = 0
    author = {'id': '7', 'username': 'fixture-user', 'discriminator': '0', 'avatar': None, 'bot': False}
    self_user = {'id': '1', 'username': 'fixture-bot', 'discriminator': '0', 'avatar': None, 'bot': True}

    async def send_message(channel_id, *, params):
        if channel_id != 42 or params.files or params.payload['content'] != 'Pong!':
            raise RuntimeError('Unexpected offline reply payload')
        requests.append(dict(params.payload))
        return payload(len(requests), 'Pong!', self_user)

    # Pinned adapter seam: replace HTTP before dispatch and seed a synthetic local user.
    # Nothing calls login(), connect(), run(token), or HTTPClient.request().
    bot.http.send_message = send_message
    async def network_forbidden(*args, **kwargs):
        raise RuntimeError('Network access is disabled in this benchmark')
    bot.http.request = network_forbidden
    bot.http.ws_connect = network_forbidden
    bot._connection.user = discord.ClientUser(state=bot._connection, data=self_user)
    channel = SimpleNamespace(id=42, guild=None)

    @bot.command()
    async def ping(ctx):
        response = await ctx.reply('Pong!', allowed_mentions=discord.AllowedMentions.none(), mention_author=False, nonce=1)
        messages[response.id] = response
        latencies.append((time.perf_counter_ns() - starts[ctx.message.id - FIRST_ID]) / 1000)
        if len(latencies) == target:
            bot.complete.set()

    seconds = 0.0
    async with bot:
        for offset in range(0, count, batch):
            size = min(batch, count - offset)
            inputs = [discord.Message(state=bot._connection, channel=channel,
                      data=payload(FIRST_ID + offset + i, '!ping', author)) for i in range(size)]
            target = offset + size
            bot.complete.clear()
            begin = time.perf_counter_ns()
            for message in inputs:
                starts[message.id - FIRST_ID] = time.perf_counter_ns()
                bot.dispatch('message', message)
            await asyncio.wait_for(bot.complete.wait(), timeout=30)
            await asyncio.sleep(0)  # Drain command-completion events before ending this batch interval.
            seconds += (time.perf_counter_ns() - begin) / 1e9
            if bot.failure:
                raise RuntimeError('discord.py benchmark callback failed') from bot.failure
    if len(latencies) != count or len(requests) != count or len(messages) != count:
        raise RuntimeError('Incomplete offline dispatch/reply count')
    return seconds, sorted(latencies)


async def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('iterations', type=int, nargs='?', default=10000)
    parser.add_argument('batch_size', type=int, nargs='?', default=128)
    args = parser.parse_args()
    if not 1 <= args.iterations <= 200000 or not 1 <= args.batch_size <= 4096:
        parser.error('Use count 1..200000, batch 1..4096')
    if discord.__version__ != PINNED_VERSION:
        parser.error(f'This adapter is pinned to discord.py {PINNED_VERSION}')
    logging.getLogger('discord').addHandler(logging.NullHandler())
    await measure(min(args.iterations, 256), args.batch_size)
    seconds, latencies = await measure(args.iterations, args.batch_size)
    print(json.dumps({'implementation': 'discord.py', 'version': discord.__version__,
        'python': platform.python_version(), 'configuration': 'CPython default GC',
        'iterations': args.iterations, 'batch_size': args.batch_size, 'dispatch_seconds': seconds,
        'commands_per_second': args.iterations / seconds,
        'latency_us': {label: latencies[math.ceil(len(latencies) * fraction) - 1]
                       for label, fraction in (('p50', .5), ('p95', .95), ('p99', .99))},
        'peak_resident_bytes': peak_memory()}))


if __name__ == '__main__':
    asyncio.run(main())
