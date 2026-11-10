/**
 * @file serial.test.js
 * @brief USB/COM behavior through the real serial stream and simulated OS driver.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { once } from 'node:events';
import { Client, StreamableHTTPClientTransport } from '@modelcontextprotocol/client';
import { SerialPortMock } from 'serialport';
import { serialFixture } from './serial-helpers.js';
import { SerialTransport, crc32 } from '../src/serial.js';
import { createBridge } from '../src/connection.js';
import { startHttp } from '../src/http.js';

const send = { bus: 'RS232', encoding: 'hex', data: '01 03 FF', confirm: true };

test('USB CRC matches an independent published check and Python zlib frame', () => {
	assert.equal(crc32('123456789'), 'cbf43926');
	assert.equal(crc32('TL1 0123456789abcdef events 0'), '18aab641');
});

test('COM reads fragmented replies amid boot logs, preserving pagination and reboot handling', async t => {
	const { bridge, frames, state } = await serialFixture(t);
	const first = await bridge.read();
	assert.equal(first.events.length, 32);
	assert.equal(first.nextCursor, 32);
	const next = await bridge.read({ after: first.nextCursor, boot: first.boot });
	assert.equal(next.events[0].seq, 33);
	state.boot = 'abcdef12';
	const reset = await bridge.read({ after: 1000, boot: first.boot });
	assert.equal(reset.reset, true);
	assert.equal(reset.nextCursor, 32);
	assert.equal(frames.length, 4);
	assert.ok(frames.every(frame => frame.includes(' events ')));
});

test('USB uses the same arming, exact payload, CAN and disarming gates as HTTP', async t => {
	const { bridge, calls, frames, state } = await serialFixture(t);
	await assert.rejects(bridge.control('send', send), /disarmed/);
	await bridge.control('arm', { confirm: true });
	await bridge.control('send', { ...send, bus: 'RS485', encoding: 'text', data: 'à&=+\r\n' });
	assert.equal(state.events.at(-1).hex, 'C3 A0 26 3D 2B 0D 0A');
	assert.equal(calls.at(-1).headers['x-tracelink'], state.boot);
	const result = await bridge.control('send', {
		...send, bus: 'CAN', id: '1FFFFFFF', extended: true, rtr: true, dlc: 8, data: ''
	});
	assert.equal(result.deliveryConfirmed, false);
	assert.equal(calls.at(-1).form.rtr, '1');
	assert.equal(calls.at(-1).form.dlc, '8');
	await bridge.control('disarm');
	assert.equal((await bridge.status()).armed, false);
	assert.equal(calls.filter(call => call.method === 'POST').length, 4);
	assert.ok(frames.some(frame => frame.includes('%C3%A0%26%3D%2B%0D%0A')));
});

test('COM stays unopened when TX is blocked or a payload is invalid', async t => {
	const { bridge, port, frames } = await serialFixture(t, { allowTx: false });
	await assert.rejects(bridge.control('arm', { confirm: true }), /TX blocked/);
	await assert.rejects(bridge.control('send', send), /TX blocked/);
	await assert.rejects(bridge.control('send', { ...send, data: 'F' }));
	assert.equal(port.isOpen, false);
	assert.equal(frames.length, 0);
});

for (const fault of ['badJson', 'oversized', 'malformed', 'disconnect', 'noReply']) {
	test(`COM ${fault} fails without falling back to Wi-Fi or resending`, async t => {
		const { bridge, wireFaults, frames } = await serialFixture(t, { timeoutMs: 60 });
		wireFaults[fault] = true;
		await assert.rejects(bridge.status());
		assert.equal(frames.length, 1);
	});
}

test('USB timeout after execution is ambiguous; late replies do not satisfy another request', async t => {
	const { bridge, frames, state, wireFaults, emit } = await serialFixture(t, { timeoutMs: 100 });
	await bridge.control('arm', { confirm: true });
	wireFaults.muteControl = true;
	await assert.rejects(bridge.control('send', send), /outcome may be unknown.*Do not retry/);
	const previous = frames.at(-1).split(' ')[1];
	assert.equal(state.events.filter(event => event.tx).length, 1);
	delete wireFaults.muteControl;
	emit(`TL1 ${previous} 202 old-response\n`);
	assert.equal((await bridge.status()).armed, true);
	assert.equal(state.events.filter(event => event.tx).length, 1);
});

test('USB rejects overlapping controls and refuses reconnect after disconnect', async t => {
	const { bridge, transport, port, wireFaults, frames } = await serialFixture(t, { timeoutMs: 60 });
	wireFaults.noReply = true;
	const pending = bridge.status();
	await assert.rejects(transport.request('api/events?after=0'), /in progress/);
	await assert.rejects(pending, /timed out/);
	port.emit('close');
	const count = frames.length;
	await assert.rejects(bridge.status(), /disconnected/);
	assert.equal(frames.length, count);
});

test('HTTP MCP clients can use the COM backend with unchanged tool names', async t => {
	const { bridge, state } = await serialFixture(t);
	const token = 'serial-local-test-token-not-a-secret-12345';
	const http = await startHttp(bridge, { port: 0, token });
	t.after(() => new Promise(resolve => { http.close(resolve); http.closeAllConnections(); }));
	const client = new Client({ name: 'usb-test', version: '1.0.0' });
	t.after(() => client.close());
	await client.connect(new StreamableHTTPClientTransport(
		new URL(`http://127.0.0.1:${http.address().port}/mcp`),
		{ requestInit: { headers: { Authorization: `Bearer ${token}` } } }
	));
	assert.equal((await client.listTools()).tools.length, 5);
	for (const [name, args] of [
		['tracelink_status', {}], ['tracelink_arm_tx', { confirm: true }],
		['tracelink_send', send], ['tracelink_disarm_tx', {}]
	]) {
		const response = await client.callTool({ name, arguments: args });
		assert.notEqual(response.isError, true, JSON.stringify(response));
	}
	assert.equal(state.events.filter(event => event.tx).length, 1);
	assert.equal(state.armed, false);
});

test('serial selection is explicit, locked, fixed baud, lazy and mutually exclusive with HTTP URL', async t => {
	let options;
	let port;
	const bridge = await createBridge({ TRACELINK_SERIAL_PORT: '/mock/selection' }, value => {
		options = value;
		SerialPortMock.binding.createPort(value.path, { record: true });
		port = new SerialPortMock(value);
		return port;
	});
	assert.deepEqual(options, {
		path: '/mock/selection', baudRate: 115200, autoOpen: false, lock: true, hupcl: false
	});
	assert.equal(port.isOpen, false);
	await bridge.close();
	assert.equal(port.isOpen, false);
	for (const env of [
		{ TRACELINK_SERIAL_PORT: '' },
		{ TRACELINK_SERIAL_PORT: 'COM7', TRACELINK_URL: 'http://192.168.4.1' },
		{ TRACELINK_ALLOW_TX: 'yes' }
	]) await assert.rejects(createBridge(env, () => assert.fail('Must not create a port.')));
});

test('serial open failure is reported without probing other ports', async () => {
	const port = new SerialPortMock({ path: '/mock/nonexistent-port', baudRate: 115200, autoOpen: false });
	const transport = new SerialTransport(port, { startupDelayMs: 0 });
	await assert.rejects(transport.request('api/events?after=0'), /does not exist/);
	await transport.close();
});

test('closing while opening or awaiting a reply releases the port and never sends disarm', async t => {
	const { transport, port, frames } = await serialFixture(t, { startupDelayMs: 30 });
	const pending = transport.request('api/events?after=0');
	const rejected = assert.rejects(pending, /closed/);
	await once(port, 'open');
	await transport.close();
	await rejected;
	assert.equal(port.isOpen, false);
	assert.equal(frames.length, 0);
});

test('COM handles a full 32-record, 64-byte batch and the exact 64 KiB line boundary', async t => {
	const { bridge, state, wireFaults } = await serialFixture(t);
	state.events = state.events.slice(0, 32).map(event => ({ ...event, hex: Array(64).fill('FF').join(' ') }));
	state.latest = 32;
	const batch = await bridge.read();
	assert.equal(batch.events.length, 32);
	assert.equal(batch.events.at(-1).hex.length, 191);
	wireFaults.responseLength = 65536;
	assert.equal((await bridge.read()).events.length, 32);
	wireFaults.responseLength = 65537;
	await assert.rejects(bridge.read(), /exceeds 64 KiB/);
	delete wireFaults.responseLength;
	assert.equal((await bridge.read()).nextCursor, 32);
});

test('an oversized USB request is rejected before writing bytes', async t => {
	const { transport, frames } = await serialFixture(t);
	await assert.rejects(transport.request('api/control', {
		method: 'POST', headers: { 'X-TraceLink': '1a2b3c4d' },
		body: new URLSearchParams({ action: 'send', data: 'x'.repeat(1024) })
	}), /exceeds 1024/);
	assert.equal(frames.length, 0);
});

test('serial write failure faults the connection instead of buffering later commands', async t => {
	const { bridge, port, frames } = await serialFixture(t);
	const original = port.write.bind(port);
	port.write = (data, callback) => String(data).includes(' control ')
		? callback(new Error('Simulated serial write failure')) : original(data, callback);
	await assert.rejects(bridge.control('arm', { confirm: true }), /outcome may be unknown/);
	assert.equal(frames.length, 1);
	await assert.rejects(bridge.status(), /write failure/);
	assert.equal(frames.length, 1);
});

test('closing an outstanding serial read rejects it and releases the named port', async t => {
	const { transport, port, wireFaults } = await serialFixture(t);
	wireFaults.noReply = true;
	const written = once(port, 'wire-write');
	const pending = assert.rejects(transport.request('api/events?after=0'), /closed/);
	await written;
	await transport.close();
	await pending;
	assert.equal(port.isOpen, false);
});
