import * as THREE from 'three';

// Simple held-weapon models, attached to a character capsule mesh (capsule centre = origin, forward = -z).
const SHAPES = {
  playerRifle: { size: [0.07, 0.12, 0.85], color: 0x222222 },
  rifle: { size: [0.07, 0.12, 0.85], color: 0x222222 },
  shotgun: { size: [0.11, 0.13, 0.6], color: 0x5a3a22 },
  sniper: { size: [0.06, 0.09, 1.4], color: 0x2c3a2c },
  knife: { size: [0.03, 0.06, 0.35], color: 0xd0d0d0 },
};
// Outside the 0.35 m capsule radius so the weapon is visible from behind (third-person camera) too.
const GRIP = [0.45, 0.3, -0.1];
const FLASH_S = 0.08, SWING_S = 0.25, RECOIL_S = 0.12, RECOIL_M = 0.12;

export function createWeaponModel(id) {
  const { size, color } = SHAPES[id];
  const pivot = new THREE.Group();
  pivot.position.set(...GRIP);
  const body = new THREE.Mesh(new THREE.BoxGeometry(...size), new THREE.MeshLambertMaterial({ color }));
  body.position.z = -size[2] / 2;
  pivot.add(body);
  const tip = new THREE.Object3D();
  tip.position.z = -size[2];
  pivot.add(tip);
  const flashMesh = new THREE.Mesh(new THREE.SphereGeometry(0.2, 8, 6), new THREE.MeshBasicMaterial({ color: 0xffd040 }));
  flashMesh.visible = false;
  tip.add(flashMesh);
  let flashT = 0, swingT = 0, recoilT = 0;

  return {
    group: pivot,
    tipWorld: () => tip.getWorldPosition(new THREE.Vector3()).toArray(),
    flash() { flashT = FLASH_S; recoilT = RECOIL_S; },
    swing() { swingT = SWING_S; },
    update(dt) {
      flashT = Math.max(0, flashT - dt);
      flashMesh.visible = flashT > 0;
      swingT = Math.max(0, swingT - dt);
      recoilT = Math.max(0, recoilT - dt);
      // Kick back along the barrel and tip up, easing out.
      const k = recoilT / RECOIL_S;
      pivot.position.z = GRIP[2] + RECOIL_M * k;
      pivot.rotation.x = 0.25 * k;
      // Knife arc: sweep from the right side across the front.
      pivot.rotation.y = swingT > 0 ? -1.3 + 2.6 * (swingT / SWING_S) : 0;
    },
  };
}

// Short-lived shot line; returns an updater that removes it when expired.
export function createTracer(scene) {
  const geo = new THREE.BufferGeometry().setFromPoints([new THREE.Vector3(), new THREE.Vector3()]);
  const line = new THREE.Line(geo, new THREE.LineBasicMaterial({ color: 0xfff2a0 }));
  line.visible = false;
  scene.add(line);
  let t = 0;
  return {
    show(from, to) { geo.setFromPoints([new THREE.Vector3(...from), new THREE.Vector3(...to)]); t = FLASH_S; line.visible = true; },
    update(dt) { t = Math.max(0, t - dt); line.visible = t > 0; },
  };
}
