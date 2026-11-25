# 음원 출처

`tools/audio_build.py`가 아래 원본을 다듬어(앞뒤 무음 제거, 길이 제한, 48 kHz 변환, 음량 정규화, 루프 크로스페이드) 이 폴더에 만듭니다.

## CC0 원본

| 파일 | 원본 | 작성자 | 링크 |
| --- | --- | --- | --- |
| player_rifle_shot, rifle_shot | The Free Firearm Sound Library, AR-15 D_32P | Ben Jaszczak, Brian Nelson, Kevin Heras, Matthew Nanney | https://opengameart.org/content/the-free-firearm-sound-library |
| shotgun_shot | The Free Firearm Sound Library, Model 12 K_22P | 위와 같음 | 위와 같음 |
| sniper_shot | The Free Firearm Sound Library, Mosin Nagant M_21P | 위와 같음 | 위와 같음 |
| step_concrete_1~6 | Footsteps on concrete | florianreichelt | https://freesound.org/people/florianreichelt/sounds/459964/ |
| step_dirt_1~6 | Footsteps_Mountain_Boots_Gravel_Mono.wav | Nox_Sound | https://freesound.org/people/Nox_Sound/sounds/556002/ |
| door_open | Door Opening Creaking | dleverett | https://freesound.org/people/dleverett/sounds/710510/ |
| door_close | Old door close | kaszapatrik | https://freesound.org/people/kaszapatrik/sounds/361476/ |

모두 CC0 1.0입니다. SoundCompass 저장소의 `Content/SoundTest/CREDITS.md`와 같은 원본입니다.

## 생성 음원

rifle_reload, shotgun_pump, sniper_bolt, knife_swing, knife_hit, step_metal_1~6, land, crate_pickup, hit_taken, death, amb_urban, amb_warehouse, amb_forest, bgm_urban, bgm_warehouse, bgm_forest는 로컬 ComfyUI의 Stable Audio 3 Medium으로 생성했습니다. 프롬프트와 시드는 `tools/audio_sources.json`에 있습니다.
