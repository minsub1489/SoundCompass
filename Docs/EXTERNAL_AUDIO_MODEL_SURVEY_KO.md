# 게임 출력 오디오 분석용 후보 모델 조사

작성일 2026-11-25. 목적은 게임 밖에서 시스템 출력(loopback)만 듣고 소리의 (a) 종류, (b) 방향, (c) 거리를 추정할 모델을 고르는 것입니다. 링크와 라이선스는 이 날짜에 각 저장소의 README, LICENSE 파일, Hugging Face 모델 카드를 직접 열어 확인했습니다. "미기재"는 해당 페이지에 적혀 있지 않다는 뜻입니다. 추론 속도는 아직 측정하지 않은 추정치이며, `Saved/ModelEval` 하네스로 실측해 갱신해야 합니다.

## 1. 전제와 판단 기준

- 입력은 대부분 2채널입니다. 게임의 기본 출력은 (1) 스피커용 스테레오 패닝, (2) 헤드폰용 HRTF 바이노럴, (3) Windows 스피커 설정이 5.1/7.1일 때의 다채널 중 하나입니다. FOA(Ambisonics)나 마이크 어레이 입력은 없습니다.
- 스테레오에서는 앞뒤 구분이 물리적으로 어렵습니다. 단순 패닝은 앞뒤 정보가 아예 없고, 바이노럴은 귓바퀴 스펙트럼 단서로만 앞뒤가 갈립니다. 이 단서는 게임마다 쓰는 HRTF가 달라서 학습 데이터와 어긋나기 쉽습니다. DCASE 2025 Task 3도 같은 이유로 방위를 정면 -90°~90°로 접어서 평가합니다.
- 5.1/7.1 출력은 채널 간 게인 비로 360° 방위를 바로 풀 수 있습니다. 앞뒤 혼동이 구조적으로 사라지므로, 사용자가 Windows 출력을 7.1로 둘 수 있는 게임에서는 이 경로가 가장 확실합니다.
- 실행 환경은 게임과 GPU를 공유하는 실시간 스트리밍입니다. 목표 지연은 청크당 100ms 안팎이고, 파라미터 수천만 개 이하의 CNN이 유리합니다. 이 PC의 실제 GPU는 RTX 4070 Ti SUPER 16GB이지만, 목표 사용자 환경(GTX급)을 기준으로 판단했습니다.
- 소리 종류는 `Saved/SoundTestSetup/wav`의 실제 파일을 기준으로 정했습니다(14종: Gunshot, Explosion, Footstep, Reload, Vehicle, Voice, GlassBreak, Door, Dog, Throwable, Impact, Alarm, Fire, Item). Reload와 Vehicle 원샷은 원래 없어서 Freesound CC0로 16개를 추가했습니다(`Saved/ModelEval/extra_sfx/credits_freesound.json`).

## 2. 소리 종류 분류 모델

모두 단일 채널(모노) 입력 모델입니다. 스테레오는 다운믹스하거나 채널별로 돌리면 되므로 입력 형식 문제는 없습니다. 아래 모델은 모두 AudioSet(527 클래스)으로 학습되어 Gunshot, Explosion, Footsteps, Vehicle, Speech, Glass, Door, Dog, Siren, Alarm 같은 게임 효과음 클래스를 이미 포함합니다. Reload에 정확히 대응하는 AudioSet 클래스는 없고, "Mechanisms", "Ratchet", "Clicking" 계열로 간접 매핑해야 합니다.

