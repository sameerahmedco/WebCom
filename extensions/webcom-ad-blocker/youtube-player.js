(() => {
  'use strict';

  function removePlayerAds(response) {
    if (!response || typeof response !== 'object' || !Object.hasOwn(response, 'playabilityStatus')) {
      return response;
    }
    Reflect.deleteProperty(response, 'playerAds');
    Reflect.deleteProperty(response, 'adPlacements');
    Reflect.deleteProperty(response, 'adSlots');
    return response;
  }

  const initial = Object.getOwnPropertyDescriptor(window, 'ytInitialPlayerResponse');
  if (!initial) {
    let playerResponse;
    Object.defineProperty(window, 'ytInitialPlayerResponse', {
      configurable: true,
      enumerable: true,
      get() { return playerResponse; },
      set(value) { playerResponse = removePlayerAds(value); }
    });
  } else if (Object.hasOwn(initial, 'value')) {
    removePlayerAds(initial.value);
  }

  // YouTube consumes player data from both JSON text and Fetch responses.
  const parseJSON = JSON.parse;
  JSON.parse = function parse(text, reviver) {
    return removePlayerAds(Reflect.apply(parseJSON, this, arguments));
  };

  const readJSON = Response.prototype.json;
  const playerEndpoint = `${location.origin}/youtubei/v1/player`;
  Response.prototype.json = function json() {
    const result = Reflect.apply(readJSON, this, arguments);
    if (this instanceof Response && (this.url === playerEndpoint || this.url.startsWith(`${playerEndpoint}?`))) {
      return result.then(removePlayerAds);
    }
    return result;
  };

  const skipSelector = '.ytp-ad-skip-button-modern, .ytp-ad-skip-button, .ytp-skip-ad-button';
  let clicked = new WeakSet();
  let player;
  let watchingAd = false;

  function adIsShowing() {
    return player && (player.classList.contains('ad-showing') || player.classList.contains('ad-interrupting'));
  }

  function trySkip() {
    if (!adIsShowing()) return;
    for (const button of player.querySelectorAll(skipSelector)) {
      if (clicked.has(button) || button.matches(':disabled, [aria-disabled="true"]') || !button.getClientRects().length) continue;
      clicked.add(button);
      button.click();
      return;
    }
  }

  const skipObserver = new MutationObserver(trySkip);
  const playerObserver = new MutationObserver(updateSkipObserver);

  function updateSkipObserver() {
    if (!adIsShowing()) {
      if (watchingAd) {
        skipObserver.disconnect();
        watchingAd = false;
        clicked = new WeakSet();
      }
      return;
    }
    if (watchingAd) return;
    watchingAd = true;
    skipObserver.observe(player, {
      childList: true,
      subtree: true,
      attributes: true,
      attributeFilter: ['class', 'style', 'hidden', 'disabled', 'aria-disabled']
    });
    trySkip();
  }

  function watchPlayer() {
    const current = document.querySelector('.html5-video-player');
    if (player === current) return;
    playerObserver.disconnect();
    skipObserver.disconnect();
    watchingAd = false;
    clicked = new WeakSet();
    player = current;
    if (player) playerObserver.observe(player, { attributes: true, attributeFilter: ['class'] });
    updateSkipObserver();
  }

  document.addEventListener('DOMContentLoaded', watchPlayer, { once: true });
  document.addEventListener('yt-navigate-finish', watchPlayer);
  document.addEventListener('loadedmetadata', watchPlayer, true);
})();
