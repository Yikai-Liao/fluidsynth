# 已做过的性能优化（按模块）

本地协作清单，不提交上游。基线为 `upstream/master` @ `2e838ae6`。
"状态"：**我** = 本次会话交付并已推送；**团队** = 其他会话/已有分支；**撤回** = 不再使用。

## 1. 插值 `src/rvoice/fluid_rvoice_dsp.cpp`、`fluid_rvoice_dsp_sinc.hpp`

| 优化思路 | 分支 | commit | 状态 |
|---|---|---|---|
| 4 阶 cubic：相邻输出样本复用 4 个抽头（窗口左移一位），每样本只读 1 个新样本点而不是 4 个 | `perf/interp-tap-window` | `486e327a` | 我 |
| `compute_interpolation_steps`：剩余缓冲先结束时跳过 64 位除法（带乘法溢出保护，超范围仍走除法） | `perf/interp-buffer-steps` | `bd6c7ec2` | 我 |
| 高阶 sinc：用预计算相位表替代每样本的 sin 核计算（归一化核） | `perf/sinc-phase-table-rebased` | `7491f437` | 团队（另有 `perf/sinc-phase-table`、`perf/sinc-phase-table-pr-20261019` 两个早期版本） |
| sinc：两个输出样本的点积交错，共享抽头加载 | `perf/sinc-paired-samples` | `c9e186aa` | 我（叠在 rebased 上） |
| sinc：核抽头直接加载进累加循环，去掉临时数组 | `perf/sinc-kernel-fused-taps` | `a8354c9c` | 我（叠在上一条上） |

## 2. IIR 滤波器 `src/rvoice/fluid_iir_filter_impl.cpp`

| 优化思路 | 分支 | commit | 状态 |
|---|---|---|---|
| cutoff 平滑期间避免重复重算系数（复用 last_fres/last_q 结果） | `perf/iir-cutoff-coefficient-reuse` | `12d885e1` | 团队 |
| 系数固定的低通：4 样本因果链闭式展开（p2/p3 多项式，反馈保持 double） | `perf/iir-stable-block` | `707b52d6` | 团队 |
| 同上思路的 double 采样类型版本 | `perf/iir-stable-double-blocks` | `491bb399` | 团队 |
| apply 循环内系数保持 `fluid_real_t`，去掉每样本 double↔float 转换 | `perf/iir-coeff-double` | `e3341732` | 我 |
| cutoff/Q 不变时跳过每样本 ramp 检查（复制一个无检查循环） | `perf/iir-steady-loop` | `2d38734e` | 我（与 `iir-stable-block` 同区域，互斥） |

## 3. 混音与线程 `src/rvoice/fluid_rvoice_mixer.c`（团队分支已改名 `.cpp`）

| 优化思路 | 分支 | commit | 状态 |
|---|---|---|---|
| rvoice mixer 改 C++，worker 状态用 `std::atomic<int>`（next-voice/stop/ready），用构造/析构替代 memset 活原子对象 | `perf/mixer-std-atomic`（另有 `perf/dev-mixer-std-atomic`、`perf/mixer-std-atomic-test-cleanup-20261103`） | `4c6e9cd0` / `03db9b06` / `a55f3375` | 团队，上游待合并 |
| 混音时增益已稳定则跳过每样本 ramp 运算（新增向量化分支） | `perf/mixer-stable-amp` | `d4afd04a` | 团队 |
| 完成声部移除走物理 membership 索引，O(1)，保留重复/陈旧成员的扫描回退 | `perf/dev-mixer-finished-index` | `b2045d6c` | 团队 |
| 用全局锁原子替换为 `__atomic` 内建（单线程 −1.3~1.9 %） | ~~`perf/builtin-atomics`~~ | 已删除 | **撤回**（编译器特定语法；由 std::atomic 分支负责） |
| 发现（无分支）：worker 每块重复清零自己的目标缓冲约占 4 核渲染 5 %（早期 20 % 的测量把 worker 循环里的 `getenv` 也计入了）；每块唤醒 worker 的上下文切换带来明显 sys 时间（4 核 3.6 s sys） | — | — | 待做，需与 std::atomic 分支配套；5 % 的收益不足以支撑改 `fluid_rvoice_buffers_mix`

## 4. 混响 `src/rvoice/fluid_rev*`

