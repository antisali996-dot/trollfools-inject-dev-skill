---
name: trollfools-inject-dev
description: iOS App 去广告/功能修改的 TrollFools 注入插件开发全流程。用于分析目标 App 机制、设计注入方案、编写 Hook dylib、交叉编译、注入调试与发布。适用于 TrollStore/TrollFools 环境下的 iOS 插件开发任务。
version: 1.0.0
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

## Skill 调用条件

### 应该启用本 Skill 的场景
- 用户要求为特定 iOS App 制作**注入式**插件（去广告/去限制/功能增强）
- 目标明确为 **TrollFools / TrollStore 注入方式**（而非重打包 IPA 重装）
- 需要分析 App 内部广告/校验机制的调用链
- 需要在无 macOS/Xcode 环境下交叉编译 iOS dylib
- 用户提供或可获得解密 IPA

### 不需要（或不适用）本 Skill 的场景
- 目标 App **未解密**（加密 IPA 无法静态分析，注入兼容性差）——先要求解密版
- 用户只需要**重打包安装**（直接改主程序二进制 + TrollStore 安装）——流程不同，但本 Skill 的逆向分析部分仍可复用
- 目标是 Android/Windows/Linux 程序——本 Skill 面向 iOS
- 无测试设备（iPhone）——无法验证，先解决设备问题
- 纯动态分析需求（Frida 调试）——本 Skill 的动态方案受限（见工具链经验），需单独评估 Frida 可用性

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

### 1. 环境检查
- **做什么**：确认目标 IPA、工具链、测试设备可用
- **为什么**：注入插件的坑大部分在构建环境，先确认再动手
- **常用工具**：`Get-ChildItem`（文件确认）、`clang --version`、`gh auth status`
- **注意事项**：
  - 确认 IPA 是**解密版**（可用 Mach-O 加载器解析）
  - 确认 frida-server/TrollStore 环境状态（frida 在 RootHide 环境可能不可用）
  - 记录目标 App 的 bundle id 与版本号（后续判断版本差异）

### 2. 项目结构分析
- **做什么**：解包 IPA，列出主程序、Frameworks、Bundle 资源
- **为什么**：识别广告 SDK 家族（Frameworks 目录直接暴露技术栈）、定位主二进制
- **常用工具**：`tar -xf`、Python struct 解析 Mach-O
- **注意事项**：主程序是 `Payload/*.app/*`（无扩展名大文件）；Frameworks 里每个 framework 都是一个广告/功能 SDK

### 3. 目标程序分析（核心）
- **做什么**：IDA 反编译，梳理目标功能的完整触发链路
- **为什么**：只有搞清楚"从启动到展示"的全部调用链，才能确定 Hook 点覆盖范围
- **常用工具**：IDA Pro + ida-pro-mcp（decompile/xrefs/stub 调用者扫描/classref 扫描）
- **注意事项**：
  - 找**唯一出口**（如 TopOn 聚合的 `showSplashWithPlacementID:` 是所有渠道开屏的最终出口）
  - 用 **selref→__objc_stubs→调用者** 三层跳转找 OC 方法真实调用方（不能只看 selref 直接引用）
  - 用 **classref 扫描**列出"谁 alloc 了目标类"（本项目获得 235 个调用者全景）
  - 记录**服务端下发开关**（如 `isAdsWithAdCode:` 读取 advertiseList）——服务端可控的开关是最佳总闸

### 4. 确定注入方案
- **做什么**：选定 Hook 点层级（入口层/展示层/SDK 层/总开关），设计多级防护
- **为什么**：单点 Hook 可能漏（多个入口），总开关 + 出口双保险
- **注意事项**：
  - 高层入口（`showSplashAds`）+ SDK 出口（`ATAdManager.showSplashWithPlacementID`）+ 总开关（`isAdsWithAdCode:` 返回 NO）三层
  - **Hook 点必须逐一用 IDA 确认方法存在**（避免版本差异 MISS）
  - 区分"用户主动触发"（保留，如激励视频）与"自动弹出"（拦截）

### 5. 编写插件
- **做什么**：纯 C 源码 + objc runtime API + dlsym 符号解析
- **为什么**：减少链接依赖（undefined 符号在 weak 加载下风险高）
- **模板**：见下方"可复用模板 - Hook 模板"（⚠️ 模板代码需按目标 App 类名/方法名调整，不可直接复制）
- **注意事项**：
  - **Hook 时机**：constructor 只注册 `UIApplicationDidFinishLaunchingNotification` 观察者，全部 Hook 在启动完成后执行（constructor 早期直接调 objc runtime API 会 SIGILL）
  - **禁用栈保护**：编译加 `-fno-stack-protector`（Theos SDK tbd 缺 `___stack_chk_fail`）
  - **链接参数**：`-undefined dynamic_lookup`（stdio 等符号运行时解析）
  - **不要 hook 启动流程关键步骤**（如启动背景图 `setDelayStartBackgroundImageView`，置空导致启动卡住）

