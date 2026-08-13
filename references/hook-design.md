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

## Runtime Hook Verification

Hook 验证是分层的，**静态确认、安装成功、真实命中是三个不同层级的事实**。完整验证流程：

```text
静态确认（IDA 方法存在、有真实调用方）
↓
Class / Method 存在（运行时 class_getClassMethod / class_getInstanceMethod）
↓
IMP 安装（method_setImplementation 成功）
↓
HOOK INSTALLED
↓
运行时 selector 真正执行（Hook 函数内部进入）
↓
HOOK HIT
↓
确认目标行为是否改变（真机观察）
```

禁止：

> 仅凭 IDA XREF 或 method_setImplementation 成功就声称 Hook 已验证。

铁律：**HOOK INSTALLED ≠ HOOK HIT**。「注册成功」只到 INSTALLED 这一层；「真命中」必须来自 Hook 函数本身的执行。

## Evidence Level（证据分级）

所有结论按以下分级标注，避免把「推测」说成「已验证」：

| 级别 | 含义 | 示例 |
|---|---|---|
| **L0** | 推测（经验或猜测），不得称「已确认」 | 「可能有个总开关方法」 |
| **L1** | 静态逆向确认 | IDA 反编译确认方法存在、调用链完整 |
| **L2** | Hook 安装成功 | 运行时确认 Class/Method 存在，IMP 已替换（HOOK INSTALLED） |
| **L3** | 运行时 Hook 真命中 | 目标 selector 执行，Hook 函数 body 进入（HOOK HIT） |
| **L4** | 真机完整行为验证 | 真机上观察到目标行为按预期改变（如开屏广告消失） |

**L4 只对当前目标 App、当前 App 版本、当前测试设备/系统环境及当前测试场景成立。旧版本、其他设备、其他系统环境或类似 App 的 L4 结论不能自动继承给当前目标；需要重新执行相应级别的验证。**

核心语义：

```text
L1 静态证据
≠
L3 运行时命中
≠
L4 当前目标真机完整验证
```

约定：

- 报告中每个关键结论标注证据级别（L0-L4）
- 无 HIT 证据（L3）不得宣称 Hook 生效
- 偶发行为（「有时没广告」）**不是** L4 证据——必须结合 HIT（L3）+ 输入数据 + 真实展示路径综合判断

## 已知禁忌

- 不要 hook 启动流程关键步骤（如启动背景图 `setDelayStartBackgroundImageView`，置空会导致启动页卡死）
- 不要 constructor 早期执行复杂逻辑
- 不要一次加入大量 Hook（先最小集验证再逐批加回）
- 不要在「没有运行时证据」时堆 Hook——确认 HIT → 查下游 → 找真实路径 → 再决定第二个 Hook
- 不要用「偶发没广告」证明去广告 Hook 成功——必须有 Hook HIT 或等价证据
