# 贡献历史与偏好依据

核对：2026-11-09（UTC+08）；上游 master：`8210cb1bdc42718db5d68c2bc5aaea99ed434472`。

## 覆盖范围

- GitHub 账户 Yikai-Liao 提交的全部 21 个 PR：20 merged，1 closed/unmerged，无 open；逐个阅读正文、全部 conversation comments、reviews 与 inline comments，核对最终变更规模。
- 交叉检查全仓库可见的 6,901 条 issue/PR conversation comments 与 1,149 条 inline review comments，找到本账户 11 条 conversation comments、3 条 inline comments，展开相关完整往返。GitHub involves 搜索漏掉了 #1718，已由评论流补齐；不只依赖搜索索引。
- 枚举全部 216 个公开 Discussions，检查正文、comments、replies；本账户只参与 #1869，相关分页无遗漏。
- 合计 21 个自己的 PR、3 个他人的 PR（#1718/#1865/#1889）、3 个 issue（#1856/#1866/#1882）和 1 个 Discussion。#1897 另作安全披露背景读取。本账户给出的 review 也纳入检查。
- 不包含不可见的私信、邮件、已删除内容和私有 security advisory。用户偏好来自 fork 原有记录，不冒充本轮获取的完整私人对话。

## 维护者与作者明确说过什么

