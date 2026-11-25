export function createGun(def) {
  const unlimited = def.magSize === 0;
  let ammo = def.magSize, lastShot = -Infinity, reloadUntil = null;

  function update(now) {
    if (reloadUntil !== null && now >= reloadUntil) { ammo = def.magSize; reloadUntil = null; }
  }
  function startReload(now) {
    update(now);
    if (unlimited || reloadUntil !== null || ammo === def.magSize) return false;
    reloadUntil = now + def.reloadS;
    return true;
  }
  function tryFire(now) {
    update(now);
    if (reloadUntil !== null || now - lastShot < def.intervalS) return false;
    if (!unlimited && ammo <= 0) { startReload(now); return false; }
    lastShot = now;
    if (!unlimited) ammo--;
    return true;
  }
  return {
    def, tryFire, startReload, update,
    get ammo() { return ammo; },
    get reloading() { return reloadUntil !== null; },
  };
}
