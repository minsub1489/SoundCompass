import * as THREE from 'three';
import { expandMap } from './expand.js';
import { GROUP } from '../physics.js';

const DOOR_OPEN_RADIUS_M = 1.5;
const DOOR_CLOSE_DELAY_S = 3;

function createDoor(spec, { scene, physics, onSound }) {
  const mesh = new THREE.Mesh(new THREE.BoxGeometry(...spec.size), new THREE.MeshLambertMaterial({ color: 0x6b4423 }));
  mesh.position.set(...spec.pos);
  mesh.rotation.y = spec.rotY ?? 0;
  scene.add(mesh);
  const collider = physics.addStaticBox(spec, GROUP.DOOR);
  const floorY = spec.pos[1] - spec.size[1] / 2;
  let open = false, idle = 0;
  const setOpen = v => {
    open = v;
    collider.setEnabled(!v);
    mesh.visible = !v;
    onSound(v ? 'door_open' : 'door_close', spec.pos);
  };
  return {
    pos: spec.pos,
    get open() { return open; },
    update(dt, characterPositions) {
      const near = characterPositions.some(p => Math.hypot(p[0] - spec.pos[0], p[2] - spec.pos[2]) < DOOR_OPEN_RADIUS_M && Math.abs(p[1] - floorY) < 2);
      if (near) { idle = 0; if (!open) setOpen(true); return; }
      if (open && (idle += dt) >= DOOR_CLOSE_DELAY_S) setOpen(false);
    },
  };
}

export function buildMap(map, { scene, physics, onSound = () => {} }) {
  scene.background = new THREE.Color(0x9fb4c7);
  scene.add(new THREE.HemisphereLight(0xffffff, 0x445544, 1.2));
  const sun = new THREE.DirectionalLight(0xffffff, 1.5);
  sun.position.set(30, 50, 20);
  scene.add(sun);

  const geo = new THREE.BoxGeometry(1, 1, 1);
  const mats = new Map();
  const mat = c => mats.get(c) ?? mats.set(c, new THREE.MeshLambertMaterial({ color: c })).get(c);
  const surfaces = new Map();
  const navMeshes = [];

  for (const b of expandMap(map)) {
    const mesh = new THREE.Mesh(geo, mat(b.color));
    mesh.position.set(...b.pos);
    mesh.scale.set(...b.size);
    mesh.rotation.y = b.rotY;
    mesh.updateMatrixWorld();
    scene.add(mesh);
    navMeshes.push(mesh);
    surfaces.set(physics.addStaticBox(b).handle, b.surface);
  }
  const doors = (map.doors ?? []).map(d => createDoor(d, { scene, physics, onSound }));
  return { navMeshes, doors, surfaceAt: collider => surfaces.get(collider?.handle) ?? map.surface };
}
