import { OCCLUSION } from '../config.js';

export function occlusionTarget(blocked) {
  return blocked ? { gain: OCCLUSION.gain, cutoffHz: OCCLUSION.cutoffHz } : { gain: 1, cutoffHz: OCCLUSION.openCutoffHz };
}

export function applyOcclusion(voice, target, now) {
  const end = now + OCCLUSION.rampSeconds;
  for (const [param, value] of [[voice.occGain.gain, target.gain], [voice.filter.frequency, target.cutoffHz]]) {
    const current = param.value;
    param.cancelScheduledValues(now);
    param.setValueAtTime(current, now);
    param.linearRampToValueAtTime(value, end);
  }
}
