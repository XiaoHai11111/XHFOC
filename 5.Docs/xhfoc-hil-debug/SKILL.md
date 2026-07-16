---
name: xhfoc-hil-debug
description: 维护并使用 XHFOC 项目的固件、串口采集、VOFA 数据解析、AI 辅助硬件在环调试和版本提交流程。当 Codex 需要浏览本项目目录、检查 XHFOC_STM32G4_FW、配置或运行 FOC-Serial-Analyzer、解析 UART 或 VOFA 数据、编写版本提交信息，或在用户明确授权后提交并推送项目改动时使用本技能。
---

# XHFOC 硬件在环调试

以仓库当前源码为准。修改串口默认值或解析数据前，重新检查本文引用的固件文件；本技能随正在开发的固件持续维护。

## 新建 Agent 推荐提示词

新建 Agent 或新任务时，复制下面的提示词并替换方括号内容：

```text
请使用 $xhfoc-hil-debug 处理 XHFOC 项目任务，并首先完整读取：
E:\Projects\XHFOC\5.Docs\xhfoc-hil-debug\SKILL.md

工作区：E:\Projects\XHFOC
本次任务：[填写需要分析、诊断、修改或验证的目标]
采集会话：[填写 captures 下的会话目录；没有则写“无”]
实验条件：[填写固件版本、母线电压、电机与负载、控制模式、命令顺序；未知项明确写“未知”]
预期现象：[填写正常结果或需要重点检查的问题]
版本与 Git 操作：[填写“不涉及”“仅生成提交信息”“提交”或“提交并推送”；涉及版本时填写版本号]

执行要求：
1. 先检查仓库状态和当前源码，以源码、配置和采集元数据为准，不假设 SKILL 中易变化的参数仍然有效。
2. 保留用户现有及无关改动；未经要求不要提交、重置或删除文件。
3. 分析串口数据时先确认 metadata.json 的 status 为 complete；必要时运行 decode_capture.py，再结合 parse_summary.json、vofa.csv、ascii.log 和原始字节给出证据。
4. 修改代码时完成与风险相称的测试；区分已验证结论、合理推断和仍需硬件确认的事项。
5. 如果目录、串口参数、VOFA 帧格式、通道顺序或操作流程发生变化，同步更新本 SKILL 和相关配置。
6. 生成提交信息前检查近期提交格式、git status、实际 diff 和验证结果，使用本技能中的版本提交模板，不把未执行的测试写成已通过。
7. 只有在我明确要求提交或推送时才执行相应 Git 操作；推送前检查暂存内容、当前分支和远端，禁止 force push。

最终请用中文说明：结论、证据、修改文件、验证结果、遗留风险和建议的下一步。
```

## 定位项目内容

- 在 `0.References/` 中查找参考代码。
- 在 `1.Model/` 中存放项目三维模型；该目录当前为空。
- 在 `2.Hardware/` 中存放 PCB 硬件工程；PCB 已完成设计，但工程文件当前尚未放入此目录。
- 将 `3.Firmware/XHFOC_STM32G4_FW/` 作为当前主要开发固件。
- 在 `4.Software/` 中维护上位机工具；串口采集与解析工具位于 `4.Software/FOC-Serial-Analyzer/`。
- 将 `5.Docs/SCH_axdr_v1.3_2026-07-16.pdf` 作为当前电路板原理图。
- 当以上目录或工作流改变时，同步维护 `5.Docs/xhfoc-hil-debug/` 中的本技能。

修改前检查仓库状态，保留用户的无关改动，不要把生成的采集数据当作源代码。

## 配置串口采集器

编辑 `4.Software/FOC-Serial-Analyzer/config.json`，保存长期使用的配置：

- 填写 `experiment.name`、`firmware_revision` 和 `notes`，让后续 AI 分析能够识别实验条件。
- 当前设备使用 `serial.port = "COM8"`。仅在确实需要枚举端口时改为 `auto`；必要时用十进制 `vid` 和 `pid` 限制自动选择范围。
- 当前固件默认使用 `115200/8N1`、无流控。固件改变前保持这些参数一致。
- 将 `capture.duration_s` 设为任意正数以定时采集；设为 `0` 表示持续采集，直到用户停止程序。
- 默认使用相对于配置文件的 `capture.output_dir`，除非明确需要绝对路径。
- 使用 `serial.connect_timeout_s = 0` 表示初次连接时无限重试；设为正数表示超时秒数。
- 保持 `serial.auto_reconnect = true`，让临时断线后继续写入同一采集会话。

