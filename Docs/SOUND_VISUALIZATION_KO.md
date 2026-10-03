# SoundCompass 소리 시각화 기능

## 원래 프로젝트 목적과 구현 범위

게임공학 프로젝트의 「위험도 기반 소리 조절 논문」 대화에서 확인한 목표는 **게임에서 동시에 발생하는 소리에 우선순위를 매겨 중요한 정보만 시각화하는 것**입니다. 캐릭터 중심으로 원형 파형을 배치하고, 선별된 소리의 아이콘을 방향에 맞춰 원 위에 표시합니다.

참고 연구는 Jain et al., *Towards Sound Accessibility in Virtual Reality*, ICMI 2021, [원문 PDF](https://www.microsoft.com/en-us/research/wp-content/uploads/2021/08/towards_sound_accessibility_in_virtual_reality_icmi_2021.pdf), DOI: [10.1145/3462244.3479946](https://doi.org/10.1145/3462244.3479946)입니다. 연구는 모든 소리를 우선순위에 따라 표시하는 프로토타입을 제시하고, 동시 피드백의 인지 부하 문제를 후속 연구 과제로 다룹니다.

이 프로젝트에서는 이를 바탕으로 **전체 표시 조건과 중요도 기준·상위 K개 선별 조건**을 비교할 수 있게 구현했습니다. 아래 점수식·가중치·상위 3개는 프로젝트 구현을 위한 초기 설정이며, 논문에서 검증한 수치나 기존 대화에서 확정한 실험 결과가 아닙니다. 음량 자체를 변경하는 오디오 믹싱 기능은 추가하지 않았습니다.

## 실행과 화면

1. Unreal Engine **5.8**에서 `SoundCompass.uproject`를 열고 C++ 모듈을 재빌드합니다. Windows에서는 UE 5.8에 맞는 Visual Studio C++ 개발 도구가 필요합니다.
2. 기존 `/Game/ThirdPerson/Lvl_ThirdPerson` 맵에서 **Play**를 누릅니다.
3. 캐릭터 주위에 원형 파형과 방향 아이콘이 표시됩니다. 오른쪽 위에는 표시 조건, 감지/표시 개수, 중요도 순위·종류·거리가 표시됩니다.
4. **B**로 전체/선별 조건, **F1**로 상세 분석 화면, **V**로 시각화 전체를 전환합니다. 이동·카메라 조작은 기존 프로젝트와 같습니다.

원 중심은 플레이어 캐릭터의 월드 위치를 매 프레임 UI 좌표로 투영한 위치입니다. 화면 크기와 DPI를 반영하며 카메라 이동에도 캐릭터를 따라갑니다. 원형 파형은 실제 출력 PCM의 256개 점을 반지름 변화로 표현합니다. 크기 표현을 위한 고정 시각화 배율 `WaveformGain=12`가 있으며, 오디오 볼륨은 바뀌지 않습니다. 무음일 때는 기준 원만 남습니다.

`main`의 맵·외부 액터·음원 파일은 변경하지 않았습니다. `AHNINTAE`의 `Content/Ariel_NewMaterial` 10개 파일을 원본 그대로 가져왔습니다. `Ariel_WBP_Marker`의 `Image_Icon`·`Image_Wave`, 원형 머티리얼과 아이콘을 네이티브 `SoundVisualizationWidget`에서 재사용합니다. 전체 화면 Blueprint `Ariel_WBP_SoundCompass`의 그래프는 수정하지 않았습니다.

## 중요도 선별

먼저 실제 엔벌로프가 들어온 재생 인스턴스를 후보로 수집합니다. 일시정지·가상화된 소리와 추정 청취 레벨이 `SourceThresholdDb`보다 작은 소리는 두 표시 조건에서 모두 제외합니다. 등록만 됐거나 재생되지 않은 소리를 임의의 크기로 표시하지 않습니다.

기본 점수식은 다음과 같습니다. 결과 범위는 0~1이며 UI에서는 0~100으로 표시합니다.

```text
거리 점수 = clamp(1 - 거리(m) / 20, 0, 1)
크기 점수 = clamp((추정 청취 dB + 60) / 60, 0, 1)
중요도 = 0.70 × 종류별 중요도 + 0.20 × 거리 점수 + 0.10 × 크기 점수
```

가중치를 변경하면 합계로 나누어 정규화합니다. 모든 가중치가 0이면 종류별 중요도를 사용합니다. 같은 점수에서는 재생 인스턴스 ID로 순서를 고정합니다.

| 종류 | 기본 중요도 |
| --- | ---: |
| 폭발 | 1.00 |
| 총소리 | 0.95 |
| 경고음·사이렌 | 0.90 |
| 발소리 | 0.75 |
| 대화 | 0.70 |
| 문소리 | 0.55 |
| 그 외 소음 | 0.35 |
| 환경음·음악 | 0.10 |

종류는 맵의 `SoundCategory` 등 제작자가 지정한 메타데이터로 구분합니다. 파형으로 소리 종류나 실제 위험을 인식하는 AI 분류기는 아닙니다. 프로젝트 실험에서 소리가 의미하는 위험·목표 관련성에 맞춰 이 값을 조정해야 합니다.

- **중요 소리 선별**: 중요도 `0.50` 이상 후보 중 상위 `3`개만 표시합니다. 기준을 넘는 소리가 없으면 아이콘을 표시하지 않습니다.
- **전체 소리 표시**: 동일한 가청 후보를 모두 표시합니다. 우선순위 정렬·종류·방향·아이콘 표현은 같습니다. 상위 6개만 요약 목록에 나오며, 원 위의 아이콘은 모든 후보를 표시합니다.

큰 환경음의 최대 기본 점수는 `0.37`이고, 조용하고 먼 총소리도 종류 점수만으로 `0.665`를 확보하므로 음량만으로 순위를 결정하지 않습니다. 선별 기능은 화면 정보만 바꾸며 실제 소리 재생에는 영향을 주지 않습니다.

## 방향·크기·신호의 의미

- **방향**: 실제 오디오 리스너의 수평 방향을 기준으로 앞 0°, 오른쪽 +90°, 왼쪽 -90°, 뒤 ±180°입니다. 카메라 회전에 맞춰 아이콘도 회전합니다.
- **거리**: 오디오 리스너부터 재생 위치까지의 3차원 거리(m)입니다. 원의 시각적 중심은 캐릭터이고 측정 기준은 엔진의 실제 오디오 리스너입니다.
- **크기**: 소리별 실제 엔벌로프에 거리 감쇠 곡선과 가림 검사를 적용한 추정값입니다. 마커 크기·투명도와 분석 화면의 막대에 반영합니다. 물리적인 음압(dB SPL)이 아니며 모든 믹서 효과를 포함하는 정확한 소리별 출력 측정값은 아닙니다.
- **가림**: 소리 Attenuation의 Occlusion이 켜져 있고 동일 Trace Channel로 벽·닫힌 문을 감지한 경우입니다.
- **원형/분석 파형**: 전체 출력 오디오의 최근 2,048 프레임에서 가장 강한 채널을 256개 점으로 축약합니다. 48 kHz에서 약 42.7 ms입니다. 원형 표현에는 고정 배율을 적용하고 상세 파형에는 원래 진폭을 사용합니다.
- **주파수**: 채널별 FFT로 분석한 32개 로그 대역입니다. Hann 창과 DC 제거를 적용합니다. 40 Hz~16 kHz 또는 Nyquist 범위이며, 낮은 대역은 창 길이에 따른 분해능 제한이 있습니다.
- **dBFS**: 전체 출력의 RMS 디지털 레벨입니다. 바닥값은 -80 dB입니다. 가장 강한 FFT 빈을 주요 주파수로 표시합니다.

아이콘 선별은 개별 소리 데이터, 원형 파형·주파수는 전체 출력 데이터입니다. 따라서 선별에서 빠진 환경음과 등록하지 않은 게임 소리도 전체 출력 파형에는 포함됩니다. 반대 위상 스테레오나 오른쪽 채널만 있는 소리가 단순 다운믹스로 사라지지 않도록 채널을 각각 분석합니다.

## 기존 맵에 연결된 소리

`SoundTestEmitter`의 반복음·랜덤 단발음·발소리, `SoundTestDoor`의 문 열기·닫기 소리와 `AmbientSound`가 자동으로 등록됩니다. 움직이는 반복음은 Audio Component 위치를 따라가며 단발음·발소리·문소리는 재생 시 위치를 유지합니다. 종료한 소리는 엔벌로프가 감쇠하며 사라집니다. 일시정지·가상화된 반복음은 재개를 위해 등록을 유지합니다.

이미 재생 중인 환경음은 엔벌로프 업데이트를 켜서 추적하며 재생을 다시 시작하지 않습니다. 오디오 스레드에서는 PCM 버퍼만 갱신하고 게임 스레드에서 초당 최대 30회 분석·UI 갱신을 처리합니다.

게임 실행에서 제거되는 에디터 전용 `MoveAreaPreview`를 기존 `OnConstruction`이 참조하던 충돌도 null 확인으로 수정했습니다.

## 조정 방법

`Config/DefaultGame.ini`의 `[/Script/SoundCompass.SoundVisualizationSubsystem]` 설정에서 조정합니다.

| 설정 | 기본값 | 의미 |
| --- | ---: | --- |
| `DisplayMode` | ImportantOnly | 시작 표시 조건. AllAudible도 가능 |
| `MaxImportantSources` | 3 | 선별 표시 수, 1~8 |
| `ImportanceThreshold` | 0.50 | 선별 최소 점수, 0~1 |
| `SemanticWeight` | 0.70 | 의미 중요도 가중치 |
| `ProximityWeight` | 0.20 | 거리 가중치 |
| `LoudnessWeight` | 0.10 | 크기 가중치 |
| `ProximityRangeMeters` | 20 | 거리 점수가 0이 되는 거리 |
| `CategoryImportance` | 위 표 | 종류별 중요도 맵 |
| `SourceThresholdDb` | -60 | 두 조건 공통의 가청 후보 기준 |
| `MarkerHoldSeconds` | 0.35 | 종료 인스턴스의 등록 유지 시간 |
| `MaxTrackedSources` | 128 | 동시 추적 인스턴스 수 제한 |

특정 `SoundTestEmitter`의 **Sound Test > Visualization > Visualization Importance**에 0~1을 지정하면 반복음·단발음의 종류별 기본값을 대체합니다. 기본값 -1은 자동 규칙입니다. 해당 액터의 발소리는 별도 Footstep 규칙을 사용합니다. API 등록에도 `ImportanceOverride`를 지정할 수 있습니다.

위젯의 `RingRadius`·`WaveformGain`으로 원 크기와 파형 배율을 바꿀 수 있습니다. PlayerController Blueprint의 **Sound Compass**에서 네이티브 위젯의 자식 클래스를 지정해 조정하거나 자체 위젯으로 대체할 수 있습니다.

## UI 담당자용 Blueprint API

`Get World Subsystem` → `SoundVisualizationSubsystem`을 사용합니다.

| API | 용도 |
| --- | --- |
| `Get Source Frames(PlayerController)` | 모든 가청 후보. 종류·각도·거리·크기·가림·기본 중요도·최종 중요도 포함 |
| `Select Source Frames(Candidates)` | 현재 표시 조건에 따라 후보 배열 선별 |
| `Get Visualization Frames(PlayerController)` | 가청 후보 수집과 선별을 한 번에 수행 |
| `Get Signal Frame()` | 전체 출력의 RMS·Peak·dBFS·주파수·Waveform·SpectrumDb·FrequenciesHz |
| `Toggle Display Mode()` | 전체 표시/중요 소리 선별 전환 |
| `On Visualization Updated` | 초당 최대 30회 데이터 갱신 알림 |
| `Register Audio Component(Audio, Category, SourceOwner, ImportanceOverride)` | 기존 재생 컴포넌트 등록. 가능하면 Play 전에 호출 |
| `Unregister Audio Component(Audio)` | 추적 해제 |
| `Play Visualized Sound(...)` | 분석을 연결한 뒤 공간 소리를 한 번 재생 |

`SourceId`는 재생 인스턴스마다 다릅니다. 동일 액터의 단발음이 겹쳐도 각 위치를 유지합니다. 선별 배열로 아이콘을 만들고, 각도를 라디안으로 바꿔 `X=sin(angle)*radius`, `Y=-cos(angle)*radius`로 캐릭터 투영 위치 주위에 배치하면 됩니다.

## 실험 비교와 검증

**B**로 전체 표시와 선별 표시를 같은 실행에서 비교하고 **V**로 시각화 없는 상태도 볼 수 있습니다. 실제 사용자 실험에서는 음원·위치·재생 순서·볼륨을 동일하게 준비하고 조건 순서를 교차 배치해야 합니다. 현재 맵에는 랜덤 이동·재생이 있으므로 단순히 서로 다른 실행을 비교하면 통제된 실험이 되지 않습니다. 탐지 정확도·방향 판단 시간·주관적인 정보 과부하 등을 평가할 수 있지만, 이번 구현에는 참가자 실험 결과가 없습니다.

검증된 항목:

- UE **5.8.2 / Mac Development Editor** 빌드 성공.
- 표준 C++ 테스트 **47개 통과**: 정현파 RMS·dBFS·FFT, 스테레오 위상/한 채널, 여러 샘플레이트, 침묵·DC·충격음·잘못된 입력, 방향·거리, 조용한 위험 소리와 큰 환경음, 점수 경계·상위 K·동점·빈 배열.
- 기존 맵의 실제 게임 실행에서 **PASS**: 출력 PCM, 오른쪽 90°/1.500 m 방향·거리, HUD 생성, 가청 후보 6개 중 상위 3개 선별, 더 큰 저중요도 환경음 제외, 전체 조건 복원, 캐릭터 중심 투영 일치, 분석 화면 전환.
- Windows 빌드 및 참가자 실험은 검증하지 않았습니다.

```sh
clang++ -std=c++17 -O2 -Wall -Wextra -pedantic Tests/signal_analysis_test.cpp -o /tmp/soundcompass-tests
/tmp/soundcompass-tests
```

Development 실행 콘솔의 `SoundCompass.Dump`는 후보의 중요도·위치·레벨과 선별 개수를 로그에 출력합니다. `SoundCompass.SmokeTest`는 기본 설정을 대상으로 제어된 반복음 5개를 추가해 위 항목을 검사하고 `Saved/SoundCompassSmokeTest.png`를 저장합니다. 준비 2초·검사 4초·복원 3초 동안 이동/카메라 입력을 고정하고 종료 시 복원합니다. `-SoundCompassExitAfterSmokeTest` 명령줄 옵션을 쓰면 검사 후 게임이 종료됩니다. 테스트용 소리는 정지하고 표시 조건을 복원합니다.

첨부 미리보기는 실제 맵에서 검증음을 추가한 검사 화면입니다. 배포 폴더에는 엔진·생성된 빌드 캐시·Git 메타데이터가 포함되지 않습니다. 원본 출처 브랜치는 main `61eb93d`, UI `AHNINTAE` `4a73c7b`입니다.