| 出处 | 原意／短引文 | 可复用判断与边界 |
|---|---|---|
| [derselbst：DLS #1882](https://github.com/FluidSynth/fluidsynth/issues/1882#issuecomment-6426126218) | “only active maintainer and PR reviewer”；先 minimal behavior fixes；约 500 行 changed code 容易消化；大改先 issue/plan；重构前补测试 | 首先优化审核负担。500 行是该讨论的经验上沿，不是全项目硬阈值；内部接口灵活，公开 API 不可破坏 |
| [rsp4jack：DLS #1882](https://github.com/FluidSynth/fluidsynth/issues/1882#issuecomment-6527670251) | 不赞成重写；大 issue 不清楚具体 bug/fix；“issues and PRs that do one thing” | DLS 原作者的直接意见，不等同唯一维护者身份。内存／数值安全与失败处理可从小修开始，不提交笼统 AI 审计包 |
| [derselbst：模式切换 #1865](https://github.com/FluidSynth/fluidsynth/pull/1865#issuecomment-6475336602) | “I don't want to over-complicate this”；统一强制 release 更 comprehensible | 在该行为上明确选择可理解性；不是允许忽略全部 MIDI 规范 |
| [derselbst：OSAL #1718](https://github.com/FluidSynth/fluidsynth/pull/1718#issuecomment-6530924185) | 新增约 1,000 行意味着维护承诺，使用需求反馈不足 | 有性能问题不代表必须接纳整套 OSAL；维护成本与需求证据一起评估 |
| [derselbst：小型 atomic 方案 #1718](https://github.com/FluidSynth/fluidsynth/pull/1718#issuecomment-6544812349) | 聚焦 mixer 的 C++11 lock-less port 有吸引力；说明并行机制的短文档／图可能有帮助 | 不是排斥 C++、内部重构或说明文档；只有确实降低理解成本才加，不强制每个 PR 写设计稿 |
| [derselbst：Linux CI #1870](https://github.com/FluidSynth/fluidsynth/pull/1870#discussion_r4315551462) | 不愿只为装包依赖第三方 community action；更喜欢 apt-get install | 优先项目已有能力，依赖增量要有足够收益；不是所有 action 一律禁止 |
| [derselbst：迁移 #1869](https://github.com/FluidSynth/fluidsynth/discussions/1869#discussioncomment-19041654) | 支持逐平台迁移，不用 *-latest images | 固定 OS 版本；#1874 review 另建议 Android 原生 ARM runner 测试，但用户暂缓依旧有效 |
| [derselbst：SF3 #1856](https://github.com/FluidSynth/fluidsynth/issues/1856#issuecomment-6298139040) | SF3 用户可能内存受限，不希望 runtime sample memory 翻倍；支持小型 aligned float buffer | 优先低内存、无全流 peak scan 的局部转换；用实际约束比较方案 |
| [derselbst：SF3 #1877](https://github.com/FluidSynth/fluidsynth/pull/1877#pullrequestreview-5658663120) | 肯定复用 default loader callbacks；另要求 FLUID_N_ELEMENTS、FLUID_MEMCPY、restrict 与浮点布局检查 | 复用既有设施、解释不变量；#1884 表明还要照顾 x87，不能只验证常见浮点目标 |
| [derselbst：回归音频 #1863](https://github.com/FluidSynth/fluidsynth/pull/1863#issuecomment-6247911952) | 亲自重听 regression 差异后接受修复造成的变化 | 音频修复不是绝对 bit-identical 门槛，要解释和验证差异；不是可以忽略 CI |
| [derselbst：插值 #1867](https://github.com/FluidSynth/fluidsynth/pull/1867#pullrequestreview-5574660115) | “keep off issue 1826 for the moment”；维护者想自己做 | 尊重明确工作归属。2026-11-09 #1894 仍由 derselbst 开放推进，先核对而不是抢做 |
| [derselbst：RMIDI #1889](https://github.com/FluidSynth/fluidsynth/pull/1889#discussion_r4427594958) | 评审中又改 1,100 行，使其必须从头开始 | 评审中控制增量、交代变更，不持续大面积重写 |
| [贡献规范](https://github.com/FluidSynth/fluidsynth/blob/master/CONTRIBUTING.md)、[安全披露反馈 #1897](https://github.com/FluidSynth/fluidsynth/issues/1897#issuecomment-6581824492) | 要动机、可复现问题、诚实披露 AI／最好精确模型；反对未报告便公开 CVE，不会被迫立刻发版 | 人工复核与负责披露是贡献条件；新漏洞按 SECURITY.md 私下报告 |

公开往返也包括这些正向例子：#1856 中我们先比较方案、询问约束再实现；#1879 按评审补齐 CC120 同类入口；#1884 配合维护者分支集成；#1889 给出三个具体简化点，而不是要求整块重写。#1866 的跨轨 merge 建议技术上得到认可，但用户后来决定暂缓，技术成立不等于当前应推进。

## 用户已明确决定（fork 历史记录）

来源：[精简前 README 的固定快照](https://github.com/Yikai-Liao/fluidsynth/blob/490c2114d6bf2f53ed53cd2eef3d300f32b40ea2/README.md)。这是既有用户决定的记录，不是仅从合并历史猜测。

- 2026-10-26：优先音频正确性；播放器控制、排序／seek、heap merge、F35 暂缓；sinc 由维护者处理。
- 2026-10-27：不为凑三个问题或 PR 数量推进边际修复；看真实条件、后果、收益和审核成本。
- 2026-10-28：重复 CC66 因真实 MIDI＋MS Basic 证据提高至下一批优先考虑，但不自动授权开 PR。
- 2026-10-30：新增索引／位图／缓存需普通 MIDI 显著、可重复的端到端收益；同属 CC66 的独立缺陷分别保存和投稿。
- 保留 dev 历史，review 完成才整理干净 PR 分支；测试也要最小、贴合既有结构；正文简短、如实披露 AI；DLS 后放，Android 等用户 review，Windows CI 以后再说。
- 2026-11-09 本轮追加：todo 文档精简，不堆分析报告和会话流水。当前队列只写根 README，历史细节用固定提交回查。

## 全部 21 个用户 PR

截至核对日全部已结束；下表只陈述提交／合并结果，不代表每个历史 head CI 全绿，也不代表已进入同一 release。

| PR | 内容与结果 | 对后续贡献的启示 |
|---|---|---|
| [#1863](https://github.com/FluidSynth/fluidsynth/pull/1863) | voice 复用 onset gain；merged | 复用已有初始化机制，最小 MIDI 与 PCM 对照；修复可能改变 rendering baseline |
| [#1864](https://github.com/FluidSynth/fluidsynth/pull/1864) | Dattorro tap indexing；merged | 局部小优化可接受；micro 67.3% 不写成全渲染收益 |
| [#1867](https://github.com/FluidSynth/fluidsynth/pull/1867) | 慢 cutoff ramp 系数复用；merged | 披露 fast-math 差异、过滤规则和原始数据；不是后来 W4 展开 |
| [#1868](https://github.com/FluidSynth/fluidsynth/pull/1868) | MIDI realtime parser 状态；merged | 保住 FF reset 例外，针对真正协议边界测试 |
| [#1870](https://github.com/FluidSynth/fluidsynth/pull/1870) | Linux CI 安装／checkout；merged | 按评审撤第三方 apt cache，撤掉不适用于最终版本的旧计时 |
| [#1872](https://github.com/FluidSynth/fluidsynth/pull/1872) | Dattorro 4 ms predelay；merged | 一行单位错误能独立贡献，不必绑重构 |
| [#1873](https://github.com/FluidSynth/fluidsynth/pull/1873) | 固定 runner OS；merged | 从讨论中已同意的最小动作开始 |
| [#1874](https://github.com/FluidSynth/fluidsynth/pull/1874) | Ubuntu 24.04／native ARM；merged | 共用矩阵、保留真实覆盖，提供 fork CI |
| [#1875](https://github.com/FluidSynth/fluidsynth/pull/1875) | Alpine CI 迁移；merged | 一平台一 PR，移除已替换配置 |
| [#1876](https://github.com/FluidSynth/fluidsynth/pull/1876) | macOS CI 迁移；merged | 完整平台主题可以较大；不能把约 500 行视为硬禁令 |
| [#1877](https://github.com/FluidSynth/fluidsynth/pull/1877) | SF3 S16 gain；merged | 先商量性能／内存约束，测真实 full-load，复用 loader 回调 |
| [#1878](https://github.com/FluidSynth/fluidsynth/pull/1878) | silent-loop phase；closed/unmerged | 主动让位已有且覆盖更全的 #1871，不重提 |
| [#1879](https://github.com/FluidSynth/fluidsynth/pull/1879) | Mono All Notes/Sounds Off 清键；merged | 评审提醒后补 CC120，覆盖单／全通道及不受影响通道 |
| [#1880](https://github.com/FluidSynth/fluidsynth/pull/1880) | sostenuto→sustain 转交；merged | 单一踏板问题独立修复，保护两种释放顺序与 Mono |
| [#1881](https://github.com/FluidSynth/fluidsynth/pull/1881) | 拒绝零 tempo；merged | 在解析入口拒坏值，不绑整个 player 改造 |
| [#1884](https://github.com/FluidSynth/fluidsynth/pull/1884) | x87 scalar fallback；merged 至 fix-x87 | 最终经 #1885 进入 master；跨平台修复可配合维护者分支 |
| [#1886](https://github.com/FluidSynth/fluidsynth/pull/1886) | INVALID_NOTE signed-char；merged | 生产一行；局部 Note On 提速、默认完整渲染无明确收益，两者都如实保留 |
| [#1895](https://github.com/FluidSynth/fluidsynth/pull/1895) | worker 首任务竞态；merged | 从性能工作先拆 correctness；维护者把测试命名改为 rvoice_mixer_worker_startup |
| [#1900](https://github.com/FluidSynth/fluidsynth/pull/1900) | SF2 mod destination 收窄前检查；merged 11-06 | 文件边界保留原始信息；维护者另补 linked-mod 检查 |
| [#1901](https://github.com/FluidSynth/fluidsynth/pull/1901) | voice 接纳 modulator 时检查；merged 11-06 | 核心边界覆盖多入口；与 #1900 互补，不以 void setter 拒值 |
| [#1902](https://github.com/FluidSynth/fluidsynth/pull/1902) | mixer std::atomic；merged 11-06 | 可解释的两步迁移；承认 OSAL_posix 更快、稀疏负载噪声和并行求和差异 |

由此可推断（不是对方明说的硬规则）：双方最成功的贡献形态是“真实问题 → 聚焦实现 → 小型回归 → 能追溯的证据”，并非“只能修一行”或“只接受 bug、不接受优化”。20/21 已合并说明此前合作有效，不能据此推导下一个 PR 必然会合并。
