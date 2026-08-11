# AGENTS.md — 给 AI Agent 看的仓库说明

本文件是给 **AI Agent（opencode / Claude Code 等）** 阅读的。你是 Agent 时，请先读完本节再操作本仓库。

## 这是什么

`trollfools-inject-dev-skill` 是一个 **Skill 定义仓库**：核心产物是 `SKILL.md`（一套 iOS TrollFools 注入插件开发的完整方法论），README.md 是给人类看的介绍。

## 仓库结构

```
├── SKILL.md      # 核心产物：完整 skill 定义（SOP + 模板 + 排错 + Agent 执行规则）
├── AGENTS.md     # 本文件：给 Agent 的说明
└── README.md     # 给人类的介绍（详细且带徽章）
```

## 你的职责（维护/发布 skill 时）

### 更新 SKILL.md 的规则

1. **不得破坏 frontmatter**：`name` / `description` / `version` / `tags` 是 opencode 加载 skill 的依据。改版时递增 `version`。
2. **description 要写清楚触发条件**：description 决定了 skill 何时被自动触发，改动时确保包含关键词（ios / trollstore / trollfools / dylib / hook / 逆向 / 注入 / 去广告）。
3. **经验类内容只增不减**：踩坑记录是对使用者最大的价值，删除前需确认该经验已被新经验替代。
4. **模板必须带警告**：`SKILL.md` 中的代码模板是参考框架，类名/方法名按目标 App 不同。新增模板时必须附"需按实际逆向结果调整"的警告。

### 修改 README.md 的规则

- 保持详细、漂亮：徽章区、表格、流程图（ASCII）、emoji 分区。
- README 中的核心经验（10 条）与常见问题（10 坑）必须与 `SKILL.md` 保持一致，改一处要同步另一处。

### 提交规范

- commit message 用中文或英文均可，格式：`type: 描述`（如 `docs: 补充 AGENTS.md`、`feat: SKILL.md v1.1.0 新增 xx 经验`）。
- 修改 `SKILL.md` 时 commit message 必须标注新的 version。

## 用户若要求"安装这个 skill"

把 `SKILL.md` 复制到目标机器的 skills 目录：

- Windows: `%USERPROFILE%\.config\opencode\skills\trollfools-inject-dev\SKILL.md`
- macOS/Linux: `~/.config/opencode/skills/trollfools-inject-dev/SKILL.md`

安装目录名（`trollfools-inject-dev`）必须与 SKILL.md frontmatter 的 `name` 一致。

## 用户若要求"用这个 skill 干活"

直接加载 `SKILL.md` 并按其流程执行（环境检查 → 结构分析 → 目标分析 → 方案设计 → 编写 → 调试 → 修复 → 交付）。注意：

- 目标 App 的类名/方法名必然与模板不同，**必须先逆向确认**，禁止直接复制模板代码。
- 每个版本必须有用户可见的验证手段（弹窗/标题），禁止"试试看"。
- 交付物必须包含踩坑记录。
