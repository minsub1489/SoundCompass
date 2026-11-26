import { describe, it, expect } from 'vitest';
import { Round, isSurpriseHit } from '../src/round.js';
import { toCsv, csvField, csvFilename, CSV_HEADER, localIsoString } from '../src/results.js';

const mk = (o = {}) => new Round({ participantId: 'P01', condition: 'A', mapId: 'urban', audioOutput: 'binaural', isPractice: false, startedAt: new Date('2026-12-01T10:20:30Z'), ...o });

describe('Round', () => {
  it('본 라운드는 180초, 연습은 90초', () => {
    expect(mk().remainingS).toBe(180);
    expect(mk({ isPractice: true }).remainingS).toBe(90);
  });
  it('일시정지 중에는 시간이 줄지 않는다', () => {
    const r = mk(); r.tick(10); r.pause(); r.tick(50); r.resume(); r.tick(5);
    expect(r.remainingS).toBe(165);
  });
  it('시간이 다 되면 끝나고 이후 기록은 무시한다', () => {
    const r = mk(); r.tick(200);
    expect(r.finished).toBe(true); expect(r.remainingS).toBe(0);
    r.addKill(); expect(r.stats.kills).toBe(0);
  });
  it('피해와 기습 횟수를 함께 센다', () => {
    const r = mk(); r.addDamage(30, true); r.addDamage(10, false);
    expect(r.stats.damageTaken).toBe(40); expect(r.stats.surpriseHits).toBe(1);
  });
});

describe('isSurpriseHit', () => {
  it('화면 밖이거나 가려지면 기습', () => {
    expect(isSurpriseHit(false, false)).toBe(true);
    expect(isSurpriseHit(true, true)).toBe(true);
    expect(isSurpriseHit(true, false)).toBe(false);
  });
});

describe('CSV', () => {
  it('쉼표·따옴표·줄바꿈을 이스케이프한다', () => {
    expect(csvField('a,b')).toBe('"a,b"');
    expect(csvField('say "hi"')).toBe('"say ""hi"""');
    expect(csvField('줄\n바꿈')).toBe('"줄\n바꿈"');
    expect(csvField('시스템켬')).toBe('시스템켬');
  });
  it('BOM, 헤더, 데이터 한 줄을 쓴다', () => {
    const r = mk({ condition: '소리 끔, 시스템' }); r.addCrate(); r.addKill(); r.addDeath(); r.addDamage(25, true);
    expect(toCsv(r)).toBe('﻿' + CSV_HEADER + '\r\n' + 'P01,"소리 끔, 시스템",urban,binaural,' + localIsoString(r.startedAt) + ',1,1,1,25,1\r\n');
  });
  it('시작 시각은 오프셋이 붙은 현지 시각이다', () => {
    const d = new Date('2026-12-01T10:20:30Z');
    const s = localIsoString(d);
    expect(s).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}[+-]\d{2}:\d{2}$/);
    expect(Date.parse(s)).toBe(d.getTime());
    expect(Number(s.slice(11, 13))).toBe(d.getHours());
  });
  it('파일 이름의 금지 문자를 바꾼다', () => {
    expect(csvFilename(mk({ participantId: 'P 01/a' }))).toMatch(/^P_01_a_urban_\d{8}-\d{6}\.csv$/);
  });
});
