/* ═══════════════════════════════════════════════════════════
   PROTOCOL CONSOLE — console.js
   All client-side logic for the test console (test.html).
   Extracted from inline <script> — identical behaviour.
   ═══════════════════════════════════════════════════════════ */

var ca    = document.getElementById('ca');
var cb    = document.getElementById('cb');
var cop   = document.getElementById('cop');
var pulse = document.getElementById('pulse');

/* ── Helpers ────────────────────────────────────────────── */

function cls(s) {
  if (!s) return 's0';
  return 's' + String(s)[0];
}

function esc(s) {
  return s.replace(/[&<>]/g, function (c) {
    return { '&': '&amp;', '<': '&lt;', '>': '&gt;' }[c];
  });
}

/* ── Wire Animation ─────────────────────────────────────── */

function animate(klass) {
  var color = {
    s2: 'var(--green)',
    s3: 'var(--amber)',
    s4: 'var(--violet)',
    s5: 'var(--red)',
    s0: 'var(--cyan)'
  }[klass] || 'var(--cyan)';

  pulse.style.background  = color;
  pulse.style.boxShadow   = '0 0 14px ' + color;
  pulse.animate(
    [
      { left: '0%',   opacity: 0, transform: 'translate(-50%,-50%) scale(0)' },
      { left: '18%',  opacity: 1, transform: 'translate(-50%,-50%) scale(1)', offset: .18 },
      { left: '100%', opacity: 1, transform: 'translate(-50%,-50%) scale(1)', offset: .5 },
      { left: '100%', opacity: 1, offset: .55 },
      { left: '0%',   opacity: 1, transform: 'translate(-50%,-50%) scale(1)', offset: .95 },
      { left: '0%',   opacity: 0, transform: 'translate(-50%,-50%) scale(0)' }
    ],
    { duration: 760, easing: 'cubic-bezier(.5,0,.5,1)' }
  );
}

/* ── Readout ────────────────────────────────────────────── */

function setReadout(status, statusText, ms, note) {
  var code  = document.getElementById('code');
  var rmeta = document.getElementById('rmeta');
  code.className   = 'code ' + cls(status);
  code.textContent = status ? status : 'ERR';
  rmeta.innerHTML  = note
    ? note
    : '<b>' + (status || '') + '</b> ' + esc(statusText || '') + ' · ' + ms + ' ms';
}

/* ── Transaction Log ────────────────────────────────────── */

function logTx(method, path, status, statusText, ms, headers, body, note) {
  var k   = cls(status);
  var log = document.getElementById('log');
  var tx  = document.createElement('div');
  tx.className = 'tx';

  var hdr = headers
    ? Object.keys(headers).map(function (h) { return h + ': ' + headers[h]; }).join('\n')
    : '(none readable)';

  var shownBody = body == null ? '' : body;
  if (shownBody.length > 4000) shownBody = shownBody.slice(0, 4000) + '\n… [truncated]';

  tx.innerHTML =
    '<div class="tx-top">'
      + '<span class="v">' + esc(method) + '</span>'
      + '<span class="p">' + esc(path) + '</span>'
      + '<span class="badge ' + k + '">' + (status ? status : 'ERR') + '</span>'
    + '</div>'
    + '<div class="tx-meta">' + (note ? esc(note) + ' · ' : '') + esc(statusText || '') + ' · ' + ms + ' ms</div>'
    + '<details open><summary>response headers</summary><pre>' + esc(hdr) + '</pre></details>'
    + '<details' + (shownBody ? ' open' : '') + '><summary>response body</summary><pre>' + esc(shownBody || '(empty)') + '</pre></details>';

  log.insertBefore(tx, log.firstChild);
}

/* ── Core Request Function ──────────────────────────────── */

function req(method, path, opts) {
  opts = opts || {};
  var t0   = performance.now();
  animate('s0');

  var init = { method: method };
  if (opts.body != null) init.body    = opts.body;
  if (opts.headers)      init.headers = opts.headers;
  init.redirect = 'manual';

  fetch(path, init)
    .then(function (r) {
      return r.text().then(function (text) {
        var ms      = Math.round(performance.now() - t0);
        var headers = {};
        r.headers.forEach(function (v, k) { headers[k] = v; });
        setReadout(r.status, r.statusText, ms);
        animate(cls(r.status));
        logTx(method, path, r.status, r.statusText, ms, headers, text);
      });
    })
    .catch(function (e) {
      var ms = Math.round(performance.now() - t0);
      setReadout(0, 'network error', ms, '<b>ERR</b> ' + esc(String(e.message || e)));
      logTx(method, path, 0, 'network / blocked', ms, null, String(e.message || e), 'fetch failed');
    });
}

/* ── Upload Helper ──────────────────────────────────────── */

function uploadFile() {
  var name = document.getElementById('upname').value || 'note.txt';
  var body = document.getElementById('upbody').value;
  req('POST', '/uploads/' + name, { body: body, headers: { 'Content-Type': 'text/plain' } });
}

/* ── Oversized Payload ──────────────────────────────────── */

function tooBig() {
  var big = new Array(2 * 1024 * 1024).join('A'); // ~2 MB, over the 1 MB cap
  req('POST', '/uploads/too-big.txt', { body: big, headers: { 'Content-Type': 'text/plain' } });
}

/* ── Browser Cookies ────────────────────────────────────── */

function showCookies() {
  var c = document.cookie || '(no JS-visible cookies — HttpOnly ones are hidden on purpose)';
  setReadout(200, 'document.cookie', 0, '<b>cookies</b> ' + esc(c));
  logTx('JS', 'document.cookie', 200, 'browser-side', 0, null, c, 'cookies the browser will send back');
}

/* ── Connection Indicator ───────────────────────────────── */

(function ping() {
  var dot = document.getElementById('cdot');
  var st  = document.getElementById('cstat');
  fetch('/', { method: 'GET' })
    .then(function (r) {
      dot.className = 'conn-dot live';
      st.textContent = 'online · ' + location.host;
    })
    .catch(function () {
      dot.className = 'conn-dot down';
      st.textContent = 'no response';
    });
})();
