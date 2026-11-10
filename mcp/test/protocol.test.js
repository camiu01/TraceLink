/**
 * @file protocol.test.js
 * @brief Real MCP clients exercise stdio, Streamable HTTP, and authentication.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
import { request } from 'node:http';
import { Client, StreamableHTTPClientTransport } from '@modelcontextprotocol/client';
import { StdioClientTransport } from '@modelcontextprotocol/client/stdio';
import { startHttp } from '../src/http.js';
import { fixture } from './helpers.js';

const entry = fileURLToPath(new URL('../src/index.js', import.meta.url));
const token = 'local-test-token-not-a-real-secret-12345';
const names = ['tracelink_arm_tx', 'tracelink_disarm_tx', 'tracelink_read_events', 'tracelink_send', 'tracelink_status'];
const input = { bus: 'RS232', encoding: 'hex', data: '01 03', confirm: true };

async function connect(t, transport) {
	const client = new Client({ name: 'tracelink-test', version: '1.0.0' });
	t.after(() => client.close());
	await client.connect(transport);
	return client;
}

async function exercise(client) {
	const tools = (await client.listTools()).tools;
	assert.deepEqual(tools.map(tool => tool.name).sort(), names);
	assert.equal(tools.find(tool => tool.name === 'tracelink_send').annotations.destructiveHint, true);
	const call = (name, args = {}) => client.callTool({ name, arguments: args });
	const first = (await call('tracelink_read_events')).structuredContent;
	assert.equal(first.events.length, 32);
	const next = (await call('tracelink_read_events', { after: first.nextCursor, boot: first.boot })).structuredContent;
	assert.equal(next.events[0].seq, 33);
	assert.equal((await call('tracelink_arm_tx', { confirm: true })).structuredContent.accepted, true);
	assert.equal((await call('tracelink_status')).structuredContent.armed, true);
	const send = await call('tracelink_send', input);
	assert.equal(send.structuredContent.accepted, true);
	assert.equal(send.structuredContent.deliveryConfirmed, false);
	assert.equal((await call('tracelink_disarm_tx')).structuredContent.accepted, true);
	assert.equal((await call('tracelink_status')).structuredContent.armed, false);
}

test('stdio entry point works from an unrelated cwd with a real MCP client', async t => {
	const { baseUrl, calls } = await fixture(t);
	const transport = new StdioClientTransport({
		command: process.execPath, args: [entry], cwd: fileURLToPath(new URL('../../../', import.meta.url)),
		env: { TRACELINK_URL: baseUrl, TRACELINK_ALLOW_TX: '1' }, stderr: 'pipe'
	});
	const client = await connect(t, transport);
	await exercise(client);
	assert.deepEqual(calls.filter(call => call.method === 'POST').map(call => call.form.action),
		['arm', 'send', 'disarm']);
});

test('stdio defaults read-only and rejects invalid parameters through MCP', async t => {
	const { baseUrl, calls } = await fixture(t);
	const client = await connect(t, new StdioClientTransport({
		command: process.execPath, args: [entry, 'stdio'], env: { TRACELINK_URL: baseUrl }, stderr: 'pipe'
	}));
	const result = await client.callTool({ name: 'tracelink_arm_tx', arguments: { confirm: true } });
	assert.equal(result.isError, true);
	assert.match(result.content[0].text, /TX blocked/);
	const invalid = await client.callTool({ name: 'tracelink_send', arguments: { ...input, data: 'F' } });
	assert.equal(invalid.isError, true);
	assert.equal(calls.length, 0);
});

test('Streamable HTTP supports a real client and rejects unauthorized, cross-origin and oversized calls', async t => {
	const { bridge, calls } = await fixture(t, { allowTx: true });
	const http = await startHttp(bridge, { port: 0, token });
	t.after(() => new Promise(resolve => { http.close(resolve); http.closeAllConnections(); }));
	const url = new URL(`http://127.0.0.1:${http.address().port}/mcp`);
	const headers = { Authorization: `Bearer ${token}` };
	assert.equal((await fetch(url, { method: 'POST' })).status, 401);
	assert.equal((await fetch(url, { method: 'POST', headers: { Authorization: 'Bearer wrong' } })).status, 401);
	// Node fetch rewrites Host, so use raw HTTP for the DNS-rebinding check.
	const badHostStatus = await new Promise((resolve, reject) => {
		const req = request(url, { method: 'POST', headers: { ...headers, Host: 'evil.example' } }, res => {
			res.resume();
			resolve(res.statusCode);
		});
		req.on('error', reject);
		req.end();
	});
	assert.equal(badHostStatus, 403);
	for (const extra of [{ Origin: 'https://evil.example' }, { Origin: 'null' }])
		assert.equal((await fetch(url, { method: 'POST', headers: { ...headers, ...extra } })).status, 403);
	assert.equal(calls.length, 0);
	assert.equal((await fetch(url, { headers })).status, 405);
	assert.equal((await fetch(new URL('/unknown', url), { method: 'POST', headers })).status, 404);
	assert.equal((await fetch(url, { method: 'POST', headers, body: 'x'.repeat(65537) })).status, 413);
	assert.equal((await fetch(url, { method: 'POST', headers, body: 'invalid json' })).status, 400);
	const client = await connect(t, new StreamableHTTPClientTransport(url, { requestInit: { headers } }));
	await exercise(client);
});

test('HTTP entry point starts and serves MCP with environment configuration', async t => {
	const { baseUrl } = await fixture(t);
	const child = spawn(process.execPath, [entry, 'http'], {
		env: { ...process.env, TRACELINK_URL: baseUrl, TRACELINK_MCP_PORT: '0',
			TRACELINK_MCP_TOKEN: token, TRACELINK_ALLOW_TX: '0' },
		stdio: ['ignore', 'pipe', 'pipe']
	});
	t.after(async () => {
		if (child.exitCode === null && child.signalCode === null) {
			const exited = once(child, 'exit');
			child.kill();
			await exited;
		}
	});
	let stderr = '';
	const url = await new Promise((resolve, reject) => {
		child.stderr.on('data', chunk => {
			stderr += chunk;
			const match = stderr.match(/http:\/\/127\.0\.0\.1:\d+\/mcp/);
			if (match) resolve(new URL(match[0]));
		});
		child.once('error', reject);
		child.once('exit', code => reject(new Error(`HTTP startup failed (${code}): ${stderr}`)));
	});
	const client = await connect(t, new StreamableHTTPClientTransport(url, {
		requestInit: { headers: { Authorization: `Bearer ${token}` } }
	}));
	const status = await client.callTool({ name: 'tracelink_status', arguments: {} });
	assert.equal(status.structuredContent.txPermittedByBridge, false);
});

test('HTTP refuses to listen without a sufficiently long token', async t => {
	const { bridge } = await fixture(t);
	await assert.rejects(startHttp(bridge), /TOKEN/);
	await assert.rejects(startHttp(bridge, { token: 'short' }), /TOKEN/);
	await assert.rejects(startHttp(bridge, { token: `${token}\n` }), /TOKEN/);
});