| 모델 | 코드 라이선스 | 가중치 위치와 라이선스 | 파라미터 | 입력 SR | 속도 추정(1초 청크, GPU) | 게임 소리 적합성 |
|---|---|---|---|---|---|---|
| [PANNs](https://github.com/qiuqiangkong/audioset_tagging_cnn) (CNN14, MobileNetV1/V2) | MIT | [Zenodo 3987831](https://zenodo.org/records/3987831), CC BY 4.0 (상용 가능, 출처 표기) | CNN14 약 80M(파일 327MB), MobileNet 계열 수 M | 32 kHz (16 kHz판 있음) | CNN14 약 5~10ms, MobileNet 약 2ms | 충분. 가중치 라이선스가 명시돼 있어 상용 판단이 가장 쉬움 |
| [EfficientAT](https://github.com/fschmid56/EfficientAT) (mn04/mn10/dymn10) | MIT | GitHub Releases, 가중치 라이선스 미기재 | mn04 0.98M, mn10 4.88M, dymn10 10.6M | 32 kHz | mn10 약 2~4ms | 높음. AST를 증류한 CNN이라 작은 크기 대비 정확도가 좋음(mn10 mAP 0.47) |
| [AST](https://github.com/YuanGongND/ast) ([HF](https://huggingface.co/MIT/ast-finetuned-audioset-10-10-0.4593)) | BSD-3-Clause | HF 카드 bsd-3-clause | 86.6M | 16 kHz | 약 15~30ms | 정확도 높지만 실시간 공유 GPU에는 무거움 |
| [BEATs](https://github.com/microsoft/unilm/tree/master/beats) | MIT | OneDrive 링크, 가중치 라이선스 미기재 | 약 90M | 16 kHz | 약 15~30ms | AST와 같은 급. 가중치가 OneDrive라 배포가 불편함 |
| [EAT](https://github.com/cwx-worst-one/EAT) ([HF](https://huggingface.co/worstchan/EAT-base_epoch30_finetune_AS2M)) | MIT | HF 카드 mit | base 88M, large 309M | 16 kHz | 약 15~30ms | AST/BEATs 대체재. HF에서 받기 쉬움 |
| [LAION CLAP](https://github.com/LAION-AI/CLAP) ([HF unfused](https://huggingface.co/laion/clap-htsat-unfused), [HF general](https://huggingface.co/laion/larger_clap_general)) | CC0-1.0 | HF 카드 apache-2.0 | 미기재(오디오 인코더 HTSAT + 텍스트 인코더, 수백 MB 추정) | 48 kHz | 오디오 인코더만 약 10ms | zero-shot으로 "gun reload" 같은 텍스트 클래스를 바로 쓸 수 있음. 게임별 클래스 추가에 유리 |
| [MS CLAP](https://github.com/microsoft/CLAP) | MIT (저장소 보관 처리됨) | [HF msclap](https://huggingface.co/microsoft/msclap), 가중치 라이선스 미기재 | 미기재 | 미기재 | 미기재 | LAION CLAP과 같은 용도. 저장소가 읽기 전용이라 유지보수가 끝남 |
| [YAMNet](https://github.com/tensorflow/models/tree/master/research/audioset/yamnet) | Apache-2.0 (tensorflow/models) | Google Storage `yamnet.h5`, 가중치 라이선스 미기재 | 3.7M | 16 kHz 모노 | 약 1~2ms (CPU로도 실시간) | 가장 가볍지만 정확도가 위 모델들보다 낮은 편(수치는 미확인). TensorFlow 의존 |

판단: 실시간 공유 GPU 조건에서는 EfficientAT mn10과 PANNs MobileNet/CNN14가 현실적인 선택지입니다. 90M급 트랜스포머(AST, BEATs, EAT)는 정확도 상한을 보는 비교용으로만 쓰고, CLAP은 게임별로 클래스를 텍스트로 늘릴 때 보조로 씁니다.

## 3. 방향과 종류를 함께 추정하는 SELD 모델

| 모델 | 라이선스 | 입력 | 출력 | 사전학습 가중치 | 스테레오·서라운드 대응 | 게임 소리 적합성 |
|---|---|---|---|---|---|---|
| [DCASE 2025 Task 3 baseline](https://github.com/partha2409/DCASE2025_seld_baseline) | MIT (README 명시, LICENSE 파일 없음) | 스테레오 2ch, log-mel, 24 kHz | 13 클래스, 방위(정면 -90°~90°로 접힘), 거리 | 있음, 저장소 `checkpoints/SELDnet_audio_multiACCDOA_*/best_model.pth` | 스테레오 전용으로 설계. 뒤쪽은 앞쪽으로 접힘 | 클래스가 실내 생활음(말소리, 박수, 문, 발소리 등)이라 그대로는 안 맞음. 구조가 작아 게임 데이터로 재학습하기 좋음. dev 결과 F20° 22.8%, DOA 24.5°, 상대 거리 오차 41% |
| [DCASE 2024 Task 3 baseline](https://github.com/partha2409/DCASE2024_seld_baseline) | MIT (README) | FOA 4ch 또는 MIC 4ch | 13 클래스, 3D 방향, 거리 | 있음(.h5) | 스테레오 불가 | 거리 추정 구조 참고용 |
| [SELDnet](https://github.com/sharathadavanne/seld-net) | TUT 라이선스: 비상용, 실험 목적만 | FOA 또는 원형 어레이 | 클래스, 3D 방향 | 미기재 | 불가 | 라이선스 때문에 제외 |
| [SELD DCASE2023 baseline](https://github.com/sharathadavanne/seld-dcase2023) | MIT (README) | FOA/MIC 4ch | 클래스, 3D 방향 | 미기재 | 불가 | 제외 |
| [EINV2](https://github.com/yinkalario/EIN-SELD) | MIT (`Licenses/MIT_LICENSE`) | FOA(TAU-NIGENS 2020) | 트랙별 클래스와 방향 | 있음, [Zenodo 4158864](https://zenodo.org/record/4158864) | 불가(입력 변경 후 재학습 필요) | 다중 음원 분리 구조는 참고할 만함 |
| [CST-former](https://github.com/yusunnny/CST-former) | MIT | FOA 4ch, 24 kHz | 클래스, 방향 | 미기재 | 불가 | 제외 |
| [NTU_SNTL_Task3](https://github.com/itsjunwei/NTU_SNTL_Task3) (DCASE2025 참가) | MIT | 스테레오(Mid-Side log-mel, MS 강도 벡터, 코히어런스) | 클래스, 방위, 거리 | 미기재 | 스테레오 | 특징 설계가 스테레오에 맞춰져 있어 재학습 레시피로 유용함. 거리 정규화 기법 포함 |
| [PSELDNets](https://github.com/Jinbo-Hu/PSELDNets) | MIT | 합성 대규모 SELD 데이터(FOA/MIC) | mACCDOA/EINV2 등 | 있음, [HF 데이터셋](https://huggingface.co/datasets/Jinbo-HU/PSELDNets) (체크포인트 122~800MB, 가중치 라이선스 미기재) | 직접은 불가, 아래 저장소가 스테레오로 이식 | 사전학습 백본으로 쓸 수 있음 |
| [DCASE2025 Stereo PSELD Mamba](https://github.com/alexandergwm/DCASE2025_TASK3_Stereo_PSELD_Mamba) | 미기재(LICENSE 없음, 사용 불가로 간주) | 스테레오 24 kHz, 7ch 유사 FOA 특징 | 13 클래스, 방위, 거리 | OneDrive | 스테레오 | 라이선스가 없어 참고만 가능 |

DCASE 2025 Task 3 결과([과제 페이지](https://dcase.community/challenge2025/task-stereo-sound-event-localization-and-detection-in-regular-video-content)): 오디오 전용 1위 Du_NERCSLIP는 F 50.4%, DOA 오차 12.2°, 상대 거리 오차 26.9%였고, 베이스라인은 F 26.1%, 23.0°, 33.2%였습니다. 스테레오에서도 정면 반구 방위는 12~25° 수준으로 풀리지만, 거리는 상위 팀도 상대 오차 27% 정도라는 뜻입니다. 데이터 생성기([SonyResearch](https://github.com/SonyResearch/dcase2025_stereo_seld_data_generator), MIT)는 FOA를 Mid-Side 스테레오로 변환하는 방식이라 HRTF 바이노럴과는 신호 특성이 다릅니다.

## 4. 바이노럴 음원 위치·거리 추정 모델

| 모델 | 라이선스 | 입력 | 출력 | 사전학습 가중치 | 비고 |
|---|---|---|---|---|---|
| [Spatial-AST / BAT](https://github.com/zszheng147/Spatial-AST) ([논문](https://arxiv.org/abs/2402.01591)) | 코드 CC BY 4.0, 가중치가 든 HF 데이터셋은 CC BY-NC 4.0 | 바이노럴 2ch, 32 kHz | AudioSet 클래스, 방위·고도(1° 단위), 거리(0~10m, 0.5m 단위) | 있음, [finetuned.pth 1.04GB](https://huggingface.co/datasets/zhisheng01/SpatialAudio/tree/main/SpatialAST) | 세 과제를 한 모델로 하는 유일한 바이노럴 공개 모델. mAP 0.50, DOA 평균 오차 18°, 거리 0.5m 이내 정답률 약 68%. SoundSpaces 2.0 시뮬레이션으로 학습해 게임 HRTF와는 다름. 비상용 가중치이고 약 90M급이라 실시간 배포용은 아님 |
| [DSpAST](https://github.com/wilkinghoff/DSpAST) | CC BY-NC 4.0 | 바이노럴(Spatial-AST와 같은 데이터) | 클래스, 방향, 거리 | 있음, [HF](https://huggingface.co/kwilk90/DSpAST) 단계별 1.05GB씩 | Spatial-AST 개선판(mAP 54.5, DOA 14.4°, 거리 오류율 28%). 비상용 |
| [BAST](https://github.com/ShengKuangCN/BAST) ([논문](https://arxiv.org/abs/2207.03927)) | MIT | 바이노럴 | 방위(각도 오차 0.9~1.3°) | 미기재, 데이터셋은 이메일 요청 | 단일 음원, 통제된 데이터 기준 수치. 클래스와 거리 없음 |
| [DeepEar](https://github.com/Qiangest/DeepEar) | CC BY-NC | 바이노럴(감마톤 특징 + 상호상관) | 8개 섹터별 존재, 방위, 거리 5구간 | 있음, `DeepEar_weights.h5` | 다중 음원과 거리 구간을 함께 내는 구조가 이 과제와 비슷함. 비상용, TF 2.5 |
| [Krause 외, SELD with Distance Estimation](https://arxiv.org/abs/2403.11827) | 논문 CC BY-NC-ND 4.0 | FOA와 바이노럴 STARSS23 | 클래스, 방향, 거리 | 코드 링크 없음 | 거리 항을 추가해도 검출·방향 성능이 떨어지지 않는다고 보고 |
| [audio-distance-estimation](https://github.com/michaelneri/audio-distance-estimation) | 코드 MIT, 데이터 CC BY-NC 4.0 | 모노(잔향 있는 음성) | 연속 거리 | 링크 미확인 | 잔향 기반 거리 추정 참고용 |
| [sound_distance_estimation](https://github.com/sakshamsingh1/sound_distance_estimation) | 라이선스 없음 | 4ch 어레이 | 거리 | 이벤트 검출기만 | 사용 불가 |

거리 추정의 근본 문제: 게임은 물리 감쇠 대신 에셋별 감쇠 곡선을 쓰고, 원래 음량을 알 수 없습니다. 쓸 수 있는 단서는 (1) 클래스별 기준 음량(게임은 같은 에셋을 반복하므로 클래스를 알면 음량이 거리 단서가 됨), (2) 공기 흡수에 의한 고역 감쇠, (3) 직접음 대 잔향 비, (4) 일부 게임의 거리 기반 저역 통과 필터입니다. 공개 사전학습 모델 중 이 조건에 맞는 것은 없으므로, 거리는 게임별 보정 또는 재학습이 전제입니다.

## 5. 비ML 기준선

| 방법 | 입력 | 출력 | 장점 | 한계 |
|---|---|---|---|---|
| GCC-PHAT로 ITD 추정 후 Woodworth 머리 모델로 방위 변환 (Knapp & Carter 1976) | 스테레오/바이노럴 | 측방각(-90°~90°) | 학습 불필요, 1ms 미만 | 앞뒤 구분 불가. 단순 패닝 게임은 ITD가 0이라 작동하지 않음 |
| ILD(고역 2 kHz 이상 레벨 차) | 스테레오/바이노럴 | 측방각 | 패닝 게임에서도 작동 | 음원 스펙트럼과 HRTF에 따라 각도 환산이 달라짐 |
| 채널 게인 역산(VBAP 역변환) | 5.1/7.1 | 360° 방위 | 앞뒤 혼동이 없음 | 게임과 Windows 출력이 다채널이어야 함. 겹친 소리 분리 불가 |

## 6. 추천

1. 종류: EfficientAT mn10_as (MIT, 4.88M, 32 kHz, 다운로드 약 20MB). 작은 크기에 AudioSet mAP 0.47이라 공유 GPU 실시간 조건에 가장 잘 맞습니다. 가중치 라이선스가 미기재라 상용 단계에서는 저자 확인이 필요하고, 막히면 가중치 라이선스가 CC BY 4.0으로 명시된 PANNs(MobileNetV2 또는 CNN14)로 바꿉니다. Reload처럼 AudioSet에 직접 대응이 없는 클래스는 임베딩 위에 작은 분류기를 학습하거나 CLAP zero-shot으로 보완합니다.
2. 방향·거리 (a): DCASE 2025 stereo SELD baseline (MIT, 저장소에 체크포인트 포함, 크기 수 MB 추정). 스테레오 입력과 거리 출력을 기본으로 갖춘 유일한 상용 가능 공개 모델입니다. 클래스가 생활음이라 그대로 쓰기보다는 방위·거리 헤드의 기준 성능을 본 뒤, 이 저장소의 합성 데이터로 재학습하는 출발점으로 씁니다. 방위가 정면으로 접혀 나오므로 앞뒤 판단은 별도 처리(바이노럴 스펙트럼 단서 학습 또는 7.1 경로)가 필요합니다.
3. 방향·거리 (b), 연구 비교용: Spatial-AST (가중치 CC BY-NC 4.0, 1.04GB). 바이노럴 입력으로 클래스, 방향, 거리를 한 번에 내는 유일한 공개 모델이라 "현재 공개 모델로 어디까지 되는가"의 상한을 재는 기준으로 씁니다. 비상용이고 무거워서 배포 후보는 아닙니다.
4. 비ML 기준선: GCC-PHAT(ITD) + ILD. 하네스 검증용 첫 어댑터(`Saved/ModelEval/adapters/gcc_phat.py`)로 구현했습니다. 합성 600장면(904이벤트)에서 청크당 4.2ms, 단일 음원 기준 정면으로 접은 방위 오차 평균 14.3°, 앞뒤 혼동률 44%였습니다(예상대로 뒤쪽 음원은 앞으로 답함). 5.1/7.1 입력일 때의 채널 게인 역산은 렌더러가 다채널을 내지 못해 아직 구현하지 않았습니다.

가중치 다운로드는 1~3번을 모두 받을 때 약 1.1GB입니다(Spatial-AST 1.04GB가 대부분, EfficientAT mn10 약 20MB, DCASE 2025 베이스라인은 저장소 안의 체크포인트). 여기에 CUDA판 PyTorch 설치가 약 2.5~3GB 추가됩니다. 둘 다 5GB 기준 이하입니다.

## 7. 실측 결과 (합성 데이터, 2026-11-25)

수업 과제 용도라 라이선스 제약을 빼고, 공개 가중치를 받을 수 있는 14개 어댑터를 모두 같은 데이터로 돌렸습니다. 전체 표는 `Saved/ModelEval/results/COMPARISON.md`에 있습니다. BEATs는 가중치가 OneDrive에만 있어서 빠졌고, BAST는 공개 가중치가 없어서 빠졌습니다.

### 테스트 기준

- 데이터: Steam Audio HRTF로 렌더한 바이노럴 600장면(이벤트 904개, 14클래스). 축은 condition(single, bg, multi), acoustics(dry, atten, room, occluded), SNR(inf, 20, 10, 5, 0, -5 dB), 배경 종류, 방위 영역(front, side, back), 거리 구간(near, mid, far)입니다. 시드는 1234로 고정했습니다.
- 입력: 장면마다 첫 onset 50ms 전부터 1초 청크 하나를 `predict(audio, sr)`에 넣습니다. AudioSet 계열 모델은 학습 때처럼 10초로 0 패딩합니다.
- 장비: 노트북 i7-10710U(6코어 12스레드) CPU와 GTX 1650 Max-Q 4GB GPU입니다. GPU는 600장면 전체, CPU는 앞 200장면(속도 측정과 예측 일치 확인용)을 돌렸습니다.

### 평가 항목

| 항목 | 정의 |
|---|---|
| class_acc, class_macro_f1 | 정답 이벤트 기준 클래스 정확도와 클래스별 F1 평균. 놓친 이벤트는 오답 |
| event_recall, pred_precision | 예측과 매칭된 정답 비율, 정답과 매칭된 예측 비율 |
| az_mae, az_median, az_within15/30 | 방위 절대 오차(°)와 15°/30° 이내 비율 |
| az_folded_mae | 앞뒤를 접은 뒤의 방위 오차. 스테레오 단서로 풀 수 있는 부분 |
| front_back_rate | 예측이 정답보다 앞뒤 거울상에 더 가까운 비율(±90° 근처 15° 이내 제외) |
| dist_bin_acc, dist_mae_m | 거리 구간(near <5m, mid 5~15m, far ≥15m) 정확도와 절대 오차(m) |
| ms/청크, p95, VRAM | 1초 청크 한 번의 추론 시간(리샘플과 특징 추출 포함, 워밍업 3회 제외)과 GPU 최대 메모리 |

### 결과 요약

| 모델 | single 클래스 정확도 | 전체 클래스 정확도 | 접은 방위 오차 | 앞뒤 혼동 | GPU ms | CPU ms | VRAM MB |
|---|---|---|---|---|---|---|---|
| CLAP larger_general | 0.61 | 0.32 | | | 87 | 312 | 786 |
| DSpAST stage3 | 0.48 | 0.31 | 32.1° | 0.42 | 394 | 926 | 415 |
| Spatial-AST | 0.48 | 0.27 | 42.8° | 0.49 | 135 | 317 | 396 |
| CLAP htsat-unfused | 0.45 | 0.29 | | | 72 | 218 | 629 |
| EfficientAT mn40_as_ext | 0.45 | 0.27 | | | 51 | 146 | 405 |
| EAT-base | 0.40 | 0.26 | | | 126 | 277 | 389 |
| AST | 0.40 | 0.24 | | | 272 | 751 | 384 |
| EfficientAT mn10_as | 0.38 | 0.24 | | | 10.6 | 25 | 41 |
| PANNs MobileNetV1 | 0.38 | 0.24 | | | 8.0 | 27 | 43 |
| YAMNet | 0.34 | 0.22 | | | 50 | 57 | 65 |
| PANNs CNN14 | 0.33 | 0.22 | | | 46 | 184 | 723 |
| EfficientAT mn04_as | 0.31 | 0.20 | | | 10.5 | 15 | 26 |
| DCASE2025 stereo SELD | 0.21 (생활음 클래스만 대응) | 0.13 | 36.2° | 0.45 | 2.5 | 4.2 | 45 |
| GCC-PHAT + ILD (비ML) | | | 20.1° | 0.37 | (CPU) | 6.3 | |

- 분류: 사전학습 그대로는 단일 음원에서도 31~61%입니다. 텍스트로 클래스를 정하는 CLAP가 가장 높고, Reload, Throwable, Fire처럼 AudioSet에 직접 대응이 없는 클래스에서 차이가 큽니다. AudioSet 모델은 모두 Voice를 79~98%로 잘 맞히지만, Footstep은 0%이고 Gunshot과 Reload는 1~20%입니다. 1초 안의 발소리 한 번이나 장전 클릭은 AudioSet 라벨로는 Knock, Tap, Clock 등으로 흩어지기 때문입니다.
- 방향: 앞뒤를 접은 방위 오차는 비ML GCC-PHAT(20.1°, single 12.2°)가 학습된 공간 모델 3개(32~43°)보다 작습니다. 학습 모델들이 실제 실내 녹음(DCASE)이나 SoundSpaces HRTF(Spatial-AST)에 맞춰져 있어 Steam Audio HRTF와 어긋나는 것으로 보입니다. 앞뒤 혼동률은 모두 37~49%라 스테레오만으로는 앞뒤를 사실상 가리지 못합니다. 벽 차폐 조건에서는 모든 방법의 오차가 36~50°로 커집니다.
- 거리: 세 공간 모델 모두 예측이 1.5~3m 근처에 몰려 거의 항상 near로 답합니다. 구간 정확도 0.34는 near 정답 비율과 같은 우연 수준이고, 실제 거리와의 상관도 0에 가깝습니다. 공개 모델로는 거리를 전혀 추정하지 못한다는 결과입니다.
- CPU와 GPU: 작은 모델은 CPU가 1.4~3.4배 느리지만 여전히 30ms 이하라 CPU만으로도 실시간 목표(100ms)를 맞춥니다(EfficientAT mn04 15ms, mn10 25ms, MobileNetV1 27ms, DCASE 4ms, GCC-PHAT 6ms). 90M급 트랜스포머(AST, EAT, Spatial-AST, DSpAST)는 GPU에서도 126~394ms라 100ms 목표를 넘습니다. CPU와 GPU의 예측은 98~100% 같았고, DSpAST만 90%였습니다(방위 argmax가 경계에서 앞뒤로 뒤집힌 경우가 일부 있었고 정확도 차이는 3%p 이내).

### multi-ACCDOA와 EINV2 계열 (추가 실측)

DCASE2025 스테레오 베이스라인 외에, FOA로 학습된 다중 트랙 SELD 모델 7개를 더 돌렸습니다. [PSELDNets](https://github.com/Jinbo-Hu/PSELDNets)의 대규모 합성 데이터 사전학습 체크포인트 6개(170클래스: mACCDOA-HTSAT, mACCDOA-CNN14-Conformer, mACCDOA-PaSST, EINV2-HTSAT, EINV2-HTSAT-AGG1, 비교용 단일 트랙 ACCDOA-HTSAT)와 원조 [EIN-SELD](https://github.com/yinkalario/EIN-SELD) EINV2(DCASE2020, 14클래스)입니다.

- 입력: 이 모델들은 FOA 4채널을 받습니다. 스테레오에서 의사 FOA(W=(L+R)/2, Y=(L−R)/2, Z=0, X는 0 또는 W)를 만들어 넣었습니다. X를 어떻게 둘지는 테스트셋 앞 60장면 결과로 모델마다 골랐습니다(HTSAT 계열은 X=W, CNN14·PaSST·EIN-SELD는 X=0). 테스트셋으로 고른 전처리라 약간 낙관적인 수치입니다.
- EIN-SELD 근사: 공개 파일에 학습 데이터 특징 정규화 통계가 없어서, 평가 청크로 평균과 표준편차를 계산해 썼습니다. 원래 조건과 다르므로 참고용입니다.

| 모델 | 접은 방위 오차 | single 접은 오차 | 차폐 접은 오차 | 앞뒤 혼동 | multi 재현율 | 장면당 예측 수 | GPU ms | CPU ms | VRAM MB |
|---|---|---|---|---|---|---|---|---|---|
| GCC-PHAT (비ML, 참고) | 20.1° | 12.2° | 36.1° | 0.37 | 0.40 | 1.00 | 6 (CPU) | 6 | |
| PSELDNets EINV2-HTSAT | 34.0° | 33.7° | 37.7° | 0.39 | 0.40 | 1.00 | 40 | 172 | 290 |
| PSELDNets ACCDOA-HTSAT | 35.9° | 35.0° | 37.1° | 0.38 | 0.40 | 1.00 | 22 | 93 | 181 |
| PSELDNets mACCDOA-HTSAT | 36.1° | 32.6° | 39.2° | 0.37 | 0.43 | 1.07 | 23 | 90 | 199 |
| DCASE2025 stereo SELD (mACCDOA) | 36.2° | 26.6° | 38.8° | 0.45 | 0.68 | 1.94 | 2.5 | 4.2 | 45 |
| PSELDNets mACCDOA-PaSST | 39.1° | 37.0° | 39.0° | 0.44 | 0.40 | 1.00 | 93 | 296 | 264 |
| PSELDNets mACCDOA-CNN14-Conformer | 39.1° | 37.2° | 50.2° | 0.42 | 0.40 | 1.00 | 64 | 401 | 827 |
| PSELDNets EINV2-HTSAT-AGG1 | 40.0° | 41.5° | 39.9° | 0.39 | 0.40 | 1.00 | 40 | 170 | 290 |
| EIN-SELD EINV2 (근사) | 46.1° | 39.1° | 50.3° | 0.47 | 0.42 | 1.03 | 11 | 38 | 143 |

- 방향: 7개 모두 접은 방위 오차가 34~46°로, DCASE2025 스테레오 베이스라인(36°)과 비슷하거나 나쁘고 GCC-PHAT(20°)보다 나쁩니다. 의사 FOA에는 앞뒤(X) 정보가 없어서, 모델 출력이 좌우 ±90° 근처로 몰리는 경향이 있었습니다.
- 다중 음원: 이 계열의 장점이 다중 트랙인데, 의사 FOA 입력에서는 활동도가 0.5를 넘는 트랙이 거의 나오지 않았습니다(장면당 예측 1.00~1.07개). 그래서 2~3개가 겹친 장면의 재현율이 단일 출력 모델과 같은 0.40~0.43입니다. 스테레오로 직접 학습한 DCASE2025 베이스라인만 장면당 1.94개를 내고 재현율 0.68을 얻었습니다.
- 종류: 170클래스 중 우리 14클래스에 대응하는 라벨이 Gunshot·GlassBreak 각 1개 등으로 적어서, 클래스 정확도는 4~17%입니다(EINV2-HTSAT가 17%로 가장 높음).
- 속도: HTSAT 계열은 GPU 22~40ms로 실시간 목표 안이지만, CPU에서는 90~172ms라 목표를 넘습니다. EIN-SELD는 GPU 11ms, CPU 38ms로 가볍습니다. CPU와 GPU 예측은 7개 모두 100% 같았습니다.
- 결론: FOA로 학습된 multi-ACCDOA·EINV2 모델을 스테레오 게임 출력에 그대로 쓰는 것은 얻는 게 없습니다. 다중 트랙 구조를 쓰려면 DCASE2025 베이스라인처럼 스테레오(또는 게임 HRTF로 만든 합성 데이터)로 다시 학습해야 합니다.

### 게임 소리로 학습한 분류 헤드

공개 데이터에서 14클래스 학습 클립 7,366개를 모았습니다(FSD50K, ESC-50, UrbanSound8K, Epidemic·Sonniss 효과음, Freesound CC0, HF 총성 데이터). 평가용 클립과 같은 Freesound 원본은 뺐고, train/val은 원본 녹음 단위로 나눴습니다. 키워드 검색으로 고른 클립 3,142개는 CLAP으로 검사해, 자기 클래스를 14개 중 7위 아래로 본 428개를 뺐습니다. 이 클립으로 학습 장면 6,000개와 검증 장면 1,200개를 테스트와 같은 방식으로 렌더했습니다. 배경음은 ESC-50의 비표적 소리를 써서 테스트 배경과 겹치지 않습니다. 그다음 고정 임베딩 위에 작은 MLP 헤드(256 은닉, 다중 라벨 BCE)를 학습했습니다.

| 모델 | single | bg | multi | 전체 | macro F1 | 다중 재현율 | GPU ms | CPU ms |
|---|---|---|---|---|---|---|---|---|
| EfficientAT mn10 zero-shot | 0.375 | 0.270 | 0.177 | 0.241 | 0.256 | 0.664 | 10.6 (GTX 1650) | 25 |
| EfficientAT mn10 + 학습 헤드 | 0.635 | 0.505 | 0.337 | 0.440 | 0.466 | 0.835 | 7.9 (RTX 4070 Ti) | 36 |
| CLAP zero-shot | 0.610 | 0.345 | 0.188 | 0.316 | 0.369 | 0.664 | 87 (GTX 1650) | 312 |
| CLAP + 학습 헤드 | 0.705 | 0.485 | 0.373 | 0.471 | 0.482 | 0.854 | 29 (RTX 4070 Ti) | 344 |

- 헤드는 거의 모든 조건에서 정확도를 크게 올렸습니다. 특히 EfficientAT mn10은 단일 음원에서 37.5%에서 63.5%가 됐고, Reload(4→55%), Gunshot(4→29%), Alarm(56→88%) 개선이 큽니다. 다중 라벨 출력이라 겹친 소리의 재현율도 0.66에서 0.84~0.85로 올랐습니다(정밀도 0.82).
- 약한 클래스는 Footstep(19~36%), Impact(22~28%), Door(33~40%), Throwable·Fire입니다. CLAP 헤드는 Fire(41→4%)와 Throwable(42→10%)에서 zero-shot보다 크게 떨어졌습니다. 두 클래스는 학습 데이터 대부분이 키워드 검색으로 모은 것이라 라벨 노이즈 영향으로 보입니다(추정).
- 속도 측정 장비가 다릅니다. 헤드는 이 PC(RTX 4070 Ti, 24스레드 CPU)에서, zero-shot은 노트북(GTX 1650, i7-10710U)에서 쟀습니다. 헤드 자체는 MLP 하나라 기반 모델 속도와 거의 같습니다.

### 실제 게임 오디오 검증 (PUBG)

[PUBG Gun Sound Dataset (BGG)](https://github.com/junwoopark92/PUBG-Gun-Sound-Dataset)는 PUBG 게임 안에서 녹음한 총성 2,132개입니다(무음 63개 제외). 플레이어 자기 총 1,013개와, 앞/뒤/왼/오 4방향에 50~600m 거리의 적 총 1,119개입니다. 스테레오 44.1kHz이고, `make_pubg_labels.py`로 같은 평가 형식으로 바꿨습니다.

| 모델 | 총성 인식률 (자기 총 / 적 총) | 적 총 접은 방위 오차 | 거리 예측 |
|---|---|---|---|
| EfficientAT mn10 zero-shot | 0.37 (0.55 / 0.21) | | |
| EfficientAT mn10 + 학습 헤드 | 0.78 (0.66 / 0.88) | | |
| CLAP zero-shot | 0.87 (0.84 / 0.89) | | |
| CLAP + 학습 헤드 | 0.85 (0.82 / 0.88) | | |
| GCC-PHAT | | 40.0° | |
| DCASE2025 stereo SELD | | 53.8° | 0~600m 모두 1~2m |

- 종류: 학습 헤드가 실제 게임 총성에서도 통했습니다(EfficientAT 37→78%). 다만 이 세트는 총성만 있어서 재현율만 재고, 다른 소리를 총성으로 잘못 부르는 비율은 재지 못합니다.
- 방향: PUBG 녹음은 모든 방향에서 ITD가 0이고 좌우 레벨 차이도 0~3dB에 그칩니다. 그래서 GCC-PHAT는 4방향 모두 0°로 답했고, DCASE 베이스라인도 −4~−32°로 몰렸습니다. 이 게임의 스테레오 출력에는 양귀 방향 단서가 거의 없다는 뜻입니다. 원 논문의 학습 모델이 방향을 맞힌 것은 스펙트럼이나 잔향 단서를 이 게임에 맞춰 학습했기 때문으로 보입니다(추정). 합성 데이터의 GCC-PHAT 점수(20°)가 실제 게임으로 옮겨지지 않는다는 점을 보여 주는 결과입니다.
- 거리: DCASE 베이스라인은 0~600m 모두 1~2m로 답했습니다.

### 테스트 환경 수정 후 재평가 (v2, 2026-11-26)

위 결과(v1)를 점검하다 테스트 환경의 문제를 찾아 고쳤습니다. 아래 v2 결과가 v1보다 우선합니다. 전체 표는 `Saved/ModelEval/results_v2/COMPARISON_v2.md`입니다.

| 문제 (v1) | 수정 (v2) |
|---|---|
| 테스트 이벤트 904개가 원본 녹음 92개(Door·Fire·Voice는 2개)에서 나와 클래스별 수치가 녹음 몇 개에 좌우됨 | 사람이 라벨을 단 FSD50K·ESC-50·UrbanSound8K에서 녹음 단위로 테스트 분할을 따로 뗌(1,290클립, 클래스당 녹음 54~125개, 학습과 겹침 0). 장면 840개. 기존 92원본 세트는 "asset92"로 따로 보고 |
| 클래스 없는 예측이 여러 정답 중 가장 가까운 방향에 매칭돼 GCC-PHAT가 유리했음(다중 장면 20.4° → 실제 41.3°) | 클래스 없는 예측은 청크를 자른 기준 소리(첫 이벤트)에 매칭. 방향 주 지표는 단일 음원 장면 |
| 렌더가 Steam Audio 기본 HRTF 하나뿐이라 ITD 기반 방법에 유리 | 같은 840장면을 SADIE H12 HRTF(사람 측정)와 스테레오 음량 패닝(ITD 없음, PUBG와 같은 성질)으로도 렌더 |
| 거리 지표에 거리 단서가 없는 dry 장면이 섞임 | dry를 빼고 계산, 순위 상관(Spearman) 추가 |
| AudioSet 모델 전부에 10초 0 패딩을 일괄 적용 | 검증 장면으로 모델별 선택(PANNs CNN14는 1초 그대로, CLAP은 반복 패딩 유지) |
| 학습 장면이 바뀐 분할과 안 맞음 | 학습·검증 장면 재렌더 후 헤드 재학습 |

종류 (단일 음원 정확도, 높을수록 좋음)

| 모델 | 기본 HRTF | SADIE | 패닝 | asset92 | PUBG 총성 |
|---|---|---|---|---|---|
| CLAP + 학습 헤드 | 0.661 | 0.636 | 0.643 | 0.765 | 0.865 |
| EfficientAT mn10 + 학습 헤드 | 0.600 | 0.604 | 0.611 | 0.605 | 0.740 |
| CLAP zero-shot | 0.443 | 0.421 | 0.432 | 0.610 | 0.865 |
| EfficientAT mn40 / EAT zero-shot | 0.404 | 0.393 | 0.379~0.404 | 0.40~0.45 | 0.54~0.72 |
| EfficientAT mn10 zero-shot | 0.332 | 0.336 | 0.332 | 0.375 | 0.369 |
| YAMNet | 0.325 | 0.289 | 0.286 | 0.340 | 0.307 |

방향 (단일 음원, 앞뒤 접은 방위 오차, 낮을수록 좋음)

| 모델 | 기본 HRTF | SADIE | 패닝 | 다중 장면 | PUBG 적 총 | GPU ms (GTX 1650) |
|---|---|---|---|---|---|---|
| GCC-PHAT (비ML) | 11.2° | 24.7° | 44.7° | 44.5° | 40.0° | 6 (CPU) |
| DSpAST | 21.5° | 27.6° | 30.4° | 38.3° | 51.8° | 394 |
| DCASE2025 stereo SELD | 26.7° | 25.4° | 23.2° | 43.6° (재현율 0.68) | 53.8° | 2.5 |
| PSELDNets EINV2-HTSAT | 33.8° | 28.6° | 15.5° | 36.9° | 51.0° | 40 |
| PSELDNets mACCDOA-HTSAT | 32.4° | 29.1° | 19.4° | 39.4° | 49.6° | 23 |

- 종류: CLAP + 학습 헤드가 모든 세트에서 1위입니다. 렌더 방식이 바뀌어도 0.64~0.66으로 거의 흔들리지 않습니다. EfficientAT mn10 + 헤드가 0.60으로 뒤를 잇습니다. 둘 다 zero-shot보다 20%p 이상 높습니다.
- 방향: 렌더 방식에 따라 1위가 바뀝니다. GCC-PHAT는 기본 HRTF에서 11°로 가장 좋지만, HRTF가 바뀌면 25°, 패닝에서는 45°로 무너집니다. 패닝에서는 의사 FOA를 쓰는 PSELDNets HTSAT 계열이 15~19°로 가장 좋습니다. 세 렌더 모두에서 고르게 버틴 것은 DCASE2025 스테레오 베이스라인(23~27°)뿐이고, 2.5ms로 가장 빠르며 다중 장면 재현율(0.68)도 유일하게 높습니다.
- 앞뒤 혼동은 단일 음원에서 모든 모델이 0.44~0.53으로 우연 수준이고, 실제 PUBG 녹음에서는 모든 모델이 40~72°로 실패했습니다.
- 거리: dry를 빼도 순위 상관이 −0.14~+0.11로, 어떤 모델도 거리를 추정하지 못합니다.
- 결과적으로 v1의 "방향은 GCC-PHAT가 가장 좋다"는 결론은 기본 HRTF 하나와 유리한 매칭에서 나온 것이었습니다. 게임마다 출력 방식이 다르다는 전제에서는 DCASE2025 스테레오 베이스라인이 가장 안전한 선택입니다.

### 클래스 재정의: Throwable·Impact를 소리 기준으로 분할 (v3, 17클래스)

Throwable은 Whoosh(던지는 휙 소리), MetalHit(깡통 튕김·핀 클릭), Hiss(연막 쉭 소리)로, Impact는 Thud(쿵·발 구르기·망치질·공 튕김)와 Scrape(의자·가구·탁자 끌기)로 나눴습니다. 기존 클립은 검색어, FSD50K 라벨, 제목 키워드로 다시 배정했고(애매한 19개 제외), 새 클래스용 Freesound 클립을 더 받았습니다. CLAP 검사도 17클래스로 다시 돌렸고, 학습·검증·테스트 장면(test2 1,020장면 × 3렌더)을 다시 렌더한 뒤 헤드를 재학습하고 전 모델을 다시 평가했습니다. 14클래스 v2 결과는 `results_v2_14cls/`에 보관했습니다.

| 모델 | 14클래스 단일 정확도 | 17클래스 단일 정확도 | 17→14 되묶음 | 바뀌지 않은 12클래스 (14→17) | 옛 Throwable/Impact 그룹 맞힘 (14→17) |
|---|---|---|---|---|---|
| CLAP + 학습 헤드 | 0.661 | 0.621 | 0.632 | 0.692 → 0.667 | 0.47 → 0.55 |
| EfficientAT mn10 + 학습 헤드 | 0.600 | 0.526 | 0.538 | 0.613 → 0.567 | 0.53 → 0.47 |
| CLAP zero-shot | 0.443 | 0.400 | 0.421 | 0.471 → 0.346 | 0.28 → 0.60 |
| EfficientAT mn10 zero-shot | 0.332 | 0.306 | 0.312 | 0.342 → 0.300 | 0.28 → 0.34 |

- 새 클래스별(기본 HRTF, CLAP 헤드): Hiss 0.62, Whoosh 0.38, MetalHit 0.32, Scrape 0.23, Thud 0.10입니다. Hiss는 잘 구분되지만 Thud와 Scrape는 거의 맞히지 못합니다.
- 쪼갠 그룹 자체는 CLAP 계열에서 나아졌습니다(헤드 0.47→0.55, zero-shot 0.28→0.60). 반면 바뀌지 않은 12클래스는 모든 모델에서 떨어졌습니다. Thud가 Knock·Tap·Wood 같은 넓은 라벨을 흡수해서, 발소리나 문소리까지 Thud로 끌려가는 것으로 보입니다(추정).
- 전체적으로는 분할 후 정확도가 내려갔습니다. 순위(CLAP 헤드 > EfficientAT 헤드 > CLAP zero-shot)는 그대로입니다.
- 한계: Scrape와 MetalHit는 FSD50K에 해당 라벨이 없어서, 테스트 클립 일부(각 25개, 17개)를 CLAP이 3위 안으로 확인한 키워드 클립으로 채웠습니다. 그래서 이 두 클래스에서는 CLAP 계열이 유리할 수 있습니다. 두 테스트 장면 세트는 클래스 구성이 달라서(쪼갠 그룹의 비중이 14%→29%), 전체 정확도의 차이에는 문제 난이도 변화도 섞여 있습니다.

### Impact 분할 되돌림 (v4, 16클래스, 현재 기준)

v3 결과에 따라 Thud·Scrape를 다시 Impact로 합치고, Throwable 분할(Whoosh/MetalHit/Hiss)만 남겼습니다. 클래스는 Alarm, Dog, Door, Explosion, Fire, Footstep, GlassBreak, Gunshot, Hiss, Impact, Item, MetalHit, Reload, Vehicle, Voice, Whoosh의 16개입니다. 같은 절차(CLAP 검사, 재렌더, test2 960장면 × 3렌더, 헤드 재학습, 전 모델 재평가)를 다시 거쳤습니다. 현재 `results_v2/`가 16클래스 결과이고, 17클래스는 `results_v3_17cls/`, 14클래스는 `results_v2_14cls/`에 있습니다.

| 모델 (단일 음원, 기본 HRTF) | 14클래스 | 17클래스 | 16클래스 | 바뀌지 않은 12클래스 (14 / 17 / 16) | 16클래스 SADIE / 패닝 / PUBG 총성 |
|---|---|---|---|---|---|
| CLAP + 학습 헤드 | 0.661 | 0.621 | 0.650 | 0.692 / 0.667 / 0.700 | 0.628 / 0.616 / 0.846 |
| EfficientAT mn10 + 학습 헤드 | 0.600 | 0.526 | 0.569 | 0.613 / 0.567 / 0.604 | 0.550 / 0.553 / 0.841 |
| CLAP zero-shot | 0.443 | 0.400 | 0.416 | 0.471 / 0.346 / 0.375 | 0.416 / 0.416 / 0.835 |

- Impact를 되돌리자 바뀌지 않은 12클래스가 14클래스 수준으로 돌아왔습니다(CLAP 헤드 0.700, EfficientAT 헤드 0.604). Thud가 다른 소리를 끌어당기던 문제가 사라졌습니다.
- Throwable 그룹: 헤드는 3개 세부 클래스를 정확히 맞힌 비율이 0.58(CLAP), 0.55(EfficientAT)입니다. 14클래스에서 Throwable 하나를 맞힌 비율(0.65, 0.60)과 비슷해서, 더 잘게 나눠도 손해가 크지 않습니다. CLAP zero-shot은 0.50에서 0.72로 올랐습니다. 다만 14클래스 테스트의 Throwable 단일 장면은 20개뿐이라 이 비교는 흔들림이 큽니다.
- 새 클래스별(CLAP 헤드): Hiss 0.62, Whoosh 0.39, MetalHit 0.29입니다. Impact는 0.15로 여전히 가장 약합니다.
- 전체 단일 음원 정확도는 클래스가 2개 늘어난 만큼 14클래스보다 조금 낮지만(0.661→0.650), 17클래스보다는 확실히 낫습니다.
- MetalHit는 학습 64개, 테스트 14개로 데이터가 가장 적고, 테스트 일부가 CLAP으로 확인한 키워드 클립입니다.

### 결과에 따른 추천 조정

1. 방향은 DCASE2025 스테레오 SELD 베이스라인을 기본으로 씁니다(v2 단일 음원 23~27°, 렌더 방식에 가장 덜 흔들림, GPU 2.5ms, CPU 4ms, 다중 음원 재현율 0.68). HRTF 바이노럴 게임만 대상이면 GCC-PHAT가 더 정확하지만(11°) HRTF가 바뀌거나 패닝 게임이면 무너집니다. 실제 PUBG 녹음에서는 모든 방법이 실패했으므로, 7.1 출력 경로를 우선 검토하고 스테레오만 가능하면 해당 게임 녹음으로 학습해야 합니다.
2. 종류는 GPU를 쓸 수 있으면 CLAP + 학습 헤드(v2 단일 음원 0.66, PUBG 총성 0.87, GPU 29ms, CPU 344ms), CPU만 쓰면 EfficientAT mn10 + 학습 헤드(0.60, PUBG 0.74, GPU 8ms, CPU 36ms)입니다. 약한 클래스(Footstep, Impact, Door, Fire, Throwable)는 라벨이 깨끗한 데이터를 더 모아야 합니다. 현재 16클래스(v4) 기준으로는 CLAP 헤드 0.650(PUBG 총성 0.846), EfficientAT 헤드 0.569(PUBG 0.841)입니다.
4. 클래스 구성은 16클래스(Throwable만 Whoosh/MetalHit/Hiss로 분할)를 씁니다. Impact 분할(Thud/Scrape)은 다른 클래스를 끌어당겨서 되돌렸습니다. Impact(0.15)와 MetalHit(0.29)는 데이터를 더 모아야 할 다음 대상입니다.
3. 거리는 공개 모델로 불가능합니다. 클래스별 기준 음량, 고역 감쇠, 잔향 비를 특징으로 쓰는 별도 회귀기를 게임 데이터로 학습해야 합니다.

## 8. 한계와 남은 확인

- 위 성능 수치는 각 논문과 README의 자체 데이터 결과입니다. 게임 오디오에서의 성능은 `Saved/ModelEval` 합성 데이터와 실제 게임 녹음으로 따로 재야 합니다.
- 2~5절 표의 속도는 조사 시점의 추정치이고, 실측값은 7절에 있습니다.
- EfficientAT, BEATs, YAMNet, MS CLAP, PSELDNets 가중치는 코드 라이선스와 별도로 가중치 라이선스가 적혀 있지 않습니다. 또 AudioSet으로 학습한 가중치는 원본 YouTube 데이터의 권리 문제가 남아 있어, 상용 배포 전에는 법무 확인이 필요합니다.
