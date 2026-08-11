# 真实案例复盘：趣智校园去开屏广告插件（QSNoAds）

> 本 Skill 的**全部经验沉淀来源**。25+ 版本迭代，横跨 zig 工具链缺陷、Mach-O 结构修补、Hook 时机调整、版本兼容等多类问题。此案例完整展示了 Skill 标准流程的执行方式。

## 1. Target Analysis（目标分析）

- **App**：趣智校园（iOS）
- **目标**：去掉开屏广告
- **环境**：TrollStore + TrollFools，iOS 16 + RootHide
- **技术栈**：广告聚合平台 **TopOn**（Frameworks 目录暴露），含多渠道广告 SDK

## 2. Reverse Engineering Findings（逆向发现）

- 主程序：`Payload/趣智校园.app/*`（无扩展名大文件）
- Frameworks：每个 framework 是一个广告/功能 SDK，直接暴露技术栈
- 调用链定位：从 `AppDelegate didFinishLaunching` → `startProcedure` → `initAds` → `showSplashAds`
- **唯一出口**：TopOn 聚合的 `ATAdManager.showSplashWithPlacementID:` —— 所有渠道开屏广告的最终出口
- **服务端总开关**：`isAdsWithAdCode:` 读取 advertiseList（服务端下发广告配置）
- 用 classref 扫描获得 235 个调用者全景，确认单点 Hook 覆盖不足

## 3. Hook Points（Hook 点）

三层防护设计：

| 层级 | Hook 点 | 作用 |
|---|---|---|
| 总开关 | `isAdsWithAdCode:` → 返回 NO | 服务端下发任何广告配置都被拦截 |
| 展示入口 | `showSplashAds` → 置空 | 业务层不触发开屏 |
| SDK 出口 | `showSplashWithPlacementID:` → 置空 | 兜底，覆盖所有渠道 |

判断原则：用户主动触发（激励视频）保留，自动弹出（开屏）拦截。

## 4. Implementation（实现）

- 纯 C + dlsym 运行时符号解析
- constructor 只做 dlsym + 注册 `UIApplicationDidFinishLaunchingNotification` 观察者
- 全部 Hook 在启动完成后执行（避免早期 SIGILL）
- 禁用栈保护（`-fno-stack-protector`，Theos SDK tbd 缺 `___stack_chk_fail`）
- 链接参数 `-undefined dynamic_lookup`（stdio 等符号运行时解析）

## 5. Build Result（构建结果与失败序列）

25+ 版本迭代的完整失败链（按时间顺序）：

1. **zig 构建 → 注入后完全无效果**：弱加载静默失败，无弹窗无崩溃无日志
2. **用弹窗验证**发现插件根本没加载 → 排查 dylib 结构
3. **发现 zig 产物缺 `CHAINED_FIXUPS` + `__init_offsets`**，rebase 表为空
4. **尝试 Python 手动修补 rebase** → load commands 全变 0x0（zig 的 `LC_DYLD_INFO` 只有 16 字节，无 bind 字段，脚本把 bind 写到了下一条命令位置）→ 只改 off+8/off+12 两字段
5. **rebase 数据放文件末尾 → 重签后被丢弃**：`__LINKEDIT` filesize 未覆盖追加数据，TrollFools 重签按段大小重写
6. **手动修补后 constructor SIGILL**：`__mod_init_func` 指针缺 image base，dyld 跳到 0x7e8 错误地址
7. **最终放弃手工修补，换 LLVM clang + ld64.lld** → 一次性生成正确结构，问题 3/4/5/6 全部消失
8. **constructor 调 objc API 崩溃**（SIGILL）→ Hook 推迟到启动完成后
9. **hook 启动背景图流程导致启动页卡死** → 不 hook 该流程
10. **Frida attach/spawn 秒退**（SIGTRAP，iOS16 + RootHide 兼容差）→ 放弃动态方案，改用弹窗可见验证

## 6. Test Result（测试结果）

- 验证方法：启动弹窗（UIAlertView + 2 秒 NSTimer 自动消失）+ 首页标题修改
- 最终版本验证通过：开屏广告消失、App 正常启动、无崩溃
- 每次迭代给用户**可验证的预期**（弹窗变没变）

## 7. Known Issues（已知问题）

- Frida 动态调试在 iOS 16 + RootHide 环境不可用（gum 注入 SIGTRAP）
- 沙盒日志文件用户无法访问，只能依赖 UI 可见标记
- 目标 App 升级后方法名可能变化，需要重新逆向

## 8. Maintenance Notes（维护记录）

- 版本差异：Hook 前 `class_getInstanceMethod` 检查，MISS 打印日志（不崩溃）
- 工具链：**坚持 clang + ld64.lld**，不再尝试手工修补 Mach-O
- README 记录全部踩坑经历——这是对后续使用者最大的价值

## 交付物

- `QSNoAds.dylib`（arm64，可注入）
- 源码 + README（含完整踩坑记录）+ LICENSE（MIT）
- GitHub 开源 + Release（dylib 附件）
