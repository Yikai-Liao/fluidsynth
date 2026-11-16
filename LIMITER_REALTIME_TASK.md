# Limiter 实时调参：长期任务记录

最后整理：2026-11-16。工作分支：`fix/1890-realtime-limiter`。

这份文件是当前分支的工作记录。后续轮次在这里更新需求、决定、证据和下一步；准备合并时再拆分正式文档、测试、工具和提交。当前任务尚未完成，不能凭测试通过就关闭 issue。

## 当前结论与下一步

目标是满足 FluidSynthPlugin 的 DAW 使用场景：synth 运行期间可以修改所有 `synth.limiter.*`，避免重建 synth、反复加载 SoundFont，并把声音过渡做得适合音乐。运行时修改 `attack`、`active` 仍在范围内。

当前代码已把处理器准备与回收移出渲染线程，但不同延迟输出的 20 ms 交叉淡化会相位抵消。已有独立信号、录音、负例和可视化证据。**解决延迟变化时的音乐性问题仍是阻塞项。**

下一步先完成插件调用路径与延迟契约梳理，再比较过渡策略。固定总延迟、等待静音、渐变延迟、时间伸缩等都还没有被选为最终方案；不得把先前讨论中的建议写成既定需求或已完成修复。

## 原始需求与使用者

### 上游原文

