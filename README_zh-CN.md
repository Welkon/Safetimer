# SafeTimer ⏱️

**轻量级嵌入式定时器库，专为资源受限的 8 位单片机设计**

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.3.1-green.svg)]()
[![C99](https://img.shields.io/badge/C-C99-brightgreen.svg)]()
[![Test Coverage](https://img.shields.io/badge/coverage-96.30%25-brightgreen.svg)]()
[![Tests](https://img.shields.io/badge/tests-63%20passing-success.svg)]()

[English](README.md) | 简体中文

---

## 🎯 特性

- **极小 RAM 占用：** 4 个并发定时器仅需 60 字节（v1.3.0）
- **精简代码体积：** ~0.8KB Flash（查询 API 禁用）| ~1.0KB Flash（完整功能）
- **零动态内存：** 无 malloc/free，完全静态内存分配
- **溢出安全：** 自动处理 32 位时间回绕，无 49 天崩溃限制（ADR-005）
- **高度可移植：** 仅需 3 个 BSP 函数，适配任意 MCU
- **灵活 API：** 核心 API 提供精细控制 + 可选辅助 API 简化使用（v1.1+）
- **协程支持（v1.3.0）：** 无栈协程（Protothread 风格）+ 信号量，用于异步编程
- **充分测试：** 63 个单元测试，96.30% 代码覆盖率
- **生产级质量：** 符合 MISRA-C 规范，静态分析无警告

---

## 📦 快速开始

### 3 步集成

**1. 复制文件（4 个必需 + 1 个可选）**
```bash
cp SafeTimer/include/{safetimer.h,safetimer_config.h,bsp.h} your_project/
cp SafeTimer/src/safetimer.c your_project/
cp SafeTimer/include/safetimer_helpers.h your_project/  # 可选
```

**2. 实现 BSP（3 个函数）**
```c
bsp_tick_t bsp_get_ticks(void);      // 返回启动后的毫秒数
void bsp_enter_critical(void);       // 禁用中断
void bsp_exit_critical(void);        // 启用中断
```

**3. 使用定时器**
```c
#include "safetimer.h"

safetimer_handle_t h = safetimer_create(1000, TIMER_MODE_REPEAT, callback, NULL);
safetimer_start(h);

while (1) {
    safetimer_process();  // 在主循环中调用
}
```

**📖 完整教程：** 参见 [tutorials/quick-start.md](tutorials/quick-start.md)

---

## ⚙️ 系统要求

**硬件：**
- **RAM：** 58 字节（4 个定时器）| 114 字节（8 个定时器）
- **Flash：** ~0.8-1.2 KB
- **定时器：** 1ms 周期中断

**软件：**
- **编译器：** C99 或 C89 + `stdint.h`
- **无依赖：** 无需 RTOS、HAL 或动态内存

**兼容性：** 8 位 MCU（8051、AVR、PIC）| 16 位 | 32 位 | 任何支持中断的架构

---

## 🎓 API 概览

### 核心 API

```c
/* 创建定时器 */
safetimer_handle_t h = safetimer_create(period_ms, mode, callback, user_data);

/* 生命周期 */
safetimer_start(h);
safetimer_stop(h);   // 可选（需要 ENABLE_QUERY_API=1）
safetimer_delete(h);

/* 处理（在主循环中调用） */
safetimer_process();

/* 查询（可选，需要 ENABLE_QUERY_API=1） */
safetimer_get_status(h, &is_running);
safetimer_get_remaining(h, &remaining_ms);
safetimer_get_pool_usage(&used, &total);
```

**📖 完整 API 参考：** 参见 `include/safetimer.h` 获取完整 API 文档

---

## 📚 文档

### 教程

从这里开始学习 SafeTimer：**[教程索引](tutorials/README.md)**

| 教程 | 说明 |
|------|------|
| [快速开始](tutorials/quick-start.md) | 安装、BSP、首个定时器 |
| [协程教程](tutorials/coroutines.md) | 使用协程进行异步编程（v1.3.0+） |
| [配置与调优](tutorials/configuration-and-tuning.md) | 资源优化、编译时标志 |
| [用例与最佳实践](tutorials/use-cases.md) | 常见模式、反模式、设计指南 |
| [测试指南](tutorials/testing.md) | 单元测试、覆盖率、CI/CD |
| [BSP 移植指南](tutorials/bsp-porting.md) | 硬件抽象层实现 |
| [架构说明](tutorials/architecture-notes.md) | 溢出处理、设计决策 |

### 技术参考

- **[API 参考文档](docs/api_reference.md)** - 完整 API 文档
- **[架构文档](docs/architecture.md)** - 设计决策和 ADR

### 头文件

在 `include/` 目录中直接查看 API：
- `safetimer.h` - 核心 API 文档
- `safetimer_config.h` - 配置选项
- `safetimer_coro.h` - 协程宏（v1.3.0+）
- `safetimer_helpers.h` - 辅助 API（v1.1+）
- `bsp.h` - BSP 接口规范

---

## 🛠️ 支持的平台

SafeTimer 高度可移植，可在任何满足以下条件的 MCU 上运行：
- C99 兼容编译器（或 C89 + `stdint.h`）
- 中断支持（启用/禁用）
- 能产生 1ms 周期中断的硬件定时器

**兼容架构：** 8 位（8051、AVR、PIC）| 16 位 | 32 位 | 任何满足上述要求的架构

**示例：** 参见 [`examples/`](examples/) 目录中的参考 BSP 实现（SC8F072、协程示例）。

---

## 🚀 开发路线

### v1.3.x（当前）
- [x] 核心实现，支持溢出处理
- [x] 协程支持（v1.3.0）
- [x] 零累积误差（v1.3.1）
- [x] 63 个单元测试，96.30% 覆盖率
- [x] GitHub Actions CI/CD

### v1.4（未来）
- [ ] 更多 MCU 平台的 BSP 示例
- [ ] 定时器分组
- [ ] 定时器优先级
- [ ] 性能基准测试

完整版本历史请参见 [CHANGELOG.md](CHANGELOG.md)。

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

## 📝 更新日志

完整版本历史请参见 [CHANGELOG.md](CHANGELOG.md)。

**最新版本：** v1.3.1 (2025-12-19) - 使用 `safetimer_advance_period()` API 修复协程累积定时误差

---

**献给在资源受限环境中战斗的嵌入式开发者 ❤️**

---

**当前版本：** 1.3.1 (2025-12-19)
