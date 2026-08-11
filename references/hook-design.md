# Hook 点设计方法参考

> 本文件沉淀自「趣智校园去开屏广告插件」项目。核心原则：**单点 Hook 可能漏（多个入口），总开关 + 出口双保险**。

## 多层级 Hook 设计

按优先级从高到低：

| 层级 | 说明 | 示例 |
|---|---|---|
| **总开关** | 服务端可控的判定函数，Hook 后一劳永逸 | `isAdsWithAdCode:` 返回 NO |
| **展示入口** | 业务层的广告展示方法 | `showSplashAds` |
| **SDK 最终出口** | 聚合层所有渠道必经的最终方法 | `ATAdManager.showSplashWithPlacementID:` |

本项目采用三层防护：
1. **总开关**：`isAdsWithAdCode:` 返回 NO（服务端下发广告配置也被拦截）
2. **展示入口**：`showSplashAds` 置空
3. **SDK 出口**：`showSplashWithPlacementID:` 置空（兜底，覆盖所有渠道）

## 如何判断 Hook 点

- **优先**：总开关（服务端可控的判定函数）
- **其次**：展示入口（业务层方法）
- **兜底**：SDK 最终出口（聚合层方法）
- 区分「用户主动触发」（保留，如激励视频）与「自动弹出」（拦截）——不要一刀切

## 确认方法存在

Hook 前用运行时检查确认方法存在，避免 MISS：

```c
Method m = p_class_getClassMethod(cls, p_sel_registerName(selName));
// 或
Method m = p_class_getInstanceMethod(cls, p_sel_registerName(selName));
if (!m) {
    // 方法不存在 = 版本差异，记录 MISS，不崩溃
}
```

## Hook 时机规则

**constructor 阶段禁止调用 objc runtime API**——dyld initializer 阶段 runtime 未完全就绪，`class_getClassMethod` 等触发 libobjc assert 导致 SIGILL。

正确做法：
1. constructor 只做 `dlsym` 符号解析 + 注册 `UIApplicationDidFinishLaunchingNotification` 观察者
2. 全部 Hook 在启动完成后（通知回调内）执行

## 版本差异处理

- Hook 前用 `class_getInstanceMethod` / `class_getClassMethod` 检查，MISS 打印日志（不崩溃）
- 方法名以 IDA 当前版本为准
- 目标 App 升级后必须重新逆向分析
- 多架构：arm64 单 slice 在 arm64e 设备兼容；如需更稳可做 arm64+arm64e fat

## 已知禁忌

- 不要 hook 启动流程关键步骤（如启动背景图 `setDelayStartBackgroundImageView`，置空会导致启动页卡死）
- 不要 constructor 早期执行复杂逻辑
- 不要一次加入大量 Hook（先最小集验证再逐批加回）
