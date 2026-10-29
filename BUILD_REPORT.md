# 빌드 및 검증 결과

2026-10-27 KST. 소프트웨어 구현과 GitHub Actions 검증 완료. **실물 동작은 미검증**이며 FLASHING_AND_VALIDATION.md의 현장 검증표를 사용한다.

- 성공 실행: https://github.com/mastercollection/zmk-for-voltball/actions/runs/40513302937
- 펌웨어 소스 SHA: `3e576882a0920be3de4b7cc4967679d065c81381`
- 브랜치: `codex/voltball-prospector-main` (이후 문서만 갱신한 커밋은 펌웨어 SHA와 구분).
- 아티팩트: https://github.com/mastercollection/zmk-for-voltball/actions/runs/40513302937/artifacts/12737903292
- 로컬 다운로드: `D:/Git/voltball/firmware/40513302937`
- 정상 펌웨어 3개와 각 장치 settings_reset 3개 모두 성공. 키맵 검증 job 및 다운로드 후 설정 검사 모두 PASS.
- 로컬 펌웨어 빌드는 수행하지 않았다. Python 검사는 소스 비교 및 이미 생성된 아티팩트 검사다.

## 소스 고정

| 구성 | SHA |
| --- | --- |
| ZMK main | 4dd95d7840eb3d5a3f59bb29fbf1ab054715ef85 |
| Zephyr | 10ba6d0cb38bc3d258775d27982f707599320085 |
| Prospector feat/new-status-screens | ed98221f3b52b7066dbb10ba3af8a29150b93a5a |
| PAW3222 | df652881be2520bde4f64ab6ca35e4c5708f4f9b |
| status-led | 1f827a452013199e42544d47fdf27e5ebd2d7b4b |
| input-processor-keybind | 64303bd3932bfa7f49261fc6d0c9a3fdd06f8271 |
| AA 배터리 로컬 모듈 원본 | 43fa56fb7d1e065393e926b6caa0ea02cb86eca7 |
| 자동 레이어 로컬 모듈 원본 | a68fedf50dd3b25d7aa594776c26b943fe41cfbf |
| TOTEM master 키맵 출처 | b4b2073e35f1db04ca3ac4ba82a5520a55df16de |

로컬 모듈 패치는 각 UPSTREAM.md에 기록했다. Prospector의 미수신 배터리 표시 패치는 `tests/patch_modules.py`가 정확한 SHA 및 원문을 검사한 뒤 적용한다. 전체 활성 의존성 SHA는 `verification/40513302937/west-frozen.yml`에 보존했다.

컨테이너: `zmkfirmware/zmk-build-arm@sha256:edb1c953438c6f720ddb79c3762f3972013b7fbbaf4fff3592fc869983e7afc5`. 빌드용 Actions도 SHA 고정. 로컬 reusable workflow의 기반은 위 ZMK SHA다. 그림 생성 workflow는 펌웨어 생성 경로와 별도다.

재현하려면 위 펌웨어 소스 SHA의 브랜치에서 GitHub Actions를 실행한다. 더 엄격한 의존성 재현에는 저장한 frozen manifest를 config/west.yml로 사용한다. 적용 패치와 소스 SHA도 각 아티팩트에 들어 있다.

## 메모리

| 정상 펌웨어 | FLASH | RAM |
| --- | --- | --- |
| 왼쪽 | 192524 B / 792 KiB (23.74%) | 38136 B / 256 KiB (14.55%) |
| 오른쪽 | 198972 B / 792 KiB (24.53%) | 38896 B / 256 KiB (14.84%) |
| Prospector | 564156 B / 788 KiB (69.92%) | 167526 B / 256 KiB (63.91%) |

링크 시점 사용량이며 런타임 스택/힙 최댓값이나 화면 성능 측정은 아니다.

## 다운로드 검증

| 파일 | 바이트 | SHA256 |
| --- | ---: | --- |
| voltball_left.uf2 | 385536 | `bfd018c708179111beec7ec6b7307fd8f080a57adb8c8b85f2b80c5c2a7b9899` |
| voltball_right.uf2 | 398336 | `2d71358dc6764a7b00afd7bbd6f8871599ed5b7381b32903c2ae3a118b60ff44` |
| prospector.uf2 | 1128448 | `49bf24ca933cdaa7fcba43fc335e10b1fc85a040f0703d4a34ddc789ca4d9853` |
| voltball_left_settings_reset.uf2 | 105472 | `0dd6be82134d011c8ee58794940f87e03b4eaf8e859ab336ef123bca190eef3a` |
| voltball_right_settings_reset.uf2 | 105472 | `0dd6be82134d011c8ee58794940f87e03b4eaf8e859ab336ef123bca190eef3a` |
| prospector_settings_reset.uf2 | 118272 | `c39e8639e83ec13bbe4d152a3f085306a16dc3b87a2658af16134085ef0eeaf7` |

각 UF2와 함께 `.config`, `.dts`, `-west.yml`, `-prospector.patch`, `-source-sha.txt`가 저장된다. `tests/check_artifacts.py`로 동일 소스 SHA, UF2 헤더와 개수, peripheral/central 역할, 입력 전달, 배터리 설정 분리, USB Studio, 센서 비활성화, settings_reset을 확인했다.

## 실행 이력

| 실행 ID | 결과와 조치 |
| --- | --- |
| 40510459892 | Prospector 실패: main 레이어 API 인자 변경. 고정 모듈에 호환 패치 적용 |
| 40510893928 | 6개 성공. 후속 리뷰에서 조도 센서 드라이버 활성 상태 발견 |
| 40511741042 | 센서 노드/드라이버 제거 후 6개 성공 |
| 40512196233 | CI 패치 스크립트의 컨테이너 경로 오류 수정 |
| 40512781966 | 컴파일 후 manifest 기록 실패: 비활성 babblesim 포함. --active-only로 수정 |
| 40513302937 | 최종 모든 job 성공, 아티팩트 다운로드 및 검사 PASS |

중복 또는 대체된 40510465928, 40512731047 실행은 취소했다. 원본 ZIP은 보존했고 기본 브랜치에는 병합하지 않았다.
