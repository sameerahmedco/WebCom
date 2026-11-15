const assert = require('node:assert/strict');
const { readFileSync } = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const vm = require('node:vm');

const script = readFileSync(path.join(__dirname, '..', 'extensions', 'webcom-ad-blocker', 'youtube-player.js'), 'utf8');
const response = {
  playabilityStatus: { status: 'OK' },
  streamingData: { formats: [{ url: 'https://video.example/normal-content' }] },
  playerAds: [{}],
  adPlacements: [{}],
  adSlots: [{}]
};

function loadPlayerScript({ window = {}, player = null } = {}) {
  const handlers = new Map();
  const observers = [];
  class Observer {
    constructor(callback) { this.callback = callback; observers.push(this); }
    observe() { this.active = true; }
    disconnect() { this.active = false; }
  }
  class MockResponse {
    constructor(url, payload) { this.url = url; this.payload = payload; }
    async json() { return this.payload; }
  }
  const context = vm.createContext({
    window,
    document: {
      querySelector: () => player,
      addEventListener: (event, handler) => handlers.set(event, handler)
    },
    location: { origin: 'https://www.youtube.com' },
    MutationObserver: Observer,
    Response: MockResponse
  });
  vm.runInContext(script, context);
  return { context, window, handlers, observers, MockResponse };
}

function assertAdDataRemoved(result) {
  assert.equal(JSON.stringify(result.streamingData), JSON.stringify(response.streamingData));
  assert.equal(result.playabilityStatus.status, 'OK');
  for (const field of ['playerAds', 'adPlacements', 'adSlots']) assert.equal(Object.hasOwn(result, field), false);
}

test('filters the initial player response and preserves playback data', () => {
  const initial = { ...response };
  const { window } = loadPlayerScript();
  window.ytInitialPlayerResponse = initial;
  assert.equal(window.ytInitialPlayerResponse, initial);
  assertAdDataRemoved(initial);

  const alreadyLoaded = { ...response };
  loadPlayerScript({ window: { ytInitialPlayerResponse: alreadyLoaded } });
  assertAdDataRemoved(alreadyLoaded);
});

test('preserves unrelated JSON, parse failures, and reviver calls', () => {
  const { context } = loadPlayerScript();
  assert.equal(vm.runInContext('JSON.parse(\'{"adPlacements":[1]}\').adPlacements[0]', context), 1);
  assert.throws(() => vm.runInContext('JSON.parse("invalid")', context), /Unexpected token/);
  context.playerJSON = JSON.stringify(response);
  const result = vm.runInContext('JSON.parse(playerJSON, (key, value) => key === "status" ? value.toLowerCase() : value)', context);
  assert.equal(result.playabilityStatus.status, 'ok');
  assert.equal(Object.hasOwn(result, 'adSlots'), false);
});

test('filters only the exact same-origin Fetch player endpoint', async () => {
  const { MockResponse } = loadPlayerScript();
  const endpoint = 'https://www.youtube.com/youtubei/v1/player';
  assertAdDataRemoved(await new MockResponse(`${endpoint}?key=example`, { ...response }).json());
  assertAdDataRemoved(await new MockResponse(endpoint, { ...response }).json());
  for (const url of [`${endpoint}-other`, 'https://other.example/youtubei/v1/player']) {
    assert.ok((await new MockResponse(url, { ...response }).json()).adPlacements);
  }
});

test('clicks a visible skip button during ads only, including when YouTube reuses the button', () => {
  let ad = false;
  let visible = false;
  let clicks = 0;
  const button = { matches: () => false, getClientRects: () => visible ? [{}] : [], click: () => clicks++ };
  const player = { classList: { contains: name => ad && name === 'ad-showing' }, querySelectorAll: () => [button] };
  const { handlers, observers } = loadPlayerScript({ player });
  const [skipObserver, playerObserver] = observers;
  handlers.get('DOMContentLoaded')();
  assert.equal(skipObserver.active, false);
  ad = true;
  playerObserver.callback();
  assert.equal(clicks, 0);
  visible = true;
  skipObserver.callback();
  skipObserver.callback();
  assert.equal(clicks, 1);
  ad = false;
  playerObserver.callback();
  assert.equal(skipObserver.active, false);
  ad = true;
  playerObserver.callback();
  assert.equal(clicks, 2);
});

test('site pause flag bypasses YouTube player mutation completely', () => {
  const initial = { ...response };
  const { observers } = loadPlayerScript({ window: { __webcomAdBlockerDisabled: true, ytInitialPlayerResponse: initial } });
  assert.ok(initial.playerAds);
  assert.ok(initial.adPlacements);
  assert.ok(initial.adSlots);
  assert.equal(observers.length, 0);
});

test('bundled manifest loads site config before filtering helpers', () => {
  const manifest = JSON.parse(readFileSync(path.join(__dirname, '..', 'extensions', 'webcom-ad-blocker', 'manifest.json'), 'utf8'));
  assert.equal(manifest.version, '0.2.0');
  assert.deepEqual(manifest.content_scripts[0].js, ['site-config.js']);
  assert.equal(manifest.content_scripts[0].world, 'MAIN');
  const playerIndex = manifest.content_scripts.findIndex(entry => Array.isArray(entry.js) && entry.js.includes('youtube-player.js'));
  assert.ok(playerIndex > 0);
});

test('all static network rules expose a profile-generated initiator exclusion slot', () => {
  const rules = JSON.parse(readFileSync(path.join(__dirname, '..', 'extensions', 'webcom-ad-blocker', 'network-rules.json'), 'utf8'));
  assert.ok(rules.length >= 5);
  for (const rule of rules) assert.deepEqual(rule.condition.excludedInitiatorDomains, ['webcom.invalid']);
});

test('cosmetic rules are gated by the per-site pause marker', () => {
  for (const name of ['web-ads.css', 'youtube.css']) {
    const css = readFileSync(path.join(__dirname, '..', 'extensions', 'webcom-ad-blocker', name), 'utf8');
    for (const line of css.split(/\r?\n/).map(line => line.trim()).filter(line => line && !line.startsWith('display') && line !== '}')) {
      assert.ok(line.startsWith('html:not([data-webcom-adblock-off]) '), `${name}: ${line}`);
    }
  }
});
