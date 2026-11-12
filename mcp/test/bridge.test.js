/**
 * @file bridge.test.js
 * @brief Contract boundaries, write safety and failure behavior over real local HTTP.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { fixture, record } from './helpers.js';
import { TraceLinkBridge } from '../src/bridge.js';
import { readInput, sendInput } from '../src/schemas.js';

const command = { bus: 'RS232', encoding: 'hex', data: '01 03 FF', confirm: true };
const can = { ...command, bus: 'CAN', id: '7FF' };

test('read pagination does not jump to latest; caller cursors are independent', async t => {
	const { bridge, calls } = await fixture(t);
	const first = await bridge.read();
	assert.equal(first.events.length, 32);
	assert.equal(first.latest, 40);
	assert.equal(first.nextCursor, 32);
	const second = await bridge.read({ after: first.nextCursor, boot: first.boot });
	assert.deepEqual(second.events.map(event => event.seq), [33, 34, 35, 36, 37, 38, 39, 40]);
	assert.equal((await bridge.read()).nextCursor, 32);
	const end = await bridge.read({ after: 40, boot: first.boot });
	assert.equal(end.nextCursor, 40);
	assert.equal(end.events.length, 0);
	assert.equal(calls.filter(call => call.method === 'POST').length, 0);
});

test('reboot resets cursor and reports overwrites', async t => {
	const { bridge, state } = await fixture(t);
	state.events = [record(10)];
	state.latest = 10;
	state.overwritten = 9;
	const result = await bridge.read({ after: 100, boot: '00000000' });
	assert.equal(result.reset, true);
	assert.equal(result.nextCursor, 10);
	assert.equal(result.lost, 9);
	assert.equal(result.overwritten, 9);
	const status = await bridge.status();
	assert.equal('events' in status, false);
	assert.equal(status.txPermittedByBridge, false);
});

test('TX requires host opt-in, confirmation, arming and interface readiness', async t => {
	const { bridge, baseUrl, calls, state } = await fixture(t);
	await assert.rejects(bridge.control('arm', { confirm: true }), /TX blocked/);
	await assert.rejects(bridge.control('send', command), /TX blocked/);
	assert.equal(calls.length, 0);
	const writable = new TraceLinkBridge({ baseUrl, allowTx: true });
	await assert.rejects(writable.control('arm', { confirm: false }));
	await assert.rejects(writable.control('send', command), /disarmed/);
	state.armed = true;
	state.interfaces[0].ready = false;
	await assert.rejects(writable.control('send', command), /not ready/);
	state.interfaces[0].ready = true;
	state.scanning = true;
	await assert.rejects(writable.control('send', command), /scanning/);
	state.scanning = false;
	state.busy = true;
	await assert.rejects(writable.control('arm', { confirm: true }), /busy/);
	assert.equal(calls.some(call => call.method === 'POST'), false);
});

test('arm, serial send, CAN RTR, and disarm use firmware fields and current token', async t => {
	const { bridge, state, calls, baseUrl } = await fixture(t, { allowTx: true });
	assert.equal((await bridge.control('arm', { confirm: true })).accepted, true);
	assert.equal((await bridge.status()).armed, true);
	const response = await bridge.control('send', command);
	assert.equal(response.deliveryConfirmed, false);
	assert.deepEqual(calls.at(-1).form, {
		action: 'send', bus: 'RS232', encoding: 'hex', data: '01 03 FF', extended: '0', rtr: '0'
	});
	assert.equal(calls.at(-1).headers['x-tracelink'], state.boot);
	assert.match(calls.at(-1).headers['content-type'], /^application\/x-www-form-urlencoded/);
	await bridge.control('send', { ...command, bus: 'RS485', encoding: 'text', data: 'à&=+\r\n' });
	assert.equal(state.events.at(-1).hex, 'C3 A0 26 3D 2B 0D 0A');
	await bridge.control('send', { ...can, id: '1FFFFFFF', extended: true, rtr: true, data: '', dlc: 8 });
	assert.deepEqual(calls.at(-1).form, {
		action: 'send', bus: 'CAN', encoding: 'hex', data: '', id: '1FFFFFFF',
		dlc: '8', extended: '1', rtr: '1'
	});
	state.boot = 'abcdef12';
	// Disarming remains available even through a read-only bridge.
	await new TraceLinkBridge({ baseUrl }).control('disarm');
	assert.equal(state.armed, false);
	assert.equal(calls.at(-1).headers['x-tracelink'], 'abcdef12');
});

test('payload and cursor limits match the firmware, including UTF-8 byte boundaries', () => {
	const valid = [
		{ ...command, data: 'FF'.repeat(64) },
		{ ...command, data: '\t01\r\n03 FF ' },
		{ ...command, encoding: 'text', data: 'à'.repeat(32) },
		{ ...can, data: 'FF'.repeat(8) },
		{ ...can, data: '' },
		{ ...can, id: '1FFFFFFF', extended: true },
		{ ...can, rtr: true, data: '', dlc: 0 }
	];
	valid.forEach(input => assert.equal(sendInput.safeParse(input).success, true, JSON.stringify(input)));
	const invalid = [
		{ ...command, confirm: false }, { ...command, data: 'F' }, { ...command, data: '0 1' },
		{ ...command, data: '01\u00a003' }, { ...command, data: 'FF'.repeat(65) },
		{ ...command, data: '' }, { ...command, data: 'x'.repeat(257) },
		{ ...command, encoding: 'text', data: 'à'.repeat(33) },
		{ ...command, encoding: 'text', data: 'a\0b' }, { ...command, id: '123' },
		{ ...command, bus: 'unknown' }, { ...command, action: 'rate' },
		{ ...can, data: 'FF'.repeat(9) }, { ...can, id: '800' },
		{ ...can, id: '20000000', extended: true }, { ...can, id: '0x123' },
		{ ...can, id: undefined }, { ...can, encoding: 'text' },
		{ ...can, rtr: true, dlc: 8 }, { ...can, rtr: true, data: '' },
		{ ...can, rtr: true, data: '', dlc: 9 }, { ...can, dlc: 1 }
	];
	invalid.forEach(input => assert.equal(sendInput.safeParse(input).success, false, JSON.stringify(input)));
	assert.equal(readInput.safeParse({ after: 999999999999999, boot: 'abcdef12' }).success, true);
	for (const after of [-1, 1.5, 1000000000000000, '1'])
		assert.equal(readInput.safeParse({ after, boot: 'abcdef12' }).success, false);
	assert.equal(readInput.safeParse({ after: 1 }).success, false);
});

test('malformed, oversized, redirected and error reads never become controls', async t => {
	const { bridge, faults, calls, state } = await fixture(t, { allowTx: true });
	for (const key of ['invalidJson', 'oversized', 'redirect']) {
		faults[key] = true;
		await assert.rejects(bridge.control('arm', { confirm: true }));
		delete faults[key];
	}
	faults.readStatus = 500;
	await assert.rejects(bridge.status(), /HTTP 500/);
	delete faults.readStatus;
	state.armed = 'true';
	await assert.rejects(bridge.control('send', command));
	assert.equal(calls.some(call => call.method === 'POST' || call.path === '/must-not-follow'), false);
});

for (const status of [400, 403, 409, 500]) {
	test(`control HTTP ${status} is returned as an error without retry`, async t => {
		const { bridge, calls, faults } = await fixture(t, { allowTx: true });
		faults.controlStatus = status;
		await assert.rejects(bridge.control('arm', { confirm: true }), new RegExp(`HTTP ${status}`));
		assert.equal(calls.filter(call => call.method === 'POST').length, 1);
	});
}

test('reboot between preflight and control is not retried with a new token', async t => {
	const { bridge, calls, faults } = await fixture(t, { allowTx: true });
	faults.rebootBeforeControl = true;
	await assert.rejects(bridge.control('arm', { confirm: true }), /HTTP 403/);
	assert.equal(calls.filter(call => call.method === 'POST').length, 1);
});

test('timeout after the write and overlapping controls never duplicate a command', async t => {
	const { bridge, calls, faults, state } = await fixture(t, { allowTx: true, timeoutMs: 50 });
	state.armed = true;
	faults.delayControl = 150;
	const pending = bridge.control('send', command);
	await assert.rejects(bridge.control('send', command), /in progress/);
	await assert.rejects(pending, /outcome may be unknown.*Do not retry/);
	assert.equal(calls.filter(call => call.method === 'POST').length, 1);
	assert.equal(state.events.filter(event => event.tx).length, 1);
});

test('disconnected response after write is ambiguous and never retried', async t => {
	const { bridge, calls, faults, state } = await fixture(t, { allowTx: true });
	state.armed = true;
	faults.dropControl = true;
	await assert.rejects(bridge.control('send', command), /outcome may be unknown/);
	assert.equal(calls.filter(call => call.method === 'POST').length, 1);
	assert.equal(state.events.filter(event => event.tx).length, 1);
});

test('base URL rejects credentials, non-HTTP, query and fragment', () => {
	for (const baseUrl of ['file:///etc/passwd', 'http://user:pass@localhost', 'http://localhost/?x=1', 'http://localhost/#x'])
		assert.throws(() => new TraceLinkBridge({ baseUrl }));
});
