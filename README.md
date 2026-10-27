# FluidSynth fork 待办

更新：2026-10-28；上游核对基线 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`。此 `todo` 分支是独立根历史，只放任务记录，与 FluidSynth 代码历史没有公共祖先。代码工作在自己的 `origin` fork；`upstream` 远程仅用于获取基线和核对合并状态。

## 分支约定与当前阶段

- `fix/dev-xxx`、`perf/dev-xxx`、`ci/dev-xxx`、`refactor/dev-xxx`：未完成工作，完整保留 Git 提交历史，允许继续迭代。
- 去掉 `dev-` 的对应分支：review 完成后基于当时的上游，整理为一个完整提交。可以先存入待办，等用户安排后再提 PR；开发分支不提前 squash。
- 已有开放 PR 的 head 分支保留，避免影响 PR；目前共 2 个（#1879、#1884），见“已提上游的工作”。已合并 PR 的现存分支仅保留历史，不再计入待 review 工作。
- `archive/previous-work`：集中保留旧分支的完整提交历史及原名称映射。原来的日期分支、重复版本和已停止实验分支从分支列表清理；所有原始提交仍可追溯。
- 分支结构已整理完成。All Notes Off 已提 #1879，仍开放；踏板转交 #1880 和零 tempo #1881 已合并。其余 dev 分支的完成程度分别记录，不能因本次状态更新而视为已完成测试精简或 rebase。审计包按主题拆分；DLS 按整体重构系列 review。

远端现有 **39 个分支：29 个 dev 分支、1 个暂存的非 dev 性能分支、2 个开放 PR 分支、4 个已合并 PR 的保留分支，以及 master / todo / archive/previous-work**。dev 分支中也保留了已合并主题的旧历史，因此分支数量不等于待修问题或待提 PR 数量。本次新增 `perf/iir-stable-block` 并更新 `todo`，其余代码/PR 分支保留。

## 当前取舍：减少上游审核压力

2026-10-27 用户决定：边际问题先保留，今后再处理；不为凑三个问题或增加 PR 数量而提交。判断是否值得推进时，同时考虑普通使用下的发生条件、实际后果、收益证据与维护者审核成本。合法输入能触发、生产只改一行，都不足以单独构成优先提交的理由。

- 先跟进现有开放 PR：#1879 Mono 清键、#1884 SF3 x87 构建兼容。当前没有安排新增边际修复 PR。
- 现有证据不足以列出三个“普通 MIDI、常用音色、默认播放下常见且严重”的剩余渲染缺陷。#1879 需 Mono 与清键事件；IIR 启动问题需 JACK 协商前后采样率不同，不能写成普通离线渲染普遍受影响。
- 重复 CC66、SF2 固定力度 0、Breath Sync 均保留为低优先级候选。没有发生率或常用资产证据支持把它们称为常见问题；Breath Sync 是需显式启用的 FluidSynth 模式，不是默认启用的 MIDI 事件。
- 播放器控制、跨轨排序/seek、heap merge、F35 继续搁置，沿用 2026-10-26 的决定；本次未恢复这些任务。
- **DLS 大重构往后放**，完整保留原版与 minimal 版分支，不自动推进 review 或 PR。
- **Android 等待用户有空 review**。当前瓶颈是用户自己的审核时间，不是要求继续补实现或催促上游；迁移与独立 probe 均先保留。
- **Windows CI 精简以后再说**，先接受现有运行速度，不为局部 CI 加速增加审核负担。
- **IIR W4 已单提交暂存，过几天再安排 PR**，减少同时进行的上游审核工作。已完成同一套内核支持 float/double 的精度与代表性 micro 验证，并调查真实 MIDI/SoundFont 渲染中短于 64 样本的调用比例；当前提交与结论见下文。没有把历史端到端数字当作当前实现收益。

### IIR W4：已保存的精简分支与后续验证

- 分支：[perf/iir-stable-block](https://github.com/Yikai-Liao/fluidsynth/tree/perf/iir-stable-block)，不带 `dev-`；当前提交 [`707b52d6`](https://github.com/Yikai-Liao/fluidsynth/commit/707b52d64443ffdd5550b6be948214a907852ceb) 是统一 float/double 实现，直接基于 `18bcb76f`，只领先上游一个提交，没有创建 PR。
- 保存范围：只修改 `src/rvoice/fluid_iir_filter_impl.cpp`，+39/−5，净增 34 行；一个共享内核、无按精度分开的副本，没有新增生产断言。旧 dev 分支的 +129 行版本与历史数据保留，不另排一个 PR。
- 同一 W4 公式和尾部同时支持 float/double。代表性 64 样本固定系数 micro 的候选/原始耗时比（15 对配对测量，95% CI）为：float GCC 0.862（0.815–0.913）、float Clang 0.837（0.810–0.865）、double GCC 0.677（0.632–0.724）、double Clang 0.649（0.594–0.708）；这些是 micro 结果，不是完整渲染收益。
- float 的全精度展开会在低截止频率、高 Q 反馈下显著偏离 MPFR 参考。按用户要求的同一原始 float 系数/输入/状态、无中途 float 截断的 MPFR 递推 oracle 定位后，最终共享内核只将 `p2`、`p3` 两个多项式中间量保留为 double，float 状态与其余 R 运算保持原类型。384 个长序列覆盖上下文中最终输出和末态与最终候选逐位一致；参考 128/256 位的最大相对 RMS 差为 `3.053e-34`。
- 准确性 Oracle：同一组已经量化为 float 的滤波系数、同一输入和初始状态，在 MPFR 中逐样本递推，中途不截回 float，并提高 MPFR 精度检查参考收敛。分别比较原实现与候选到该参考的误差；候选与原 float 的输出差异另作兼容性指标，不把不同直接当作更差，不重新用高精度设计系数。
- 已有验证：GCC/Clang 与 float/double 的四种组合均完成 release 构建，原有独立标量探针（count 0–64）和启用原有断言的验证通过。独立探针与实验脚本留在本地证据目录，未把旧的 318 行探针或实验产物装入代码提交。
- 证据目录：`/root/code/fluidsynth-iir-compact-20261027/compact-delivery/` 保留初版 double 的配对 micro 数据；`float-study/` 保留 MPFR、精度消融及最终速度证据；`real-length-study/` 记录真实曲目长度分布和原始计数。
- 真实长度采样覆盖 7 组 MIDI/SoundFont 组合；各自 float/double 共 14 次完整离线渲染，所有短于 64 样本调用合计占活跃 IIR 调用约 `0.01025%`，处理样本约 `0.00433%`。因此不增加专门的短块生产特化；本次未把 instrumented profile 时间用于性能结论。再提 PR 前仍需精简必要的自动回归测试、核对届时上游，并按最终源码复查 PCM 与完整渲染收益。用户安排后再提 PR，不自动创建或按日期催审。

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

本表同时列出审计前的独立修复、已从审计包提取的修复、性能和 CI 工作。生产差异只计 `src`；CI 差异计相关工作流/工具。除精简 IIR 分支以 `18bcb76f` 为基线外，数值相对各分支与声明上游 `c7ba7b49` 的 merge-base；仍需在最终 rebase 时重新检查。

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
| [perf/iir-stable-block](https://github.com/Yikai-Liao/fluidsynth/tree/perf/iir-stable-block) | 精简 double W4，单提交暂存；float 统一实现继续验证，PR 延后 | S | +43 / −5 |
| [perf/dev-iir-stable-block](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-iir-stable-block) | 旧 double W4 与历史证据保留；由精简分支承接，不重复排 PR | L | +129 / −0 |
| [perf/dev-midi-track-merge](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-midi-track-merge) | MIDI 跨轨事件 heap merge 备选实现；随 player 搁置 | L | +84 / −58 |
| [ci/dev-windows-build](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-windows-build) | Windows CI 子模块与 MSYS2 依赖精简；以后再说 | L | +27 / −12 |
| [ci/dev-android-migration](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-android-migration) | Android CI 迁移与构建/测试工具整合；等用户有空 review | L | +647 / −751 |
| [ci/dev-android-developer-probe](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-android-developer-probe) | Android API 35 Debug 与消费端独立 probe；等用户有空 review | L | +708 / −751 |
| [refactor/dev-dls-boundaries](https://github.com/Yikai-Liao/fluidsynth/tree/refactor/dev-dls-boundaries) | DLS 边界、存储、WSMP 与 voice 生命周期重构；后放 | L | 混合/重构包，见下文 |
| [refactor/dev-dls-boundaries-minimal](https://github.com/Yikai-Liao/fluidsynth/tree/refactor/dev-dls-boundaries-minimal) | DLS minimal 版与后续分配失败处理；与原重构比较后再决定，后放 | L | 混合/重构包 |

## 需要保留的区别与依赖

- `perf/iir-stable-block` 是基于 `18bcb76f` 的精简 W4 单提交快照；当前仅优化 double，float 统一实现正在验证。`perf/dev-iir-stable-block` 的 +129 行生产代码、+318 行旧测试和历史端到端证据继续保留。两者是同一项待办，与已合并的系数复用 #1867 不同；最终提 PR 前仍需整合最小测试并重查 PCM、完整渲染及届时上游。
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


## 2026-10-28 夜间音频正确性排查（进行中，仅记录，不修复）

核对上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`，本批于北京时间 01:40 确认。仅追加此前清单之外的问题；未修改生产源码、未发上游 issue/PR。Linux Debug、double、OpenMP 关闭，使用普通合法 MIDI/API 与原版 GeneralUser GS 2.0.1。以下三项已完成独立复核；没有将定向测试通过等同于全量通过，也未证明真实曲库发生率。P1/P2 是本轮使用场景排序，不是安全严重度。

