# Stable voice gain mixer：当前上游独立测量

2026-11-09；基线 `8210cb1bdc42718db5d68c2bc5aaea99ed434472`，候选 `ea8e842ec1329ca1c7a646610c6f8a7d55ae8e0a`，仅生产 +10/−1。逐对原始耗时、A/A 对照、bootstrap 区间、命令、二进制及素材身份见 [results.json](results.json)。不上传 MIDI、SoundFont 或 PCM。

## 方法

Intel i7-10875H／Linux x86-64，taskset 固定 CPU 2，Release／上游默认优化 flags，OpenMP 关闭，单 synthesis core、44.1 kHz、默认效果器。GCC 16.2.1／Clang 23.1.4，各测 float 与 double。

使用未裁剪的 Dance_GM、e1m1、Etude 三首完整 MIDI，GM_Roland.sf2。前两首与音源来自 FluidSynth/testdata 固定提交，Etude 来自用户 symusic 固定提交；来源 URL、大小、SHA256 记录在 JSON。曲目/音源有限，不代表所有音源或硬件。

每组先分别渲染原始 float PCM、比较 SHA256，再交错配对基线与候选，轮换先后顺序；正式计时输出到 /dev/null，包含进程启动、音源加载、完整渲染和清理，不称 render-only。GCC double 每曲 12 对，另有 6 对同基线 A/A；其他配置每曲 6 对。区间是配对 log ratio 的 10,000 次 percentile bootstrap，95%，seed 1905；不删异常值、不选择最优单次。

## 结果

| GCC double 完整曲目 | 候选／基线配对几何均值 | 95% 区间 | 耗时下降 |
|---|---:|---:|---:|
| Dance_GM | 0.9704 | 0.9666–0.9744 | 2.96% |
| e1m1 | 0.9823 | 0.9793–0.9850 | 1.77% |
| Etude | 0.9824 | 0.9810–0.9837 | 1.76% |

A/A 比率分别 1.0044／1.0025／1.0008，揭示约 0.1–0.4% 的漂移／噪声，不将它当作精确零基线。Clang double 前两曲有改善，Etude 区间跨 1；float 大多收益中性或受离群耗时影响，不能承诺全面提速。完整 12 组结果包括不确定／不利结果都保留在 JSON。

12 组完整 MIDI 的基线／候选 PCM 全部逐位一致；仅比较相同工具链和精度的一对，不声称不同工具链互相 bit-identical。另以 VintageDreamsWaves-v2 的 12 音符序列检查声像／增益变化、Note Off 和 1/63/64/65/127/128/257 帧输出请求，四配置输出也分别一致。

候选四配置各 `check` 51/51。本次未运行完整 check_rendering、OpenMP、sanitizer 或非 x86-64 硬件测量；远端 CI 尚待完成。结论是适合小型独立优化 PR 的 double 实测收益，不是全平台性能保证。
