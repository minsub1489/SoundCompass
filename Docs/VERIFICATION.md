# 검증 기록

- 검증일: 2026-10-03
- 엔진: Unreal Engine 5.8.2
- 빌드: SoundCompassEditor / Mac / Development — Succeeded
- 신호 및 중요도 계산: 47 checks passed
- 실제 게임 실행: 기존 main 맵, 1440×900 화면, 실제 출력 오디오 48 kHz / 2채널
- main 기존 Content 파일 변경 없음. UI 파일 10개는 origin/AHNINTAE의 Git blob과 일치.

```text
SoundCompass smoke test: PASS audio=1 direction=1 hud=1 priority=1 baseline=1 characterCenter=1 diagnostics=1 categoryRules=1 candidates=6 selected=3 RMS=0.03057 rate=48000 channels=2
SoundCompass priority check: important=0.908 envelope=0.0050; ambience=0.311 envelope=0.0348
SoundCompass controlled source: angle=90.00 distance=1.500m
```

검증 화면은 [SoundCompass-preview.png](SoundCompass-preview.png)입니다. 테스트용 소리를 추가한 화면이며, 참가자 평가 결과는 아닙니다.

프로젝트 ZIP에는 소스·설정·맵·음원·UI·문서가 포함되며, Git 메타데이터와 생성된 빌드 파일은 제외합니다.
