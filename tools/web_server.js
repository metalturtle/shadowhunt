#!/usr/bin/env node
// Serves the browser build and relays browser WebSockets to the UDP game
// server. Each browser gets its own UDP socket, so the game server sees it as
// an ordinary client. No npm packages: the WebSocket framing is done here.
//
//   node tools/web_server.js [--dir build-web/web] [--port 8080]
//                            [--game-host 127.0.0.1] [--game-port 8000]
//
// Then open http://localhost:8080/ (use ?server=ws://host:port/ws to point a
// page at another relay).
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const dgram = require('dgram');

function option(name, fallback) {
  const index = process.argv.indexOf(name);
  return index >= 0 && process.argv[index + 1] ? process.argv[index + 1] : fallback;
}

const root = path.resolve(option('--dir', path.join(__dirname, '..', 'build-web', 'web')));
const httpPort = Number(option('--port', '8080'));
const gameHost = option('--game-host', '127.0.0.1');
const gamePort = Number(option('--game-port', '8000'));
const MAX_MESSAGE = 16384;

const types = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript',
  '.wasm': 'application/wasm',
  '.data': 'application/octet-stream',
  '.json': 'application/json',
  '.png': 'image/png',
};

const server = http.createServer((request, response) => {
  const urlPath = decodeURIComponent(new URL(request.url, 'http://x').pathname);
  const relative = urlPath === '/' ? 'shadowhunt.html' : urlPath.replace(/^\/+/, '');
  const file = path.resolve(root, relative);
  if (!file.startsWith(root + path.sep)) {
    response.writeHead(403).end('forbidden');
    return;
  }
  fs.readFile(file, (error, data) => {
    if (error) {
      response.writeHead(404).end('not found');
      return;
    }
    response.writeHead(200, {
      'Content-Type': types[path.extname(file)] || 'application/octet-stream',
      'Cache-Control': 'no-cache',
    });
    response.end(data);
  });
});

// Minimal RFC 6455 server side: binary messages only, no extensions.
function encodeFrame(payload, opcode = 0x2) {
  const length = payload.length;
  let header;
  if (length < 126) {
    header = Buffer.from([0x80 | opcode, length]);
  } else if (length < 65536) {
    header = Buffer.alloc(4);
    header[0] = 0x80 | opcode;
    header[1] = 126;
    header.writeUInt16BE(length, 2);
  } else {
    header = Buffer.alloc(10);
    header[0] = 0x80 | opcode;
    header[1] = 127;
    header.writeBigUInt64BE(BigInt(length), 2);
  }
  return Buffer.concat([header, payload]);
}

let nextBrowserID = 1;

server.on('upgrade', (request, socket) => {
  const key = request.headers['sec-websocket-key'];
  if (new URL(request.url, 'http://x').pathname !== '/ws' || !key) {
    socket.end('HTTP/1.1 400 Bad Request\r\n\r\n');
    return;
  }
  const accept = crypto.createHash('sha1')
    .update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
  socket.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n' +
               'Connection: Upgrade\r\nSec-WebSocket-Accept: ' + accept + '\r\n\r\n');
  socket.setNoDelay(true);

  const id = nextBrowserID++;
  const udp = dgram.createSocket('udp4');
  let closed = false;
  let pending = Buffer.alloc(0);

  const close = (reason) => {
    if (closed) return;
    closed = true;
    console.log(`browser ${id} left (${reason})`);
    try { udp.close(); } catch (e) { /* already closed */ }
    socket.destroy();
  };

  udp.on('message', (datagram) => {
    if (!closed) socket.write(encodeFrame(datagram));
  });
  udp.on('error', (error) => close('udp ' + error.message));
  udp.bind(0, () => console.log(`browser ${id} joined via UDP port ${udp.address().port}`));

  socket.on('data', (chunk) => {
    pending = Buffer.concat([pending, chunk]);
    while (pending.length >= 2) {
      const opcode = pending[0] & 0x0f;
      const masked = (pending[1] & 0x80) !== 0;
      let length = pending[1] & 0x7f;
      let offset = 2;
      if (length === 126) {
        if (pending.length < 4) return;
        length = pending.readUInt16BE(2);
        offset = 4;
      } else if (length === 127) {
        if (pending.length < 10) return;
        length = Number(pending.readBigUInt64BE(2));
        offset = 10;
      }
      if (!masked || length > MAX_MESSAGE) {
        close('protocol error');
        return;
      }
      if (pending.length < offset + 4 + length) return;
      const mask = pending.subarray(offset, offset + 4);
      const payload = Buffer.from(pending.subarray(offset + 4, offset + 4 + length));
      for (let i = 0; i < payload.length; i++) payload[i] ^= mask[i & 3];
      pending = pending.subarray(offset + 4 + length);

      if (opcode === 0x2) {
        udp.send(payload, gamePort, gameHost);
      } else if (opcode === 0x8) {
        close('browser closed');
        return;
      } else if (opcode === 0x9) {
        socket.write(encodeFrame(payload, 0xA));   // ping -> pong
      }
    }
  });
  socket.on('close', () => close('socket closed'));
  socket.on('error', (error) => close(error.message));
});

server.listen(httpPort, () => {
  console.log(`serving ${root} on http://localhost:${httpPort}/`);
  console.log(`relaying ws://localhost:${httpPort}/ws to UDP ${gameHost}:${gamePort}`);
});
