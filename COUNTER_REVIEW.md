# 최종 카운터 리뷰

대상 펌웨어 소스 `3e576882a0920be3de4b7cc4967679d065c81381`. 수정 후 실행 40513302937에서 6개 대상과 검증 job이 모두 성공했고, 다운로드 산출물 검사도 PASS였다. 아래 결과는 소스·컴파일·생성 설정의 검증이며 실물 검증을 주장하지 않는다.

## 지적 및 수정

- **P1** `modules/threshold-layer/src/input_processor_threshold_temp_layer.c:70,117,157`: main API와 인자 불일치로 빌드 실패. index→ID 변환과 non-locking 인자 추가; 최종 Prospector 빌드 성공.
- **P1** `modules/non-lipo/src/non_lipo_battery_management.c:34,287`: 정수 300000을 IS_ENABLED로 검사해 5분 절전 코드 제외. >0으로 수정; 최종 양쪽 컴파일 및 설정 확인. 실제 전류·깨움은 실물 검증 대상.
- **P1** `config/voltball.keymap:16`: 자동 타이머가 수동 MOUSE 홀드를 해제할 위험. 수동 mo/lt를 locking으로 보호하고 자동 해제는 non-locking으로 제한.
- **P1** `config/boards/shields/voltball/voltball-trackball-processing.dtsi:15`: 자동 MOUSE 상태에서 클릭 입력 전 레이어가 꺼질 위험. 클릭 위치 35/36/37을 제외 목록에 포함; 최종 DTS 확인.
- **P1** `config/voltball.keymap`: 38→42키 및 레이어 오매핑 위험. 승인된 위치 적용, tests/check_mapping.py에서 228개 원본 바인딩·9개 combo·hold-tap 시간·추가 키·42개 좌표/transform 검사 PASS.
- **P1** peripheral 하드웨어의 central 혼입 위험. layout만 공유하고 AA/GPIO/SPI를 분리. 최종 central config에서 NON_LIPO/PAW3222/STATUS_LED/SLEEP 미활성, DTS에서 AA 노드 부재 확인.
- **P1** 조도 기능 n만으로는 upstream APDS9960 드라이버가 남음. prospector-fixed snippet으로 센서/I2C disabled 및 CONFIG_APDS9960=n. 최종 config/DTS 확인.
- **P2** `tests/patch_modules.py:18`: 배터리 미수신 초기 이벤트가 0%로 보일 수 있음. 정확한 Prospector SHA에 무효 source 패치를 적용해 N/A 유지. 생성 patch 아티팩트에서 확인.
- **P2** UF2만 보존하면 재현 근거 부족. 빌드 Actions SHA·컨테이너 digest 고정, 각 보드 config/DTS/frozen manifest/패치/소스 SHA 저장. 전체 아티팩트와 체크섬 확인.
- **P2** Studio 물리 좌표 편집을 지원한다고 오인할 위험. 키 바인딩 편집과 소스 좌표 변경/재빌드를 안내서에서 구분.

## 요구사항별 근거

| 항목 | 확인한 근거 | 판정 |
| --- | --- | --- |
| 키 위치·레이어 | 원본 fixture 대조 및 승인 매핑, BASE0/SYM1/NAVI2/FUNC3/NUM4/MOUSE5/SCROLL6 | 소스 검사 통과 |
| BLE 입력 | 오른쪽 PAW3222→input-split reg0, central reg0→listener, NAVI2/SCROLL6 override, 자동 MOUSE5 | 양쪽 생성 DTS/config 및 빌드 통과 |
| 배터리·GPIO 분리 | AA 두 peripheral만 활성, central 배터리 fetching=2 peripherals, 화면은 peripheral event 사용 | 생성 설정 및 소스 확인 |
| 화면·메모리 | 240×280, brightness50, APDS9960 disabled, central RAM63.91%/FLASH69.92% | 컴파일·설정 통과, 화면 실물 미검증 |
| Studio | central UART=y, BLE transport=n, locking=y, 42키 physical-layout, unlock33 | 컴파일·설정 통과, USB UI 실물 미검증 |
| 절전·재연결 | peripheral sleep=y/adv timeout300000, central sleep=n, 기존 IRQ/wakeup GPIO 유지 | 소스·설정 확인, 실물 미검증 |
| CI 재현성 | 성공 소스 SHA, 고정 모듈/Actions/이미지, frozen 활성 의존성, UF2 6개 SHA256 | 기록 및 다운로드 검사 통과 |

이 검토에서 발견한 구현 결함은 수정하고 관련 6개 빌드를 다시 통과시켰다. 잔여 실물 검증: 키 감각/hold-tap·콤보 타이밍, 클릭·드래그, 화면 및 배터리 보고, 실제 소비전류·저전압·5분 깨움, 양쪽 순서별 재연결, USB 재연결·전원 재투입. 구체적인 절차는 FLASHING_AND_VALIDATION.md에 있다.
