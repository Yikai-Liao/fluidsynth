# DLS 最小职责重构：阶段记录与后续计划

本次先保存并推送现阶段代码，不继续扩展实现或验证。范围仅限 DLS 及其必需的共享 voice 接口。

## 当前保存的内容

- 保留统一的 `read / limit / fseek` 范围检查，以及失败后作废逻辑游标的处理。
- 所有 DLS voice 始终安装 DLS resolver，使首次选区和单音连奏复用采用相同的选区规则；原始 MIDI 键仍由 voice 保存。
- INFO 复用 `visit_subchunks`，保留找到首个 INAM 后停止的行为。
- WSMP 在内部检查整个 payload 已消费，调用者不再比较返回长度；保留 Crystal 小 cbSize 和合法扩展字段的兼容处理。
- minimal 候选补入默认调制器稳定槽引用，以及共享 voice 采样地址安全转换。SF2 loader 的一行适配是共享字段类型变化的必要配套，没有扩展 SF2 重构。
- 新增回归覆盖普通 DLS 调音跨区连奏、默认调制器替换与清除、极端 region loop，以及 libsndfile 回调读写位置失败。
- 解码回调暂以 `exception_ptr` 保存读取／seek 异常，在解码操作返回 C++ 边界后传播；关闭资源使用现有 scope guard。未知逻辑位置不能执行相对 seek。

这些是阶段性改动，不代表整个 DLS 职责审查已经完成。此前的 `dls-boundaries-review.md` 是本轮前的记录，其统计和“候选不含默认调制器所有权修复”等描述不再代表当前候选。

## 已取得的证据

修复前的 minimal 候选重现了三项问题：

1. 普通 DLS 将键 61 调到区域 62 后，mono multi-retrigger 从 60 切到 61，held voice 从 1 变成 2。
2. 极端 region WSMP 循环地址在共享 voice 中触发 float-to-int 越界转换。
3. 清除 SoundFont 默认调制器后，下一次 DLS note-on 读取已释放链表，ASan 报 use-after-free。

独立检查还发现：在 μ-law wave 的数据读取中持续注入 callback 失败，原候选仅报告提前 EOF，仍返回加载成功。这证明错误被吞掉，但其产品行为应结合仓库惯例评价，不能仅凭“存在短读”认定一切降级都是 bug。

截至本次暂停，minimal 候选的 ASan、UBSan、float-cast-overflow 定向检查已通过 5/5：三项 DLS 测试、`test_default_mod`、`test_modulator`。本轮没有重新完成全套测试；不把此前全套结果记作当前候选的验证。额外无 libsndfile 构建在用户要求先推送后停止，不作为本轮验收证据。

## 读取失败策略：现状和待确认点

仓库不是所有失败都采用同一种策略：

- SF2 主要采样数据读取失败：记录 error，完整加载失败。
- SF3 解码不足预期帧数：解码失败；完整加载路径继续传播失败。
- 动态加载单个 sample 失败：记录 error，并禁用该 sample，其他声音可以继续。
- SF2 额外 24-bit 数据失败：已有完整 16-bit 数据可用，因此允许降级并给出诊断。
- DLS 原有解码短读：warning 后继续。

当前候选只将**明确的底层 callback 失败**传播为加载失败；**没有 callback 失败的解码短读**仍保留原有 warning 策略。callback 返回 0 表示未读到字节，不等于已经明确选择了“生成静音”。

后续需要复核这个区分是否是本次应交付的兼容边界。若选择“禁用失败 sample 并警告”，应由 wave 加载／发布操作统一完成，明确清理已追加的 PCM、sample 长度和引用该 sample 的 region；不能仅靠预分配缓冲中的零值形成隐式降级。不要在这一轮未经论证地把全部短读统一改成加载失败。

## 后续计划

### 1. 重新验证职责划分，而非逐条照搬原 review

沿三条完整操作检查：

- chunk 遍历、叶解析、失败与恢复：遍历器承担父子范围和 padding；叶解析器承担自身格式及完整消费。核对是否仍依赖隐藏游标状态或外部收尾。
- wave 解码、PCM 累积、sample 发布：谁决定失败、短读降级和是否发布；回调适配不得自行把失败解释成成功。
- DLS 选区、voice 创建与复用、释放：DLS resolver 拥有选区／播放键转换，voice 拥有 MIDI 身份与生命周期，SoundFont 拥有当前默认调制器。

只有能说明“删掉了什么同步义务、保证由谁承接”的调整才继续实施。INFO 与 WSMP 的局部整理不能代替完整操作的验收。

### 2. 收尾错误传播与兼容性判断

- 复核 libsndfile 首次定位、打开、读取、短读、关闭这条链路，确保异常不穿过 C callback，资源清理和失败传播完整。
- 区分读取失败、正常 EOF、损坏压缩流和合法空 sample，分别决定拒绝、禁用或兼容；保留原有行为时明确原因。
- 确认是否保留 `exception_ptr`；仅在更简单的表示仍能保留必要错误语义时替换，不引入通用错误框架。
- 默认调制器列表创建位于 `noexcept` loader 入口，内存分配失败可能抛异常；这是源码检查候选，尚未做故障注入或修复。先验证再决定是否纳入。

### 3. 整理为便于 review 的提交范围

