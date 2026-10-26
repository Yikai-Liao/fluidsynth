# FluidSynth fork 待办

更新：2026-10-26。此 `todo` 分支是独立根历史，只放任务记录，与 FluidSynth 代码历史没有公共祖先。代码工作在自己的 `origin` fork；`upstream` 远程仅用于获取基线和核对合并状态。

## 分支约定与当前阶段

- `fix/dev-xxx`、`perf/dev-xxx`、`ci/dev-xxx`、`refactor/dev-xxx`：未完成工作，完整保留 Git 提交历史，允许继续迭代。
- 去掉 `dev-` 的对应分支：review 完成后基于当时的上游，整理为一个完整提交，再提 PR。开发分支不提前 squash。
- 已有开放 PR 的 head 分支保留原名，避免影响 PR；目前是 macOS CI 和 SF3 gain。
- `archive/previous-work`：集中保留旧分支的完整提交历史及原名称映射。原来的日期分支、重复版本和已停止实验分支从分支列表清理；所有原始提交仍可追溯。
- 本阶段先完成分支结构与待办整理。下面的 dev 分支还保留原开发基线，**不等于已经完成测试精简、rebase 或上游提交 review**。审计包后续仍要按主题拆分；DLS 按整体重构系列 review。

整理后的结构为 **25 个 dev 工作分支、2 个开放 PR 分支，以及 master / todo / archive/previous-work，共 30 个分支**。旧的 100 个分支头均保留在归档历史中。

## 优先顺序

先 review 生产改动小、后果较明确的修复；S/M/L 只表示初步人工 review 规模，不含构建和等待 CI。

1. 审计包中的 F14：注销已释放 synth 的设置回调，生产 +2，现有生命周期测试补 +3 即可。
2. `fix/dev-sample-guard-frame-overflow`：生产 +6，已有 sample validate 测试可扩展。
3. 审计包中的 F18、F30、F09/F10：分别是合法 unpitched SF2 根音、采样结束边界、调音名称缓冲区；先精简夹具并合入适合的现有测试。
4. `fix/dev-midi-zero-tempo`、`fix/dev-timer-thread-handle-release`、`fix/dev-shell-server-mutex-release`、`fix/dev-midi-track-count`。
5. `fix/dev-shared-synth-init`：并发首次构造的后果较重，等待和原子可见性需额外 review。
6. 其余控制器/效果器修复，再到 player、DLS、性能和 CI 较大工作。

测试整理要求：先复用现有测试文件和夹具；确需独立文件时说明原因。命名、缩进、注释和断言沿用邻近测试。对精简后的关键回归做修复前失败/修复后通过的对照，避免只凭删行数判断有效性。

## 审计前的积压与当前工作分支

本表同时列出审计前的独立修复、性能和 CI 工作。生产差异只计 `src`；CI 差异计相关工作流/工具。所有数值相对各分支与声明上游 `c7ba7b49` 的 merge-base；仍需在最终 rebase 时重新检查。

