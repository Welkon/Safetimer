# SafeTimer Architecture Traps & Fixes

**Version:** 1.3.2
**Last Updated:** 2025-12-19

This document catalogs all known architectural traps in SafeTimer and their fixes.

---

## 🔴 Critical - Code Fixes Applied

### Risk #0: Heavy Division in Critical Section
**Status:** ✅ Fixed in v1.3.3
**Severity:** High (8-bit MCU)

**Problem:**
32-bit division in critical section (`missed_periods = lag / period`) takes 50-150μs on 8-bit MCUs without hardware divider. Unacceptable interrupt latency for hard real-time systems.

**Fix:**
- Moved division outside critical section
- Snapshot values, release lock, calculate, re-acquire lock
- Added verification to prevent Stop-Start overwrite race (v1.3.4)
- **Impact:** Interrupt latency reduced from 150μs to <10μs

**Code Location:** `src/safetimer.c:815-858`, `src/safetimer.c:479-498`

---

### Risk #21: Stop-Start Overwrite Race
**Status:** ✅ Fixed in v1.3.4, Enhanced in v1.3.5
**Severity:** Low (rare scenario)

**Problem:**
When division moved outside critical section (v1.3.3 optimization), introduced new race:
1. Main loop snapshots `expire_time`, releases lock
2. ISR calls `stop()` then `start()`, sets new `expire_time`
3. Main loop re-acquires lock, overwrites with old calculated value

**Fix (v1.3.4):**
- Verify `expire_time` unchanged before updating
- If ISR modified it, discard calculation and keep ISR's value

**Enhancement (v1.3.5):**
- Added `active` state verification (double-check)
- Prevents ABA variant where ISR results in same `expire_time` value
- **Impact:** Prevents ISR timer corruption even in extreme coincidence

**Code Location:** `src/safetimer.c:885-895`, `src/safetimer.c:503-511`

---

### Risk #22: Recursive Stack Overflow (Runtime Guard)
**Status:** ✅ Fixed in v1.3.5
**Severity:** High (8-bit MCU)

**Problem:**
If callback erroneously calls `safetimer_process()` again, causes recursive stack overflow. On 8-bit MCUs with ~176B RAM (e.g., SC8F072), even 2-3 recursions exhaust stack and cause system reset.

**Fix:**
- Added runtime recursion guard (`s_processing` flag)
- Silently returns if already processing
- **RAM Cost:** +1 byte
- **Impact:** Prevents stack overflow, system remains stable

**Code Location:** `src/safetimer.c:91`, `src/safetimer.c:543-547`, `src/safetimer.c:608`

---

### Trap #1: ABA Handle Reuse
**Status:** ✅ Fixed in v1.3.2, Enhanced in v1.4.0
**Severity:** Critical

**Problem:**
Documentation claimed "algebraic counter" prevents reuse, but code only used array index (0~N). After deleting handle 0, new timer may immediately reuse handle 0.

**Fix (v1.3.2):**
- Added 3-bit generation counter (1~7) to `timer_slot_t`
- Handle encoding: `[generation:3bit][index:5bit]`
- `validate_handle()` checks both index and generation
- **RAM Cost:** +9 bytes (MAX_TIMERS=8)

**Enhancement (v1.4.0):**
- Added collision detection loop in `safetimer_create()`
- Prevents `SAFETIMER_INVALID_HANDLE` (-1) collision edge case
- When generation/index combination produces -1, advances generation counter
- **Impact:** Guarantees valid handles on all architectures (including int8_t)

**Code Location:** `src/safetimer.c:82-98`, `src/safetimer.c:257-269`, `src/safetimer.c:675-699`

---

### Trap #2: Catch-up Loop Blocking
**Status:** ✅ Fixed in v1.3.2
**Severity:** High

**Problem:**
If system stalls and time lags severely, `safetimer_process()` executes `while` loop in critical section (interrupts disabled), potentially causing watchdog timeout.

