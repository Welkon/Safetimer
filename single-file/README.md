# SafeTimer 单文件版本 - 完整使用指南

## 📦 什么是单文件版本？

SafeTimer 单文件版本是一个精简、易集成的定时器库，只需 **3 个文件** 即可使用：

| 文件 | 说明 | 是否必需 |
|------|------|---------|
| `safetimer_single.h` | API 接口 + 类型定义 | ✅ 必需 |
| `safetimer_single.c` | SafeTimer 核心实现 | ✅ 必需 |
| `safetimer_bsp_example.c` | BSP 参考实现 | ⚠️ 需适配 |

---

## 🎯 使用场景

### ✅ 推荐使用单文件版本：
- 快速原型开发
- 简单项目（1-2个模块）
- 不想管理多个文件
- 学习和实验
- **8051/STM8/AVR 等资源受限的 MCU**

### ⚠️ 推荐使用标准版本（多文件）：
- 生产项目
- 团队协作
- 需要定制配置
- 大型项目

---

## 🚀 快速开始（5分钟集成）

### 步骤 1：复制文件到项目

```bash
cp single-file/safetimer_single.h your_project/
cp single-file/safetimer_single.c your_project/
cp single-file/safetimer_bsp_example.c your_project/
```

### 步骤 2：适配 BSP（关键！）

打开 `safetimer_bsp_example.c`，根据您的平台修改：

#### 2.1 包含平台头文件

```c
#include "safetimer_single.h"
#include <8051.h>  /* ← 改为您的平台头文件 */
```

#### 2.2 配置硬件定时器（1ms 中断）

**🎯 定时器模式要求（必须！）**

SafeTimer 要求硬件定时器必须工作在 **定时模式（Timer Mode）**：

| 要求 | 说明 |
|------|------|
| **✅ 必须** | 定时模式（Timer Mode）- 使用稳定时钟源 |
| **❌ 禁止** | 计数模式（Counter Mode）- 外部引脚事件计数 |
| **时钟源** | 内部时钟（Fosc/HSI/LSI）或外部晶振（稳定） |
| **周期精度** | 必须精确 1ms（允许 ±5% 误差） |

**配置要点**：
- 使用**定时模式**（基于时钟周期计数，非外部事件计数）
- 时钟源必须稳定可靠（内部 RC 或外部晶振均可）
- 确保定时器溢出周期精确为 1ms

---

在 BSP 文件底部有多个平台的示例代码（`#if 0` 区域），取消注释并适配：

```c
/* 示例：8051 Timer0 配置（定时模式）*/
void init_timer0(void) {
    TMOD = 0x01;  /* Timer0, Mode 1 */
    TH0 = 0xFC;   /* 1ms @ 11.0592MHz */
    TL0 = 0x18;
    ET0 = 1;      /* Enable interrupt */
    EA = 1;       /* Enable global */
    TR0 = 1;      /* Start timer */
}

void timer0_isr(void) __interrupt(1) {
    TH0 = 0xFC;   /* Reload */
    TL0 = 0x18;
    timer_interrupt_handler();  /* ← 调用 SafeTimer 的处理函数 */
}
```

**⚠️ 关键：** 确保硬件定时器每 1ms 调用一次 `timer_interrupt_handler()`。

#### 2.3 理解原子保护（关键概念）

当前 BSP 已经实现了**原子保护机制**，这对于 8-bit MCU 读取 32-bit 数据至关重要。

**什么是原子保护？**

在 8-bit MCU 上，读取 32-bit 变量需要 4 次 8-bit 读取操作。如果在读取过程中发生中断并修改了这个变量，就会导致**数据撕裂**（读取到新旧数据的混合值）。

**问题示例**：
```c
/* 假设 s_system_ticks = 0x000000FF (255) */
uint32_t tick = s_system_ticks;  /* 开始读取 */
/* 读取第 1 个字节: 0xFF */
/* ⚠️ 中断发生！s_system_ticks 变为 0x00000100 (256) */
/* 读取第 2-4 个字节: 0x00, 0x00, 0x00 */
/* 结果：tick = 0x000000FF (255)，而实际值已是 256 */
```

