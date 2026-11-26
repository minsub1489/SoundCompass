// Chrome on Windows occasionally reports one pointer-lock mousemove with a jump of hundreds of px out of near rest.
// A real fast turn ramps up (Chrome coalesces events per frame, so big values are normal mid-flick), so only a
// large event far bigger than the previous one counts as a spike.
const SPIKE_PX = 250, SPIKE_RATIO = 6, REST_PX = 20, REST_GAP_MS = 100;
export const isMouseSpike = (dx, dy, prevMag) => {
  const mag = Math.max(Math.abs(dx), Math.abs(dy));
  return mag > SPIKE_PX && mag > SPIKE_RATIO * Math.max(prevMag, REST_PX);
};

export function createInput(canvas) {
  const keys = new Set();
  const lockCallbacks = [];
  let dx = 0, dy = 0, firing = false, locked = false;
  const release = () => { keys.clear(); firing = false; };

  addEventListener('keydown', e => { if (locked) { keys.add(e.code); if (e.code === 'Space') e.preventDefault(); } });
  addEventListener('keyup', e => keys.delete(e.code));
  addEventListener('blur', release);
  let prevMag = 0, prevTime = 0;
  document.addEventListener('mousemove', e => {
    if (!locked) return;
    if (e.timeStamp - prevTime > REST_GAP_MS) prevMag = 0; // the mouse was resting
    prevTime = e.timeStamp;
    if (isMouseSpike(e.movementX, e.movementY, prevMag)) return;
    prevMag = Math.max(Math.abs(e.movementX), Math.abs(e.movementY));
    dx += e.movementX; dy += e.movementY;
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
