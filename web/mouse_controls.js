// Browser capture is asynchronous and must start in a user gesture. Keep input totals
// outside raylib's event callbacks so multiple events in one frame are preserved.
function installBoatMouseControls(module, canvas, document, window) {
  let wanted = false, allowed = false, locked = false, pending = false, released = false;
  let movement = 0, scroll = 0, toggle = false, failed = false;
  function fail(report = true) {
    if (report && wanted) failed = true;
    if (wanted) released = true;
    wanted = false;
    pending = false;
    movement = 0;
  }
  function setCapture(value, available) {
    allowed = available;
    wanted = value;
    if (!wanted || !allowed) {
      movement = 0;
      if (document.pointerLockElement === canvas) document.exitPointerLock();
    }
  }
  function request() {
    if (!wanted || !allowed || locked || pending) return;
    if (!canvas.requestPointerLock) { fail(); return; }
    pending = true;
    try {
      // Older browsers return void, newer ones return a Promise.
      const result = canvas.requestPointerLock();
      if (result && result.catch) result.catch(fail);
    } catch (_) { fail(); }
  }
  document.addEventListener('pointerlockchange', () => {
    const wasLocked = locked, wasPending = pending;
    locked = document.pointerLockElement === canvas;
    pending = false;
    movement = 0;
    if (locked && (!wanted || !allowed)) document.exitPointerLock();
    if (!locked && wanted && allowed && (wasLocked || wasPending)) fail(false);
  });
  document.addEventListener('pointerlockerror', fail);
  document.addEventListener('mousemove', event => {
    if (locked && wanted && allowed && Number.isFinite(event.movementX))
      movement += event.movementX;
  });
  canvas.addEventListener('keydown', event => {
    if (event.code !== 'KeyM' || event.repeat || !allowed) return;
    toggle = !toggle; // Preserve short presses even between slow browser frames.
    if (wanted) setCapture(false, allowed);
    else { wanted = true; request(); }
  });
  canvas.addEventListener('click', request);
  canvas.addEventListener('wheel', event => {
    if (!module['boatGameStarted'] || (document.activeElement !== canvas && !locked)) return;
    event.preventDefault();
    if (event.ctrlKey || !Number.isFinite(event.deltaY)) return; // Pinch zoom isn't thrust.
    const units = event.deltaMode === 1 ? event.deltaY / 3
      : event.deltaMode === 2 ? event.deltaY : event.deltaY / 100;
    scroll -= units; // Browser down-positive becomes boat forward-positive.
  }, { passive: false });
  window.addEventListener('blur', () => {
    movement = scroll = 0;
    if (wanted) fail(false);
    if (document.pointerLockElement === canvas) document.exitPointerLock();
  });
  module['boatMouse'] = {
    'state': (value, available) => {
      setCapture(!!value, !!available);
      const result = (locked ? 1 : 0) | (released ? 2 : 0) | (failed ? 4 : 0);
      released = failed = false;
      return result;
    },
    'toggle': () => { const value = toggle; toggle = false; return value; },
    'movement': () => { const value = movement; movement = 0; return value; },
    'scroll': () => { const value = scroll; scroll = 0; return value; }
  };
}
