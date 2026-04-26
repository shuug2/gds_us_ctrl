# Historical Artifacts

이 폴더의 파일들은 **read-only 보존 자료**입니다. 수정하거나 의존하지 마세요.

## gds_usctrl.ioc

STM32CubeMX 프로젝트 파일. 2026-04-26 brainstorming에서 CubeMX UI 의존을 제거하기로
결정한 후, 회로 핀 할당 이력 추적 목적으로만 보존. 펌웨어 빌드는 본 파일을 참조하지 않으며
실제 핀 매핑의 source of truth는 `docs/pinmap.md`입니다.
