# AA 배터리 모듈 출처와 패치

- 원본: https://github.com/sekigon-gonnoc/zmk-feature-non-lipo-battery-management
- 원본 SHA: `43fa56fb7d1e065393e926b6caa0ea02cb86eca7`
- 라이선스: 함께 보존한 MIT LICENSE.
- 원본 CMakeLists.txt, Kconfig, dts, src를 복사했다. 독립 west 프로젝트 대신 이 저장소의 Zephyr 모듈에 포함한다.
- 유일한 C 소스 변경: 두 곳의 `IS_ENABLED(CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT)`를 `CONFIG_ZMK_NON_LIPO_ADV_SLEEP_TIMEOUT > 0`으로 변경했다.

`IS_ENABLED`는 boolean Kconfig 값 1을 확인한다. 이 설정은 밀리초 단위 정수이므로 300000으로 설정하면 원본에서는 타임아웃 코드와 초기화 코드가 제외된다. 수정 후 0은 비활성화, 양수는 지정 시간의 타임아웃을 활성화한다.

이 수정은 소스 수준 수정이며 Actions 컴파일 및 실물 5분 절전/복귀 검증은 아직 완료되지 않았다. 모듈은 `CONFIG_ZMK_NON_LIPO_BATTERY_MANAGEMENT`가 활성화된 Voltball 양쪽에서만 컴파일된다.