Windows 下直接双击 `run_logger.bat`。启动器即使在 CMD 的 `PATH` 中找不到 Python，也会自动定位已安装的 Python，然后加载 `config.json` 并开始采集，不需要联网下载依赖。安装了 pyserial 时优先使用 pyserial，否则自动使用内置 Win32 串口后端。定时采集正常结束后窗口自动关闭；发生错误时窗口保持打开以显示原因。

在其他操作系统中手动或由 AI 通过命令行运行时，先安装 pyserial：

```powershell
python -m pip install -r requirements.txt
```

只校验配置而不打开串口：

```powershell
python serial_logger.py --check-config
```

## 采集串口数据

在 `4.Software/FOC-Serial-Analyzer/` 中运行：

```powershell
python serial_logger.py
```

程序自动读取 `config.json`，连接成功后立即写入数据，不要求用户交互。用户要求双击启动时优先使用 `run_logger.bat`。

一次性 HIL 任务可通过命令行覆盖配置，不必修改共享配置文件：

```powershell
python serial_logger.py --port COM8 --duration 30 --output-dir captures
```

持续采集时使用 `Ctrl+C` 或 `Ctrl+Break` 停止。关闭 Windows 控制台也会请求收尾，但优先使用键盘停止；强制结束进程或断电无法保证最终元数据完整。正常停止时，等待程序明确报告串口已关闭且文件已保存。

每次成功采集会创建 `captures/<prefix>_<timestamp>/`，其中包含：

- `serial_raw.bin`：收到的原始字节，作为分析的原始证据。
- `chunks.jsonl`：每次操作系统读取的接收时间、单调时钟耗时、字节偏移和长度。不要把读取块边界当作协议帧边界。
- `metadata.json`：实验信息、实际串口与采集配置、重连记录、错误、字节统计和停止原因。只有 `status` 为 `complete` 时才表示正常收尾。

先读取 `metadata.json`，再用 `chunks.jsonl` 将时间范围映射到字节偏移，最后只解析 `serial_raw.bin` 中需要的部分。不要原地修改原始采集文件；将 CSV、图表和报告写入独立的派生目录。

## 解析采集数据

采集器正常停止后，使用 `4.Software/FOC-Serial-Analyzer/decode_capture.py`。可传入会话目录或其中的 `serial_raw.bin`；省略参数时自动选择最新的完整会话：

```powershell
python decode_capture.py captures/xhfoc_YYYYMMDD_HHMMSS_microseconds
python decode_capture.py
```

交互使用时直接双击 `run_decoder.bat`，解析最新完整会话。也可将采集会话目录或 `serial_raw.bin` 拖到 BAT 文件上，启动器会把路径传给解析器。解析完成后窗口保持打开，按任意键关闭。

解析器从 `decoder_config.json` 读取通道顺序和帧格式，并在会话目录下生成 `decoded/`：

- `vofa.csv`：每个有效 JustFloat 帧一行，包含帧序号、接收块时间、原始字节偏移和 16 个命名通道。
- `ascii.log`：从二进制帧之外提取的高可信固件标签日志和已知 ASCII 命令响应。
- `parse_summary.json`：源文件完整性、协议格式、有效与拒绝帧数、非帧字节、非有限值和估算接收帧率。

将 CSV 时间视为“操作系统读取块的接收时间”，不要视为 MCU 精确采样时间。同一次系统读取中的多个帧可能共用相同时间戳。需要精确控制周期时，在固件中增加 MCU 序号或时间戳通道。

不要在 `metadata.json` 状态为 `capturing` 时正常解析。先停止采集，确保原始文件大小和块索引稳定。仅在明确需要临时快照时使用 `--allow-incomplete`。

在本仓库中交给 AI 分析时，提供采集会话目录路径、实验问题和预期行为即可。需要向外部上传时，至少提供 `metadata.json`、`decoded/parse_summary.json` 和 `decoded/vofa.csv`；需要审计协议完整性、丢帧、截断帧、重连或 ASCII 事件时，再提供 `serial_raw.bin` 和 `chunks.jsonl`。同时说明固件版本、电机与负载条件、命令顺序和故障大致时间。采集前尽量填写 `config.json` 中的 `experiment` 字段。

## 编写版本提交信息并推送

生成版本提交信息前，先查看近期提交正文、`git status --short`、实际 diff、未跟踪文件和验证结果。提交标题沿用以下格式：版本号单独一行，空一行后写 Conventional Commit 标题。正文只描述本次提交真实包含的内容。

使用下面的中文模板；删除不适用的条目，不保留空占位符：

