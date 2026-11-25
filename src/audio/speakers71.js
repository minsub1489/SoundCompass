// WAVE/Windows 7.1 channel order.
export const SPEAKER_ORDER_71 = ['FL', 'FR', 'FC', 'LFE', 'BL', 'BR', 'SL', 'SR'];
// Degrees, positive = right of the listener.
export const SPEAKER_AZIMUTH_71 = { FC: 0, FR: 30, SR: 90, BR: 150, BL: -150, SL: -90, FL: -30 };

// Speakers around the circle starting at BL; BL appears again at +210 so the back pair wraps.
const RING = [['BL', -150], ['SL', -90], ['FL', -30], ['FC', 0], ['FR', 30], ['SR', 90], ['BR', 150], ['BL', 210]];

export const PANNER = { refDistance: 2, rolloffFactor: 1, maxDistance: 120 };

export function speakerGains71(azimuthDeg) {
  let az = ((((azimuthDeg + 180) % 360) + 360) % 360) - 180;
  if (az < -150) az += 360;
  const gains = new Array(8).fill(0);
  for (let i = 0; i < RING.length - 1; i++) {
    const [a, aDeg] = RING[i], [b, bDeg] = RING[i + 1];
    if (az >= aDeg && az < bDeg) {
      const t = ((az - aDeg) / (bDeg - aDeg)) * (Math.PI / 2);
      gains[SPEAKER_ORDER_71.indexOf(a)] = Math.cos(t);
      gains[SPEAKER_ORDER_71.indexOf(b)] += Math.sin(t);
      break;
    }
  }
  return gains;
}

export const canUse71 = maxChannelCount => maxChannelCount >= 8;

// Same curve as PannerNode distanceModel 'inverse', so both outputs fall off identically.
export function inverseDistanceGain(d, { refDistance, rolloffFactor, maxDistance } = PANNER) {
  const dist = Math.min(Math.max(d, refDistance), maxDistance);
  return refDistance / (refDistance + rolloffFactor * (dist - refDistance));
}
