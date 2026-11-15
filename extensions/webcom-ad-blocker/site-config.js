(() => {
  'use strict';
  const allowed = new Set([]);
  const host = location.hostname.toLowerCase();
  const paused = allowed.has(host);
  window.__webcomAdBlockerDisabled = paused;
  if (!paused) return;
  const mark = () => {
    if (!document.documentElement) return false;
    document.documentElement.setAttribute('data-webcom-adblock-off', '1');
    return true;
  };
  if (!mark()) {
    const observer = new MutationObserver(() => { if (mark()) observer.disconnect(); });
    observer.observe(document, { childList: true, subtree: true });
  }
})();
