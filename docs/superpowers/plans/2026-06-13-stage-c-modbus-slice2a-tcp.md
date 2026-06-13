# Stage C slice 2a — Modbus TCP (W5500, static IP) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a standard Modbus-TCP slave server over a W5500 Ethernet controller (SPI1, static IP from FRAM), reusing the slice-1 shared Modbus core and RTU register-apply path unchanged.

**Architecture:** Mirror the slice-1 three-layer split. A pure, host-tested MBAP framing unit (`app_modbus_tcp_frame`) wraps the untouched `app_modbus_core` (`MB_MODE_TCP`); a HAL/ioLibrary transport (`app_modbus_tcp` + `app_eth` + `spi1`) drives the socket FSM and W5500; `app_modbus.c` exposes its shared core + write-apply pass so TCP and RTU share register semantics, and `app_modbus_tick()` runs exactly one transport per tick (RTU when `comm_mode==SERIAL`, else TCP when `comm_mode==ETH` and the W5500 is present). The vendored upstream WIZnet ioLibrary is isolated as a separate warning-suppressed CMake library.

**Tech Stack:** C11, STM32F410 HAL (SPI1), WIZnet ioLibrary_Driver (vendored), CMake/Ninja (`arm-none-eabi-gcc`), host unit tests via `fw/test/Makefile` (plain `cc` + manual asserts), `mbpoll -m tcp` for HW E2E.

**Branch:** `feat/stage-c-modbus-tcp-static` (already created, base = main `e351cad`; spec committed `65c7d25`).

**Spec:** `docs/superpowers/specs/2026-06-13-stage-c-modbus-slice2a-tcp-design.md`

---

## File Structure

| File | Responsibility | Layer | Host-tested? |
|---|---|---|---|
| `fw/vendor/wiznet/Ethernet/wizchip_conf.{c,h}` | Vendored ioLibrary chip config (`_WIZCHIP_=W5500`) + callback registration + netinfo | vendor | no (3rd-party) |
| `fw/vendor/wiznet/Ethernet/socket.{c,h}` | Vendored ioLibrary BSD-like socket API | vendor | no |
| `fw/vendor/wiznet/Ethernet/W5500/w5500.{c,h}` | Vendored W5500 register access | vendor | no |
| `fw/vendor/wiznet/README.md` | Pinned upstream version/commit + which files vendored | vendor | n/a |
| `fw/drivers/spi1.c` + `fw/include/spi1.h` | SPI1 master + W5500 CS/RST GPIO + ioLibrary SPI callbacks | transport driver | no (HAL) |
| `fw/src/app_modbus_tcp_frame.c` + `fw/include/app_modbus_tcp_frame.h` | **Pure** MBAP strip + core decode + MBAP wrap (CRC stripped, unit echoed) | pure | **yes** |
| `fw/src/app_eth.c` + `fw/include/app_eth.h` | W5500 bring-up: register callbacks, reset, `wizchip_init`, PHY-link non-fatal timeout, `setnetinfo` from cfg | bringup | no (HAL) |
| `fw/src/app_modbus_tcp.c` + `fw/include/app_modbus_tcp.h` | Socket FSM (port 502) + recv → frame → shared apply → send | transport | no (HAL/socket) |
| `fw/src/app_modbus.c` + `fw/include/app_modbus.h` | **Modify**: expose shared core + write-apply; run TCP transport + mirror in `app_modbus_tick()` when in ETH mode | integration | core test unaffected |
| `fw/src/main.c` | **Modify**: call `app_eth_init()` once at boot | integration | no |
| `fw/test/test_app_modbus_tcp_frame.c` + `fw/test/Makefile` | Host test for the pure framing unit | test | — |
| `fw/CMakeLists.txt` | **Modify**: vendored wiznet library + include path | build | — |

---

## Task 1: Vendor the WIZnet ioLibrary + CMake integration (warning-isolated build)

> **Why first (advisor):** The vendored API surface and the 0-warning isolation strategy gate every layer built on top. Pin the version and prove the build is green *before* writing `app_eth`/`app_modbus_tcp`, so a later API/warning surprise doesn't force rework. The current upstream defaults `_WIZCHIP_` to **W6300** — it MUST be set to `W5500` or `wiz_NetInfo` takes the IPv6-extended shape.

**Files:**
- Create: `fw/vendor/wiznet/Ethernet/wizchip_conf.{c,h}`, `fw/vendor/wiznet/Ethernet/socket.{c,h}`, `fw/vendor/wiznet/Ethernet/W5500/w5500.{c,h}` (copied from upstream)
- Create: `fw/vendor/wiznet/README.md`
- Modify: `fw/CMakeLists.txt`

- [ ] **Step 1: Fetch + vendor the upstream files (pin the version)**

Per the research-reuse rule, vendor from the official repo and pin a release. Run from repo root:

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
TMP=$(mktemp -d)
git clone --depth 1 https://github.com/Wiznet/ioLibrary_Driver.git "$TMP/iolib"
( cd "$TMP/iolib" && git log -1 --format='%H %d' )   # RECORD this commit hash for the README
mkdir -p fw/vendor/wiznet/Ethernet/W5500
cp "$TMP/iolib/Ethernet/wizchip_conf.c" "$TMP/iolib/Ethernet/wizchip_conf.h" fw/vendor/wiznet/Ethernet/
cp "$TMP/iolib/Ethernet/socket.c"       "$TMP/iolib/Ethernet/socket.h"       fw/vendor/wiznet/Ethernet/
cp "$TMP/iolib/Ethernet/W5500/w5500.c"  "$TMP/iolib/Ethernet/W5500/w5500.h"  fw/vendor/wiznet/Ethernet/W5500/
rm -rf "$TMP"
```

Expected: 6 files copied. (Do NOT vendor `Internet/DHCP` — that is slice 2b. Do NOT vendor `loopback`.)

- [ ] **Step 2: Set the chip to W5500 in `wizchip_conf.h`**

Open `fw/vendor/wiznet/Ethernet/wizchip_conf.h` and find the chip-selection block (a series of `#define _WIZCHIP_ <CHIP>` with all but one commented). Make exactly `W5500` active:

```c
//#define _WIZCHIP_                      W5100
//#define _WIZCHIP_                      W5100S
//#define _WIZCHIP_                      W5200
//#define _WIZCHIP_                      W5300
#define _WIZCHIP_                      W5500   /* <-- this project (LQFP W5500) */
//#define _WIZCHIP_                      W6100
//#define _WIZCHIP_                      W6300
```