**Fix:**
- Replaced loop with mathematical calculation
- `missed_periods = lag / period`
- **RAM Cost:** 0 bytes

**Code Location:** `src/safetimer.c:710-724`

---

### Trap #6: Stop Race Condition (TOCTOU)
**Status:** ✅ Fixed in v1.3.2
**Severity:** High

**Problem:**
Main loop decides to trigger callback, releases critical section, ISR calls `stop()`, main loop still executes callback on stopped timer.

**Fix:**
- Double-check `active` state before callback execution
- **RAM Cost:** 0 bytes

**Code Location:** `src/safetimer.c:530-539`

---

### Trap #10: Semaphore Overflow
**Status:** ✅ Fixed in v1.3.2
**Severity:** High

**Problem:**
`safetimer_sem_t` is `int8_t`. `WAIT_SEM` timeout_count > 126 causes overflow, wraps to -1, immediate timeout.

**Fix:**
- Added `_Static_assert(timeout_count <= 126)` compile-time check
- **RAM Cost:** 0 bytes

**Code Location:** `include/safetimer_sem.h:160-164`

---

### Trap #13: 16-bit Mode Truncation
**Status:** ✅ Fixed in v1.3.2
**Severity:** High

**Problem:**
Enabling `BSP_TICK_TYPE_16BIT` limits max period to 65.5s. Setting >65535ms silently truncates, causes premature trigger.

**Fix:**
- Added runtime check in `safetimer_create()`, `safetimer_set_period()`, `safetimer_advance_period()`
- Returns `TIMER_ERR_INVALID` if period > 65535ms in 16-bit mode
- **RAM Cost:** 0 bytes

**Code Location:** `src/safetimer.c:170-175`, `src/safetimer.c:347-352`, `src/safetimer.c:434-439`

---

## 🟠 High - Documentation Strengthened

### Trap #3: Default Init Trap
**Status:** ✅ Documented in v1.3.2
**Severity:** Medium

**Problem:**
C defaults static variables to 0, which is a valid handle in SafeTimer. Uninitialized handle variables may mistakenly point to first timer.

**Fix:**
- Added `@warning` in `safetimer.h` requiring explicit initialization
- **Code Location:** `include/safetimer.h:38-45`

---

### Trap #5: Implicit Priority Inversion
**Status:** ✅ Documented in v1.3.2
**Severity:** Medium

**Problem:**
`safetimer_process()` iterates 0→N. Index-small timer callbacks block index-large timers.

**Fix:**
- Documented callback design guideline: < 100us execution time
- **Code Location:** `include/safetimer.h:81-100`

---

### Trap #7: ISR Context Misuse
**Status:** ✅ Documented in v1.3.2
**Severity:** Medium

**Problem:**
Calling `safetimer_process()` in ISR causes all callbacks to execute in interrupt context, blocks system.

**Fix:**
- Strengthened `@warning` in `safetimer_process()` documentation
- **Code Location:** `include/safetimer.h:287-310`

---

### Trap #9: Variable Amnesia (Coroutine)
**Status:** ✅ Documented in v1.3.0
**Severity:** High

**Problem:**
Coroutines based on Protothread. `YIELD/SLEEP` causes function return, stack local variables destroyed.

**Fix:**
- Documented in coroutine tutorial: use `static` or context struct
- **Code Location:** `tutorials/coroutines.md`

---

### Trap #11: Deadlock Risk (Forever Wait)
**Status:** ✅ Documented in v1.3.0
**Severity:** Medium

**Problem:**
`WAIT_SEM_FOREVER` waits indefinitely. If signal source fails, coroutine permanently hangs.

**Fix:**
- Documented: production code should always use timeout-based `WAIT_SEM`
- **Code Location:** `include/safetimer_sem.h:184-200`

---

### Trap #12: Struct Alignment Trap
**Status:** ✅ Documented in v1.3.0
**Severity:** High

