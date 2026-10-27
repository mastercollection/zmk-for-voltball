# Voltball + Prospector 진행 기록

기준일: 2026-10-27 (KST). 구현 및 검증 진행 중이며 배포용 펌웨어는 없다.

## 작업 위치와 보존

- 원본 ZIP: `D:/Git/voltball/zmk-for-voltball-main` (변경하지 않음).
- 작업 복제본: `D:/Git/voltball/work`.
- 브랜치: `codex/voltball-prospector-main`.
- 원격 기준 커밋: `cda0b6ee3554bc3e73882f5f1a26d3d003136144`.
- 하드웨어 구성 커밋: `815759d` (원격 업로드 완료).
- ZIP과 원격 소스는 CRLF/LF를 제외하면 동일했다. ZIP에만 있는 GOAL.md도 그대로 보존했다.

## 고정한 소스 후보 (빌드 검증 전)

| 소스 | SHA |
| --- | --- |
| zmk main | 4dd95d7840eb3d5a3f59bb29fbf1ab054715ef85 |
| prospector feat/new-status-screens | ed98221f3b52b7066dbb10ba3af8a29150b93a5a |
| TOTEM master (키맵 출처) | b4b2073e35f1db04ca3ac4ba82a5520a55df16de |
| paw3222 | df652881be2520bde4f64ab6ca35e4c5708f4f9b |
| non-lipo-battery-management | 43fa56fb7d1e065393e926b6caa0ea02cb86eca7 |
| status-led | 1f827a452013199e42544d47fdf27e5ebd2d7b4b |
| input-processor-keybind | 64303bd3932bfa7f49261fc6d0c9a3fdd06f8271 |
| input-processor-threshold-temp-layer | a68fedf50dd3b25d7aa594776c26b943fe41cfbf |

## 구현한 부분

- main 보드 이름으로 6개 Actions 매트릭스 구성: 양쪽 Voltball, Prospector, 각 장치 settings_reset.
- 공유 물리 배열에서 AA 배터리, GPIO, SPI를 분리.
- PAW3222는 오른쪽에 유지하고 `zmk,input-split` reg 0으로 전달.
- Prospector의 virtual input listener에서 레이어 의존 처리.
- AA/저전압/광고 절전 설정은 양쪽 shield conf로 이동.
- Prospector central, 2 peripherals, USB Studio, 고정 밝기 50%, 디스플레이 버퍼 25% 설정.
- 배터리 모듈을 위 고정 SHA에서 `modules/non-lipo`로 포함하고 숫자형 광고 절전 타임아웃의 전처리 조건 두 곳을 수정했다. 원본과 비교해 C 소스 변경은 이 두 줄뿐임을 확인했다. 원본 라이선스 및 출처 기록을 보존했다.
- `FLASHING_AND_VALIDATION.md`에 백업, 3장치 초기화, 왼쪽 우선 페어링, Studio 복원, 복구, 실물 검증표 초안을 작성했다.

## 미완료 / 다음 작업

1. 사용자에게 보낸 38→42키 매핑 질문 답변 필요. 아직 TOTEM 키맵을 이식하지 않았다. 현재 키맵과 입력 처리 레이어 인덱스는 기존 Voltball 값이다.
2. Actions 실행 없음. CLI 인증은 샌드박스 밖에서 정상이며 repo/workflow 범위 및 저장소 admin 권한을 확인했다. 푸시 후 실행이 없고 workflows 목록도 빈 상태다. 명시적 `gh workflow run build.yml --ref codex/voltball-prospector-main`는 default branch workflow not found 404. 포크 Actions 초기 활성화 화면 확인을 사용자에게 요청했다. 브라우저는 GitHub 로그인 전이다.
3. Actions 시작 후 6개 빌드 실패 로그 수정, SHA 및 아티팩트 기록 필요. 로컬 펌웨어 빌드는 수행하지 않았다.
4. Studio는 물리 배열을 표시하고 바인딩 편집/저장 및 사전 정의 배열 선택을 지원한다. ZMK 자체는 Studio 안에서 새로운 물리 좌표를 만드는 기능을 지원하지 않는다 (`sources/zmk/docs/docs/features/studio.md`). 좌표 변경은 소스의 layout dtsi 편집 후 재빌드가 필요하다. 이 구분을 최종 안내에 명시할 것.
5. threshold-temp-layer 모듈의 자동 해제가 수동 MOUSE hold와 충돌할 가능성을 검토할 것. 타이머가 직접 layer_deactivate를 호출한다. 단순 빌드 통과로 동작 검증을 대체하지 말 것.
6. Prospector의 APDS9960은 conf에서 비활성화되어 있지만 upstream DT에는 노드가 있다. 최종 .config/.dts로 드라이버 비활성화 및 센서 접근 부재를 확인할 것.
7. 최종 카운터 리뷰, 플래싱/복구 절차, 실물 검증 체크리스트 작성 필요.
8. 후속 재조회에서도 Actions workflows total_count=0, 작업 브랜치 runs=[]였다. 실행 중인 작업을 기다리는 상태가 아니다. 키 매핑과 포크 Actions 초기 활성화 답변은 아직 없다.
9. 5분 광고 절전 결함: 원본 배터리 모듈의 34, 287행이 정수 설정을 `IS_ENABLED`로 검사했다. 수정 후 `> 0`으로 검사한다. 소스 비교 및 diff 공백 검사는 통과했으나 컴파일/실물 검증은 아직 필요하다.

## 사용자 확인 대기 중인 매핑 제안

- 공통 문자 30키는 물리 위치 유지.
- TOTEM index 20 (바깥 Ctrl) → Voltball 32; TOTEM 31 (바깥 Enter) → Voltball 41.
- TOTEM 엄지 32–37 → Voltball 35,36,37,38,39,40.
- 추가 Voltball 25=MOUSE 순간 활성화, 26=SCROLL 순간 활성화, 33=Studio unlock, 34=미할당.
- TOTEM 실제 레이어 순서 0 BASE, 1 SYM, 2 NAVI, 3 FUNC, 4 NUM, 5 MOUSE를 유지하고 SCROLL 6 추가 제안.
- 트랙볼 자동 MOUSE=5, 방향키 NAVI=2, 스크롤=6 제안. 답변 전 확정 금지.