**解决方案：原子读取**

BSP 中的 `bsp_get_ticks()` 使用临界区保护：
```c
bsp_tick_t bsp_get_ticks(void) {
    bsp_tick_t ticks;
    uint8_t saved_ea = EA ? 1U : 0U;  /* 保存中断状态 */
    EA = 0;                            /* 禁用中断 */
    ticks = s_system_ticks;            /* 原子读取（无中断干扰）*/
    if (saved_ea) EA = 1;              /* 恢复中断状态 */
    return ticks;
}
```

**三重保护机制**：

1. **原子读取保护**：确保多字节数据读取的完整性
2. **临界区嵌套支持**：允许在 ISR 中安全调用 SafeTimer API
3. **中断状态恢复**：不破坏调用者的临界区状态

**⚠️ 不要修改以下函数的核心逻辑**：
- `bsp_get_ticks()` - 已实现原子读取
- `bsp_enter_critical()` / `bsp_exit_critical()` - 已实现嵌套保护

**只需修改平台相关的部分：**
```c
/* 根据您的平台修改这里 */
EA = 0;  /* ← 改为您的平台的禁用中断方式 */
EA = 1;  /* ← 改为您的平台的启用中断方式 */
```

### 步骤 3：在主程序中使用

```c
#include "safetimer_single.h"

/* 声明硬件初始化函数（避免编译警告） */
extern void init_timer0(void);

/* 定时器回调函数 */
void led_callback(void *user_data) {
    int led_id = *(int*)user_data;
    toggle_led(led_id);  /* 翻转 LED */
}

int main(void) {
    int led1 = 1, led2 = 2;

    /* 1. 初始化硬件定时器（1ms 中断） */
    init_timer0();  /* ← 您在 BSP 中实现的函数 */

    /* 2. 创建定时器 */
    safetimer_handle_t timer1 = safetimer_create(
        500,                    /* 500ms 周期 */
        TIMER_MODE_REPEAT,      /* 重复模式 */
        led_callback,           /* 回调函数 */
        &led1                   /* 用户数据 */
    );

    safetimer_handle_t timer2 = safetimer_create(
        1000,                   /* 1000ms 周期 */
        TIMER_MODE_REPEAT,
        led_callback,
        &led2
    );

    /* 3. 启动定时器 */
    safetimer_start(timer1);
    safetimer_start(timer2);

    /* 4. 主循环 */
    while (1) {
        safetimer_process();  /* ← 必须定期调用！ */

        /* 您的应用代码 */
    }

    return 0;
}
```

---

## 🔧 编译优化建议

**推荐优化级别**：
```bash
# 调试阶段：无优化，生成调试符号
-O0 -g

# 生产阶段：优化代码大小（推荐用于资源受限MCU）
-Os
```

---

## ⚙️ 配置选项

直接编辑 `safetimer_single.h` 头部的宏定义：

```c
/* ========================================================================== */
/*                         CONFIGURATION SECTION                              */
/* ========================================================================== */

/**
 * @brief Maximum number of concurrent timers
 * Range: 1-8 (default: 1 for ultra-low memory)
 * RAM Impact: ~14 bytes per timer + 2 bytes overhead
 *
 * Current default (1) is optimized for SC8F072 (160 bytes RAM).
 * For platforms with more RAM, increase to 2-8 as needed.
 */
#ifndef MAX_TIMERS
#define MAX_TIMERS 1  /* ← 修改这里：1-8 */
#endif

/**
 * @brief Enable parameter checking in public APIs
 * 0 = Disabled (faster, saves ~60 bytes ROM), 1 = Enabled (safer)
 *
 * ⚠️ 注意：此选项仅影响 Program space (ROM/Flash)，不影响 RAM
 * Default: 1 (enabled) for safety. Disable in production if ROM is critical.
 */
#ifndef ENABLE_PARAM_CHECK
#define ENABLE_PARAM_CHECK 1  /* ← 修改这里：0 或 1（默认开启）*/
#endif

/**
 * @brief Use 16-bit tick type (saves RAM but limits max period)
 * 1 = uint16_t (saves 8 bytes RAM, max 65.5s), 0 = uint32_t (max 49.7 days)
 *
 * ⚠️ 注意：此选项影响 RAM 占用
 * Current default (1) saves RAM on ultra-low memory platforms.
 * Set to 0 if you need timer periods longer than 65 seconds.
 */
#ifndef BSP_TICK_TYPE_16BIT
#define BSP_TICK_TYPE_16BIT 1  /* ← 修改这里：0 或 1 */
#endif

/**
 * @brief Use stdint.h for integer types
 * 0 = Custom typedefs (C89), 1 = stdint.h (C99)
 *
 * ⚠️ 注意：此选项不影响 RAM 或 ROM 大小，仅改变类型定义方式
 */
#ifndef USE_STDINT_H
#define USE_STDINT_H 0  /* ← 修改这里：0 或 1 */
#endif
```