- [Issue #1890](https://github.com/FluidSynth/fluidsynth/issues/1890)，作者 `prof-spock`，2026-10-30。
- [Discussion #1883](https://github.com/FluidSynth/fluidsynth/discussions/1883)，同一作者，2026-10-26；已读取正文、两条评论及两条回复。
- [作者解释插件状态模型的评论](https://github.com/FluidSynth/fluidsynth/discussions/1883#discussioncomment-19226993)。
- [维护者说明当前 limiter 设置只在分配时应用](https://github.com/FluidSynth/fluidsynth/discussions/1883#discussioncomment-19225700)。维护者没有在这里承诺动态修改可无伪影，也没有指定新延迟策略。

作者希望像 reverb/chorus 一样，在 synth 运行时修改 limiter 设置。插件把不可运行时修改的设置列为 critical settings，修改时重建 synth；逐项应用设置会导致多次重建，在大型、多实例 DAW 工程里还会反复加载 SoundFont。

这里的 real-time 在原文中的明确含义是“synth 正在运行时可修改”。不能自行删去 attack 或 active，也不能把必须停播、重建或等待静音作为默认完成方式。原文尚未指定采样级 limiter 自动化、硬实时 setter、任意延迟跳变零音色变化、或 DAW 延迟补偿接口；这些需要单独确定，不能冒充作者已经提出的具体契约。

### 插件不是假设场景

项目：[prof-spock/FluidSynthPlugin](https://github.com/prof-spock/FluidSynthPlugin)。本轮源码核对固定在提交 `0e7fda8074b179184d013aca0056ecce1a2f35ed`，不把以后变化的 master 当成同一份证据。

作者在 DAW 内交互试验 FluidSynth 设置，再希望与外部渲染流程保持一致。插件有文本配置和 Confirm 操作，并提供与命令行输出比较的 Reaper 工程。不能只按现场键盘演奏场景选方案，也不能忽略其声音复现和时间对齐需求。见[该版本 README](https://github.com/prof-spock/FluidSynthPlugin/blob/0e7fda8074b179184d013aca0056ecce1a2f35ed/README.md)。

源码中已确认的调用路径：

| 位置 | 已确认行为 | 对本任务的影响 |
| --- | --- | --- |
| [FluidSynthSettings.cpp](https://github.com/prof-spock/FluidSynthPlugin/blob/0e7fda8074b179184d013aca0056ecce1a2f35ed/src/Libraries/FluidSynth/FluidSynthSettings.cpp#L169) | `_restartingKeysList` 包含全部七个 limiter 参数 | 库修好后，现有插件也需要解除相应的静态重建分类；它不会凭空自动适配 |
| [MidiEventConverter.cpp](https://github.com/prof-spock/FluidSynthPlugin/blob/0e7fda8074b179184d013aca0056ecce1a2f35ed/src/MIDI/MidiEventConverter.cpp#L541) | `setSetting` 按重建清单决定是否删除、重建 synthesizer，并恢复 SoundFont 等状态 | 这是 issue 所述逐项重建成本的具体实现 |
| [Editor.cpp](https://github.com/prof-spock/FluidSynthPlugin/blob/0e7fda8074b179184d013aca0056ecce1a2f35ed/src/Main/FluidSynthPlugin/FluidSynthPlugin_Editor.cpp#L475) | 编辑确认路径调用 `processor.setSettings(st)` | 应沿实际配置提交路径做集成测试，不能假定它已是连续旋钮自动化 |
| [EventProcessor.cpp：设置路径](https://github.com/prof-spock/FluidSynthPlugin/blob/0e7fda8074b179184d013aca0056ecce1a2f35ed/src/Main/FluidSynthPlugin/FluidSynthPlugin_EventProcessor.cpp#L403) | 更新设置时持互斥锁；音频 `processBlock` 拿不到锁时把输出置零 | 即使库不重建，插件自己的设置提交路径仍可能造成静音块；端到端验收须覆盖它 |
| [EventProcessor.cpp：延迟上报](https://github.com/prof-spock/FluidSynthPlugin/blob/0e7fda8074b179184d013aca0056ecce1a2f35ed/src/Main/FluidSynthPlugin/FluidSynthPlugin_EventProcessor.cpp#L273) | 已读到的 `setLatencySamples` 用于内部 buffer 时间栅格补偿 | 这处调用不包含 limiter 的预读延迟；仍须完整核对是否有别处处理，不能声称已支持 limiter 的动态 PDC |

源码快照在本地 `build/scratch/1890/requirements/`；引用采用固定提交链接，源码快照不随 Git 分支迁移。尚未构建或运行作者的插件，没有与作者或维护者发送消息。

## 必须保留的需求

| 编号 | 要求 | 当前状态 |
| --- | --- | --- |
| R1 | 全部七个 limiter 参数能在已有 synth 运行时更新；最终状态确实生效 | 库实现及多类语义测试已有；插件集成未验收 |
| R2 | limiter 调参不重建 synth，不重载 SoundFont，不清掉现有 voices/MIDI 状态 | 库实现不重建 synth；真实插件的重建分类仍待适配验证 |
| R3 | 明确 setter 线程与渲染线程的契约；渲染中不执行 DSP 分配、初始化、析构或等待控制锁 | 当前实现把准备/回收移出渲染；setter 仍可能分配和阻塞，须在控制线程调用 |
| R4 | 连续/批量请求不导致无界积压，停止调参后收敛到最后成功准备的目标 | mailbox 合并已有；中间请求是否必须逐个可听见不作承诺 |
| R5 | 处理 attack/active 引起的延迟变化，并说明其可听代价及宿主时间对齐方式 | 阻塞：当前交叉淡化存在已复现的相位抵消 |
| R6 | 用独立预期验证 DSP 正确性，以真实输出路径验证传输与时限 | 已有工具和本机数据，平台范围有限 |

## 音乐性目标

这是后续方案的验收维度，不是当前实现已提供的保证：

1. 调参不额外制造可听静音、爆音或突兀音量跳变；对音高、节奏、起音瞬态、声像和尾音的影响都要检查。
2. 低于限幅阈值的声音，在不改变延迟的参数调整中应保持原波形和时间关系。
3. 改阈值、hold/release、stages、link 时，既要确认参数真正生效，也要确认过渡没有额外问题。阈值变小何时必须满足新上限，需要明确瞬态契约。
4. 改 attack/active 时，同时考虑数据连续性、短时响度、音高/音色、节奏及延迟补偿；“相邻采样没有大跳变”不足以证明音乐性。
5. 保留原参数语义与静态输出行为。不要为了消掉正弦反例，暗中改 attack 的定义、强制所有用户承担 250 ms 延迟，或把未满足的运行时变更延后到停播。
6. 先用可推导信号定位，再用瞬态、节奏、持续音、多频/立体声及真实 MIDI/SoundFont 试听和测量。单个 500 Hz 正弦既是有效反例，也不是全部音乐的代表。

若延迟必须在播放中真实变化，完全保持原来的时间轴不可能作为无条件承诺。应明确所选择的时间调整行为、可听代价和适用边界，再取得相应证据，而不是用“平滑”一词代替说明。

## 已采用的实现与验证决定

| 编号 | 决定 | 状态 / 依据 |
| --- | --- | --- |
| D1 | 沿用 `fluid_settings_set*` 的运行时入口和现有 mixer/limiter 边界 | 保留；没有另增公开 limiter API |
| D2 | 控制端准备完整 replacement；渲染端消费 prepared 对象 | 生产实现基线 `a0ec8c0c`；不把准备工作挪回音频回调 |
| D3 | 单次所有权转移、合并待消费目标、渲染端只发布退休对象，控制/helper 端回收 | 保留既有所有权和资源上界约束；改声音路径时不得破坏 |
| D4 | 正在播放的处理器继续输出，直到 replacement 预热；失败保留已有可用状态 | 保留故障恢复与不输出未填满缓冲的要求 |
| D5 | 当前两个不同延迟的 DSP 输出做 20 ms 线性淡化 | 这是当前实现，不是已认可的最终音乐性方案；已测到抵消，必须继续处理 |
| D6 | 用已知输入及独立公式/性质检查 DSP；实际输出快照只用于后端传输对照 | 保留；禁止把 limiter 自身输出当成其正确性的独立参考 |
| D7 | 用“两份实际录音一起错”的反例检查分析器能否识别 DSP 错误 | 已实现错误 gain、delay、limit、fade-duration 反例 |
| D8 | 音频服务输出端就是本轮平台测试边界 | 用户明确不要求覆盖声卡和扬声器 |
| D9 | 先保留分支上的任务记录、诊断脚本和证据，合并时再整理 | 用户明确要求；不把工作记录直接当作最终上游设计说明 |

现有并发机制的约束：渲染端独占 current/pending；ready/retired 只转移完整对象；最多并行处理两套 DSP，准备期间总驻留有上界；静态用户不启动 helper，helper 空闲退出、销毁时 join。更详细的旧实现分析在本地 `build/scratch/1890/stall/report.md` 和 `review.md`。后续改结构时应重新证明并复测，不把旧检查外推到新结构。

### 尚未选定的延迟策略

| 候选 | 可能解决什么 | 必须付出的代价 / 待验证点 |
| --- | --- | --- |
| 固定总延迟、补齐两路时间位置后只过渡增益 | 避免不同时间点波形直接相加 | 额外延迟预算、bypass 语义、现有静态输出兼容性；不能预设固定最大值适合作者 |
| 连续改变 delay/read position | 避免两路交叉抵消 | 可能产生音高/速度变化；大范围延迟变化所需时间和插值质量 |
| 保持音高的时间伸缩 | 调整时间位置，同时减少音高改变 | 瞬态/声像/音色、副作用、状态与实时成本，不应先引入大型新依赖 |
| 静音或合适边界切换 | 可能减轻过渡可听性 | 不保证等待时长、持续音可能没有边界；默认等待静音不能直接算满足运行时需求 |
| 只改 crossfade 曲线 | 修改部分能量轨迹 | 对完全反相的两路不能保证消除抵消；不能作为充分修复 |

在确定插件宿主契约和比较数据前，不选择上述某一项作为既定最终方案。

## 当前代码与提交

实现历史：

- `5809d164`：本任务修改前基线。
- `fbfa283c`：运行时设置入口。
- `b191a318`：保留旧输出的连续性过渡。
- `c8d7e426`：统一参数过渡与活跃状态测试。
- `a0ec8c0c`：准备和回收移出渲染线程；这是当前生产实现的基线。

当前文件：

- [limiter 状态与处理](src/rvoice/fluid_limiter_impl.cpp)
- [settings 回调](src/synth/fluid_synth.c)
- [最终 mixer 阶段](src/rvoice/fluid_rvoice_mixer.cpp)
- [settings 文档](doc/fluidsettings.xml)
- [limiter 回归测试](test/test_synth_limiter.cpp)

本轮独立验证和需求整理没有进一步修改生产声音策略。后续修改前先读 `git status` 与本文件，保留既有工作；不要把任务整体当作已完成修复。

## 验证结果与明确限制

本机 48 kHz、double、现有 Debug 构建下的 12 组抓取，通过各自声明的独立契约和后端传输检查。最大独立残差约 `5.96e-9`，容差 `2e-7`。

- static/controls 的低电平信号：比较区间逐样本检查 delayed identity。
- 孤立 delay 更新：检查独立延迟输入、共享混合系数、淡化斜率与最终目标。
- DC：约 68% 波形区间检查代数稳态；没有伪称其包络瞬态也已用稳态公式验证。
- rapid：约 27% 区间检查初始/最终收敛；尚未精确验证所有中间合并序列。
- 比较排除最初和最后 0.5 秒，图中灰区表示未做独立波形断言。
- 本机 PulseAudio 接口和 ALSA 默认 PCM 实际也接入同一 PipeWire 服务；不是三个独立服务器或硬件平台的证据。
- Windows WASAPI、macOS CoreAudio 步骤已写好，尚未执行。
- 未检测到持续 2 ms 的近零掉音或非有限值，不代表已证明短时响度没有凹陷。
- 当前采集的 render / render+copy 调用未超过其帧数预算；这不是任意 OS 调度、声部负载或后端的零 xrun 保证。

已复现的阻塞反例：500 Hz，attack 从 250 ms 改为 1 ms。两路相差 124.5 个周期，近似反相；交叉淡化经过各一半时发生抵消。在录音 `2.752–2.753 s` 的 1 ms 窗口测到约 `−32.68 dB` 能量凹陷。该反例符合当前淡化规则，却不满足声音透明这一更强目标，不能以“规则测试通过”将其判为修好。

其他已完成检查：14 项分析器正/负例；56 项项目测试；新增公共 `write_float`/`process`/s16 的请求分块对照；float ASan/UBSan limiter 和 callback 生命周期两项检查。s16 对照要考虑现有 `32766` headroom 比例和 dither，不能误用 `32768` 作为同一数学归一化。

早先 C++ 分配守卫、并发/故障恢复、TSan、JACK dummy 和配对测量记录保存在本地 stall 报告；那些结果属于对应代码和工作负载，修改实现后按受影响范围重新验证。

## 脚本、证据与复现入口

### 随分支保存的工具

| 文件 | 用途 |
| --- | --- |
| [test/limiter_audio.py](test/limiter_audio.py) | 启动真实后端、抓取 monitor、调用独立检查，输出 WAV/CSV/JSON/HTML |
| [test/limiter_audio_probe.cpp](test/limiter_audio_probe.cpp) | 在真实最终 mixer 前输入已知信号；预分配采集空间，停机后写文件 |
| [test/limiter_oracle.py](test/limiter_oracle.py) | 独立解析信号、延迟和稳态增益性质；明确覆盖范围 |
| [test/limiter_audio_test.py](test/limiter_audio_test.py) | 分析器的故障注入与“两份一起错”反例 |
| [test/plot_limiter_phase_case.py](test/plot_limiter_phase_case.py) | 重建现有 500 Hz/250→1 ms 反例的科研图；需额外绘图库和 nature-figure 对齐工具 |
| [test/limiter-audio.md](test/limiter-audio.md) | 公式、断言边界、各平台回录步骤和结果解释 |

音频工具只需 Python 标准库及对应系统录音命令；绘图依赖是可选的，不属于常规 CTest 依赖。

```sh
# 在仓库根目录；每次 record 都使用新的目录和 synth。
python3 test/limiter_audio_test.py
python3 test/limiter_audio.py record --driver pipewire --mode controls --output build/scratch/1890/audio/next-controls
python3 test/limiter_audio.py record --driver pipewire --mode delay --source sine --output build/scratch/1890/audio/next-phase
python3 test/limiter_audio.py record --driver pipewire --mode controls --source dc --output build/scratch/1890/audio/next-dc
python3 test/limiter_audio.py record --driver pipewire --mode rapid --period 64 --output build/scratch/1890/audio/next-rapid
```

绘图前在任务 scratch 内建立临时环境，安装 numpy/matplotlib；PDF 碰撞审计另需 PyMuPDF。当前图使用 numpy 2.5.4、matplotlib 3.11.2、PyMuPDF 1.28.2；精确锁定版本在证据目录的 `requirements-lock.txt`。绘图命令示例：

```sh
python test/plot_limiter_phase_case.py --input build/scratch/1890/audio/oracle-pipewire-delay-sine --output build/scratch/1890/audio/next-figure --skill-scripts ~/.agents/skills/nature-figure/scripts
```

该绘图脚本只适用于已声明的正弦反例；混合系数是从实际输出反推的诊断量，不能冒充独立正确性预期。波形和能量的独立对照来自输入公式。每次图形修改后应重新运行对齐、字号及 PDF 碰撞审计。

### 本地保留、不随分支迁移的材料

- `build/scratch/1890/audio/RESULTS.md`：12 组结果、原始数据与限制；音频目录约 171 MiB。
- `build/scratch/1890/audio/oracle-*`：每组 manifest、f32/WAV、事件/计时 CSV、分析 JSON、交互报告。
- `build/scratch/1890/audio/figure/`：PDF/SVG/PNG/TIFF、原始绘图脚本快照、source CSV、输入哈希和图形 QA，约 2.9 MiB。
- `build/scratch/1890/stall/`：之前实现的线程/分配/性能证据及重跑脚本。
- `build/scratch/1890/requirements/`：本轮读取的插件源码快照和来源版本。

这些目录被 Git 忽略。切换机器或删除 worktree 前，保存需要的原始证据，或按脚本重新采集；不要把本地链接存在误认为附件已进入 Git。绘图临时环境和下载缓存已清理；没有留下采集/播放进程。

## 后续队列

- [x] P0：回查 issue、discussion、插件仓库，固定资料版本，建立本文件。
- [ ] P1：完整梳理插件的 Confirm/状态恢复/设置线程/音频路径、全部延迟上报及 PDC；区分库侧问题与插件置零路径。
- [ ] P2：把“运行时参数更新、预读与输出总延迟、宿主补偿、声音过渡”写成明确契约，保留运行时 attack/active 的需求。
- [ ] P3：按同一组信号和音乐素材比较候选策略，记录延迟、音高、短时能量、瞬态、声像及计算/内存成本；形成有依据的方案决定。
- [ ] P4：实现最小可审查的完整路径，继续保持实时线程与所有权约束；不靠降低测试强度把旧反例涂绿。
- [ ] P5：补上新契约的独立 oracle、快速更新的中间行为检查、过载瞬态、真实音乐和插件多实例验收。
- [ ] P6：在真实 Windows/macOS 和不同 Linux 服务上补测；必要时适配作者插件中的重建分类、设置提交与延迟补偿。
- [ ] P7：合并前整理英文设计说明、正式回归测试、可选诊断工具、提交/PR、AI 辅助披露与未验证范围；不自动发布或上传证据。

每轮结束更新本文件的当前结论、决定状态、证据路径和下一步。方案被替换时保留简短原因和反例，不让旧建议在下一轮被误当成已经选定的方向。
