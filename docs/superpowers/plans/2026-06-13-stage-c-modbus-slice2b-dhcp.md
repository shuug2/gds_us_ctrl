# Stage C slice 2b — Modbus TCP (W5500, DHCP) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a DHCP client so `comm_mode==ETH_DHCP` dynamically acquires IP/NM/GW/DNS for the W5500 Modbus-TCP server built in slice 2a, reusing slice 2a's stack unchanged.

**Architecture:** Vendor the official WIZnet ioLibrary `Internet/DHCP/dhcp.{c,h}` (same pinned commit as 2a) into the warning-isolated `wiznet` lib. Extend `app_eth` only: `app_eth_init()` gains a DHCP branch (start client, stay unavailable), a new `app_eth_tick()` drives `DHCP_run()` + the 1-second `DHCP_time_handler()` from the superloop, and the IP-assign callback applies the lease + flips `app_eth_available()` true. `app_eth_available()` is extended to mean "TCP-ready" so `app_modbus_tick`'s existing gate (`comm_mode!=SERIAL && app_eth_available()`) works unchanged. Acquisition is non-blocking (boot never stalls on DHCP); the acquired IP is RAM-only (mirrored into live cfg for LCD display, never committed to FRAM). Core/framing/transport/SPI are untouched.

**Tech Stack:** C11, STM32F410 HAL, WIZnet ioLibrary_Driver DHCP (vendored), CMake/Ninja (`arm-none-eabi-gcc`), `mbpoll -m tcp` against a DHCP-served network for HW E2E.

**Branch:** `feat/stage-c-modbus-tcp-dhcp` (already created, base = slice 2a tip `faf89d5`; spec committed `5444aee`).

**Spec:** `docs/superpowers/specs/2026-06-13-stage-c-modbus-slice2b-dhcp-design.md`

---

## File Structure

| File | Responsibility | Change | Host-tested? |
|---|---|---|---|
| `fw/vendor/wiznet/Internet/DHCP/dhcp.{c,h}` | Vendored ioLibrary DHCP client | Create (copy upstream) | no (3rd-party) |
| `fw/vendor/wiznet/README.md` | Provenance (add DHCP subset note) | Modify | n/a |
| `fw/CMakeLists.txt` | Add dhcp.c to `wiznet` lib + DHCP include dir | Modify | — |
| `fw/include/app_eth.h` | Add `app_eth_tick()` decl + doc | Modify | no |
| `fw/src/app_eth.c` | DHCP init branch + assign/conflict callbacks + `app_eth_tick()` | Modify | no (HAL/socket) |
| `fw/src/app.c` | Call `app_eth_tick()` in `app_loop_iter` before `app_modbus_tick` | Modify | no |

No new host test (DHCP is HAL/socket/callback glue; the pure framing unit `app_modbus_tcp_frame` is unchanged — spec §6). The existing 3 host suites must stay green.

---

## Task 1: Vendor ioLibrary `Internet/DHCP/dhcp.{c,h}` + CMake (same pinned commit)

> **Why first:** the vendored DHCP API surface gates `app_eth`. Pin the SAME commit as slice 2a (`220ca7a6`) and prove the isolated build is green before writing app_eth, so a later API surprise doesn't force rework. After the files land, the controller (not the subagent) greps the vendored `dhcp.h` to confirm the exact API names/signatures and feeds them into Task 2.

**Files:**
- Create: `fw/vendor/wiznet/Internet/DHCP/dhcp.{c,h}` (copied from upstream)
- Modify: `fw/vendor/wiznet/README.md`, `fw/CMakeLists.txt`

