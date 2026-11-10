/**
 * @file serial-helpers.js
 * @brief Mock OS serial binding with a stateful TraceLink wire peer.
 */
import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { once } from 'node:events';
import { SerialPortMock } from 'serialport';
import { createMock } from './helpers.js';
import { SerialTransport, crc32 } from '../src/serial.js';
import { TraceLinkBridge } from '../src/bridge.js';

let nextPort = 0;

export async function serialFixture(t, { allowTx = true, timeoutMs = 1000, startupDelayMs = 0 } = {}) {
	const mock = createMock();
	const http = createServer(mock.handler);
	http.listen(0, '127.0.0.1');
	await once(http, 'listening');
	const url = `http://127.0.0.1:${http.address().port}`;
	t.after(() => new Promise(resolve => { http.close(resolve); http.closeAllConnections(); }));
	const path = `/mock/tracelink-${++nextPort}`;
	SerialPortMock.binding.createPort(path, { record: true, maxReadSize: 7 });
	const port = new SerialPortMock({ path, baudRate: 115200, autoOpen: false });
	const transport = new SerialTransport(port, { timeoutMs, startupDelayMs });
	const frames = [];
	const faults = {};
	const pending = [];
	const write = port.write.bind(port);
	const emit = text => { if (port.port?.isOpen) port.port.emitData(text); };
	port.write = (data, callback) => {
		const result = write(data, callback);
		const task = (async () => {
			const frame = String(data).trimEnd();
			frames.push(frame);
			port.emit('wire-write', frame);
			const match = frame.match(/^TL1 ([a-f0-9]{16}) (.*) \*([a-f0-9]{8})$/);
			assert.ok(match, 'Host must use TL1 framing.');
			assert.equal(crc32(frame.slice(0, -10)), match[3]);
			const [, id, payload] = match;
			if (faults.noReply) return;
			if (faults.oversized) { emit('x'.repeat(65537) + '\n'); return; }
			if (faults.badJson) { emit(`TL1 ${id} 200 not-json\n`); return; }
			if (faults.malformed) { emit(`TL1 ${id} bad-status nope\n`); return; }
			if (faults.disconnect) { port.emit('close'); return; }
			const control = payload.match(/^control ([a-f0-9]{8}) (.*)$/);
			const response = control
				? await fetch(`${url}/api/control`, {
					method: 'POST', headers: { 'X-TraceLink': control[1] },
					body: new URLSearchParams(control[2])
				})
				: await fetch(`${url}/api/events?after=${payload.slice(7)}`);
			const body = await response.text();
			if (faults.muteControl && control) return;
			if (faults.responseLength) {
				emit(`TL1 ${id} ${response.status} ${body}`.padEnd(faults.responseLength, ' ') + '\n');
				return;
			}
			const fake = `TL1 0000000000000000 200 {"wrong":true}\n`;
			emit(`rst:0x1 (POWERON_RESET)\n#1 10 ms RS232 RX 30 31\n${fake}TL1 ${id} ${response.status} ${body}\r\n`);
		})();
		pending.push(task);
		return result;
	};
	t.after(async () => { await Promise.all(pending); await transport.close(); });
	return {
		...mock, path, port, transport, frames, wireFaults: faults, emit,
		bridge: new TraceLinkBridge({ allowTx, transport })
	};
}