This is the ONLY edit allowed inside the vendored sources (config selection). After it, `wiz_NetInfo` resolves to the classic `{ uint8_t mac[6]; ip[4]; sn[4]; gw[4]; dns[4]; dhcp_mode dhcp; }`.

- [ ] **Step 3: Write the vendor README (provenance)**

Create `fw/vendor/wiznet/README.md`:

```markdown
# Vendored WIZnet ioLibrary_Driver (W5500 subset)

Upstream: https://github.com/Wiznet/ioLibrary_Driver
Pinned commit: <PASTE THE HASH FROM TASK 1 STEP 1>
Fetched: 2026-06-13

Vendored subset (Modbus-TCP server over W5500, static IP — Stage C slice 2a):
- Ethernet/wizchip_conf.{c,h}
- Ethernet/socket.{c,h}
- Ethernet/W5500/w5500.{c,h}

Local edit (config only): `wizchip_conf.h` sets `#define _WIZCHIP_ W5500`.
NOT vendored: Internet/DHCP (slice 2b), loopback, other PHY chips.
Treated as read-only vendor code (like fw/vendor/Drivers ST HAL) — warnings
isolated in CMake (separate library, SYSTEM includes). Do not hand-edit beyond
the chip-select.
```

- [ ] **Step 4: Add the warning-isolated CMake library + include path**

In `fw/CMakeLists.txt`, after the `stm32_hal` library block (after line 69), add:

```cmake
# vendor WIZnet ioLibrary (W5500) — warning-isolated, SYSTEM includes
set(WIZNET ${VENDOR}/wiznet)
add_library(wiznet STATIC
    ${WIZNET}/Ethernet/wizchip_conf.c
    ${WIZNET}/Ethernet/socket.c
    ${WIZNET}/Ethernet/W5500/w5500.c
)
target_include_directories(wiznet SYSTEM PUBLIC
    ${WIZNET}/Ethernet
    ${WIZNET}/Ethernet/W5500
)
# 3rd-party: do not hold it to our -Wall -Wextra -Wundef -Wshadow gate
target_compile_options(wiznet PRIVATE -w)
```

Then link + expose its headers to the app target. Change the `target_link_libraries` line (currently line 81) to add `wiznet`:

```cmake
target_link_libraries(${PROJECT_NAME}.elf PRIVATE stm32_hal wiznet)
```

(The `wiznet` target's `SYSTEM PUBLIC` includes propagate to the app target, so our code can `#include "wizchip_conf.h"` / `"socket.h"` without warnings from those headers.)

- [ ] **Step 5: Configure + build to prove the vendor compiles and OUR code stays 0-warning**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b1.log
```

Expected: build succeeds. The `wiznet` lib may emit nothing (suppressed by `-w`). Crucially, confirm **no warnings from our own sources**:

```bash
grep -iE 'warning:' /tmp/b1.log | grep -v '/vendor/' || echo "0 warnings in our code"
```

Expected: `0 warnings in our code`. (At this point no app code uses wiznet yet — `--gc-sections` will drop the unused lib from the ELF; that is fine. The goal of Task 1 is a green, isolated vendor build.)

- [ ] **Step 6: Commit**

```bash
git add fw/vendor/wiznet fw/CMakeLists.txt
git commit -m "build(stage-c): vendor WIZnet ioLibrary (W5500 subset), warning-isolated"
```

---

## Task 2: SPI1 driver + W5500 CS/RST GPIO (`spi1.c`)

> No host test — this is a HAL peripheral layer (like `usart6_mb.c` / `board.c`); correctness is proven by build + HW. Provides the byte/burst/CS/reset primitives the ioLibrary callbacks need.

**Files:**
- Create: `fw/include/spi1.h`, `fw/src` … actually `fw/drivers/spi1.c` (drivers/ is GLOB'd)
- Reference: `docs/pinmap.md` §SPI1 (PA4 CS sw-GPIO, PA5/6/7 AF5, PC5 NRST out, PC4 INT in)

- [ ] **Step 1: Write the header `fw/include/spi1.h`**

```c
/* fw/include/spi1.h — SPI1 master + W5500 CS/RST GPIO (spec §4.2).
 * PA5/6/7 = SCK/MISO/MOSI (AF5), PA4 = software-GPIO CS (NOT HW NSS — W5500
 * needs CS held LOW across a multi-byte frame), PC5 = NRST out, PC4 = INT in
 * (configured, polled-mode so unused this slice). These functions back the
 * ioLibrary callbacks registered in app_eth.c. */
#pragma once
#include <stdint.h>

void    spi1_init(void);                 /* clocks, GPIO AF, SPI1 CR (Mode 0, MSB) */
uint8_t spi1_xfer_byte(uint8_t tx);      /* blocking 1-byte transceive */
void    spi1_burst_read(uint8_t *buf, uint16_t len);
void    spi1_burst_write(uint8_t *buf, uint16_t len);
void    spi1_cs_low(void);               /* assert CS (PA4 low) */
void    spi1_cs_high(void);              /* deassert CS (PA4 high) */
void    spi1_eth_reset(void);            /* PC5 NRST pulse (W5500 reset timing) */
```

- [ ] **Step 2: Write `fw/drivers/spi1.c`**

```c
/* fw/drivers/spi1.c — see spi1.h. HAL SPI1 @ ~12 MHz (APB2 96 MHz / 8),
 * Mode 0 (CPOL=0/CPHA=0), 8-bit MSB-first. CS = PA4 plain GPIO. */
#include "spi1.h"
#include "stm32f4xx_hal.h"

static SPI_HandleTypeDef s_spi1;

#define ETH_CS_PORT    GPIOA
#define ETH_CS_PIN     GPIO_PIN_4
#define ETH_RST_PORT   GPIOC
#define ETH_RST_PIN    GPIO_PIN_5
#define ETH_INT_PORT   GPIOC
#define ETH_INT_PIN    GPIO_PIN_4

void spi1_cs_low(void)  { HAL_GPIO_WritePin(ETH_CS_PORT, ETH_CS_PIN, GPIO_PIN_RESET); }
void spi1_cs_high(void) { HAL_GPIO_WritePin(ETH_CS_PORT, ETH_CS_PIN, GPIO_PIN_SET);   }

void spi1_eth_reset(void)
{
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(2);                 /* W5500 RSTn low >= 500us */
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(60);                /* PLL lock >= 50ms before access */
}

uint8_t spi1_xfer_byte(uint8_t tx)
{
    uint8_t rx = 0u;
    (void)HAL_SPI_TransmitReceive(&s_spi1, &tx, &rx, 1u, 10u);
    return rx;
}

