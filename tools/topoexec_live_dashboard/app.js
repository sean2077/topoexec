(() => {
  const token = document.body.dataset.token || new URLSearchParams(location.search).get('token') || '';
  const set = (id, value) => { document.querySelector(`#${id} pre`).textContent = value; };
  const raw = [];
  const cap = 1000;
  fetch(`/snapshot?token=${encodeURIComponent(token)}`).then(r => r.json()).then(snapshot => {
    set('overview', JSON.stringify({
      runtime: snapshot.final_summary?.runtime_ok,
      observe_level: snapshot.observe_level,
      exactness: snapshot.exactness,
      observer_dropped_event_count: snapshot.observer_dropped_event_count,
      assertions_ok: snapshot.final_summary?.assertions_ok
    }, null, 2));
    set('topology', JSON.stringify(snapshot.manifest?.files || {}, null, 2));
    set('channels', 'Waiting for channel events...');
    set('triggers', 'Waiting for trigger summary events...');
    set('loops', 'Waiting for CompositeLoop events...');
    set('assertions', 'Waiting for assertion events...');
    set('health', 'Waiting for health/error events...');
  });
  const source = new EventSource(`/events?token=${encodeURIComponent(token)}`);
  source.addEventListener('observe', event => {
    const record = JSON.parse(event.data);
    raw.push(record);
    while (raw.length > cap) raw.shift();
    set('raw', JSON.stringify(raw.slice(-50), null, 2));
    if (record.kind?.startsWith('component_') || record.kind?.startsWith('run_')) set('timeline', JSON.stringify(record, null, 2));
    if (record.kind?.startsWith('channel_')) set('channels', JSON.stringify(record, null, 2));
    if (record.kind?.startsWith('trigger_')) set('triggers', JSON.stringify(record, null, 2));
    if (record.kind?.startsWith('loop_')) set('loops', JSON.stringify(record, null, 2));
    if (record.kind?.startsWith('assertion_')) set('assertions', JSON.stringify(record, null, 2));
    if (record.kind === 'runtime_error' || record.kind === 'health_event' || record.kind === 'observer_drop_summary') set('health', JSON.stringify(record, null, 2));
    if (record.kind === 'final_summary') set('overview', JSON.stringify(record, null, 2));
  });
})();
