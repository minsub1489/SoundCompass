export const CSV_HEADER = 'participant_id,condition,map,audio_output,started_at,crates,kills,deaths,damage_taken,surprise_hits';

export function csvField(value) {
  const s = String(value);
  return /[",\r\n]/.test(s) ? `"${s.replace(/"/g, '""')}"` : s;
}

export function toCsv(round) {
  const s = round.stats;
  const row = [round.participantId, round.condition, round.mapId, round.audioOutput, localIsoString(round.startedAt),
    s.crates, s.kills, s.deaths, s.damageTaken, s.surpriseHits].map(csvField).join(',');
  // BOM so Excel opens Korean text as UTF-8.
  return '﻿' + CSV_HEADER + '\r\n' + row + '\r\n';
}

const pad = n => String(n).padStart(2, '0');

// Local wall-clock time with its UTC offset, e.g. 2026-12-01T19:20:30+09:00, so it lines up with survey times.
export function localIsoString(d) {
  const off = -d.getTimezoneOffset(), sign = off >= 0 ? '+' : '-', a = Math.abs(off);
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}T${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}${sign}${pad(Math.floor(a / 60))}:${pad(a % 60)}`;
}

export function csvFilename(round) {
  const d = round.startedAt;
  const stamp = `${d.getFullYear()}${pad(d.getMonth() + 1)}${pad(d.getDate())}-${pad(d.getHours())}${pad(d.getMinutes())}${pad(d.getSeconds())}`;
  const safe = s => String(s).replace(/[\/:*?"<>|\s]/g, '_');
  return `${safe(round.participantId)}_${safe(round.mapId)}_${stamp}.csv`;
}

export function downloadCsv(round) {
  if (round.isPractice) return;
  const url = URL.createObjectURL(new Blob([toCsv(round)], { type: 'text/csv;charset=utf-8' }));
  const a = Object.assign(document.createElement('a'), { href: url, download: csvFilename(round) });
  document.body.append(a);
  a.click();
  a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