### 6. 调试验证
- **做什么**：注入后验证插件是否**真的加载并执行**
- **为什么**：TrollFools 用 `LC_LOAD_WEAK_DYLIB` 弱加载——**加载失败静默跳过**（无崩溃无日志），必须先建立可见验证
- **常用工具**：可见标记（启动弹窗/首页标题修改）、TrollFools 注入日志、.ips 崩溃日志
- **注意事项**：
  - **弹窗验证**是最可靠手段（`UIAlertView` + 2 秒 NSTimer 自动消失）
  - 弱加载失败的表现：无弹窗、无崩溃、无任何迹象
  - 崩溃报告在：设置 → 隐私与安全性 → 分析与改进 → 分析数据（.ips 格式：首行 JSON 元数据 + body）

### 7. 修复问题
- **做什么**：按崩溃/失效表现定位修复（见"常见问题与解决方案"）
- **为什么**：本项目 25 个版本迭代全部踩在 4 类问题上
- **方法**：**最小化二分**——只保留最小 Hook 集验证，逐批加回定位有问题的 Hook
- **注意事项**：一次只改一个变量；每个版本给用户**可验证的预期**（弹窗变没变）

### 8. 最终交付
- **做什么**：清理测试产物、写 README/LICENSE、GitHub 开源 + Release
- **为什么**：可复现 + 可维护
- **常用工具**：git、gh（`gh repo create --public --source . --push`、`gh release create v1.0.0 QSNoAds.dylib`）
- **注意事项**：README 必须包含**踩坑记录**（zig 结构问题等），这是别人（和未来的你）最需要的

---

## 工具链经验

| 工具 | 用途 | 适合解决的问题 |
|---|---|---|
| **IDA Pro + ida-pro-mcp** | 静态反编译、xrefs、stub/classref 扫描 | 找调用链、Hook 点、唯一出口 |
| **Python + struct** | Mach-O 解析（load commands/段/section/指针） | 验证二进制结构、发现链接器缺陷 |
| **LLVM clang + ld64.lld** | iOS 交叉编译 + 链接 | 本项目验证有效的构建方式（生成 chained fixups）——其他工具链需先验证结构 |
| **Theos iOS SDK** | 头文件 + tbd 库 | 无 Xcode 环境下的 iOS 编译 |
| **TrollFools** | dylib 注入（weak 加载到任意 framework） | 设备端注入与移除 |
| **.ips 崩溃日志** | 崩溃分析 | 崩溃线程/模块归属/异常类型 |
| **Frida + frida-server** | 动态 hook/内存操作 | ⚠️ 本项目不可用（gum 注入被拒/SIGTRAP，RootHide 环境兼容差）——动态验证受阻时改用**可见标记** |
| **git + gh** | 版本管理与发布 | 开源交付 |

---

## 逆向与开发经验

### 如何寻找关键代码位置
1. 从入口追链路：`AppDelegate didFinishLaunching → startProcedure → initAds → showSplashAds`
2. 用字符串反推：搜索功能关键词（`splash`、`shake`、`ads`）→ 找引用者
3. 用 **stub 扫描**找 OC 方法真实调用方（selref → `__objc_stubs` → cref 调用者）
4. 找**唯一出口**：所有渠道最终都经过的 SDK 方法（如 TopOn `showSplashWithPlacementID:`）

### 如何判断 Hook 点
- 优先：**总开关**（服务端可控的判定函数，如 `isAdsWithAdCode:`）
- 其次：**展示入口**（`showSplashAds` 等）
- 兜底：**SDK 最终出口**（聚合层 `ATAdManager.showSplashWithPlacementID:`）
- 确认方法存在：`class_getClassMethod/class_getInstanceMethod` 返回非空（运行时打印确认，避免 MISS）

### 如何验证修改是否生效
- **弹窗/UI 可见标记**（最强）：启动后弹 `UIAlertView`（2 秒自动消失）
- 首页标题修改（`setTitle:`）
- **禁止**依赖沙盒日志文件（iOS 沙盒 Documents 用户无法访问）
- 崩溃报告：仅崩溃时可用，正常运行无法确认

