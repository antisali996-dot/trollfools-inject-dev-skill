# trollfools-inject-dev-skill

TrollFools 注入插件开发的 AI Agent Skill（openai/opencode skill 格式）。

面向 iOS 注入插件（dylib）开发的完整方法论，覆盖**逆向分析、Hook 实现、交叉编译、注入调试、发布**全链路。沉淀自「趣智校园去开屏广告插件」真实项目（25+ 版本迭代，含大量失败经验）。

## 适用场景

- 为 iOS App 开发去广告/去校验/功能增强 dylib
- 分析 App 内部机制（广告触发、越狱检测、内购校验等）
- 在 TrollStore / TrollFools 环境下注入插件
- 在无 macOS/Xcode 环境下交叉编译 iOS dylib（Windows/Linux）

## 核心经验

1. **TrollFools 注入是 weak 加载，失败静默无痕**（`LC_LOAD_WEAK_DYLIB`）——必须建立可见验证（弹窗/标题）
2. **iOS dylib 必须用 clang + ld64.lld 构建**——生成 `CHAINED_FIXUPS` + `__init_offsets` 结构；zig 等工具链产物会被 dyld 静默拒绝
3. **Hook 时机推迟到启动完成后**——constructor 阶段调用 objc runtime API 会 SIGILL
4. **Hook 点选择顺序**：总开关（服务端判定函数）→ 展示入口 → SDK 唯一出口
5. **崩溃分析看 .ips 报告**：SIGILL = constructor 指针/rebase 问题；SIGTRAP = Frida 注入被拒
6. **最小化二分排错**——批量 Hook 失效时先最小集验证再逐批加回

## 文件

- `SKILL.md` — 完整 skill 定义（含标准工作流 SOP、常见问题与解决方案、可复用模板、Agent 执行规则）

## 安装

将 `SKILL.md` 放入 opencode 的 skills 目录：

```
~/.config/opencode/skills/trollfools-inject-dev/SKILL.md
```

## 技术栈

iOS / TrollStore / TrollFools / dylib / Objective-C runtime / LLVM clang + ld64.lld / IDA Pro

## License

MIT