void spi1_burst_read(uint8_t *buf, uint16_t len)
{
    /* W5500 burst read: clock out 0xFF, capture MISO. */
    for (uint16_t i = 0u; i < len; i++) { buf[i] = spi1_xfer_byte(0xFFu); }
}

void spi1_burst_write(uint8_t *buf, uint16_t len)
{
    (void)HAL_SPI_Transmit(&s_spi1, buf, len, 100u);
}

void spi1_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_InitTypeDef g = {0};

    /* PA5/6/7 = SPI1 SCK/MISO/MOSI, AF5 */
    g.Pin       = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode      = GPIO_MODE_AF_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);

    /* PA4 = software CS, idle HIGH */
    HAL_GPIO_WritePin(ETH_CS_PORT, ETH_CS_PIN, GPIO_PIN_SET);
    g.Pin       = ETH_CS_PIN;
    g.Mode      = GPIO_MODE_OUTPUT_PP;
    g.Pull      = GPIO_NOPULL;
    g.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    g.Alternate = 0u;
    HAL_GPIO_Init(ETH_CS_PORT, &g);

    /* PC5 = NRST out, idle HIGH (deasserted) */
    HAL_GPIO_WritePin(ETH_RST_PORT, ETH_RST_PIN, GPIO_PIN_SET);
    g.Pin  = ETH_RST_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    HAL_GPIO_Init(ETH_RST_PORT, &g);

    /* PC4 = INT input (polled this slice) */
    g.Pin  = ETH_INT_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(ETH_INT_PORT, &g);

    s_spi1.Instance               = SPI1;
    s_spi1.Init.Mode              = SPI_MODE_MASTER;
    s_spi1.Init.Direction         = SPI_DIRECTION_2LINES;
    s_spi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    s_spi1.Init.CLKPolarity       = SPI_POLARITY_LOW;     /* Mode 0 */
    s_spi1.Init.CLKPhase          = SPI_PHASE_1EDGE;      /* Mode 0 */
    s_spi1.Init.NSS               = SPI_NSS_SOFT;
    s_spi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;  /* 96/8 = 12 MHz */
    s_spi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    s_spi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    s_spi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    s_spi1.Init.CRCPolynomial     = 7u;
    (void)HAL_SPI_Init(&s_spi1);
}
```

- [ ] **Step 3: Enable the SPI HAL module + add the HAL SPI source**

`stm32f4xx_hal_spi.c` is not in the HAL_SOURCES list yet. In `fw/CMakeLists.txt` add to `HAL_SOURCES` (after the ADC line ~56):

```cmake
    # Stage C slice 2a — W5500 SPI:
    ${VENDOR}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_spi.c
```

Then enable the module in the HAL conf. Open `fw/vendor/Core/Inc/stm32f4xx_hal_conf.h` (or wherever `HAL_*_MODULE_ENABLED` live — confirm path with `grep -rl HAL_I2C_MODULE_ENABLED fw/vendor/Core fw/include`) and ensure `#define HAL_SPI_MODULE_ENABLED` is present (uncomment if commented).

- [ ] **Step 4: Build (syntax + 0-warning on our code)**

```bash
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja   # picks up new HAL src + GLOB'd spi1.c
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b2.log
grep -iE 'warning:' /tmp/b2.log | grep -v '/vendor/' || echo "0 warnings in our code"
```

Expected: build OK, `0 warnings in our code`. (`spi1_*` unused so far → `--gc-sections` drops it; that is fine.)

- [ ] **Step 5: Commit**

```bash
git add fw/drivers/spi1.c fw/include/spi1.h fw/CMakeLists.txt fw/vendor/Core/Inc/stm32f4xx_hal_conf.h
git commit -m "feat(stage-c): SPI1 master + W5500 CS/RST GPIO driver"
```

---

## Task 3: Pure MBAP framing unit (`app_modbus_tcp_frame`) — TDD with host test