| 优化思路 | 分支 | commit | 状态 |
|---|---|---|---|
| Dattorro 输出抽头索引取模开销 | `perf/dattorro-tap-wrap` | `1ee6ef9f` | 团队 |
| 缓存 delay line 长度，每样本少一次 `size()` 调用链 | `perf/reverb-delay-size` | `e89c8734` | 我 |
| 静音输入且引擎无可移动状态时跳过 `processmix`（虚函数能力查询，仅 Dattorro 允许） | `perf/reverb-idle-skip` | `773935f6` | 我 |
| 为什么 FDN/Signalsmith/Freeverb 不能跳过（调制相位推进、DC 偏置） | 记录在 `progress.md` | — | — |

## 5. 合唱 `src/rvoice/fluid_chorus.c`

| 优化思路 | 分支 | commit | 状态 |
|---|---|---|---|
| 静音输入时只推进调制器/相位/缓冲索引，不读写延迟线、不做混音 | `perf/chorus-idle-skip` | `35443f4d` | 我 |

## 6. 声部分配与管理 `src/synth/fluid_voice.c`、`fluid_synth.c`、`fluid_chan.*`

| 优化思路 | 分支 | commit | 状态 |
|---|---|---|---|
| 按 MIDI 通道索引已分配声部 | `perf/dev-channel-voice-index` | `20bbca67` | 团队 |
| exclusive class 成员位图，跳过不可能匹配的声部（普通 MIDI 收益未确立，每 synth +8 KiB） | `perf/dev-exclusive-candidates-standalone` | `ebe48363` | 团队 |
| exclusive-class 扫描限制在通道索引内 | `perf/dev-exclusive-channel-scan` | `24530a25` | 团队 |
| 完成 rvoice 通过不可变 owner 指针解析，免全量 polyphony 扫描 | `perf/dev-finished-voice-owner`、`perf/dev-finished-voice-owner-standalone` | `4c1445c9` / `3c2fe217` | 团队 |
| 保留最低可用声部搜索提示 | `perf/dev-lowest-free-voice` | `2b896710` | 团队 |
| 同键事件批量处理 | `perf/dev-same-key-event-batch` | `30f5a9a8` | 团队 |
| portamento legato 返回 int，保留 INVALID_NOTE 哨兵，避免普通 Note On 触发全池扫描 | `perf/legato-invalid-note-sentinel` | `2442aeed` | 团队 |

## 7. MIDI 解析 `src/midi/fluid_midi.c`

| 优化思路 | 分支 | commit | 状态 |
|---|---|---|---|
| 同一时刻多个 track 的事件用有界堆合并 | `perf/dev-midi-track-merge`、`perf/midi-track-merge-20261021` | `1f8dba15` | 团队 |

## 8. 已撤回或放弃

- `perf/builtin-atomics`：编译器特定 `__atomic` 语法，已本地与远端删除。
- `perf/phase-incr-cache`、`perf/iir-short-recurrence`：指针停在基线上（实验未采用）。
- `perf/iir-param-reorder`：每次调用前缓存系数方向，实测中性，删除。
- 结构体内存布局重排（ADSR / rvoice）、mixer 缓存行填充、`fluid_iir_filter_calc` 调用点内联：见 `progress.md`，均无收益。

## 9. 尚未覆盖的模块（动手前先看这里）

- `src/rvoice/fluid_adsr_env.c` 包络计算（只做过结构体布局实验，无收益）。
- `src/rvoice/fluid_rev_signalsmith.cpp` 与 FDN 内部（只有 `dattorro-tap-wrap` 覆盖了 Dattorro）。
- `src/sfloader/` SF2/SF3 加载：实测 MuseScore_General 加载 + 启动仅 ~0.19 s，占整轮渲染 <2 %，优先级低。
- `src/rvoice/fluid_limiter*.c` limiter。
- 多线程渲染的 worker 缓冲清零与唤醒开销（第 3 节最后一行）。
- `fluid_player` / `src/synth/fluid_synth.c` 事件调度（已有 track merge，其余未做）。

## 10. 交付栈（本会话）

`perf/interp-buffer-steps`、`perf/iir-coeff-double`、`perf/iir-steady-loop`、`perf/interp-tap-window`、`perf/reverb-delay-size`、`perf/reverb-idle-skip`、`perf/chorus-idle-skip`、`perf/sinc-paired-samples`、`perf/sinc-kernel-fused-taps`（后两个叠在 `perf/sinc-phase-table-rebased` 上）。

指令数几何平均 0.878（−12.2 %），墙钟 0.913（−8.7 %），PCM 与上游最多差 1 个 ULP（来自 IIR 的 `-ffast-math` 重结合），sinc 路径逐字节相同。每条分支单独 `make check` 50/50 通过。
