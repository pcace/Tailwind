#pragma once

#include <Arduino.h>

const char webInterface[] PROGMEM = R"HTML(<!doctype html>
<html lang="de">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="color-scheme" content="light dark">
  <title>Tailwind</title>
  <style>
    :root { --bg:#f3f5f3; --panel:#fff; --text:#17231e; --muted:#51645a; --line:#cdd8d0; --accent:#185d44; --on-accent:#fff; }
    @media (prefers-color-scheme: dark) {
      :root { --bg:#14211b; --panel:#1d2e25; --text:#eef6ef; --muted:#b1c4b6; --line:#4a6254; --accent:#9fddbc; --on-accent:#123523; }
    }
    * { box-sizing:border-box; }
    body { margin:0; background:var(--bg); color:var(--text); font:16px/1.45 system-ui,sans-serif; }
    main { max-width:44rem; margin:auto; padding:1.25rem 1rem 2.5rem; }
    h1,h2,p { margin:0; }
    h1 { font-size:1.6rem; letter-spacing:-.04em; }
    h2 { font-size:1.12rem; margin-bottom:.65rem; }
    section { margin-top:1.5rem; }
    .bar,.buttons { display:flex; align-items:center; flex-wrap:wrap; gap:.5rem; }
    .bar { justify-content:space-between; }
    .subtle { color:var(--muted); font-size:.9rem; }
    .panel { background:var(--panel); border:1px solid var(--line); border-radius:.85rem; padding:1rem; }
    .speed { display:block; font-variant-numeric:tabular-nums; font-size:clamp(2.5rem,12vw,4.25rem); line-height:1.13; font-weight:650; }
    .speed small { font-size:1.05rem; color:var(--muted); font-weight:400; }
    .modes { display:grid; grid-template-columns:repeat(3,1fr); gap:.5rem; margin-top:.8rem; }
    button { min-height:2.75rem; padding:.5rem .85rem; color:var(--text); background:var(--panel); border:1px solid var(--line); border-radius:.55rem; font:inherit; cursor:pointer; }
    button:disabled { opacity:.6; cursor:default; }
    button[aria-pressed="true"], .primary { background:var(--accent); color:var(--on-accent); border-color:var(--accent); }
    dl { margin:.8rem 0 0; display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:.5rem; }
    dl div { background:var(--panel); border:1px solid var(--line); border-radius:.65rem; padding:.6rem .7rem; }
    dt { color:var(--muted); font-size:.85rem; }
    dd { margin:0; font-size:1.15rem; font-weight:600; font-variant-numeric:tabular-nums; }
    #mode-message,#record-message,#storage-message { margin-top:.5rem; }
    #rides { margin:.6rem 0 0; padding:0; list-style:none; }
    #rides li { display:flex; justify-content:space-between; align-items:center; gap:.65rem; border-top:1px solid var(--line); padding:.55rem 0; }
    @media (min-width:38rem) { dl { grid-template-columns:repeat(3,minmax(0,1fr)); } }
  </style>
</head>
<body>
<main>
  <header class="bar">
    <h1>Tailwind</h1>
    <p id="connection" class="subtle" role="status">Verbinde mit dem Fahrrad …</p>
  </header>
  <section aria-label="Livewerte und Unterstützung">
    <div class="panel">
      <p class="subtle">Tempo</p>
      <output class="speed"><span id="speed">–</span> <small>km/h</small></output>
      <p id="motor-state" class="subtle">Motorstatus unbekannt</p>
      <div class="modes" role="group" aria-label="Unterstützungsmodus">
        <button type="button" data-mode="0" aria-pressed="false" disabled>Urban</button>
        <button type="button" data-mode="1" aria-pressed="false" disabled>Max</button>
        <button type="button" data-mode="2" aria-pressed="false" disabled>Aus</button>
      </div>
      <p id="mode-message" class="subtle" role="status" hidden></p>
    </div>
    <dl id="metrics"></dl>
  </section>
  <section aria-labelledby="record-heading">
    <h2 id="record-heading">Fahrtaufzeichnung</h2>
    <div class="panel">
      <div class="buttons">
        <button id="start" class="primary" type="button" disabled>Aufzeichnung starten</button>
        <button id="stop" type="button" disabled>Stoppen</button>
      </div>
      <p id="record-message" role="status"></p>
      <p class="subtle">Lass die Seite während der Fahrt im Vordergrund und das Gerät entsperrt. Messwerte bleiben in diesem Browser gespeichert.</p>
    </div>
  </section>
  <section aria-labelledby="rides-heading">
    <h2 id="rides-heading">Gespeicherte Fahrten</h2>
    <p id="storage-message" class="subtle" role="status">Lade Fahrten …</p>
    <ul id="rides"></ul>
  </section>
</main>
<script>
  'use strict';
  const $ = id => document.getElementById(id);
  const modeButtons = [...document.querySelectorAll('[data-mode]')];
  const modeNames = ['Urban', 'Max', 'Aus'];
  const labels = [
    ['rider_w', 'Fahrerleistung', 'W', 0],
    ['cadence_rpm', 'Trittfrequenz', 'U/min', 0],
    ['torque_nm', 'Pedaldrehmoment', 'Nm', 1],
    ['target_motor_a', 'Motor-Sollstrom', 'A', 1],
    ['motor_a', 'Motor-Iststrom', 'A', 1],
    ['battery_a', 'Akkustrom', 'A', 1],
    ['battery_v', 'Akkuspannung', 'V', 1],
    ['battery_percent', 'Akkustand', '%', 0],
    ['duty', 'Tastgrad', '%', 1],
    ['watt_hours', 'Entnommen seit Start', 'Wh', 1],
    ['watt_hours_charged', 'Rückgespeist seit Start', 'Wh', 1],
    ['temp_fet_c', 'VESC-Temperatur', '°C', 1]
  ];
  const columns = [
    'uptime_ms', 'vesc_valid', 'speed_kmh', 'mode', 'motor_enabled',
    ...labels.map(([key]) => key)
  ];
  const metrics = labels.map(([key, name, unit, decimals]) => {
    const row = document.createElement('div');
    const label = document.createElement('dt');
    const value = document.createElement('dd');
    label.textContent = name;
    value.textContent = '–';
    row.append(label, value);
    $('metrics').append(row);
    return {key, unit, decimals, value};
  });
  const number = (value, decimals) =>
    Number.isFinite(value) ? value.toLocaleString('de-DE', {maximumFractionDigits:decimals}) : '–';
  let latest = null;
  let modeBusy = false;
  let requestedMode = null;
  let db;
  let activeRide = null;
  let recordBusy = false;
  let pendingSample = Promise.resolve();

  function updateButtons() {
    for (const button of modeButtons) {
      button.disabled = !latest || modeBusy;
      button.setAttribute('aria-pressed', String(latest?.mode === Number(button.dataset.mode)));
    }
    $('start').disabled = !db || !latest || recordBusy || !!activeRide;
    $('stop').disabled = recordBusy || !activeRide;
  }

  function modeMessage(message) {
    $('mode-message').textContent = message;
    $('mode-message').hidden = !message;
  }

  function renderStatus(data) {
    $('connection').textContent = data
      ? (data.vesc_valid ? 'Verbunden' : 'Verbunden · VESC-Daten fehlen')
      : 'Keine Verbindung zum ESP';
    $('speed').textContent = number(data?.speed_kmh, 1);
    $('motor-state').textContent = data
      ? (data.motor_enabled ? 'Motor unterstützt' : 'Motor unterstützt gerade nicht')
      : 'Motorstatus unbekannt';
    for (const item of metrics) {
      let value = data?.[item.key];
      if (item.key === 'duty' && Number.isFinite(value)) value *= 100;
      item.value.textContent = Number.isFinite(value) ? number(value, item.decimals) + ' ' + item.unit : '–';
    }
    if (requestedMode) {
      if (data?.mode === requestedMode.id) {
        modeMessage(modeNames[requestedMode.id] + ' aktiv.');
        requestedMode = null;
      } else if (performance.now() > requestedMode.deadline) {
        modeMessage('Moduswechsel noch nicht bestätigt.');
        requestedMode = null;
      }
    }
    updateButtons();
  }

  async function request(path, options = {}) {
    const controller = new AbortController();
    const timeout = setTimeout(() => controller.abort(), 3000);
    try {
      const response = await fetch(path, {...options, cache:'no-store', signal:controller.signal});
      if (!response.ok) throw new Error('HTTP ' + response.status);
      return options.method === 'POST' ? response.status : await response.json();
    } finally {
      clearTimeout(timeout);
    }
  }

  for (const button of modeButtons) {
    button.addEventListener('click', async () => {
      const mode = Number(button.dataset.mode);
      modeBusy = true;
      requestedMode = null;
      modeMessage('Moduswechsel läuft …');
      updateButtons();
      try {
        const status = await request('/api/mode', {
          method:'POST',
          headers:{'Content-Type':'application/x-www-form-urlencoded', 'X-Tailwind-Request':'1'},
          body:'mode=' + mode
        });
        if (status !== 202) throw new Error('Unerwartete Antwort');
        requestedMode = {id:mode, deadline:performance.now() + 3000};
        modeMessage(modeNames[mode] + ' angefordert.');
      } catch {
        modeMessage('Moduswechsel konnte nicht bestätigt werden.');
      } finally {
        modeBusy = false;
        updateButtons();
      }
    });
  }

  function openStorage() {
    return new Promise((resolve, reject) => {
      const opening = indexedDB.open('tailwind-ride-log', 1);
      opening.onupgradeneeded = () => {
        opening.result.createObjectStore('rides', {keyPath:'id', autoIncrement:true});
        const samples = opening.result.createObjectStore('samples', {autoIncrement:true});
        samples.createIndex('rideId', 'rideId');
      };
      opening.onsuccess = () => resolve(opening.result);
      opening.onerror = () => reject(opening.error);
    });
  }

  function transaction(stores, mode, execute) {
    return new Promise((resolve, reject) => {
      const tx = db.transaction(stores, mode);
      try {
        const operation = execute(tx);
        tx.oncomplete = () => resolve(operation?.result);
        tx.onabort = () => reject(tx.error);
      } catch (error) {
        tx.abort();
        reject(error);
      }
    });
  }

  function energyText(ride) {
    if (ride.reset) return 'Wh nicht vergleichbar (Steuergerät oder Zähler neu gestartet)';
    if ((ride.energySamples || 0) < 2) return 'Wh noch nicht verfügbar';
    const drawn = ride.lastEnergy[0] - ride.firstEnergy[0];
    const returned = ride.lastEnergy[1] - ride.firstEnergy[1];
    return number(drawn - returned, 2) + ' Wh netto';
  }

  async function saveSample(ride, data) {
    const time = Date.now();
    const next = {...ride, count:ride.count + 1, lastUptime:data.uptime_ms};
    if (ride.lastUptime !== undefined && data.uptime_ms < ride.lastUptime) next.reset = true;
    if (data.vesc_valid && Number.isFinite(data.watt_hours) && Number.isFinite(data.watt_hours_charged)) {
      const energy = [data.watt_hours, data.watt_hours_charged];
      if (ride.lastEnergy && (energy[0] < ride.lastEnergy[0] || energy[1] < ride.lastEnergy[1])) next.reset = true;
      next.firstEnergy = ride.firstEnergy || energy;
      next.lastEnergy = energy;
      next.energySamples = (ride.energySamples || 0) + 1;
    }
    await transaction(['rides', 'samples'], 'readwrite', tx => {
      tx.objectStore('samples').add({rideId:ride.id, time, data});
      tx.objectStore('rides').put(next);
    });
    Object.assign(ride, next);
    if (activeRide === ride) $('record-message').textContent = 'Aufzeichnung läuft · ' + energyText(ride);
  }

  async function exportRide(ride) {
    const samples = await transaction('samples', 'readonly',
      tx => tx.objectStore('samples').index('rideId').getAll(ride.id));
    const csv = ['recorded_at,' + columns.join(',')];
    for (const sample of samples) {
      const values = columns.map(field => {
        const value = sample.data[field];
        return Number.isFinite(value) || typeof value === 'boolean' ? String(value) : '';
      });
      csv.push([new Date(sample.time).toISOString(), ...values].join(','));
    }
    const url = URL.createObjectURL(new Blob([csv.join('\r\n') + '\r\n'], {type:'text/csv;charset=utf-8'}));
    const link = document.createElement('a');
    link.href = url;
    link.download = 'tailwind-fahrt-' + ride.id + '.csv';
    document.body.append(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  async function showRides() {
    const rides = await transaction('rides', 'readonly', tx => tx.objectStore('rides').getAll());
    $('rides').replaceChildren();
    $('storage-message').textContent = rides.length ? '' : 'Noch keine Fahrten gespeichert.';
    for (const ride of rides.reverse()) {
      const row = document.createElement('li');
      const details = document.createElement('div');
      const date = document.createElement('p');
      const summary = document.createElement('p');
      const exportButton = document.createElement('button');
      date.textContent = new Date(ride.start).toLocaleString('de-DE');
      summary.className = 'subtle';
      summary.textContent = (ride.end ? '' : 'Unterbrochen · ') + ride.count + ' Messwerte · ' + energyText(ride);
      exportButton.type = 'button';
      exportButton.textContent = 'CSV';
      exportButton.setAttribute('aria-label', 'CSV für Fahrt vom ' + date.textContent + ' speichern');
      exportButton.disabled = !ride.count;
      exportButton.addEventListener('click', () => exportRide(ride).catch(() => {
        $('storage-message').textContent = 'CSV-Export fehlgeschlagen.';
      }));
      details.append(date, summary);
      row.append(details, exportButton);
      $('rides').append(row);
    }
  }

  $('start').addEventListener('click', async () => {
    recordBusy = true;
    updateButtons();
    try {
      const ride = {start:Date.now(), count:0};
      ride.id = await transaction('rides', 'readwrite', tx => tx.objectStore('rides').add(ride));
      activeRide = ride;
      $('record-message').textContent = 'Aufzeichnung läuft.';
    } catch {
      $('record-message').textContent = 'Aufzeichnung konnte nicht gestartet werden.';
    } finally {
      recordBusy = false;
      updateButtons();
    }
  });

  $('stop').addEventListener('click', async () => {
    const ride = activeRide;
    if (!ride) return;
    activeRide = null;
    recordBusy = true;
    updateButtons();
    try {
      await pendingSample;
      ride.end = Date.now();
      await transaction('rides', 'readwrite', tx => tx.objectStore('rides').put(ride));
      $('record-message').textContent = 'Fahrt gespeichert · ' + energyText(ride);
    } catch {
      $('record-message').textContent = 'Aufzeichnung unterbrochen. Bereits gespeicherte Messwerte bleiben erhalten.';
    } finally {
      recordBusy = false;
      updateButtons();
      showRides().catch(() => { $('storage-message').textContent = 'Fahrten konnten nicht geladen werden.'; });
    }
  });

  async function poll() {
    const start = performance.now();
    try {
      latest = await request('/api/status');
      if (!Number.isFinite(latest?.uptime_ms)) latest = null;
    } catch {
      latest = null;
    }
    renderStatus(latest);
    if (latest && activeRide) {
      pendingSample = saveSample(activeRide, latest);
      try {
        await pendingSample;
      } catch {
        activeRide = null;
        updateButtons();
        $('record-message').textContent = 'Speichern fehlgeschlagen. Bereits gespeicherte Messwerte bleiben erhalten.';
        showRides().catch(() => { $('storage-message').textContent = 'Fahrten konnten nicht geladen werden.'; });
      }
    }
    setTimeout(poll, Math.max(0, 1000 - (performance.now() - start)));
  }

  openStorage().then(opened => {
    db = opened;
    updateButtons();
    return showRides();
  }).catch(() => {
    $('storage-message').textContent = 'Dieser Browser konnte den Fahrtspeicher nicht laden.';
  });
  poll();
</script>
</body>
</html>)HTML";