### N01 / P1：CC121 错误清除声道 RPN 调音，后续新音符失去移调

- **场景与影响：** 标准 RPN 0,2 设置粗调，或 0,1 设置微调，再发送 Reset All Controllers（CC121）。已有音符保留调音，但后续新音符回到未调音音高，可导致旋律意外移调或与持续和声失谐。无需 Mono、特殊引擎模式或定制音色；需先调音后复位，不能称为所有普通 MIDI 都受影响。
- **预期依据：** [MIDI RP-015](https://midi.org/response-to-reset-all-controllers) 要求保留 registered/non-registered 参数值，复位的是参数选择器；[RFC6295 A.4.2.1](https://www.rfc-editor.org/rfc/rfc6295.html#appendix-A.4.2.1) 明确说明同一规则。本项测试为 SF2，不涉及 DLS2 例外。
- **原因：** [fluid_chan.c:127–132](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_chan.c#L127-L132) 在区分 CC121 复位范围前无条件清零全部 channel generator；[CC121 调用处](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L2002-L2007) 不保留调音值。[RPN 调音赋值](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L2111-L2118) 正是这些 generator。
- **验证：** GeneralUser Flute（零基 program73），key69、velocity100、44.1kHz，关闭效果隔离音高。粗调 +12 的 channel getter 在 CC121 前后从12变0；微调 +50 cents 从50变0。微调参考与持有音符的1秒窗口主峰均453Hz，复位后新音符440Hz；参考与持有音符 PCM 完全一致。频谱分辨率1Hz，不以此宣称样本绝对调律精度。公开 API 流程和实际 PCM 已独立重跑。
- **去重/边界：** 不同于 F31 气息同步、F32 sequencer 消息、F49 连奏选区及 #1879 清键。相关五项既有 controller/tuning/voice 测试通过，但未覆盖此场景。尚无真实设备或曲库发生率统计；不把同根的其他 NRPN 参数另计新问题。

### N02 / P2：连奏复用 voice 时，键号调制声像停留在上一音符

- **场景：** 原版 GeneralUser GS 2.0.1，bank0/program6 Harpsichord；CC126=1，默认 multi-retrigger；key60/velocity100 渲染4410帧，再重叠按 key64。44.1kHz，无自定义调音或人工调制器。Poly 模式下显式 CC68=127 也复现；普通 Poly 且未开连奏不受影响。
- **实际/预期：** 既有 key→pan 调制 amount250，应从 key60 的 −15.625 变为 key64 的0；两个复用 voice 的 key 已为64，pan.mod 却仍 −15.625。发送数值未变的 CC10=64 才修正为0。全新 synth、相同事件和相同4410帧窗口 A/B，仅 B 在第二个 note-on 后补同值 CC10：前段能量相同，后段右/左能量比从0.539979dB变为0.941670dB。这是声像数值变化证据，不是主观可闻性结论。
- **原因：** [fluid_voice.c:1283–1315](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_voice.c#L1283-L1315) 在更新 key/vel 后仅按 velocity 重算通用调制器，再单独更新部分键号缩放与 pitch，遗漏通用 key 调制源；函数注释承诺更新 key/vel 相关生成器。与 velocity 共用目的地的 key 调制可能间接刷新，不能称全部 key 调制失效。[SF2 2.04 §8.2.1](https://www.synthfont.com/sfspec24.pdf) 定义 note-on key 调制源。
- **验证/去重：** 同时核对实际音频与 voice 状态，并独立重跑。Church Organ program19 的60→62也复现；48→60跨区域例不触发。不重复 F49 的自定义调音跨区域问题。属于真实常用资产中的合法连奏条件，但未证明高发生率；未测 float/设备或全量回归。

### N03 / P2：单个效果组关闭后，独立 wet 输出仍漏出左声道原始发送量

- **场景：** `synth.effects-groups=2`，组0混响或合唱关闭，组1同效果开启；新建 synth 后仅 channel0 播放普通音符，CC91/93=127；`fluid_synth_process` 使用8个清零独立效果缓冲、2个干声缓冲，渲染8192帧。全部 API 成功，无旧尾音，组1无音符。
- **预期/实测：** 关闭组的左右 wet 输出应为零。[原 API 讨论](https://github.com/FluidSynth/fluidsynth/pull/673#issuecomment-723129007) 区分关闭与原样 bypass。GeneralUser 的关闭组混响左 RMS=0.0061446049、合唱左 RMS=0.0043832512，右均为0；两个组全关时严格为零，全开时正常双声道 wet，所有对照干声一致。上游 VintageDreamsWaves-v2.sf2 亦复现。客户端混回 wet 时会多叠加左声道干声，改变音量与声像。
- **原因：** [mixer:307–322](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rvoice_mixer.c#L307-L322) 按“任一组开启”的全局标志建立所有组原始 mono send；[混响205–207](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rvoice_mixer.c#L205-L207) / [合唱241–243](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rvoice_mixer.c#L241-L243) 跳过关闭组却未清除原始左输入，最终被作为 wet 交付。
- **范围：** 独立重跑六种条件确认，现有清单无重复；需要显式多组与独立效果输出，不是默认单组播放的普遍问题。`write_float` 内混效果路径不受此项直接影响。未测 JACK 实设备、float、OpenMP；不涉及 LADSPA 或非法输入。

以上为已核实问题记录，不代表修复已实施。后续排查继续在本节追加，未证实线索不列为确认缺陷。

### N04–N08：补充确认的条件性渲染问题

共同基线：上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`，Linux Debug double；合法事件、正常音源和串行公开 API。已独立复核，未修改生产代码、未尝试修复。以下不是默认播放普遍受影响的结论；MuseScore `MS Basic.sf3` 的补充用例均为人工构造控制实验，不计作真实 MIDI 曲库自然命中。

#### N04｜MTS 声道掩码未隔离共享调音（中优先级）

- 仅向 MIDI 声道 1 发送 +50 cents 的 Scale/Octave Tuning，未选的声道 2 也变调。GeneralUser Flute/key69 的主峰由 440 Hz 变为 453 Hz；实时、非实时消息均复现，音符均在消息后启动。官方 MS Basic 补充用例亦复现。
- [MTS 规范](https://midi.org/midi-tuning-updated-specification)第 6–7 页规定逐声道位掩码。[处理器](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L2652-L2666)先替换共享 tuning 0/0，再检查掩码；[替换逻辑](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L7289-L7330)传播至所有引用旧调音的声道。
- 需要专门的 MTS 调音消息；显式使用其他调音的声道不能笼统算作受影响。与 F18、F49 不同。

#### N05｜实时调音漏更新踏板保持的声音（中优先级）

- 同一音符收到实时 +50 cents 调音：手指仍按键时由 440 Hz 更新至 453 Hz；已经松键、由 sustain 或 sostenuto 保持时仍为 440 Hz。GeneralUser 与官方 MS Basic 补充用例均复现。
- [MTS 规范](https://midi.org/midi-tuning-updated-specification)要求更新已经发声的音符。[调音更新](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L7349-L7364)仅筛选 `fluid_voice_is_on`，而[该谓词](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_voice.c#L1601-L1620)排除已收到 note-off 的声音。
- 需要实时调音与踏板延音组合；不是 N04 的跨声道污染，也不代表所有调音 API 均已测试。

#### N06｜多输出 limiter 只处理第一对声道（中优先级）

- 显式设置 `synth.audio-groups=2`、`synth.audio-channels=2`、`synth.limiter.active=1`；效果器关闭，两组演奏同一和弦。以合法输出上限 0.1 测试：第一对受限并增加 221 帧延迟，第二对与未开启 limiter 的输出逐位相同。官方 MS Basic 补充用例第二对峰值仍为 0.28473/0.25976，首对约为 0.1。
- [设置文档](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/fluidsettings.xml#L164-L204)描述末端输出限幅与 look-ahead 延迟；[调用处](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rvoice_mixer.c#L271-L279)只传入第一对缓冲区，未遍历其他组。
- 后续干声输出既不限幅又与首对错位；这不是独立 wet 输出不进入 limiter 的设计限制。仅验证内存多声道输出，未验证物理设备；MIDI 文件本身不能开启此宿主配置。

#### N07｜顺序切换渲染 API 丢失缓冲余样（罕见，低实际优先级）

- 同一线程、效果器关闭：先 `fluid_synth_process` 输出 65 帧，再 `fluid_synth_write_float` 输出 63 帧。第二次实际输出与连续参考的第 128–190 帧完全一致，而非第 65–127 帧，跳过 63 帧。仅用其中一种 API，以及 64 帧边界切换，均与参考逐位一致。
- [process 尾部](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L4618-L4627)只更新 `cur`；[write 路径](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L4765-L4785)依据未同步的 `curmax` 决定重新渲染。
- [合成线程文档](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/usage/synth_context.txt)未禁止串行混用，但也没有明确承诺混合 API 的连续性；预期基于连续渲染契约推断。应用中途切换渲染接口的实际需求未证实，不能宣传为常见播放错误。

#### N08｜进入 sustain 阶段后调整 sustain level 不生效（中优先级，发生率未证实）

- GeneralUser Organ/program19 持续发声 1 秒后，以 `fluid_synth_set_gen` 或正常 SF2 NRPN 增加 `GEN_VOLENVSUSTAIN` 偏移 240：调用成功、参数更新，但包络保持 1，后续同时间窗能量与未修改控制组完全相同。相同偏移在 note-on 前设置时包络为 0.76，确认参数本身有效。
- [公开 API 文档](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L7770-L7785)明确说明立即应用于当前和后续声音；[实现](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_voice.c#L1080-L1088)仅更新 decay 阶段边界，已进入 sustain 的包络不跟随。
- 需要实时 API/NRPN 调整该参数；没有常用曲库自然命中证据，也未声称在 MS Basic 复现。只记录“不响应实时变化”，不将历史 EMU 包络缩放近似另算新问题。
