# FluidSynth fork 待办

更新：2026-10-27；上游核对基线 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`。此 `todo` 分支是独立根历史，只放任务记录，与 FluidSynth 代码历史没有公共祖先。代码工作在自己的 `origin` fork；`upstream` 远程仅用于获取基线和核对合并状态。

## 分支约定与当前阶段

- `fix/dev-xxx`、`perf/dev-xxx`、`ci/dev-xxx`、`refactor/dev-xxx`：未完成工作，完整保留 Git 提交历史，允许继续迭代。
- 去掉 `dev-` 的对应分支：review 完成后基于当时的上游，整理为一个完整提交，再提 PR。开发分支不提前 squash。
- 已有开放 PR 的 head 分支保留，避免影响 PR；目前共 2 个（#1879、#1884），见“已提上游的工作”。已合并 PR 的现存分支仅保留历史，不再计入待 review 工作。
- `archive/previous-work`：集中保留旧分支的完整提交历史及原名称映射。原来的日期分支、重复版本和已停止实验分支从分支列表清理；所有原始提交仍可追溯。
- 分支结构已整理完成。All Notes Off 已提 #1879，仍开放；踏板转交 #1880 和零 tempo #1881 已合并。其余 dev 分支的完成程度分别记录，不能因本次状态更新而视为已完成测试精简或 rebase。审计包按主题拆分；DLS 按整体重构系列 review。

远端现有 **38 个分支：29 个 dev 分支、2 个开放 PR 分支、4 个已合并 PR 的保留分支，以及 master / todo / archive/previous-work**。dev 分支中也保留了已合并主题的旧历史，因此分支数量不等于待修问题或待提 PR 数量。本次更新仅修改 `todo` 文档，不删除、重写或推送任何代码/PR 分支。

## 当前取舍：减少上游审核压力

2026-10-27 用户决定：边际问题先保留，今后再处理；不为凑三个问题或增加 PR 数量而提交。判断是否值得推进时，同时考虑普通使用下的发生条件、实际后果、收益证据与维护者审核成本。合法输入能触发、生产只改一行，都不足以单独构成优先提交的理由。

- 先跟进现有开放 PR：#1879 Mono 清键、#1884 SF3 x87 构建兼容。当前没有安排新增边际修复 PR。
- 现有证据不足以列出三个“普通 MIDI、常用音色、默认播放下常见且严重”的剩余渲染缺陷。#1879 需 Mono 与清键事件；IIR 启动问题需 JACK 协商前后采样率不同，不能写成普通离线渲染普遍受影响。
- 重复 CC66、SF2 固定力度 0、Breath Sync 均保留为低优先级候选。没有发生率或常用资产证据支持把它们称为常见问题；Breath Sync 是需显式启用的 FluidSynth 模式，不是默认启用的 MIDI 事件。
- 播放器控制、跨轨排序/seek、heap merge、F35 继续搁置，沿用 2026-10-26 的决定；本次未恢复这些任务。
- **DLS 大重构往后放**，完整保留原版与 minimal 版分支，不自动推进 review 或 PR。
- **Android 等待用户有空 review**。当前瓶颈是用户自己的审核时间，不是要求继续补实现或催促上游；迁移与独立 probe 均先保留。
- **Windows CI 精简以后再说**，先接受现有运行速度，不为局部 CI 加速增加审核负担。
- **double IIR W4 保留为性能候选**，尚未安排提交。它是剩余非正确性工作中已有真实端到端收益证据的一项；下面明确旧数据的范围，不能把旧结果当作当前 master 的收益。

### IIR W4 的历史收益与限制

`perf/dev-iir-stable-block` 是稳态四采样展开，与已合并 #1867 的 IIR 系数复用不同。2026-10-16 的报告在旧组合基线 `6be67a90` 上测得：All The Small Things 的 10 秒真实 MIDI 片段，double、默认四阶插值、效果器开启、44.1 kHz、64 帧写入，30 对候选/基线测量。

| 计时范围 | 候选/基线耗时的配对几何均值 | 耗时降低 | 95% 置信区间 |
|---|---:|---:|---:|
| 创建 synth、加载 SF2/MIDI、分配并触及输出缓冲、完成渲染 | 0.9422 | **5.8%** | 比例 0.9235–0.9613，即降低约 3.9%–7.7% |
| 仅渲染阶段 | 0.9061 | **9.4%** | 比例 0.8791–0.9339 |

**5.8% 是上述端到端流程，不是 micro benchmark**；不是所有曲目平均值，不含真实音频设备，也未在当前上游重测。float 对照比例 1.0028、置信区间跨 1，没有可宣称收益。生产代码 +129、现有测试 +318；真正交付前需基于当时上游重新核对实现、测试规模、PCM 与端到端收益。完整历史报告和数据保留在 [分支报告](https://github.com/Yikai-Liao/fluidsynth/blob/e9133d017d6571712a1aac249d74362ec1a879fc/doc/performance-evidence/iir-block-e2/REPORT.md)；下方分支表可用于定位当前历史。

### 已审查但暂不推进的小修复

以下是现有 dev 分支相对各自 merge-base 的实际差异（包含注释和空行），不代表当前上游已重新完成整套验证。

| 主题 | 触发条件与后果 | 生产差异 | 测试增量 | 修复/审核难度与安排 |
|---|---|---:|---:|---|
| 启动采样率变化后 IIR 表未更新；`fix/dev-initial-rate-iir-table` | JACK 初次连接时 synth/device 速率不同；滤波响应与按目标速率创建的 synth 不同 | +1/−0 | +40 | 实现低；需区分滤波表修复与默认 Dattorro 的换率限制。条件性候选，暂不提 |
| F31 Breath Sync 复位；`fix/dev-breath-sync-reset` | 显式启用气息同步 Mono，CC121 后等待音符漏启动 | +1/−1 | +21 | 低；特殊模式，后放 |
| F46 重复 CC66；`fix/dev-sostenuto-repeat` | Sostenuto 保持按下时重复发按下值，错误捕获后来的音符 | +9/−7 | +8 | 低至中；需核对逐声道转发和按下边沿。不同于已合并 #1880 的释放交接，后放 |
| F19 固定力度 0；`fix/dev-fixed-velocity-zero` | SF2 区域显式设固定 velocity=0 时，力度相关调制错误；不等于必定静音 | +2/−1 | +24 | 低；合法但较少见的音色参数，后放 |

2026-10-27 在独立 worktree 中以 `18bcb76f` 的生产源码导入现有回归：Mono 清键、重复 CC66、Breath Sync、固定力度 0 和 IIR 启动换率五项均在各自预期断言处失败；合并应用已有五项诊断修复后均通过。配置为 Linux RelWithDebInfo、double、OpenMP 关闭；这只是五个定向探针，不是当前分支全量测试、真实 JACK 设备验证或常见程度调查。原四项 dev 的 2026-10-26 Debug UBSan/float-cast-overflow 46/46 等结果仍是历史证据，未当作本次全量重测。

F18 unpitched SF2 + 自定义调音、F49 调音跨音区 + Mono 连奏、Dattorro 换率、短循环、内存释放与参数边界等继续保留证据，不排成新的上游提交批次。严重内存安全问题仍按实际入口单独评价；源码审计分数不能直接代表普通 MIDI 渲染的影响。

今后恢复某个主题时，保持最小生产差异，先复用项目已有结构，不夹带查询 API、注释、命名或线程机制整理。测试沿用邻近命名、缩进、注释和断言；复用文件必须符合其职责，确需独立文件时说明原因。对关键回归做修复前失败/修复后通过的对照；完成 review 后才从当时上游建立单提交 PR 分支，作者使用 Yikai-Liao，PR 正文简短并注明 `AI-assisted with Codex`。

## 审计前的积压与当前工作分支

本表同时列出审计前的独立修复、已从审计包提取的修复、性能和 CI 工作。生产差异只计 `src`；CI 差异计相关工作流/工具。所有数值相对各分支与声明上游 `c7ba7b49` 的 merge-base；仍需在最终 rebase 时重新检查。

| 工作分支 | 内容 | review 规模 | 生产/CI 差异 |
|---|---|---|---|
| [fix/dev-all-notes-off-mono-keys](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-all-notes-off-mono-keys) | All Notes Off 清除 mono held keys；#1879 open（另含 CC120） | M | +7 / −0 |
| [fix/dev-basic-channel-count-bounds](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-basic-channel-count-bounds) | basic channel 数量校验与加法溢出 | M | +2 / −8 |
| [fix/dev-breath-sync-reset](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-breath-sync-reset) | F31：CC121 复位 Breath Sync 的上一气息值；已 review，暂缓提交 | S | +1 / −1 |
| [fix/dev-dattorro-sample-rate](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-dattorro-sample-rate) | Dattorro 采样率变化时重建状态 | M | +25 / −2 |
| [fix/dev-dither-table-init](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-dither-table-init) | 共享 S16 dither 表并发初始化 | M | +20 / −19 |
| [fix/dev-fixed-velocity-zero](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-fixed-velocity-zero) | F19：固定力度 0 覆盖输入力度；已 review，暂缓提交 | S | +2 / −1 |
| [fix/dev-initial-rate-iir-table](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-initial-rate-iir-table) | 采样率变化后更新 IIR 查表 | M | +1 / −0 |
| [fix/dev-jack-rate-change](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-jack-rate-change) | JACK 采样率变化时停止不匹配渲染 | L | +48 / −18 |
| [fix/dev-midi-track-count](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-midi-track-count) | MIDI 轨道数符号扩展与边界 | S | +6 / −1 |
| [fix/dev-midi-zero-tempo](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-midi-zero-tempo) | 拒绝 MIDI 零 tempo；#1881 merged，dev 仅保留历史 | S | +6 / −0 |
| [fix/dev-player-control](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-player-control) | MIDI player 暂停恢复与 tempo 请求；搁置 | L | +74 / −58 |
| [fix/dev-player-track-order](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-player-track-order) | 跨轨全局 tick 顺序与 seek 重建；搁置 | L | +53 / −63 |
| [fix/dev-relative-clock-init](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-relative-clock-init) | 相对时钟 origin 的并发首次初始化 | M | +10 / −0 |
| [fix/dev-sample-guard-frame-overflow](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-sample-guard-frame-overflow) | 采样 guard-frame 分配尺寸溢出 | S | +6 / −0 |
| [fix/dev-shared-synth-init](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-shared-synth-init) | 并发构造时等待共享 synth 初始化完成 | M | +18 / −3 |
| [fix/dev-shell-server-mutex-release](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-shell-server-mutex-release) | shell server 失败与关闭时释放 mutex | S | +2 / −0 |
| [fix/dev-short-loop-interpolation](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-short-loop-interpolation) | 短循环的高阶插值 tap wrap | L | +68 / −0 |
| [fix/dev-smf-sysex-continuations](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-smf-sysex-continuations) | SMF SysEx 续包与 escaped MIDI 时序 | L | +120 / −55 |
| [fix/dev-timer-thread-handle-release](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-timer-thread-handle-release) | timer join 后释放线程句柄 | S | +3 / −1 |
| [fix/dev-sostenuto-sustain-handoff](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-sostenuto-sustain-handoff) | CC66 抬起时转交仍按下的 sustain；#1880 merged，dev 仅保留历史 | M | +7 / −2 |
| [fix/dev-sostenuto-repeat](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-sostenuto-repeat) | F46：重复 CC66 不重新捕获后来音符；已 review，暂缓提交 | S | +9 / −7 |
| [fix/dev-audit-fixes](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-audit-fixes) | 审计混合包：51 项成立问题，待按主题拆分 | L | 混合/重构包，见下文 |
| [perf/dev-iir-stable-block](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-iir-stable-block) | double IIR 稳态四采样展开（W4）；保留性能候选 | L | +129 / −0 |
| [perf/dev-midi-track-merge](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-midi-track-merge) | MIDI 跨轨事件 heap merge 备选实现；随 player 搁置 | L | +84 / −58 |
| [ci/dev-windows-build](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-windows-build) | Windows CI 子模块与 MSYS2 依赖精简；以后再说 | L | +27 / −12 |
| [ci/dev-android-migration](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-android-migration) | Android CI 迁移与构建/测试工具整合；等用户有空 review | L | +647 / −751 |
| [ci/dev-android-developer-probe](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-android-developer-probe) | Android API 35 Debug 与消费端独立 probe；等用户有空 review | L | +708 / −751 |
| [refactor/dev-dls-boundaries](https://github.com/Yikai-Liao/fluidsynth/tree/refactor/dev-dls-boundaries) | DLS 边界、存储、WSMP 与 voice 生命周期重构；后放 | L | 混合/重构包，见下文 |
| [refactor/dev-dls-boundaries-minimal](https://github.com/Yikai-Liao/fluidsynth/tree/refactor/dev-dls-boundaries-minimal) | DLS minimal 版与后续分配失败处理；与原重构比较后再决定，后放 | L | 混合/重构包 |

## 需要保留的区别与依赖

- `perf/dev-iir-stable-block` 是尚未提 PR 的 double-only W4 优化，生产 +129、已有测试 +318。它与已合并的 IIR 系数复用 #1867 不同。历史报告有通过的质量与性能门槛，但后续仍需精简/整合测试，并基于当前上游重新核对；不直接沿用旧基线的性能结论。
- `ci/dev-windows-build` 是尚未提 PR 的 Windows CI 整理，单个 workflow +27/−12。既有多个开发提交在 dev 分支完整保留。
- Android 迁移整合稿在 `ci/dev-android-migration`；独立 API 35 developer probe 在 `ci/dev-android-developer-probe`。旧的 cicd-work、maintainability、superbuild 版本在归档保留，先比较差异再决定是否还要移植；不将同一迁移重复算多项 PR。
- `fix/dev-player-track-order` 与 `perf/dev-midi-track-merge` 是正确性方案与 heap 备选实现；需要比较后决定最终实现，不能重复宣传同一修复。
- 旧 `fix/midi-track-duration-20261021` 通过拒绝超 INT_MAX 曲目解决计数问题；审计 F35 改为内部 64 位继续播放。旧方案已归档，优先 review F35，再决定旧用例如何移植。
- 两版 sostenuto/sustain 修复保留较早但独立的工作分支 `fix/dev-sostenuto-sustain-handoff`；另一版含不同旧基线和部分测试差异，完整原历史仍在归档。
- DLS 的四个重构提交保留在 `refactor/dev-dls-boundaries`，继承的审计包也保留。先核对依赖再整理到干净上游基线；不把 13 个 DLS 审计条目自动拆成 13 个 PR。
- `fix/dev-audit-fixes` 保留原审计与两次复审的历史，并加入 F18 测试夹具精简提交。F18 独立测试暂由 211 行减到 74 行；此前 double/float Debug ASan/UBSan/float-cast-overflow 均通过，撤销修复或将 fallback 错设为 61 均失败。下一步检查合入现有 sample/pitch 测试的合适位置。

## 用户补充与移出队列的工作

- **优先级按音频正确性排序（2026-10-26 用户决定）。** 下一批先处理音色、力度、漏发音和踏板延音；内存释放、空指针和防御性边界后移，不再仅按审计严重度或生产行数排在最前。
- **播放器问题搁置（2026-10-26 用户决定）。** 暂停继续精简、拆 PR 或提交 `fix/dev-player-control`；跨轨顺序/seek、heap merge 备选以及审计 F35 的播放器计数改造也移出当前队列，已有分支和证据保留。暂停恢复的公开 API 复现已确认上游有 unsigned 时间差溢出，但现有修复仍涉及 tempo 请求与 timer 生命周期；恢复工作时先评估局部修复或拆分方案，不直接提交当前大补丁。零 tempo 的解析期拒绝已独立提 #1881，并于 2026-10-26 合并。
- **sinc：维护者自行处理。** 不继续 review 或提 PR。两个旧实现分支及证据在归档历史中保留。
- **未定位的时间偏移修复：** 用户记得声音或效果整体提前/延后约一秒，可能与效果器有关；明确排除 Dattorro 4 ms predelay 的 #1872。已检查当前分支标题、效果器修改和主要实验报告，尚未找到能确认匹配的记录；保留为待定位，不能用 player 或 predelay 的近似问题替代。
- 早期无稳定收益、质量门槛未过或仅 instrumentation 的 perf/explore/measure/investigate 原型已移入归档。SoA IIR 是搁置的架构实验，不列入当前提交队列；需要恢复时可从归档取回完整历史。

## 已提上游的工作

| 工作 | 状态与分支处理 |
|---|---|
| [#1879 Mono All Notes Off / All Sounds Off](https://github.com/FluidSynth/fluidsynth/pull/1879) | open；`fix/all-notes-off-mono-keys`；保留 PR head；不在本次状态更新中修改 |
| [#1884 SF3 x87 fallback](https://github.com/FluidSynth/fluidsynth/pull/1884) | open；`fix/sf3-x87-fallback`；保留 PR head；不在本次状态更新中修改 |
| [#1880 sostenuto → sustain](https://github.com/FluidSynth/fluidsynth/pull/1880) | merged（2026-10-26）；`fix/sostenuto-sustain-handoff`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1881 MIDI zero tempo](https://github.com/FluidSynth/fluidsynth/pull/1881) | merged（2026-10-26）；`fix/midi-zero-tempo`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1877 SF3 gain](https://github.com/FluidSynth/fluidsynth/pull/1877) | merged（2026-10-26）；`fix/sf3-sample-gain-20261021`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1876 macOS CI](https://github.com/FluidSynth/fluidsynth/pull/1876) | merged（2026-10-26）；`ci/macos-intel-arm`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1878 silent-loop](https://github.com/FluidSynth/fluidsynth/pull/1878) | 关闭未合并；上游已通过 [#1871](https://github.com/FluidSynth/fluidsynth/pull/1871) 修复对应问题，不重提 |
| [#1863 voice reuse](https://github.com/FluidSynth/fluidsynth/pull/1863)、[#1864 Dattorro tap](https://github.com/FluidSynth/fluidsynth/pull/1864)、[#1867 IIR](https://github.com/FluidSynth/fluidsynth/pull/1867)、[#1868 realtime MIDI](https://github.com/FluidSynth/fluidsynth/pull/1868) | merged；历史与证据保留 |
| [#1870 Linux CI](https://github.com/FluidSynth/fluidsynth/pull/1870)、[#1872 predelay](https://github.com/FluidSynth/fluidsynth/pull/1872)、[#1873 runner](https://github.com/FluidSynth/fluidsynth/pull/1873)、[#1874 Ubuntu/Arm](https://github.com/FluidSynth/fluidsynth/pull/1874)、[#1875 Alpine](https://github.com/FluidSynth/fluidsynth/pull/1875) | merged；历史与证据保留 |

状态于 2026-10-27 通过 GitHub PR 元数据与上游提交核对。开放 PR 只表示尚未合并，不等于所有远端 CI 已通过；历史性能、sanitizer 与全量测试结论仅适用于其原始版本。

## 归档与恢复

[archive/previous-work](https://github.com/Yikai-Liao/fluidsynth/tree/archive/previous-work) 的 README 列出全部 100 个旧分支名、原 head SHA 和工作分支对应关系。归档提交把独立旧分支头保留为父提交，保证所有旧提交仍可达。恢复旧版本时，从表中的原 SHA 创建新的 `*/dev-*` 分支即可；不要重建旧的日期命名。

另有本地 Git bundle 备份及引用清单，位于当前仓库的 Git 元数据目录 `branch-backups`；bundle 已通过校验。

## 审计问题索引

以下保留 51 项成立问题作为后续拆分清单，F26 已撤回。严重度沿用审计报告的排序分，不是 CVSS；这里的独立编号不等于已经存在独立任务分支。DLS 条目作为整体重构的覆盖关系保留。

| 编号 | 问题 | 报告分数 | review 规模 | 已有测试 |
|---|---|---|---|---|
| [F01](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L63) | 路由器把不同声道的同音高音符当成一个 | 6 | M–L | `test_midi_router` |
| [F02](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L139) | 合法的 8 位 DLS 样本会触发未定义行为 | 3 | DLS 整体 L | `test_dls_parser` |
| [F03](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L317) | DLS 子块与采样索引能越过父 LIST | 2 | DLS 整体 L | `test_dls_parser` |
| [F04](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L330) | SDL 尚未启动时，每次建 settings 都漏一个锁 | 2 | M | `test_sdl3_settings_init` |
| [F05](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L342) | SDL 枚举结果及驱动拥有资源未完整释放 | 2 | M | `test_sdl3_settings_init；test_sdl3_driver_lifecycle` |
| [F06](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L355) | SoundFont NRPN 选择器累加很多次后会绕回到有效生成器 | 2 | M | `test_nrpn_selector_wrap` |
| [F07](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L368) | DLS wsmp 的大小相加会绕回去，读到旁边的数据 | 2 | DLS 整体 L | `test_dls_parser` |
| [F08](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L152) | 极端调音数值在 MTS 导出时转成整数会溢出 | 3 | M | `test_mts_bulk_dump_pitch_range` |
| [F09](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L127) | 调音名称缓冲区长度为 0 时发生栈越界 | 4 | S | `test_tuning_dump_zero_name_length` |
| [F10](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L638) | 调音名称缓冲区够长，最后一个字还是被截掉 | 1 | S | `test_tuning_dump_zero_name_length` |
| [F11](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L380) | DLS fmt 子块太短，解析器仍从后面拿格式字段 | 2 | DLS 整体 L | `test_dls_parser` |
| [F12](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L101) | 坏调制器目的地会变成 voice 数组下标 | 5 | M | `test_sfont_mod_dest_bounds` |
| [F13](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L76) | 替换 SoundFont 默认调制器后，新音符会读旧地址 | 6 | M–L | `test_sfont_mod_dest_bounds` |
| [F14](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L51) | 删掉合成器后，设置变化仍会呼叫已释放的合成器 | 7 | S | `test_settings_unregister_callback` |
| [F15](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L392) | GS / XG SysEx 地址含高位时触发有符号移位 UB | 2 | M | `test_synth_sysex_address_high_bit` |
| [F16](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L165) | DLS 的音高缩放能把 MIDI 音符变成数组外下标 | 3 | DLS 整体 L | `test_dls_parser` |
| [F17](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L178) | 超范围固定键或根音能越过调音表 | 3 | M–L | `test_sfont_keynum_bounds；test_voice_generator_bounds` |
| [F18](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L89) | 合法的 unpitched SF2 样本在自定义音律下越界 | 6 | S | `test_sfont_unpitched_tuning` |
| [F19](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L191) | SF2 的固定力度 0 被忽略 | 3 | S | `test_sfont_fixed_velocity_zero` |
| [F20](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L404) | DLS unity note 大于 127 后会越过调音表 | 2 | DLS 整体 L | `test_dls_parser` |
| [F21](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L204) | 小 DLS 文件能诱发数 GB 的未验证预分配 | 3 | DLS 整体 L | `test_dls_parser` |
| [F22](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L416) | DLS 的 alias 数量超过 PGAL 块长度 | 2 | DLS 整体 L | `test_dls_parser` |
| [F23](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L428) | 零长度 RGNH 会把相邻子块读成范围字段 | 2 | DLS 整体 L | `test_dls_parser` |
| [F24](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L440) | MIDI 事件可以读出声明轨道之外的字节 | 2 | M | `test_midi_file_track_bounds` |
| [F25](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L651) | 时间签名分母指数过大时发生浮点转整数未定义行为 | 1 | M | `test_midi_file_timesig_exponent` |
| [F27](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L452) | 超大 MIDI 轨道长度被读成负数，坏文件被当成空轨道 | 2 | M | `test_midi_file_track_bounds` |
| [F28](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L464) | 高位 GS SysEx 节奏参数被误认成鼓声道设置 | 2 | M | `test_synth_sysex_address_high_bit` |
| [F29](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L476) | 超长 GS SysEx 校验和会让有符号累加溢出 | 2 | M | `test_synth_sysex_address_high_bit` |
| [F30](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L114) | SF2 样本结束位置能超出 PCM 数据边界 | 5 | S | `test_sfont_sample_end_bounds` |
| [F31](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L217) | CC 121 后气息同步的等待音符不会再启动 | 3 | S | `test_synth_reset_cc` |
| [F32](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L229) | 控制器复位后，旧的定时音符会关闭新音符 | 3 | M–L | `test_seqbind_channel_all_notes_off` |
| [F33](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L241) | 优先级线程上下文释放方式错误，异常退出还会泄漏 | 3 | M | `test_threading；test_thread_exception` |
| [F34](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L489) | 极端生成器数值在音高、力度和采样换算时溢出 | 2 | M–L | `test_sfont_gen_offset_bounds；test_voice_generator_bounds` |
| [F35](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L502) | 高速 MIDI 与累计长 tick 暴露播放器计数范围不足 | 2 | M–L | `test_midi_file_timesig_exponent` |
| [F36](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L515) | NaN 混响或合唱参数会污染后续音频 | 2 | M | `test_effect_nonfinite_controls` |
| [F37](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L527) | 非法时值音符会取消另一音符的关闭消息 | 2 | M–L | `test_seqbind_note_bounds` |
| [F38](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L540) | NaN 合成器总增益会让后续音频变成 NaN | 2 | S | `test_effect_nonfinite_controls` |
| [F39](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L552) | 公开 voice API 接受越界 generator 下标 | 2 | M | `test_voice_generator_bounds` |
| [F40](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L564) | 极端排他类别值导致浮点转整数溢出 | 2 | M | `test_sfont_gen_offset_bounds` |
| [F41](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L576) | 插值步长归零会除零，最近邻尾部取整还可越界 | 2 | M–L | `test_sfont_gen_offset_bounds；test_rvoice_dsp_interpolate` |
| [F42](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L254) | DLS 条件查询压栈、未知查询及布尔编码不符合规范 | 3 | DLS 整体 L | `test_dls_parser` |
| [F43](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L589) | 非有限调音值会让 MIDI 音符无声 | 2 | M | `test_tuning_nonfinite_input` |
| [F44](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L601) | 循环关闭后，插值仍读取失效的循环边界 | 2 | S | `test_rvoice_loop_guard_reset` |
| [F45](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L613) | SDL3 音频设备枚举失败时释放未初始化的名字指针 | 2 | M | `test_sdl3_asprintf_oom` |
| [F46](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L267) | 重复的 Sostenuto 踏板消息会把新音符也锁住 | 3 | M–L | `test_synth_reset_cc` |
| [F47](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L279) | DLS articulation 条件为假时错误失败或泄漏参数 | 3 | DLS 整体 L | `test_dls_parser` |
| [F48](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L663) | Meta 事件后错误沿用已取消的 MIDI 运行状态 | 1 | M | `test_midi_file_track_bounds` |
| [F49](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L292) | 调音跨越音区后，单音连奏仍保留旧音区的声音 | 3 | M–L | `test_monopoly_legato_tuning_split` |
| [F50](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L626) | 短小 MIDI 文件可诱发巨额元事件分配 | 2 | M | `test_midi_file_meta_size_bounds` |
| [F51](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L675) | 无效 DLS 采样偏移让解析器先预留 36 MB | 1 | DLS 整体 L | `test_dls_ptbl_allocation` |
| [F52](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L304) | 极端 DLS gain 取负发生有符号整数溢出 | 3 | DLS 整体 L | `test_dls_parser` |

F26：撤回基线归因；见报告更正及已合并 #1868。
