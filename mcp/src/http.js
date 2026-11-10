/**
 * @file http.js
 * @brief Loopback-only, bearer-protected stateless MCP Streamable HTTP.
 */
import { createServer } from 'node:http';
import { createHash, timingSafeEqual } from 'node:crypto';
import { NodeStreamableHTTPServerTransport } from '@modelcontextprotocol/node';
import { createMcpServer } from './server.js';

/** @brief Serve HTTP only on loopback. Remote deployment requires a separate secured gateway. */
export async function startHttp(bridge, { port = 3001, token } = {}) {
	if (typeof token !== 'string' || !/^[\x21-\x7e]{32,}$/.test(token))
		throw new Error('HTTP requires TRACELINK_MCP_TOKEN with at least 32 printable non-space characters.');
	if (!Number.isInteger(port) || port < 0 || port > 65535) throw new Error('Invalid MCP port.');
	const digest = value => createHash('sha256').update(value).digest();
	const expected = digest(`Bearer ${token}`);
	const http = createServer(async (req, res) => {
		res.setHeader('Cache-Control', 'no-store');
		const hosts = [`127.0.0.1:${http.address().port}`, `localhost:${http.address().port}`];
		if (!hosts.includes(req.headers.host) ||
			(req.headers.origin && !hosts.map(host => `http://${host}`).includes(req.headers.origin))) {
			res.writeHead(403).end('Forbidden Host or Origin');
			return;
		}
		if (!timingSafeEqual(expected, digest(req.headers.authorization ?? ''))) {
			res.setHeader('WWW-Authenticate', 'Bearer');
			res.writeHead(401).end('Unauthorized');
			return;
		}
		if (req.url !== '/mcp') { res.writeHead(404).end(); return; }
		if (req.method !== 'POST') { res.setHeader('Allow', 'POST'); res.writeHead(405).end(); return; }
		let server;
		try {
			const chunks = [];
			let size = 0;
			for await (const chunk of req) {
				size += chunk.length;
				if (size > 65536) { res.writeHead(413).end(); return; }
				chunks.push(chunk);
			}
			const body = JSON.parse(Buffer.concat(chunks).toString('utf8'));
			server = createMcpServer(bridge);
			const transport = new NodeStreamableHTTPServerTransport({
				sessionIdGenerator: undefined, enableJsonResponse: true
			});
			res.on('close', () => { server.close().catch(() => {}); });
			await server.connect(transport);
			await transport.handleRequest(req, res, body);
		} catch {
			if (!res.headersSent) res.writeHead(400).end('Invalid MCP request');
			else res.end();
			if (server) await server.close();
		}
	});
	http.requestTimeout = 10000;
	http.headersTimeout = 10000;
	await new Promise((resolve, reject) => {
		http.once('error', reject);
		http.listen(port, '127.0.0.1', resolve);
	});
	return http;
}
