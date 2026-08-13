# UI / 可见诊断参考

> 本文件解决一个核心问题：**TrollFools 注入环境中，如何在不依赖系统日志、不依赖 Frida 的前提下确认 dylib 与 Hook 的真实状态**。沉淀自 5EPlay 去开屏广告插件的实战验证。

## 1. TrollFools dylib 为什么需要可见诊断

TrollFools 通过 `LC_LOAD_WEAK_DYLIB` 弱加载，加载失败静默无痕。用户侧还可能：

- 看不到 iOS Console / 系统日志
- 不方便使用 Frida（iOS 16 + RootHide 下不可靠）
- 无法判断 dylib 是否真正加载
- 无法判断 Hook 是否真正执行

因此**开发版本必须至少提供一个可验证信号**，且这个信号不能依赖「用户能看日志」这个假设。

## 2. 三层信号

所有诊断围绕三个严格分层的信号展开：

| 信号 | 含义 | 证明方式 | 不证明 |
|---|---|---|---|
| **PLUGIN LOADED** | dylib constructor / 初始化入口已执行 | constructor 入口输出 | Hook 已安装、Hook 已命中 |
| **HOOK INSTALLED** | Class / Method 找到，Implementation 已替换 | `class_getClassMethod`/`class_getInstanceMethod` 非空 + `method_setImplementation` 成功 | 目标 selector 真正被执行 |
| **HOOK HIT #N** | 目标 selector 实际执行，Hook 函数真实进入 | Hook 函数**内部**输出（计数递增） | —— |

铁律：

> **HOOK INSTALLED ≠ HOOK HIT**

禁止在注册代码里输出 HIT 类字样（例如 `method_setImplementation` 成功后打印 "HOOK HIT"）——那只是 INSTALLED。

## 3. UIKit 线程

**UIKit UI 操作不能假设当前 callback 位于主线程。**

以下回调都不保证在主线程：

- `CFNotificationCenterGetDarwinNotifyCenter()` 的观察者回调（Darwin 通知在通知服务线程）
- `NSNotificationCenter` 任意通知的观察者回调（posting 线程）
- dylib 内其他 runtime callbacks

特别注意：

> `NSNotificationCenter` 的 `addObserver:selector:name:object:`（`queue:nil`）回调运行在 **posting thread**，并不等价于固定主线程。

**任何 UIKit 操作（弹窗、改标题、UI 状态更新）必须切到主线程：**

```objc
dispatch_async(dispatch_get_main_queue(), ^{
    // UIKit / UIAlertView / 状态更新
});
```

## 4. UIAlertView 的正确定位

- 已 deprecated，不作为现代 App UI 推荐。
- 在 **JB/TrollFools 短时诊断提示**场景，如果**某个目标系统上已经验证可用**，可以作为低复杂度方案。
- **必须主线程**调用。
- 不要声称「所有 iOS 14–17 都可靠」——按实际验证过的系统范围表述。

## 5. 不要过度设计 UIWindow

为了显示一句调试信息，**不应该第一时间创建复杂 UIWindow / UIWindowScene / overlay 系统**。

优先级从高到低：

```
当前项目已有验证实现
↓
Skill references/templates 中已有验证实现
↓
用户提供的参考实现
↓
最后才自行设计
```

即：**先复用，再设计**。参考实现的优先级永远高于自己新造的 UI 系统。

## 6. Hook HIT 计数

`HOOK HIT #1`、`HOOK HIT #2`、`HOOK HIT #3` 计数可以用于：

- 观察多次触发
- 冷启动 / 热启动对照（启动路径不同，HIT 次数与时机不同）
- 判断 Hook 是否重复进入（同一行为是否被多次执行）

但注意：

> 不能仅凭 HIT 次数断言某一次一定是 cold 或 hot——生命周期判断仍需调用链分析或运行时状态支持。

## 7. 无日志环境 fallback

如果用户无法查看 log，支持**持久化诊断**，例如记录：

```
PLUGIN_LOADED
HOOK_INSTALLED
HOOK_HIT
```

实现方式可选用：

- 文件
- NSUserDefaults
- 当前项目已有的可读状态机制

注意：

> 不要假设某个特定绝对路径始终存在（例如沙盒 Documents 用户无法访问；不要硬编码本机路径）。

选型原则：用户**能实际读取**的机制 > 逻辑上存在但用户拿不到的机制。
