# 崩溃和失败排查参考

> 本文件沉淀自「趣智校园去开屏广告插件」25+ 版本迭代，全部失败经验与解决方案。遇到问题时按此表逐条排查。

## 排查方法论

### 最小化二分法
批量 Hook 失效/崩溃时：
1. 只保留最小 Hook 集验证（单个 Hook）
2. 确认稳定后逐批加回
3. 一次只改一个变量

### 可见标记验证（最重要的方法）
TrollFools 用 `LC_LOAD_WEAK_DYLIB` 弱加载——**加载失败静默跳过**（无崩溃无日志）。必须先建立可见验证：
- **弹窗验证**是最可靠手段（`UIAlertView` + 2 秒 NSTimer 自动消失）
- 首页标题修改（`setTitle:`）
- **禁止**依赖沙盒日志文件（iOS 沙盒 Documents 用户无法访问）
- 崩溃报告：仅崩溃时可用，正常运行无法确认
- 弱加载失败的表现：无弹窗、无崩溃、无任何迹象

### .ips 崩溃日志
崩溃报告位置：设置 → 隐私与安全性 → 分析与改进 → 分析数据（.ips 格式：首行 JSON 元数据 + body）
- 崩溃线程模块归属：看 `usedImages[i].name`，镜像名 `?` 且地址在 dyld 范围外 = 注入代码

## 常见问题与解决方案

### 问题 1：插件注入后完全无效果（无弹窗、无崩溃、无日志）
- **原因**：TrollFools 用 `LC_LOAD_WEAK_DYLIB` 弱加载——dyld 加载失败**静默跳过**，不报错
- **排查**：① 确认注入日志完整（ct_bypass 签名成功）② 用弹窗验证是否加载 ③ 检查 dylib 结构
- **解决**：修复 dylib 结构（见 macho-debug.md），或改强加载（改主程序 LC_LOAD_DYLIB 重打包）

### 问题 2：zig 编译的 dylib 不被 dyld 接受
- **原因**：zig 链接器产物缺 `CHAINED_FIXUPS` + `__init_offsets`（dyld 对 iOS 15+ 新链接库的期望结构），且 rebase 表为空
- **排查**：解析 load commands——正常 dylib 应有 `LC_DYLD_CHAINED_FIXUPS(0x80000034)`；zig 产物是空 `LC_DYLD_INFO(0x26)`
- **解决**：用 **LLVM clang + ld64.lld**；其他工具链先验证结构再继续；手动 Python 修补 rebase 属高危操作（易损坏 load commands）
- **详见**：references/macho-debug.md 陷阱 1

### 问题 3：注入后闪退（SIGILL @ constructor）
- **原因**：`__mod_init_func` 的 constructor 指针缺 image base（zig 空 rebase 表导致），dyld 跳到 0x7e8 之类错误地址
- **排查**：崩溃日志 `qs_noads_init + 偏移`、检查 `__mod_init_func` 指针值是否含 `0x100000000` 基址
- **解决**：换 lld 构建（生成正确 chained fixups）；或 Python 修指针+rebase（高风险，需逐字节验证）
- **详见**：references/macho-debug.md 陷阱 2

### 问题 4：手动修 rebase 后 load commands 全变 0x0
- **原因**：zig 的 `LC_DYLD_INFO` 只有 **16 字节**（仅 cmd/size/rebase_off/rebase_size，无 bind 字段）——修复脚本把 bind 字段写到下一条命令（SOURCE_VER）的位置
- **排查**：dump load commands，第 N 条后全为 `cmd=0`
- **解决**：只更新 rebase_off/rebase_size 两个字段（off+8/off+12），**绝不碰 off+16**（zig 16 字节 DYLD_INFO 没有该字段）
- **详见**：references/macho-debug.md 陷阱 3

### 问题 5：rebase 数据放文件末尾，重签后被丢弃
- **原因**：`__LINKEDIT` 段 filesize 未覆盖追加数据，TrollFools 重签按段大小重写，文件尾数据丢失
- **排查**：`__LINKEDIT` filesize + fileoff ≠ 文件末尾
- **解决**：追加数据后**必须更新 `__LINKEDIT` filesize**（或换 lld 构建一劳永逸）
- **详见**：references/macho-debug.md 陷阱 4

