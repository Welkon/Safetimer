# SC8F072 测试文档使用指南

## 📚 文档总览

我们为 SC8F072 硬件测试准备了完整的文档体系：

| 文档 | 用途 | 预计时间 |
|------|------|----------|
| **QUICK_TEST_CHECKLIST.md** | 快速测试清单（推荐首先使用） | 30 分钟 |
| **HARDWARE_TEST_GUIDE.md** | 完整测试指南（详细版） | 2-4 小时 |
| **test_results/TEST_DATA_TEMPLATE.md** | 测试数据记录模板 | - |
| **test_results/README.md** | 测试结果目录说明 | - |

---

## 🚀 开始测试的 3 种方式

### 方式 1：快速测试（推荐新手）⚡

**适合场景：** 首次测试，快速验证基本功能

**使用文档：** `QUICK_TEST_CHECKLIST.md`

**步骤：**
```bash
cd examples/sc8f072

# 1. 打开 SCMCU IDE
# 2. File → Open Project

# 3. 编译项目（按 F7）

# 4. 查看输出窗口的 Program Size 信息
```

**优点：**
- ✅ 快速完成（30 分钟）
- ✅ 覆盖核心功能
- ✅ 简单易懂

---

### 方式 2：完整测试（推荐生产验证）📋

**适合场景：** 正式验证，需要详细记录

**使用文档：** `HARDWARE_TEST_GUIDE.md` + `test_results/TEST_DATA_TEMPLATE.md`

**步骤：**
```bash
cd examples/sc8f072

# 1. 阅读完整测试指南
# 使用文本编辑器或浏览器打开 HARDWARE_TEST_GUIDE.md

# 2. 复制测试模板
cp test_results/TEST_DATA_TEMPLATE.md test_results/my_test_$(date +%Y%m%d).md

# 3. 在 SCMCU IDE 中编译并测试（2-4 小时）
# 4. 在模板中记录所有数据
# 5. 保存所有测试附件（日志、照片等）
```

**优点：**
- ✅ 全面覆盖（40 个测试项）
- ✅ 详细记录可追溯
- ✅ 包含问题排查指南

---

### 方式 3：自动化测试（高级用户）🤖

**适合场景：** 批量测试，CI/CD 集成

**使用文档：** 自己编写脚本，参考测试指南

**⚠️ 注意：** SC8F072 使用中微半导体的 SCMCU IDE，目前**不支持命令行编译**。自动化测试需要：
1. 手动在 SCMCU IDE 中编译生成 .hex 文件
2. 使用 SCMCU Writer 命令行模式烧录（如果支持）
3. 或使用硬件测试夹具自动化验证功能

---

## 📝 测试流程建议

### 首次测试流程

```
1. 使用快速测试清单
   └─ 30 分钟快速验证
   └─ 确认基本功能正常

2. 如果快速测试通过
   └─ 进行完整测试（可选）
   └─ 记录详细数据

3. 保存测试结果
   └─ 将填写好的文档保存到 test_results/
   └─ 拍摄硬件照片
```

### 生产环境测试流程

```
1. 使用完整测试指南
   └─ 执行所有 40 个测试项
   └─ 详细记录在测试模板中

2. 稳定性测试
   └─ 短期：30 分钟（必需）
   └─ 中期：4 小时（推荐）
   └─ 长期：24 小时（可选）

3. 生成测试报告
   └─ 填写 TEST_DATA_TEMPLATE.md
   └─ 保存所有附件
   └─ 审核并签字
```

---

## 🔍 关键测试项说明

### 必须通过的测试（MUST）

| 测试项 | 验收标准 | 如何测试 |
|--------|----------|----------|
| 编译成功 | 无错误 | SCMCU IDE → Project → Build |
| RAM 占用 | ≤200 字节 | 查看 IDE 输出窗口 Program Size |
| Flash 占用 | ≤2048 字节 | 查看 IDE 输出窗口 Program Size |
| LED 闪烁 | 3 个 LED 按预期周期闪烁 | 目测观察 2 分钟 |
| 短期稳定 | 30 分钟无异常 | 连续运行观察 |

### 推荐通过的测试（SHOULD）

| 测试项 | 验收标准 | 如何测试 |
|--------|----------|----------|
| 时间精度 | ±5% | 秒表计时或示波器 |
| 中期稳定 | 4 小时无异常 | 长时间运行 |
| 复位恢复 | <100ms | 按复位键观察 |

---

## 📊 测试结果保存

### 文件命名建议

```
test_results/
├── test_20251214_initial.md          # 首次测试
├── test_20251215_stability.md        # 稳定性测试
├── test_20251220_production.md       # 生产验证
└── photos/
    ├── 20251214_hardware_setup.jpg
    └── 20251214_led_blink.jpg
```

### 必需保存的文件

```bash
# 1. 编译日志（手动复制）
# 在 SCMCU IDE 输出窗口，复制编译信息
# 保存到 test_results/compilation_$(date +%Y%m%d).log

# 2. 内存报告（手动记录）
# 在 IDE 中查看 Program Size 信息
# 记录到 test_results/memory_report_$(date +%Y%m%d).txt

# 3. 填写好的测试文档
cp QUICK_TEST_CHECKLIST.md test_results/quick_test_$(date +%Y%m%d).md
# 手动填写后保存

# 4. 硬件照片
# 拍摄后保存到 test_results/photos/
```

---

## ⚠️ 常见问题

### Q1: 我应该用哪个文档？

**A:**
- **快速验证** → 使用 `QUICK_TEST_CHECKLIST.md`（30 分钟）
- **详细测试** → 使用 `HARDWARE_TEST_GUIDE.md`（2-4 小时）
- **记录数据** → 使用 `TEST_DATA_TEMPLATE.md`

### Q2: 测试失败怎么办？

**A:** 参考 `HARDWARE_TEST_GUIDE.md` 的"问题排查指南"章节

### Q3: 必须完成所有 40 个测试项吗？

**A:**
- **最少要求：** 编译、资源、LED 闪烁、短期稳定（4 类）
- **推荐完成：** 加上复位、精度测试（6 类）
- **完整测试：** 所有 40 项（用于生产验证）

### Q4: 需要什么测试设备？

**最少：**
- SC8F072 开发板
- 3 个 LED + 电阻
- USB-TTL 编程器

**可选：**
- 示波器（精度测试）
- 秒表（简易精度测试）
- 万用表（电压测试）

---

## 📞 获取帮助

**测试过程中遇到问题？**

1. **查看文档**
   - `HARDWARE_TEST_GUIDE.md` 的问题排查章节
   - `README.md` 的故障排除部分

2. **查看示例**
   - `verification/VERIFICATION.md` - 已有的验证示例

3. **提交 Issue**
   - [GitHub Issues](https://github.com/your-repo/SafeTimer/issues)
   - 附上测试日志和照片

---

## ✅ 测试完成清单

测试完成后，确认以下事项：

- [ ] 选择了合适的测试方式（快速/完整/自动化）
- [ ] 完成了所有必需的测试项
- [ ] 保存了测试数据和日志
- [ ] 拍摄了硬件照片
- [ ] 填写了测试结论
- [ ] 测试文件保存到 `test_results/` 目录
- [ ] （可选）提交测试结果到项目仓库

---

**祝测试顺利！🎉**

如有问题，请参考各文档的详细说明或联系项目维护者。

---

**文档版本：** 1.0
**创建日期：** 2025-12-14
**适用版本：** SafeTimer v1.2.0
**平台：** SC8F072
