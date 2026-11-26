import { ROUND_SECONDS, PRACTICE_SECONDS } from './config.js';

export function isSurpriseHit(inView, lineOfSightBlocked) {
  return !inView || lineOfSightBlocked;
}

export class Round {
  constructor({ participantId, condition, mapId, audioOutput, isPractice, startedAt }) {
    Object.assign(this, { participantId, condition, mapId, audioOutput, isPractice, startedAt });
    this.durationS = isPractice ? PRACTICE_SECONDS : ROUND_SECONDS;
    this.elapsedS = 0;
    this._paused = false;
    this.stats = { crates: 0, kills: 0, deaths: 0, damageTaken: 0, surpriseHits: 0 };
  }

  tick(dt) { if (!this._paused) this.elapsedS = Math.min(this.durationS, this.elapsedS + dt); }
  pause() { this._paused = true; }
  resume() { this._paused = false; }
  get paused() { return this._paused; }
  get remainingS() { return this.durationS - this.elapsedS; }
  get finished() { return this.elapsedS >= this.durationS; }

  addCrate() { if (!this.finished) this.stats.crates++; }
  addKill() { if (!this.finished) this.stats.kills++; }
  addDeath() { if (!this.finished) this.stats.deaths++; }
  addDamage(amount, surprise) {
    if (this.finished) return;
    this.stats.damageTaken += amount;
    if (surprise) this.stats.surpriseHits++;
  }
}
