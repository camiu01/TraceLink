#!/usr/bin/env node
/**
 * @file index.js
 * @brief TraceLink MCP entry point. stdout is reserved for stdio protocol messages.
 */
import { StdioServerTransport } from '@modelcontextprotocol/server/stdio';
import { createBridge } from './connection.js';
import { createMcpServer } from './server.js';
import { startHttp } from './http.js';

try {
	const transport = process.argv[2] ?? 'stdio';
	if (process.argv.length > 3 || !['stdio', 'http'].includes(transport))
		throw new Error('Usage: node /absolute/path/mcp/src/index.js [stdio|http]');
	const bridge = await createBridge();
	let close;
	if (transport === 'stdio') {
		const server = createMcpServer(bridge);
		await server.connect(new StdioServerTransport());
		close = () => server.close();
	} else {
		const http = await startHttp(bridge, {
			port: process.env.TRACELINK_MCP_PORT === undefined ? 3001 : Number(process.env.TRACELINK_MCP_PORT),
			token: process.env.TRACELINK_MCP_TOKEN
		});
		console.error(`TraceLink MCP: http://127.0.0.1:${http.address().port}/mcp`);
		close = () => new Promise(resolve => { http.close(resolve); http.closeAllConnections(); });
	}
	let stopping;
	const shutdown = () => {
		stopping ??= Promise.allSettled([close(), bridge.close()]).then(() => {});
		return stopping;
	};
	if (transport === 'stdio') process.stdin.once('end', () => { shutdown(); });
	for (const signal of ['SIGINT', 'SIGTERM'])
		process.once(signal, () => { shutdown().finally(() => process.exit(0)); });
} catch (error) {
	console.error(error.message);
	process.exitCode = 1;
}