**Problem:**
`SAFETIMER_CORO_CONTEXT` macro must be first member in user struct. Wrong placement causes pointer cast error, memory corruption.

**Fix:**
- Documented in coroutine tutorial
- **Code Location:** `tutorials/coroutines.md`

---

### Trap #13: Switch Nesting Conflict
**Status:** ✅ Documented in v1.3.0
**Severity:** Medium

**Problem:**
Coroutine macros use `switch/case` internally (Duff's Device). User wrapping coroutine in `switch` breaks jump logic.

**Fix:**
- Documented: prohibit `switch` outside coroutine macros
- **Code Location:** `include/safetimer_coro.h`

---

### Trap #14: One-Shot Mode Death
**Status:** ✅ Documented in v1.3.0
**Severity:** High

**Problem:**
Coroutine timer set to `ONE_SHOT` stops after first `SLEEP`, coroutine never wakes again.

**Fix:**
- Documented: coroutine timers MUST use `TIMER_MODE_REPEAT`
- **Code Location:** `tutorials/coroutines.md`

---

### Trap #15: Keep-Alive Starvation
**Status:** ✅ Documented in v1.3.2
**Severity:** Medium

**Problem:**
Repeatedly calling `safetimer_start()` in loop resets countdown, timer never expires.

**Fix:**
- Added `@warning` in `safetimer_start()` documentation with examples
- **Code Location:** `include/safetimer.h:149-169`

---

### Trap #16: User Data Dangling Pointer
**Status:** ✅ Documented in v1.3.2
**Severity:** High

**Problem:**
`user_data` points to stack local variable. Callback executes after variable destroyed, causes crash.

**Fix:**
- Added `@warning` in callback typedef documentation
- **Code Location:** `include/safetimer.h:81-100`

---

### Trap #17: Batch Create Silent Failure
**Status:** ✅ Documented in v1.3.0
**Severity:** Medium

**Problem:**
`create_started_batch()` returns success count. If pool full, partial failure leaves invalid handles, direct use causes crash.

**Fix:**
- Documented: always check return value equals requested count
- **Code Location:** `include/safetimer_helpers.h`

---

### Trap #18: Period Change Phase Jump
**Status:** ✅ Documented in v1.2.4
**Severity:** Low

**Problem:**
`set_period()` resets phase (restarts from current tick). Causes jitter in waveform generation.

**Fix:**
- Use `advance_period()` for phase-locked period changes
- **Code Location:** `CHANGELOG.md`, `include/safetimer.h`

---

### Trap #19: Recursive Stack Overflow
**Status:** ✅ Documented in v1.3.2
**Severity:** Critical

**Problem:**
Calling `safetimer_process()` from timer callback causes recursive stack overflow, immediate crash.

**Fix:**
- Added `@warning` in callback typedef and `safetimer_process()` documentation
- **Code Location:** `include/safetimer.h:81-100`, `include/safetimer.h:293-295`

---

## 🟢 Low - BSP Layer Responsibility

### Trap #4: Sleep Time Freeze
**Status:** ✅ Documented in v1.3.2
**Severity:** Low

**Problem:**
MCU enters low-power mode, hardware timer stops. SafeTimer thinks time hasn't changed, tasks lag after wakeup.

**Fix:**
- BSP layer must compensate sleep time in `bsp_get_ticks()`
- **Code Location:** `tutorials/architecture-notes.md:337-373`

---

### Trap #8: BSP Atomicity Violation
**Status:** ✅ Documented in v1.3.2
**Severity:** Low

**Problem:**
On 8-bit MCU, reading 16/32-bit tick counter without critical section may read corrupted timestamp (e.g., 0x00FF → 0x0100 transition reads 0x0000).

**Fix:**
- BSP layer must protect multi-byte reads with `bsp_enter_critical()`
- **Code Location:** `tutorials/bsp-porting.md`

---

### Trap #24: Brute Force Authentication Retry (Anti-DoS Pattern)
**Status:** ✅ Solved in v1.4.0 (Coroutine Pattern)
**Severity:** Medium (Security)

**Problem:**
Traditional blocking authentication loops allow unlimited retries:
```c
while (1) {
    send_challenge();
    wait_for_response();  /* BLOCKS SYSTEM */
    if (verify()) break;
    /* No retry limit - attacker can brute force */
}
```

**Issues:**
- No timeout enforcement → System freezes if response never arrives
- No retry limit → Vulnerable to brute force attacks
- No backoff → High CPU usage during attack
- Global state machine required → Complex code

**Solution (Coroutine Pattern):**
```c
typedef struct {
    SAFETIMER_CORO_CONTEXT;
    uint8_t retries;
    uint32_t lockout_duration;
} auth_ctx_t;

void auth_task(void *user_data) {
    auth_ctx_t *ctx = (auth_ctx_t *)user_data;
    SAFETIMER_CORO_BEGIN(ctx);

    while (1) {
        send_challenge();

        /* Non-blocking timeout (5 seconds) */
        SAFETIMER_CORO_WAIT_UNTIL(has_response() || timeout(), 50);

        if (!has_response() || !verify()) {
            ctx->retries++;
            if (ctx->retries >= 3) {
                /* Hard lockout after 3 failures */
                SAFETIMER_CORO_SLEEP(10000);
                ctx->retries = 0;
            } else {
                /* Exponential backoff: 1s, 2s, 3s */
                SAFETIMER_CORO_SLEEP(1000 * ctx->retries);
            }
        } else {
            ctx->retries = 0;  /* Success - reset */
            SAFETIMER_CORO_SLEEP(10000);
        }
    }

    SAFETIMER_CORO_END();
}
```

**Benefits:**
- ✅ Non-blocking: System remains responsive
- ✅ Timeout enforcement: Automatically counts timeout as failure
- ✅ Linear backoff: Progressive delays (1s → 2s → 3s)
- ✅ Hard lockout: 10-second freeze after 3 failures
- ✅ State persistence: Retry counter survives across function calls
- ✅ No global state: Context embedded in coroutine structure

**Security Recommendations:**
- Use cryptographically secure random (CSPRNG, not LCG)
- Implement real signature verification (HMAC/RSA/ECC)
- Add replay attack prevention (nonce tracking)
- Consider persistent lockout state (survives power cycle)

**Code Location:** `examples/coroutine_demo/example_coroutine.c:184-281`

---

## 📊 Summary

| Category | Count | RAM Cost | Performance Impact |
|----------|-------|----------|-------------------|
| Code Fixes | 9 | +10 bytes | Interrupt latency: 150μs → <10μs |
| Documentation | 14 | 0 bytes | - |
| **Total** | **23** | **+10 bytes** | **Significantly improved** |

**All traps fixed or documented in v1.3.5.**

### Key Improvements in v1.3.3-v1.3.5
- ✅ **Critical Section Optimization:** Division moved outside locks (v1.3.3)
- ✅ **Interrupt Latency:** Reduced from 150μs to <10μs (8-bit MCU)
- ✅ **Race Condition Fix:** Stop-Start overwrite prevention (v1.3.4)
- ✅ **ABA Variant Fix:** Double verification with active state (v1.3.5)
- ✅ **Recursion Guard:** Runtime stack overflow prevention (v1.3.5)
- ✅ **Hard Real-Time Safe:** Suitable for high-frequency PWM and time-critical tasks

---

## 📖 See Also

- [Architecture Notes](architecture-notes.md) - Core design decisions
- [Coroutines Tutorial](coroutines.md) - Coroutine usage guide
- [BSP Porting Guide](bsp-porting.md) - BSP implementation requirements
- [CHANGELOG.md](../CHANGELOG.md) - Version history
