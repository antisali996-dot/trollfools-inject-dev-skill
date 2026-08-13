# 真实案例复盘：5EPlay 去开屏广告插件（经验增量）

> 本案例沉淀自 5EPlay 去开屏广告插件的实战，是对 Skill v2.1.0 新增经验（三层 Hook 信号、证据分级、偶发行为判断）的实例化说明。
>
> ⚠️ **版本特定声明**：以下所有类名/selector/调用链（`handleBrandAd:`、`handleAdInfo:`、`fetchAdInfo:`、`enterSplashAD` 等）都是**本案例当时分析的 5EPlay 版本**的逆向结果，不是通用规则——**不是所有广告 App 都有这些 selector**，其他目标 App 必须重新逆向。

## 1. 原始 Hook 与问题

**原始方案**：Hook `handleBrandAd:` 拦截品牌广告（`adv_brand`）。

**问题**：只覆盖 `adv_brand` 一条路径，拦截不完整——存在同一展示行为的**另一分支**。

**发现（L1 IDA 静态确认）**：真正的共同汇合点是 `handleAdInfo:`，它分叉出两条广告类型：

```text
handleAdInfo:
    ├── adv_brand        ← 原 Hook 只覆盖这里
    └── adv_slot_items   ← 漏掉的另一条路径
```

结论：Hook 点应该选在**共同汇合点**（`handleAdInfo:`），而不是单一分支（`handleBrandAd:`）。这与 Skill 既有「找唯一出口」经验一致，但本例说明：**出口可能有两个分叉，必须确认覆盖所有分叉**。

## 2. 冷启动 / 热启动两条路径

同一个展示行为存在两条启动路径，最终汇合到同一方法：

```text
冷启动：
enterSplashAD
    → fetchAdInfo(NO)
        → handleAdInfo:

热启动：
enterForeground
    → enterHotLaunchAD
        → fetchAdInfo(YES)
            → handleAdInfo:
```

**共同汇合点**：`handleAdInfo:`（两条路径都经过）。

设计 Hook 时以此为准（L1 确认 + L3 运行时验证），而不是分别 hook 两条启动路径。

## 3. 验证过程（三层信号）

| 阶段 | 信号 | 证据级别 | 说明 |
|---|---|---|---|
| dylib 注入 | PLUGIN LOADED | L2 | constructor 执行，确认 dylib 被加载 |
| 方法安装 | HOOK INSTALLED | L2 | `handleAdInfo:` Class/Method 找到，IMP 已替换 |
| 真命中 | HOOK HIT #N | L3 | 冷/热启动时 `handleAdInfo:` 真实进入，计数递增 |
| 行为验证 | 开屏广告消失 | L4 | 真机完整行为验证 |

关键教训：

- 只看「有弹窗 = 插件加载」只能到 L2——**不能证明 Hook 命中了正确的方法**。
- `HOOK INSTALLED` 后如果 `HOOK HIT` 迟迟不出现，说明**目标 selector 没走这条路径**（版本差异 / 路径不同），而不是 Hook 没生效。
- 本例中 HIT 在冷启动、热启动各出现，结合调用链确认两条路径都汇合到 `handleAdInfo:`。

## 4. 偶发行为判断（示例）

「有时候有广告、有时候没广告」在验证中**不能直接当作 Hook 成功**。本例的排查口径：

```text
服务端不下发（该时段没有广告位）
频控 / 缓存
广告填充失败
其他展示路径（未被覆盖的分支）
Hook 真命中（L3）
```

判断依据必须是：

```text
HOOK HIT（L3）
+ 输入数据（fetchAdInfo 参数 NO/YES、服务端返回内容）
+ 真实展示路径（逆向确认的调用链）
```

## 5. 本案例对 Skill 的增量贡献

- 三层信号（PLUGIN LOADED / HOOK INSTALLED / HOOK HIT #N）在真实项目中的使用方式
- 证据分级（L0-L4）在报告中的标注方式
- 「共同汇合点可能分叉」——找出口时确认是否覆盖所有分叉
- 冷启动/热启动双路径对照验证
- 偶发行为必须结合 HIT + 输入数据 + 展示路径判断
