# Codex Handoff — Execute the Stage B implementation plan

> Paste this whole file as the task prompt for Codex (or run Codex in this worktree and say:
> "Read docs/superpowers/handoff-codex.md and follow it."). The plan it points to is final and reviewed.

---

You are implementing a finalized, reviewed plan. Follow it exactly — do not redesign or expand scope.

## PROJECT CONTEXT
- STM32F410RBT firmware (GD-SONIC ultrasonic controller). C11 + STM32F4 HAL, CMake + Ninja,
  arm-none-eabi-gcc. Superloop, no RTOS.
- Stage B goal: on boot, load config from the on-board FM24C16B FRAM (I2C1) and drive the DGUS LCD
  through a LOGO -> RUN transition with model string / icons / numeric fields pre-filled
  (port of samd20 init_lcd_mode).

## WORKING LOCATION (do not deviate)
- Work ONLY in the git worktree:  /Users/tknoh/dev/work/gds_us_ctrl-stageB
- Branch (already checked out):    feat/stage-b-lcd-app
- Do NOT touch the main repo at    /Users/tknoh/dev/work/gds_us_ctrl
- Before starting, confirm:
    git -C /Users/tknoh/dev/work/gds_us_ctrl-stageB rev-parse --abbrev-ref HEAD   # -> feat/stage-b-lcd-app
    git -C /Users/tknoh/dev/work/gds_us_ctrl-stageB status                        # -> clean

## READ FIRST (authoritative)
1. docs/superpowers/plans/2026-05-25-stage-b-lcd-app.md   <- THE PLAN. Execute its 8 tasks in order.
2. docs/superpowers/specs/2026-05-25-stage-b-lcd-app-design.md   <- design rationale.
3. CLAUDE.md (root)   <- project conventions.
4. ref/samd20/main.c, ref/samd20/define.h, ref/samd20/dgus_lcd.h   <- port source (READ-ONLY).
5. fw/include/dgus_lcd.h, fw/drivers/usart1.c   <- Stage A patterns being mirrored.

## HOW TO EXECUTE
- Do the plan's tasks 1..8 in order. Each task gives exact file paths, COMPLETE code, a build step, and
  a commit. Implement each step's code EXACTLY as written.
- One commit per task, using the commit message written in that task's commit step. Conventional commits.
  (Task 3 is a single commit covering all of its sub-steps.)
- Do NOT add any "Co-Authored-By" or "Generated with" trailer to commits (project policy).

## BUILD GATE (the per-task verification — there is NO host test framework)
- After each task's code changes:
    cd /Users/tknoh/dev/work/gds_us_ctrl-stageB/fw && \
      env -u STM32_TOOLCHAIN cmake -B build -G Ninja && \
      env -u STM32_TOOLCHAIN cmake --build build
- PASS = build succeeds with ZERO warnings (flags -Wall -Wextra -Wundef -Wshadow). A warning is a failure.
- `env -u STM32_TOOLCHAIN` is REQUIRED: that env var is stale and breaks cmake; unset it for the command.
  arm-none-eabi-gcc / cmake / ninja are on PATH (Homebrew).
- CMake globs src/*.c and drivers/*.c, so new files require the `cmake -B build` re-configure shown above.
- Do NOT set up pytest/gtest/any test framework. Build-green + code-matches-plan IS the gate.
  Ignore clangd/LSP noise.

## CRITICAL GOTCHA (Task 3)
- Task 3 must enable HAL_I2C_MODULE_ENABLED in fw/vendor/Core/Inc/stm32f4xx_hal_conf.h (line 57, uncomment).
- That path is under vendor/ (which CLAUDE.md marks read-only), BUT this specific file is the
  PROJECT-OWNED HAL CONFIG — the same file that already enables UART/TIM/GPIO/RCC. Enabling I2C there is
  REQUIRED and explicitly sanctioned by the plan. Without it, I2C_HandleTypeDef and HAL_I2C_* do not compile.
- Do NOT edit any other vendor/ file (HAL Driver Src/Inc library code stays read-only).
- Within Task 3, do NOT build between steps 1-4 (periph.h references I2C_HandleTypeDef, which only exists
  after step 4). The first valid build is step 5.

## SCOPE GUARDRAILS (do NOT do these)
- Do NOT implement SETUP pages, comm/ethernet LCD pages, DISP_STD_DATA string formatting
  (time2str/energy2str), or I2C_POT writes — explicitly out of scope.
- Do NOT add HW flashing/probe steps. The board is unavailable; the plan's "Deferred to a follow-up HW task"
  section is NOT for you.
- Do NOT regenerate docs/codemaps. Do NOT modify the spec or plan files.

## IF REALITY DIVERGES FROM THE PLAN
- If a build fails or a symbol is missing despite following the plan, make the MINIMAL fix to reach a green
  build and note the deviation in that task's commit body. Do not redesign or broaden scope.
- If a step is genuinely ambiguous or impossible, STOP and report rather than guessing.

## WHEN DONE (after Task 8)
- Report: `git log --oneline 00f9ec3..HEAD`, the final `arm-none-eabi-size fw/build/gds_us_ctrl.elf` output,
  and any deviations from the plan.
- Do NOT merge to main and do NOT push. Leave the branch for review.
