---
name: trollfools-inject-dev
description: iOS App 去广告/功能修改的 TrollFools 注入插件开发全流程。用于分析目标 App 机制、设计注入方案、编写 Hook dylib、交叉编译、注入调试与发布。适用于 TrollStore/TrollFools 环境下的 iOS 插件开发任务。
version: 2.0.0
tags: [ios, trollstore, trollfools, dylib, hook, objc, reverse-engineering, 逆向, 注入, 去广告]
---

# TrollFools 注入插件开发 Skill

## Skill 信息

- **名称**：trollfools-inject-dev（TrollFools 注入插件开发）
- **简介**：面向 iOS 注入插件（dylib）开发的完整方法论，覆盖逆向分析、Hook 实现、交叉编译、注入调试、发布全链路。沉淀自「趣智校园去开屏广告插件」真实项目（25+ 版本迭代，含大量失败经验）。
- **适用场景**：
  - 为某个 iOS App 开发去广告/去校验/功能增强 dylib
  - 需要分析 App 内部机制（广告触发、越狱检测、内购校验等）
  - 在 TrollStore/TrollFools 环境下注入插件
  - 需要交叉编译 iOS dylib（Windows/Linux 环境）
- **输入条件**：
  - 目标 App 的**解密版 IPA**（必须，静态分析前提）
  - TrollStore + TrollFools 已安装的测试设备（iPhone，iOS 14-17）
  - 可用的交叉编译工具（LLVM clang + ld64.lld）
  - 目标 App 的 bundle id 与版本号
- **输出目标**：
  - 可注入的 arm64 dylib（含源码 + README + LICENSE，可开源）
  - Hook 点清单与触发链路分析文档
  - 验证通过的标准（可见标记/弹窗确认注入生效）

---

## Before Start Checklist

> Agent 开始任何任务前，逐项确认。缺项即阻塞——先解决缺项再动手，禁止跳过。

| # | 检查项 | 确认方式 | 缺项处理 |
|---|---|---|---|
| 1 | IPA 状态 | 是否**解密版**（可用 Mach-O 加载器解析） | 未解密 → 先获取解密版，中止后续 |
| 2 | App 版本 | 记录目标 App 的 bundle id 与版本号 | 影响方法名判断，升级后需重新分析 |
| 3 | Bundle ID | `Payload/*.app/Info.plist` 读取 | 用于注入确认与版本差异判断 |
| 4 | 架构 | arm64 / arm64e（`lipo -info` 或 Python 解析） | arm64 单 slice 在 arm64e 设备兼容 |
| 5 | iOS 环境 | iOS 版本（14-17）、是否 RootHide、TrollStore 状态 | 决定 Frida 是否可用（RootHide 下不可靠） |
| 6 | TrollStore/TrollFools | 已安装且注入可用 | 缺失 → 先安装，否则无法验证 |
| 7 | 测试方案 | 已确定**可见验证**手段（弹窗/标题修改） | 未定 → 先设计弹窗验证，禁止「试试看」 |

---

## Decision Tree

> Agent 根据输入自动选择路线，减少试错。