- [ ] **Step 1: Fetch + vendor the DHCP files at the SAME pinned commit as slice 2a**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
TMP=$(mktemp -d)
git clone https://github.com/Wiznet/ioLibrary_Driver.git "$TMP/iolib"
( cd "$TMP/iolib" && git checkout 220ca7a6d92677830f1dab1383c47e1eb620cad9 )
mkdir -p fw/vendor/wiznet/Internet/DHCP
cp "$TMP/iolib/Internet/DHCP/dhcp.c" "$TMP/iolib/Internet/DHCP/dhcp.h" fw/vendor/wiznet/Internet/DHCP/
rm -rf "$TMP"
ls fw/vendor/wiznet/Internet/DHCP/
```

Expected: `dhcp.c  dhcp.h` copied. (Same commit `220ca7a6` that slice 2a pinned — do NOT take master tip; the `git checkout <hash>` is mandatory.)

- [ ] **Step 2: Confirm dhcp's includes resolve against the already-vendored headers**

```bash
grep -nE '^[[:space:]]*#include' fw/vendor/wiznet/Internet/DHCP/dhcp.c fw/vendor/wiznet/Internet/DHCP/dhcp.h
```

Expected: only `"socket.h"` and `"dhcp.h"` (and standard headers). `socket.h` is already vendored at `fw/vendor/wiznet/Ethernet/socket.h`. If dhcp.c `#include`s anything else not already vendored (e.g. a DNS or util header), STOP and report it — it would need vendoring from the same commit. (Per the samd20 reference `ref/samd20/W5500/dhcp.c`, the only includes are `socket.h` + `dhcp.h`.)

- [ ] **Step 3: Update the vendor README provenance**

In `fw/vendor/wiznet/README.md`, under the "Vendored subset" list, add the DHCP line. Change:

```markdown
Vendored subset (Modbus-TCP server over W5500, static IP — Stage C slice 2a):
- Ethernet/wizchip_conf.{c,h}
- Ethernet/socket.{c,h}
- Ethernet/W5500/w5500.{c,h}
```

to:

```markdown
Vendored subset (Modbus-TCP server over W5500 — Stage C slice 2a/2b):
- Ethernet/wizchip_conf.{c,h}
- Ethernet/socket.{c,h}
- Ethernet/W5500/w5500.{c,h}
- Internet/DHCP/dhcp.{c,h}   (slice 2b — DHCP client)
```

And change the `NOT vendored` line from `Internet/DHCP (slice 2b), loopback, ...` to `loopback, Application/, other PHY chips` (DHCP is now vendored).

- [ ] **Step 4: Add dhcp.c + its include dir to the `wiznet` CMake library**

In `fw/CMakeLists.txt`, in the `add_library(wiznet STATIC ...)` block, add the dhcp source after the w5500 line:

```cmake
add_library(wiznet STATIC
    ${WIZNET}/Ethernet/wizchip_conf.c
    ${WIZNET}/Ethernet/socket.c
    ${WIZNET}/Ethernet/W5500/w5500.c
    ${WIZNET}/Internet/DHCP/dhcp.c
)
```

and add the DHCP include dir to the `wiznet` SYSTEM include list:

```cmake
target_include_directories(wiznet SYSTEM PUBLIC
    ${WIZNET}/Ethernet
    ${WIZNET}/Ethernet/W5500
    ${WIZNET}/Internet/DHCP
)
```

(The existing `target_compile_options(wiznet PRIVATE -w)` already isolates its warnings — leave it.)

- [ ] **Step 5: Configure + build to prove the vendored DHCP compiles and our code stays 0-warning**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b2b1.log | tail -4
grep -iE 'warning:' /tmp/b2b1.log | grep -v '/vendor/' || echo "0 warnings in our code"
```

Expected: build succeeds; `0 warnings in our code`. (dhcp.c is unreferenced from our code yet → `--gc-sections` drops it from the ELF; that is fine. The goal of Task 1 is a green, isolated vendor build.)

- [ ] **Step 6: Commit**

```bash
git add fw/vendor/wiznet/Internet fw/vendor/wiznet/README.md fw/CMakeLists.txt
git commit -m "build(stage-c): vendor WIZnet ioLibrary DHCP client (slice 2b)"
```

---

## Task 2: `app_eth` DHCP lifecycle (init branch + callbacks + `app_eth_tick`)

> The meat of the slice. Modifies `app_eth.{c,h}` only. **Preserve the slice-2a static path verbatim** (the `comm_mode != ETH_DHCP` branch must be byte-identical to the current static netinfo block). No host test — HAL/socket layer; verified by build + HW.

**Files:**
- Modify: `fw/include/app_eth.h`, `fw/src/app_eth.c`

- [ ] **Step 1: Add the `app_eth_tick()` declaration to `fw/include/app_eth.h`**

Replace the entire current header with:

```c
/* fw/include/app_eth.h — W5500 bring-up over SPI1 (spec §4.3 / slice 2b DHCP).
 * Static IP from cfg(FRAM) or DHCP-acquired (comm_mode==ETH_DHCP); MAC =
 * hardcoded constant. Non-fatal: if the chip is absent or the link never comes
 * up, app_eth_init() returns false and the TCP path stays off — RTU / mon / LCD
 * are unaffected. */
