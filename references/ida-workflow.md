# IDA 分析流程参考

> 本文件沉淀自「趣智校园去开屏广告插件」项目，用于在 IDA Pro 中定位目标功能的完整触发链路。所有方法均经真实项目验证。

## 工具配置

- **IDA Pro + ida-pro-mcp**：静态反编译、xrefs、stub/classref 扫描
- 常用 MCP 调用：`decompile`（反编译函数）、`xrefs_to`（交叉引用）、`entity_query`（类/函数/字符串查询）、`find`（字符串/立即数搜索）
- 主二进制定位：`Payload/*.app/*`（无扩展名大文件）；Frameworks 里每个 framework 都是一个广告/功能 SDK

## 如何寻找关键代码位置

### 1. 从入口追链路（自上而下）
```
AppDelegate didFinishLaunching → startProcedure → initAds → showSplashAds
```
以 App 生命周期入口为起点，沿函数调用逐层向下，直到找到目标功能的实际执行点。

### 2. 用字符串反推（自下而上）
搜索功能关键词（`splash`、`shake`、`ads`、`interstitial` 等）→ 找到引用这些字符串的函数 → 从引用者继续向上/向下扩展。

### 3. 用 stub 扫描找 OC 方法真实调用方
Objective-C 方法调用不能只看 selref 直接引用。标准链路是：

```
selref → __objc_stubs → 调用者（真实方法调用方）
```

三层跳转步骤：
1. 找到目标 selector 的 `selref` 引用
2. 追踪到 `__objc_stubs` 段的 stub 函数
3. 从 stub 函数找出所有调用它的真实调用方

只查 selref 直接引用会漏掉大量间接调用者。

### 4. 用 classref 扫描列出「谁 alloc 了目标类」
扫描 classref 引用，列出所有初始化/使用目标类的调用者全景。本项目通过该方法获得 235 个调用者的完整视图，从而判断单点 Hook 覆盖是否足够。

### 5. 找「唯一出口」
所有渠道/分支最终都经过的 SDK 方法是注入的最佳 Hook 点。例如 TopOn 聚合的 `showSplashWithPlacementID:` 是所有渠道开屏广告的最终出口——无论上层有多少入口，最后都必须经过这里。

## 记录服务端下发开关

逆向时注意识别**服务端可控的判定函数**。例如 `isAdsWithAdCode:` 读取 advertiseList（服务端下发的广告配置）。这类函数是最佳总开关：Hook 它返回固定值，服务端后续下发任何广告配置都会被拦截。

## 确认 Hook 点方法存在

任何候选 Hook 点必须先用 IDA 逐一确认方法存在：
- 搜索方法名对应的 selector / stub
- 确认该 selector 有真实调用方（不是死代码）
- 版本差异会导致 MISS——以当前目标 App 版本为准，升级后重新分析

## 交付要求

分析完成后必须产出：
1. 目标功能的完整触发链路（从启动到展示的全部调用链）
2. Hook 点清单（含层级标注：入口层/展示层/SDK 出口/总开关）
3. 每个 Hook 点的 IDA 证据（方法存在的确认记录）
