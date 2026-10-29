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
- **重复 CC66（F46）提高优先级（2026-10-28 用户决定）**：已有真实 MIDI＋MuseScore MS Basic 音源的原生播放证据，列入下一批优先考虑提交的修复 PR。修复保留在 `fix/dev-sostenuto-repeat`（`54817529`），尚未提上游 PR；此调整不代表授权立即提交。SF2 固定力度 0、Breath Sync 仍为低优先级候选；Breath Sync 需显式启用。单个真实样本不等于高发生率。
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
| F46 重复 CC66；`fix/dev-sostenuto-repeat` | Sostenuto 保持按下时重复发按下值，错误捕获后来的音符 | +9/−7 | +8 | 提高至下一批优先考虑；真实 MIDI＋MS Basic 已复现。提交前重查基线、逐声道转发与边沿测试；尚未授权立即提 PR |
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
| [fix/dev-sostenuto-repeat](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-sostenuto-repeat) | F46：重复 CC66 不重新捕获后来音符；已 review，用户提高至下一批优先考虑，尚未提 PR | S | +9 / −7 |
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


### 真实 MIDI 快速筛查：第一批结果（2026-10-28 北京时间 02:13）

- **语料与分母：** [Lakh Clean 官方数据页](https://colinraffel.com/projects/lmd/)的 17,256 个 MIDI，按 SHA256 去重后仍为17,256；严格原始事件解析成功16,985、拒绝271。成功文件中16,564个多声道、421个单声道。另有5个既有音乐文件便利样本单独统计，不混入 Lakh 比例。文件级比例不代表所有 MIDI、所有风格或独立作品的发生率。
- **原始事件结果：** 5,365/16,985（31.5867%）含 CC121，共62,440条；非默认 RPN 调音30个文件，其中3个文件随后出现 CC121（共5次），但没有后续受影响新起音。三份原始曲目完整诊断回放均与保留调音的对照 PCM 一致；原生 `fluid_player` 再核对事件顺序，同样没有受影响新音。**N01 合法构造成立，但本批未发现自然触发；不能用31.59%当缺陷发生率，也不能据此列为常见严重问题。**
- **其他筛查：** MTS消息0；Mono/连奏宽松前置条件54个文件，不等于54个音频问题。大量 CC126 在默认 basic-channel 设置下被引擎拒绝，不能通过另改宿主配置把它们算成自然命中。旧 F46 仅1个文件出现重复CC66前置条件，实际音频影响仍在核对，不计新问题数量。
- **MuseScore 音源：** 官方 MuseScore `main` 固定提交 `7a43b88219259fecc44a23cdeccd5489dc042fc8` 的 `MS Basic.sf3`，SHA256 `5ea2375e8bd7d8e71def1036978c1621e85b66934169b6a2744b27b9b3c2d99c`，已在 libsndfile/SF3 支持开启的干净上游构建中实际加载渲染。此音源没有非零 key→pan，因此 N02 的 GeneralUser 声像症状不能直接套用；同根其他 key 调制仅作为候选。德沃夏克原始文件原生 player 中出现8次旧 key→attack 状态，部分理论时长可能差1块，但尚未证实 PCM/可闻影响，不计确认音频命中。
- **Symusic：** 已实际使用0.6.0，单进程扫描全部17,261文件用时13.13秒。Lakh 加载成功17,032、拒绝224；其 Score 中5,053文件含CC121（29.67%）。但在共同成功的16,981文件中，原始事件有5,365文件含CC121、Score仅5,045：320个原始阳性被曲谱表示省略。独立核对例为10cc/Dreadlock Holiday.mid：原第10轨无音符但有9条CC121，Score未保留该控制轨。因此 Symusic 适合快筛，不能仅凭 Score 阴性排除原始控制器/RPN/SysEx 条件；精确事件统计保留原始事件兜底。未把该语义差别认定为 Symusic 缺陷。
- **交付与范围：** 可复用双层扫描器已打包，含脚本、依赖、复现说明、来源/hash清单、逐文件CSV、汇总和解析器比较；16项状态检查、10个真实文件打包烟测、断点续跑与ZIP校验通过。无 MIDI/SoundFont/PCM 版权载荷或生产修改。7份代表/近似命中文件已完成原生 player + MS Basic 输出检查，均有限非零音频；这不等于全曲库音质验证。
- **后续取舍：** 按用户要求，继续发现其他普通合法场景的渲染问题；常见程度只保留快速筛查和少量关键对照，不为单项发生率无限延长验证。尚未证明的影响直接注明。多效果组、多个输出与渲染API切换属于宿主配置条件，不能从 MIDI 文件出现率推导发生率。

### N09–N13：补充确认的音频状态与参数问题

共同基线仍为 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`。以下均使用合法输入，已独立复跑控制用例并核对源码；未改生产代码或尝试修复。不以构造用例代替真实曲目发生率。

#### N09｜Signalsmith 混响 width 被缩小 100 倍（中优先级，需选择该引擎）

- 公共[设置文档](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/fluidsettings.xml#L454-L461)规定 width=1 为完整立体声展开，推荐范围 0–1；[Signalsmith 实现](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rev_signalsmith.cpp#L119-L127)却先除以 100。
- 普通音色演奏时，width=1 的左右差信号幅度仅为 width=100 的约 1%；默认 width=0.8 仅约 0.8%。width=0 为单声道，中央信号不变，五档结果符合错误混合比例。不是不同混响算法音色差异的主观判断。
- 仅影响显式选择 `smith` 的路径，不声称默认混响引擎受影响或真实曲库常见。

#### N10｜Lexverb 调整输出 level 时重置延迟线位置（中优先级，需选择该引擎）

- 相同新建 synth、相同 MIDI 与渲染时间；播放后读取当前 level，再原值写回。Lexverb 下一段输出从首帧起改变，最大差约 0.006401、差值 RMS 约 0.001370；未调用 setter 的重复控制，以及 dat/free/smith 的原值写回控制均逐位一致。
- [参数更新](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rev_lexverb.cpp#L67-L88)每次重新调用延迟线 `set_buffer`；[后者](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rev_filters.h#L88-L100)即使长度不变也重置读写位置和上一输出，改变现有尾音状态。
- 将 level 减半也出现超出纯增益变化的差异，但以“原值写回仍改变声音”为主要证据，避免把正常平滑过渡误报。需要实时 API/界面控制，不是默认 MIDI 播放必发。

#### N11｜chorus 输出 level setter 重启调制相位（中优先级）

- 默认 chorus 实现，普通音色、CC93=127。播放后将当前 level 原值写回，下一段最大 PCM 差约 0.027724、差值 RMS 约 0.007265；不调用 setter 的同时间控制逐位一致。仅减半 level 也不等价于保持原状态的半幅输出。
- [输出 level API](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L6680-L6700)进入[无条件参数重建](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_chorus.c#L788-L793)，[重建函数](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_chorus.c#L510-L562)重置调制中心与振荡器初相位，而非只改变输出幅度。
- 涉及宿主/API/界面调整 chorus 参数；普通 MIDI CC93 控制的是发送量，不能直接算作本问题触发。与 N10 的延迟线位置重置是不同根因。

#### N12｜主增益降为零后，仍按住的音符永久消失（条件性宿主 API 问题）

- GeneralUser 与官方 MS Basic 的 Organ/program19，按住 key60 发声 1 秒，将主增益设为 0 持续 100 ms，再恢复 0.2：活跃 voice 从 1 变 0，之后 PCM 完全无声。未静音、CC7 静音后恢复，以及主增益仅降至 `1e-5` 的控制均保留声音。这里没有 note-off 或复音数压力。
- [文档](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/fluidsettings.xml#L147-L156)将 gain 描述为实时主输出增益，合法范围含 0。[voice 增益处理](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_voice.c#L1879-L1898)把零夹至极小正值；[噪声门限](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rvoice.c#L870-L884)随之提高，[静音裁剪](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rvoice.c#L51-L94)永久结束声音，没有考虑之后恢复增益。
- “恢复音量应恢复仍按住的音符”基于输出增益语义；文档并未单独承诺 mute/unmute 行为。普通音量滑块操作可触发，但未测实际应用频率。与 F38 的非法 NaN 增益不同；MS Basic 结果仅为补充控制用例。

#### N13｜不同 transform 的调制器被错误合并（中优先级，罕见合法配置）

- 合法公开 API 添加同一双极 CC14→pan 的两个调制器，amount 均为 250，一个 linear、一个 absolute。CC14=0 时两者应抵消；实际添加顺序使 pan 从控制组 −25 变为 −525（右声道无声）或 475（偏右），两次添加均成功。
- [SF2.04 规范 §9.5](https://www.synthfont.com/sfspec24.pdf)第 54 页明确把 transform 纳入调制器身份，并禁止将不同身份的 amount 直接合并。[`fluid_mod_test_identity`](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_mod.c#L750-L759)遗漏 transform，导致默认调制器合并路径混淆二者。
- 验证使用正常音源与自定义合法调制器，没有改造音源文件；已检查的 GeneralUser/MS Basic 未提供此 transform 组合，不声称默认音色或真实曲目命中。与 F13 的旧地址问题不同。

#### N14｜切换鼓组后，排他类别错误截断上一 preset 的声音（中优先级）

- 官方 MS Basic.sf3，默认鼓声道9：program0 Standard 演奏 open hi-hat/key46，100ms 后以普通 `fluid_synth_program_change` 切换 program56 Marching Snare，再演奏 cymbal/key74。两者恰好都使用 exclusiveClass1，但属于不同 preset；旧踩镲被强制缩短并提前结束。
- [SF2 规范 §8.1.3、generator57](https://musescore.org/sites/musescore.org/files/2023-01/sfspec24.pdf)将排他类别作用域限定在同一个 preset。[匹配代码](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L5465-L5493)只检查声道、类别和 note-on 分组 ID，未区分声音所属 preset。
- 原生 PCM 与公开 voice getter 验证：旧声音的 exclusiveClass 从1变0，volEnvRelease 从5830变−2000，随后0.5秒内结束。只切 program、不发第二音时旧声音继续；第二音改放另一个明确选择相同鼓组的声道时旧声音也继续。后一控制与出错组的混音 RMS 分别为0.00338003和0.00282601；根因依据是状态与生命周期，不仅是整体音量差。普通 program-change 与 `program_select` 两套流程的三组 PCM 均逐位一致，已独立复核。
- 需要旧声音仍在响时切换到复用相同排他编号的另一个 preset；固定鼓组播放不受此项影响。尚无真实 MIDI 曲库命中率证据，不称为所有普通播放普遍发生。不同于 F40 的非法极端类别与 F49/N02 的连奏问题；未修改生产源码。


##### N14 真实数据集补充（2026-10-28 续查）

- 本次在未改写的已发布数据集文件中确认 N14，不新增根因编号。上游换为 `f784f7833acbd07181ea3d3bd6b0d16253b883f4`，420个源码文件与官方归档逐字节一致；官方 MS Basic.sf3 哈希仍为 `5ea2375e8bd7d8e71def1036978c1621e85b66934169b6a2744b27b9b3c2d99c`。
- 文件来自 [Tegridy 固定版本](https://github.com/asigalov61/Tegridy-MIDI-Dataset/tree/0278ae2bff273c192cca0fa9f3f87999a1b86dc5) 的 `Mono-Melodies/Tegridy-Mono-Melodies-CC-BY-NC-SA.zip`，成员 `Flute/063deb6ba2e0838b1b9f53ca45309b45.mid`，SHA256 `418a2a09abac4340cba1e2ab45ebbc36ed0ed67c9b9d174dffa8ce6e605c70bf`。这是发布的加工数据集成员，不冒称原作曲者未经处理的原稿；本次未改 MIDI、音源或引擎。
- 三个独立原生 sample-clock player 完整播放88.891791秒：默认回调、仅观察的身份控制、单事件诊断参考。track10 在tick2880以实际preset128:24播放key46，tick2976先切preset128:0再播放key42；旧声部仍有release尾音，二者class均为1。全部相关轨为port0，PC与起音同轨有序。
- 诊断参考仅在该自然NoteOn（event284）前临时屏蔽不同实际preset旧声部的exclusiveClass，随后恢复；不改事件、时间或顺序。这是基于规范作用域的因果对照，不是独立参考渲染器或生产修复。身份控制逐样本相同；单事件对照在效果关闭时39,546个声道样本不同、最大差0.007956913；保留默认效果器时也确认差异、最大差0.007956116。首次差异frame372929；回调记录的块起点不是精确事件采样时刻。
- 触发NoteOn返回成功；两侧另有相同的89次velocity0 NoteOn（NoteOff表示）返回失败，无正力度NoteOn失败，不隐瞒这些数据/声部状态限制。所有PCM有限。效果尾音会放大非零差异样本数，不能据此推断可闻严重性或发生率。此前“无自然命中”结论仅对应前一批；现在至少有这个发布文件的确认，仍不代表普通播放普遍受影响。


## 已知 F46：真实 MIDI 与 MuseScore 音色的补充证据（非新增问题）

- 上游：`18bcb76f48bd24a383bff19240dc39dd3a9d4651`；未修改引擎。
- 原文件：Lakh Clean MIDI `The Beatles/Tell Me Why.mid`，SHA256 `bac9be13b344c0b3b2515ac6321f407f5047be33aa1f4c4021478f48f65d1110`。未改写文件、未向观察组注入事件。
- 音色：MuseScore 官方 `MS Basic.sf3`，SHA256 `5ea2375e8bd7d8e71def1036978c1621e85b66934169b6a2744b27b9b3c2d99c`；原生 SF3 加载。
- 方法：两个独立原生 `fluid_player` 按 sample 时钟播放同一完整 SMF，分别驱动两个相同 synth，64 帧交替离线渲染。对照先不改变任何事件；诊断参考仅忽略同一通道 CC66 已高时的重复高值。参考不是实际数据集文件，也不是生产代码修复。
- 身份对照：134.0169 秒、5,910,144 帧、6,823 个事件，双声道 PCM **逐样本完全相等**；两轮均有限数值、无回调错误。
- 诊断参考忽略 63 次重复 CC66 高值。与观察组相比，9,287,933 个声道样本不同，最大绝对差 0.128918，差值 RMS / 参考 RMS = 0.382852；首次不同在第 1,266,176 帧（约 28.71 秒），晚于首次过滤事件。该指标说明实际渲染不同，不等同于主观听感评分或 63 个额外延音。
- 自然序列包含 MIDI 通道 2、program 24 的新音符→重复 CC66=127→Note Off→CC64 释放。例如 tick 17037–17040 新音，17042 重复 CC66，17200–17225 Note Off，17230 松开 sustain；符合既有 F46 机制。文件无 SysEx、无 velocity=0 的 Note On。
- **限制：** JSON 的 `voice_state_differences` 在第二播放器回调内部采样，可跨越同一块内不同事件位置，不能作为可靠的延音数量或独立状态证明，报告不使用该项。旧 `native-f46/` 单播放器双 synth 实验未通过身份对照，作废；仅 `native-f46-v2/` PCM 结果有效。
- 数据集意义：已有问题至少获得 1 个自然文件的原生音频差异验证；不是新发现，也不是所有 MIDI 的发生率估计。若以本次 16,985 个成功扫描文件为分母，已确认样本下界为 1/16,985（约 0.0059%），并非总体流行程度。

验证脚本与原始结果保存在本轮证据包；不上传或再分发原始 MIDI/音源。

## 无异常基线与 RPN 近似命中

`native-panel/00.json`–`06.json`：4 首分层选取的普通文件与 3 首 RPN→CC121 近似命中文件，全部以原生 SMF player + MS Basic 完成播放，PCM 有限且非零。3 首近似命中文件在非零调音被 CC121 重置后均没有同通道后续 Note On，故不计 N01 自然命中。7/7 成功仅验证管线及这几项检查，不代表全曲所有渲染行为正确。

#### N15｜原值写回 LFO 频率反转下降方向（中优先级）

**优先级：中等；需实时频率 API / SF2 NRPN，不宣称普通文件默认播放普遍触发。**

- 基线：上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651` 未修改，Debug double / no OpenMP；原版 GeneralUser GS 2.0.1 Flute（bank0/program73），44.1kHz，混响和合唱关闭。
- 用例：CC1=127，note69/velocity100，先渲染4096帧。读取 `GEN_VIBLFOFREQ` 声道 offset=0，再通过 `fluid_synth_set_gen()` 把原值0写回（返回成功）；与不调用 setter 的同时间线、新 synth 对照比较接下来4096帧双声道 PCM。两次未改参数对照逐位相同。
- 实测：写回前振荡值0.205624、increment=−0.0284822、freq generator值−884、mod=0、nrpn=0、vibrato深度49.6094 cents。原值写回仍改变8188/8192个样本，最大差0.06803024560213089、差值RMS=0.024798774866253329、参考RMS=0.019046271648827204。这不是 setter 改变频率数值的正常结果。
- 阶段对照：只渲染2048帧、LFO处于上升段时（increment为正），同样原值写回与对照逐位一致。等价正常 SF2 NRPN：CC99=120、98=24、38=0、6=64，四个调用均成功，offset仍为0，也产生相同差异。没有添加自定义 modulator 或改写音源。
- 源码原因：`src/synth/fluid_voice.c` 的 GEN_VIBLFOFREQ / GEN_MODLFOFREQ 分支每次从频率算出正 increment；[fluid_lfo.c:3–8](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_lfo.c#L3-L8) 不保留振荡器原方向，直接赋值。正常振荡更新在 `fluid_lfo.h` 中通过改变 increment 符号维护三角波方向，因此下降段被错误切成上升段。
- 预期与边界：SF2 的 freqVibLFO 参数描述频率；公开 generator API 描述实时 offset，不是重置振荡器。认为不改变频率的原值写回应保持同一演奏波形，是由参数语义与正/负方向对照支持的契约推断；文档没有单独承诺相位连续性。本次直接动态验证 vibrato LFO，modulation LFO 仅共享源码路径，未另算一个问题。
- 发生率限制：检查的 MS Basic、GeneralUser 及本地 SF2 样本没有非零频率目的地22/24的 modulator；没有真实 MIDI 自然命中证据，API/NRPN 的使用频率未统计。PCM差异不是主观听感评级。
- 已独立复核正常 API、等价 NRPN 和上升段阴性对照，未修改生产代码。

#### N16｜CoreAudio 多输出的 FX 数量不匹配导致静音（中优先级，待真实平台验证）

- **条件：** CoreAudio、`synth.audio-channels=3`、`synth.audio-groups=3`，保持默认 `synth.effects-groups=1`；设备支持对应的六声道输出。合法多输出设置，不涉及异常输入。
- **预期及源码：** [CoreAudio 多输出实现](https://github.com/FluidSynth/fluidsynth/pull/1081) 的用途是按 MIDI channel 路由到多组硬件输出。驱动读取 stereo-pair 数并乘二（[348–355](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/drivers/fluid_coreaudio.c#L348-L355)），设置非交错 Float32 多声道格式（[429–435](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/drivers/fluid_coreaudio.c#L429-L435)）。三个普通 MIDI channel 的音符应能在三组输出发声，而不应要求增加独立效果器组。
- **静态原因：** [callback 548–559](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/drivers/fluid_coreaudio.c#L548-L559) 清空输出后，把硬件 mono buffer 数同时作为 `nfx` 和 `nout`；六声道时 `nfx=6`。但默认一个效果组仅提供四个 mono FX buffers，[process 4490–4495](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L4490-L4495) 拒绝该组合。callback 忽略失败返回并报告 `noErr`，留下全部零输出。
- **功能验证（非真实 Apple 硬件测试）：** 从该 SHA 原样提取 callback 函数体，在 Linux 上仅用等价 CoreAudio 结构类型和正常非交错 buffer 几何调用，链接同 SHA 的原生 synth 库。每个 mono buffer 均分配完整 8192 帧、设置一个声道；普通 VintageDreamsWaves-v2.sf2，各 MIDI channel Note On `60+i/100`。1/2 stereo pairs + effects-groups=1 所有输出非零；3/4 pairs + effects-groups=1 所有输出 RMS 精确为 0；3/4 pairs + effects-groups=2 恢复非零。所有 callback status 均为 0。该验证覆盖 callback 参数生成、库拒绝和静音结果，但没有在 macOS 或真实 CoreAudio 设备上运行，设备协商路径尚未实测。
- **去重及频率：** 上游 CoreAudio callback/effects 检索未找到对应修复；不是普通双声道播放普遍故障，也不能从 MIDI 曲库推算发生率。仅限多输出 CoreAudio 主机配置，保持中优先级且保留平台验证限制。
- 已独立检查抽取回调与六组正常缓冲几何实验；未修改生产源码。平台验证范围严格限于上述内容。

#### N17｜MIDI router 的负倍率规则把音符映射高一个半音（条件性，中优先级）

- **触发条件：** 应用启用合法的自定义 router 负倍率，例如反向键盘 `router_par1 0 127 -1 127`。默认直通规则不受影响；不是普通 SMF 默认播放问题，未统计实际使用率。
- **预期依据：** [官方命令文档](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/fluidsynth.1) 及 [rule API 注释](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/midi/fluid_midi_router.c#L432-L469) 定义先乘 `mul` 再加 `add`；未禁止负倍率。整数输入 60 应映射为 `60×(-1)+127=67`，不存在浮点舍入歧义。
- **实测：** 正常 public API 规则/Note On 输入 60→68、64→64（应63）、127→1（应0）；输入0、1同时映射127。无操作规则、+12移调与固定键67的控制组全部符合预期。
- **音频验证：** 原生 MuseScore `MS Basic.sf3` flute/program73、velocity100、效果关闭：输入60经此规则的PCM与直接播放68逐样本完全相等，与应有67不同；稳态频谱峰416Hz对393Hz。反向旋律实际错高半音，并且边缘键发生重合。
- **原因：** [router.c:729–782](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/midi/fluid_midi_router.c#L729-L782) 对 channel/param1/param2 都先无条件加0.5再转整数，负乘积向零截断，产生+1误差。这里仅对音符param1的声音影响作实测，不把其余同根字段另计问题。
- **去重：** 不同于 F01 的跨通道 pending-note 记录；本测试没有规则清除中的待释放音符、没有重叠音符。独立复核已接受。只建立外部测试，未修改引擎。
- **基线与证据：** 上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`；音色SHA256 `5ea2375e8bd7d8e71def1036978c1621e85b66934169b6a2744b27b9b3c2d99c`。`router_negative_scale.py` / `router_negative_scale_results.json` / `router_positive_controls.json`；`router_negative_audio.py` / `router_negative_audio_results.json`。音频测试是明确标注的合成有效控制序列，不计自然曲库命中。

#### N18｜Mono／CC68 切换音色后叠加旧音色，切回时首音漏发（中优先级）

**优先级：中等条件性问题；真实普通音色，无自定义调音或 modulator，但需要显式 Mono 或 CC68 连奏和演奏期间 Program Change。**

- 基线：上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`，未修改生产源码，Debug double / no OpenMP，GeneralUser GS 2.0.1，44.1kHz，关闭效果器。
- 流程：program19 Church Organ，note60/velocity100，渲染4096帧；Program Change73 Flute，再 note62/velocity100，渲染4096帧；释放旧 key60，保持新 key62，再渲染120×4096帧（约11.15秒）。PC和note调用均成功。
- 普通 Poly 控制：旧 Organ 维持原 key60，不被新音符改调，收到 off60 后结束；后期只剩 Flute62，双声道末4096帧与相同时间线的仅Flute参考逐位一致。
- CC126=1、默认 multi-retrigger，或 Poly 下 CC68=127：旧 Organ voice被改为key62，仍保留原Organ sample，同时分配新Flute62；off60后11.15秒仍是两种音色一起发新key62。与仅Flute参考相比，末窗8192个样本全部不同，最大差0.024141151458024979、差值RMS=0.0099215758816053011。
- 预期边界：Program Change允许已经发声的旧音符保持原音色；问题不是旧尾音继续发声，而是旧音色被重新定到新key并与新程序同时发声。本项不称“所有键松开后无限挂音”；主对照窗口内新key62仍按着。
- 根因：`src/synth/fluid_synth_monopoly.c:670–719` 仅凭旧voice的key/velocity区域决定复用，没有确认该区域属于当前preset；把旧zone标为ignore后，又调用新preset的note-on，新preset不会消费旧zone的ignore请求。
- 同根额外后果（已独立复核）：上述流程再释放key62、等待11.15秒使全部旧声音结束，切回program19并重新note60；Mono/CC68调用成功，但首个新音符没有voice，4096帧PCM严格为0；普通Poly同流程正常发声，能量0.1159562303372396。旧zone残留的ignore在`fluid_defsfont.c:2050–2061`第一次查询时才被消费，跳过本应发声的区域。不另计第二个问题。
- 已独立复跑 Poly、Mono 与 CC68 三组，检查同时间窗 PCM 和 voice 生命周期；未修改生产源码或音源。
- 范围：无真实曲库发生率统计；不混淆F49自定义调音跨区域或N02键号modulator未刷新，也不同于跨preset exclusiveClass。没有修改SoundFont或实现修复。

### N12 的实际宿主边界补充

已只读追踪 MuseScore 开发提交 `7a43b88219259fecc44a23cdeccd5489dc042fc8` 对应 `muse_framework` 子模块 `ecdc6e7ef87b41c3fd2ce4e9df39343640862c4f`：普通轨道/主音量滑块通过控制节点对合成后的音频乘增益，[输出处理代码](https://github.com/musescore/muse_framework/blob/ecdc6e7ef87b41c3fd2ce4e9df39343640862c4f/framework/audio/engine/internal/nodes/automationcontrolnode.cpp#L119-L138)不调用 FluidSynth 主增益 API。[SoundFont 包装器](https://github.com/musescore/muse_framework/blob/ecdc6e7ef87b41c3fd2ce4e9df39343640862c4f/framework/audio/engine/internal/synthesizers/fluidsynth/fluidsynth.cpp)仅初始化固定 `synth.gain=4.8`，没有 `fluid_synth_set_gain` 调用。因此不能拿该版本 MuseScore 普通音量滑块当 N12 的现实触发实例。静音按钮有节点停用/解除静音后 seek 逻辑，不保证所有持续音保留，也不能混同 N12。此为源码核对，未做 UI 或发布版本验证；该框架随附的是适配的 FluidSynth 2.3.3，不等于本轮受测上游 SHA。

N12 必须直接将 FluidSynth 增益设为0、继续渲染仍在保持的声音、再恢复增益才进入已测条件。外部混音器静音以及本轮 CC7 静音/恢复控制不属于该路径；暂不把它列为已证实常见宿主问题。

#### N19｜演奏中配置 custom filter 后，活动音符持续旁路（中优先级，显式 API 场景）

- **条件：** 普通 SoundFont/音符，通过 `fluid_synth_set_custom_filter()` 在活动音符上启用滤波、切换 low/high-pass，或写回原配置。custom filter 默认关闭，属于应用显式使用功能，不能称为自然 MIDI 文件普遍命中。
- **基线：** 上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`、Debug double；VintageDreamsWaves-v2.sf2，关闭 reverb/chorus 分离干声，channel0 `GEN_CUSTOM_FILTERFC=7000`、`GEN_CUSTOM_FILTERQ=0`，Note On60/100。flags=0，Q 使用默认 centibel 转换，0 对应正常非共振 Q，不是 `FLUID_IIR_Q_ZERO_OFF`，也不是非法参数。
- **预期：** [公开 API 8310–8342](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth.c#L8310-L8342) 配置独立 low/high-pass 并报告成功，且实现明确遍历现有 voices。启用或切换类型应让活动音符使用该滤波器；写回原配置不应永久关闭滤波。
- **实测：** 渲染8192帧后调用 setter，再渲染8192帧。原本关闭→启用 LOWPASS 的整个后段与保持关闭的对照逐位相同；原本 LOWPASS→同值 LOWPASS、LOWPASS→HIGHPASS，在丢开1024帧过渡后也都与关闭对照逐位相同，而与保持 LOWPASS 的对照最大差=`0.0549271926`。保持配置不动的重跑逐位相同；同值配置后额外写回同一个 Q=0，则过渡后恢复与正常 LOWPASS 逐位相同。可见不是普通系数平滑误差，而是 Q 更新丢失造成持续旁路。
- **源码原因：** [`fluid_voice_set_custom_filter:2079–2082`](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_voice.c#L2079-L2082) 仅调用 filter init；[`fluid_iir_filter.c:25–49`](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_iir_filter.c#L25-L49) 对启用类型执行 reset，把 `last_q` 清为0，却没有重新应用当前 Q generator；[`fluid_iir_filter_impl.cpp:172`](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_iir_filter_impl.cpp#L172-L175) 对 `last_q<Q_MIN` 直接旁路。新音符初始化时会更新 Q，所以主要影响已有活动音符。
- **去重及限制：** 原51项、custom-filter 上游 issues/PR 检索未发现相同修复；相关功能PR331/1469/1567并非此问题。与已知启动采样率 IIR 查表更新不同。未验证应用使用率或自然曲库发生率；只记录，不修复。
- 六组模式均已独立复跑，所有 API 返回成功；未修改生产源码。

#### N20｜Mono/Legato 的 Note Off 提前消耗 CC84，滑音应用到错误事件（条件性，中优先级）

- **合法场景：** CC68=127 启用单音连奏，依次按住60和67；发送 CC84=72，松开67后再按69。松开67会返回仍按住的60，但不应消耗原本留给下一条收到的 Note On 的 CC84。
- **预期依据：** MIDI1.0 详细规范4.2.1第16–17页明确规定 CC84 仅作用于匹配通道下一条收到的 Note On，并在该 Note On 后复位。[MMA 原文镜像](https://www.seriesten.org/docs/protocols/MIDI_1.0_Detailed_Specification.pdf)。[FluidSynth PolyMono 文档 §3.4.11](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/polymono/FluidPolyMono-0004.pdf) 也将其描述为下一音符的起始滑音键。
- **实测：** 通过公开 API 发送上述事件，`fluid_synth_get_cc(...,84)` 在松开67前为72，松开后变为内部已消费值255；期间没有收到新 Note On。将相同 CC84 延后到松开67之后、按69之前的诊断对照，触发前 PCM 逐样本相等，之后不同。GeneralUser flute 最大差0.06929、差值RMS0.01966；官方原生 MS Basic SF3 flute 最大差0.05815、RMS0.01672。所有测试使用正常控制值与现成音色，没有改引擎。
- **音频影响与限制：** 返回旧按键时提前发生由CC84指定起点的滑音，后续真实 Note On 又失去该一次性指定。PCM差值包含两段连奏路径的累积差异，不单独归因为下一条Note On，也不把差值大小当主观严重性。需要单音/CC68、仍按住旧键及特定合法消息顺序；没有自然曲库命中或发生率证明。
- **原因：** [fluid_synth_monopoly.c:398–423](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth_monopoly.c#L398-L423) 的 Note Off 回退进入通用 legato 路径；[同文件:169–179、658](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/synth/fluid_synth_monopoly.c#L169-L179) 无条件消费 PTC，没有区分外部 Note On 与内部 Note Off 回退。
- **去重与状态：** 不同于 F49 调音选区、N02 键号调制器、N18 换预设、#1879 清键；本例没有调音、程序更换或清键。独立复核已接受。正常对照还验证了 CC84 优先于 CC65 开关、源键 Note Off 不关闭已重定向的目标键、目标键 Note Off 能释放声音。
- **证据：** 上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`。`ptc_noteoff_consumption.py`、`ptc_noteoff_consumption_results.json`、`ptc_noteoff_consumption_ms_basic_results.json`；补充对照 `portamento_normal_controls_results.json`。MS Basic SHA256 `5ea2375e8bd7d8e71def1036978c1621e85b66934169b6a2744b27b9b3c2d99c`。以上均为明确标注的合成有效消息序列，不计入自然 MIDI 比例。

- **真实语料快查（不重跑全曲）：** 本批 Lakh 16,985 个成功扫描文件中，61个含 CC84（约0.359%，共2,437条）；这些文件无 CC126，唯一 CC68 同现为 Blind Guardian/Time What Is Time.mid 的 CC68=1（关闭），未检出 CC68 开启。按默认初始通道状态和已扫描控制器条件，没有找到 N20 所需 Mono/连奏前置条件；此为条件筛查阴性，不是已经做过61首原生音频对照。外部宿主预先设置 Mono 属于不同场景，不能据这批文件推断其发生率。

#### N21｜滑音时间改为0后，复用声部仍带着上一段滑音偏移（条件性，中优先级）

- **正常输入场景：** GeneralUser GS 2.0.1 organ/program19；CC68=127、CC65=127；CC5=8、CC37=0（自动模式切换为线性1024ms）。按60，100ms后按64，再过100ms把 CC5 置0，按67。没有程序切换、CC84、调音或清键。
- **预期依据：** [FluidPolyMono §3.4.9](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/polymono/FluidPolyMono-0004.pdf) 说明时间为0时滑音不可感知；[当前设置文档](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/doc/fluidsettings.xml#L388-L401) 将线性时间定义为 `CC5*128+CC37` 毫秒。因此下一音符应立即到达其目标音高，而非继续近一秒的旧滑音。
- **决定性实测：** 所有输入都经公开 API，读取声部状态仅作诊断。旧滑音在64上剩余偏移−360.906516音分；时间归零并按67、再渲染64帧后，目标键已变为67，但偏移仍为−360.339943音分、旧增量仍为0.566572音分/块。相同前史只把新时间改为1ms的控制组，64帧后偏移为0。原音色带八度移调，所以声部目标pitch=5500；问题是额外遗留约−3.6半音，不是这个音色自身的移调。
- **声音影响：** 本应立即到位的新音符错低数个半音并继续滑动。公开API渲染对照在最后按键前PCM完全一致，之后不同；但0ms与1ms本来可能有很小过渡差异，不能仅凭PCM不相等判错，主要依据是持续数百音分的旧状态。
- **根因：** [fluid_rvoice.c:748–770](https://github.com/FluidSynth/fluidsynth/blob/18bcb76f48bd24a383bff19240dc39dd3a9d4651/src/rvoice/fluid_rvoice.c#L748-L770) 仅在 `countinc!=0` 时更新滑音；零时长分支没有清除此前的 `pitchoffset/pitchinc`。新分配声部初值为0，复用的Mono/Legato声部则保留旧值。
- **条件/负向对照：** 依赖滑音尚未结束且后续按键复用同一声部。MS Basic program19 的60→64→67及60→64→62控制在新键上的实测偏移均为0，**未复现此问题**；不能宣称所有音色均受影响。没有自然曲库触发或发生率统计。与N20提前消费CC84、N18换预设、N02调制更新、F49调音选区及#1879清键不同。独立复核已接受GeneralUser场景。
- **证据：** 上游 `18bcb76f48bd24a383bff19240dc39dd3a9d4651`，未修改引擎。`portamento_zero.c`、`portamento_zero_results.txt`、`portamento_one_ms_results.txt`；公开音频测试 `portamento_zero_audio.py`及结果；MS Basic负向控制 `portamento_zero_ms_basic_results.txt`、`portamento_zero_within_zone_ms_basic.txt`、`portamento_one_ms_within_zone_ms_basic.txt`（文件名within_zone只是尝试意图，不证明实际复用）。GeneralUser SHA256 `e5d3617c427e94fe388980d83bc955359047a53842b60ffe5558df8bc19c0300`。


### N12 补充：Qsynth 的实际主增益调用路径（源码证据，非现场复现）

只读核对官方 Qsynth v1.0.6，固定提交 `ca7af9dff09a7a50f2a7a0a4218cce0bcf77a288`：其 [Master Gain 控件](https://github.com/rncbc/qsynth/blob/ca7af9dff09a7a50f2a7a0a4218cce0bcf77a288/src/qsynthMainForm.ui#L103-L119)范围允许0；数值除以100后经更新定时器，调用[现有 synth 的 fluid_synth_set_gain](https://github.com/rncbc/qsynth/blob/ca7af9dff09a7a50f2a7a0a4218cce0bcf77a288/src/qsynthMainForm.cpp#L2430-L2438)。[增益更新路径](https://github.com/rncbc/qsynth/blob/ca7af9dff09a7a50f2a7a0a4218cce0bcf77a288/src/qsynthMainForm.cpp#L2766-L2787)没有停播、reset 或销毁驱动，控件在 synth 存在时可用。因此“播放时把 Master Gain 转到0，再调回”有具体真实宿主的源码路径支持，不只是人为想象的 API 组合。

此处不等于已在 Qsynth 界面、真实设备或某首实际曲目中复现；是否恰有仍保持的声音、安装的 FluidSynth 版本及使用频率均未现场核实。也不把该控件称为已验证的独立 mute 按钮。MuseScore 普通音量滑块的前述阴性调用链结论不变。

### 二次契约审查补充

对本轮记录另做只读源码、完整 API 警告及历史 issue/PR 复核，未发现新的足以撤销已编号条目的反证。仍应区分：N07 混合渲染 API 连续性、N10/N11 参数原值写回的状态连续性、N12 输出增益恢复、N15 频率原值写回相位连续性均含契约语义推断，不包装成文档逐字承诺；N16 有多输出默认单效果组的公开配置依据，但没有 Apple 硬件验证。N09 只主张公共 width 定义与实际缩放不一致，不要求不同混响引擎的听感相同。已知 #964、#1695 等继续不计新增。


### 正常路径与 WITH_FLOAT 补充验证（非新增问题）

- 同一上游 SHA 的独立 Debug/GCC14.2 WITH_FLOAT 构建，实际配置确认 SF3/Vorbis 支持、OpenMP关闭、无 fast-math，原生 MS Basic SF3 加载成功。N12 主增益零后丢音、N14 跨 preset 排他截断、N18 切回旧音色首音漏发均复现；N18 探针使用匹配 float 生成头与库，未拿 double 布局读内部状态。没有测试全部21项的 float 版本。
- float 正常控制：6组真实音色 voice 复用逐样本一致，6组和弦/单键释放线性叠加最大误差4.657e-9；6份48kHz双声道文件及S16/S24/S32量化/饱和符合对应误差界；原有 voice-reuse 测试通过。这些是同精度内对照，不宣称 float/double 逐位一致。
- 普通 preset 分层/生成器/调制器优先级检查：GeneralUser 287与MS Basic309个 preset，代表键位、区域分界及力度1/64/100/127，共67,676次起音、121,338个 voices，未发现选区数量/合并规则差异。此为采样的状态与结构检查，不是每个键/力度的完整 PCM 音质测试；oracle 的默认生成器及默认调制器仍部分依赖引擎定义，不能据此验证默认定义本身。
- 普通循环波形15组、真实音色叠层释放16组、动态采样加载18组均通过限定对照；实际原生 sequencer 的32组分块/变速配对及定时音符替换/取消控制通过。分块对照两侧共享同一引擎，只说明这些路径的一致性，不独立证明所有时间映射。
- 原生 MIDI player→文件渲染：3个正常有限曲目场景、13组对照中，末音/踏板释放/尾音正确交付；64/256帧仅有合法末块补齐，实际CLI导出与API结果逐位一致。固定2秒尾音等待及默认reset符合当前设计，不把预期尾音裁剪误报。
- 上游源码在这些验证结束时仍为 `18bcb76f48bd24a383bff19240dc39dd3a9d4651` 且工作树干净，没有实施生产修复。详尽脚本、配置、对照指标在本轮可复现证据包；阴性结果仅适用于列明范围，不代替全平台回归。


### 续查语料与已知播放器限制（2026-10-28）

- C++ MiniMIDI（`a9139ce7c15c3bf4c4237cc46259ad09a1852151`）全量静态扫描：[Aria固定版本](https://huggingface.co/datasets/loubb/aria-midi/tree/8cc4cf5c83b47f2689ac256a947b2a57c17a4c8b)的已下载归档实际含1,142,478个MIDI，全部解析成功；页面宣称1,186,253个，差43,775个，属尚未解释的页面/完整归档差异，不是扫描或下载缺失，不能把宣传数字作为已测数。Tegridy已获取113,520个成员，按文件字节SHA去重后106,795个，106,318成功、477输入失败明确记录。这里是文件/事件扫描，不是百万曲完整音频对照；数据包含转录、加工及生成内容，版本/音乐近似重复未按作品去重。
- **已知时间漂移，不新增问题：** 在 `f784f783` 原生 sample-clock player 上，两个未改写Tegridy成员（SHA前缀`451adcc4`、`640b5666`）分别含240/192条tempo，9,526/6,774条音符全部匹配；末音事件相对原文件tempo积分提前720.141ms/37.833ms。单tempo原文件控制504事件误差0–1.415ms。首例另用独立原始字节解析与有理数积分核对。这是事件派发时序，不是声学起音/主观音质测量；对应上游 [#1288](https://github.com/FluidSynth/fluidsynth/issues/1288)（bug、wontfix，关闭不代表修复），[维护者已确认限制](https://github.com/FluidSynth/fluidsynth/issues/1288#issuecomment-1837224176)。仅补充现状证据，不恢复搁置的播放器实现工作。
- **FF21端口不支持，不新增问题：** 非零/多端口和同声道跨端口Program Change冲突仅作为兼容性候选；对应开放功能请求 [#1361](https://github.com/FluidSynth/fluidsynth/issues/1361)及 [#1352的明确范围说明](https://github.com/FluidSynth/fluidsynth/issues/1352#issuecomment-2227310369)，不是新发现的实现违约。没有把候选文件数当作确认PCM故障数，也没有实现端口映射。

## 通用性能修复候选：legato 起始键哨兵的 signed-char 截断（2026-10-28）

- **定位与范围：** 已在未修改的上游 `f784f7833acbd07181ea3d3bd6b0d16253b883f4` 确认；这是普通 Poly Note On 路径也会进入的无效声部扫描，不是仅针对 Black MIDI 的特化。实际开销取决于音符事件数、配置的 polyphony 及编译目标上 plain `char` 的符号性；不据此声称普通曲目都有显著收益或存在音质错误。
- **根因：** [`fluid_synth_get_fromkey_portamento_legato()`](https://github.com/FluidSynth/fluidsynth/blob/f784f7833acbd07181ea3d3bd6b0d16253b883f4/src/synth/fluid_synth_monopoly.c#L164) 返回类型为 `static char`。在本次 GCC Release 构建（`CHAR_MIN=-128`，未使用 `-funsigned-char`）中，`INVALID_NOTE=255` 返回后符号扩展为 `int fromkey=-1`；反汇编已确认 `movsbl`。[`fluid_channel_is_valid_note(n)`](https://github.com/FluidSynth/fluidsynth/blob/f784f7833acbd07181ea3d3bd6b0d16253b883f4/src/synth/fluid_chan.h) 仅检查 `n != 255`，因此误把 −1 视为有效键。
- **后果：** 普通 Poly、没有有效 legato 起始键时，本应跳过的 [legato 声部搜索](https://github.com/FluidSynth/fluidsynth/blob/f784f7833acbd07181ea3d3bd6b0d16253b883f4/src/synth/fluid_synth_monopoly.c#L658-L670) 仍遍历全部 `synth->polyphony` 个槽位；无符号 voice key 不可能匹配 −1。这是额外的 O(polyphony) 扫描；与实际需要的 Mono/CC68/CC84 声部复用不同，也不把普通无符号 char 目标算作同样受影响。
- **最小修复方向：** 将这个内部 helper 的返回类型改为 `unsigned char` 或 `int`，避免 255 被截成负值；预计生产改动一行。保留有效键 0–127，以及 helper 的 CC84 消费、portamento 状态更新等副作用，不直接绕过整个 helper，不夹带声部索引重构。
- **安排与证据边界：** 按用户要求记入待办，作为独立小型性能修复 PR 候选；不依赖 Black MIDI 优化，也不并入已有 IIR 工作。当前记录确认的是根因与多余扫描，单独一行修复的配对性能测量和回归结果仍待完成，暂不填写加速倍数。提交前需覆盖 signed/unsigned-char、无效哨兵与有效键边界、普通 Poly 以及 Mono/legato/CC84 正常行为，并核对 PCM 与届时上游。本次仅更新待办文档，没有发布引擎修复或创建 PR。

### 同项验证进展（2026-10-28，补充上述待验证状态）

- **本地最小候选已实现：** `fluid_synth_get_fromkey_portamento_legato()` 的返回类型由 `char` 改为 `int`；独立于批处理原型的生产 diff 确认仅这一行，无公共 API 或对象布局变化。生产补丁 SHA256：`38a4e17f7f20b6fb7ff8e2f86cc3acdb18cb6b7846e539b25b1a0b6570a048ba`。
- **实际通过的检查：** 单独一行修复 Release 44/44；120 行批处理原型叠加此修复的 Release 44/44、ASan＋UBSan 44/44（LSan 关闭）。后两项是组合版本验证，不冒称单独修复的 sanitizer 结果，也不代表全平台或 unsigned-char 配置已经覆盖。
- **定向回归：** 在现有 `test_voice_reuse_gain.c` 增加35行，验证无效起始键255不会访问 voice pool；有效 PTC=60 仍释放相应 voice、设置 `fromkey_portamento=60` 并消费 CC84。负向对照将 voice pool 故意设为 NULL 来检查“不应访问”：旧 char 实现退出 −11（SIGSEGV），int 修复退出0。这个崩溃是测试夹具刻意捕获多余访问，不是普通播放会崩溃的证据。含测试补丁 SHA256：`80d679b7b22e18d2e51e89fe80be8e75794213718e2ff20852b79a2ccd9959e7`。
- **仍待补足：** 单独修复相对未修改上游的配对性能、PCM 与更全面的模式/键边界验证；不把批处理叠加修复的单次诊断计时写成独立修复或通用性能收益。上述全套测试通过不替代这些专项验证。候选仍仅本地保留，本次只补充 `todo` 文档，未推送引擎代码或创建 PR。

## 三项独立 dev 快照已保存（2026-10-28）

按用户安排保留开发历史，不提前整理成最终 PR 分支；下列三个提交均**直接且仅以上游 `f784f7833acbd07181ea3d3bd6b0d16253b883f4` 为父提交**，彼此不叠加。没有夹带 IIR W4、atomic、DLS 或其他优化，没有创建 PR。此前“候选仅本地”的状态由本节更新；原始120行版本单独保存，后续继续优化也不覆盖这个快照。

| 分支 | 已验证远端提交 | 生产差异 | 测试差异 |
|---|---|---|---|
| [fix/dev-legato-invalid-note-sentinel](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-legato-invalid-note-sentinel) | [031121d2](https://github.com/Yikai-Liao/fluidsynth/commit/031121d2a945c3cc13e87f13e802884c1e74c25b) | 1文件 +1/−1，char→int | 现有 reuse_gain +35 |
| [perf/dev-lowest-free-voice](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-lowest-free-voice) | [2b896710](https://github.com/Yikai-Liao/fluidsynth/commit/2b8967109310bd1bc3dcc81a549c5c9f3ed947f1) | 2文件 +21/−0 | 现有 reuse_gain +56/−2 |
| [perf/dev-same-key-event-batch](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-same-key-event-batch) | [30f5a9a8](https://github.com/Yikai-Liao/fluidsynth/commit/30f5a9a8db11e28973d81674548c8a2ddb11a019) | 6文件 +120/−0 | 现有 callback +42、reuse_gain +106 |

### 语义与验证记录

- **哨兵修复：** 上节根因、负向对照及 PTC60 副作用测试继续适用。独立 Release 44/44；不要把组合版本的 sanitizer 结果写成独立分支结果。
- **最低空槽游标：** `free_voice_hint` 是最低 AVAILABLE 槽位的下界，不是活动声部计数。找到空槽后保留该索引而非递增，保持“已分配但未 start”再次分配的原有行为；primary 完成并 stop 后降低下界，逻辑缩容时夹紧。没有空槽时仍执行原完整溢出评分/偷音路径，保留评分、同分选择顺序与失败行为。连续初填可摊销到 O(P＋分配次数)，不宣称任意完成/复用交错或满池分配都为 O(1)。本机 LP64 探针 synth=96,392、voice=4,744字节，加入int利用既有对齐填充，实测尺寸不变；不是全架构保证。
- **游标回归：** 对原扫描 oracle 核对未 start 复用、最低空洞、逻辑缩容、实际扩容、普通复用、满池受害者、两个 rvoice 均忙时失败。独立 Release 44/44；批处理＋哨兵＋游标组合 Release 44/44、ASan＋UBSan 44/44（LSan关闭）。独立游标分支未宣称完成 sanitizer 全套。
- **120行批处理：** 保留同键事件批次的实现和原回退路径。现有测试覆盖同一原生 timer 边界下快路径与原数组回退的逐样本 PCM、回调数量/顺序，以及 sustain、generator、All Sounds Off、队列处理、polyphony改变和已进行的release在后续批次重新处理。最终 Release 44/44、ASan＋UBSan 44/44（LSan关闭）。这仍是 dev 版本，不等于完成最终上游 review。
- 三份最终 diff 均通过 `git diff --check`；发布前核对每个修改文件的基线 blob 与官方 f784 一致，发布后回读远端父提交、完整树差异与每个修改文件 blob，确认没有多余文件。作者为 Yikai Liao。构建为本地 Linux GCC；不把本地检查当作 Windows/macOS 等全部平台通过。
- 完整“生产＋测试”补丁 SHA256：哨兵 `80d679b7b22e18d2e51e89fe80be8e75794213718e2ff20852b79a2ccd9959e7`；游标 `a6d309f863d8f5977d24cc50f105e0c21f6e7230275cc6a54dd440ab44faf582`；原120行版本 `feedffd4c48bc070dd8aba602f1b4ca57b638ac4aeb1e953ad5bb1a587b6a541`。上述固定提交保存实际源码与测试，不依赖临时工作目录才能找回。

### 短片段诊断结果归档：不是三个独立分支的通用收益

保留本轮已测数值以免丢失，但不把它们当稳定 benchmark。输入为从 Lightstorm 派生的60ms稠密片段，原生 sample-clock 离线渲染，44.1kHz、polyphony=65,535；以下仅渲染前1,024或2,304帧，不是完整原曲，也不是保留原曲全部前史。诊断构建有计数观测；每格为一次观测，运行间波动明显，未做配对多轮中位数或置信区间。

| 渲染窗口 | 未改上游 | 仅120行批处理 | 批处理＋哨兵 | 批处理＋哨兵＋游标 |
|---|---:|---:|---:|---:|
| 1核，1,024帧（约23.22ms音频） | 33.139328s | 9.297991s | 3.252293s | 0.788754s |
| 4核，2,304帧（约52.24ms音频） | 120.258126s | 35.884194s | 20.190051s | 1.957146s |

- **表中后两列是组合实现，不是独立哨兵/游标分支的收益。** 即使最后一列也远慢于实时：音频秒/墙钟秒约0.02944和0.02669；没有宣称已经实时播放。独立哨兵/游标相对未改上游的稳定配对性能仍待测。
- 四个版本在相同窗口 PCM 逐bit一致且有限。1,024帧PCM SHA256为 `8c3996e7b022be82557d8d6b3160058c7a581ec46c38cf790b765dbfb357ece6`；2,304帧为 `9b2ca8ff3f791dd58686eafe5cbb36df42656b7910566bb9d75ec65fc0d2c512`。事件/起音/最终声部计数一致；NoteOn失败、write失败、队列满警告为0。各版本均有相同的7,677/15,279次 NoteOff失败返回，不能包装成所有事件都成功。
- 诊断源码/计数结果与本地原始日志继续保留；公开分支只包含项目源码和回归测试，不上传第三方 MIDI、SoundFont、PCM、二进制或内部笔记。上述表格是整理后的证据摘要，不用诊断库替代实际分支源码。
- 发布后已确认三个 SHA 各触发9项 push workflow；本节写入时仍有排队/执行中的 CI，不能视为远端全绿。此次目标是保留独立 dev 快照与证据；后续迭代沿用正常提交，不强推覆盖历史。

## 通道索引与 exclusive 扫描：独立开发分支及依赖（2026-10-28）

用户要求继续分别保存并更新待办；本次只保存以下两个主题，没有将已有120行批处理、空槽游标、哨兵修复、W4 或 atomic 混入。

| 分支 | 固定提交 / 父提交 | 增量范围 |
|---|---|---|
| [perf/dev-channel-voice-index](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-channel-voice-index) | [20bbca67](https://github.com/Yikai-Liao/fluidsynth/commit/20bbca67893c20d5b48e157280f8764fc6d2b3e0) / pristine `f784f7833acbd07181ea3d3bd6b0d16253b883f4` | 5个生产文件 +106/−3；现有 reuse_gain 测试 +93 |
| [perf/dev-exclusive-channel-scan](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-exclusive-channel-scan) | [24530a25](https://github.com/Yikai-Liao/fluidsynth/commit/24530a2589622635f28cab26566db7a3cf9fec2f) / 上述 `20bbca67` | **依赖通道索引**；自身仅 fluid_synth.c +7/−1、现有 reuse_gain 测试 +44 |

exclusive 分支相对 pristine 总计生产 +113/−4、测试 +137；不能把它说成只改7行且独立于通道索引。两个 dev 分支分别保留，不强推、不创建 PR；继续迭代时保留上述固定提交。

### 作用与兼容边界

- 通道索引用每通道 bitset 标记**已分配到该通道的槽位**，包括未 start 和无效键 voice，而非仅活跃发声列表。Pitch bend/CC 调制和全部调制更新仍按原槽位升序处理；原始通道255与 `NO_CHANNEL` 重合的行为保留完整扫描回退。回调中缩小 polyphony 时重新检查逻辑边界。
- 位图随 voice 初始化/换通道和 stop 维护；容量增长失败保留原索引，已分配槽位扩容与缩容都在测试内。索引增加每通道位图内存，不把“只遍历该通道”说成没有内存成本，也不承诺所有负载都加速。
- exclusive 优化仅借用此索引缩小候选集合，保留原 exclusiveClass 比较、同 note-on 分组排除、原始顺序和 raw255 回退。它不修复已有跨 preset exclusiveClass 语义问题，不顺带改变音符或调制行为。

### 实际交付源码的验证

- 从 pristine f784 重新移植：生产新增/删除行逐行对照已审查的增量完全一致，只去掉原实验堆叠版本的上下文；没有复制完整堆叠测试文件。通道测试自行设置 sample loop，exclusive 测试显式设置256通道，不依赖此前空槽/批处理测试的隐含初始化。
- **通道索引独立树：Release 44/44、ASan＋UBSan 44/44；exclusive-on-index 树：Release 44/44、ASan＋UBSan 44/44。** LSan 关闭；Linux GCC、double、OpenMP启用，无 fast-math；sanitizer 为 Debug、`-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`。这次是实际发布源码的独立构建结果，不借用早先堆叠版本45/45。
- 定向检查：候选顺序/成员与原 raw-channel 数组扫描一致；未 start/无效键调制、CC121、稀疏通道跨位图word、带成员扩容、回调中缩容、恢复容量、reset及完成回收、raw255。exclusive 检查不同通道不受影响、同note-on分组排除、effective class（含mod）、分数class原比较语义、released但仍playing声部和raw255。
- 两树 `git diff --check` 干净；独立复核源码与测试通过。发布后回读远端唯一父提交、全部修改文件 blob 和树差异，确认每树只涉及预期5个生产文件及1个现有测试文件。
- 移植后完整“生产＋测试”补丁 SHA256：通道索引 `9efcf2c55c89af74451212081120d4251ce23dc232dbf1c875d064eb584df160`；exclusive-on-index 相对 pristine 的完整补丁 `24a154bb6ada37e6cbcf5d7c8618227f43187cb8303f5581f75177b22ef91fa0`。实际源码和回归用例以以上固定远端提交为准。
- **性能边界：** 此次独立移植未重新测量端到端性能，不将实验堆叠版本的加速数字归给这两个独立提交；也不把测试通过当作所有曲目逐bit PCM证明。后续 benchmark 必须明确是否叠加其他优化。
- 两个新SHA各已触发9项 push workflow，记录时排队/执行中。前三分支已经发现 Windows mingw32 在未改动的 `fluid_sffile.c:2631` 触发既有 `SF3 sample conversion requires float-precision evaluation` 编译guard；各自其他11个Windows构建job成功。最低空槽分支另有 macOS26 MacPorts 依赖安装报 `Port libsndfile not found`，尚未进入项目构建/测试。不能把这些失败统称本轮优化回归或声称CI全绿；未为消除CI红灯混入修复，其他任务仍按实际状态核查。


## 2026-10-29：结束声部回收优化分别保存（dev 分支）

本轮按用户要求分别创建独立开发分支，并在既有 `todo` 历史上追加记录；没有强推、没有重写旧分支，也没有创建上游 PR。发布前重新核对上游 `master` 为 [`f784f783`](https://github.com/FluidSynth/fluidsynth/commit/f784f7833acbd07181ea3d3bd6b0d16253b883f4)。以下独立移植与此前用于测量的实验堆叠是不同源码身份。

### 独立源码与依赖

| 分支 | 固定远端提交 / 基线 | 本主题增量与范围 |
|---|---|---|
| [perf/dev-finished-voice-owner](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-finished-voice-owner) | [4c1445c9](https://github.com/Yikai-Liao/fluidsynth/commit/4c1445c9c098448d9f76d4b1673524465befa2f7)；唯一父提交为已发布的通道索引 [20bbca67](https://github.com/Yikai-Liao/fluidsynth/commit/20bbca67893c20d5b48e157280f8764fc6d2b3e0) | 4 个生产文件 +7/−6；现有 reuse_gain 测试 +183。依赖 stable `voice_index`，不引入空槽游标、same-key batch、lazy release、SPSC 或 C++ mixer。 |
| [perf/dev-mixer-finished-index](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-mixer-finished-index) | [b2045d6c](https://github.com/Yikai-Liao/fluidsynth/commit/b2045d6c36c5ee1508cef719c3a396f45a93fd66)；直接基于 pristine [f784f783](https://github.com/FluidSynth/fluidsynth/commit/f784f7833acbd07181ea3d3bd6b0d16253b883f4) | 原 C mixer，3 个生产文件 +39/−7；新 membership 模型测试 326 行及 1 行注册。没有其他性能分支依赖。 |

- owner 分支不是“无依赖单提交”：相对 pristine f784 共 2 个提交，包含已发布的通道索引。累计差异为 7 个生产文件 +113/−9、现有 reuse_gain 测试 +276；[仅 owner 增量](https://github.com/Yikai-Liao/fluidsynth/compare/20bbca67893c20d5b48e157280f8764fc6d2b3e0...4c1445c9c098448d9f76d4b1673524465befa2f7)与[相对 pristine 的累计差异](https://github.com/Yikai-Liao/fluidsynth/compare/f784f7833acbd07181ea3d3bd6b0d16253b883f4...4c1445c9c098448d9f76d4b1673524465befa2f7)分别可查。未为规避依赖而放松逻辑缩容的成员边界。
- mixer 分支只领先 pristine 一个提交；[完整独立差异](https://github.com/Yikai-Liao/fluidsynth/compare/f784f7833acbd07181ea3d3bd6b0d16253b883f4...b2045d6c36c5ee1508cef719c3a396f45a93fd66)。没有把已经验证的 owner+mixer 实验堆叠冒充最小独立补丁。
- 发布后回读远端 ref、唯一父提交、tree 和作者。owner 远端 tree `3b6924448a441bf37634cdac67264f2b2f7524a6` 与本地已测 `83b0354b` 完全一致；mixer tree `56e4b551e56acfae8f7d9af20d2f1a758df03b05` 与本地已测 `c51dba6b` 完全一致。GitHub API 生成的提交身份与本地不同，但已验证源码树相同；作者均为 Yikai Liao，提交注明 Codex 辅助。
- 本次发布后远端快照为 **50 个分支、38 个 dev 分支**；前文旧数量保留为当时记录。本次只新增上述两个分支并追加 todo，未重写既有历史。

### 语义、空间与新移植验证

- **owner：** 复用现有 `finished_cb_voice` 作为物理 rvoice 的不可变 owner，只在构造时写入；reset 和 callback setter 不再改写。每条完成记录先校验稳定索引、当前逻辑 polyphony 与数组身份，再执行原 primary/overflow 的解锁、sample 引用和计数处理，消除逐条全池查找。公共回调语义不变；私有 raw-rvoice callback 事件的冗余 slot 1 不再支持任意改写第一参数，不能当成新的公开兼容承诺。该增量不新增字段或结构体字节。
- **mixer：** 私有 renderer-owned `mixer_index` 令唯一成员按原 swap-last 顺序 O(1) 移除；`-1` 为已知未绑定，重复/异常成员进入永久 `-2` 保守路径，保留旧扫描的 `j++` 行为、重复完成通知和满容量替换顺序。没有拒绝原本支持的重复 start，没有改变混音、DSP、声部上限或音质。
- **独立 mixer 不是零空间增量：** 当前 LP64 布局探针中 double rvoice 为 1224→1232 字节，float 为 704→712，原字段偏移不变；每个物理 rvoice 增加 8 字节，一个逻辑 voice 的两份 rvoice 合计增加 16 字节。此前带 lazy-release 头部的实验堆叠可利用填充，这个结论不适用于此次 pristine 移植。
- **实际发布源码的新验证：owner Release 45/45、ASan+UBSan 45/45；mixer Release 46/46、ASan+UBSan 46/46。** Linux GCC、double、离线固定依赖；SF3 功能未启用。Release 开 OpenMP；sanitizer 关闭 OpenMP 与 leak detection，错误即停止。两者公共头不变；owner 的 409 个动态导出与父逻辑重链接参考一致，mixer 的 409 导出与已接受构建一致。
- owner 回归覆盖不可变 owner、callback 替换/清除、primary/overflow 换位、sample 引用、外来/空 owner、逻辑缩容/扩容与并行只读探针。mixer 模型覆盖唯一成员和末尾交换、活跃幸存者、重复成员旧扫描顺序、满容量 hole/duplicate 两种次序、pending 记录重加、重复回调、异常索引、resize、DSP reset、公共重复 start 及 256 轮确定性成员序列；删除 moved-index 更新和改变 duplicate `j++` 的两个负对照均被断言捕获。没有 TSAN 或全场景回调重入验证声明。

### 性能证据与 CI 边界

- 此前已验证的**实验堆叠**在固定 50,624-note 派生片段、8 核、polyphony 65,535 的 1.2 秒范围，用 4 对预先声明配对测量比较 owner `a65a729d/f66c` 与 owner+mixer `a758567e/df8c`：wall 中位数 2.98727→2.64992 秒，降低约 **11.29%**，4 对均同向；process CPU 19.43218→19.52933 秒（约 +0.50%，2/2 混合），**没有 CPU 降低结论**。包含 warmup 的 10 次 PCM 均一致；独立诊断 twin 的 .05/1.2 秒四项实际故障计数均为 0。
- 这只是恢复片段的派生尾音，1.2 秒仍有 5,577 活跃声部，**并非整曲、完全排空或实时达标**。上述数字不自动转移到本次两个独立源码分支；独立移植尚未重新测量端到端性能，也不把本地测试通过等同于所有曲目的逐 bit PCM 证明。
- 本次两个固定远端 SHA 各触发 9 项 push workflow。记录时 owner 仍排队/执行；mixer 的 Linux、FreeBSD、Alpine、OS/2、Solaris、文档已成功，iOS/macOS 仍执行。mixer Windows 仅 mingw32 cpp11 失败，其余 Windows 构建成功：实际日志在未修改的 `fluid_sffile.c:2631` 命中 pristine f784 已有的 `SF3 sample conversion requires float-precision evaluation` 编译 guard。未为消除红灯夹带其他修复，也没有声称 CI 全绿；继续按这两个精确 SHA 核查。