#pragma once
#include <stdbool.h>

/* Bring up SPI1 + W5500. For static comm_mode, applies netinfo from cfg and
 * becomes available immediately. For DHCP comm_mode, starts the DHCP client and
 * stays unavailable until app_eth_tick() acquires a lease. Returns true if the
 * chip answered and the PHY linked within the timeout. Call once at boot, after
 * app_init() (needs cfg loaded). */
bool app_eth_init(void);

/* Drive the DHCP client (1s DHCP_time_handler + DHCP_run). No-op unless DHCP
 * mode started a lease. Call every superloop iteration (from app_loop_iter). */
void app_eth_tick(void);

/* True when ready to serve TCP: chip+link up AND IP ready (static: at init;
 * DHCP: after the lease is acquired). */
bool app_eth_available(void);
```

- [ ] **Step 2: Rewrite `fw/src/app_eth.c` with the DHCP lifecycle**

Replace the entire file with the following. The static netinfo block (inside `app_eth_init`, the `else` path) is byte-identical to the current slice-2a code — only the DHCP branch, the callbacks, the new includes/state, and `app_eth_tick()` are added:

```c
/* fw/src/app_eth.c — see app_eth.h. ioLibrary callback wiring mirrors samd20
 * ethernet.c, retargeted to STM32 SPI1 (spi1.c) + W5500 chip. Static IP from
 * cfg, or DHCP-acquired (comm_mode==ETH_DHCP) via the ioLibrary DHCP client
 * driven from app_eth_tick(). */
#include "app_eth.h"
#include "spi1.h"
#include "app_lcd.h"        /* app_lcd_cfg() -> comm_mode, ether_ip/nm/gw */
#include "app_config.h"
#include "mon.h"
#include "sys_tick.h"       /* sys_tick_get_ms() — 1s DHCP cadence */
#include "stm32f4xx_hal.h"  /* HAL_Delay */
#include "wizchip_conf.h"
#include "socket.h"
#include "dhcp.h"

#define COMM_ETH_DHCP  2u   /* cfg->comm_mode: 0=SERIAL 1=ETH_STATIC 2=ETH_DHCP */
#define SOCK_DHCP      1u   /* UDP socket for DHCP (TCP server uses socket 0) */

/* Faithful to samd20 ethernet.c default MAC (per-unit MAC = future, spec §8). */
static const uint8_t kEthMac[6] = { 0x00, 0x08, 0xdc, 0x78, 0x91, 0x71 };

static bool    s_available   = false;
static bool    s_dhcp_active  = false;   /* DHCP_init done → drive in app_eth_tick */
static uint8_t s_dhcp_buf[1024];         /* ioLibrary DHCP RX buffer (min 548) */

bool app_eth_available(void) { return s_available; }

/* ioLibrary CS callbacks (it wants void(*)(void)). */
static void cs_sel(void)   { spi1_cs_low();  }
static void cs_desel(void) { spi1_cs_high(); }

/* DHCP IP-assign + IP-update callback (same handler for both). Pull the leased
 * address from the client, apply it to the W5500, mirror it into the live cfg
 * for LCD display (RAM only — never committed to FRAM), and mark available. */