---

## ⚡ 极限资源优化（RAM < 200 bytes）

### 🎯 优化目标

适用于极度资源受限的 MCU，如 SC8F072（160 bytes RAM）。

### 📦 优化配置清单

在 `safetimer_single.h` 中进行以下配置：

```c
/* 1. 最小化定时器数量（影响 RAM）*/
#define MAX_TIMERS 1              /* 每个定时器约 14 bytes RAM */

/* 2. 使用 16-bit tick（影响 RAM，节省 8 bytes）*/
#define BSP_TICK_TYPE_16BIT 1     /* 最大支持 65.5 秒定时周期 */

/* 3. 参数检查（可选：仅影响 ROM，不影响 RAM）*/
#define ENABLE_PARAM_CHECK 1      /* 默认开启，节省 ROM 可设为 0 */

/* 4. 使用 C89 类型定义（不影响 RAM 和 ROM）*/
#define USE_STDINT_H 0            /* 兼容性最好 */
```

**⚠️ RAM 占用说明**：
- **影响 RAM 的配置**：
  - `MAX_TIMERS`：定时器数量（每个约 14 bytes）
  - `BSP_TICK_TYPE_16BIT`：tick 位宽（16-bit 节省 8 bytes）
- **不影响 RAM 的配置**：
  - `ENABLE_PARAM_CHECK`：仅影响 ROM 代码大小
  - `USE_STDINT_H`：仅改变类型定义方式

**⚠️ ROM 优化说明**：
- **影响 ROM 的主要因素**：
  - `MAX_TIMERS`：定时器数量（影响循环和逻辑复杂度）
  - `BSP_TICK_TYPE_16BIT`：tick 位宽（16-bit 减少运算指令）
- **次要影响**：
  - `ENABLE_PARAM_CHECK`：参数检查（约 50-80 bytes）

**推荐配置**：保持 ENABLE_PARAM_CHECK=1（默认），RAM/ROM 优化通过调整 MAX_TIMERS 和 BSP_TICK_TYPE_16BIT

### 📊 内存占用对比

| 配置 | RAM 占用 | 占用率 (160B) | 最大周期 | 定时器数 |
|------|---------|--------------|---------|---------|
| **标准配置** | ~81 bytes | 50.6% | 49.7 天 | 4 |
| **优化配置** | ~44 bytes | 27.5% ✅ | 65.5 秒 | 1 |
| **回退方案** | ~58 bytes | 36.3% ✅ | 49.7 天 | 1 |

### ⚠️ 优化配置的限制

#### 1. **16-bit tick 的限制**

```c
bsp_tick_t = uint16_t (0 - 65535 ms)

✅ 可以：
  safetimer_create(1000, ...);    // 1 秒
  safetimer_create(30000, ...);   // 30 秒
  safetimer_create(65535, ...);   // 65.5 秒 (最大)

❌ 不可以：
  safetimer_create(70000, ...);   // 70 秒 - 超出范围！
  safetimer_create(120000, ...);  // 2 分钟 - 超出范围！
```

**⚠️ 特别警告：静默截断风险**

当 `BSP_TICK_TYPE_16BIT=1` 且 `ENABLE_PARAM_CHECK=0` 时：