### 问题 6：constructor 里调用 objc runtime API 崩溃（SIGILL）
- **原因**：dyld initializer 阶段 runtime 未完全就绪，`class_getClassMethod` 等触发 libobjc assert
- **排查**：崩溃堆栈在 constructor
- **解决**：constructor 只做 `dlsym` 解析 + 注册 `UIApplicationDidFinishLaunchingNotification` 观察者，Hook 推迟到启动完成后
- **详见**：references/hook-design.md「Hook 时机规则」

### 问题 7：弹窗在注入早期崩溃
- **原因**：UIAlertView 在 App UI 环境未就绪时调用
- **解决**：弹窗放在通知回调（启动完成后）；用 NSTimer 2 秒自动 dismiss

### 问题 8：注入后启动页卡住
- **原因**：hook 了启动背景图流程（`setDelayStartBackgroundImageView`）——置空后背景图不移除
- **排查**：二分法——只保留最小 Hook 集，逐批加回
- **解决**：不 hook 该流程；确认某 Hook 导致卡住就用最小化二分定位

### 问题 9：Frida attach/spawn 秒退（SIGTRAP）
- **原因**：frida-server 与 iOS 16/RootHide 环境兼容差（gum 注入崩溃），或 App 反调试
- **排查**：崩溃线程模块归属（镜像名 `?` = 注入代码）；对照测试（attach 系统 App 也失败 = 环境问题）
- **解决**：放弃 Frida 动态方案，改用**可见标记**（弹窗）做验证

### 问题 10：注入后 app 崩（插件加载成功但 hook 执行崩）
- **原因**：hook 的方法调用方对返回值/行为有强依赖，或 hook 了启动关键路径
- **排查**：崩溃日志定位崩溃线程；最小化二分
- **解决**：延迟 hook 时机 + 逐批验证（先最小集，确认稳定再扩展）

## 信号分层排查（四类情况）

> 用三层信号（PLUGIN LOADED / HOOK INSTALLED / HOOK HIT）定位问题，详见 ui-diagnostics.md。**不要看到行为异常就直接判断「Hook 点错了」。**

### 情况 1：没有任何提示（无弹窗、无 HIT、无日志）

按顺序逐层确认，定位断在哪一层：

```text
PLUGIN LOADED?（constructor 是否执行 → dylib 是否加载）
↓
HOOK INSTALLED?（Class/Method 是否找到，IMP 是否替换）
↓
HOOK HIT?（目标 selector 是否真正执行）
```

### 情况 2：PLUGIN LOADED 有，HOOK HIT 没有

可能原因：

```text
Method 不存在（MISS）
版本差异（App 升级后方法名变化）
IMP 被覆盖（其他插件/代码路径重复安装）
实际路径不同（目标类来自 Framework 而非主程序）
目标 selector 没执行（展示路径没走到这个点）
Hook 时机不对（Hook 安装晚于首次调用）
```

### 情况 3：HOOK HIT 有，但目标行为仍然存在

按以下顺序排查，**不要马上增加第二个 Hook**：

```text
HOOK HIT（Hook 确实命中了）
↓
Hook 后是否继续执行下游（置空/改返回值后，原流程是否仍被继续调用）
↓
异步 callback / delegate / notification（结果由另一个线程/对象补上）
↓
另一对象（同一个行为由另一个实例完成）
↓
另一展示入口（存在第二个展示路径，本 Hook 点只是其一）
↓
真实行为路径（重新逆向确认真正的执行路径）
```

### 情况 4：目标行为偶尔出现、偶尔不出现

> **偶发行为本身不能证明 Hook 成功。**

「有时候有广告、有时候没广告」不能直接归因于 Hook 生效，可能因素包括：

```text
服务端不下发
频控
缓存
网络
广告填充失败
随机策略
其他执行路径
Hook 真命中（L3）
```

必须结合三者判断：

```text
Hook HIT（L3 证据）
+ 输入数据（服务端下发内容、参数）
+ 真实展示路径（逆向确认的完整链路）
```

## Frida 使用注意

Frida + frida-server 在本类环境（iOS 16 + RootHide）**不可靠**——gum 注入 SIGTRAP。动态验证受阻时**改用可见标记方案**（弹窗验证）。若需使用 Frida，先做对照测试判断是环境问题还是 App 反调试。