static void dhcp_ip_assign(void)
{
    wiz_NetInfo ni = {0};
    for (int i = 0; i < 6; i++) { ni.mac[i] = kEthMac[i]; }
    getIPfromDHCP(ni.ip);
    getGWfromDHCP(ni.gw);
    getSNfromDHCP(ni.sn);
    getDNSfromDHCP(ni.dns);
    ni.dhcp = NETINFO_DHCP;
    wizchip_setnetinfo(&ni);

    /* mirror into the live config so the LCD shows the leased IP (RAM only). */
    app_config_t *cfg = app_lcd_cfg();
    for (int i = 0; i < 4; i++) {
        cfg->ether_ip[i] = ni.ip[i];
        cfg->ether_nm[i] = ni.sn[i];
        cfg->ether_gw[i] = ni.gw[i];
    }

    s_available = true;
    mon_printf("[eth] dhcp lease ip=%u.%u.%u.%u\r\n",
               (unsigned)ni.ip[0], (unsigned)ni.ip[1],
               (unsigned)ni.ip[2], (unsigned)ni.ip[3]);
}

/* DHCP IP-conflict callback. NON-FATAL (samd20's while(1) halt is dropped):
 * drop availability and let the library DECLINE + re-request. */
static void dhcp_ip_conflict(void)
{
    s_available = false;
    mon_printf("[eth] dhcp ip conflict — re-requesting\r\n");
}

bool app_eth_init(void)
{
    s_available   = false;
    s_dhcp_active  = false;
    spi1_init();

    /* Register the ioLibrary SPI / CS callbacks.
     * CRITICAL: the byte callbacks (spi1_rb/spi1_wb) must NOT touch CS — the
     * ioLibrary holds CS low across the whole frame via cs_sel/cs_desel. */
    reg_wizchip_cs_cbfunc(cs_sel, cs_desel);
    reg_wizchip_spi_cbfunc(spi1_rb, spi1_wb);
    reg_wizchip_spiburst_cbfunc(spi1_burst_read, spi1_burst_write);

    spi1_eth_reset();

    /* socket buffer sizes: 2KB each. wizchip_init returns -1 if they don't fit. */
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

    const app_config_t *cfg = app_lcd_cfg();

    if (cfg->comm_mode == COMM_ETH_DHCP) {
        /* DHCP: start the client; the lease is acquired in app_eth_tick() and
         * applied by dhcp_ip_assign(). Stay unavailable until then. */
        DHCP_init(SOCK_DHCP, s_dhcp_buf);
        reg_dhcp_cbfunc(dhcp_ip_assign, dhcp_ip_assign, dhcp_ip_conflict);
        s_dhcp_active = true;
        mon_printf("[eth] dhcp init — acquiring lease...\r\n");
        return true;   /* chip+link up; s_available stays false until lease */
    }

    /* static netinfo from cfg(FRAM). */
    wiz_NetInfo ni = {0};
    for (int i = 0; i < 6; i++) { ni.mac[i] = kEthMac[i]; }
    for (int i = 0; i < 4; i++) {
        ni.ip[i]  = cfg->ether_ip[i];
        ni.sn[i]  = cfg->ether_nm[i];
        ni.gw[i]  = cfg->ether_gw[i];
        ni.dns[i] = 0u;
    }
    ni.dhcp = NETINFO_STATIC;
    wizchip_setnetinfo(&ni);

    mon_printf("[eth] up ip=%u.%u.%u.%u\r\n",
               (unsigned)ni.ip[0], (unsigned)ni.ip[1],
               (unsigned)ni.ip[2], (unsigned)ni.ip[3]);
    s_available = true;
    return true;
}

/* Drive the DHCP client (no-op unless comm_mode==ETH_DHCP started a lease).
 * Call every superloop iteration. DHCP_time_handler must tick at 1s during
 * acquisition AND lease (else DISCOVER never retransmits — spec §3.2). On
 * DHCP_FAILED we re-init to keep retrying (no static fallback — spec §3.3). */
