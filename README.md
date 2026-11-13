# FluidSynth fork 待办

更新：2026-11-14（UTC+08）。本次核对上游 master 为 [3ac3b3b4](https://github.com/FluidSynth/fluidsynth/commit/3ac3b3b4c3e236b0f850b374a3dd36f632341550)；最新 release 为 [2.6.2](https://github.com/FluidSynth/fluidsynth/releases/tag/v2.6.2)。fork master 仍为 c7ba7b49，未移动。todo 是独立根历史的文档分支，不合入代码 PR。

## 使用入口

- **贡献方式与双方偏好：** [fluidsynth-contributing skill](.agents/skills/fluidsynth-contributing/SKILL.md)。[依据与全部 PR 索引](.agents/skills/fluidsynth-contributing/references/history.md)只放稳定出处，不重复维护当前队列。
- **复现材料：** [真实 MIDI 验证](evidence/real-midi-20261030/README.md)，保留原来源、哈希和 94-byte 派生夹具，不含整曲／PCM／私有日志。
- Skill 位于 todo，不会自动出现在代码分支。后续可从 todo worktree 读取；需要全局自动发现时，自行复制完整 skill 目录到 ~/.agents/skills/，再 /reload。这里不改用户全局配置。

## 当前状态

Yikai-Liao 的 **26 个 PR：24 已合并，#1878 关闭未合并，仅 [#1910](https://github.com/FluidSynth/fluidsynth/pull/1910) 开放**。#1904/#1905 已于 11 月 9 日合并，#1908/#1909 已于 11 月 12 日合并（UTC 为 11 月 11 日），不再列入待投稿或待评审队列；#1878 被覆盖更全的 #1871 替代，不重提。合并、发布、历史 head 的 CI 是不同状态，不因合并便宣称已进入 2.6.2 或全部 CI 成功。

上游其他 open PR：[#1894 DSP 插值](https://github.com/FluidSynth/fluidsynth/pull/1894)（derselbst）、[#1889 RMIDI](https://github.com/FluidSynth/fluidsynth/pull/1889)、[#1718 POSIX OSAL](https://github.com/FluidSynth/fluidsynth/pull/1718)。不要重复维护者正在做的插值工作。#1897 仍 open 不代表 #1900/#1901 没有合并。

## 当前安排

11 月 9–15 日准备的两个 CC66 修复、router 清规则修复和缩容修复均已合并；本周性能投稿最终是 **#1910 的 mixer 连续访问优化**，不是原计划的稳定增益分支。先处理 #1910 的后续 review 与 CI，不因旧排期自动追加 PR，不催审；建议自己的开放 PR 不超过 2 个。后备实验与既有暂缓决定保留，不自动恢复投稿。

## 评审中：mixer sample-relative indexing（#1910）

**Improve voice mixer vectorization with sample-relative indexing**；[perf/mixer-contiguous-access](https://github.com/Yikai-Liao/fluidsynth/tree/perf/mixer-contiguous-access) @ [8ac616a2](https://github.com/Yikai-Liao/fluidsynth/commit/8ac616a2c87aada1a318ce3cbf8c5f1dc5a5d0af)。基于当前上游 3ac3b3b4，初始实现 cadc40e6，随后新增一个评审修正提交；没有 amend／rebase／force push，原开发分支未改写。

- **实现：** 将块偏移一次性应用到 source／destination 指针，循环只用相对 sample 下标，不改增益算术。新增聚焦的 test_rvoice_mixer，覆盖非零块偏移、partial／full block、增益变化和混音区间外样本。
- **维护者反馈：** [review 5893812978](https://github.com/FluidSynth/fluidsynth/pull/1910#pullrequestreview-5893812978) 认可指针偏移方案，并要求将 GCC／MSVC 的技术差异写进 PR 描述；两条 inline 意见要求偏移后再检查对齐，并删掉冗余断言。状态是 COMMENTED，不是正式 APPROVED，也尚未合并。
- **已完成响应：** 8ac616a2 调整 buf／dsp_buf 的断言位置，删除第二条 dsp_buf 断言；PR 正文已补充类型缩小／提升只解决 GCC、不能解决 MSVC reason 1200 的对照。32 位乘法溢出顾虑仍标为 review 实验提示的可能解释，不写成确定根因。
- **此前测量（初始实现）：** Linux x86-64、i7-10875H、GCC 16.2.1、Release -O3，无 -march=native；三首完整 MIDI／GM_Roland.sf2／44.1 kHz／单合成核，period 64／512，float／double、OpenMP OFF／ON，完整进程耗时下降约 3.46%–16.69%。交错配对与 A/A 对照；Clang 23.1.4、OpenMP OFF、period 64 基本中性（−0.11% 至 +0.36%）。完整表格与 Godbolt 示例见 [PR 正文](https://github.com/FluidSynth/fluidsynth/pull/1910)。MinGW／ARM64／MSVC 只有编译对照，不声称目标平台实测。
- **验证分开记录：** 初始实现 GCC／Clang × float／double 的 check 各 53/53，24 组完整 MIDI PCM 与上游逐位相同，GCC 两种精度的四项定向测试通过 ASan+UBSan。此次评审修正后，新跑 GCC Debug、OpenMP ON、float／double 各 check 53/53，断言启用；没有重跑全性能／PCM／sanitizer 矩阵，不把历史结果说成本次重测。
- **远端 CI（11 月 14 日 00:59 UTC+08 快照）：** 新 head 8ac616a2 部分通过，其余仍运行／排队；[mingw32 cpp11](https://github.com/FluidSynth/fluidsynth/actions/runs/43467864206/job/132031710044) 失败在 Setup MSYS2，Configure／Build／Test 均未执行。不能记为全绿，也不能据此判为本补丁代码失败。下一步跟进 CI 最终结果与维护者反馈，必要修正仍以普通追加提交处理。

## 本轮已合并

| PR | 结果与保留的边界 |
|---|---|
| [#1904](https://github.com/FluidSynth/fluidsynth/pull/1904)：CC66 跨周期旧 voice ID | 11/9 merged（c13970d7）；历史 Two of Us.1.mid＋GeneralUser GS／TimGM6mb 的默认 Poly 复现，回归同时保护同周期正常重弹。fix/sostenuto-cycle @ 03e5f8f2、原 dev @ a492c976 保留 |
| [#1905](https://github.com/FluidSynth/fluidsynth/pull/1905)：重复 CC66 高值错误重捕新音 | 11/9 merged（ad99f3cb）；只在 off→on 捕获，global-controller 使用各接收通道旧值。fix/sostenuto-repeat @ 945242a5、原 dev @ 58a97453 保留；与 #1904 独立且双向试合并无冲突 |
| [#1908](https://github.com/FluidSynth/fluidsynth/pull/1908)：router 清规则后保留 CC66 release | 11/12 merged（UTC 11/11；4330622f）；需要宿主在踏板按下时清规则，不称原生 EOF 永久挂音。既有 CC64 与 CC66 都覆盖 63/64 阈值及 release 后移除规则；GCC double／float、Clang double 的 Debug check 各 52/52。fix/router-sostenuto-release @ 32496159、原 dev @ 575c0376 保留 |
| [#1909](https://github.com/FluidSynth/fluidsynth/pull/1909)：缩容后匹配 finished voice | 11/12 merged（UTC 11/11；3ac3b3b4）；搜索已分配槽位，不只查新 polyphony 范围。ZMusic 公共 API 复现：64 个 active voice 时从 128 缩到 16，note-off＋渲染后原版残留 48，修复归零；不称可听挂音。维护者稍调测试注释。fix/polyphony-finished-voice @ bdbb67d5 为最终 head；旧 fix/shrink-finished-voices @ 4ef1802a、dev @ e18868ca 保留，不再另投 |

#1904/#1905 当时 GCC Debug float／double、Clang Debug double 各 check 51/51，组合 GCC double 51/51，反例均能抓住旧问题；完整原曲新基线 PCM／sanitizer 未重测。更详细的准备条件、工具链与旧 CI 快照见 [11 月 9 日记录](https://github.com/Yikai-Liao/fluidsynth/blob/a4b8f1a9534923a3cd4c6f4cd38b5c7b00a394b8/README.md)，不再保留其已失效的待投稿状态。

## 未投稿的性能后备

- **perf/mixer-stable-amp @ ea8e842e：** 8210cb1b 上的旧单提交，生产 +10/−1；当时 GCC／Clang × float／double 的 Release check 各 51/51，12 组完整 MIDI PCM 及声像／增益补测一致，GCC double 三曲完整进程耗时下降约 1.8%–3.0%，float 多数未建立稳定收益，见[该版本测量与原始数据](evidence/mixer-stable-amp-20261109/README.md)。原 dev 历史及后续 perf/mixer-gain 实验保留，均未开独立 PR。#1910 处理的是访问下标，不把旧稳定增益分支或组合栈收益归到它；若后续再推进，先在新的基线独立评估边际收益与差异。
- **IIR／reverb 后备：** perf/iir-coeff-double 历史 909.mid 渲染循环约降 4.1%，仍需当前编译器、多曲目及 fast-math 误差复测；IIR W4 维持原暂缓安排；reverb-delay-size 的历史整曲置信区间跨 1。旧数字不视为本次当前结果。

## 下一项与暂缓队列

| 主题／分支 | 条件、状态与取舍 |
|---|---|
| **PR #1910：perf/mixer-contiguous-access** @ 8ac616a2 | review 要求已落地并补正文，等待 CI 最终结果与后续反馈；#1904/#1905/#1908/#1909 均已合并，不重复排队 |
| fix/dev-mono-cc123-sustain @ d0b0510e | 需显式 Mono＋note-cut=2；默认 Poly 负控不变，后放 |
| fix/dev-reverb-reset-history @ e33ce9f6、fix/dev-lexverb-zero-room-reset @ f61728df | 非默认引擎／设置，真实路径差异很小，无听感验证，后放 |
| perf/iir-stable-block @ 707b52d6 | 统一 float/double W4，生产 +39/−5；不是已合并 #1867。MPFR 与 micro 历史验证保留，最终最小回归、PCM、普通 MIDI 端到端收益仍需复核；与 perf/iir-steady-loop 同区域方案择一，不自动恢复投稿 |
| perf/mixer-stable-amp @ ea8e842e、perf/mixer-gain | 未投稿的后备实验；上述数字仅适用于原基线，不沿用为 #1910 之后的独立收益；先跟进现有 PR |
| perf/interp-buffer-steps、perf/interp-tap-window；perf/sinc-* | 历史实验保留；插值已有维护者 #1894，sinc 由维护者处理，先不 review／投稿 |
| perf/iir-coeff-double、perf/iir-steady-loop、perf/reverb-delay-size、perf/reverb-idle-skip、perf/chorus-idle-skip | fork 性能实验，旧组合结果不作为当前独立收益；idle-skip 要保留引擎相位、DC 偏置与尾音约束 |
| perf/dev-channel-voice-index、perf/dev-exclusive-*、perf/dev-finished-voice-owner*、perf/dev-mixer-finished-index、perf/dev-lowest-free-voice、perf/dev-same-key-event-batch | 保留索引／位图／owner／批处理实验；需普通 MIDI 显著可重复端到端收益，不用 Black MIDI 或局部扫描次数替代；exclusive standalone 另有每 synth 8 KiB 成本 |
| fix/dev-breath-sync-reset、fix/dev-fixed-velocity-zero、fix/dev-initial-rate-iir-table | 特殊启用条件／音色或 JACK 启动换率；继续低优先级，不因改动小排到前面 |
| fix/dev-player-control、fix/dev-player-track-order、perf/dev-midi-track-merge、F35 | 用户明确暂缓；排序／seek 与 heap 是备选方案，不能重复算两项修复 |
| refactor/dev-dls-boundaries、refactor/dev-dls-boundaries-minimal | 两版与完整历史都保留；按 #1882 意见后放，不把 13 个审计条目直接排成 13 个 PR |
| ci/dev-android-migration、ci/dev-android-developer-probe | 等用户有空 review；不是继续补实现或催审的指令 |
| ci/dev-windows-build | 用户决定以后再说，先接受现有 CI 速度 |

其余并发初始化、MIDI 边界、短循环、采样 guard-frame、资源释放等候选留在既有 fix/dev-* 分支与历史审计索引，未自动恢复投稿。F13/F14/F18/F30/F39 等旧内存安全发现须按当前源码重查，旧 ASan 报告不是本次重现；F12 两个边界修复已由 #1900/#1901 合并；SDL3 的 #1893 只覆盖部分问题，不能整包勾完。安全问题按最新 SECURITY.md 处理。

用户记得“约一秒的声音／效果偏移”仍未定位，明确不是 #1872 的 4 ms predelay；不得用近似问题替代。

## 实验停止与文档约定

- 相位增量缓存未在代表性 MIDI 减少目标转换调用，unity-gain 分支普通 MIDI 回退；ADSR 快路径／布局、预取、双位置 tap shift 等无可靠端到端收益，暂不重启。worker 缓冲清零的早期 20% 含测量污染，修正约 5%；不足以单独支撑复杂缓冲协议。
- 历史组合栈结果、旧 CI、精度实验和原有审计都可在下方固定快照回查，不复制成新的当前结果。已保存分支不等于已完成 review、rebase 或授权投稿。
- 当前安排只维护本 README；稳定贡献判断维护 skill；具体证据放一个专题目录。每项只记结论、条件、版本与下一步，不再按会话追加流水。

## 历史回查（不丢证据，不在当前队列重复展开）

以下均固定在本次精简前的 todo 提交 [490c2114](https://github.com/Yikai-Liao/fluidsynth/tree/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2)：

- [旧 README：用户决定、完整分支／F/N 审计索引、精度与 CI](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/README.md)
- [旧 progress：原始性能测量与拒绝实验](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/progress.md)；[旧 perf-inventory：逐模块分支／提交及栈依赖](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/perf-inventory.md)
- [旧 COMPLETED：2.6.2 发布包含关系及历史验证](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/COMPLETED.md)；其中 #1900/#1901/#1902 当时仍开放，不作当前状态使用。
- [archive/previous-work](https://github.com/Yikai-Liao/fluidsynth/tree/archive/previous-work) 保留 100 个旧分支名、head 映射及提交历史；恢复时从原 SHA 新建 dev 分支，不重建日期命名。