```
开始
│
├─ IPA 是否解密？
│   ├─ 否 → 要求解密版 IPA，中止（加密 IPA 无法静态分析）
│   └─ 是 ↓
│
├─ 是否适合 TrollFools 注入？
│   ├─ 用户需要重打包安装（直接改主程序二进制）？
│   │   └─ 是 → 走重打包分支（逆向分析部分仍复用本 Skill）
│   ├─ 目标是 Android/Windows/Linux？→ 不适用本 Skill，中止
│   ├─ 无测试设备？→ 先解决设备，中止
│   └─ 是（TrollStore/TrollFools 注入）↓
│
├─ 目标功能类型？
│   ├─ 开屏/插屏广告 → 找 SDK 聚合出口（TopOn `showSplashWithPlacementID:`）+ 服务端总开关
│   ├─ 越狱检测 → 反检测 Hook（判定函数返回未越狱）
│   ├─ 内购校验 → 分析服务端校验链路，找判定函数
│   └─ 其他功能修改 → 逆向定位触发点，按 hook-design.md 设计
│       ↓
├─ 执行标准流程（见下方 SOP 8 步）
│       ↓
├─ 崩溃/失效？
│   ├─ SIGILL (EXC_BAD_INSTRUCTION) → 查 constructor 指针 / rebase（macho-debug.md 陷阱 2）
│   ├─ SIGTRAP (EXC_BREAKPOINT) → Frida 注入被拒（troubleshooting.md 问题 9）
│   ├─ 卡住（无崩溃）→ hook 破坏启动流程（troubleshooting.md 问题 8）
│   ├─ 无任何效果（无弹窗无崩溃）→ 弱加载失败（troubleshooting.md 问题 1-2）
│   └─ Hook 不生效 → 版本差异 MISS / Hook 点错误（hook-design.md）
└─ 完成 → 输出 Final Report（见下方格式）
```

---

## 核心经验摘要

以下是本 Skill 最重要的经验，Agent 应优先理解：

1. **TrollFools 注入是 weak 加载，失败静默无痕**——`LC_LOAD_WEAK_DYLIB` 加载失败时 dyld 直接跳过，无崩溃无日志。因此**必须建立可见验证**（弹窗/标题），否则"注入成功但没效果"无法区分是"没加载"还是"hook 没生效"。
2. **iOS dylib 必须用 clang + ld64.lld 构建**——生成 `CHAINED_FIXUPS` + `__init_offsets` 结构。zig 等工具链产物缺此结构，会被 dyld 静默拒绝（本项目最大的坑，浪费了大量迭代）。
3. **Hook 时机推迟到启动完成后**——constructor（dyld initializer）阶段直接调用 objc runtime API 会 SIGILL（runtime 未初始化）。constructor 只做 dlsym + 注册通知观察者。
4. **先找"唯一出口"，再找"总开关"，最后兜底"入口层"**——多层级 Hook 设计（如 TopOn 聚合的 `showSplashWithPlacementID:` 是所有渠道开屏的最终出口；`isAdsWithAdCode:` 是服务端控制的总开关）。
5. **崩溃分析看 .ips 报告**——SIGILL 多为 constructor 指针/rebase 问题；SIGTRAP 多为 Frida 注入被拒；"卡住不崩"多为 hook 破坏了启动流程。
6. **最小化二分排错**——批量 Hook 失效/崩溃时，先最小集验证再逐批加回，一次只改一个变量。
7. **Hook 点必须逐条确认存在**（运行时检查 + 记录 MISS），否则版本差异会导致静默失效。
8. **不要 hook 启动流程关键步骤**（如启动背景图流程），置空会导致启动页卡死。
9. **Frida 在本类环境（iOS 16 + RootHide）不可靠**——gum 注入 SIGTRAP；动态验证受阻时改用可见标记方案。
10. **交付物必须含踩坑记录**——README 记录失败经验是对后续使用者最大的价值。

---

## 核心能力

| 能力 | 说明 |
|---|---|
| **目标程序分析** | 解包 IPA、识别技术栈（广告 SDK/聚合平台）、梳理业务模块 |
| **逆向定位** | IDA 反编译定位关键类/方法/调用链（开屏、插屏、越狱检测等） |
| **注入方案设计** | 选择 Hook 点（高层入口 vs SDK 出口 vs 总开关），设计多层级防护 |
| **Hook 实现** | Objective-C runtime swizzle（method_setImplementation）+ 通知延迟执行 |
| **插件开发** | 纯 C + dlsym 运行时符号解析，零依赖或正常链接系统框架 |
| **交叉编译** | LLVM clang + ld64.lld 生成 iOS 原生 Mach-O（chained fixups） |
| **调试排错** | .ips 崩溃日志分析、最小化二分、可见标记验证（弹窗/标题） |
| **打包部署** | TrollFools 注入、Release 发布（GitHub + dylib 附件） |