```c
/* 危险：period_ms 参数是 uint32_t，但内部存储是 uint16_t */
timer = safetimer_create(70000, ...);  // 传入 70000 (70 秒)
/* 实际存储：70000 & 0xFFFF = 4464 ms (4.5 秒) - 静默截断！ */

/* 结果：定时器每 4.5 秒触发一次，而不是预期的 70 秒 */
```

**如何避免：**

1. **开发阶段**：启用参数检查
```c
#define ENABLE_PARAM_CHECK 1  // 超出范围会返回错误
```

2. **生产环境**：在应用层验证
```c
uint32_t period = 70000;
if (period > 65535) {
    /* 错误处理：拒绝或使用软件计数器 */
    return ERROR_PERIOD_TOO_LARGE;
}
timer = safetimer_create(period, ...);
```

3. **使用编译时断言**（C11）
```c
_Static_assert(MY_PERIOD_MS <= 65535, "Period exceeds 16-bit tick limit");
```

**解决方案 A：使用软件计数器扩展周期**

```c
/* 实现 5 分钟定时器（300 秒）*/
uint16_t minute_counter = 0;

void long_timer_callback(void *data) {
    minute_counter++;

    if (minute_counter >= 60) {  /* 60 次 × 5 秒 = 300 秒 */
        minute_counter = 0;
        /* 执行 5 分钟任务 */
        do_something();
    }
}

/* 创建 5 秒基础定时器 */
timer = safetimer_create(5000, TIMER_MODE_REPEAT,
                         long_timer_callback, NULL);
```

**解决方案 B：改回 32-bit tick（增加 14 bytes）**

```c
/* safetimer_single.h */
#define BSP_TICK_TYPE_16BIT 0     /* 使用 uint32_t */

/* 内存占用：44 bytes → 58 bytes (36.3%) */
/* 支持最大周期：49.7 天 */
```

#### 2. **只支持 1 个定时器**

**解决方案：在回调中实现多任务**

```c
/* 方法 1：状态机轮流执行 */
void multi_task_callback(void *data) {
    static uint8_t state = 0;

    switch (state) {
        case 0:
            led1_toggle();  /* 任务 1 */
            state = 1;
            break;
        case 1:
            led2_toggle();  /* 任务 2 */
            state = 0;
            break;
    }
}

/* 方法 2：计数器实现不同周期 */
void multi_task_callback(void *data) {
    static uint16_t counter = 0;
    counter++;

    if (counter % 5 == 0) {   /* 每 500ms */
        led1_toggle();         /* 任务 1 */
    }

    if (counter % 10 == 0) {  /* 每 1000ms */
        led2_toggle();         /* 任务 2 */
    }
}

/* 100ms 基础定时器 */
timer = safetimer_create(100, TIMER_MODE_REPEAT,
                         multi_task_callback, NULL);
```

#### 3. **参数检查（可选）**

`ENABLE_PARAM_CHECK = 1`（默认）提供以下安全检查：

**保护的检查项：**

```c
/* safetimer_create() */
- period_ms == 0                    /* 周期为 0 */
- period_ms > 0x7FFFFFFF            /* 周期过大 */
- mode 不是合法值                   /* 模式错误 */

/* safetimer_start() / stop() / delete() */
- handle < 0 || handle >= MAX_TIMERS  /* 超出范围 */
- 定时器未被分配                     /* used_bitmap 检查 */

/* safetimer_get_status() */
- handle 无效
- is_running == NULL                 /* NULL 指针 */

/* safetimer_get_remaining() */
- handle 无效
- remaining_ms == NULL               /* NULL 指针 */
- 定时器未运行                       /* !active 检查 */

/* safetimer_get_pool_usage() */
- used == NULL                       /* NULL 指针 */
- total == NULL                      /* NULL 指针 */
```

**开销与收益：**
- **ROM 开销**：约 50-80 bytes
- **RAM 开销**：0 bytes（检查代码不占用 RAM）
- **性能开销**：每次 API 调用约 5-10 个时钟周期
- **收益**：避免无效参数导致的崩溃和未定义行为

