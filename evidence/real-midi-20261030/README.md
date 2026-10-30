# 9 项问题的真实 MIDI 验证

2026-10-30；基线 [`fc1c55f4627f0050c820d72784c5463e12620140`](https://github.com/FluidSynth/fluidsynth/commit/fc1c55f4627f0050c820d72784c5463e12620140)。这里记录原始曲目、触发事件、运行状态和诊断补丁对照，供后续判断是否提交上游。

## 当前结论

| 项目 | 真实材料结果 | 额外条件 |
|---|---|---|
| 1 两次 CC66 周期继承旧 voice ID | **确认**：原始 `Two of Us.1.mid`，两种普通 SoundFont | 两种SoundFont均为默认Poly/note-cut0/P256 |
| 2 router_clear 漏 CC66 松踏板 | **确认**：同一原曲，待结束事件漏转发 | 播放中明确执行一次 `router_clear` |
| 3 polyphony 缩容漏完成回收 | **确认**：原始 Etude，256→64 后遗留 22 个管理计数 | 播放中修改容量；声音本身可已静音 |
| 4 Freeverb / FDN reset 遗留滤波历史 | **真实路径确认，影响小**：原曲 GS reset 及 Etude＋System Reset | 非默认混响；默认 Dattorro 对照无变化 |
| 5 Lexverb roomsize=0 reset | **真实路径确认，影响小**：Etude播放约10秒后 room .5→0＋System Reset | 选择 Lexverb、roomsize=0、reset |
| 6 custom IIR Q_ZERO_OFF | 未找到实际宿主按所需方式调用的证据 | 不能把普通 CC71/74 当作确认 |
| 7 sample 内容/loop 缓存 | 未找到已 optimize 的同一个 sample 被实际宿主复用修改的证据 | 已查 reload 路径通常新建对象 |
| 8 Mono CC123 漏记踏板保持键 | **有条件确认**：原始 Bill Evans MIDI，独立源码补丁对照 | 宿主明确设 Mono＋note-cut=2；默认 Poly 负控不变 |
| 9 扩容中途构造失败 | 本轮仅静态源码/宿主路径审计，不计真实MIDI自然触发 | 普通 MIDI 不会自行扩容或制造分配失败 |

因此，默认普通播放有直接证据的是 **第 1 项**；原曲加指定宿主动作/设置确认 **第 2、3、8 项**；**第 4、5 项**真实可达但本次测得历史相关信号很小。不能把这批记录写成“9 项都在普通 MIDI 默认播放复现”。

## 1：默认 Poly 原曲证据

完整原始文件未改字节，从开头原生播放 25 秒（不是先裁剪再验证）。44.1 kHz、64-frame block、FLOAT、1 synthesis core、默认效果。GeneralUser GS与TimGM6mb完整原曲均有直接P256双臂重放，各自与早先P512输出逐位相同；TimGM峰值97/85。新增确认见 `item1/timgm-p256-confirmation.json` 与 `*-timgm-p256.log`，原P512证据仍保留。衍生夹具两种SoundFont也均为P256。原文件 15 tracks、PPQN 96、ports 全为 0；完整播放输入 14,363 events、6,662 positive NoteOn、475 CC66。25 秒窗口 1,461 events，GeneralUser 峰值 134 voices、0 handler errors。

关键原始事件（channel、track 均从 0 开始）：

| tick / 时间 | track / ordinal | ch5 事件与状态 |
|---|---|---|
| 2305 / 13.216726s | 6 / 216、217 | NoteOn47 后 CC66=127 |
| 2639 / 15.131861s | 6 / 269 | 原曲同键 NoteOn velocity0；旧 voice 进入 release |
| 2687 / 15.407090s | 6 / 280 | CC66=0，上一周期结束 |
| 2689 / 15.418558s | 6 / 282、283 | 先 CC66=127，再 NoteOn47；新音不应被新周期保持 |
| 2735 | 6 / 286 | 新音 NoteOff；原版仍 held，诊断版正常 release |

实际 native frame679872/event752：旧 voice `id154, has_noteoff1, sostenuto1`，新踏板 order339；原版新音错误继承154，单条件诊断版分配339。原曲两个相关通道均没有连续重复 CC66 高值，区别于 F46。

| SoundFont | 不同交错 samples / 2,205,056 | 最大 PCM 差 | 差值 RMS / 诊断版 RMS |
|---|---:|---:|---:|
| GeneralUser GS | 1,380,097 | 0.0483546555 | 10.11698% |
| TimGM6mb | 1,380,179 | 0.0774842463 | 17.57182% |

第一处 PCM 差异早于上表示例：frame412417 / 9.351859s。之前 tick1631 的 key55/50 NoteOff 中，原版继承75/74（低于踏板 order88），诊断版95/96正确 release。不可把第一处差异归给15.4秒示例。原版重复运行逐位相同；不含 CC66 的 Etude 前25秒负控原版/诊断版逐位相同；GeneralUser P256/P512 两组也逐位相同。

[`fixture/`](fixture/) 是 **94-byte real-derived 辅助夹具**，仅两个同音和四个真实踏板消息。原始相对 tick/顺序保留，原 bank/program/mix/tempo 移到起点作设置；其他音乐事件删除。来源事件清单明确注明这些变换，它不代替完整原文件验证。夹具首音选择2305→2351；上表完整原曲使用保留中间同键重弹后的2639 NoteOff，两条事件链分别记录。两种 SoundFont 均复现旧ID0/新ID1差别。

## 2：原曲＋运行中清路由

原生 player → 默认 router → synth。首次 ch4/key62 NoteOn frame97024、CC66down97280、NoteOff108928；宿主在 **109056 / 2.472925s** 清规则，之后原始 CC66up frame193920 /4.397279s。

原版漏转发该松踏板；两条件诊断补丁转发。两臂同为14,363输入 events，分别56/57转发，唯一差别为该 CC66up；只转发过一个 positive NoteOn，因此没有混入第1项同键重弹机制。5.0068s原版/诊断版 active=1/0，CC66=127/0。首处 PCM 差异193985frame，最大差0.0007866701。原生 EOF约213.5757s直接重置踏板后两臂最终都 DONE；这里没有永久挂音结论。

原始状态日志和精确比较见 `item2/`（完整记录私有保留）。

## 3：原曲＋polyphony 256→64

Etude 原字节，原生完整播放；44.1kHz、256-frame block、FLOAT、1 core、GeneralUser GS。时间由独立未改版活动计数预选：frame2510336 /56.923719s有86 voices；此时宿主 `fluid_settings_setint(...,"synth.polyphony",64)`。

| 观察 | 原版 | 一行诊断版 |
|---|---:|---:|
| 约1秒后 active | 84 | 62 |
| 原始派发 events / NoteOn 错误 | 1192 / 10 | 1192 / 10 |
| DONE | 到180秒仍未到达 | 114.3002秒 |
| 最终遗留 active | 22 | 0 |

共同前5,481,728 frames逐位相同；原版末段峰值约4.2e-45，证据指向完成通知/计数及播放器结束状态，不能改写成可听挂音或已量化内存增长。未缩容对照有1个原有 NoteOn 错误，缩容后的10个错误相同，不声称缩容不丢音。更极端256→4历史对照也保留，但不以它代替64容量证据。见 `item3/`（完整记录私有保留）。

## 4：真实 reset 的影响分量

原始 `Two of Us.mid` 在最后 NoteOff约6ms后有合法 GS reset，之后无新音；另以 Etude播放至10秒调用一次公开 System Reset。每案比较 normal原版/诊断版及无此前音符历史的反事实控制，用 `(normal原版−normal诊断版)−(silent-history原版−silent-history诊断版)` 分离历史残留与 reset 自身 DC transient。控制中省略音符，不冒充自然原曲。

| reset 场景 | Freeverb 历史相关峰值 | FDN 历史相关峰值 |
|---|---:|---:|
| 原曲 GS reset | −138.83 dBFS | −115.17 dBFS |
| Etude＋宿主 System Reset | −100.35 dBFS | −75.26 dBFS |

默认 Dattorro 整曲 PCM 原版/诊断版逐位相同。没有听感测试；组件人工输入中较大的残音数值不转写为这里的真实曲目影响。原曲 normal 两臂均有1个同样的重复 NoteOff返回错误，host两臂均8个，reset本身成功。见 `item4/analysis.json`（完整记录私有保留）。

## 5：Lexverb roomsize0＋reset

原始 Etude字节和后续事件不变，明确选非默认Lexverb；frame440960（约10秒）依次调用 `fluid_synth_set_reverb_group_roomsize(synth,0,0.0)`、`fluid_synth_system_reset(synth)`，均返回0，中间不渲染。原roomsize .5，setter后getter确认为0；两项队列操作按序应用。对应Qsynth公开控制路径，但未运行GUI。

独立源码仅将reset的 `set_positions(1,1)` 改为 `(0,0)`，没有叠加第4项。normal两臂reset前PCM逐位相同；零此前音符历史控制在reset后4秒内两臂差为0。含首个reset块的一秒历史相关峰值 **4.18592244e−5 / −87.564 dBFS**，RMS5.82705e−7。四臂均DONE frame5040576、569 positive NoteOn、8个相同NoteOff错误、无非有限输出。是小幅可测差异，不据此声称明显可闻或默认MIDI独立触发。见 `item5/analysis.json`（完整记录私有保留）。

## 8：原曲事件＋明确 Mono 宿主设置

`Evans Bill/You Go to My Head.mid`：原字节14,973，3 tracks，PPQN384，解析3574 events、1471 positive NoteOn。ch2/key72 on11.598958s → CC123 11.703125s → key44 on11.994792s，Sustain127；中间原始冗余 CC123/NoteOff完整保留。

原文件没有要求 Mono。宿主在文件开头 GS reset 后、首个 positive NoteOn 前设 Mono＋note-cut2。25秒原生窗口238 events，peak13，原版/各对照均1个相同冗余 NoteOff错误。原版 CC123留下过时 `key_mono_sustained46`，实际被保持键72没有被下一键44切掉。

先做“在特定 CC123 前额外普通 NoteOff”的反事实控制，随后用**独立干净源码仅加7行字段维护**重放原始事件；源码诊断版正确记录72，PCM与反事实控制逐位相同，同时保留原始 monolist last_key72。由此排除了控制中其他单键维护的混杂。首差11.997460s，最大0.006212729，差值RMS/control RMS2.36885%。该7行仍需多layer、模式切换等组合review。

默认 Poly/note-cut0 的原版/诊断版负控逐位相同；此项不能归为默认播放器问题。全量17,256 Lakh文件重新核对SHA无差异；16,982文件两解析器 note counts一致，24文件给出857个**粗候选事件**，不等于857缺陷或24首独立作品。见 `item1/full-item8-summary.json`（完整记录私有保留） 及 item8状态/构建材料。

第9项的指定构造失败注入结果来自用户提供的先前记录；本轮没有重做该故障注入，也没有复现自然OOM。当前新增证据仅为源码所有权缺口及宿主扩容路径。

## 材料与复现

- [`sources.json`](sources.json)：完整原文件来源、字节数、SHA256、基线/DSO/SoundFont身份。完整原 MIDI 与原始输出日志另保留在私有复现归档中；公开包不含整曲或 PCM。
- 完整复现脚本、参数说明、原始状态日志和诊断源码在私有证据包保留；本目录提供结果摘要、来源、输出hash及两音符夹具。
- [`pcm-hashes.json`](pcm-hashes.json)：各次保留输出的大小与SHA256。
- 文中 `item*/` 是私有完整证据包内路径，并非本分支在线目录；本次不上传完整日志或压缩包。后续整理的六个独立修复分支见根目录待办，各自尚未提上游PR。
- Etude来自用户 symusic仓库固定提交，仓库根MIT许可证；这里保留来源事实，不从曲名推定曲谱权属。Lakh来源及归属见其[官方页面](https://colinraffel.com/projects/lmd/)。
