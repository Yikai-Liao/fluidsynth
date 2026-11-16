# FluidSynth 分支盘点

核对：2026-11-16；上游 `6d43acdd`。覆盖 fetch 后 origin 的 **83 个真实分支**（不计 HEAD 符号引用），归档 README 的 **100 个旧分支映射**、旧 todo 的 F 审计与 N01–N21、perf-inventory/progress，以及归档实验报告。所有本地分支提交均可从 origin 远端引用到达；本地 mixer-shared-source／mixer-contiguous-dev 是 #1910 旧实现，无额外未保存候选。

分支盘点只核对证据和当前源码，没有做全分支构建或性能测量；随后按用户安排完成 N14 的独立修复、三配置 check 与真实 MIDI 对照。其他“历史已复现”不等于本次复现，也没有对 83 分支全部重新证明正确性。当前建议顺序只在 [README](README.md#当前安排) 维护。

## 当前远端分支逐项覆盖

已合并主题包括 retained dev 和旧投稿 head；不要求提交 SHA 是上游祖先，squash merge 需按 PR 主题和最终补丁核对。分支名没有 dev 也不代表已准备好投稿。

| 分支 | head | 状态 | 内容与下一步 |
|---|---|---|---|
| `archive/previous-work` | `1fe0bf5f` | 归档 | 100 个旧分支名称／head 映射与完整历史；见下文 |
| `ci/android-migration` | `d2e16074` | 等用户 review | 已有整理版；不是实现已缺项或需要继续催审 |
| `ci/dev-android-developer-probe` | `6a3325d3` | 等用户 review | 独立 API35 probe；不能把迁移旧版本另计多项 |
| `ci/dev-android-migration` | `79e714ab` | 等用户 review | 原开发历史保留，与投稿准备版同一迁移 |
| `ci/dev-windows-build` | `a7164f74` | 明确暂缓 | 用户决定；上游 #1911 和 a24ded4a 已改变 Windows 工作流，旧补丁需重新核对 |
| `ci/macos-intel-arm` | `4ef44acf` | 已合并主题 | [#1876](https://github.com/FluidSynth/fluidsynth/pull/1876)；dev／旧 head 保留，不重复投稿 |
| `evidence/lfo-frequency-1310` | `787f0d82` | 证据 | 已合并 #1912 的图表材料，不是另一项修复 |
| `fix/all-notes-off-mono-keys` | `2fac96b3` | 已合并主题 | [#1879](https://github.com/FluidSynth/fluidsynth/pull/1879)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-all-notes-off-mono-keys` | `9929ccff` | 已合并主题 | [#1879](https://github.com/FluidSynth/fluidsynth/pull/1879)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-audit-fixes` | `45fbbb63` | 需拆分 | 混合审计包；F18 有精简测试，F01/F32/F49 等音频项仍值得单独评价；不整包投稿 |
| `fix/dev-basic-channel-count-bounds` | `70044ec4` | 低优先 | 非法 basic-channel 数量／溢出边界；不是正常音频首选 |
| `fix/dev-breath-sync-reset` | `41acd7a8` | 后放 | CC121 后气息同步漏起音；用户原决定：显式 Breath Sync Mono，低优先 |
| `fix/dev-dattorro-sample-rate` | `6603116f` | 修复后备 | 默认混响引擎不支持换率；现有补丁重建状态并丢弃尾音，需确认预期与 JACK 实机路径 |
| `fix/dev-dither-table-init` | `02da8b35` | 低优先 | 并发初始化共享 S16 dither 表；需当前竞态复现与线程初始化成本审查 |
| `fix/dev-exclusive-preset-scope` | `5809d164` | 已保存独立修复 | N14；生产 +33/−4，回归98行；当前本地三配置 check 各55/55，真实曲目复现与因果状态对照完成，待用户 review／远端 CI核对 |
| `fix/dev-fixed-velocity-zero` | `dea2b6d0` | 后放 | SF2 显式固定 velocity=0；保留用户低优先决定 |
| `fix/dev-initial-rate-iir-table` | `a7d56827` | 修复后备 | JACK 初次协商改变采样率后 IIR 表未重建；当前仍缺该更新，需设备路径验证，独立于 Dattorro 换率 |
| `fix/dev-jack-rate-change` | `2c82f2ef` | 修复后备 | 运行中换率时停止不匹配渲染；需 JACK 实机重现和停播契约，不与启动 IIR 表混算 |
| `fix/dev-legato-invalid-note-sentinel` | `031121d2` | 已合并主题 | [#1886](https://github.com/FluidSynth/fluidsynth/pull/1886)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-lexverb-zero-room-reset` | `f61728df` | 后放 | 非默认引擎；历史真实路径差异小、无听感验证 |
| `fix/dev-lfo-frequency-direction` | `07bd2b54` | 已合并主题 | [#1912](https://github.com/FluidSynth/fluidsynth/pull/1912)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-midi-tempo-drift` | `6f1f7d6f` | 恢复列项 | 已保存时钟锚点修复＋精简回归；与暂停／跨轨排序不同，但涉及 player tempo API，先决定是否解除这部分暂缓 |
| `fix/dev-midi-track-count` | `f8edcf0a` | 低优先 | SMF 高位轨道数的符号扩展；需有效大轨道文件和当前输入限制 |
| `fix/dev-midi-zero-tempo` | `84e39f6a` | 已合并主题 | [#1881](https://github.com/FluidSynth/fluidsynth/pull/1881)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-mono-cc123-sustain` | `d0b0510e` | 修复后备 | 真实 Bill Evans MIDI 已有证据，但需宿主 Mono＋note-cut=2；默认 Poly 阴性 |
| `fix/dev-player-control` | `fb9a82bd` | 明确暂缓 | 用户决定；暂停恢复和 tempo 请求涉及时钟所有权，不直接投稿旧大补丁 |
| `fix/dev-player-track-order` | `e24167f8` | 明确暂缓 | 用户决定；#1866 仍 open，不因此自动恢复 |
| `fix/dev-relative-clock-init` | `2c7321b7` | 低优先 | 并发首次初始化相对时钟 origin；需实际并发入口、竞态证据 |
| `fix/dev-reverb-reset-history` | `e33ce9f6` | 后放 | Freeverb/FDN 非默认引擎，真实 reset 路径有小差异，默认 Dattorro 对照不变 |
| `fix/dev-router-sostenuto-release` | `575c0376` | 已合并主题 | [#1908](https://github.com/FluidSynth/fluidsynth/pull/1908)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-sample-guard-frame-overflow` | `ce6705c6` | 安全边界 | 尺寸分配溢出；先核对当前入口与 SECURITY.md，旧报告不算新重现 |
| `fix/dev-shared-synth-init` | `d865d0b1` | 低优先 | 并发构造 synth 等待共享初始化；需当前源码和线程重现 |
| `fix/dev-shell-server-mutex-release` | `4b10b321` | 低优先 | 失败／关闭释放 mutex；资源生命周期，不排在音频修复前 |
| `fix/dev-short-loop-interpolation` | `0842256d` | 修复后备 | 循环短于插值 stencil；已有周期 oracle 和补丁，缺实际音源触发；先核对维护者 #1894 重叠 |
| `fix/dev-shrink-finished-voices` | `e18868ca` | 已合并主题 | [#1909](https://github.com/FluidSynth/fluidsynth/pull/1909)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-smf-sysex-continuations` | `af38af77` | 修复后备 | SMF F0/F7 续包和 escaped MIDI 时序；合法协议但旧补丁较大，需真实文件和 RMIDI #1889 重叠审查 |
| `fix/dev-sostenuto-cycle` | `a492c976` | 已合并主题 | [#1904](https://github.com/FluidSynth/fluidsynth/pull/1904)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-sostenuto-repeat` | `58a97453` | 已合并主题 | [#1905](https://github.com/FluidSynth/fluidsynth/pull/1905)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-sostenuto-sustain-handoff` | `abe6dab2` | 已合并主题 | [#1880](https://github.com/FluidSynth/fluidsynth/pull/1880)；dev／旧 head 保留，不重复投稿 |
| `fix/dev-timer-thread-handle-release` | `53d29244` | 低优先 | join 后释放线程句柄；先看当前 GLib/OSAL 契约 |
| `fix/midi-zero-tempo` | `5f03d226` | 已合并主题 | [#1881](https://github.com/FluidSynth/fluidsynth/pull/1881)；dev／旧 head 保留，不重复投稿 |
| `fix/mixer-worker-first-job` | `9a22ee18` | 已合并主题 | [#1895](https://github.com/FluidSynth/fluidsynth/pull/1895)；dev／旧 head 保留，不重复投稿 |
| `fix/polyphony-finished-voice` | `bdbb67d5` | 已合并主题 | [#1909](https://github.com/FluidSynth/fluidsynth/pull/1909)；dev／旧 head 保留，不重复投稿 |
| `fix/router-sostenuto-release` | `32496159` | 已合并主题 | [#1908](https://github.com/FluidSynth/fluidsynth/pull/1908)；dev／旧 head 保留，不重复投稿 |
| `fix/sf3-sample-gain-20261021` | `436597dc` | 已合并主题 | [#1877](https://github.com/FluidSynth/fluidsynth/pull/1877)；dev／旧 head 保留，不重复投稿 |
| `fix/sf3-x87-fallback` | `9981ef6e` | 已合并主题 | [#1884](https://github.com/FluidSynth/fluidsynth/pull/1884)；dev／旧 head 保留，不重复投稿 |
| `fix/sfont-mod-dest-bounds` | `9060d2fe` | 已合并主题 | [#1900](https://github.com/FluidSynth/fluidsynth/pull/1900)；dev／旧 head 保留，不重复投稿 |
| `fix/shrink-finished-voices` | `4ef1802a` | 已合并主题 | [#1909](https://github.com/FluidSynth/fluidsynth/pull/1909)；dev／旧 head 保留，不重复投稿 |
| `fix/sostenuto-cycle` | `03e5f8f2` | 已合并主题 | [#1904](https://github.com/FluidSynth/fluidsynth/pull/1904)；dev／旧 head 保留，不重复投稿 |
| `fix/sostenuto-repeat` | `945242a5` | 已合并主题 | [#1905](https://github.com/FluidSynth/fluidsynth/pull/1905)；dev／旧 head 保留，不重复投稿 |
| `fix/sostenuto-sustain-handoff` | `0271fe5c` | 已合并主题 | [#1880](https://github.com/FluidSynth/fluidsynth/pull/1880)；dev／旧 head 保留，不重复投稿 |
| `master` | `c7ba7b49` | 旧基线 | c7ba7b49；本次不移动 |
| `perf/chorus-idle-skip` | `35443f4d` | 次级后备 | 历史 chorus-free 曲目约降 2%–3.6%；补丁含较多相位状态逻辑，需新基线独立测收益和静音→起音等价 |
| `perf/dev-channel-voice-index` | `20bbca67` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/dev-exclusive-candidates-standalone` | `ebe48363` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/dev-exclusive-channel-scan` | `24530a25` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/dev-finished-voice-owner` | `4c1445c9` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/dev-finished-voice-owner-standalone` | `3c2fe217` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/dev-iir-stable-block` | `e9133d01` | 旧方案 | W4 旧 double 实现和完整报告；由 707b52d6 承接，不另算候选 |
| `perf/dev-lowest-free-voice` | `2b896710` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/dev-midi-track-merge` | `1f8dba15` | 明确暂缓 | 跨轨 heap 备选，与 player-track-order 比较后择一 |
| `perf/dev-mixer-finished-index` | `b2045d6c` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/dev-mixer-gain` | `d7e68012` | 同主题历史 | 稳定增益；统一由 mixer-gain 承接，先测 #1910 之后的独立边际收益，复用现有 mixer 测试 |
| `perf/dev-mixer-stable-amp` | `a911a82f` | 同主题历史 | 稳定增益；统一由 mixer-gain 承接，先测 #1910 之后的独立边际收益，复用现有 mixer 测试 |
| `perf/dev-mixer-std-atomic` | `03db9b06` | 已合并主题 | [#1902](https://github.com/FluidSynth/fluidsynth/pull/1902)；dev／旧 head 保留，不重复投稿 |
| `perf/dev-same-key-event-batch` | `30f5a9a8` | 证据不足 | 索引／owner／位图／游标／批处理实验；普通 MIDI 完整渲染收益未确立，不沿用稠密 MIDI／组合栈收益 |
| `perf/iir-coeff-double` | `e3341732` | 优先复测 | 小改动将 float 系数提升到 fluid_real_t，历史单曲约降 4.1%；当前转换／汇编、多曲目和 fast-math 误差待核对 |
| `perf/iir-stable-block` | `707b52d6` | 保留暂缓 | 最终共享 float/double W4；历史 MPFR/micro 有证据，但未完成该版本普通 MIDI 端到端与最小回归 |
| `perf/iir-steady-loop` | `2d38734e` | 次级后备 | 固定系数时绕过 ramp 检查；与 W4 同区域择一，旧组合收益不能算独立收益 |
| `perf/interp-buffer-steps` | `bd6c7ec2` | 维护者重叠 | 跳过不必要除法，有历史小收益；#1894 正在改插值，先核对不重复 |
| `perf/interp-tap-window` | `486e327a` | 维护者重叠 | cubic 抽头窗口复用，历史有收益与边界修正；先核对 #1894 |
| `perf/legato-invalid-note-sentinel` | `2442aeed` | 已合并主题 | [#1886](https://github.com/FluidSynth/fluidsynth/pull/1886)；dev／旧 head 保留，不重复投稿 |
| `perf/mixer-contiguous-access` | `8ac616a2` | 已合并主题 | [#1910](https://github.com/FluidSynth/fluidsynth/pull/1910)；dev／旧 head 保留，不重复投稿 |
| `perf/mixer-gain` | `8ff0c280` | 优先复测 | 稳定增益；统一由 mixer-gain 承接，先测 #1910 之后的独立边际收益，复用现有 mixer 测试 |
| `perf/mixer-stable-amp` | `ea8e842e` | 同主题历史 | 稳定增益；统一由 mixer-gain 承接，先测 #1910 之后的独立边际收益，复用现有 mixer 测试 |
| `perf/reverb-delay-size` | `e89c8734` | 较弱后备 | 小型缓存；历史整曲区间跨 1，新基线无可信收益前不优先 |
| `perf/reverb-idle-skip` | `773935f6` | 次级后备 | 历史无 reverb-send 曲目有收益；只可跳过无移动状态的引擎，不能给 FDN/Signalsmith/Freeverb 通用套用 |
| `perf/sinc-kernel-fused-taps` | `a8354c9c` | 维护者负责 | 用户决定 sinc 由维护者处理；三个堆叠版本不是三项独立投稿 |
| `perf/sinc-paired-samples` | `c9e186aa` | 维护者负责 | 用户决定 sinc 由维护者处理；三个堆叠版本不是三项独立投稿 |
| `perf/sinc-phase-table-rebased` | `7491f437` | 维护者负责 | 用户决定 sinc 由维护者处理；三个堆叠版本不是三项独立投稿 |
| `refactor/dev-dls-boundaries` | `77ee7d54` | 明确后放 | DLS 大重构，遵守 #1882 决定，不把 13 项审计拆成 13 个 PR |
| `refactor/dev-dls-boundaries-minimal` | `52faea7f` | 明确后放 | DLS minimal 备选与失败处理，和原版比较后择范围 |
| `todo` | `e2d55d2b` | 文档 | 本次盘点前的记录分支 head |

## 没有独立分支的音频候选：N01–N21

这些只在 [490c2114 的旧 README](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/README.md) 保存诊断和证据索引，不能因远端没有对应 fix 分支而漏掉。**N15 已由 #1912 合并；N14 现已另开独立修复分支并完成本地验证，其余 19 项并不都是当前已重现或值得立刻投稿。**

| 编号 | 问题与触发条件 | 本次建议／证据边界 |
|---|---|---|
| N01 | CC121 清除先前 RPN 粗／微调，后续新音变调 | 保留；合法构造成立，但旧 Lakh 近似命中均无后续受影响起音，不沿用旧 P1 为当前高发生率 |
| N02 | Mono／CC68 复用 voice 后 key→pan 等通用键号调制未刷新 | 条件性修复后备；GeneralUser 正常音色有诊断，MS Basic 无对应 key→pan，未确证自然曲目 PCM 影响 |
| N03 | 多效果组中关闭一组，独立 wet 输出漏出左声道原始 send | 宿主多输出修复后备；当前 mixer 仍直接 continue 关闭组，需新基线完整缓冲流程复现 |
| N04 | MTS 声道掩码未隔离共享调音 | 条件性后备；需要专门 MTS 消息，旧曲库无该消息，需共享 tuning 身份与替换语义审查 |
| N05 | 实时调音漏更新 sustain／sostenuto 保持音 | 条件性后备；需实时调音和踏板组合，不因与 CC66 有关便和已合并修复合包 |
| N06 | limiter 多输出只处理第一对，后续声道不限幅且延迟不同 | 宿主多输出修复后备；需核对 limiter 每组状态和延迟契约，非默认双声道普遍问题 |
| N07 | process/write API 非块边界切换丢余样 | 低优先；实际宿主需求和混合 API 连续性契约仍未证实 |
| N08 | 包络进入 sustain 后实时调 sustain level 不生效 | 条件性后备；合法 API／NRPN 构造，缺自然使用路径证据 |
| N09 | Signalsmith width 缩放疑似小 100 倍 | 非默认引擎后备；当前仍 /100 且 setter 接受至 100，先复核公共文档、推荐范围与引擎映射，不把旧判断当新验证 |
| N10 | Lexverb level 原值写回重置延迟线位置 | 非默认引擎后备；实际尾音连续性诊断已有，需重新检查 setter 对状态的影响 |
| **N11** | 默认 chorus level setter 重置调制相位 | **恢复调查**；当前 set 仍无条件调用会初始化中心／振荡器的参数重建，旧 PCM 差异明确；CC93 只改发送量，不是同一触发入口 |
| **N12** | synth gain=0 渲染后恢复，仍按住的音已被裁掉 | **恢复调查**；旧 double/float 复现；Qsynth v1.0.6 的 Master Gain 控件源码确有 set_gain 路径，未做 UI／设备复现。MuseScore 普通滑块走外部混音，不能套用。修复须权衡静音裁剪和 CPU 成本 |
| N13 | 不同 transform 的合法 modulators 被当成相同身份合并 | 罕见音色／自定义 modulator 后备；无常用音源自然组合，先验证 loader 合并契约 |
| **N14** | 换鼓组后，同 class 不同 preset 的旧尾音被截断 | **独立修复待 review**；fix/dev-exclusive-preset-scope @ 5809d164，见 README 当前安排；不等于 exclusive 扫描优化 |
| N15 | LFO 频率更新丢失下降方向 | **已合并 #1912**，不重复投稿；与旧 #1182/#1183 的负 absolute-cents 转换不同 |
| N16 | CoreAudio 三对以上输出＋默认单 FX 组静音 | 平台候选；旧 Linux 抽取 callback 验证不是 Apple 硬件测试，先找 macOS 多输出验证条件 |
| N17 | router 负倍率映射高一个半音 | 小型条件性后备；正常反向键盘规则60→68而预期67，有旧音频证据；不是默认直通规则问题 |
| N18 | Mono／CC68 演奏时换 preset，旧音色被复用叠加，切回首音漏发 | 正确性后备；常用音源、旧 double/float 已复现，无自然曲库命中。与 N14 同涉 preset 身份但症状／根因不同，不能自动合包 |
| N19 | 对活动 voice 配置 custom filter 后 Q 未重施，持续旁路 | 显式 API 后备；新 note-on 正常，重写同值 Q 可恢复，需最小回归和调用路径验证 |
| N20 | Mono／Legato 的 Note Off 回退提前消费下一 Note On 的 CC84 | 低于自然触发候选；旧合法构造成立，Lakh 中无所需 Mono／Legato 同现前提 |
| N21 | 滑音时间改零，复用 voice 残留上次滑音偏移 | 条件性后备；GeneralUser 已有数百 cents 旧偏移诊断，MS Basic 阴性；不能称所有音色受影响，也未证明是用户记忆中的“一秒偏移” |

### N14 为什么恢复到前面

[旧 N14 真实数据集补充](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/README.md#n14-真实数据集补充2026-10-28-续查)使用 Tegridy 固定发布版本中的 `Flute/063deb6ba2e0838b1b9f53ca45309b45.mid`（SHA256 `418a2a09abac4340cba1e2ab45ebbc36ed0ed67c9b9d174dffa8ce6e605c70bf`），官方 MS Basic.sf3，完整原生 player 播放 88.891791 秒。身份对照 PCM 相同；只隔离自然 NoteOn 对不同 preset 旧声部的排他作用，效果关闭时 39,546 个声道样本不同，最大差 0.007956913，默认效果开启也有差异。它是发布的加工数据集成员，不是本轮改写输入；诊断参考是因果对照，不是生产修复或独立渲染器。

本轮修复前上游的 [exclusive-class 匹配](https://github.com/FluidSynth/fluidsynth/blob/6d43acddbdb0a8d3f1f3f09f90fa6de3d8938d8f/src/synth/fluid_synth.c#L5485)仍只比较声道、class 和 note-on ID。旧证据比纯构造更强，但仍只有单个已确认发布样本，没有总体发生率或听感严重性结论。本轮已在 6d43acdd 完整重放并保存修复；每个 voice 复制来源数值，不持有长期 preset 指针，正常 Note On 与 fluid_synth_start 均经统一内部回调入口。当前结果见 README 和专项验证摘要。

## 混合审计包里仍值得看的项目

`fix/dev-audit-fixes @ 45fbbb63` 不能只按“审计包”整项后放而忘掉其独立音频问题。下列为旧报告索引，不是本轮重新运行 sanitizer；完整 F01–F52（F26 撤回）见[旧清单](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/README.md#审计问题索引)。

| 候选 | 为什么保留／下一步 |
|---|---|
| F01 跨声道同键的 router 规则清除 | 合法双通道 MIDI＋宿主清规则可丢第二个 Note Off；与已合并 #1908 的 CC66 release 不同。需新上游最小复现，别把它当已随 #1908 修好 |
| **F18 unpitched SF2＋自定义音律** | 有修复、精简为 74 行的独立回归，旧 sanitizer 与反例证据较完整；当前 sample import 仍直接保存 origpitch=255，调音路径仍用根音作下标。值得优先重现这一合法格式组合；音源实际覆盖与安全报告路径需核对，不以旧审计分数直接压过 N14 |
| F32 sequencer 复位后旧 note-off 关闭新音 | 合法定时 note 与 CC121，属实际音频状态；需明确预期和最小修复，不能和 F37 非法时值混包 |
| F49 自定义调音跨区后 Mono 连奏错误复用 | 正常音源条件性问题；与 N02/N18 各自独立，需新上游复现 |
| F44 关闭 loop 后仍读旧循环边界 | 需当前动态循环控制入口、音频／sanitizer 复现；不能由插值 PR 标题推断已经解决 |
| F13/F14、F09/F10、F17/F30/F34/F39/F40/F41/F43 | 默认 modulator／settings 生命周期、名称与数值／sample 边界；按真实入口单独评价，未公开问题按当前 SECURITY.md 处理 |
| F05/F45、F33、F06/F08/F15/F24/F25/F27/F28/F29/F36/F37/F38/F48/F50 | SDL3 部分修复后的资源与失败路径、线程、NRPN／MTS／SMF／SysEx／非有限参数；需要分别重查，不以 #1893 或整包旧测试自动完成 |
| F02/F03/F07/F11/F16/F20/F21/F22/F23/F42/F47/F51/F52 | DLS 重构覆盖项，维持用户后放决定 |
| F04/F12/F46 | 分别已由 #1893、#1900/#1901、#1905 解决主题；不再投稿。F26 已撤回，F35 随播放器明确暂缓 |

## 100 个旧分支：有映射，有停止理由

[固定归档索引 1fe0bf5f](https://github.com/Yikai-Liao/fluidsynth/blob/1fe0bf5f/README.md)包含 100 条映射：28 条标有工作分支去向，72 条仅归档。映射表示承接关系，不能据此认为旧 head 与现分支源码相同；其中旧 master 映射也是历史身份。没有发现足以直接恢复投稿的遗漏性能补丁。

已回读[25 项实验结果索引](https://github.com/Yikai-Liao/fluidsynth/blob/f227be8e2e087abf6e2f64a092eafe74b6c2d7ea/doc/performance-evidence/EXPERIMENT_INDEX.md)，并针对容易被分支名误认为可投稿的下列项目读取原报告：

| 归档 head／主题 | 原报告的决定；本次取舍 |
|---|---|
| f227be8e offline-render-batching | 只需现有 CLI `-z`，无源码补丁；更大块还会改变文件尾长，不恢复为优化 PR |
| 4bfb3a91 modulator-preparation | 密集输入实测 merge helper 仅约 0.27% 总 CPU，索引维护成本不值；停止 |
| 4579a124 Freeverb block-process | 三对短片段范围跨基线，约 2.2% 中位改善未建立稳定收益；原型已撤销 |
| 75f68969 FDN output-gain | 三对约 0.6% 中位改善、范围跨基线；停止 |
| 494e690b IIR gain-factor-reuse | 中性且有微小 float 差异；停止 |
| 0e1deed4 custom/SF2 IIR fusion | 三对均回退，约慢 5.1%；停止 |
| f48afbdd fused voice mix | 约慢 10.2%，原型已撤销；停止 |
| c97283c0 cross-voice SoA IIR | 涉及 render/filter 分阶段、scratch 所有权与混音次序的大改，旧架构筛查不足以支持；维持暂缓 |

其他旧插值 row/SIMD/step/count 实验多为中性、回退或被当前分支承接；与 #1894 重叠，不能因名字不在新 README 就恢复。旧 IIR coefficient reuse、Dattorro tap-wrap、voice-reuse gain 已有对应合并主题；早期 sinc 表和组合栈不是新的独立收益。旧 midi-track-duration 的 INT_MAX 拒绝方案由 F35 的 64 位方案承接，仍随播放器暂缓。Android／CI 多版本继续按当前迁移分支收敛。

未定位的约一秒声音／效果偏移保留原问题，不用 N21、player tempo drift 或 4 ms predelay 近似替代。