void app_eth_tick(void)
{
    if (!s_dhcp_active) {
        return;
    }

    static uint32_t s_prev_ms = 0u;
    uint32_t now = sys_tick_get_ms();
    if ((uint32_t)(now - s_prev_ms) >= 1000u) {
        s_prev_ms = now;
        DHCP_time_handler();
    }

    switch (DHCP_run()) {       /* assign/conflict callbacks fire from here */
        case DHCP_FAILED:
            s_available = false;
            DHCP_init(SOCK_DHCP, s_dhcp_buf);   /* keep retrying — no fallback */
            break;
        default:
            break;              /* ASSIGN/CHANGED/LEASED handled in callbacks */
    }
}
```

- [ ] **Step 3: Build (0-warning on our code)**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b2b2.log | tail -4
grep -iE 'warning:' /tmp/b2b2.log | grep -v '/vendor/' || echo "0 warnings in our code"
```

Expected: build OK, `0 warnings in our code`. (`app_eth_tick` is still unreferenced from the superloop → `--gc-sections` may drop it; that is fine, it is wired in Task 3.)

If a compile error mentions an unknown DHCP symbol (`DHCP_init`, `DHCP_run`, `DHCP_time_handler`, `reg_dhcp_cbfunc`, `getIPfromDHCP`/`getGWfromDHCP`/`getSNfromDHCP`/`getDNSfromDHCP`, `DHCP_FAILED`, `NETINFO_DHCP`), the name differs in the vendored `dhcp.h` — STOP and report NEEDS_CONTEXT with the actual name from `grep -nE 'DHCP_|getIPfromDHCP|reg_dhcp' fw/vendor/wiznet/Internet/DHCP/dhcp.h`. Do not guess. (The controller pre-verifies these in Task 1 and supplies the confirmed names; this is a safety net.)

- [ ] **Step 4: Host tests still green (no regression to the pure layers)**

```bash
make -C fw/test test 2>&1 | grep -E 'PASSED|FAILED'
```

Expected: all three suites PASS (`app_reg_calc`, `app_modbus_core`, `app_modbus_tcp_frame`) — unchanged by this task.

- [ ] **Step 5: Commit**

```bash
git add fw/include/app_eth.h fw/src/app_eth.c
git commit -m "feat(stage-c): W5500 DHCP client lifecycle in app_eth (non-blocking acquire)"
```

---

## Task 3: Wire `app_eth_tick()` into the superloop (`app.c`)

> One-line integration. `app_eth_tick()` must run every iteration, BEFORE `app_modbus_tick()`, so a lease acquired this iteration flips `app_eth_available()` true before the Modbus gate reads it.

**Files:**
- Modify: `fw/src/app.c`

- [ ] **Step 1: Add the include**

In `fw/src/app.c`, add `#include "app_eth.h"` with the other app includes (after `#include "app_modbus.h"` at line 11):

```c
#include "app_modbus.h"
#include "app_eth.h"
```

- [ ] **Step 2: Call `app_eth_tick()` before `app_modbus_tick()` in `app_loop_iter`**

In `fw/src/app.c`, the current end of `app_loop_iter` is:

```c
    /* 4. Modbus slave — occupancy re-eval + one RTU frame per iter (spec §2).
     * After app_reg_tick so the mirror sees this iter's freshest measure. */
    app_modbus_tick();
}
```

Insert the eth tick just before it:

```c
    /* 4. Ethernet/DHCP — drive the W5500 DHCP client (no-op unless DHCP mode).
     * Before Modbus so a lease acquired this iter flips app_eth_available(). */
    app_eth_tick();

    /* 5. Modbus slave — occupancy re-eval + one RTU/TCP frame per iter (spec §2).
     * After app_reg_tick so the mirror sees this iter's freshest measure. */
    app_modbus_tick();
}
```

