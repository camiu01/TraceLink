/**
 * @file helpers.js
 * @brief Isolated local HTTP mock fixture, never the real ESP32.
 */
import { createServer } from 'node:http';
import { createRequire } from 'node:module';
import { once } from 'node:events';
import { TraceLinkBridge } from '../src/bridge.js';

const require = createRequire(import.meta.url);
export const { createMock, record } = require('../../postman/mocks/tracelink/handler.cjs');

export async function fixture(t, options = {}) {
	const mock = createMock();
	const http = createServer(mock.handler);
	http.listen(0, '127.0.0.1');
	await once(http, 'listening');
	t.after(() => new Promise(resolve => { http.close(resolve); http.closeAllConnections(); }));
	const baseUrl = `http://127.0.0.1:${http.address().port}`;
	return { ...mock, baseUrl, bridge: new TraceLinkBridge({ baseUrl, ...options }) };
}
