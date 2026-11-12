/**
 * @file bridge.js
 * @brief Shared TraceLink safety gates over HTTP or USB, with no automatic write retries.
 */
import { confirmation, readInput, sendInput, snapshot } from './schemas.js';

export class TraceLinkBridge {
	#busy = false;

	constructor({ baseUrl = 'http://192.168.4.1', allowTx = false, timeoutMs = 3000, transport } = {}) {
		this.baseUrl = new URL(baseUrl);
		if (!['http:', 'https:'].includes(this.baseUrl.protocol) || this.baseUrl.username ||
			this.baseUrl.password || this.baseUrl.search || this.baseUrl.hash)
			throw new Error('TRACELINK_URL must be an HTTP(S) URL without credentials, query or fragment.');
		if (!this.baseUrl.pathname.endsWith('/')) this.baseUrl.pathname += '/';
		this.allowTx = allowTx === true;
		this.timeoutMs = timeoutMs;
		this.transport = transport;
	}

	async request(path, options = {}) {
		const mutating = options.method === 'POST';
		try {
			if (this.transport) return await this.transport.request(path, options);
			const response = await fetch(new URL(path, this.baseUrl), {
				...options, redirect: 'manual', signal: AbortSignal.timeout(this.timeoutMs)
			});
			const chunks = [];
			let bytes = 0;
			for await (const chunk of response.body) {
				bytes += chunk.length;
				if (bytes > 65536) throw new Error('Response exceeds 64 KiB.');
				chunks.push(chunk);
			}
			const body = Buffer.concat(chunks).toString('utf8');
			if (!response.ok) throw new Error(`TraceLink HTTP ${response.status}.`);
			if (mutating) {
				if (response.status !== 202) throw new Error('Expected HTTP 202 from control.');
				return { accepted: true, deliveryConfirmed: false, message: body };
			}
			return JSON.parse(body);
		} catch (error) {
			throw new Error(mutating
				? `${error.message} Control outcome may be unknown. Do not retry automatically; read status first.`
				: `Cannot read TraceLink: ${error.message}`);
		}
	}

	async close() { await this.transport?.close(); }

	async read(input = {}) {
		const { after, boot } = readInput.parse(input);
		let state = snapshot.parse(await this.request(`api/events?after=${after}`));
		const reset = boot !== undefined && boot !== state.boot;
		if (reset) state = snapshot.parse(await this.request('api/events?after=0'));
		const nextCursor = state.events.at(-1)?.seq ?? (reset ? 0 : after);
		return { ...state, nextCursor, reset, txPermittedByBridge: this.allowTx };
	}

	async status() {
		const { events, nextCursor, reset, ...status } = await this.read();
		return status;
	}

	async control(action, input = {}) {
		if (action === 'send') input = sendInput.parse(input);
		else if (action === 'arm') confirmation.parse(input);
		else if (action !== 'disarm') throw new Error('Unsupported action.');
		if (action !== 'disarm' && !this.allowTx)
			throw new Error('TX blocked by bridge. Start with TRACELINK_ALLOW_TX=1 only after reviewing hardware.');
		if (this.#busy) throw new Error('Another control is in progress. Read status before trying again.');
		this.#busy = true;
		try {
			const state = await this.read();
			if (state.busy || state.scanning) throw new Error('Device is busy or scanning. No control sent.');
			if (action === 'send' && !state.armed) throw new Error('TX is disarmed. No command sent.');
			if (action === 'send' && !state.interfaces.find(bus => bus.name === input.bus)?.ready)
				throw new Error('Selected interface is not ready. No command sent.');
			const form = new URLSearchParams({ action });
			if (action === 'send') {
				for (const key of ['bus', 'encoding', 'data', 'id', 'dlc'])
					if (input[key] !== undefined) form.set(key, String(input[key]));
				form.set('extended', input.extended ? '1' : '0');
				form.set('rtr', input.rtr ? '1' : '0');
			}
			return await this.request('api/control', {
				method: 'POST', headers: { 'X-TraceLink': state.boot }, body: form
			});
		} finally {
			this.#busy = false;
		}
	}
}
