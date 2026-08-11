# AGENTS.md — 给 AI Agent 看的仓库说明

本文件是给 **AI Agent（opencode / Claude Code 等）** 阅读的。你是 Agent 时，请先读完本节再操作本仓库。

## 这是什么

`trollfools-inject-dev-skill` 是一个 **Skill 定义仓库**：核心产物是 `SKILL.md`（一套 iOS TrollFools 注入插件开发的完整方法论），README.md 是给人类看的介绍。本仓库是 GitHub 同步发布版本，**本地主版本**位于 `~/.config/opencode/skills/trollfools-inject-dev/`。

## 仓库结构

```
trollfools-inject-dev/
├── SKILL.md                    # 核心产物：完整 skill 定义（v2.0.0，决策树+SOP+规则）
├── AGENTS.md                   # 本文件：给 Agent 的说明
├── README.md                   # 给人类的介绍（精简版）
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

## 你的职责（维护/发布 skill 时）

### 更新 SKILL.md 的规则

1. **不得破坏 frontmatter**：`name` / `description` / `version` / `tags` 是 opencode 加载 skill 的依据。改版时递增 `version`。
2. **description 要写清楚触发条件**：description 决定了 skill 何时被自动触发，改动时确保包含关键词（ios / trollstore / trollfools / dylib / hook / 逆向 / 注入 / 去广告）。
3. **经验类内容只增不减**：踩坑记录是对使用者最大的价值，删除前需确认该经验已被新经验替代。所有迁移必须知识零丢失。
4. **模板必须带警告**：`templates/` 下代码模板是**经过验证的可编译参考模板**，类名/方法名按目标 App 不同。新增模板时必须附"需按目标 App、SDK、架构和环境调整"的警告。

### 同步规则

- **本地为主版本**：先改 `~/.config/opencode/skills/trollfools-inject-dev/`，再同步到本仓库。
- 同步命令：删除本仓库全部内容，将本地主版本完整复制过来，更新 AGENTS.md 后再提交。

### 提交规范

- commit message 用中文或英文均可，格式：`type: 描述`（如 `docs: 补充 AGENTS.md`、`feat: SKILL.md v1.1.0 新增 xx 经验`）。
- 修改 `SKILL.md` 时 commit message 必须标注新的 version。

## 用户若要求"安装这个 skill"

把整个目录（SKILL.md + references + templates + examples）复制到目标机器的 skills 目录：

- Windows: `%USERPROFILE%\.config\opencode\skills\trollfools-inject-dev\`
- macOS/Linux: `~/.config/opencode/skills/trollfools-inject-dev/`

安装目录名（`trollfools-inject-dev`）必须与 SKILL.md frontmatter 的 `name` 一致。

## 用户若要求"用这个 skill 干活"

直接加载 `SKILL.md` 并按其流程执行（Before Start Checklist → Decision Tree → SOP 8 步 → Final Report）。注意：

- 目标 App 的类名/方法名必然与模板不同，**必须先逆向确认**，禁止直接复制模板代码。
- 每个版本必须有用户可见的验证手段（弹窗/标题），禁止"试试看"。
- 交付物必须包含踩坑记录。
- 崩溃/失效时按 references/troubleshooting.md 的 10 个问题逐条排查。
