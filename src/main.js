import * as THREE from 'three';
import RAPIER from '@dimforge/rapier3d-compat';
import { FIXED_DT, MAX_STEPS_PER_FRAME } from './config.js';
import { stepCount } from './fixedStep.js';
import { Round } from './round.js';
import { downloadCsv } from './results.js';
import { createPhysics } from './physics.js';
import { buildMap } from './maps/build.js';
import { MAPS } from './maps/index.js';
import { createInput } from './input.js';
import { createPlayer } from './player.js';
import { updateThirdPersonCamera } from './camera.js';
import * as ui from './ui.js';

const params = new URLSearchParams(location.search);
const map = MAPS[params.get('map')];

if (!map) {
  ui.showStartForm(({ participantId, condition, mapId, audioOutput }) => {
    location.search = new URLSearchParams({ pid: participantId, cond: condition, map: mapId, out: audioOutput }).toString();
  });
} else {
  await run(map);
}

async function run(map) {
  await RAPIER.init();
  const audioOutput = params.get('out') === '7.1' ? '7.1' : 'binaural';
  const canvas = document.getElementById('game');
  const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
  const scene = new THREE.Scene();
  const camera = new THREE.PerspectiveCamera(70, 1, 0.1, 300);
  const physics = createPhysics(RAPIER);
  const onSound = () => {}; // wired to audio in Task 7
  const built = buildMap(map, { scene, physics, onSound: (id, pos) => onSound(id, pos) });
  const input = createInput(canvas);
  let round = null;
  let player = null;
  let accumulator = 0, finished = false;

  const resize = () => {
    renderer.setSize(innerWidth, innerHeight, false);
    camera.aspect = innerWidth / innerHeight;
    camera.updateProjectionMatrix();
  };
  addEventListener('resize', resize);
  resize();

  ui.showStartGate(map.name, async () => {
    if (round) return;
    round = new Round({
      participantId: params.get('pid') ?? '', condition: params.get('cond') ?? '', mapId: map.id,
      audioOutput, isPractice: map.id === 'practice', startedAt: new Date(),
    });
    // Time only runs while the pointer is locked, so a failed lock leaves the round paused, not running blind.
    round.pause();
    player = createPlayer({ scene, physics, map, round, camera, surfaceAt: built.surfaceAt, onSound: (id, pos) => onSound(id, pos) });
    if (import.meta.env.DEV) window.__testbed = { round, player, physics, built, step };
    ui.showHud();
    requestLock();
  });

  const requestLock = () => Promise.resolve(input.requestLock()).catch(() => ui.showPaused(requestLock));

  function step(inputState) {
    player.fixedUpdate(FIXED_DT, inputState);
    built.doors.forEach(d => d.update(FIXED_DT, [player.position]));
    physics.step();
    round.tick(FIXED_DT);
  }

  input.onLockChange(locked => {
    if (!round || round.finished) return;
    if (locked) { round.resume(); ui.hidePaused(); } else { round.pause(); ui.showPaused(requestLock); }
  });

  const clock = new THREE.Clock();
  renderer.setAnimationLoop(() => {
    const frameDt = clock.getDelta();
    if (round && player && !finished) {
      player.look(input.consumeMouse());
      if (!round.paused) {
        const r = stepCount(accumulator, frameDt, FIXED_DT, MAX_STEPS_PER_FRAME);
        accumulator = r.accumulator;
        for (let i = 0; i < r.steps && !round.finished; i++) step(input);
      }
      player.syncMesh();
      updateThirdPersonCamera(camera, player.position, player.yaw, player.pitch, physics);
      ui.updateHud({ health: player.health, remainingS: round.remainingS, crates: round.stats.crates, ammo: player.ammo, reloading: player.reloading });
      if (round.finished) {
        finished = true;
        document.exitPointerLock();
        downloadCsv(round);
        ui.showResults(round, { onNext: () => { location.search = ''; } });
      }
    } else if (!round) {
      camera.position.set(map.playerSpawn[0], 12, map.playerSpawn[2] - 10);
      camera.lookAt(0, 0, 0);
    }
    renderer.render(scene, camera);
  });
}
