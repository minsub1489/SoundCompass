# 검증 기록

- 검증일: 2026-10-05 (Asia/Seoul)
- 엔진: Unreal Engine 5.8.2
- 빌드: SoundCompassEditor / Mac / Development — Succeeded
- 신호 및 중요도 계산: 47 checks passed
- 방향 범위 및 표시 유지·감쇠: 32 presentation checks passed
- 방향 파형: 소리별 엔벌로프 이력, 기본 50° 고정 범위
- 표시 수명: 마지막 가청 측정 후 0.6초 유지 + 0.8초 smoothstep 감쇠
- 아이콘: 기본 52 크기, 종류별 색상 배지, 배지 테두리도 함께 감쇠
- 실제 게임 실행: 기존 main 맵, 1440×900 화면, 실제 출력 오디오 48 kHz / 2채널
- main 기존 Content 파일 변경 없음. UI 파일 10개는 origin/AHNINTAE의 Git blob과 일치.

```text
SoundCompass smoke test: PASS audio=1 direction=1 hud=1 priority=1 baseline=1 characterCenter=1 diagnostics=1 categoryRules=1 readableIcons=1 candidates=6 selected=3 RMS=0.03116 rate=48000 channels=2
SoundCompass priority check: important=0.908 envelope=0.0050; ambience=0.311 envelope=0.0350
SoundCompass controlled source: angle=90.00 distance=1.500m
SoundCompass retention stage: hold PASS stopped=1 alpha=1.000 samples=9
SoundCompass retention stage: fade PASS stopped=1 alpha=0.726 samples=9
SoundCompass retention test: PASS hold=1 fade=1 removed=1
```

검증 화면은 [동시 소리·색상 아이콘](SoundCompass-preview.png), [짧은 발소리 유지](SoundCompass-hold.png), [짧은 발소리 감쇠](SoundCompass-fade.png)입니다. 반복음에 총소리·폭발·발소리 종류를 지정한 제어된 검사 화면이며, 참가자 평가 결과는 아닙니다. 유지·감쇠 검사는 실제 짧은 발소리를 한 번 재생하고 오디오 컴포넌트가 종료된 상태에서 확인했습니다. 다른 맵 소리가 계속 재생되어도 해당 소리의 이력과 감쇠는 독립적으로 유지됩니다.

Windows 빌드 및 패키징은 이번 검증에 포함되지 않았습니다.

프로젝트 ZIP에는 소스·설정·맵·음원·UI·문서가 포함되며, Git 메타데이터와 생성된 빌드 파일은 제외합니다.
