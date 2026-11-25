export function createInput(canvas) {
  const keys = new Set();
  const lockCallbacks = [];
  let dx = 0, dy = 0, firing = false, locked = false;
  const release = () => { keys.clear(); firing = false; };

  addEventListener('keydown', e => { if (locked) { keys.add(e.code); if (e.code === 'Space') e.preventDefault(); } });
  addEventListener('keyup', e => keys.delete(e.code));
  addEventListener('blur', release);
  document.addEventListener('mousemove', e => { if (locked) { dx += e.movementX; dy += e.movementY; } });
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
    requestLock() { return canvas.requestPointerLock(); },
  };
}
