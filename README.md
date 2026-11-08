# FluidSynth fork 待办

更新：2026-11-09（UTC+08）。本次核对上游 master 为 [8210cb1b](https://github.com/FluidSynth/fluidsynth/commit/8210cb1bdc42718db5d68c2bc5aaea99ed434472)；最新 release 为 [2.6.2](https://github.com/FluidSynth/fluidsynth/releases/tag/v2.6.2)。fork master 仍为 c7ba7b49，未移动。todo 是独立根历史的文档分支，不合入代码 PR。

## 使用入口

- **贡献方式与双方偏好：** [fluidsynth-contributing skill](.agents/skills/fluidsynth-contributing/SKILL.md)。[依据与全部 PR 索引](.agents/skills/fluidsynth-contributing/references/history.md)只放稳定出处，不重复维护当前队列。
- **复现材料：** [真实 MIDI 验证](evidence/real-midi-20261030/README.md)，保留原来源、哈希和 94-byte 派生夹具，不含整曲／PCM／私有日志。
- Skill 位于 todo，不会自动出现在代码分支。后续可从 todo worktree 读取；需要全局自动发现时，自行复制完整 skill 目录到 ~/.agents/skills/，再 /reload。这里不改用户全局配置。

## 当前状态

Yikai-Liao 的 **21 个 PR：20 已合并，#1878 关闭未合并，无开放 PR**。#1900、#1901、#1902 均于 11 月 6 日合并，旧记录中的“待合并”已失效；#1878 被覆盖更全的 #1871 替代，不重提。合并、发布、历史 head 的 CI 是不同状态，不因合并便宣称已进入 2.6.2 或全部 CI 成功。

上游现有 open PR：[#1894 DSP 插值](https://github.com/FluidSynth/fluidsynth/pull/1894)（derselbst）、[#1889 RMIDI](https://github.com/FluidSynth/fluidsynth/pull/1889)、[#1718 POSIX OSAL](https://github.com/FluidSynth/fluidsynth/pull/1718)。不要重复维护者正在做的插值工作。#1897 仍 open 不代表 #1900/#1901 没有合并。

## 今晚建议：只准备 CC66 跨周期旧 voice ID 修复

建议标题：**Do not inherit released voice IDs across sostenuto cycles**。

前一个 sostenuto 周期的同音声部已进入 release，但状态仍可满足 sostenuto 判断。下一次踏板按下后新弹的同音继承其旧 ID，被误认为在本次按踏板前就已存在，Note Off 后仍被保持。

**为什么先做这一项：**

- 历史原曲 Two of Us.1.mid 在默认 Poly／note-cut=0／polyphony=256 下，GeneralUser GS 与 TimGM6mb 都已复现；不是靠开启特殊 Mono 模式制造问题。一个样本不等于普遍高发生率。
- 根因和修复局部，不改公开 API、不加状态／索引、不改 MIDI player 或踏板协议。保护同周期正常重弹，同时排除已 release 尾音。
- 生产只改一行 has_noteoff 判断，保留原注释；复用 test_voice_callback.c 加 24 行回归，共两个文件、+25/−1。规模小且有实际音频证据，符合双方降低审核成本的方向。
- 当前 open PR 中未见同项修复；最新上游源码仍缺此条件。没有将 F46 重复 CC66、router 清规则或 #1880 踏板转交并入。

**本次新测，而非历史结果：** 在上游 8210cb1b 的临时 worktree 单独移植 dev 分支的两个提交改动，未移动原 dev／master refs。

| 检查 | 结果 |
|---|---|
| 上游生产代码＋新增回归，Linux Debug double | test_voice_callback 在新周期 Note Off 的 noteoff_count > 0 断言失败 |
| 同一源码＋独立修复，Linux Debug double | check 51/51 |
| 同一源码＋独立修复，Linux Debug float | check 51/51 |
| 最终整理版，Linux Debug Clang double | check 51/51 |

工具链：Linux x86-64／GCC 16.2.1，cpp11 OSAL，SF3／mixer threads 开启；OpenMP、Signalsmith、native DLS 关闭。未启用 sanitizer；这是内存合成测试，不是实设备测试。核心构建命令（float 对照再加 -Denable-floats=ON）：

~~~sh
cmake -S <source> -B <build> -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -Denable-openmp=OFF -Denable-signalsmith=OFF -Denable-native-dls=OFF
cmake --build <build> --target check -j 6
~~~

**已整理并推送：** [fix/sostenuto-cycle](https://github.com/Yikai-Liao/fluidsynth/tree/fix/sostenuto-cycle) @ [03e5f8f2](https://github.com/Yikai-Liao/fluidsynth/commit/03e5f8f23f237713cce7243f82c68a6608382c62)，以 8210cb1b 为唯一父提交，作者 Yikai-Liao，单 commit；dev 历史未改。自查覆盖命名、注释、commit 消息与现有设施复用；收紧回归计数复位位置，保留生产原注释，测试注释改为直接描述场景，仍为 +24 行测试。此前同逻辑版本 GCC double／float、Clang double 各 check 51/51；本次仅改注释后重跑 GCC double 定向回归通过。去掉修复会在跨周期断言失败，错用 fluid_voice_is_on() 会在同周期保护断言失败。**尚未创建上游 PR**；余项是新 head 远端 CI、完整原曲与 check_rendering 新基线 PCM 对照，sanitizer 未重跑。

## 下一项与暂缓队列

| 主题／分支 | 条件、状态与取舍 |
|---|---|
| **已准备：fix/sostenuto-cycle** @ 03e5f8f2 | 上述跨周期问题，最新上游上的单提交；原 fix/dev-sostenuto-cycle @ a492c976 保留不动 |
| **次选：fix/dev-sostenuto-repeat** @ 58a97453 | F46，重复 CC66 高值错误重捕新音；用户已提高优先级。历史 MS Basic 原生播放确认；生产涉及旧值传递与逐声道 global-controller 路径，较首选更广，本次未重测 |
| fix/dev-router-sostenuto-release @ 575c0376 | 需播放中 router_clear；原生 EOF 会复位踏板，不称永久挂音。继续独立保存 |
| fix/dev-shrink-finished-voices @ e18868ca | 需宿主运行中缩容 256→64；完成计数遗留可阻塞 DONE，共同 PCM 相同，不称可听挂音 |
| fix/dev-mono-cc123-sustain @ d0b0510e | 需显式 Mono＋note-cut=2；默认 Poly 负控不变，后放 |
| fix/dev-reverb-reset-history @ e33ce9f6、fix/dev-lexverb-zero-room-reset @ f61728df | 非默认引擎／设置，真实路径差异很小，无听感验证，后放 |
| perf/iir-stable-block @ 707b52d6 | 统一 float/double W4，生产 +39/−5；不是已合并 #1867。MPFR 与 micro 历史验证保留，最终最小回归、PCM、普通 MIDI 端到端收益仍需复核；与 perf/iir-steady-loop 同区域方案择一，不自动恢复投稿 |
| perf/mixer-stable-amp @ d4afd04a | 已保存小型优化；历史普通 MIDI 结果保留，当前上游未重新定论，不凭旧数字直接投稿 |
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
