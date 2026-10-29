# Voltball + Prospector

Voltball 양쪽(nice!nano v2)은 BLE peripheral, Prospector(XIAO nRF52840)는 central이며 PC에는 Prospector USB로 연결한다. 오른쪽 PAW3222 센서 입력을 BLE로 전달하고 central에서 자동 MOUSE, 방향키, 스크롤을 처리한다.

[검증된 Actions 실행](https://github.com/mastercollection/zmk-for-voltball/actions/runs/40513302937)에서 정상 펌웨어 3개와 settings_reset 3개가 성공했다. 실물 동작 검증은 별도다.

2026-10-29 자동 MOUSE 유지 방식 변경은 로컬 소스와 호스트 회귀 검사에 반영했다. 위 기존 빌드·배포에는 포함되지 않으며 새 펌웨어 빌드와 실물 검증이 필요하다.

- [빌드 결과, 소스/모듈 SHA, UF2 체크섬](BUILD_REPORT.md)
- [백업·플래싱·왼쪽 우선 페어링·Studio 복원·복구·실물 검증](FLASHING_AND_VALIDATION.md)
- [카운터 리뷰와 수정 근거](COUNTER_REVIEW.md)

## 다운로드

Actions의 `firmware.zip`에는 설치용 UF2 6개만 들어 있다. `build-evidence.zip`은 설정·devicetree·모듈 버전 등의 검증 자료이며 장치에 복사하지 않는다.

- Prospector: `prospector.uf2`
- 왼쪽 Voltball: `voltball_left.uf2`
- 오른쪽 Voltball: `voltball_right.uf2`
- 각 `*_settings_reset.uf2`: 초기화가 필요할 때 먼저 넣은 뒤 정상 펌웨어를 다시 넣는다.

공개 배포 ZIP은 [Releases](https://github.com/mastercollection/zmk-for-voltball/releases)에서 받는다. GitHub가 자동 제공하는 Source code ZIP은 설치용 펌웨어가 아니다.

## 구성

- 검증한 ZMK main SHA 고정. v0.3 미사용.
- TOTEM master의 실제 QWERTY 바인딩과 6개 레이어, hold-tap, modifier, 9개 combo를 승인된 42키 배열로 이식. 원본에는 사용자 macro 정의가 없다.
- 레이어 순서: BASE 0 / SYM 1 / NAVI 2 / FUNC 3 / NUM 4 / MOUSE 5 / SCROLL 6.
- B 오른쪽: MOUSE 순간 활성화. N 왼쪽: SCROLL 순간 활성화.
- 맨 아래 왼쪽에서 두 번째(index 33): Studio unlock. 그 오른쪽(index 34): 미할당.
- PAW3222: 오른쪽 SPI/IRQ 유지, 950 CPI. 자동 MOUSE 임계값 100, 시간 제한 없이 유지. 왼손 수정키 이외 첫 입력은 버리고 해제한다. 해당 키를 뗀 뒤 200ms 동안 잔여 움직임을 무시한다. NAVI에서 방향키, SCROLL에서 반전 Y·1/10 배율.
- Prospector: 240×280 LCD, 고정 밝기 50%, 조도 센서 및 해당 I2C 비활성화. 터치 UI 없음.
- 화면: 레이어 이름, 페어링 순서별 양쪽 연결·실제 보고 배터리 값. 첫 보고 전 N/A.
- AA/저전압/5분 광고 절전/GPIO 설정은 Voltball 양쪽에만 적용.
- USB Studio는 Prospector에서만 활성화하며 잠금 사용. 키 바인딩 편집·저장은 지원하지만 물리 좌표 변경은 소스 수정 및 재빌드가 필요하다.

![키맵](keymap-drawer/voltball.svg)

## 파일

- `config/voltball.keymap`: 키맵 및 hold-tap/combos.
- `config/boards/shields/voltball/voltball-layout.dtsi`: 42키 물리 좌표 및 transform.
- 같은 디렉터리의 left/right/dongle `.conf`, `.overlay`: 장치 역할과 하드웨어 설정.
- `modules/`: 고정 원본과 호환 패치를 포함한 배터리/자동 레이어 모듈.
- `config/west.yml`: 외부 모듈 SHA. 전체 활성 의존성은 `verification/40513302937/west-frozen.yml`.
- `build.yaml`, `.github/workflows/build*.yml`: GitHub Actions 전용 6개 빌드 및 검증 자료 저장.

펌웨어 빌드는 GitHub Actions로 수행한다. `python tests/check_mapping.py`는 소스 이식 검사이며 펌웨어를 빌드하지 않는다.

## 자동 MOUSE 유지 규칙

- 볼을 멈춰도 자동 MOUSE는 유지된다. 오른손 키 입력은 해제하지 않는다.
- 왼손 수정키 이외 첫 입력은 **동작하지 않고 자동 MOUSE만 해제**한다. 다음 입력부터 정상 동작한다. 문자뿐 아니라 클릭·스크롤·레이어·Studio unlock·미할당 키도 해제용이다. 왼손 키가 포함된 콤보도 같은 규칙을 따른다.
- Ctrl·Shift·Alt·GUI와 sticky modifier는 정상 동작하며 자동 MOUSE를 유지한다. F 탭은 문자를 보내지 않고 해제하고, F 홀드의 Shift는 정상 동작한다. MOUSE에서 A/S/D는 수정키가 아닌 스크롤 바인딩이므로 해제용이다.
- 버린 입력은 키를 뗄 때도 보내지 않는다. 키를 누른 동안 재활성화를 막고, 뗀 뒤 200ms 동안 잔여 움직임을 무시한다. 기존에 정상 전달된 키의 release는 보존한다.
- 이 규칙에서는 왼쪽에 배치된 마우스 버튼도 클릭 대신 자동 MOUSE를 해제한다. 자동 해제 로직이 수동으로 잠근 MOUSE 레이어를 강제로 끄지는 않는다.
- `tests/patch_auto_mouse.py`가 고정된 ZMK의 behavior dispatcher에 press/release 필터를 삽입한다. 투명 바인딩은 통과시키고 hold-tap 판정 후 실제 동작을 필터링한다. 사용자 정의 수정키 behavior는 별도 검토가 필요하다.
- `python tests/check_auto_mouse.py`, `python tests/auto_mouse_driver_test.py`와 `tests/auto_mouse_test.c`는 설정·패치 및 호스트 회귀 검사다. Zephyr 빌드나 실물 BLE 검증을 대신하지 않는다.
