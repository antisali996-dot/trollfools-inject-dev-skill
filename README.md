# trollfools-inject-dev

> 面向 AI Agent 的 **TrollFools 注入插件开发** Skill——为 iOS App 制作去广告/功能修改 dylib 的完整方法论。

沉淀自「趣智校园去开屏广告插件」真实项目（25+ 版本迭代，含全部失败经验），覆盖**逆向分析 → Hook 设计 → 交叉编译 → 注入调试 → 发布**全链路。

## 功能

- 目标 App 机制分析（广告 SDK/聚合平台识别、触发链路梳理）
- 多层级 Hook 方案设计（总开关 / 展示入口 / SDK 出口）
- iOS dylib 交叉编译（clang + ld64.lld，chained fixups）
- 注入调试与崩溃排查（.ips 分析、最小化二分、可见标记验证）
- 可编译参考模板（hook.c / build.sh / check_macho.py）
- 真实案例复盘（examples/qs-noads-case.md）

## 安装方式

```bash
# opencode
mkdir -p ~/.config/opencode/skills/trollfools-inject-dev
cp -r . ~/.config/opencode/skills/trollfools-inject-dev/

# Codex
mkdir -p ~/.codex/skills/trollfools-inject-dev
cp -r . ~/.codex/skills/trollfools-inject-dev/
```

## 使用示例

```
为 [App 名称] 开发一个去开屏广告的 TrollFools 插件
```

Agent 将自动执行：Before Start Checklist → Decision Tree → SOP 8 步 → Final Report。

## 项目结构

```
trollfools-inject-dev/
├── SKILL.md                    # 核心流程 + 决策入口（v2.0.0）
├── README.md
├── LICENSE                     # MIT
├── references/
│   ├── macho-debug.md          # Mach-O、dyld、rebase、load commands 深度排错
│   ├── ida-workflow.md         # IDA 分析流程
│   ├── hook-design.md          # Hook 点设计方法
│   └── troubleshooting.md      # 崩溃和失败排查（10 个常见问题）
├── templates/
│   ├── hook.c                  # Hook 源码模板
│   ├── build.sh                # 构建脚本模板
│   └── check_macho.py          # Mach-O 结构验证脚本
└── examples/
    └── qs-noads-case.md        # 趣智校园真实案例复盘
```

## 更新记录

- **v2.0.0**（结构重构）：SKILL.md 增加 Before Start Checklist / Decision Tree / Anti Patterns / Final Report Format / Maintenance；拆分 references/templates/examples 目录；全部经验零丢失迁移
- **v1.0.0**：初始版本，单文件 SKILL.md

## 许可证

MIT License
