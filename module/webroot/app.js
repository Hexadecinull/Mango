'use strict';

/*
 * Talks to root through the `ksu` global that KernelSU's WebView injects,
 * and that Magisk (v28.1+) and APatch are expected to provide too via the
 * same webroot/ convention (see docs/ARCHITECTURE.md and
 * docs/BUILDING.md's WebUI section). Not independently confirmed on every
 * manager yet, if `ksu.exec` isn't there on your setup, that's the thing
 * to file an issue about.
 *
 * Shell strings below aren't escaped against injection. That's a
 * conscious choice, not an oversight: this whole page only runs inside a
 * WebView that already has root on your own device, so the only person
 * who could inject anything is you, into your own shell, which you
 * already have.
 */

const logLines = [];
function logLine(line) {
  logLines.push(line);
  const el = document.getElementById('log');
  el.textContent = logLines.join('\n');
  el.scrollTop = el.scrollHeight;
}

function hasRootBridge() {
  return typeof ksu !== 'undefined' && typeof ksu.exec === 'function';
}

async function run(cmd) {
  logLine('$ ' + cmd);
  if (!hasRootBridge()) {
    throw new Error('No root bridge found. Open this page from your root manager app, not a browser.');
  }
  const result = await ksu.exec(cmd);
  const stdout = (result && result.stdout) || '';
  if (stdout.trim()) {
    logLine(stdout.trim());
  }
  if (result && result.errno && result.errno !== 0) {
    logLine('(exit code ' + result.errno + ')');
  }
  return result;
}

/* Mirrors core/src/main/kotlin/dev/mango/core/CompatibilityChecker.kt.
 * Kept in sync by hand, JS can't share code with the JVM side, see
 * docs/ARCHITECTURE.md. If you change one, change the other. */
const ARMEABI_V7A = 'armeabi-v7a';
const X86 = 'x86';

function checkCompatibility(abisPresent, supportedAbis, bridgeActive) {
  if (abisPresent.length === 0) {
    return {
      verdict: 'RUNS_NATIVELY',
      notes: ["No native libraries at all, architecture doesn't apply."],
    };
  }
  if (abisPresent.some((abi) => supportedAbis.includes(abi))) {
    return {
      verdict: 'RUNS_NATIVELY',
      notes: ['Already ships a native ABI this device supports directly.'],
    };
  }
  const is32BitOnly = abisPresent.every((abi) => abi === ARMEABI_V7A || abi === X86);
  if (!is32BitOnly) {
    return {
      verdict: 'BLOCKED',
      notes: ["Doesn't ship an ABI this device supports, and it's not a 32-bit-only case Mango handles."],
    };
  }
  if (bridgeActive) {
    return {
      verdict: 'NEEDS_BRIDGE',
      notes: ['32-bit-only app. The Mango module is active, this should install and launch through the native bridge.'],
    };
  }
  return {
    verdict: 'BLOCKED',
    notes: ["32-bit-only app, but the Mango module isn't active on this device yet."],
  };
}

let deviceAbis = [];
let bridgeActive = false;
let selectedPath = '';

async function refreshStatus() {
  const statusBody = document.getElementById('status-body');
  try {
    const abilist = await run('getprop ro.product.cpu.abilist');
    deviceAbis = abilist.stdout.trim().split(',').map((s) => s.trim()).filter(Boolean);

    const bridge = await run('getprop ro.dalvik.vm.native.bridge');
    bridgeActive = bridge.stdout.trim() === 'libmango_translator.so';

    statusBody.innerHTML =
      'Device ABIs: ' + (deviceAbis.join(', ') || 'unknown') + '<br>' +
      'Mango module: ' + (bridgeActive ? 'active' : 'not active');
  } catch (err) {
    statusBody.textContent = String((err && err.message) || err);
  }
}

async function loadInstalledApps() {
  const select = document.getElementById('app-select');
  try {
    const result = await run('pm list packages -3');
    const packages = result.stdout
      .split('\n')
      .map((line) => line.replace(/^package:/, '').trim())
      .filter(Boolean)
      .sort();

    select.innerHTML = '<option value="">Select an app…</option>';
    for (const pkg of packages) {
      const opt = document.createElement('option');
      opt.value = pkg;
      opt.textContent = pkg;
      select.appendChild(opt);
    }
  } catch (err) {
    select.innerHTML = '<option value="">Failed to list apps</option>';
  }
}

/*
 * Two different questions, two different tools, on purpose:
 *
 * - An installed package: ask Android's own package manager what ABI
 *   it's actually using, via `dumpsys package`. This doesn't need
 *   `unzip` at all, and reflects Android's own authoritative call
 *   rather than us re-deriving it. The exact output format isn't
 *   guaranteed identical across Android versions (dumpsys is a debug
 *   dump, not a stable API), so this scans for known ABI name strings
 *   on lines that mention "abi" rather than parsing one exact field.
 * - A raw .apk file (not installed yet): there's no dumpsys entry for
 *   it, so this actually needs to look inside the zip. `unzip` covers
 *   this, but it is NOT part of Android's built-in toybox utilities
 *   (checked the full applet list, it's genuinely not there), so some
 *   devices won't have it at all. Checked for up front instead of
 *   failing with a confusing error partway through.
 */