- [ ] **Step 3: Build (0-warning on our code) + size**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b2b3.log | tail -4
grep -iE 'warning:' /tmp/b2b3.log | grep -v '/vendor/' || echo "0 warnings in our code"
arm-none-eabi-size fw/build/gds_us_ctrl.elf
```

Expected: build OK, `0 warnings in our code`. Now `app_eth_tick` + the DHCP lib are reachable → the ELF grows (record text/data/bss). Verify FLASH stays well under 128 KB (131072 B).

- [ ] **Step 4: Host tests still green**

```bash
make -C fw/test test 2>&1 | grep -E 'PASSED|FAILED'
```

Expected: all three suites PASS.

- [ ] **Step 5: Commit**

```bash
git add fw/src/app.c
git commit -m "feat(stage-c): drive W5500 DHCP client from the superloop"
```

---

## Task 4: Build gate + final verification (host-complete)

> The slice is code-complete and host-verified here. HW E2E is Task 5 (board-gated).

- [ ] **Step 1: Clean reconfigure + build, assert 0-warning on our code**

```bash
cd /Users/tknoh/dev/work/gds_us_ctrl
rm -rf fw/build
env -u STM32_TOOLCHAIN cmake -S fw -B fw/build -G Ninja
env -u STM32_TOOLCHAIN cmake --build fw/build 2>&1 | tee /tmp/b2bfinal.log | tail -4
grep -iE 'warning:' /tmp/b2bfinal.log | grep -v '/vendor/' || echo "0 warnings in our code"
arm-none-eabi-size fw/build/gds_us_ctrl.elf
```

Expected: clean build, `0 warnings in our code`, size printed (record FLASH %/RAM % for the changelog).

- [ ] **Step 2: Full host test run**

```bash
make -C fw/test clean && make -C fw/test test 2>&1 | grep -E 'PASSED|FAILED'
```

Expected: `app_reg_calc … PASSED`, `app_modbus_core … PASSED`, `app_modbus_tcp_frame: all checks PASSED`.

- [ ] **Step 3: Request code review**

Run the integrated `cpp-reviewer` over the slice diff (`git diff feat/stage-c-modbus-tcp-static...HEAD`, our code only — exclude `fw/vendor/`). Address CRITICAL/HIGH; record MEDIUM/LOW dispositions. Pay attention to: the static path being preserved verbatim, the `app_eth_available()` semantics (false until lease in DHCP mode), the 1s `DHCP_time_handler` cadence + `DHCP_FAILED` re-init keep-retrying loop, the RAM-only cfg mirror (no FRAM commit), and that `app_eth_tick` runs before `app_modbus_tick`.

- [ ] **Step 4: Commit any review fixes, then update tracking docs**

Update `docs/changelog.md` (new entry: slice 2b DHCP code-complete, host PASS, size), and `docs/superpowers/RESUME.md` status block (slice 2b code-complete, next = HW E2E). Commit:

```bash
git add docs/changelog.md docs/superpowers/RESUME.md
git commit -m "docs(stage-c): slice 2b DHCP code-complete, host PASS — next HW E2E"
```

---

## Task 5: HW E2E (board + W5500 + DHCP-served network) — HW-GATED, board session

> Runs when the board + a populated/linked W5500 + a DHCP server (router/dnsmasq) are connected. No code expected; if a defect surfaces, fix on this branch then re-run.

**Setup:**
- Via LCD/FRAM set `comm_mode = ETH_DHCP` (2). Power-cycle.
- Connect the W5500 to a network with a DHCP server (home router or `dnsmasq`).

- [ ] **Step 1: Lease acquisition**
  Mon shows `[eth] dhcp init — acquiring lease...` then `[eth] dhcp lease ip=a.b.c.d` within a few seconds. Note the leased IP. (If no DHCP server: mon stays at "acquiring", `app_eth_available()` false — proves the keep-retrying non-fatal path; RTU/mon/LCD unaffected.)

- [ ] **Step 2: FC03 poll = live mirror**
  `IP=<leased>; ping $IP`. `mbpoll -m tcp -a 1 -t 4 -r 1 -c 30 -1 $IP` → dump of 0x00..0x1D equals the boot `[cfg]`/`set_pot` values (same as slice-2a item). STATUS (0x1D) = 0 when idle.

- [ ] **Step 3: FC06 write + clamp + FRAM persistence**
  Write OUT_POWER=80 (`mbpoll -m tcp -a 1 -t 4 -r 7 $IP 80`), read back 80. Write 30 → reads 50 (clamp), 120 → 100. `openocd … reset`, power on, read back → clamped value persists (FRAM).

- [ ] **Step 4: START → ceiling → STOP + ICON_RUN**
  Write START (0x1B=1) → STATUS bit0=1, 560 ms on-time ceiling auto-stops, ICON_RUN visible on the LCD; write STOP (0x1C=1). Repeat ×3.

- [ ] **Step 5: LCD shows the leased IP**
  On the LCD ethernet page, confirm the displayed IP matches the DHCP-leased `$IP` (the RAM cfg mirror). Confirm it is NOT persisted: power-cycle with the network still up → re-leases and re-displays (FRAM ether stays 0; a static-mode boot would show 0.0.0.0 / configured static, not the old lease).

- [ ] **Step 6: RTU regression spot-check (advisor #1 guard from slice 2a)**
  Switch back to SERIAL, `mbpoll -a 1 -b <baud> -P <parity> -t 4 -r 7 $DEV 77` → reads back 77, persists — confirms slice 2a's shared apply-path still works after the 2b changes.

- [ ] **Step 7: Record + finish**
  Log the matrix in `docs/changelog.md`. Restore OUT_POWER to its bench default. Per `superpowers:finishing-a-development-branch`: merge to main (`--no-ff`) + tag `hw-revA_fw-stage-c2b` (after slice 2a is merged — 2a→2b order), post-merge build 0-warning + host PASS.

---

## Self-Review Notes (author)

- **Spec coverage:** §1.1 scope (DHCP-only) → no hot-plug/serial-skip tasks (correct, deferred). §2 modules (vendor dhcp / app_eth modify / app.c wire) → Tasks 1/2/3. §2.1 available() extension → Task 2 (init DHCP branch leaves false; assign sets true). §3 lifecycle (init branch + tick + callbacks) → Task 2. §3.1 socket 1 + 1KB buf → Task 2 (`SOCK_DHCP=1`, `s_dhcp_buf[1024]`). §3.2 1s tick always → Task 2 `app_eth_tick` (DHCP_time_handler at 1s unconditionally while active). §3.3 conflict non-fatal + no static fallback → Task 2 (`dhcp_ip_conflict` no halt; `DHCP_FAILED` re-init, not static). §4 RAM-only mirror → Task 2 (`dhcp_ip_assign` writes cfg, no FRAM commit). §5 gate unchanged → no app_modbus change (verified: gate already `comm_mode!=SERIAL && app_eth_available()`). §6 no host test + HW matrix → Tasks 4/5. §8 unresolved (includes, API names, socket #, 1s gate, mirror safety) → resolved concretely in Tasks 1/2 + controller pre-verify. §9 deferred → not in plan (correct).
- **Type/name consistency:** `app_eth_tick`, `app_eth_available`, `app_eth_init`, `dhcp_ip_assign`, `dhcp_ip_conflict`, `s_dhcp_active`, `s_dhcp_buf`, `SOCK_DHCP`, `COMM_ETH_DHCP` — used identically across Tasks 2/3. The static-path block in Task 2's `app_eth_init` else-branch is byte-identical to the committed slice-2a code.
- **Watch at impl time (controller, after Task 1):** grep the vendored `dhcp.h` and confirm the exact spellings of `DHCP_init`/`DHCP_run`/`DHCP_time_handler`/`reg_dhcp_cbfunc`/`getIPfromDHCP`/`getGWfromDHCP`/`getSNfromDHCP`/`getDNSfromDHCP`/`DHCP_FAILED`; if any differ from Task 2's code, fix Task 2 before dispatching it. `NETINFO_DHCP` is from the already-vendored `wizchip_conf.h` (W5500-active block — confirmed in slice 2a).
