# HW finding — STD 이더넷/comm_mode 영속 + comm_mode 표시 버그 (2026-05-27)

> 브랜치 `feat/stage-lcd-full-behavior`, T10 검증 중(항목 ⑤ STD 이더넷) 사용자 요청으로 "STD도 MULTI처럼 ether/comm_mode 영속" 작업하다 발견·확장.
> 상태: **영속 수정 = 구현+HW검증 완료(미커밋). 표시 버그 = root cause 확정, 수정 미착수(설계 필요).**
> systematic-debugging Phase 1~3, LCD_TRACE_RX + commit/boot 진단으로 ground truth 확보.

## 한 줄 요약

STD 저장 경로가 comm_mode/ether를 커밋 안 하던 samd20 퀴크를 수정(→ **데이터는 FRAM에 정상 영속, `boot cm=2` 확인**). 그러나 **comm_mode 표시(serial/ethernet/dhcp 체크)가 부팅 후 STDE 진입에서 틀리게 뜸**(serial+dhcp 동시 체크). 표시 버그는 `temp_comm_mode` 섀도우 생명주기 결함이며, 수정 중 **comm_mode=0xFF 커밋 손상 위험**도 드러남.

## 작업 1 — STD ether/comm_mode 영속 (구현+검증 완료, 미커밋)

- **변경**: `data_save_commit()` STD 분기에 `commit_comm_mode_and_ether();` 추가 (app_lcd_input.c). 사용자 승인.
- **검증 (LCD_TRACE_RX 진단 빌드)**: `commit cm temp=1 cfg=0` → 커밋, 전원사이클 후 `boot cm=1` → 영속. DHCP도 `boot cm=2` 영속 확인. IP(192.168.1.128)도 영속.
- **결론**: 영속 자체는 정상 동작. = 사용자가 처음 본 "ethernet enable 저장 안됨"은 영속 실패가 아니라 아래 표시 버그였음.

## 작업 2 — comm_mode 표시 버그 (root cause 확정, 미수정)

### 증상
STD에서 DHCP 설정→저장→전원 OFF/ON→STDE 화면에 **serial과 dhcp가 동시에 체크**. (데이터는 `boot cm=2`로 정상.)

### Root cause (`temp_comm_mode` 섀도우 생명주기, 복합)
1. **부팅 zero-init**: `static lcd_app_state_t g_lcd;` → `temp_comm_mode = 0`(=COMM_SERIAL), **0xFF sentinel 아님**(app_lcd.c:10).
2. **seed가 0xFF 게이트**: render seed 블록(`render.c:143/178` `if temp_comm_mode==0xff`)이 cfg→섀도우 로드를 1회만 함. 부팅 후 첫 comm 페이지 진입에서 temp_comm_mode=0(≠0xff)이면 **seed 스킵** → temp_comm_mode가 cfg(=2)를 반영 못 하고 0(serial)에 머묾.
3. **표시**: `render.c:198` `if (temp_comm_mode==0) DISP_COMM_MODE=0`(serial) → serial 체크. else만 `DISP_EN_DHCP` 씀.
4. **serial 분기가 DISP_EN_DHCP 미클리어**: serial일 때 `DISP_EN_DHCP=0`을 안 써서 이전 dhcp 체크가 **stale로 남음** → serial+dhcp 동시.
   - (STD1/STD2/STD3 등 페이지 진입이 `temp_comm_mode=0xff`를 세팅하므로(115/120/124/131), 그 경로를 거치면 seed가 돌아 정상 표시됨 — 그래서 nav 경로에 따라 재현 여부가 갈림.)

### 추가 발견 — comm_mode=0xFF 커밋 손상 위험 (latent)
`commit_comm_mode_and_ether()`(app_lcd_input.c)는 `if (temp_comm_mode != cfg->comm_mode) cfg->comm_mode = temp_comm_mode;`로 **0xFF sentinel을 거르지 않음**. comm 페이지 미방문 상태(temp_comm_mode=0xFF)에서 MULTI/STD 셋업 페이지 저장 시 **cfg->comm_mode=0xFF(255) 손상** → FRAM 기록 → 다음 부팅 garbage. MULTI에도 이미 latent(STD3/MH2 진입이 0xff 세팅 후 저장). 작업 1이 이 위험을 STD로 확장.

## 제안 수정 (coherent, 설계 후 적용 — reactive 패치 금지)

`temp_comm_mode` 섀도우 생명주기를 일관되게:
- **A. 부팅 init**: `app_lcd_init_mode`에서 `temp_comm_mode = 0xFF`(미로드 sentinel) — 첫 comm 페이지 진입이 cfg에서 seed하도록.
- **B. 커밋 가드**: `commit_comm_mode_and_ether`에서 `temp_comm_mode == 0xFF`면 comm_mode/ether 커밋 스킵(미로드 = cfg 불변). 손상 방지.
- **C. seed**: A 적용 시 기존 0xFF 게이트가 정상 동작(첫 진입 seed).
- **D. 표시 일관**: serial 분기에서 `DISP_EN_DHCP=0` 명시 기록(stale dhcp 제거). ethernet 분기는 이미 DISP_COMM_MODE=1+DISP_EN_DHCP 기록.

> samd20 원본의 comm 섀도우 로드 방식(0xFF sentinel이 포팅 발명인지 samd20 패턴인지) 대조 필요. 4지점이 상호작용 + 손상 위험이므로 spec/plan 후 적용 권장.

## 진단 스캐폴딩 (머지 전 정리 필요)
LCD_TRACE_RX 게이트로 추가됨 (머지 바이너리엔 컴파일 아웃):
- `app.c`: `[lcd] boot cm=%u ip=...` (부팅 시 FRAM 로드값).
- `app_lcd_input.c` commit_comm_mode_and_ether: `[lcd] commit cm temp=%u cfg=%u`.
→ 유지(게이트 진단으로 유용) 또는 제거 결정 필요.

## 현재 working tree 상태 (미커밋)
- STD `commit_comm_mode_and_ether` 추가 (작업 1, 검증됨).
- 진단 2개 (위).
- 모델-셋업 롱프레스 수정은 별도 커밋됨(`c6c89ef`).
