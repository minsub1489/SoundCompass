// Temporary Task 4 viewer: orbit around the practice map. Replaced in Task 5.
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import RAPIER from '@dimforge/rapier3d-compat';
import { createPhysics } from './physics.js';
import { buildMap } from './maps/build.js';
import { MAPS } from './maps/index.js';

await RAPIER.init();
const canvas = document.getElementById('game');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(70, 1, 0.1, 300);
camera.position.set(20, 18, -24);
const controls = new OrbitControls(camera, canvas);
const physics = createPhysics(RAPIER);
buildMap(MAPS.practice, { scene, physics });

function resize() {
  renderer.setSize(innerWidth, innerHeight, false);
  camera.aspect = innerWidth / innerHeight;
  camera.updateProjectionMatrix();
}
addEventListener('resize', resize);
resize();
renderer.setAnimationLoop(() => { controls.update(); renderer.render(scene, camera); });