**⚠️ 注意**：参数检查对 ROM 优化几乎无影响。真正影响 ROM 大小的是：
- **MAX_TIMERS**：定时器数量（影响循环和逻辑复杂度）
- **BSP_TICK_TYPE_16BIT**：tick 位宽（影响运算指令数量）

**何时禁用参数检查（ENABLE_PARAM_CHECK = 0）：**
- ✅ Flash 极度受限（< 2KB 且每个字节都关键）
- ✅ 生产环境，代码已充分测试且性能关键
- ❌ **一般不推荐禁用**，ROM 优化应优先调整 MAX_TIMERS

### 🔧 完整优化步骤

#### **步骤 1：修改 safetimer_single.h**

```c
#define MAX_TIMERS 1
#define ENABLE_PARAM_CHECK 0
#define BSP_TICK_TYPE_16BIT 1
#define USE_STDINT_H 0
```

#### **步骤 2：添加编译器警告禁用（如需要）**

在 `safetimer_single.c` 顶部：

```c
#pragma warning disable 373   /* implicit signed to unsigned conversion */
#pragma warning disable 520   /* function never called */
#pragma warning disable 752   /* conversion to shorter data type */
#pragma warning disable 759   /* expression generates no code */
#pragma warning disable 1471  /* indirect function call via NULL pointer */
```

#### **步骤 3：验证内存占用**

编译完成后，检查：
```
Data space: 40-50 bytes (25%-31%)  ✅ 目标达成
Program space: 250-350 bytes
```

### 💡 选择适合的配置

| 您的需求 | 推荐配置 | RAM 占用 | 说明 |
|---------|---------|---------|------|
| 单个任务，周期 < 65 秒 | 极限优化 | ~44 bytes (27%) | 最省内存 |
| 单个任务，周期 > 65 秒 | 回退方案 | ~58 bytes (36%) | 32-bit tick |
| 2 个独立任务，周期 < 65 秒 | MAX_TIMERS=2 + 16-bit | ~53 bytes (33%) | 平衡方案 |
| 2 个独立任务，周期 > 65 秒 | MAX_TIMERS=2 + 32-bit | ~67 bytes (42%) | 标准方案 |

### 📝 内存预算计算器

```
SafeTimer RAM = (14 bytes × MAX_TIMERS) + 3 bytes (bitmap + reserved)

16-bit tick (BSP_TICK_TYPE_16BIT = 1):
  - MAX_TIMERS = 1: 14 + 3 = 17 bytes (实际含对齐 ~44 bytes)
  - MAX_TIMERS = 2: 28 + 3 = 31 bytes (实际含对齐 ~53 bytes)

32-bit tick (BSP_TICK_TYPE_16BIT = 0):
  - MAX_TIMERS = 1: 14 + 3 = 17 bytes (实际含对齐 ~58 bytes)
  - MAX_TIMERS = 2: 28 + 3 = 31 bytes (实际含对齐 ~67 bytes)

额外开销（BSP 全局变量）：
  - s_system_ticks: 2 bytes (16-bit) 或 4 bytes (32-bit)
  - s_critical_nesting: 1 byte
  - s_saved_ea: 1 byte
  - 编译器对齐: ~15-25 bytes
```

---

## ⚠️ 常见问题

### 1. 编译错误 "undefined symbol: bsp_get_ticks"
**原因**：BSP 文件未编译或未链接  
**解决**：确保编译和链接 BSP 文件
```bash
sdcc main.c safetimer_single.rel bsp.rel -o output.hex
                                 ↑ 必须包含
```

### 2. 定时器不工作或不准确
**检查清单**：
- [ ] 硬件定时器工作在**定时模式**（Timer Mode），而非计数模式
- [ ] 定时器配置为精确 1ms 中断
- [ ] `timer_interrupt_handler()` 被正确调用
- [ ] `safetimer_process()` 在主循环中定期调用

**快速验证**：
```c
static volatile uint32_t isr_count = 0;
void timer_interrupt_handler(void) {
    s_system_ticks++;
    isr_count++;  /* 调试用：观察此变量 */
}
/* 1秒后 isr_count 应接近 1000 */
```

