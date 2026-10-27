# 자동 레이어 모듈

원본: https://github.com/gohanda11/zmk-input-processor-threshold-temp-layer

고정 SHA: `a68fedf50dd3b25d7aa594776c26b943fe41cfbf`. 원본 파일의 MIT SPDX 및 저작권 표기를 보존했다. 원본 저장소에는 별도 LICENSE 파일이 없다.

main 호환 패치: 레이어 index를 ID로 변환하고 activate/deactivate에 `locking=false`를 전달한다. 수동 `mo`/`lt`는 `&mo { locking; };`로 소유권을 유지하므로 자동 해제가 수동 홀드를 취소하지 않는다. 사용자 입력에 따른 원래 hold-tap 시간과 바인딩은 유지한다.

MOUSE 클릭 위치 35/36/37은 입력 직전에 자동 레이어가 해제되지 않도록 excluded-positions에 포함한다. 입력 처리기는 Prospector에만 존재한다.
