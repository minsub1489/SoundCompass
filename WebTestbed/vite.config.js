import { defineConfig } from 'vite';

// Rapier/recast embed their WASM in JS (~5 MB); fine for a locally served test build.
export default defineConfig({ base: './', build: { chunkSizeWarningLimit: 6000 } });