const KNOWN_ABIS = ['arm64-v8a', 'armeabi-v7a', 'x86_64', 'x86', 'armeabi'];

async function inspectAbisFromDumpsys(pkg) {
  const result = await run('dumpsys package ' + pkg + ' | grep -i abi');
  const found = new Set();
  for (const line of result.stdout.split('\n')) {
    /* Token match, not substring match: 'armeabi' is a substring of
     * 'armeabi-v7a' and 'x86' of 'x86_64', so a naive .includes() scan
     * would report both for a line that only actually says the longer
     * one. Splitting into tokens on anything that isn't part of an ABI
     * name avoids that collision. */
    const tokens = line.split(/[^a-zA-Z0-9_-]+/);
    for (const token of tokens) {
      if (KNOWN_ABIS.includes(token)) {
        found.add(token);
      }
    }
  }
  return Array.from(found);
}

async function inspectAbisFromApkFile(apkPath) {
  const has = await run('command -v unzip');
  if (!has.stdout.trim()) {
    throw new Error(
      "No unzip on this device, can't inspect a raw APK file this way. " +
        'Try picking the app from the installed-apps list instead, that path checks '
        + 'the app through Android itself and needs no unzip.',
    );
  }
  const result = await run('unzip -l "' + apkPath + '" | grep -oE "lib/[^/]+/" | sort -u');
  return result.stdout
    .split('\n')
    .map((line) => line.replace('lib/', '').replace('/', '').trim())
    .filter(Boolean);
}

async function resolveInstalledPackagePath(pkg) {
  const result = await run('pm path ' + pkg + ' | head -n 1');
  return result.stdout.replace(/^package:/, '').trim();
}

function renderResult(report) {
  const card = document.getElementById('result-card');
  const title = document.getElementById('result-title');
  const notes = document.getElementById('result-notes');
  const installButton = document.getElementById('install-button');

  card.classList.remove('hidden');

  const titles = {
    RUNS_NATIVELY: 'This app already runs natively',
    NEEDS_BRIDGE: '32-bit-only, the bridge should handle it',
    BLOCKED: "Can't proceed",
  };
  title.textContent = titles[report.verdict] || report.verdict;

  notes.innerHTML = '';
  for (const note of report.notes) {
    const li = document.createElement('li');
    li.textContent = note;
    notes.appendChild(li);
  }

  installButton.classList.toggle('hidden', report.verdict === 'BLOCKED');
}

async function onCheck() {
  const select = document.getElementById('app-select');
  const pathInput = document.getElementById('path-input');
  const target = pathInput.value.trim() || select.value;
  if (!target) {
    return;
  }

  try {
    const isRawPath = target.startsWith('/');
    const abis = isRawPath
      ? await inspectAbisFromApkFile(target)
      : await inspectAbisFromDumpsys(target);
    selectedPath = isRawPath ? target : await resolveInstalledPackagePath(target);

    const report = checkCompatibility(abis, deviceAbis, bridgeActive);
    renderResult(report);
  } catch (err) {
    renderResult({ verdict: 'BLOCKED', notes: [String((err && err.message) || err)] });
  }
}

async function onInstall() {
  if (!selectedPath) {
    return;
  }
  try {
    await run('pm install -r "' + selectedPath + '"');
    if (typeof ksu !== 'undefined' && typeof ksu.toast === 'function') {
      ksu.toast('Install finished, check the log below for details.');
    }
  } catch (err) {
    if (typeof ksu !== 'undefined' && typeof ksu.toast === 'function') {
      ksu.toast('Install failed: ' + ((err && err.message) || err));
    }
  }
}

function onAdvancedToggle() {
  const checked = document.getElementById('advanced-toggle').checked;
  document.getElementById('log').classList.toggle('hidden', !checked);
}

async function init() {
  document.getElementById('check-button').addEventListener('click', onCheck);
  document.getElementById('install-button').addEventListener('click', onInstall);
  document.getElementById('advanced-toggle').addEventListener('change', onAdvancedToggle);
  document.getElementById('app-select').addEventListener('change', () => {
    document.getElementById('check-button').disabled = false;
  });
  document.getElementById('path-input').addEventListener('input', () => {
    document.getElementById('check-button').disabled = false;
  });

  if (!hasRootBridge()) {
    document.getElementById('status-body').textContent =
      'No root bridge found. Open this page from Magisk, KernelSU, KernelSU-Next, or APatch, not a regular browser.';
    return;
  }

  await refreshStatus();
  await loadInstalledApps();
}

init();
