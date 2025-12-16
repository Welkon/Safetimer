# SafeTimer ⏱️

**轻量级嵌入式定时器库，专为资源受限的 8 位单片机设计**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.2.3-green.svg)]()
[![C99](https://img.shields.io/badge/C-C99-brightgreen.svg)]()
[![Test Coverage](https://img.shields.io/badge/coverage-96.30%25-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/tests-55%20passing-success.svg)]()

[English](README.md) | 简体中文

---

## 🎯 特性

- **极小 RAM 占用：** 4 个并发定时器仅需 58 字节
- **精简代码体积：** Flash 占用约 1.0 KB（启用参数检查）
- **零动态内存：** 无 malloc/free，完全静态内存分配
- **溢出安全：** 自动处理 32 位时间回绕，无 49 天崩溃限制（参见 [ADR-005](docs/architecture.md)）
- **高度可移植：** 仅需 3 个 BSP 函数，适配任意 MCU
- **灵活 API：** 核心 API 提供精细控制 + 可选辅助 API 简化使用（v1.1+）
- **充分测试：** 55 个单元测试，96.30% 代码覆盖率
- **生产级质量：** 符合 MISRA-C 规范，静态分析无警告

---

## 📦 快速开始

### 1. 集成方式

**🎯 选择适合你的集成方式：**

---

#### 📋 方式 1A：单文件版本（最简单）⭐

**适用场景：** 快速原型开发、简单项目、学习入门

```bash
# 仅需 2 个文件！
cp SafeTimer/single-file/safetimer_single.h your_project/
cp SafeTimer/single-file/safetimer_single.c your_project/
```

✅ 最小化集成复杂度，完整核心功能
✅ 所有配置在单个头文件中
✅ [查看单文件版本说明](single-file/README.md)

---

#### 📋 方式 1B：标准版本（推荐）

复制以下文件到你的项目目录：

```bash
# 步骤 1：复制必需文件（4 个）
cp SafeTimer/include/safetimer.h your_project/
cp SafeTimer/include/safetimer_config.h your_project/
cp SafeTimer/include/bsp.h your_project/
cp SafeTimer/src/safetimer.c your_project/

# 步骤 2（可选）：如需辅助 API
cp SafeTimer/include/safetimer_helpers.h your_project/
```

⚠️ **不要复制** `src/safetimer_internal.h`（内部实现文件）

**文件清单：**
- ✅ **必需（4 文件）：** safetimer.h、safetimer_config.h、bsp.h、safetimer.c
- ✅ **可选（1 文件）：** safetimer_helpers.h（便捷 API，v1.1+）
- ❌ **禁止复制：** safetimer_internal.h（内部实现）

### 2. 实现 BSP 接口（3 个函数）

创建 `safetimer_bsp.c` 并实现以下 3 个函数：

> **💡 命名建议：** 推荐使用 `safetimer_bsp.c` 以避免与其他库冲突。也可使用 `myapp_bsp.c` 或放在子目录如 `bsp/safetimer.c`。

```c
#include "bsp.h"

static volatile bsp_tick_t s_ticks = 0;

/* Called by hardware timer interrupt every 1ms */
void timer_isr(void) {
    s_ticks++;
}

bsp_tick_t bsp_get_ticks(void) {
    return s_ticks;
}

void bsp_enter_critical(void) {
    EA = 0;  /* Disable interrupts */
}

void bsp_exit_critical(void) {
    EA = 1;  /* Enable interrupts */
}
```

完整的 BSP 实现示例请参考 [`examples/`](examples/) 目录。

### 3. 使用定时器

```c
#include "safetimer.h"

void led_callback(void *user_data) {
    toggle_led();  /* User code */
}

int main(void) {
    /* Initialize hardware timer (1ms tick) */
    init_timer0();

    /* Create a 1000ms repeating timer */
    safetimer_handle_t led_timer = safetimer_create(
        1000,                    /* period_ms */
        TIMER_MODE_REPEAT,       /* mode */
        led_callback,            /* callback */
        NULL                     /* user_data */
    );

    safetimer_start(led_timer);

    /* Main loop */
    while (1) {
        safetimer_process();  /* Process timers */
    }
}
```

**⚡ 更简洁的写法（v1.1+）：**

对于常见的"创建即启动"场景，可使用可选的辅助 API：

```c
#include "safetimer_helpers.h"  /* Optional convenience layer */

int main(void) {
    init_timer0();

    /* Create and start in one line (zero overhead) */
    safetimer_handle_t led_timer = safetimer_create_started(
        1000, TIMER_MODE_REPEAT, led_callback, NULL
    );

    if (led_timer == SAFETIMER_INVALID_HANDLE) {
        /* Handle error */
    }

    while (1) {
        safetimer_process();
    }
}
```

**选择哪套 API？**
- 📦 **核心 API**（`safetimer.h`）：适用于级联定时器、条件启动等复杂场景
- ⚡ **辅助 API**（`safetimer_helpers.h`）：适用于 90% 场景的简单立即启动需求

详细对比请查看 [`examples/helpers_demo/`](examples/helpers_demo/)。

---

## ⚙️ 系统要求

SafeTimer 要求极低，几乎可在任何 8 位单片机上运行：

### 硬件要求
- **RAM：** 最低 58 字节（4 个定时器 + 内部状态）
  - 每个定时器：14 字节
  - 额外开销：2 字节
  - 用户栈空间：建议预留 20-50 字节
- **Flash/ROM：** 最低 1.0 KB（启用参数检查）
  - 优化构建：约 0.8 KB
- **时钟源：** 1ms 定时器中断（硬件定时器）
  - 精度要求：典型 ±1%，最大可接受 ±5%

### 软件要求
- **编译器：** C99 兼容或 C89 + `stdint.h`
  - 已测试：SDCC 4.x、GCC 9+、Keil C51 9.x
- **标准库：** 无需（辅助 API 可选使用 `stddef.h`）
- **中断支持：** 必须支持启用/禁用中断

### MCU 要求
- **架构：** 任意（8 位、16 位、32 位）
- **字节序：** 任意（大端或小端）
- **定时器：** 任何能产生 1ms 周期中断的硬件定时器

### 不需要
- ❌ RTOS
- ❌ 动态内存分配（malloc/free）
- ❌ 复杂的 HAL 库
- ❌ 特定 MCU 厂商

---

## 📊 资源占用

**默认配置（4 个定时器）：**
- **RAM：** 58 字节（4 定时器）到 114 字节（8 定时器）
- **Flash：** ~0.8 KB（精简版）到 ~1.2 KB（完整版）
- **处理耗时：** 典型 8 位 MCU 上每次 `safetimer_process()` 调用约 5-10µs

**扩展能力（通过 MAX_TIMERS 配置）：**
- 4 定时器（默认）= 58 字节 RAM
- 8 定时器 = 114 字节 RAM
- 16 定时器 = 226 字节 RAM
- 32 定时器 = 450 字节 RAM

---

## 🏗️ 架构亮点

### 安全的 32 位溢出处理（ADR-005）

SafeTimer 采用**有符号差值比较算法**自动处理时间回绕：

```c
/* Works correctly even when tick counter wraps from 2^32-1 to 0 */
if ((long)(current_tick - expire_time) >= 0) {
    /* Timer expired */
}
```

**结果：** 无 49 天崩溃限制，可无限运行。

**限制：** 单个定时器周期 ≤ 2³¹-1 ms（约 24.8 天）。

### 极简 BSP 接口

仅需实现 3 个函数：
- `bsp_get_ticks()` - 返回启动后的毫秒数
- `bsp_enter_critical()` - 进入临界区（关中断）
- `bsp_exit_critical()` - 退出临界区（开中断）

无需复杂的 HAL 或 RTOS 依赖！

---

## 📚 文档

| 文档 | 说明 |
|------|------|
| [用户指南](docs/user_guide.md) | 快速上手和常用模式 |
| [API 参考](docs/api_reference.md) | 完整 API 文档 |
| [移植指南](docs/porting_guide.md) | BSP 实现指南 |
| [架构设计](docs/architecture.md) | 设计决策（ADR） |
| [项目状态](docs/project_status.md) | 当前实现状态 |
| [开发计划](docs/epics_and_stories.md) | 开发路线图 |

---

## 🔬 测试

SafeTimer 使用 Unity 框架提供完整的单元测试：

```bash
cd test

# 安装 Unity（一次性操作）
cd unity && wget https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.5.2.tar.gz
tar -xzf v2.5.2.tar.gz && cp Unity-2.5.2/src/unity.* .

# 运行测试
cd .. && make test

# 生成覆盖率报告
make coverage
```

**测试覆盖率：** ~80%（目标：≥95%）

---

## 🛠️ 支持的平台

SafeTimer 设计为高度可移植，可在任何满足以下条件的 MCU 上运行：
- C99 兼容编译器（或 C89 + `stdint.h`）
- 中断支持（启用/禁用）
- 能产生 1ms 周期中断的硬件定时器

**兼容架构：**
- 8 位 MCU（8051、AVR、PIC 等）
- 16 位 MCU
- 32 位 MCU
- 任何满足上述要求的架构

参考 BSP 实现请查看 [`examples/`](examples/) 目录。

---

## 🎓 API 概览

### 定时器生命周期

```c
/* 创建定时器（不会自动启动） */
safetimer_handle_t h = safetimer_create(period_ms, mode, callback, user_data);

/* 启动/停止定时器 */
safetimer_start(h);
safetimer_stop(h);

/* 删除定时器（释放资源） */
safetimer_delete(h);
```

### 定时器处理

```c
void main_loop(void) {
    while (1) {
        safetimer_process();  /* 定期调用（建议频率 ≥ 最短周期的 2 倍） */
    }
}
```

### 查询函数

```c
int is_running;
safetimer_get_status(h, &is_running);

uint32_t remaining_ms;
safetimer_get_remaining(h, &remaining_ms);

int used, total;
safetimer_get_pool_usage(&used, &total);
```

---

## ⚙️ 配置

编辑 `include/safetimer_config.h` 或使用编译器标志：

```c
#define MAX_TIMERS 8              /* 最大并发定时器数量（1-32） */
#define ENABLE_PARAM_CHECK 1      /* 启用参数检查（0/1） */
#define USE_STDINT_H 0            /* 使用 stdint.h 或自定义类型 */
```

**编译器标志：**
```bash
gcc -DMAX_TIMERS=16 -DENABLE_PARAM_CHECK=0 ...
```

---

## 🚀 开发路线

### v1.0（当前）
- [x] 核心实现
- [x] 单元测试 + 模拟 BSP
- [ ] 各类 MCU 的 BSP 示例
- [ ] 完善文档
- [ ] 测试覆盖率 ≥95%

### v1.1（未来）
- [ ] GitHub Actions CI
- [ ] 更多 BSP 示例
- [ ] 定时器分组
- [ ] 定时器优先级
- [ ] 性能基准测试

详见 [开发计划](docs/epics_and_stories.md)。

---

## 🤝 参与贡献

欢迎贡献！请参阅 [CONTRIBUTING.md](CONTRIBUTING.md) 了解贡献指南。

**高价值贡献方向：**
- 更多 MCU 平台的 BSP 实现
- 文档改进
- 实际使用中的 Bug 报告
- 新增测试用例

---

## 📄 许可证

MIT License - 详见 [LICENSE](LICENSE) 文件。

可自由用于商业和非商业项目。

---

## 🙏 致谢

- **Unity Test Framework** by ThrowTheSwitch（MIT）
- 灵感来源于 FreeRTOS 软件定时器
- 溢出处理算法参考嵌入式系统最佳实践

---

## 📞 支持

- **问题反馈：** [GitHub Issues](https://github.com/your-repo/SafeTimer/issues)（待开放）
- **讨论交流：** [GitHub Discussions](https://github.com/your-repo/SafeTimer/discussions)（待开放）
- **文档：** `docs/` 目录

---

## ⭐ 为什么选择 SafeTimer？

与其他定时器库对比：

| 特性 | SafeTimer | FreeRTOS 定时器 | Arduino Timer | 自己实现 |
|------|-----------|----------------|---------------|----------|
| RAM 占用 | 114 字节 | 500+ 字节 | 200+ 字节 | 可变 |
| 代码体积 | ~1KB | ~5KB | ~3KB | 可变 |
| 依赖 | 无 | RTOS | Arduino | - |
| 溢出安全 | ✅ 是 | ✅ 是 | ❌ 否 | ⚠️ 取决于实现 |
| 适配 8 位 MCU | ✅ 是 | ❌ 否 | ⚠️ 有限 | ⚠️ 取决于实现 |
| 测试覆盖率 | ✅ 80%+ | ✅ 是 | ❌ 否 | ❌ 否 |

**SafeTimer = 精简 + 可移植 + 安全**

---

## 💡 使用场景与最佳实践

### ✅ SafeTimer 适用场景

SafeTimer 擅长**异步超时管理**和**周期性任务调度**：

**1. 周期性任务**
```c
/* LED 闪烁、心跳包、看门狗喂狗 */
h_led = safetimer_create(500, TIMER_MODE_REPEAT, led_blink, NULL);
h_heartbeat = safetimer_create(1000, TIMER_MODE_REPEAT, send_heartbeat, NULL);
```

**2. 通信超时**
```c
/* 发送数据并等待 ACK，3秒超时 */
void send_packet(void) {
    uart_send(data);
    h_timeout = safetimer_create(3000, TIMER_MODE_ONE_SHOT, timeout_cb, NULL);
    safetimer_start(h_timeout);
}

void on_ack_received(void) {
    safetimer_delete(h_timeout);  /* 取消超时 */
}
```

**3. 多阶段状态机**
```c
/* 开机流程：上电 → 2秒后初始化传感器 → 5秒后启动通信 */
void power_on(void) {
    h1 = safetimer_create(2000, TIMER_MODE_ONE_SHOT, init_sensor_cb, NULL);
    safetimer_start(h1);
}

void init_sensor_cb(void *data) {
    init_sensor();
    h2 = safetimer_create(5000, TIMER_MODE_ONE_SHOT, start_comm_cb, NULL);
    safetimer_start(h2);
}
```

**4. 延时动作**
```c
/* 10秒后关闭LED */
h_off = safetimer_create(10000, TIMER_MODE_ONE_SHOT, led_off_cb, NULL);
safetimer_start(h_off);
```

---

### ❌ SafeTimer 不适用场景

**1. 按键消抖** - 使用时间戳法更高效：
```c
/* ✅ 高效：6 bytes RAM，无需占用定时器槽位 */
void key_scan(void) {
    uint8_t current = BUTTON_PIN;
    uint32_t now = bsp_get_ticks();

    if (current != g_last_state) {
        if ((now - g_last_change_time) >= 20) {  /* 20ms 消抖 */
            g_last_state = current;
            g_last_change_time = now;
            if (current == 0) g_key_event = 1;
        }
    }
}

/* ❌ 浪费：14 bytes RAM + 1个定时器槽位用于简单任务 */
/* 不要为每次按键按下都创建/删除定时器！ */
```

**2. 高频率轮询** - 在主循环直接检测
**3. 微秒级精度** - SafeTimer 使用 1ms 滴答分辨率
**4. 硬实时** - 回调时机取决于 `safetimer_process()` 调用频率

---

### 📐 设计指南

**定时器槽位分配策略：**
```c
/* 示例：MAX_TIMERS = 4 */

/* 静态定时器（70-80%）：创建一次，永不删除 */
槽位 0：LED 闪烁（500ms REPEAT）
槽位 1：心跳包（1000ms REPEAT）

/* 动态定时器（20-30%）：按需创建/删除 */
槽位 2：通信超时（临时）
槽位 3：延时动作（临时）
```

**何时使用 `safetimer_delete()`：**
- ✅ 取消超时定时器（如超时前收到 ACK）
- ✅ 清理临时延时动作
- ✅ 测试清理函数
- ❌ 不用于静态周期定时器（创建一次，永久运行）

---

**献给在资源受限环境中战斗的嵌入式开发者 ❤️**

---

**当前版本：** 1.2.3 (2025-12-16)