### 如何分析崩溃原因
- **EXC_BAD_INSTRUCTION (SIGILL)**：执行了非法指令——常见于 constructor 指针错误（rebase 缺失）或 runtime 未就绪时调用 objc API
- **EXC_BREAKPOINT (SIGTRAP)**：断点/断言——常见于 Frida gum 注入被拒或 assert
- **卡住（无崩溃）**：某 Hook 破坏了启动流程（如启动背景图流程被置空）
- 崩溃线程模块归属：看 `usedImages[i].name`，镜像名 `?` 且地址在 dyld 范围外 = 注入代码

### 如何处理版本差异
- Hook 前用 `class_getInstanceMethod` 检查，MISS 打印日志（不崩溃）
- 方法名以 IDA 当前版本为准；升级目标 App 后重新分析
- 多架构：arm64 单 slice 在 arm64e 设备兼容；如需更稳可做 arm64+arm64e fat

---

## 常见问题与解决方案

**问题 1：插件注入后完全无效果（无弹窗、无崩溃、无日志）**
- 原因：TrollFools 用 `LC_LOAD_WEAK_DYLIB` 弱加载——dyld 加载失败**静默跳过**，不报错
- 排查：① 确认注入日志完整（ct_bypass 签名成功）② 用弹窗验证是否加载 ③ 检查 dylib 结构
- 解决方案：修复 dylib 结构（见问题 2），或改强加载（改主程序 LC_LOAD_DYLIB 重打包）

**问题 2：zig 编译的 dylib 不被 dyld 接受（weak 加载静默失败）**
- 原因：zig 链接器产物缺 `CHAINED_FIXUPS` + `__init_offsets`（dyld 对 iOS 15+ 新链接库的期望结构），且 rebase 表为空
- 排查：解析 load commands——正常 dylib 应有 `LC_DYLD_CHAINED_FIXUPS(0x80000034)`；zig 产物是空 `LC_DYLD_INFO(0x26)`
- 解决方案：本项目验证有效的是 **LLVM clang + ld64.lld**；若使用其他工具链，先验证上述结构再继续；手动 Python 修补 rebase 属高危操作（易损坏 load commands）

**问题 3：注入后闪退（SIGILL @ constructor）**
- 原因：`__mod_init_func` 的 constructor 指针缺 image base（zig 空 rebase 表导致），dyld 跳到 0x7e8 之类错误地址
- 排查：崩溃日志 `qs_noads_init + 偏移`、检查 `__mod_init_func` 指针值是否含 `0x100000000` 基址
- 解决方案：换 lld 构建（生成正确 chained fixups）；或 Python 修指针+rebase（高风险，需逐字节验证）

**问题 4：手动修 rebase 后 load commands 全变 0x0**
- 原因：zig 的 `LC_DYLD_INFO` 只有 **16 字节**（仅 cmd/size/rebase_off/rebase_size，无 bind 字段）——修复脚本把 bind 字段写到下一条命令（SOURCE_VER）的位置
- 排查：dump load commands，第 N 条后全为 `cmd=0`
- 解决方案：只更新 rebase_off/rebase_size 两个字段（off+8/off+12），**绝不碰 off+16**（zig 16 字节 DYLD_INFO 没有该字段）

**问题 5：rebase 数据放文件末尾，重签后被丢弃**
- 原因：`__LINKEDIT` 段 filesize 未覆盖追加数据，TrollFools 重签按段大小重写，文件尾数据丢失
- 排查：`__LINKEDIT` filesize + fileoff ≠ 文件末尾
- 解决方案：追加数据后**必须更新 `__LINKEDIT` filesize**（或换 lld 构建一劳永逸）

**问题 6：constructor 里调用 objc runtime API 崩溃（SIGILL）**
- 原因：dyld initializer 阶段 runtime 未完全就绪，`class_getClassMethod` 等触发 libobjc assert
- 排查：崩溃堆栈在 constructor
- 解决方案：constructor 只做 `dlsym` 解析 + 注册 `UIApplicationDidFinishLaunchingNotification` 观察者，Hook 推迟到启动完成后

**问题 7：弹窗在注入早期崩溃**
- 原因：UIAlertView 在 App UI 环境未就绪时调用
- 解决方案：弹窗放在通知回调（启动完成后）；用 NSTimer 2 秒自动 dismiss

**问题 8：注入后启动页卡住**
- 原因：hook 了启动背景图流程（`setDelayStartBackgroundImageView`）——置空后背景图不移除
- 排查：二分法——只保留最小 Hook 集，逐批加回
- 解决方案：不 hook 该流程；确认某 Hook 导致卡住就用最小化二分定位

