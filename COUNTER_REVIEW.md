# 카운터 리뷰 (진행 중)

최종 Actions와 산출물 검증 후 확정한다. 아래 소스 검토는 실물 검증을 대체하지 않는다.

| 심각도 | 발견 사항 | 조치 및 증거 |
| --- | --- | --- |
| P1 | 원본 threshold 모듈은 main 레이어 API의 locking 인자가 없어 Prospector 빌드 실패 | 첫 실행 40510459892, job 122210765144 로그의 too few arguments. 고정 원본을 포함하고 index→ID 변환 및 false 인자를 추가; 수정 빌드 진행 중 |
| P1 | AA 광고 절전 설정 300000을 IS_ENABLED로 검사하여 코드가 빠짐 | 고정 배터리 모듈의 조건 두 곳을 >0으로 수정; 양쪽 초기 Actions 컴파일 성공. 실물 5분/깨움 검증 필요 |
| P1 | 자동 MOUSE 레이어 해제가 수동 레이어 홀드를 취소할 수 있음 | main의 locking API를 사용해 수동 mo/lt 홀드를 보호하고 자동 해제는 non-locking으로 제한 |
| P1 | 클릭 위치 입력이 자동 MOUSE 레이어를 먼저 해제할 위험 | MOUSE의 클릭 위치 35/36/37을 excluded-positions에 포함. 클릭/드래그와 release는 실물 확인 필요 |
| P1 | TOTEM과 Voltball의 하단 배열 및 레이어 번호가 다름 | 사용자가 승인한 38→42 매핑 적용. tests/check_mapping.py가 228개 바인딩, 9개 combo, hold-tap 시간, 추가 4키, 42개 transform/좌표 개수를 검사 |
| P1 | AA/GPIO가 Prospector로 유입될 위험 | layout만 공유, AA/kscan/SPI/LED는 peripheral 전용 dtsi/conf로 분리. 최종 빌드 DTS/Kconfig로 재확인 예정 |
| P2 | Studio에서 임의 물리 좌표를 편집할 수 있다고 오인할 수 있음 | 펌웨어 물리 배열 표시/키 바인딩 편집과 소스 좌표 수정/재빌드를 안내서에서 구분 |
| P1 | 조도 기능을 끈 상태에도 upstream DT 노드로 APDS9960 드라이버가 활성화됨 | 실행 40510893928에서 CONFIG_APDS9960=y 확인. 최종 적용 snippet에서 센서/I2C 노드를 disabled, conf에서 드라이버 n 지정. 재빌드 필요 |
| P2 | 실제 배터리 보고 전 초기화 이벤트가 0%로 표시될 수 있음 | 고정 Prospector SHA와 원문 두 곳을 검사하는 CI 패치로 미수신 source를 무효값으로 처리해 초기 N/A 유지 |
| P2 | UF2만 저장하면 실제 적용 설정과 의존성 SHA를 추적하기 어려움 | 고정 ZMK reusable workflow를 로컬에 포함하고 Actions SHA 고정. 각 타깃의 frozen manifest, config, DTS, 적용 패치, 소스 SHA를 UF2와 함께 저장 |

남은 검토: 최종 Prospector RAM/FLASH, fixed brightness/센서 비활성화, 양쪽 실제 배터리 보고와 연결 슬롯, USB Studio, 재연결/전원 재투입, 아티팩트 및 SHA 기록. 전체 목표는 아직 미완료다.
