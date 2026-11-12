/**
 * @file handler.cjs
 * @brief Stateful local stand-in derived from docs/api.md and src/TraceLink.cpp.
 */
const { setTimeout: delay } = require('node:timers/promises');

function record(seq, source = 'RS232', hex = '30 31', tx = false) {
	return { seq, ms: seq * 10, source, hex, id: 0, ext: false, rtr: false, dlc: 0, tx };
}

function createMock({ store } = {}) {
	const state = {
		boot: '1a2b3c4d', uptime: 12345, latest: 40, lost: 0, overwritten: 0,
		canReady: true, canMissed: 0, canOverrun: 0, canErrors: 0,
		armed: false, busy: false, scanning: false, estimates: [0, 0], canDetected: 0,
		message: 'Pronto. Trasmissione disabilitata.',
		interfaces: ['RS232', 'RS485', 'CAN'].map(name => ({
			name, protocol: name === 'CAN' ? 'can' : 'serial', ready: true,
			rate: name === 'CAN' ? 500000 : 9600, count: name === 'RS232' ? 40 : 0
		})),
		events: Array.from({ length: 40 }, (_, i) => record(i + 1))
	};
	const calls = [];
	const faults = {};
	const handler = async (req, res) => {
		try {
			const current = store ? await store.get('tracelink.state') || state : state;
			const url = new URL(req.url, 'http://localhost');
			calls.push({ method: req.method, path: url.pathname, headers: req.headers });
			res.setHeader('Cache-Control', 'no-store');
			if (req.method === 'GET' && url.pathname === '/api/events') {
				if (faults.readStatus) { res.writeHead(faults.readStatus).end('Read error'); return; }
				if (faults.redirect) { res.writeHead(302, { Location: '/must-not-follow' }).end(); return; }
				if (faults.invalidJson) { res.end('<html>offline</html>'); return; }
				if (faults.oversized) { res.end('x'.repeat(65537)); return; }
				const after = url.searchParams.get('after') || '0';
				if (!/^\d{1,15}$/.test(after)) { res.writeHead(400).end('invalid cursor'); return; }
				const oldest = current.events[0]?.seq ?? current.latest + 1;
				const result = {
					...current, lost: Math.max(0, oldest - Number(after) - 1),
					events: current.events.filter(event => event.seq > Number(after)).slice(0, 32)
				};
				res.setHeader('Content-Type', 'application/json');
				res.end(JSON.stringify(result));
				return;
			}
			if (req.method !== 'POST' || url.pathname !== '/api/control') {
				res.writeHead(501).end('Not modeled by this local mock');
				return;
			}
			let body = req.body;
			if (body === undefined) {
				const chunks = [];
				for await (const chunk of req) chunks.push(chunk);
				body = Buffer.concat(chunks).toString();
			}
			const form = new URLSearchParams(body);
			calls.at(-1).form = Object.fromEntries(form);
			if (faults.rebootBeforeControl) current.boot = '87654321';
			if (req.headers['x-tracelink'] !== current.boot) { res.writeHead(403).end(); return; }
			if (faults.controlStatus) { res.writeHead(faults.controlStatus).end(); return; }
			const action = form.get('action');
			if (current.busy || current.scanning || (action === 'send' && !current.armed)) {
				res.writeHead(409).end(); return;
			}
			if (action === 'arm') current.armed = true;
			else if (action === 'disarm') current.armed = false;
			else if (action === 'send') {
				const hex = form.get('encoding') === 'text'
					? Buffer.from(form.get('data')).toString('hex').match(/../g).join(' ').toUpperCase()
					: form.get('data').replace(/[ \t\r\n]/g, '').match(/../g)?.join(' ').toUpperCase() || '';
				const event = record(++current.latest, form.get('bus'), hex, true);
				if (event.source === 'CAN') Object.assign(event, {
					id: parseInt(form.get('id'), 16), ext: form.get('extended') === '1',
					rtr: form.get('rtr') === '1', dlc: Number(form.get('dlc') ?? (hex ? hex.split(' ').length : 0))
				});
				current.events.push(event);
			} else { res.writeHead(400).end(); return; }
			current.message = action === 'send' ? 'Accodato; non conferma consegna.' : `TX ${action}`;
			if (store) await store.set('tracelink.state', current);
			if (faults.delayControl) await delay(faults.delayControl);
			if (faults.dropControl) { req.socket.destroy(); return; }
			res.writeHead(202, { 'Content-Type': 'text/plain' }).end('Richiesta accettata');
		} catch {
			res.writeHead(500).end('Mock failure');
		}
	};
	return { state, calls, faults, handler };
}

module.exports = { createMock, record };
