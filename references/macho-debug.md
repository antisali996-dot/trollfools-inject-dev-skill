# Mach-O / dyld 深度排错参考

> 本文件是本项目 25+ 版本迭代中**最核心的坑**的完整技术细节。**注入插件的坑大部分在构建环境**，先理解这里的内容再动手。

## 核心机制：TrollFools 弱加载

TrollFools 通过 `LC_LOAD_WEAK_DYLIB` 将插件 dylib 弱加载到目标 App。弱加载的致命特性：

- **加载失败时 dyld 直接跳过，无崩溃、无日志**
- 结果：「注入成功但没效果」无法区分是「没加载」还是「hook 没生效」
- **因此必须建立可见验证**（弹窗/标题），否则无从排错

## 工具链结论（最重要）

| 工具链 | 结论 |
|---|---|
| **LLVM clang + ld64.lld** | ✅ 本项目验证有效，生成正确的 `CHAINED_FIXUPS` + `__init_offsets` |
| **zig** | ❌ 产物缺 `CHAINED_FIXUPS` + `__init_offsets`，rebase 表为空，被 dyld 静默拒绝 |
| **Theos iOS SDK** | 提供头文件 + tbd 库，无 Xcode 环境下编译 iOS 代码用 |

**铁律：使用任何其他工具链时，先验证产物结构（`CHAINED_FIXUPS`/`__init_offsets` 存在）再继续。**

## Mach-O 关键结构

### Load Commands
- `LC_DYLD_CHAINED_FIXUPS (0x80000034)`：正常 dylib 应有此命令，包含 chained fixups（iOS 15+ 新链接库的期望结构）
- `LC_DYLD_INFO (0x26)`：旧结构。zig 产物是**空**的 `LC_DYLD_INFO`——这是问题根源
- `LC_LOAD_WEAK_DYLIB`：TrollFools 注入的加载方式

### 验证命令（Python struct 解析）
```python
# 关键验证点：
# 1. 是否有 LC_DYLD_CHAINED_FIXUPS (0x80000034)
# 2. __init_offsets 是否存在
# 3. __mod_init_func 指针值是否含 0x100000000 基址
```

### `__mod_init_func` 与 constructor
- `__mod_init_func` 段保存 constructor 函数指针
- **zig 空 rebase 表导致 constructor 指针缺 image base**，dyld 跳到 0x7e8 之类错误地址 → SIGILL
- 崩溃日志表现为 `qs_noads_init + 偏移`

### `__LINKEDIT` 段
- 追加 rebase 数据后**必须更新 `__LINKEDIT` filesize**
- 否则 TrollFools 重签按段大小重写，文件尾数据被丢弃

## 已知陷阱（踩坑记录）

### 陷阱 1：zig 编译的 dylib 不被 dyld 接受
- **现象**：weak 加载静默失败
- **原因**：缺 `CHAINED_FIXUPS` + `__init_offsets`，且 rebase 表为空
- **排查**：解析 load commands——正常应有 `LC_DYLD_CHAINED_FIXUPS(0x80000034)`；zig 产物是空 `LC_DYLD_INFO(0x26)`
- **解决**：用 LLVM clang + ld64.lld；若必须用其他工具链，先验证上述结构；手动 Python 修补 rebase 属高危操作（易损坏 load commands）

### 陷阱 2：注入后闪退（SIGILL @ constructor）
- **原因**：`__mod_init_func` 的 constructor 指针缺 image base（zig 空 rebase 表导致），dyld 跳到错误地址
- **排查**：崩溃日志 `qs_noads_init + 偏移`、检查 `__mod_init_func` 指针值是否含 `0x100000000` 基址
- **解决**：换 lld 构建（生成正确 chained fixups）；或 Python 修指针+rebase（高风险，需逐字节验证）

### 陷阱 3：手动修 rebase 后 load commands 全变 0x0
- **原因**：zig 的 `LC_DYLD_INFO` 只有 **16 字节**（仅 cmd/size/rebase_off/rebase_size，无 bind 字段）——修复脚本把 bind 字段写到下一条命令（SOURCE_VER）的位置
- **排查**：dump load commands，第 N 条后全为 `cmd=0`
- **解决**：只更新 rebase_off/rebase_size 两个字段（off+8/off+12），**绝不碰 off+16**（zig 16 字节 DYLD_INFO 没有该字段）

### 陷阱 4：rebase 数据放文件末尾，重签后被丢弃
- **原因**：`__LINKEDIT` 段 filesize 未覆盖追加数据，TrollFools 重签按段大小重写，文件尾数据丢失
- **排查**：`__LINKEDIT` filesize + fileoff ≠ 文件末尾
- **解决**：追加数据后**必须更新 `__LINKEDIT` filesize**（或换 lld 构建一劳永逸）

## 崩溃分类（dyld 层面原理）

| 异常 | 含义 | 常见原因 |
|---|---|---|
| **EXC_BAD_INSTRUCTION (SIGILL)** | 执行了非法指令 | constructor 指针错误（rebase 缺失）或 runtime 未就绪时调用 objc API |
| **EXC_BREAKPOINT (SIGTRAP)** | 断点/断言 | Frida gum 注入被拒 或 assert |
| **卡住（无崩溃）** | 启动流程被破坏 | hook 了启动关键步骤（如启动背景图流程被置空） |

崩溃线程模块归属判断：`usedImages[i].name` 为 `?` 且地址在 dyld 范围外 = 注入代码。
