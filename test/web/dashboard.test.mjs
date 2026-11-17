/**
 * @file dashboard.test.mjs
 * @brief Execute the shipped inline script with a minimal DOM and simulated API.
 */
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createContext, Script } from 'node:vm';

const html = readFileSync(new URL('../../web/index.html', import.meta.url), 'utf8');
const script = new Script(html.match(/<script>([\s\S]*?)<\/script>/)[1]);

/**
 * @purpose Provide only DOM operations used by the dashboard, not a layout engine.
 * @returns A mutable element for script-level regression tests.
 */
const element = () => ({
	dataset: {}, value: '', checked: true, children: [], textContent: '',
	scrollTop: 0, scrollHeight: 1000, clientHeight: 200, listeners: {},
	addEventListener(name, callback) { this.listeners[name] = callback; },
	setAttribute() {},
	append(...children) { this.children.push(...children); },
	appendChild(child) { this.children.push(child); },
	replaceChildren(...children) { this.children = children; },
	add(option) { this.children.push(option); if (!this.value) this.value = option.value; },
	click() { this.listeners.click?.(); }
});

/**
 * @purpose Run the real dashboard without a board, network, timers or dependencies.
 * @param options Initial persisted language and simulated control response.
 * @returns Isolated script context and observable DOM.
 */
const dashboard = async ({ language = 'en', controlStatus = 202 } = {}) => {
	const nodes = new Map();
	const get = id => {
		if (!nodes.has(id)) nodes.set(id, element());
		return nodes.get(id);
	};
	get('filter').value = 'all';
	const state = {
		boot: '12345678', latest: 0, lost: 0, overwritten: 0, uptime: 1000,
		armed: false, busy: false, scanning: false, estimates: [0, 0], canDetected: 0,
		canReady: true, canMissed: 0, canOverrun: 0, canErrors: 0,
		interfaces: [], events: [], message: 'Device ready'
	};
	const calls = [];
	const downloads = [];
	class TestURL extends URL {
		static createObjectURL(blob) { downloads.push(blob); return 'blob:simulation'; }
		static revokeObjectURL() {}
	}
	const context = createContext({
		document: {
			getElementById: get, documentElement: {}, querySelectorAll: () => [],
			createDocumentFragment: element, createElement: element
		},
		navigator: { language: 'en-GB' },
		localStorage: { getItem: () => language, setItem() {} },
		Option: function(text, value) { return { text, value }; },
		URL: TestURL, URLSearchParams, Blob, AbortSignal, setTimeout() {}, confirm: () => false,
		fetch: async (path, options) => {
			calls.push({ path, options });
			if (path === '/api/systems') return { ok: true, json: async () => [] };
			if (path === '/api/control') return {
				ok: controlStatus === 202, text: async () => `Control ${controlStatus}`
			};
			return { ok: true, json: async () => structuredClone(state) };
		}
	});
	script.runInContext(context);
	await new Promise(resolve => setImmediate(resolve));
	return { get, state, calls, downloads, run: expression => new Script(expression).runInContext(context) };
};

test('dashboard starts with an unsupported stored language', async () => {
	const { get } = await dashboard({ language: 'invalid' });
	assert.equal(get('connection').textContent, 'Connected · capturing');
});

test('control errors survive subsequent unchanged status polls without a retry', async () => {
	const { get, run, calls } = await dashboard({ controlStatus: 403 });
	await run("postControl({ action: 'disarm' })");
	assert.match(get('command-result').textContent, /Control 403.*No automatic retry/);
	await run('poll()');
	assert.match(get('command-result').textContent, /Control 403.*No automatic retry/);
	assert.equal(calls.filter(call => call.path === '/api/control').length, 1);
});

test('device execution status remains visible independently of local results', async () => {
	const { get, run, state } = await dashboard();
	await run("postControl({ action: 'disarm' })");
	state.message = 'Execution finished';
	await run('poll()');
	assert.equal(get('command-result').textContent, 'Control 202');
	assert.equal(get('device-message').textContent, 'Execution finished');
});

test('pause still refreshes safety state; reboot resets history and cursor', async () => {
	const { get, run, state } = await dashboard();
	get('pause').click();
	state.armed = true;
	await run('poll()');
	assert.equal(get('tx-state').textContent, 'TX ENABLED · CAN sends ACKs');
	run('cursor = 900; records = [{ seq: 900 }]; missed = 2');
	state.boot = '87654321';
	state.armed = false;
	await run('poll()');
	assert.equal(run('cursor'), 0);
	assert.equal(run('records.length'), 0);
	assert.equal(run('missed'), 0);
	assert.equal(get('send').disabled, true);
});

test('payload rendering uses text and autoscroll can retain reading position', async () => {
	const { get, run } = await dashboard();
	run("records = [{seq:1,ms:1,source:'RS232',hex:'3C 69 6D 67 3E',tx:false}]");
	get('follow').checked = false;
	get('traffic-scroll').scrollTop = 23;
	run('render()');
	assert.equal(get('traffic-scroll').scrollTop, 23);
	const row = get('rows').children[0].children[0];
	assert.equal(row.children[5].textContent, '<img>');
	get('follow').checked = true;
	run('render()');
	assert.equal(get('traffic-scroll').scrollTop, 1000);
});

test('CSV neutralizes formulas from bus bytes and doubles quotes', async () => {
	const { get, run, downloads } = await dashboard();
	run("records = [{seq:1,ms:1,source:'RS232',hex:'3D 31 2B 22 31 22',tx:false}]");
	get('export').click();
	const csv = await downloads[0].text();
	assert.match(csv, /"'\=1\+""1"""/);
	assert.match(csv, /sequence.*time_ms/);
});
