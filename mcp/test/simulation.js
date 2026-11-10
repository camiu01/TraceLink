/**
 * @file simulation.js
 * @brief Opt-in local Postman simulation smoke test. Refuses non-loopback URLs.
 */
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { Client } from '@modelcontextprotocol/client';
import { StdioClientTransport } from '@modelcontextprotocol/client/stdio';

const url = new URL(process.env.TRACELINK_TEST_URL || 'http://127.0.0.1:4010/tracelink/');
assert.equal(url.hostname, '127.0.0.1', 'This test must never address a real ESP32.');
const client = new Client({ name: 'simulation-test', version: '1.0.0' });
try {
	await client.connect(new StdioClientTransport({
		command: process.execPath, args: [fileURLToPath(new URL('../src/index.js', import.meta.url))],
		env: { TRACELINK_URL: url.href, TRACELINK_ALLOW_TX: '1' }, stderr: 'inherit'
	}));
	const call = async (name, args = {}) => {
		const result = await client.callTool({ name, arguments: args });
		assert.notEqual(result.isError, true, JSON.stringify(result));
		return result.structuredContent;
	};
	assert.equal((await client.listTools()).tools.length, 5);
	await call('tracelink_disarm_tx');
	const first = await call('tracelink_read_events');
	assert.equal(first.events.length, 32);
	const second = await call('tracelink_read_events', { after: first.nextCursor, boot: first.boot });
	assert.equal(second.events[0].seq, 33);
	await call('tracelink_arm_tx', { confirm: true });
	assert.equal((await call('tracelink_status')).armed, true);
	const sent = await call('tracelink_send', {
		bus: 'RS232', encoding: 'hex', data: '01 03 FF', confirm: true
	});
	assert.equal(sent.deliveryConfirmed, false);
	const result = await call('tracelink_read_events', { after: second.nextCursor, boot: second.boot });
	assert.ok(result.events.some(event => event.tx && event.hex === '01 03 FF'));
	await call('tracelink_disarm_tx');
	assert.equal((await call('tracelink_status')).armed, false);
	console.log('Postman simulation: stdio MCP discovery, pagination, arm, send, TX record and disarm passed.');
} finally {
	await client.close();
}
