# Voltball + Prospector 상태

2026-10-27: 승인된 키 매핑 반영, 구현 및 Actions 검증 완료. 실물 동작은 미검증.

- 성공 실행: https://github.com/mastercollection/zmk-for-voltball/actions/runs/40513302937
- 펌웨어 소스 SHA: `3e576882a0920be3de4b7cc4967679d065c81381`
- 작업 브랜치: `codex/voltball-prospector-main`
- 원본 ZIP: `D:/Git/voltball/zmk-for-voltball-main` (보존).
- 작업 clone: `D:/Git/voltball/work`. 기본 브랜치에는 병합하지 않았다.
- 원본 기준 SHA: `cda0b6ee3554bc3e73882f5f1a26d3d003136144`.
- 원본 ZIP은 줄바꿈 이외 원격 소스와 동일했으며 ZIP의 GOAL.md도 보존했다.

결과/SHA/체크섬: [BUILD_REPORT.md](BUILD_REPORT.md).
카운터 리뷰: [COUNTER_REVIEW.md](COUNTER_REVIEW.md).
플래싱·복구·실물 검증: [FLASHING_AND_VALIDATION.md](FLASHING_AND_VALIDATION.md).

사용자가 승인한 매핑: 공통 문자 위치 유지, 바깥 Ctrl→32, 바깥 Enter→41, TOTEM 엄지 32–37→35,36,37,38,39,40. 추가 25=MOUSE, 26=SCROLL, 33=Studio unlock, 34=미할당. BASE0/SYM1/NAVI2/FUNC3/NUM4/MOUSE5 유지, SCROLL6 추가. 자동 레이어5, 방향키2, 스크롤6.

Studio는 42키 물리 배열을 표시하고 키 바인딩을 편집한다. 좌표 자체의 변경은 layout dtsi 수정 및 Actions 재빌드로 수행한다. 실제 장치의 키/트랙볼/화면/전력/재연결은 현장 검증표로 확인해야 한다.
