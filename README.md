<div align="center">

# 🧰 TrollFools 注入插件开发 Skill

**TrollFools Injection Plugin Development — AI Agent Skill**

iOS 注入插件（dylib）开发全流程方法论，沉淀自真实项目的 25+ 版本迭代

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-blue.svg)](https://github.com/antisali996-dot/trollfools-inject-dev-skill)
[![Platform](https://img.shields.io/badge/platform-iOS%2014%20--%2017-black.svg)](https://developer.apple.com/ios/)
[![Skill Format](https://img.shields.io/badge/format-opencode%20skill-8A2BE2.svg)](https://opencode.ai)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/antisali996-dot/trollfools-inject-dev-skill/pulls)

</div>

---

## 📖 这是什么

一个面向 **AI Agent（opencode / Claude Code 等）** 的 Skill 定义文件，封装了在 **TrollStore / TrollFools** 环境下为 iOS App 开发注入插件（dylib）的完整方法论：

> 逆向分析 → 注入方案设计 → Hook 实现 → 交叉编译 → 注入调试 → 发布开源

所有经验沉淀自「**趣智校园去开屏广告插件**」真实项目 —— 25+ 个版本迭代、4 大类高频问题、以及大量用真金白银踩出来的坑。

> [!IMPORTANT]
> 这不是成品插件，而是一套**给 AI 用的方法论与工作流**。让 Agent 拿到 `SKILL.md` 后，能像有经验的逆向工程师一样思考、动手、排错。

---

## ✨ 特性一览

| 能力 | 说明 |
|------|------|
| 🔍 **目标程序分析** | 解包 IPA、识别广告 SDK 技术栈、梳理业务模块 |
| 🛠 **逆向定位** | IDA 反编译定位关键类/方法/调用链（开屏、插屏、越狱检测等） |
| 🎯 **注入方案设计** | 三层 Hook 架构：入口层 / SDK 出口 / 服务端总开关 |
| ⚡ **Hook 实现** | Objective-C runtime swizzle + 通知延迟执行，纯 C + dlsym 零依赖 |
| 🔧 **交叉编译** | LLVM clang + ld64.lld 生成 iOS 原生 Mach-O（chained fixups），无需 macOS/Xcode |
| 🐛 **调试排错** | .ips 崩溃日志分析、最小化二分、可见标记验证（弹窗/标题） |
| 📦 **打包部署** | TrollFools 注入、GitHub 开源 + Release 发布 |

---

## 🎯 适用场景

- 为某个 iOS App 开发**去广告 / 去校验 / 功能增强** dylib
- 分析 App 内部机制（广告触发、越狱检测、内购校验等）的调用链
- 明确使用 **TrollFools / TrollStore 注入方式**（而非重打包 IPA）
- 在 **无 macOS/Xcode** 的环境（Windows/Linux）交叉编译 iOS dylib

### 不适用的情况

| 情况 | 说明 |
|------|------|
| ❌ 目标 App 未解密 | 加密 IPA 无法静态分析，注入兼容性差 |
| ❌ 只需要重打包安装 | 流程不同（改主程序二进制 + TrollStore 安装），但逆向分析部分可复用 |
| ❌ 目标是 Android/Windows/Linux | 本 Skill 面向 iOS |
| ❌ 无测试设备 | 无法验证，先解决设备问题 |

---

## 💎 核心经验（血的教训）

> 这 10 条是本 Skill 的精华。AI Agent 执行任务时优先理解：

1. **TrollFools 注入是 weak 加载，失败静默无痕** —— `LC_LOAD_WEAK_DYLIB` 加载失败时 dyld 直接跳过，无崩溃无日志。**必须建立可见验证**（弹窗/标题），否则"注入成功但没效果"无法区分是"没加载"还是"hook 没生效"。
2. **iOS dylib 必须用 clang + ld64.lld 构建** —— 生成 `CHAINED_FIXUPS` + `__init_offsets` 结构。zig 等工具链产物缺此结构，会被 dyld **静默拒绝**（本项目最大的坑，浪费了大量迭代）。
3. **Hook 时机推迟到启动完成后** —— constructor（dyld initializer）阶段直接调用 objc runtime API 会 **SIGILL**（runtime 未初始化）。constructor 只做 dlsym + 注册通知观察者。
4. **先找"唯一出口"，再找"总开关"，最后兜底"入口层"** —— 多层级 Hook 设计。例如 TopOn 聚合的 `showSplashWithPlacementID:` 是所有渠道开屏的最终出口；`isAdsWithAdCode:` 是服务端控制的总开关。
5. **崩溃分析看 .ips 报告** —— SIGILL 多为 constructor 指针/rebase 问题；SIGTRAP 多为 Frida 注入被拒；"卡住不崩"多为 hook 破坏了启动流程。
6. **最小化二分排错** —— 批量 Hook 失效/崩溃时，先最小集验证再逐批加回，一次只改一个变量。
7. **Hook 点必须逐条确认存在** —— 运行时检查（`class_getInstanceMethod`）+ 记录 MISS，否则版本差异导致静默失效。
8. **不要 hook 启动流程关键步骤** —— 如启动背景图流程，置空会导致启动页卡死。
9. **Frida 在本类环境不可靠** —— iOS 16 + RootHide 下 gum 注入 SIGTRAP；动态验证受阻时改用**可见标记**方案。
10. **交付物必须含踩坑记录** —— README 记录失败经验，是对后续使用者最大的价值。

---

## 🔄 标准工作流程（SOP）

```
┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│ 1. 环境检查 │ → │ 2. 结构分析 │ → │ 3. 目标分析 │ → │ 4. 方案设计 │
└─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘
                                                      
┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
│ 8. 最终交付 │ ← │ 7. 修复问题 │ ← │ 6. 调试验证 │ ← │ 5. 编写插件 │
└─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘
```

### 各阶段要点

| 阶段 | 做什么 | 关键注意 |
|------|--------|----------|
| **1. 环境检查** | 确认 IPA / 工具链 / 设备可用 | IPA 必须是**解密版**；记录 bundle id 与版本号 |
| **2. 项目结构分析** | 解包 IPA，识别广告 SDK 家族 | Frameworks 目录直接暴露技术栈；主程序是 `Payload/*.app/*` 无扩展名大文件 |
| **3. 目标程序分析** | IDA 反编译，梳理完整触发链路 | selref→`__objc_stubs`→调用者三层跳转；classref 扫描列出所有 alloc 者；记录服务端下发开关 |
| **4. 确定注入方案** | 选定 Hook 点层级，设计多级防护 | 总开关 + 出口双保险；Hook 点逐一用 IDA 确认存在 |
| **5. 编写插件** | 纯 C + objc runtime + dlsym | constructor 只注册通知；`-fno-stack-protector`；`-undefined dynamic_lookup` |
| **6. 调试验证** | 确认插件**真的加载并执行** | **弹窗验证**最可靠（UIAlertView + 2 秒 NSTimer）；弱加载失败无任何迹象 |
| **7. 修复问题** | 按崩溃/失效表现定位修复 | 最小化二分，一次只改一个变量 |
| **8. 最终交付** | 清理产物、写 README/LICENSE、开源 | README 必须包含**踩坑记录** |

---

## 🧩 Hook 点选择策略

```
优先级：总开关 > 展示入口 > SDK 唯一出口
```

1. **总开关**（最优）—— 服务端可控的判定函数，如 `isAdsWithAdCode:` 读取 `advertiseList`
2. **展示入口** —— 如 `showSplashAds`，拦截"自动弹出"类广告（保留用户主动触发的激励视频）
3. **SDK 最终出口**（兜底）—— 聚合层如 `ATAdManager.showSplashWithPlacementID:`，所有渠道必经

> 为什么需要三层？单点 Hook 可能漏（多个入口），总开关 + 出口双保险才稳。

---

## 🐛 常见问题速查（10 大坑）

| # | 症状 | 原因 | 解决方案 |
|---|------|------|----------|
| 1 | 注入后完全无效果，无任何迹象 | weak 加载失败**静默跳过** | 检查 dylib 结构；用弹窗验证加载 |
| 2 | zig 编译的 dylib 不被 dyld 接受 | 缺 `CHAINED_FIXUPS` + `__init_offsets` | 换 **clang + ld64.lld** 构建 |
| 3 | 注入后闪退（SIGILL @ constructor） | constructor 指针缺 image base | 换 lld 构建；勿手动修指针 |
| 4 | 手动修 rebase 后 load commands 全 0x0 | zig 的 `LC_DYLD_INFO` 只有 16 字节，误写 bind 字段 | 只更新 `rebase_off/rebase_size`，绝不碰 off+16 |
| 5 | rebase 数据被重签丢弃 | `__LINKEDIT` filesize 未覆盖追加数据 | 追加后更新 filesize，或直接换 lld |
| 6 | constructor 调 objc runtime API 崩溃 | runtime 未就绪触发 libobjc assert | constructor 只 dlsym + 注册通知 |
| 7 | 弹窗在注入早期崩溃 | UI 环境未就绪 | 弹窗放启动完成通知回调 + NSTimer 自动 dismiss |
| 8 | 注入后启动页卡住 | hook 了启动背景图流程 | 不 hook 该流程；最小化二分定位 |
| 9 | Frida attach/spawn 秒退（SIGTRAP） | RootHide 环境兼容差 / 反调试 | 放弃 Frida，改用可见标记验证 |
| 10 | 加载成功但 hook 执行崩 | 调用方对返回值强依赖 / hook 了关键路径 | 延迟 hook 时机 + 最小集验证 |

---

## 🛠 技术栈

| 工具 | 用途 | 说明 |
|------|------|------|
| **IDA Pro + ida-pro-mcp** | 静态反编译 / xrefs / stub 扫描 | 找调用链、Hook 点、唯一出口 |
| **LLVM clang + ld64.lld** | iOS 交叉编译 + 链接 | ✅ 本项目验证有效的构建方式（生成 chained fixups） |
| **Theos iOS SDK** | 头文件 + tbd 库 | 无 Xcode 环境下的 iOS 编译 |
| **TrollFools** | dylib 注入（weak 加载） | 设备端注入与移除 |
| **.ips 崩溃日志** | 崩溃分析 | 崩溃线程/模块归属/异常类型 |
| **Python + struct** | Mach-O 解析 | 验证二进制结构、发现链接器缺陷 |
| **git + gh** | 版本管理与发布 | 开源交付 |

---

## 📁 文件结构

```
trollfools-inject-dev-skill/
├── SKILL.md      # 完整 skill 定义（SOP + 模板 + 排错 + Agent 规则）
└── README.md     # 本文件
```

---

## 📦 安装

### opencode

将 `SKILL.md` 放入 skills 目录：

```bash
# Windows
%USERPROFILE%\.config\opencode\skills\trollfools-inject-dev\SKILL.md

# macOS / Linux
~/.config/opencode/skills/trollfools-inject-dev/SKILL.md
```

### 其他 Agent（Claude Code / Codex 等）

参照各自平台的 skill 目录规范，将 `SKILL.md` 放入对应目录即可。

---

## 📝 输入 / 输出

### 输入条件

- ✅ 目标 App 的**解密版 IPA**（静态分析前提）
- ✅ TrollStore + TrollFools 已安装的测试设备（iOS 14-17）
- ✅ 可用的交叉编译工具（LLVM clang + ld64.lld）
- ✅ 目标 App 的 bundle id 与版本号

### 输出目标

- 📦 可注入的 arm64 dylib（含源码 + README + LICENSE，可开源）
- 📋 Hook 点清单与触发链路分析文档
- ✅ 验证通过的标准（可见标记/弹窗确认注入生效）

---

## 🤝 贡献

欢迎 PR / Issue：

- 新的踩坑记录（常见问题表）
- 新的工具链验证（其他构建方式的结构验证结果）
- 工作流改进建议

---

## 📜 License

[MIT](LICENSE)

---

<div align="center">

**如果这个 Skill 帮到了你，请给它一个 ⭐**

</div>
