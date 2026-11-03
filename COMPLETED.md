# FluidSynth 已完成与已结束工作

更新：2026-11-03。[返回待办](README.md)。这里只收录上游已合并或明确被替代、不再推进的项；fork-only、open PR、仅 CI 完成的工作仍留在主待办。历史段落的“当时待测”等措辞不代表当前状态。

## 当前结论与发布追溯

本轮用户 PR：17 个已合并、1 个关闭未合并；3 个开放 PR #1900/#1901/#1902 留在主待办。

| PR | 当前结论 | 合并提交 | 2.6.2 状态 |
|---|---|---|---|
| [#1895](https://github.com/FluidSynth/fluidsynth/pull/1895) Avoid skipping audio when mixer workers start late | merged 2026-11-01 | `32291359` | 已包含；见下方验证口径 |
| [#1886](https://github.com/FluidSynth/fluidsynth/pull/1886) Avoid unnecessary legato voice scans on signed-char targets | merged 2026-10-29 | `14669717` | 已包含；见下方验证口径 |
| [#1884](https://github.com/FluidSynth/fluidsynth/pull/1884) Add scalar fallback for SF3 float-to-S16 conversion on x87 | merged 2026-10-29 | `c90a865b` | 已包含；见下方验证口径 |
| [#1879](https://github.com/FluidSynth/fluidsynth/pull/1879) Clear mono held keys on All Notes Off and All Sounds Off | merged 2026-10-27 | `f784f783` | 已包含；见下方验证口径 |
| [#1877](https://github.com/FluidSynth/fluidsynth/pull/1877) Preserve SF3 sample gain during S16 conversion | merged 2026-10-26 | `18bcb76f` | 已包含；见下方验证口径 |
| [#1880](https://github.com/FluidSynth/fluidsynth/pull/1880) Honor sustain when releasing sostenuto | merged 2026-10-26 | `414d7471` | 已包含；见下方验证口径 |
| [#1863](https://github.com/FluidSynth/fluidsynth/pull/1863) Fix inconsistent note onset ramps when reusing a voice | merged 2026-10-18 | `2088d413` | 已包含；见下方验证口径 |
| [#1864](https://github.com/FluidSynth/fluidsynth/pull/1864) Reduce Dattorro output-tap indexing overhead | merged 2026-10-18 | `414fb866` | 已包含；见下方验证口径 |
| [#1867](https://github.com/FluidSynth/fluidsynth/pull/1867) Avoid redundant IIR coefficient calculations during slow cutoff ramps | merged 2026-10-19 | `2f913abf` | 已包含；见下方验证口径 |
| [#1868](https://github.com/FluidSynth/fluidsynth/pull/1868) Preserve MIDI parser state except on System Reset | merged 2026-10-19 | `0a7a2c97` | 已包含；见下方验证口径 |
| [#1870](https://github.com/FluidSynth/fluidsynth/pull/1870) Streamline Linux CI checkout and dependency installation | merged 2026-10-19 | `36807ccb` | 已包含；见下方验证口径 |
| [#1872](https://github.com/FluidSynth/fluidsynth/pull/1872) Fix the 4 ms predelay in Dattorro reverb | merged 2026-10-20 | `60dae2b9` | 已包含；见下方验证口径 |
| [#1873](https://github.com/FluidSynth/fluidsynth/pull/1873) Pin CI runner images to explicit OS versions | merged 2026-10-22 | `4a8f0b82` | 已包含；见下方验证口径 |
| [#1874](https://github.com/FluidSynth/fluidsynth/pull/1874) Update Linux CI to Ubuntu 24.04 and add native Arm64 coverage | merged 2026-10-22 | `c7ba7b49` | 已包含；见下方验证口径 |
| [#1875](https://github.com/FluidSynth/fluidsynth/pull/1875) Migrate Alpine CI to GitHub Actions | merged 2026-10-22 | `e5c94b1f` | 已包含；见下方验证口径 |
| [#1876](https://github.com/FluidSynth/fluidsynth/pull/1876) Migrate macOS CI to GitHub Actions | merged 2026-10-26 | `2469ec11` | 已包含；见下方验证口径 |
| [#1881](https://github.com/FluidSynth/fluidsynth/pull/1881) Reject zero tempo in MIDI files | merged 2026-10-26 | `b22ff26a` | 已包含；见下方验证口径 |
| [#1878](https://github.com/FluidSynth/fluidsynth/pull/1878) silent-loop | 关闭未合并；由 #1871 替代，不重提 | 不适用 | #1871 已列入发布说明 |

发布依据：[2.6.2](https://github.com/FluidSynth/fluidsynth/releases/tag/v2.6.2)，tag `c3b244dd`。#1863/#1868 直接由发布说明确认；#1864 用 GitHub compare 确认合并提交为 tag 祖先；其余 14 个已合并项用本地 Git `merge-base --is-ancestor` 确认。CI 改动“包含”指源码/工作流历史，不是运行时功能，也不表示历史 CI 失败被改成成功。

#1895：2026-11-01 合并 `32291359`，修复 mixer worker 启动晚导致遗漏首轮音频；发布说明明确收录。`fix/mixer-worker-first-job` head `9a22ee18` 保留历史。不与仍开放 #1902 stdatomic 性能优化混为一项。

## 原始完成记录与历史验证

以下移动自 todo README（`9b227dad`），保留原文及历史身份；其中 2026-10-30 的 PR 总数与时间快照已由上方当前结论取代。

## 已提上游的工作

| 工作 | 状态与分支处理 |
|---|---|
| [#1879 Mono All Notes Off / All Sounds Off](https://github.com/FluidSynth/fluidsynth/pull/1879) | merged（2026-10-27）；merge `f784f783`；`fix/all-notes-off-mono-keys` 与 dev 仅保留历史 |
| [#1884 SF3 x87 fallback](https://github.com/FluidSynth/fluidsynth/pull/1884) | merged（2026-10-29）；先合入 `fix-x87`（`c90a865b`），再经 [#1885](https://github.com/FluidSynth/fluidsynth/pull/1885) 合入 master（`fc1c55f4`）；分支仅保留历史 |
| [#1886 invalid-note 哨兵](https://github.com/FluidSynth/fluidsynth/pull/1886) | merged（2026-10-29）；merge `14669717`；`perf/legato-invalid-note-sentinel` 与 dev 仅保留历史；不再列为待提交候选 |
| [#1880 sostenuto → sustain](https://github.com/FluidSynth/fluidsynth/pull/1880) | merged（2026-10-26）；`fix/sostenuto-sustain-handoff`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1881 MIDI zero tempo](https://github.com/FluidSynth/fluidsynth/pull/1881) | merged（2026-10-26）；`fix/midi-zero-tempo`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1877 SF3 gain](https://github.com/FluidSynth/fluidsynth/pull/1877) | merged（2026-10-26）；`fix/sf3-sample-gain-20261021`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1876 macOS CI](https://github.com/FluidSynth/fluidsynth/pull/1876) | merged（2026-10-26）；`ci/macos-intel-arm`；已移出待 review 队列；分支与 dev 历史保留 |
| [#1878 silent-loop](https://github.com/FluidSynth/fluidsynth/pull/1878) | 关闭未合并；上游已通过 [#1871](https://github.com/FluidSynth/fluidsynth/pull/1871) 修复对应问题，不重提 |
| [#1863 voice reuse](https://github.com/FluidSynth/fluidsynth/pull/1863)、[#1864 Dattorro tap](https://github.com/FluidSynth/fluidsynth/pull/1864)、[#1867 IIR](https://github.com/FluidSynth/fluidsynth/pull/1867)、[#1868 realtime MIDI](https://github.com/FluidSynth/fluidsynth/pull/1868) | merged；历史与证据保留 |
| [#1870 Linux CI](https://github.com/FluidSynth/fluidsynth/pull/1870)、[#1872 predelay](https://github.com/FluidSynth/fluidsynth/pull/1872)、[#1873 runner](https://github.com/FluidSynth/fluidsynth/pull/1873)、[#1874 Ubuntu/Arm](https://github.com/FluidSynth/fluidsynth/pull/1874)、[#1875 Alpine](https://github.com/FluidSynth/fluidsynth/pull/1875) | merged；历史与证据保留 |

状态于 2026-10-30 通过 GitHub PR 元数据核对；上述 17 个用户 PR 中 16 个 `merged=true`，仅 #1878 `merged=false`。#1879、#1884、#1886 的最终 head 分别为 `2fac96b3`、`9981ef6e`、`2442aeed`，均有 derselbst 对该 head 的 APPROVED review。合并状态与 CI 分开记录：这三个 head 的 check-run 分别仍含 2、1、2 项失败；#1885 head `c90a865b` 的 135 项 check-run 为 129 成功、6 跳过。commit status API 均未返回独立 status context（total_count=0），不把其 `pending` 当作仍在运行的 CI。历史性能、sanitizer 与全量测试结论仅适用于其原始版本。


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


## 已完成条目的原始索引

### 历史代码分支

| 工作分支 | 内容 | review 规模 | 生产/CI 差异 |
|---|---|---|---|
| [fix/dev-all-notes-off-mono-keys](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-all-notes-off-mono-keys) | All Notes Off 清除 mono held keys；#1879 merged（另含 CC120）；dev 仅保留历史 | M | +7 / −0 |
| [fix/dev-midi-zero-tempo](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-midi-zero-tempo) | 拒绝 MIDI 零 tempo；#1881 merged，dev 仅保留历史 | S | +6 / −0 |
| [fix/dev-sostenuto-sustain-handoff](https://github.com/Yikai-Liao/fluidsynth/tree/fix/dev-sostenuto-sustain-handoff) | CC66 抬起时转交仍按下的 sustain；#1880 merged，dev 仅保留历史 | M | +7 / −2 |

### F04：已由上游修复

| 编号 | 问题 | 报告分数 | review 规模 | 已有测试 |
|---|---|---|---|---|
| [F04](https://github.com/Yikai-Liao/fluidsynth/blob/0b8c7240de5d1e331dec71b68438cd63ed36c17e/audit-report/report.md#L330) | SDL 尚未启动时，每次建 settings 都漏一个锁 | 2 | M | `test_sdl3_settings_init` |

[#1893](https://github.com/FluidSynth/fluidsynth/pull/1893) 删除 settings 枚举入口创建的临时 mutex，消除未初始化 SDL 提前返回时的锁泄漏；已随 2.6.2 发布。F05 只部分覆盖，F45 仍有 driver 枚举与 asprintf 失败输出问题，二者留在主待办。