| 工作分支 | 内容 | review 规模 | 生产/CI 差异 |
|---|---|---|---|
| [fix/dev-all-notes-off-mono-keys](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-all-notes-off-mono-keys) | All Notes Off 清除 mono held keys | M | +7 / −0 |
| [fix/dev-basic-channel-count-bounds](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-basic-channel-count-bounds) | basic channel 数量校验与加法溢出 | M | +2 / −8 |
| [fix/dev-dattorro-sample-rate](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-dattorro-sample-rate) | Dattorro 采样率变化时重建状态 | M | +25 / −2 |
| [fix/dev-dither-table-init](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-dither-table-init) | 共享 S16 dither 表并发初始化 | M | +20 / −19 |
| [fix/dev-initial-rate-iir-table](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-initial-rate-iir-table) | 采样率变化后更新 IIR 查表 | M | +1 / −0 |
| [fix/dev-jack-rate-change](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-jack-rate-change) | JACK 采样率变化时停止不匹配渲染 | L | +48 / −18 |
| [fix/dev-midi-track-count](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-midi-track-count) | MIDI 轨道数符号扩展与边界 | S | +6 / −1 |
| [fix/dev-midi-zero-tempo](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-midi-zero-tempo) | 拒绝 MIDI 零 tempo | S | +6 / −0 |
| [fix/dev-player-control](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-player-control) | MIDI player 暂停恢复与 tempo 请求 | L | +81 / −111 |
| [fix/dev-player-track-order](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-player-track-order) | 跨轨全局 tick 顺序与 seek 重建 | L | +53 / −63 |
| [fix/dev-relative-clock-init](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-relative-clock-init) | 相对时钟 origin 的并发首次初始化 | M | +10 / −0 |
| [fix/dev-sample-guard-frame-overflow](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-sample-guard-frame-overflow) | 采样 guard-frame 分配尺寸溢出 | S | +6 / −0 |
| [fix/dev-shared-synth-init](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-shared-synth-init) | 并发构造时等待共享 synth 初始化完成 | M | +18 / −3 |
| [fix/dev-shell-server-mutex-release](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-shell-server-mutex-release) | shell server 失败与关闭时释放 mutex | S | +2 / −0 |
| [fix/dev-short-loop-interpolation](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-short-loop-interpolation) | 短循环的高阶插值 tap wrap | L | +68 / −0 |
| [fix/dev-smf-sysex-continuations](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-smf-sysex-continuations) | SMF SysEx 续包与 escaped MIDI 时序 | L | +120 / −55 |
| [fix/dev-timer-thread-handle-release](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-timer-thread-handle-release) | timer join 后释放线程句柄 | S | +3 / −1 |
| [fix/dev-sostenuto-sustain-handoff](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-sostenuto-sustain-handoff) | CC66 抬起时转交仍按下的 sustain | M | +7 / −2 |
| [fix/dev-audit-fixes](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-audit-fixes) | 审计混合包：51 项成立问题，待按主题拆分 | L | 混合/重构包，见下文 |
| [perf/dev-iir-stable-block](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-iir-stable-block) | double IIR 稳态四采样展开（W4） | L | +129 / −0 |
| [perf/dev-midi-track-merge](https://github.com/Yikai-Liao/fluidsynth/tree/perf/dev-midi-track-merge) | MIDI 跨轨事件 heap merge 备选实现 | L | +84 / −58 |
| [ci/dev-windows-build](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-windows-build) | Windows CI 子模块与 MSYS2 依赖精简 | L | +27 / −12 |
| [ci/dev-android-migration](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-android-migration) | Android CI 迁移与构建/测试工具整合 | L | +647 / −751 |
| [ci/dev-android-developer-probe](https://github.com/Yikai-Liao/fluidsynth/tree/ci/dev-android-developer-probe) | Android API 35 Debug 与消费端独立 probe | L | +708 / −751 |
| [refactor/dev-dls-boundaries](https://github.com/Yikai-Liao/fluidsynth/tree/refactor/dev-dls-boundaries) | DLS 边界、存储、WSMP 与 voice 生命周期重构 | L | 混合/重构包，见下文 |

## 需要保留的区别与依赖

- `perf/dev-iir-stable-block` 是尚未提 PR 的 double-only W4 优化，生产 +129、已有测试 +318。它与已合并的 IIR 系数复用 #1867 不同。历史报告有通过的质量与性能门槛，但后续仍需精简/整合测试，并基于当前上游重新核对；不直接沿用旧基线的性能结论。
- `ci/dev-windows-build` 是尚未提 PR 的 Windows CI 整理，单个 workflow +27/−12。既有多个开发提交在 dev 分支完整保留。
- Android 迁移整合稿在 `ci/dev-android-migration`；独立 API 35 developer probe 在 `ci/dev-android-developer-probe`。旧的 cicd-work、maintainability、superbuild 版本在归档保留，先比较差异再决定是否还要移植；不将同一迁移重复算多项 PR。
- `fix/dev-player-track-order` 与 `perf/dev-midi-track-merge` 是正确性方案与 heap 备选实现；需要比较后决定最终实现，不能重复宣传同一修复。
- 旧 `fix/midi-track-duration-20261021` 通过拒绝超 INT_MAX 曲目解决计数问题；审计 F35 改为内部 64 位继续播放。旧方案已归档，优先 review F35，再决定旧用例如何移植。
- 两版 sostenuto/sustain 修复保留较早但独立的工作分支 `fix/dev-sostenuto-sustain-handoff`；另一版含不同旧基线和部分测试差异，完整原历史仍在归档。
- DLS 的四个重构提交保留在 `refactor/dev-dls-boundaries`，继承的审计包也保留。先核对依赖再整理到干净上游基线；不把 13 个 DLS 审计条目自动拆成 13 个 PR。
- `fix/dev-audit-fixes` 保留原审计与两次复审的历史，并加入 F18 测试夹具精简提交。F18 独立测试暂由 211 行减到 74 行；本轮 double/float Debug ASan/UBSan/float-cast-overflow 均通过，撤销修复或将 fallback 错设为 61 均失败。下一步检查合入现有 sample/pitch 测试的合适位置。

## 用户补充与移出队列的工作

- **sinc：维护者自行处理。** 不继续 review 或提 PR。两个旧实现分支及证据在归档历史中保留。
- **未定位的时间偏移修复：** 用户记得声音或效果整体提前/延后约一秒，可能与效果器有关；明确排除 Dattorro 4 ms predelay 的 #1872。已检查当前分支标题、效果器修改和主要实验报告，尚未找到能确认匹配的记录；保留为待定位，不能用 player 或 predelay 的近似问题替代。
- 早期无稳定收益、质量门槛未过或仅 instrumentation 的 perf/explore/measure/investigate 原型已移入归档。SoA IIR 是搁置的架构实验，不列入当前提交队列；需要恢复时可从归档取回完整历史。

## 已提上游的工作

| 工作 | 状态与分支处理 |
|---|---|
| [#1877 SF3 gain](https://github.com/FluidSynth/fluidsynth/pull/1877) | open；保留 `fix/sf3-sample-gain-20261021` |
| [#1876 macOS CI](https://github.com/FluidSynth/fluidsynth/pull/1876) | open；保留 `ci/macos-intel-arm` |
| [#1878 silent-loop](https://github.com/FluidSynth/fluidsynth/pull/1878) | 关闭未合并，作者以 [#1871](https://github.com/FluidSynth/fluidsynth/pull/1871) 覆盖为由关闭；旧分支归档，不重提 |
| [#1863 voice reuse](https://github.com/FluidSynth/fluidsynth/pull/1863)、[#1864 Dattorro tap](https://github.com/FluidSynth/fluidsynth/pull/1864)、[#1867 IIR](https://github.com/FluidSynth/fluidsynth/pull/1867)、[#1868 realtime MIDI](https://github.com/FluidSynth/fluidsynth/pull/1868) | merged；旧实现/证据分支归档 |
| [#1870 Linux CI](https://github.com/FluidSynth/fluidsynth/pull/1870)、[#1872 predelay](https://github.com/FluidSynth/fluidsynth/pull/1872)、[#1873 runner](https://github.com/FluidSynth/fluidsynth/pull/1873)、[#1874 Ubuntu/Arm](https://github.com/FluidSynth/fluidsynth/pull/1874)、[#1875 Alpine](https://github.com/FluidSynth/fluidsynth/pull/1875) | merged；旧分支归档 |

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
