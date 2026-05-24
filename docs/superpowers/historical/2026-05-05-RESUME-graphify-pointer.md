# RESUME — Graphify Regeneration (fresh session)

> **2026-05-05**: Phase 1+2 Bootstrap **doc-sync까지 모두 완료** (Chunks 1–13 doc-side).
> 남은 단 한 가지는 `graphify-out/` 재생성. 본 RESUME은 그 한 가지만을 위한 가이드.

---

## ⚡ 새 세션 한 줄 절차

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl-phase12
claude
```

세션 시작 후 한 줄 입력 (스코프는 아래 §1 참조):

```
/graphify fw/src
```

> 본 RESUME(`/Users/tknoh/dev/work/gds_us_ctrl/docs/superpowers/RESUME.md`)은 **메인 레포의 stale pointer**. 메인 레포에서 세션 시작 시 SessionStart 훅으로 자동 로드되지만 본격 작업은 위 worktree로 이동.

---

## 1. 스코프 선택

graphify는 단일 path만 받음. 후보:

| 스코프 | 파일 수 | 특징 |
|---|---|---|
| `fw/src` | 7 | Phase 1+2 신규 모듈만 (main, app, board, sys_tick, irq, periph, clock). **추천 — 가장 깔끔** |
| `fw/src` + `fw/drivers` + `fw/include` (각각 별도 실행) | 7+3+6 | 분할 그래프, merge 부담 있음 |
| 전체 worktree (`.`) | 273 | **fw/vendor (HAL/CMSIS) 247 file이 그래프를 잠식** — 비추천. AST는 무료지만 노드 수 폭증 |
| `docs` | 11 | 문서 지식 그래프만 |

권장 명령:
```
/graphify fw/src
```

다 끝나면 `graphify-out/` 산출물(`graph.html`, `graph.json`, `GRAPH_REPORT.md`)이 worktree 루트에 생성됨 (`.gitignore` 권장 — 메인 레포 패턴과 동일).

---

## 2. 직전 세션 결과 요약 (참고용)

**worktree branch**: `feat/phase1-2-bootstrap` (main +33 commits)

**최근 commit**:
```
f4da20e docs: sync CLAUDE.md / pinmap / changelog with Phase 1+2 bootstrap
765fc9b docs: fix TIM_AUTORELOAD_PRELOAD_DISABLE typo in spec/plan
09cc0a4 docs: update RESUME for Chunk 12 PASS (Phase 2 HW verify)
59d6e66 docs: update RESUME for Chunk 11 PASS (Phase 2 build verified)
de0c35f fw: fix TIM_AUTORELOAD_PRELOAD_DISABLE macro name (build fix)
```

**Phase 1+2 산출물**:
- FLASH 22 060 B (16.83%) / 128 KB
- RAM 2 520 B (7.69%) / 32 KB
- HW verify ✅: banner `[boot] gds_us_ctrl phase2 ready` + 1 Hz `[t=N ms] hello` + PB3 1 Hz heartbeat 모두 관찰
- 직전 RESUME 아카이브: `docs/superpowers/historical/2026-05-05-RESUME.md` (전체 진행 이력)

**Chunk 13 doc-side 완료 항목**:
- CLAUDE.md (worktree에 신규, 96 MHz, fw/vendor 구조)
- docs/pinmap.md (96 MHz + Phase 2 사용 핀)
- docs/changelog.md (Phase 1+2 완료 entry)
- spec/plan TIM_AUTORELOAD_PRELOAD_DISABLE 오타 정정
- docs/superpowers/RESUME.md → historical/ 아카이브

---

## 3. 다음 마일스톤 (graphify 후)

graphify 재생성 끝나면 Phase 1+2 Bootstrap 전체 종료. 다음은:

> **Stage A — LCD + IO** (DGUS LCD UART 드라이버, GPIO 입력/디바운스, GPIO 출력, Buzzer PWM, Monitor UART 보강)

Stage A 진입은 별도 brainstorming/spec 세션 권장 — `superpowers:brainstorming` 스킬 + `superpowers:writing-plans`로 새 plan 생성.

---

## 4. 트러블슈팅

### "/graphify가 graphifyy 패키지 설치를 시도하다 차단됨"
이미 `/opt/homebrew/bin/graphify` 설치돼 있음. `head -1 /opt/homebrew/bin/graphify`로 인터프리터 확인:
```
/opt/homebrew/opt/python@3.12/bin/python3.12
```
`graphify-out/.graphify_python`에 위 경로 직접 기록 후 진행.

### "corpus 200 files / 2M words 초과 경고"
스킬 규칙대로 사용자 확인 필요. `fw/src`로 좁혀 실행 추천 (§1 참조).