---

## 标准工作流程（SOP）

> 每步「要点」为必做，「详见」指向详细参考。完整分析流程见 references/。

### 1. 环境检查
- **要点**：确认目标 IPA（解密版）、工具链（clang + ld64.lld）、测试设备（TrollStore + TrollFools）
- **为什么**：注入插件的坑大部分在构建环境，先确认再动手
- **常用工具**：`Get-ChildItem`（文件确认）、`clang --version`、`gh auth status`
- **详见**：references/macho-debug.md（工具链结论）

### 2. 项目结构分析
- **要点**：解包 IPA，列出主程序、Frameworks、Bundle 资源，识别广告 SDK 家族
- **为什么**：Frameworks 目录直接暴露技术栈；主程序是 `Payload/*.app/*`（无扩展名大文件）
- **常用工具**：`tar -xf`、Python struct 解析 Mach-O

### 3. 目标程序分析（核心）
- **要点**：IDA 反编译，梳理目标功能从"启动到展示"的完整触发链路
- **核心方法**：找唯一出口、selref→`__objc_stubs`→调用者三层跳转、classref 扫描、服务端下发开关识别
- **详见**：references/ida-workflow.md

### 4. 确定注入方案
- **要点**：选定 Hook 点层级（总开关/展示入口/SDK 出口），设计多级防护；Hook 点逐一用 IDA 确认存在
- **详见**：references/hook-design.md

### 5. 编写插件
- **要点**：纯 C + objc runtime API + dlsym；constructor 只注册通知，Hook 放启动后；禁用栈保护
- **模板**：templates/hook.c（⚠️ 需按目标 App 调整，不可直接复制）
- **详见**：references/hook-design.md（Hook 时机规则）

### 6. 调试验证
- **要点**：建立**可见验证**（弹窗/标题）——weak 加载失败静默无痕，没有弹窗标记就等于没验证
- **常用工具**：可见标记（`UIAlertView` + 2 秒 NSTimer）、TrollFools 注入日志、.ips 崩溃日志
- **详见**：references/troubleshooting.md

### 7. 修复问题
- **要点**：按崩溃/失效表现定位修复；**最小化二分**——一次只改一个变量，每个版本给用户可验证预期
- **详见**：references/troubleshooting.md（10 个常见问题 + 排查方法）

### 8. 最终交付
- **要点**：清理测试产物、写 README/LICENSE、GitHub 开源 + Release；README 必须含踩坑记录
- **常用工具**：git、gh（`gh repo create --public --source . --push`、`gh release create v1.0.0 out.dylib`）

---

## Anti Patterns

> Agent 禁止以下行为。这些都是本项目真实踩过的坑。

| 禁止行为 | 后果 |
|---|---|
| **未分析 IPA 就编写 Hook** | 方法名/类名错误，静默失效，浪费迭代 |
| **一次加入大量 Hook** | 崩溃/失效时无法定位是哪个 Hook 的问题 |
| **constructor 阶段执行复杂逻辑**（调 objc API） | SIGILL 崩溃（runtime 未就绪） |
| **未验证 dylib 加载就判断失败** | weak 加载失败静默无痕，无弹窗验证 = 没验证 |
| **修改 Mach-O 前不备份** | 修补 rebase/load commands 出错后无法回滚 |
| **hook 启动流程关键步骤**（如启动背景图） | 启动页卡死 |
| **依赖沙盒日志文件验证** | iOS 沙盒 Documents 用户无法访问 |
| **在 RootHide 环境依赖 Frida 动态验证** | gum 注入 SIGTRAP，浪费时间 |
| **手工修补 zig 产物 rebase** | load commands 损坏（16 字节 DYLD_INFO 陷阱） |
| **App 升级后不重新逆向** | 方法名变化导致 Hook MISS |

---

## 可复用模板