### 3. 定时器周期不准确或随机变化
**常见原因**：硬件定时器配置为**计数模式**（外部引脚计数）

**错误示例（8051）**：
```c
TMOD = 0x05;  /* ❌ C/T=1 → 计数模式，错误！ */
```

**正确配置**：
```c
TMOD = 0x01;  /* ✅ C/T=0 → 定时模式，正确 */
```

**验证方法**：
```c
if (TMOD & 0x04) {
    /* ❌ 计数模式，检查 TMOD 配置 */
}
```

### 4. 编译警告 "function declared implicit int"
**原因**：函数调用前未声明  
**解决**：添加函数声明
```c
extern void init_timer0(void);  /* 添加声明 */

int main(void) {
    init_timer0();  /* 现在正确 */
}
```

### 5. 8-bit MCU 读取时间不准确/跳变
**原因**：32-bit 读取缺少原子保护（旧版本）  
**解决**：使用最新版本（v1.2.0+），`bsp_get_ticks()` 已实现原子保护

---


## 🎓 完整示例

```c
#include "safetimer_single.h"

/* 定时器回调函数 */
void led1_callback(void *data) {
    LED1 = !LED1;  /* LED1 每 500ms 切换 */
}

void led2_callback(void *data) {
    LED2 = !LED2;  /* LED2 每 1000ms 切换 */
}

int main(void) {
    safetimer_handle_t timer1, timer2;
    
    /* 初始化硬件定时器（1ms 中断）*/
    init_timer0();
    
    /* 创建并启动定时器 */
    timer1 = safetimer_create(500, TIMER_MODE_REPEAT, led1_callback, NULL);
    timer2 = safetimer_create(1000, TIMER_MODE_REPEAT, led2_callback, NULL);
    safetimer_start(timer1);
    safetimer_start(timer2);
    
    /* 主循环 */
    while (1) {
        safetimer_process();  /* 必须定期调用 */
    }
}
```

---

## 📖 更多资源

### 文档
- [用户指南](../docs/user_guide.md)
- [API 参考](../docs/api_reference.md)
- [架构设计](../docs/architecture.md)
- [移植指南](../docs/porting_guide.md)

### 示例
- [SC8F072 完整示例](../examples/sc8f072/)
- [STM8 完整示例](../examples/stm8/)
- [STC8 完整示例](../examples/stc8/)

### 支持
- GitHub Issues: [提交问题](https://github.com/your-repo/SafeTimer/issues)
- 邮件支持: support@example.com

---

## 📝 版本历史

### v1.2.1-single (2025-12-16)
- 🔥 **关键修复**：修复 16-bit tick 时间比较逻辑错误（导致定时器立即触发）
  - 添加 `safetimer_tick_diff()` 辅助函数正确处理回绕
  - 修复 `safetimer_process()` 和 `safetimer_get_remaining()` 中的错误比较
- 🔧 修复 `bsp_exit_critical()` 不平衡调用保护（防止永久禁用中断）
- 📚 更新配置文档，明确当前默认值针对超低内存平台（SC8F072）优化
- ⚠️ 添加 period_ms 参数截断警告和避免方法
- ✅ 完善 API 注释，说明 16-bit tick 的参数限制

### v1.2.0-single (2025-12-15)
- ✅ 修复 BSP 原子读取问题
- ✅ 添加临界区嵌套支持
- ✅ 修复所有类型转换警告
- ✅ 修复标准版本所有平台 BSP（SC8F072/STM8/STC8）
- ✅ 完善文档和示例
- ✅ 添加定时器模式要求说明（定时模式 vs 计数模式）
- ✅ 新增 16-bit tick 支持（节省 8 bytes RAM）
- ✅ 新增极限资源优化章节（RAM < 200 bytes 配置指南）
- ✅ 添加内存预算计算器和配置选择表

### v1.0.0-single (2024-XX-XX)
- 🎉 首次发布单文件版本

---

## ⚖️ 许可证

MIT License - 详见 [LICENSE](../LICENSE) 文件

---

**💡 提示：单文件版本是为了快速上手和学习，生产环境建议使用标准版本以获得更好的模块化和可维护性。**
