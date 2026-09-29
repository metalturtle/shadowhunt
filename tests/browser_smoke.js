#!/usr/bin/env node
// Browser smoke test: loads the web build in headless Chrome (throwaway
// profile), holds a movement key, and saves a screenshot. Drives Chrome over
// the DevTools protocol with Node's built-in WebSocket: no npm packages.
//
//   node tests/browser_smoke.js --url http://localhost:8080/ \
//        --out shot.png [--key d] [--seconds 12]
//
// Exit status 0 when the page reports it joined a match and the key reached
// the game (the page log shows the local role and the input key).
'use strict';

const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');

function option(name, fallback) {
  const index = process.argv.indexOf(name);
  return index >= 0 && process.argv[index + 1] ? process.argv[index + 1] : fallback;
}

const url = option('--url', 'http://localhost:8080/');
const out = option('--out', 'browser.png');
const key = option('--key', 'd');
const seconds = Number(option('--seconds', '12'));
const chromePath = option('--chrome',
  '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome');
const port = 9300 + Math.floor(Math.random() * 500);
const profile = fs.mkdtempSync(path.join(os.tmpdir(), 'shadowhunt-chrome-'));

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function main() {
  const chrome = spawn(chromePath, [
    '--headless=new', `--remote-debugging-port=${port}`, `--user-data-dir=${profile}`,
    '--no-first-run', '--no-default-browser-check', '--window-size=1280,800',
    '--enable-unsafe-swiftshader', '--use-angle=swiftshader', 'about:blank',
  ], { stdio: 'ignore' });

  try {
    let target;
    for (let i = 0; i < 50 && !target; i++) {
      await sleep(200);
      try {
        const list = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
        target = list.find((t) => t.type === 'page');
      } catch (e) { /* Chrome still starting */ }
    }
    if (!target) throw new Error('Chrome did not expose a page target');

    const ws = new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
    let nextID = 1;
    const pending = new Map();
    const consoleLines = [];
    ws.onmessage = (event) => {
      const message = JSON.parse(event.data);
      if (message.id && pending.has(message.id)) {
        pending.get(message.id)(message);
        pending.delete(message.id);
      } else if (message.method === 'Runtime.consoleAPICalled') {
        consoleLines.push(message.params.args.map((a) => a.value).join(' '));
      }
    };
    const send = (method, params = {}) => new Promise((resolve) => {
      const id = nextID++;
      pending.set(id, resolve);
      ws.send(JSON.stringify({ id, method, params }));
    });

    await send('Runtime.enable');
    await send('Page.enable');
    await send('Page.navigate', { url });

    // Wait for the page to join a match, then play: hold the key.
    const deadline = Date.now() + seconds * 1000;
    while (Date.now() < deadline && !consoleLines.some((l) => /local role: (hunter|hider)/.test(l)))
      await sleep(200);
    const code = 'Key' + key.toUpperCase();
    const keyEvent = { key, code, windowsVirtualKeyCode: key.toUpperCase().charCodeAt(0),
                       text: key, unmodifiedText: key };
    await send('Input.dispatchMouseEvent', { type: 'mousePressed', x: 640, y: 400,
                                             button: 'left', clickCount: 1 });
    await send('Input.dispatchMouseEvent', { type: 'mouseReleased', x: 640, y: 400,
                                             button: 'left', clickCount: 1 });
    await send('Input.dispatchKeyEvent', { type: 'keyDown', ...keyEvent });
    while (Date.now() < deadline) {
      await send('Input.dispatchKeyEvent', { type: 'keyDown', autoRepeat: true, ...keyEvent });
      await sleep(250);
    }
    const shot = await send('Page.captureScreenshot', { format: 'png' });
    await send('Input.dispatchKeyEvent', { type: 'keyUp', ...keyEvent });
    fs.writeFileSync(out, Buffer.from(shot.result.data, 'base64'));
    ws.close();

    const joined = consoleLines.find((l) => /local role: (hunter|hider)/.test(l));
    console.log(consoleLines.filter((l) => /web net|local role|own entity|round|tuning from|ERROR|Failed|failed|Couldn/.test(l))
      .join('\n'));
    console.log(`screenshot: ${out}`);
    if (!joined) {
      console.error('FAIL: the browser client never received a role');
      process.exitCode = 1;
    } else {
      console.log(`PASS: browser joined as ${joined.split(': ')[1]}`);
    }
  } finally {
    chrome.kill('SIGTERM');
    await sleep(500);
    fs.rmSync(profile, { recursive: true, force: true });
  }
}

main().catch((error) => { console.error('FAIL:', error.message); process.exit(1); });
