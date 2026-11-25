// Chrome on Windows occasionally reports one pointer-lock mousemove with a jump of hundreds of px; real flicks at
// normal polling rates arrive as many small events, so a single event this large is treated as noise.
const SPIKE_PX = 300;
export const isMouseSpike = (dx, dy) => Math.abs(dx) > SPIKE_PX || Math.abs(dy) > SPIKE_PX;

export function createInput(canvas) {
  const keys = new Set();
  const lockCallbacks = [];
  let dx = 0, dy = 0, firing = false, locked = false;
  const release = () => { keys.clear(); firing = false; };

  addEventListener('keydown', e => { if (locked) { keys.add(e.code); if (e.code === 'Space') e.preventDefault(); } });
  addEventListener('keyup', e => keys.delete(e.code));
  addEventListener('blur', release);
  document.addEventListener('mousemove', e => {
    if (locked && !isMouseSpike(e.movementX, e.movementY)) { dx += e.movementX; dy += e.movementY; }
  });
  canvas.addEventListener('mousedown', e => { if (locked && e.button === 0) firing = true; });
  addEventListener('mouseup', e => { if (e.button === 0) firing = false; });
  document.addEventListener('pointerlockchange', () => {
    locked = document.pointerLockElement === canvas;
    if (!locked) release();
    lockCallbacks.forEach(cb => cb(locked));
  });

  return {
    keys,
    consumeMouse() { const r = { dx, dy }; dx = dy = 0; return r; },
    get firing() { return firing; },
    get locked() { return locked; },
    onLockChange(cb) { lockCallbacks.push(cb); },
    // Raw mouse input (no OS acceleration) when the browser supports it; plain lock otherwise.
    async requestLock() {
      try { await canvas.requestPointerLock({ unadjustedMovement: true }); } catch { await canvas.requestPointerLock(); }
    },
  };
}