```text
<版本号，例如 v1.5.0>

<type>(<scope>): <一句话概括本次主要改动>

[背景]
- 问题/需求: <为什么需要本次改动>
- 触发条件: <在什么测试、故障或开发阶段触发>

[改动]
- 修改内容:
  - <核心改动一>
  - <核心改动二>
- 修改文件:
  - <关键文件或目录>
- 关键点:
  - <重要实现约束、协议或行为变化>

[影响评估]
- 硬件影响: <无或具体影响>
- 实时性影响: <控制周期、中断、通信或阻塞变化>
- 资源影响: <Flash、RAM 或工具依赖变化>
- 兼容性影响: <启动方式、协议、配置或数据格式变化>

[验证]
- 构建:
  - [x] <实际执行且通过的构建命令>
- 自动测试:
  - [x] <实际执行且通过的测试命令和结果>
- 板测:
  - [x] <已经完成并有证据的板测>
  - [ ] <尚未完成的验证>

[已知问题]
- <本次未解决的问题、硬件限制或后续风险>

[回滚]
- git revert <this_commit_hash>
- 重点回滚文件:
  - <关键文件或目录>
```

遵守以下提交规则：

1. 根据主要目的选择 `feat`、`fix`、`refactor`、`perf`、`docs` 或 `test`；scope 使用 `foc`、`encoder`、`comm`、`telemetry`、`hil` 等实际模块。
2. 只把当前 diff 中存在的内容写入“改动”；不要把分析结论误写为已经完成的代码修复。
3. 只有实际运行成功的项目才能标记 `[x]`；未验证的板测保留 `[ ]`。
4. 明确写出启动行为、串口协议、配置文件或数据格式的不兼容变化。
5. 不提交 `captures/` 中的实际采集会话、构建产物、缓存和临时文件；提交前检查忽略规则是否生效。
6. 用户明确要求提交时，先检查 staged diff，再使用完整模板提交。用户同时明确要求推送时，仅正常推送当前分支到已确认远端，禁止使用 `--force`。
7. 推送完成后检查本地分支与上游同步状态，并报告提交哈希、分支、远端和仍未提交的文件。

## 解释当前固件数据流

分析前检查以下源码：

- `3.Firmware/XHFOC_STM32G4_FW/Core/Src/usart.c` 定义 USART3 参数。当前为 `115200/8N1`、无硬件流控、PB10 TX、PB11 RX。
- `3.Firmware/XHFOC_STM32G4_FW/UserApp/main.cpp` 定义 VOFA 任务和通道顺序。
- `3.Firmware/XHFOC_STM32G4_FW/Platform/Vofa/vofa_debug.cpp` 定义 JustFloat 序列化和帧尾。
- `3.Firmware/XHFOC_STM32G4_FW/Platform/Communication/` 实现 UART 传输和 ASCII 命令处理。

当前 UART 数据流可能同时包含 ASCII 状态或命令响应，以及 VOFA JustFloat 二进制帧。不要把整个文件直接解码为文本，也不要把整个文件直接重排为浮点数组。

当前每个 VOFA 帧包含 16 个小端序 `float32`，随后是帧尾 `00 00 80 7F`，合计 68 字节。通道顺序为：

1. `dutyA`
2. `dutyB`
3. `dutyC`
4. `iA`
5. `iB`
6. `iC`
7. `iAlpha`
8. `iBeta`
9. `iq`
10. `id`
11. `position`
12. `velocity`
13. `target`
14. `adcRawIa`
15. `adcRawIb`
16. `adcRawIc`

围绕帧尾重新同步，并检查候选帧长度和数值合理性。将有效帧之间的 ASCII 字节作为独立日志流处理。在分析报告中标记数据间隙、重连边界、截断帧、NaN 或 Inf，以及元数据错误。

## 维护与验证

固件 UART 参数、VOFA 通道数量或顺序、发送周期、帧格式发生变化时：

1. 物理串口参数改变时更新 `config.json` 默认值。
2. 同时更新 `decoder_config.json` 和本技能的“解释当前固件数据流”部分。
3. 明确说明原始采集兼容性，不要静默地用新格式重新解释旧会话。
4. 运行采集与解析测试：

```powershell
python -m unittest discover -s tests -v
```

5. 使用本地 `skill-creator` 的 `quick_validate.py` 校验本技能。

进行硬件冒烟测试时，使用 `--duration 3` 等短时覆盖参数，确认程序正常退出且 `bytes_written` 非零，检查 `serial_raw.bin` 大小与元数据一致，并确认测试后可以再次打开串口。