> This is the one host-testable unit (advisor #3). Hard boundary: byte buffers + `mb_core_t *` only, zero socket/HAL. Tests come first.

**Files:**
- Create: `fw/include/app_modbus_tcp_frame.h`, `fw/src/app_modbus_tcp_frame.c`
- Test: `fw/test/test_app_modbus_tcp_frame.c`, modify `fw/test/Makefile`

- [ ] **Step 1: Write the header `fw/include/app_modbus_tcp_frame.h`**

```c
/* fw/include/app_modbus_tcp_frame.h — pure standard-Modbus-TCP framing
 * (spec §3). MBAP strip on input, MBAP wrap on output, trailing CRC dropped,
 * request unit id echoed. Wraps the untouched mb_core (MB_MODE_TCP). No socket
 * or HAL — host-tested. samd20's raw-response quirk is intentionally dropped
 * (spec §1.1/§3.3). */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "app_modbus_core.h"

#define MB_TCP_MBAP_LEN  6u                 /* txn(2)+proto(2)+length(2) before unit */
#define MB_TCP_RESP_MAX  (MB_RESP_MAX + 4u) /* 6 MBAP + (core resp - 2 CRC) */

/* Build a standard Modbus-TCP response from ONE received TCP frame.
 * req/req_len = raw recv() bytes (MBAP + PDU). On a frame that must be
 * answered, fills out[0..*out_len-1] and returns true. Returns false (send
 * nothing) when: < 8 bytes, protocol id != 0, MBAP length field < 2, the
 * declared length exceeds what was received or the core frame bound, or the
 * core stays silent (malformed / unsupported FC — samd20-faithful).
 * *fc_out = function code processed (0 if none) so the caller runs the
 * write-apply pass when *fc_out == 0x06. */
bool mb_tcp_build_response(mb_core_t *mb, const uint8_t *req, uint16_t req_len,
                           uint8_t out[MB_TCP_RESP_MAX], uint16_t *out_len,
                           uint8_t *fc_out);
```

- [ ] **Step 2: Write the failing test `fw/test/test_app_modbus_tcp_frame.c`**

```c
/* Host unit test for the pure Modbus-TCP framing unit. Manual asserts
 * (same style as test_app_modbus_core.c). */
#include <stdio.h>
#include <string.h>
#include "app_modbus_tcp_frame.h"
#include "app_modbus_core.h"

static int g_fail = 0;
#define CHECK(c) do { if(!(c)){ printf("FAIL %s:%d  %s\n",__FILE__,__LINE__,#c); g_fail++; } } while(0)

/* FC03 read 1 holding register at 0x0006 (OUT_POWER) = 80 (0x0050). */
static void test_fc03_read_one(void)
{
    mb_core_t mb;
    mb_core_init(&mb, 1u);
    mb.holding[MB_REG_OUT_POWER] = 80u;

    /* MBAP txn=0x0001 proto=0 length=6 ; PDU unit=1 fc=3 start=0x0006 cnt=0x0001 */
    uint8_t req[] = { 0x00,0x01, 0x00,0x00, 0x00,0x06,
                      0x01, 0x03, 0x00,0x06, 0x00,0x01 };
    uint8_t out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u;
    uint8_t  fc = 0xFFu;

    bool resp = mb_tcp_build_response(&mb, req, (uint16_t)sizeof req, out, &out_len, &fc);
    CHECK(resp == true);
    CHECK(fc == 0x03u);
    /* expected wire: txn echo 0001, proto 0000, len 0005, unit 01, fc 03,
       bytecount 02, data 00 50  — NO CRC. total 11 bytes. */
    CHECK(out_len == 11u);
    CHECK(out[0]==0x00 && out[1]==0x01);   /* txn echo */
    CHECK(out[2]==0x00 && out[3]==0x00);   /* proto */
    CHECK(out[4]==0x00 && out[5]==0x05);   /* length = 5 (big-endian) */
    CHECK(out[6]==0x01);                   /* unit echo */
    CHECK(out[7]==0x03);                   /* fc */
    CHECK(out[8]==0x02);                   /* byte count */
    CHECK(out[9]==0x00 && out[10]==0x50);  /* value 80 */
}

/* FC06 write OUT_POWER = 90 (0x005A): standard response echoes the request PDU. */
static void test_fc06_write(void)
{
    mb_core_t mb;
    mb_core_init(&mb, 1u);

    uint8_t req[] = { 0x12,0x34, 0x00,0x00, 0x00,0x06,
                      0x01, 0x06, 0x00,0x06, 0x00,0x5A };
    uint8_t out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u;
    uint8_t  fc = 0u;

    bool resp = mb_tcp_build_response(&mb, req, (uint16_t)sizeof req, out, &out_len, &fc);
    CHECK(resp == true);
    CHECK(fc == 0x06u);                    /* caller will run apply pass */
    CHECK(out_len == 12u);                 /* 6 MBAP + [unit fc addrHi addrLo valHi valLo] */
    CHECK(out[0]==0x12 && out[1]==0x34);   /* txn echo */
    CHECK(out[4]==0x00 && out[5]==0x06);   /* length = 6 */
    CHECK(out[6]==0x01 && out[7]==0x06);   /* unit, fc */
    CHECK(out[8]==0x00 && out[9]==0x06);   /* reg addr echo */
    CHECK(out[10]==0x00 && out[11]==0x5A); /* value echo */
    /* and the holding reg actually took the write (core wrote it): */
    CHECK(mb.holding[MB_REG_OUT_POWER] == 0x5Au);
}

static void test_rejects(void)
{
    mb_core_t mb;
    mb_core_init(&mb, 1u);
    uint8_t out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u; uint8_t fc = 0u;

    /* too short (<8) */
    uint8_t a[] = { 0,1,0,0,0,1,1 };
    CHECK(mb_tcp_build_response(&mb, a, sizeof a, out, &out_len, &fc) == false);

    /* protocol id != 0 */
    uint8_t b[] = { 0,1, 0,9, 0,6, 1,3,0,6,0,1 };
    CHECK(mb_tcp_build_response(&mb, b, sizeof b, out, &out_len, &fc) == false);

    /* declared length longer than received */
    uint8_t c[] = { 0,1, 0,0, 0,20, 1,3,0,6,0,1 };
    CHECK(mb_tcp_build_response(&mb, c, sizeof c, out, &out_len, &fc) == false);
}

int main(void)
{
    test_fc03_read_one();
    test_fc06_write();
    test_rejects();
    if (g_fail == 0) { printf("app_modbus_tcp_frame: all checks PASSED\n"); return 0; }
    printf("app_modbus_tcp_frame: %d FAILED\n", g_fail);
    return 1;
}
```

- [ ] **Step 3: Wire the test into `fw/test/Makefile`**

Edit `fw/test/Makefile` — add the binary, building against the pure core too:

```make
BIN_REG := /tmp/gds_test_app_reg_calc
BIN_MB  := /tmp/gds_test_app_modbus_core
BIN_TCP := /tmp/gds_test_app_modbus_tcp_frame

test: $(BIN_REG) $(BIN_MB) $(BIN_TCP)
	$(BIN_REG)
	$(BIN_MB)
	$(BIN_TCP)
```

and add the rule (alongside the existing rules):

```make
$(BIN_TCP): test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c ../include/app_modbus_tcp_frame.h ../include/app_modbus_core.h
	$(CC) $(CFLAGS) $(INC) -o $@ test_app_modbus_tcp_frame.c ../src/app_modbus_tcp_frame.c ../src/app_modbus_core.c
```

Also add `$(BIN_TCP)` to the `clean` rule's `rm -f` line.

- [ ] **Step 4: Run the test to verify it FAILS (no implementation yet)**

```bash
make -C fw/test test
```

Expected: FAIL — compile/link error `undefined reference to mb_tcp_build_response` (the `.c` is empty/absent).

- [ ] **Step 5: Implement `fw/src/app_modbus_tcp_frame.c`**

```c
/* fw/src/app_modbus_tcp_frame.c — see app_modbus_tcp_frame.h. */
#include "app_modbus_tcp_frame.h"

bool mb_tcp_build_response(mb_core_t *mb, const uint8_t *req, uint16_t req_len,
                           uint8_t out[MB_TCP_RESP_MAX], uint16_t *out_len,
                           uint8_t *fc_out)
{
    *fc_out  = 0u;
    *out_len = 0u;

    if (req_len < 8u) { return false; }                 /* MBAP(6)+unit+fc */

    uint16_t proto  = (uint16_t)(((uint16_t)req[2] << 8) | req[3]);
    uint16_t length = (uint16_t)(((uint16_t)req[4] << 8) | req[5]);  /* unit+PDU */
    if (proto != 0u)  { return false; }
    if (length < 2u)  { return false; }                 /* need unit + fc */
    if ((uint32_t)req_len < (uint32_t)MB_TCP_MBAP_LEN + (uint32_t)length) {
        return false;                                   /* truncated */
    }
    if (length > MB_FRAME_MAX) { return false; }        /* core frame bound */

    /* PDU-for-core = [unit, fc, data...] at req[6], len = MBAP length field.
     * MB_MODE_TCP skips the unit/addr filter + CRC check. */
    uint8_t resp[MB_RESP_MAX];
    uint8_t n = mb_core_decode(mb, &req[6], (uint8_t)length, MB_MODE_TCP,
                               resp, fc_out);
    if (n == 0u) { return false; }                      /* core silent */

    /* core resp = [unit, fc, data, CRC_hi, CRC_lo]; drop the trailing 2 CRC. */
    uint16_t pdu_len = (uint16_t)(n - 2u);              /* unit + PDU bytes */

    out[0] = req[0];                       /* txn id hi (echo) */
    out[1] = req[1];                       /* txn id lo (echo) */
    out[2] = 0u;                           /* proto id hi */
    out[3] = 0u;                           /* proto id lo */
    out[4] = (uint8_t)(pdu_len >> 8);      /* length hi (big-endian) */
    out[5] = (uint8_t)(pdu_len & 0xFFu);   /* length lo */
    for (uint16_t i = 0u; i < pdu_len; i++) {
        out[6u + i] = resp[i];             /* [unit, fc, data] — no CRC */
    }
    out[6] = req[6];                       /* unit id = echo request unit (standard) */
    *out_len = (uint16_t)(6u + pdu_len);
    return true;
}
```

- [ ] **Step 6: Run the test to verify it PASSES**

```bash
make -C fw/test test
```

Expected: `app_modbus_tcp_frame: all checks PASSED` (and the other two suites still PASS).

- [ ] **Step 7: Confirm the firmware build still 0-warning (the new src is GLOB'd in)**

```bash
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b3.log
grep -iE 'warning:' /tmp/b3.log | grep -v '/vendor/' || echo "0 warnings in our code"
```

Expected: `0 warnings in our code`.

- [ ] **Step 8: Commit**

```bash
git add fw/include/app_modbus_tcp_frame.h fw/src/app_modbus_tcp_frame.c fw/test/test_app_modbus_tcp_frame.c fw/test/Makefile
git commit -m "feat(stage-c): pure standard-Modbus-TCP MBAP framing + host test"
```

---

## Task 4: Expose shared core + write-apply from `app_modbus` (behavior-preserving)

> **⚠ Sensitive — advisor #1.** The RTU write-apply path (`apply_writes()`) and its register table (`g_mb`) were hardware-verified and merged (tag `hw-revA_fw-stage-c1`: "FC06 OUT_POWER write+clamp+FRAM" matrix item). This task is a **pure visibility change** (file-static → externally callable) with **zero logic change** to the chain. Do NOT edit the clamp chain, the FRAM commit, or `mirror_live()`. Re-verification posture: build-green + host-pass is sufficient (the function body is byte-identical, only its linkage changes); the next board session should still spot-check FC06-over-RTU once as a regression guard (noted in §HW task).

**Files:**
- Modify: `fw/src/app_modbus.c`, `fw/include/app_modbus.h`

- [ ] **Step 1: Add the two accessors to the header `fw/include/app_modbus.h`**

After the `app_modbus_owns_usart6()` declaration, add:

```c
#include "app_modbus_core.h"

/* Shared Modbus core instance (register tables). RTU (this file) and TCP
 * (app_modbus_tcp.c) decode into the SAME core so register semantics are
 * single-sourced. */
mb_core_t *app_modbus_core(void);

/* The samd20 update_holding_reg(1) write-apply pass: clamp the one changed
 * config field, persist to FRAM, dispatch commands. Called by whichever
 * transport processed an FC06 (RTU inline; TCP via this accessor). Operates
 * on the shared core from app_modbus_core(). */
void app_modbus_apply_writes(void);
```

- [ ] **Step 2: Rename `apply_writes` → public + add the core accessor in `fw/src/app_modbus.c`**

Change the definition at line 84 from:

```c
static void apply_writes(void)
```

to:

```c
void app_modbus_apply_writes(void)
```

and update its single internal caller at line 299 (inside `app_modbus_tick`) from `apply_writes();` to `app_modbus_apply_writes();`. Add the core accessor near `app_modbus_owns_usart6()` (after line 36):

```c
mb_core_t *app_modbus_core(void)
{
    return &g_mb;
}
```

**Do not touch any other line** in `apply_writes` / `mirror_live` / `apply_config`.

- [ ] **Step 3: Build + host test (prove no behavior change)**

```bash
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b4.log
grep -iE 'warning:' /tmp/b4.log | grep -v '/vendor/' || echo "0 warnings in our code"
make -C fw/test test
```

Expected: `0 warnings in our code`; all three host suites PASS. (`app_modbus_core` test is unchanged and still green — the RTU semantics are untouched.)

- [ ] **Step 4: Commit**

```bash
git add fw/src/app_modbus.c fw/include/app_modbus.h
git commit -m "refactor(stage-c): expose shared mb core + write-apply for TCP reuse (no behavior change)"
```

---

## Task 5: W5500 bring-up (`app_eth`) — non-fatal init

> HAL/ioLibrary layer, no host test. Registers the SPI1 callbacks, resets + inits the W5500, applies static netinfo from cfg, and reports availability. PHY-link wait is non-fatal so a board without a populated/linked W5500 still boots (spec §4.3).

**Files:**
- Create: `fw/include/app_eth.h`, `fw/src/app_eth.c`

- [ ] **Step 1: Write the header `fw/include/app_eth.h`**

```c
/* fw/include/app_eth.h — W5500 bring-up over SPI1 (spec §4.3). Static IP from
 * cfg(FRAM); MAC = hardcoded constant. Non-fatal: if the chip is absent or the
 * link never comes up, app_eth_init() returns false and the TCP path stays off
 * — RTU / mon / LCD are unaffected. */
#pragma once
#include <stdbool.h>

/* Bring up SPI1 + W5500, apply static netinfo from cfg. Returns true if the
 * chip answered and the PHY linked within the timeout. Call once at boot,
 * after app_init() (needs cfg loaded). */
bool app_eth_init(void);

/* True if app_eth_init() succeeded (W5500 present + linked). */
bool app_eth_available(void);
```

- [ ] **Step 2: Write `fw/src/app_eth.c`**

```c
/* fw/src/app_eth.c — see app_eth.h. ioLibrary callback wiring mirrors samd20
 * ethernet.c, retargeted to STM32 SPI1 (spi1.c) + W5500 chip. */
#include "app_eth.h"
#include "spi1.h"
#include "app_lcd.h"        /* app_lcd_cfg() -> ether_ip/nm/gw */
#include "app_config.h"
#include "mon.h"
#include "wizchip_conf.h"
#include "socket.h"

/* Faithful to samd20 ethernet.c default MAC (per-unit MAC = future, spec §8). */
static const uint8_t kEthMac[6] = { 0x00, 0x08, 0xdc, 0x78, 0x91, 0x71 };

static bool s_available = false;

bool app_eth_available(void) { return s_available; }

/* ioLibrary CS callbacks (it wants void(*)(void)). */
static void cs_sel(void)   { spi1_cs_low();  }
static void cs_desel(void) { spi1_cs_high(); }

bool app_eth_init(void)
{
    s_available = false;
    spi1_init();

    /* register the SPI / CS callbacks (byte + burst). */
    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(/*rb*/ NULL, /*wb*/ NULL);  /* replaced below */
    /* The byte callbacks must match the ioLibrary signatures:
     *   uint8_t (*spi_rb)(void)  ;  void (*spi_wb)(uint8_t) */
    extern uint8_t spi1_rb(void);
    extern void    spi1_wb(uint8_t b);
    reg_wizchip_spi_cbfunc(spi1_rb, spi1_wb);
    reg_wizchip_spiburst_cbfunc(spi1_burst_read, spi1_burst_write);

    spi1_eth_reset();

    /* socket buffer sizes: 2KB each, socket 0 only used. wizchip_init returns
     * -1 if the requested sizes don't fit. */
    uint8_t txsize[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
    uint8_t rxsize[8] = { 2, 2, 2, 2, 2, 2, 2, 2 };
    if (wizchip_init(txsize, rxsize) != 0) {
        mon_printf("[eth] wizchip_init failed — unavailable\r\n");
        return false;
    }

    /* PHY-link poll, non-fatal timeout (~1s = 100 x 10ms). */
    int8_t link = PHY_LINK_OFF;
    for (int i = 0; i < 100; i++) {
        if (ctlwizchip(CW_GET_PHYLINK, (void *)&link) == 0 && link == PHY_LINK_ON) {
            break;
        }
        HAL_Delay(10);
    }
    if (link != PHY_LINK_ON) {
        mon_printf("[eth] no PHY link — unavailable\r\n");
        return false;
    }

    /* static netinfo from cfg(FRAM). */
    const app_config_t *cfg = app_lcd_cfg();
    wiz_NetInfo ni;
    for (int i = 0; i < 6; i++) { ni.mac[i] = kEthMac[i]; }
    for (int i = 0; i < 4; i++) {
        ni.ip[i] = cfg->ether_ip[i];
        ni.sn[i] = cfg->ether_nm[i];
        ni.gw[i] = cfg->ether_gw[i];
        ni.dns[i] = 0u;
    }
    ni.dhcp = NETINFO_STATIC;
    wizchip_setnetinfo(&ni);

    mon_printf("[eth] up ip=%u.%u.%u.%u\r\n",
               ni.ip[0], ni.ip[1], ni.ip[2], ni.ip[3]);
    s_available = true;
    return true;
}
```

- [ ] **Step 3: Add the byte SPI callbacks to `spi1`**

The ioLibrary byte read callback is `uint8_t(*)(void)` and write is `void(*)(uint8_t)`, which differ from `spi1_xfer_byte`. Add thin adapters. In `fw/include/spi1.h` add:

```c
uint8_t spi1_rb(void);          /* ioLibrary read byte: clock 0xFF, return MISO */
void    spi1_wb(uint8_t b);     /* ioLibrary write byte */
```

In `fw/drivers/spi1.c` add:

```c
uint8_t spi1_rb(void)        { return spi1_xfer_byte(0xFFu); }
void    spi1_wb(uint8_t b)   { (void)spi1_xfer_byte(b); }
```

Then in `app_eth.c` Step 2, replace the `extern` declarations with the header include path (they are now declared in `spi1.h`, already included) — delete the two `extern` lines and the first `reg_wizchip_spi_cbfunc(NULL,NULL)` placeholder, keeping only:

```c
    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi1_rb, spi1_wb);
    reg_wizchip_spiburst_cbfunc(spi1_burst_read, spi1_burst_write);
```

- [ ] **Step 4: Build (0-warning on our code)**

```bash
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b5.log
grep -iE 'warning:' /tmp/b5.log | grep -v '/vendor/' || echo "0 warnings in our code"
```

Expected: build OK, `0 warnings in our code`. If `HAL_Delay`/`mon_printf` prototypes are missing, add the includes (`stm32f4xx_hal.h` for `HAL_Delay` is pulled via `app_eth.c`'s chain; if not, `#include "stm32f4xx_hal.h"`).

- [ ] **Step 5: Commit**

```bash
git add fw/include/app_eth.h fw/src/app_eth.c fw/drivers/spi1.c fw/include/spi1.h
git commit -m "feat(stage-c): W5500 bring-up (non-fatal init, static netinfo from cfg)"
```

---

## Task 6: TCP transport + socket FSM (`app_modbus_tcp`)

> HAL/socket layer, no host test (framing already covered by Task 3). Ports samd20 `process_tcp.c control_tcps` + the main-loop recv/decode/send (main.c:5069-5100), retargeted to the shared core + standard framing.

**Files:**
- Create: `fw/include/app_modbus_tcp.h`, `fw/src/app_modbus_tcp.c`

- [ ] **Step 1: Write the header `fw/include/app_modbus_tcp.h`**

```c
/* fw/include/app_modbus_tcp.h — Modbus-TCP server transport over W5500
 * socket 0, port 502 (spec §3.2). Drives the socket state machine and, on a
 * received frame, runs the pure framing (app_modbus_tcp_frame) against the
 * shared core (app_modbus_core) and the shared write-apply (app_modbus). Call
 * each tick while in ETH mode with the W5500 available. */
#pragma once

void app_modbus_tcp_poll(void);
```

- [ ] **Step 2: Write `fw/src/app_modbus_tcp.c`**

```c
/* fw/src/app_modbus_tcp.c — see app_modbus_tcp.h.
 *
 * ASSUMPTION (spec §3, advisor #4): the request/response cadence of a Modbus
 * master means each recv() yields exactly ONE complete MBAP frame. We do not
 * accumulate partial frames or split coalesced ones — a short read is rejected
 * by mb_tcp_build_response() (length-field check) and the master retries. This
 * is sufficient for mbpoll-class masters; streaming reassembly is future work. */
#include "app_modbus_tcp.h"
#include "app_modbus.h"            /* app_modbus_core(), app_modbus_apply_writes() */
#include "app_modbus_tcp_frame.h"
#include "socket.h"
#include "wizchip_conf.h"

#define MB_TCP_SOCK   0u
#define MB_TCP_PORT   502u

static uint8_t s_rxbuf[MB_FRAME_MAX + MB_TCP_MBAP_LEN];

/* samd20 control_tcps: walk the socket state machine one step. */
static void control_tcp(void)
{
    switch (getSn_SR(MB_TCP_SOCK)) {
        case SOCK_ESTABLISHED:
            if (getSn_IR(MB_TCP_SOCK) & Sn_IR_CON) {
                setSn_IR(MB_TCP_SOCK, Sn_IR_CON);
            }
            break;
        case SOCK_CLOSE_WAIT:
            (void)disconnect(MB_TCP_SOCK);
            break;
        case SOCK_INIT:
            (void)listen(MB_TCP_SOCK);
            break;
        case SOCK_CLOSED:
            (void)socket(MB_TCP_SOCK, Sn_MR_TCP, MB_TCP_PORT, 0x00u);
            break;
        default:
            break;
    }
}

void app_modbus_tcp_poll(void)
{
    control_tcp();

    if (getSn_SR(MB_TCP_SOCK) != SOCK_ESTABLISHED) {
        return;
    }
    uint16_t avail = getSn_RX_RSR(MB_TCP_SOCK);
    if (avail == 0u) {
        return;
    }
    if (avail > (uint16_t)sizeof s_rxbuf) {
        avail = (uint16_t)sizeof s_rxbuf;
    }
    int32_t got = recv(MB_TCP_SOCK, s_rxbuf, avail);
    if (got <= 0) {
        return;
    }

    uint8_t  out[MB_TCP_RESP_MAX];
    uint16_t out_len = 0u;
    uint8_t  fc = 0u;
    if (mb_tcp_build_response(app_modbus_core(), s_rxbuf, (uint16_t)got,
                              out, &out_len, &fc)) {
        if (fc == 0x06u) {
            app_modbus_apply_writes();   /* shared with RTU: clamp + FRAM + cmd */
        }
        (void)send(MB_TCP_SOCK, out, out_len);
    }
}
```

- [ ] **Step 3: Build (0-warning on our code)**

```bash
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b6.log
grep -iE 'warning:' /tmp/b6.log | grep -v '/vendor/' || echo "0 warnings in our code"
```

Expected: build OK, `0 warnings in our code`. (Still unreferenced from `app.c` → dropped by gc-sections; wired in Task 7.)

- [ ] **Step 4: Commit**

```bash
git add fw/include/app_modbus_tcp.h fw/src/app_modbus_tcp.c
git commit -m "feat(stage-c): Modbus-TCP server transport (socket FSM, port 502)"
```

---

## Task 7: Integration — boot init + `app_modbus_tick` ETH branch

> Wires the TCP server into the superloop. **Closes a real gap:** in ETH mode the existing `app_modbus_tick()` returns early (RTU not owning USART6) so `mirror_live()` never runs — the TCP branch must refresh the mirror. On entering ETH the shared core is re-seeded with the configured slave address.

**Files:**
- Modify: `fw/src/main.c`, `fw/src/app_modbus.c`

- [ ] **Step 1: Call `app_eth_init()` once at boot in `fw/src/main.c`**

`app_modbus_init()` is already called at `main.c:27` after `app_init()` (cfg loaded). Add the eth init right after it. Add the include near the top with the other app includes:

```c
#include "app_eth.h"
```

and after the `app_modbus_init();` line:

```c
    app_eth_init();    /* Stage C slice 2a: W5500 bring-up (non-fatal). TCP
                        * server runs from app_modbus_tick() when comm_mode==ETH. */
```

- [ ] **Step 2: Add the ETH branch to `app_modbus_tick()` in `fw/src/app_modbus.c`**

Add includes (top of file, with the others):

```c
#include "app_eth.h"
#include "app_modbus_tcp.h"
```

Add a file-static activation latch near `g_applied` (after line 31):

```c
static uint8_t g_tcp_active;   /* rising-edge baseline guard for ETH mode */
```

Replace the body of `app_modbus_tick()` (lines 282-304) with:

```c
void app_modbus_tick(void)
{
    apply_config();
    const app_config_t *cfg = app_lcd_cfg();

    if (g_applied.owned != 0u) {
        /* RTU owns USART6 (comm_mode==SERIAL && addr!=0). */
        g_tcp_active = 0u;
        uint8_t frame[MB_FRAME_MAX];
        uint8_t len = usart6_mb_rx_frame(frame, sizeof frame);
        if (len != 0u) {
            uint8_t resp[MB_RESP_MAX];
            uint8_t fc = 0u;
            uint8_t n  = mb_core_decode(&g_mb, frame, len, MB_MODE_RTU, resp, &fc);
            if (n != 0u) {
                usart6_mb_send(resp, n);
            }
            if (fc == 0x06u) {
                app_modbus_apply_writes();   /* samd20: update_holding_reg(1) on FC06 */
            }
        }
        mirror_live();
        return;
    }

    /* Not RTU. Run the TCP server when in an ETH mode and the W5500 is up. */
    if ((cfg->comm_mode != MB_COMM_MODE_SERIAL) && app_eth_available()) {
        if (g_tcp_active == 0u) {
            mb_core_init(&g_mb, cfg->comm_address);  /* seed addr + zero tables */
            g_tcp_active = 1u;
        }
        app_modbus_tcp_poll();   /* decode(MB_MODE_TCP) + apply on FC06 + respond */
        mirror_live();           /* keep holding[] fresh for reads (closes the gap) */
    } else {
        g_tcp_active = 0u;
    }
}
```

(Note: `mirror_live` and `g_mb` are file-static in this same file — no new exposure needed. The TCP poll reaches `g_mb` via `app_modbus_core()` and the apply via `app_modbus_apply_writes()`, both already public from Task 4.)

- [ ] **Step 3: Build (0-warning on our code)**

```bash
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b7.log
grep -iE 'warning:' /tmp/b7.log | grep -v '/vendor/' || echo "0 warnings in our code"
arm-none-eabi-size fw/build/gds_us_ctrl.elf
```

Expected: build OK, `0 warnings in our code`. Now the wiznet lib + spi1 + app_eth + app_modbus_tcp are all reachable, so the ELF grows (FLASH/RAM up — record the size). Verify FLASH stays well under 128 KB.

- [ ] **Step 4: Host tests still green (no regression to pure layers)**

```bash
make -C fw/test test
```

Expected: all three suites PASS (`app_reg_calc`, `app_modbus_core`, `app_modbus_tcp_frame`).

- [ ] **Step 5: Commit**

```bash
git add fw/src/main.c fw/src/app_modbus.c
git commit -m "feat(stage-c): wire W5500 TCP server into superloop (ETH-mode gating + mirror)"
```

---

## Task 8: Build gate + final verification (host-complete)

> The slice is code-complete and host-verified here. HW E2E is Task 9 (board-gated).

- [ ] **Step 1: Clean reconfigure + build, assert 0-warning on our code**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
rm -rf fw/build
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/bfinal.log
grep -iE 'warning:' /tmp/bfinal.log | grep -v '/vendor/' || echo "0 warnings in our code"
arm-none-eabi-size fw/build/gds_us_ctrl.elf
```

Expected: clean build, `0 warnings in our code`, size printed (record FLASH %/RAM % for the changelog).

- [ ] **Step 2: Full host test run**

```bash
make -C fw/test clean && make -C fw/test test
```

Expected: `app_reg_calc … PASSED`, `app_modbus_core … PASSED`, `app_modbus_tcp_frame: all checks PASSED`.

- [ ] **Step 3: Request code review**

Per subagent-driven-development, run the integrated `cpp-reviewer` over the slice diff (`git diff main...HEAD`). Address CRITICAL/HIGH; record MEDIUM/LOW dispositions. Pay attention to: the Task 4 extraction being truly behavior-preserving, the framing bounds (`MB_TCP_RESP_MAX`, `length > MB_FRAME_MAX`), and the ETH/RTU mutual exclusion + mirror refresh.

- [ ] **Step 4: Commit any review fixes, then update tracking docs**

Update `docs/changelog.md` (2026-06-13 entry: slice 2a code-complete, host PASS, size), and `docs/superpowers/RESUME.md` status block (slice 2a code-complete, next = HW E2E). Commit:

```bash
git add docs/changelog.md docs/superpowers/RESUME.md
git commit -m "docs(stage-c): slice 2a Modbus TCP code-complete, host PASS — next HW E2E"
```

---

## Task 9: HW E2E (board + W5500) — HW-GATED, board session

> Runs when the board + a populated/linked W5500 are connected. Mirror of the slice-1 RTU matrix, over `mbpoll -m tcp`. No code expected; if a defect surfaces, fix on this branch then re-run.

**Setup:**
- Via LCD/FRAM set `comm_mode = ETH_STATIC` (1) and a static IP/NM/GW on the host subnet (e.g. ip 192.168.0.15, nm 255.255.255.0, gw 192.168.0.1). Power-cycle; confirm mon prints `[eth] up ip=…` (or `[eth] no PHY link / unavailable` if the W5500 is absent — which proves the non-fatal path).
- Host: `IP=192.168.0.15`. Sanity: `ping $IP`.

- [ ] **Step 1: Link + listen sanity**
  Mon shows `[eth] up`. `mbpoll -m tcp -a 1 -t 4 -r 7 -c 1 -1 $IP` returns one register (socket opened → listen → established → response → close).

- [ ] **Step 2: FC03 poll = live mirror**
  `mbpoll -m tcp -a 1 -t 4 -r 1 -c 30 -1 $IP` → dump of 0x00..0x1D equals the boot `[cfg]`/`set_pot` values (same as slice-1 RTU item ①). STATUS (0x1D) = 0 when idle.

- [ ] **Step 3: FC06 write + clamp + FRAM persistence**
  Write OUT_POWER=80 (`mbpoll -m tcp -a 1 -t 4 -r 7 $IP 80`), read back 80. Write 30 → reads 50 (clamp), 120 → 100. `openocd … reset`, power on, read back → clamped value persists (FRAM). (= slice-1 item ②.)

- [ ] **Step 4: START → ceiling → STOP + ICON_RUN**
  Write START (0x1B=1) → STATUS bit0=1, 560 ms on-time ceiling auto-stops (limit_on_time=56), ICON_RUN visible on the LCD; write STOP (0x1C=1). Repeat ×3. (= slice-1 item ③, ICON_RUN visual confirmed by user.)

- [ ] **Step 5: Occupancy serial↔eth switch**
  With TCP polling, switch `comm_mode` to SERIAL via LCD → mon resumes + TCP times out; switch back to ETH → TCP re-acquires. (= slice-1 item ④, the eth side.)

- [ ] **Step 6: Register preservation + work_cnt**
  Re-poll after writes → values preserved. Write work_cnt-low (0x01) = 0 → accepted. (= slice-1 items ⑤⑥.)

- [ ] **Step 7: RTU regression spot-check (advisor #1 guard)**
  Switch back to SERIAL, `mbpoll -a 1 -b <baud> -P <parity> -t 4 -r 7 $DEV 77` → reads back 77, persists — confirms the Task 4 apply-path extraction did not regress RTU.

- [ ] **Step 8: Record + finish**
  Log the matrix in `docs/changelog.md`. Restore OUT_POWER to its bench default. Per `superpowers:finishing-a-development-branch`: merge to main (`--no-ff`) + tag `hw-revA_fw-stage-c2a`, post-merge build 0-warning + host PASS.

---

## Self-Review Notes (author)

- **Spec coverage:** §1.1 standard-TCP decision → Task 3 framing + tests. §2 modules → Tasks 1/2/3/5/6/7. §2.1 shared core/apply → Task 4. §3 data flow + framing math → Task 3 (impl + tests assert the wire bytes). §3.2 socket FSM → Task 6. §4 pins/SPI/bringup → Tasks 2/5. §5 occupancy/gating/boot init → Task 7. §6 tests/gate → Tasks 3/8; HW matrix → Task 9. §8 unresolved (MAC, SPI clock, INT, warning isolation, ioLibrary version) → resolved concretely in Tasks 1/2/5. §9 deferred (DHCP) → not in plan (correct).
- **Type/name consistency:** `mb_tcp_build_response`, `MB_TCP_RESP_MAX`, `MB_TCP_MBAP_LEN`, `app_modbus_core()`, `app_modbus_apply_writes()`, `app_modbus_tcp_poll()`, `app_eth_init()`/`app_eth_available()`, `spi1_rb`/`spi1_wb`/`spi1_burst_read`/`spi1_burst_write`/`spi1_cs_low`/`spi1_cs_high`/`spi1_eth_reset` — used identically across tasks.
- **Big-endian MBAP length** (advisor minor): explicit in Task 3 (`out[4]=hi, out[5]=lo`) + the FC03 test asserts `out[4]==0x00 && out[5]==0x05`.
- **Watch at impl time:** confirm `wizchip_init`/`PHY_LINK_ON`/`CW_GET_PHYLINK`/`getSn_RX_RSR` names against the vendored headers (Task 1 fetched master); if a name differs, the symbol is in `wizchip_conf.h`/`socket.h`/`w5500.h` — grep there, do not guess. `getSn_SR/getSn_IR/setSn_IR/Sn_MR_TCP/Sn_IR_CON` live in `w5500.h`.