- 以 `refactor/dev-dls-boundaries-minimal` 为上游干净基线上的候选，避免带入当前开发分支继承的其他审计修复。
- 分清读取职责收敛、DLS voice 行为修复、必要共享边界修复、回归测试及证据。
- 对共享文件逐项解释 DLS 调用链中的必要性；不扩展 MIDI、其他音频后端或无关 SF2 整理。
- 更新旧 review 文档的统计及结论，再进行最终审查；当前先保留阶段记录，不宣称完成验收。

### 4. 用户要求继续后再开展验证

- 完成当前候选的有／无 libsndfile 配置验证。
- 用回归测试对照修复前版本，确认每项测试确实能检测对应行为差异。
- 对受共享字段及地址转换影响的现有测试做必要检查，随后再决定是否运行全套。
- 尚无性能基准；resolver 调用增加发生在分配／换音路径，不能宣称零开销。

## 2026-10-26 本地继续执行记录（未提交、未推送）

基于精确提交 `cb7b8ce67ed16f071b28e5b46ef7dc1f744c7af0`，以下是本轮
实际证据，不覆盖上文历史记录，也不把历史 sanitizer 结果记成本轮结果。

### 计划 1：职责链复核

已复核 chunk 遍历/叶解析、解码/PCM 累积/发布、resolver/voice/默认调制器
三条链。遍历器仍负责父子范围、padding 与推进；严格叶格式（如 WSMP）负责自身格式和
完整消费；未知 chunk/允许扩展仍由遍历器跳过。sample 数据最终发布在 PCM
累积完成之后；voice 持有 sample 引用，SoundFont 删除依据 sample.refcount
延迟释放，从而守住 resolver context 的存活期。本轮没有足够依据再引入新抽象或
重复实现已经存在的 resolver、INFO、WSMP、默认槽位及地址转换修复。
独立逐项审查矩阵随本轮交付保存；这不意味着所有异常路径已动态覆盖。

### 计划 2：错误传播与兼容边界

已完成源码复核：libsndfile callback 内保存异常，在 C++ 边界传播；首次
定位错误不会被当成功；close guard 先于底层状态析构。只读 close 返回值
的处理与共享 SF2 实践一致。保留 `exception_ptr` 和现有“无明确 callback
失败的短读给 warning”策略，没有擅自更改为一律拒绝或静音。

新增唯一产品改动：在 `noexcept` loader 创建默认调制器列表的位置增加
局部 `bad_alloc` 捕获，记录错误、释放尚未发布的 SoundFont、返回空。
helper 抛出前已清临时链表，失败赋值不会发生，SoundFont 的 list 仍为零
初始化的空值。独立源审通过；本轮不做分配故障注入，因此不声称 catch
分支已有运行覆盖。损坏压缩流、底层故障和历史内存错误复现没有重新运行。
另用合法零帧 PCM 验证现有兼容行为：font 可加载，空 region 不产生 voice，
输出为零，相邻非空 region 保持有效；运行保留现有 sample 校验 warning。
这不是对所有空压缩格式或损坏流作统一策略承诺。

### 计划 3：review 范围

产品代码仅 `src/sfloader/fluid_dls.cpp` +10/-1；本记录是额外文档更新。
没有混入开发分支 `77ee7d54`，没有新增 SF2/MIDI/后端重构。共享文件本轮
零修改；已有必要共享接口的正常行为已选择测试。没有提交、推送或 PR。
旧 review 中的历史统计没有冒充当前结论，以本记录及独立矩阵为准。

### 计划 4：实际验证与剩余项

- 有/无 libsndfile：两种配置均构建成功，生成 config.h 已核实际启闭。
- 两配置均通过：`test_dls_sample_storage`、`test_modulator`、
  `test_sfont_loading`、`test_voice_callback`、`test_voice_reuse_gain`，各5个
  已审选正常目标。它们覆盖合法 PCM、DLS 调音跨区、默认调制器替换/清除、
  SF2 加载/重载/移除重加及 voice 生命周期和复用音频一致。
- 有 libsndfile 另通过正常 μ-law DLS：3种长度，独立 G.711 整数解码
  oracle 检查全部 sample、span、可听且有限输出，以及正常默认 mod/legato。
- 合法 INFO/WSMP 兼容：首 INAM（前有 odd IART padding，后有另一个 INAM）
  强断言保持 First；WSMP cbSize=24 合法扩展仍维持全部 sample/audio 结果。
  μ-law ON、PCM ON/OFF 三种正常变体均通过；PCM ON/OFF 也通过上述空样本控制。
- 没有跑完整混合 DLS parser/allocation suite，没有跑修复前危险复现、
  故障注入、ASan/UBSan；历史红绿和 sanitizer 仅为历史证据，未重新核实。
- 未开展 resolver 性能测量，不宣称零开销。

因此本轮已补最小实现和正常关键路径验证，但不把整份计划或所有安全性
验收标成已完成。剩余项包括空压缩格式策略的推广判断、未复核的历史异常红绿、
完整测试/跨平台覆盖和性能测量；应分别作后续决策而非隐去限制。

新增正常兼容测试**未纳入仓库默认 CTest/CI**：完整可复跑源码
`normal_mulaw_smoke.cpp`、`run-normal-variants.sh`、配套编译命令及结果日志
保存在随本轮交付的 `fluidsynth-dls-minimal-maintenance-20261026.zip` 附件。
它们不是本仓库中的相对文件路径；仓库现有5个白名单目标保持原样。