**问题 9：Frida attach/spawn 秒退（SIGTRAP）**
- 原因：frida-server 与 iOS 16/RootHide 环境兼容差（gum 注入崩溃），或 App 反调试
- 排查：崩溃线程模块归属（镜像名 `?` = 注入代码）；对照测试（attach 系统 App 也失败 = 环境问题）
- 解决方案：放弃 Frida 动态方案，改用**可见标记**（弹窗）做验证

**问题 10：注入后 app 崩（插件加载成功但 hook 执行崩）**
- 原因：hook 的方法调用方对返回值/行为有强依赖，或 hook 了启动关键路径
- 排查：崩溃日志定位崩溃线程；最小化二分
- 解决方案：延迟 hook 时机 + 逐批验证（先最小集，确认稳定再扩展）

---

## 可复用模板

> ⚠️ **重要**：以下模板为**参考框架**。目标 App 的类名、方法名、bundle id、SDK 出口必然不同，**必须基于实际逆向结果调整**，不可直接复制使用。模板中的 `p_xxx` 函数指针需对应实际 dlsym 的符号。

### 项目结构模板
```
project/
├── out.dylib        # 最终产物
├── src.c            # 源码
├── README.md        # 文档（含踩坑记录）
└── LICENSE          # MIT
```

### 初始化模板（符号解析 + 通知延迟）
```c
// 参考框架：需替换实际类名/符号
__attribute__((constructor))
static void init(void) {
    // 1. dlsym 解析所有 objc/Foundation 符号（全部运行时获取）
    *(void **)&p_objc_getClass = dlsym(RTLD_DEFAULT, "objc_getClass");
    *(void **)&p_objc_msgSend = dlsym(RTLD_DEFAULT, "objc_msgSend");
    // ... 其他符号（sel_registerName / class_getClassMethod / method_setImplementation / NSHomeDirectory 等）
    // 2. 注册 UIApplicationDidFinishLaunching 观察者（block）
    // 3. 通知回调里执行全部 Hook
}
```

### Hook 模板
```c
// 参考框架：类名/方法名按目标 App 调整；返回类型匹配原方法
static void stub_void(id self, SEL _cmd, ...) { }
static BOOL stub_no(id self, SEL _cmd, ...) { return NO; }

static void qs_hook_class(Class cls, const char *selName, IMP newImp) {
    if (!cls) return;
    Method m = p_class_getClassMethod(cls, p_sel_registerName(selName));
    if (m) p_method_setImplementation(m, newImp);
    // 记录 MISS（方法不存在 = 版本差异，需重新逆向确认）
}
```

### 调试日志模板
```c
#define LOG(fmt, ...) fprintf(stderr, "[PluginName] " fmt "\n", ##__VA_ARGS__)
// 每次 hook 成功/失败都 LOG，方便 stderr 排查
// 可见验证：UIAlertView + NSTimer 2 秒自动 dismiss（参考实现见下方要点）
```

### 发布流程模板
```bash
git init && git add . && git commit -m "Initial release: v1.0.0"
gh repo create NAME --public --source . --remote origin --push
gh release create v1.0.0 out.dylib --title "v1.0.0" --notes "使用说明"
```

---

## Agent 执行规则

1. **先分析后修改**：任何 Hook 点必须先用 IDA 确认方法存在与调用链，禁止凭猜测写 Hook
2. **每次修改保持可验证**：每个版本必须有用户可见的验证手段（弹窗/标题/行为变化），禁止"试试看"
3. **保留调试日志**：stderr 日志 + 落盘日志双保险，hook 成功/MISS 都要记录
4. **遇到未知行为优先逆向确认**：不猜——崩溃用 .ips 分析，失效用最小化二分，加载问题查结构
5. **不凭猜测修改关键逻辑**：rebase/load commands 等二进制级修改必须逐字节验证（对比 diff、dump 完整性）
6. **弱加载必须建立可见验证**：TrollFools weak 加载失败静默无痕，没有弹窗标记就等于没验证
7. **编译工具链先验证结构**：本项目验证有效的是 clang + ld64.lld；使用其他工具链时，先验证 `CHAINED_FIXUPS`/`__init_offsets` 存在再继续
8. **Hook 时机推迟**：constructor 只注册通知，Hook 放启动完成后（避免早期 SIGILL）
9. **最小化二分**：批量 Hook 失效/崩溃时，先最小集验证再逐批加回
10. **交付含失败经验**：README 必须记录踩坑（结构问题/时机问题），这是对使用者最大的价值
