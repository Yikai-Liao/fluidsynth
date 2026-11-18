# Limiter 运行时设置

更新：2026-11-18。当前开发分支：`fix/dev-limiter-runtime-settings`。
原实验分支 `fix/1890-realtime-limiter` @ `87a53f2c` 保留，未改写。

## 当前决定

按照 [#1890](https://github.com/FluidSynth/fluidsynth/issues/1890) 的最新反馈，
本项交付目标是：**已有 synth 上修改全部七个 limiter 参数，无需重建 synth、
重载 SoundFont 或清除 voices。允许重新配置 limiter 时短暂中断声音及改变延迟。**

- [derselbst 的回复](https://github.com/FluidSynth/fluidsynth/issues/1890#issuecomment-6923925341)：
  认为另加 `max-lookahead` 设置多余，可查询现有 attack 范围；对 active 的延迟取舍无偏好。
  这不等于他要求所有用户采用固定 250 ms 延迟，也不代表已经批准具体实现。
- [prof-spock 的回复](https://github.com/FluidSynth/fluidsynth/issues/1890#issuecomment-6954709411)：
  使用文本编辑后 Confirm 的手动配置，不要求 DAW 连续自动化或声音连续；
  只希望 limiter 设置更新不再重建 synth，接受切换的声音副作用。

不新增 max-lookahead，不固定到最大延迟，也不把动态宿主 PDC 或无缝自动化作为
本项完成条件。`output-limit=1` 仍会限制超过 1 的输入，不代替真正的 inactive 旁路。

## 实现契约

1. 沿用 `fluid_settings_setnum/setint`；七个 limiter 设置均标记为 realtime。
2. 控制线程准备完整新 DSP；音频线程在处理块边界接收，立即替换当前 limiter。
   新 DSP 的音频历史与包络从初始状态开始，lookahead 未填满时输出零。
3. attack 决定当前延迟。active=0 后直接旁路，不保留 limiter 延迟；再次启用从空缓冲开始。
4. 未消费的请求可合并；停止调参后收敛到最后成功准备的配置。
5. 分配、configure、析构、回收均不放回渲染线程；沿用已有控制锁、mailbox 和退休对象回收。
   回收 helper 空闲退出，销毁时 join；embedded 在后续控制调用或销毁时回收。
6. 准备失败保留此前可用 DSP／已发布请求，记录错误。现有 settings callback 无返回状态，
   因而 settings 的记录值可能已更新，不能将 setter 成功返回解释为 DSP 分配一定成功。
7. 未修改 synth、SoundFont 或 voice 生命周期。没有新公开 API 或 ABI 变化。

决策注释放在 `src/rvoice/fluid_limiter_impl.cpp` 的处理入口旁，只解释最终实现的需求、
状态重置和声音／延迟代价，并引用需求方回复。旧实验行为不写入生产注释。
用户文档在 `doc/fluidsettings.xml` 中说明控制线程调用、缓冲重置和延迟变化。

## 验证

新增回归先在原 `87a53f2c` 的生产实现上运行：attack 更新后首块应为新空缓冲，
旧实现失败；当前实现通过。该断言验证的是此次选定的重置契约，不把旧交叉淡化实验称为上游 bug。

当前回归覆盖：

- 七个参数实际生效、hold/release 恢复、smoothing stages、立体声 link、阈值和 attack 延迟。
- 44,101／48,000／96,000 Hz 的重新配置、空缓冲、直接旁路、SoundFont 对象和活动 voices 保留。
- 并发发布／回收、连续更新后的最终收敛、准备失败恢复及 synth 销毁后 settings 复用。
- 渲染阶段 C++ 分配／析构守卫；公共 float/process/s16 入口及不同请求分块一致性。
- 独立波形 oracle：孤立 delay 更新逐样本验证旧延迟输入 → 空缓冲 → 新延迟输入；
  切换块受 setter／callback 观测范围约束，两通道必须使用同一个块。
- 分析器正负例包含两份录音同时错误的 gain、delay、limit 和额外静音；
  分组／rapid 更新只检查稳定区间和最终收敛，不声称验证所有中间配置顺序。

本轮实际结果（Linux x86-64，Debug；非性能基准）：

| 配置 | 结果 |
| --- | --- |
| GCC double／float、Clang double，Signalsmith ON | 各 check 56/56 |
| GCC double，Signalsmith OFF | check 55/55 |
| GCC float，ASan+UBSan，含 leak 检查 | limiter／callback 生命周期 2/2 |
| Python 分析器正负例 | 14/14 |

PipeWire 48 kHz double 的本轮实际回录：

| 场景 | 时长／period | 独立最大残差 | 波形覆盖率 | 后端传输最大误差 |
| --- | --- | --- | --- | --- |
| delay，500 Hz sine | 6 s／256 | 1.79e-9 | 100% | 0 |
| controls，multitone | 5 s／256 | 3.73e-9 | 98.76% | 0 |
| rapid，multitone | 5 s／64 | 3.73e-9 | 56.08% | 0 |

三组未观察到超出帧数预算的 render 调用；这不是全负载／全平台的时限保证。
覆盖率按排除首尾各 0.5 s 后的比较区间计算。测试到音频服务 monitor 为止，
不覆盖声卡和扬声器。未新增听感透明性结论。

复现入口：常规 Debug 构建 `cmake --build build --target check`；切换
`enable-floats`／编译器／`enable-signalsmith` 使用独立构建目录。
`LIMITER_AUDIO_TEST_DIR=build/scratch/limiter-runtime/analyzer python3 test/limiter_audio_test.py`
运行分析器。三组回录使用 `test/limiter_audio.py record --driver pipewire`，分别指定
`--mode delay --source sine --seconds 6`、`--mode controls --seconds 5` 和
`--mode rapid --period 64 --seconds 5`，每次使用新 `--output` 目录。

本轮临时构建、日志与回录位于 `build/scratch/limiter-runtime/`，汇总后清理；
常规 `build/` 留作构建缓存。旧 worktree 的原始证据未动。

## 范围与后续

- 本轮完成库侧实现及本机验证，未修改或运行 FluidSynthPlugin。
  原插件的 `_restartingKeysList` 仍需移除对应 limiter 条目，才能使用库的新能力。
- 宿主若补偿 limiter 的延迟，需要处理 attack／active 的变化；此实现不提供自动 PDC。
- Windows WASAPI、macOS CoreAudio 仍未实测；不把 Linux 的验证外推到这些平台。
- 未推送、未发送 issue 评论、未开 PR。投稿前需要从最新上游整理独立补丁，
  排除继承的 N14 修改及实验工具，核对完整 diff 与 CI；原 dev／实验历史保留。

## 历史证据

原分支任务记录可用 `git show 87a53f2c:LIMITER_REALTIME_TASK.md` 回查，
其中有插件固定源码版本、原始音频证据路径和测试边界。
原分支 `build/scratch/1890/` 属于另一 worktree 的既有材料，本轮未删除或修改。

原实验使用不同延迟输出的 20 ms 交叉淡化；500 Hz、250→1 ms 的独立反例测到
约 −32.68 dB 的 1 ms 能量凹陷。该证据仍有效，但它评价的是旧实验，
不再作为当前重置方案的验收条件。`test/plot_limiter_phase_case.py` 继续用于旧报告；
当前采集与分析入口和边界见 `test/limiter-audio.md`。