> ⚠️ 所有模板为**经过验证的可编译参考模板**，需根据目标 App、SDK、架构和环境调整，不可直接复制。

| 模板 | 文件 | 说明 |
|---|---|---|
| Hook 源码 | templates/hook.c | 完整 C 源码（符号解析 + 通知延迟 + 逐点确认 + 弹窗验证） |
| 构建脚本 | templates/build.sh | clang + ld64.lld 编译 + 结构自检 |
| 结构验证 | templates/check_macho.py | Mach-O 结构 PASS/FAIL 检查（CHAINED_FIXUPS/__init_offsets/__LINKEDIT） |

---

## Final Report Format

> Agent 完成任务后，必须按以下格式输出报告。

```
1. Target Analysis
   - App 名称 / 版本 / Bundle ID / 架构 / iOS 环境

2. Reverse Engineering Findings
   - 技术栈（广告 SDK/聚合平台）
   - 目标功能完整触发链路
   - 关键证据（唯一出口、总开关、调用者数量）

3. Hook Points
   - 每个 Hook 点：类名 / 方法名 / 层级（总开关/入口/出口）/ IDA 确认证据

4. Implementation
   - 源码位置 / Hook 时机 / 链接参数 / 工具链版本

5. Build Result
   - 构建命令 / 产物结构验证结果（check_macho.py PASS）

6. Test Result
   - 验证手段（弹窗/标题）/ 实际观察结果 / 是否通过

7. Known Issues
   - 未解决项 / 环境限制 / 潜在兼容问题

8. Maintenance Notes
   - 版本差异风险 / 更新注意事项 / 踩坑记录
```

---

## Maintenance

> Skill 自身版本管理规范。SKILL.md frontmatter 的 `version` 字段。

| 类型 | 适用场景 | 示例 |
|---|---|---|
| **patch**（x.y.**z**） | 修复错误内容、补充单条排错记录、修正模板 bug | `2.0.0 → 2.0.1` |
| **minor**（x.**y**.0） | 新增 references/模板/案例、扩展已有章节 | `2.0.0 → 2.1.0` |
| **major**（**x**.0.0） | 工作流程或结构变更、决策树调整、章节增删 | `1.0.0 → 2.0.0` |

维护规则：
- 每次更新同步更新本地源（`~\.config\opencode\skills\trollfools-inject-dev\`）与 GitHub 仓库（`trollfools-inject-dev-skill`），本地为主版本
- 内容变更必须保留历史经验——只增不删，或迁移时保持知识零丢失
- 发布到 GitHub 时更新 CHANGELOG 或 Release notes

---

## Agent 执行规则

1. **先分析后修改**：任何 Hook 点必须先用 IDA 确认方法存在与调用链，禁止凭猜测写 Hook
2. **每次修改保持可验证**：每个版本必须有用户可见的验证手段（弹窗/标题/行为变化），禁止"试试看"
3. **保留调试日志**：stderr 日志 + 落盘日志双保险，hook 成功/MISS 都要记录
4. **遇到未知行为优先逆向确认**：不猜——崩溃用 .ips 分析，失效用最小化二分，加载问题查结构
5. **不凭猜测修改关键逻辑**：rebase/load commands 等二进制级修改必须逐字节验证（对比 diff、dump 完整性），修改前必须备份
6. **弱加载必须建立可见验证**：TrollFools weak 加载失败静默无痕，没有弹窗标记就等于没验证
7. **编译工具链先验证结构**：本项目验证有效的是 clang + ld64.lld；使用其他工具链时，先验证 `CHAINED_FIXUPS`/`__init_offsets` 存在再继续
8. **Hook 时机推迟**：constructor 只注册通知，Hook 放启动完成后（避免早期 SIGILL）
9. **最小化二分**：批量 Hook 失效/崩溃时，先最小集验证再逐批加回
10. **交付含失败经验**：README 必须记录踩坑（结构问题/时机问题），这是对使用者最大的价值
