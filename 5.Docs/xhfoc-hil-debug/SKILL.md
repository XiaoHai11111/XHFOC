---
name: xhfoc-hil-debug
description: 维护并使用 XHFOC 项目的 CLion/STM32 开发环境、固件、USB Native/Fibre JSON 对象控制、串口采集、VOFA 数据解析、AI 辅助硬件在环调试和版本提交流程。当 Codex 需要浏览本项目目录、检查或复现 CLion、CMake、STM32CubeCLT、编译、烧录与调试配置，维护 XHFOC_STM32G4_FW，通过 USB 设置电流/速度/多圈位置目标，运行 FOC-Serial-Analyzer，解析 UART 或 VOFA 数据，编写版本提交信息，或在用户明确授权后提交并推送项目改动时使用本技能。
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
开发环境：[默认写“沿用当前 CLion/STM32CubeCLT 配置”；工具链、探针或路径有变化时明确填写]
版本与 Git 操作：[填写“不涉及”“仅生成提交信息”“提交”或“提交并推送”；涉及版本时填写版本号]

执行要求：
1. 先检查仓库状态和当前源码，以源码、配置和采集元数据为准，不假设 SKILL 中易变化的参数仍然有效。
2. 保留用户现有及无关改动；未经要求不要提交、重置或删除文件。
3. 分析串口数据时先确认 metadata.json 的 status 为 complete；必要时运行 decode_capture.py，再结合 parse_summary.json、vofa.csv、ascii.log 和原始字节给出证据。
4. 修改代码时完成与风险相称的测试；区分已验证结论、合理推断和仍需硬件确认的事项。
5. 如果目录、串口参数、USB Native/Fibre JSON 对象接口、VOFA 帧格式、通道顺序或操作流程发生变化，同步更新本 SKILL 和相关配置。
6. 生成提交信息前检查近期提交格式、git status、实际 diff 和验证结果，使用本技能中的版本提交模板，不把未执行的测试写成已通过。
7. 只有在我明确要求提交或推送时才执行相应 Git 操作；推送前检查暂存内容、当前分支和远端，禁止 force push。
8. 处理构建或调试问题时先检查 CMakePresets.json、工具链文件、.ioc 和实际工具版本；不要把被忽略的 .idea、CMakeCache.txt 或本机绝对路径当作跨机器唯一事实。
9. 只有在我明确要求烧录或复位硬件时才操作 ST-LINK；执行前说明将使用的构建类型、ELF 路径和当前未提交控制参数，执行后报告芯片识别、下载校验和复位结果。
10. 进行闭环调参时，每次复位后先等待校准完成，再发送 `!START`；采集结束先发送 `!STOP`，随后用 ST-LINK 复位。每轮只改变有明确假设的一组参数，并用长时复测排除粘滑和积分累积造成的假收敛。
11. 需要切换控制模式或目标时优先使用 USB Native/Fibre 对象：电流、速度和位置分别调用 `motor.set_current()`、`motor.set_velocity()`、`motor.set_position()`；位置目标使用绝对多圈机械角（rad），禁止擅自归一化为 `[-π, π]`。

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

## 使用 CLion 开发 STM32 固件

当前开发机配置如下。版本、安装路径和本地 CLion 配置可能变化，处理环境问题时先重新读取实际配置，再同步维护本节。

### 当前 IDE 与工具链

- IDE：CLion `2025.3.3`，当前可执行文件为 `D:\Program Files (x86)\CLion 2025.3.3\bin\clion64.exe`。
- CLion 工具链名称：`STM32`，类型为 Windows System Toolset。
- STM32CubeCLT：`D:\Program Files (x86)\STM32CubeCLT_1.21.0`。
- CMake：STM32CubeCLT 自带 `3.28.1`。
- Ninja：STM32CubeCLT 自带 `1.11.1`。
- GNU Arm 编译器：`arm-none-eabi-gcc 14.3.1`，发行标识为 `GNU Tools for STM32 14.3.rel1.20251027-0700`。
- CLion 调试器：JetBrains 随 IDE 提供的 GDB `16.3`；终端中的 `arm-none-eabi-gdb` 来自 STM32CubeCLT，版本为 `15.2.90.20241229-git`。
- STM32CubeMX：`D:\Program Files (x86)\STM32CubeMX\STM32CubeMX.exe`，当前文件版本标识为 `>6.17.0-RC5`。
- OpenOCD：`D:\Program Files (x86)\xpack-openocd-0.12.0-7\bin\openocd.exe`，版本为 `xPack OpenOCD 0.12.0+dev-02228-ge5888bda3-dirty`；当前项目调试目标不使用它。

CLion 的本机工具链配置位于 `%APPDATA%\JetBrains\CLion2025.3\options\windows\toolchains.xml`，嵌入式工具路径位于同目录的 `embedded-support.xml`。不要把这些用户级文件提交到仓库，也不要在 Skill 中记录凭据、账号或其他无关个人配置。

### 当前工程与构建配置

- 固件工程根目录：`E:\Projects\XHFOC\3.Firmware\XHFOC_STM32G4_FW`。
- 目标 MCU：`STM32G431RBT6`；以 `XHFOC_STM32G4_FW.ioc`、`STM32G431XX_FLASH.ld` 和 `cmake/stm32cubemx/CMakeLists.txt` 共同确认。
- 工程使用 CMake、Ninja 和 `cmake/gcc-arm-none-eabi.cmake`；`cmake/starm-clang.cmake` 存在，但当前未启用。
- 语言标准为 C11 和 C++17，并生成 `compile_commands.json` 供 CLion 索引。
- CPU 选项为 Cortex-M4、FPv4-SP-D16、硬件浮点 ABI。
- Debug 使用 `-Og -g3`；Release 使用 `-Os -g0`。
- C++ 关闭 RTTI、异常和线程安全静态初始化；链接使用 `nano.specs`、垃圾段回收、map 文件和内存占用报告，并启用浮点 `printf/scanf`。
- 主要产物为 `XHFOC_STM32G4_FW.elf` 和对应 `.map`；当前 CMake 没有自动生成 `.hex` 或 `.bin` 的 post-build 步骤。

`CMakePresets.json` 是命令行和 AI 构建的首选事实源：

```powershell
cd E:\Projects\XHFOC\3.Firmware\XHFOC_STM32G4_FW
cmake --preset Debug
cmake --build --preset Debug -j 8
cmake --preset Release
cmake --build --preset Release -j 8
```

预设使用 Ninja，命令行输出目录为 `build/Debug` 和 `build/Release`。当前 CLion 本地启用了 `Debug-STM32` 与 `Release-STM32` 两个 Profile，分别传入 `--preset=Debug` 和 `--preset=Release`，并选中 `Release-STM32`；CLion 管理的本地输出目录为 `cmake-build-debug-stm32` 和 `cmake-build-release-stm32`。

不要混用不同输出目录中的 `CMakeCache.txt` 来判断当前配置。出现编译器或路径变化时，先让对应 Profile 重新配置；未经用户要求不要删除整个构建目录。`.idea/`、`build/` 和 `cmake-build-*-stm32/` 均为被忽略的本机状态，跨机器复现时以受版本控制的 `CMakePresets.json`、CMake 工具链文件、`.ioc` 和链接脚本为准。

### 当前烧录与调试配置

- CLion 调试服务器名称为 `ST-LINK`，使用 STM32CubeCLT 的 `ST-LINK_gdbserver.exe 7.13.0`。
- STM32CubeProgrammer 路径为 `D:\Program Files (x86)\STM32CubeCLT_1.21.0\STM32CubeProgrammer\bin`。
- 调试探针接口为 SWD，GDB Server 端口为 `61234`，启动等待为 `500 ms`。
- SWO 当前关闭；预留端口为 `61235`。
- 本机调试配置保存在被忽略的 `.idea/debugServers/ST_LINK.xml`，因此新电脑需要在 CLion 中重新建立同等配置。

处理“CLion 能编译但命令行不能编译”时，先检查终端中的 `cmake`、`ninja`、`arm-none-eabi-gcc` 和 `arm-none-eabi-gdb` 是否解析到 STM32CubeCLT 1.21.0。处理“能编译但不能调试”时，依次检查 ST-LINK 连接、SWD 接线、GDB Server 路径、STM32CubeProgrammer 路径、端口占用和目标芯片设置；不要把 OpenOCD 配置误认为当前实际调试链路。

### 由 AI 编译、烧录并复位

以下流程已于 2026-07-18 使用当前 CLion `Release-STM32` 构建目录、STM32CubeProgrammer `2.22.0` 和 ST-LINK V2 实际验证。CLion 交互调试继续使用 `ST-LINK_gdbserver.exe`；AI 或终端执行一次性下载时，使用同一 STM32CubeCLT 中的 `STM32_Programmer_CLI.exe`，不要同时启动两个占用探针的服务器。

烧录属于硬件状态修改。仅在用户明确要求后执行，并先完成以下检查：

1. 运行 `git status --short` 并检查实际 diff，明确烧录的是提交版本还是未提交工作区。
2. 检查控制模式、目标值、电流/电压限幅和上电使能逻辑；提示电机可能运动，并确认机械与供电条件安全。
3. 检查 `ST-LINK_gdbserver`、OpenOCD、GDB 或其他 STM32CubeProgrammer 实例是否占用探针；不要直接终止用户正在使用的调试会话。
4. 确认 ELF 来自当前源码和正确的 Debug/Release 目录，不要烧录旧时间戳产物。

使用 CLion 当前 `Release-STM32` 输出目录编译：

```powershell
cd E:\Projects\XHFOC\3.Firmware\XHFOC_STM32G4_FW
& 'D:\Program Files (x86)\STM32CubeCLT_1.21.0\CMake\bin\cmake.exe' `
  --build 'cmake-build-release-stm32' `
  --target XHFOC_STM32G4_FW `
  -j 8
```

编译后检查产物时间、大小和段占用：

```powershell
$elf = 'E:\Projects\XHFOC\3.Firmware\XHFOC_STM32G4_FW\cmake-build-release-stm32\XHFOC_STM32G4_FW.elf'
Get-Item -LiteralPath $elf | Select-Object FullName, Length, LastWriteTime
Get-FileHash -LiteralPath $elf -Algorithm SHA256
& 'D:\Program Files (x86)\STM32CubeCLT_1.21.0\GNU-tools-for-STM32\bin\arm-none-eabi-size.exe' $elf
```

只枚举 ST-LINK、不连接目标：

```powershell
$programmer = 'D:\Program Files (x86)\STM32CubeCLT_1.21.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
& $programmer -l st-link-only
```

通过 SWD 下载 ELF、逐字节校验、软件复位并运行：

```powershell
$programmer = 'D:\Program Files (x86)\STM32CubeCLT_1.21.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$elf = 'E:\Projects\XHFOC\3.Firmware\XHFOC_STM32G4_FW\cmake-build-release-stm32\XHFOC_STM32G4_FW.elf'
& $programmer `
  -c port=SWD mode=NORMAL reset=SWrst freq=4000 `
  -w $elf `
  -v `
  -rst `
  -run
```

确认命令退出码为 0，并同时看到以下关键信息后才能报告成功：

- 目标为 `STM32G43x/G44x`、Device ID 为 `0x468`、NVM 为 `128 KBytes`。
- `File download complete`。
- `Download verified successfully`。
- `Software reset is performed`。
- `Core run`。

不要把 ST-LINK 序列号写入 Skill、提交信息或公开报告。只需要复位时，保持同样的连接参数并执行 `-rst -run`，不要重复下载。Normal 模式连接失败时，先退出 CLion 调试会话并排查探针占用、供电与 SWD 接线；只有固件导致普通连接失败且 NRST 已接入时，才改用 `mode=UR reset=HWrst` 连接后烧录。不要为了恢复连接而默认全片擦除或修改 Option Bytes。

## 通过 USB Native/Fibre JSON 对象控制电机

接口由 `CmdCtrlMotor::MakeProtocolDefinitions()` 发布到 Fibre 对象树，设备的 JSON 描述符用于让上位机发现方法和参数，实际调用走 USB Native/ODrive 端点，不是向 CDC 虚拟串口发送 JSON 文本。USART3（当前 COM8）继续用于日志、ASCII 启停和 VOFA 数据采集。

在 `4.Software/CLI-Tool/` 中启动交互工具，设备默认显示为 `foc0`：

```powershell
python run_shell.py shell
```

连接后调用：

```python
foc0.motor.set_current(1.0)
foc0.motor.set_velocity(5.0)
foc0.motor.set_position(12.5663706)
```

- `set_current(target)`：切换到电流/转矩模式，目标为 q 轴电流，单位 A，绝对值不得超过 `focMotor.config.currentLimit`。
- `set_velocity(target)`：切换到速度模式，目标为机械角速度，单位 rad/s，绝对值不得超过 `focMotor.config.velocityLimit`。
- `set_position(target)`：切换到位置模式，目标为从本次上电编码器累计坐标原点计算的绝对多圈机械角，单位 rad。`4π` 表示正向两圈；位置误差直接使用 `target - estimateAngle`，不得折算为单圈最短路径。
- 三种设置命令将模式与目标作为同一事务更新。同一控制模式下重复设置目标时只原子更新目标，保持 PID、力矩和位置轨迹连续；只有实际切换控制模式时才停止并复位 PID，更新模式和目标后恢复原运行状态。电机原本停止时保持停止，等待 `!START`。
- 三个方法返回 `bool`：`true` 表示目标已接受，`false` 表示 NaN/Inf、超过限制、模式无效或运行恢复失败。原有 CDC `!START`、`!STOP`、`!DISABLE` 保持兼容。

实现位于 `Platform/CmdCtrlMotor/cmd_ctrl_motor.*`，对象树入口位于 `UserApp/protocols/cmd_protocol.cpp`，多圈位置反馈来自 `EncoderBase::GetFullAngle()`。修改接口时同步更新固件 README、本节和上位机调用方，并至少执行 Release 构建；涉及目标单位、模式切换或启停语义时必须进行受控硬件验证。

向 `MakeProtocolDefinitions()` 增加 Fibre 函数后，检查 `build/Release/**/*.su` 中 `MakeObjTree()` 的静态栈占用；对象树在 `commTask` 中构造，当前测得 816 B，因此该任务栈为 1536 B。不要只看链接器 RAM 百分比：若对象树构造踩栈，可能出现通信仍有响应但 FOC 未进入 ready 的假象。

复位后没有机械校准动作不一定是故障：只有启动日志出现 `record=valid align=flash` 时才表示正常复用了 Flash 对齐记录。若 `!START` 返回 `motor not ready`，在复位前先打开 USART3 日志并检查：

- `[err] create task focControlTask failed`：任务或 FreeRTOS 堆不足。
- `[foc] init failed`：PWM、编码器或 FOC 初始化失败。
- 有 `[foc] ...` 但没有 `[foc] ready`：初始化中断、阻塞或栈破坏。
- 出现 `[foc] ready` 后仍拒绝启动：检查 `CmdCtrlMotor::ready_` 的写入和内存破坏。

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

VOFA JustFloat 没有 CRC，ASCII 命令响应与二进制帧共用串口时，极少数错位候选可能产生不符合物理范围的浮点值。分析控制性能前统计并报告拒绝帧数；至少拒绝 NaN/Inf、角度或目标绝对值大于 `1000 rad`、速度绝对值大于 `1000 rad/s`、以及 `iA/iB/iC/iq/id` 任一绝对值大于 `20 A` 的帧。不要让单个明显伪帧决定峰值电流或稳定性结论；原始数据仍保留用于审计。

## 自动闭环调参与验收

用户明确授权修改、编译、烧录、控制电机和采集数据后，按以下顺序执行每一轮：

1. 记录当前 PID、目标、控制频率、限幅和 Git diff，只修改本轮假设涉及的参数。
2. 用 `Release-STM32` 构建并检查 RAM/FLASH 占用；下载 ELF、执行 verify、复位并确认 Core run。
3. 复位后先检查启动日志：`record=valid align=flash` 表示已复用 Flash 中的 FOC 编码器对齐结果，`record=empty/invalid align=fresh` 表示本次执行了方向与电角度零偏对齐并尝试保存。首次对齐时预留至少 5 秒；无论哪种路径，都必须等待 `[foc] ready` 后再发送 `!START`。采集器独占 COM8 期间定时发送启动和停止命令，例如：

```powershell
python serial_logger.py --duration 33 --send '5:!START' --send '30:!STOP' --final-command '!STOP'
```

4. 采集结束后立即用 ST-LINK 执行 `-rst -run`，让固件回到校准后等待 `!START` 的安全状态；再解码和分析。
5. 计算初始角度、目标阶跃、10%–90% 上升时间、±0.1 rad 和 2% 稳定时间、超调、末 5 秒平均/MAE/P95 误差、位置标准差、峰值速度、`iq/id` 与相电流峰值。
6. 至少使用 20–25 秒运行窗口检查慢积分和机械粘滑；候选参数必须独立复测。短时误差很小但随后周期性跳动不算收敛。
7. 任何一轮出现非有限角度、持续振荡、异常电流、通信丢失或无法确认 STOP 时，停止提高增益，发送 STOP 并复位后再诊断。

2026-07-18 在当前电机、目标角度 `31.4 rad`、角度/速度环 `1 kHz` 和电流环 `10 kHz` 条件下，已验证的候选参数为：

- `pidAngle = {P=10.0, I=0, D=0, outputRamp=0, limit=5.0 rad/s}`。
- `pidVelocity = {P=0.30, I=0.02, D=0, outputRamp=80, limit=12}`；初始化后输出限幅仍受 `currentLimit` 约束。
- `Motor::Init()` 保留显式配置且更严格的角度 PID limit，仅在该 limit 禁用或超过 `velocityLimit` 时回退到全局速度限制。

两次 25 秒验收均无超调和粘滑跳变：10%–90% 上升时间约 `4.49–4.66 s`，±0.1 rad 稳定时间约 `5.50–5.66 s`，末 5 秒平均绝对误差约 `0.0011–0.0030 rad`，位置标准差约 `0.00016–0.00019 rad`，峰值 `|iq|` 约 `0.70–0.75 A`。这些值只对当时硬件、负载和目标有效；更换电机、惯量、母线电压、负载或目标轨迹后重新执行长时验收。

2026-07-25 在当前固定电机上完成了无扰动保持和单次大外力扰动的板级收敛测试。当前已烧录、已验证的扰动候选参数为：

- `pidAngle = {P=3.0, I=0, D=0, outputRamp=0, limit=3.0 rad/s}`。
- `pidVelocity = {P=0.22, I=0, D=0, outputRamp=80, limit=12}`；位置模式运行时输出还受 `positionCurrentLimit` 约束。
- `lpfVelocity = 40 ms`。
- `positionAccelerationLimit = 8 rad/s²`。
- `positionCurrentLimit = 1.0 A`。
- `positionFrictionCurrent = 0.22 A`，`positionDeadband = 0.005 rad`。补偿在死区外线性渐入，并与速度环输出相加后再次受位置模式 `1.0 A` 总限流约束。
- 速度目标同时受角度 PID、`pidAngle.limit`、剩余制动距离和加速度斜坡约束，保持绝对多圈位置语义。
- 速度环积分保持为零。板测中 `I=0.02` 会在静摩擦下形成“停住—积分累积—突然挣脱”的粘滑振荡，不要仅为减小静态误差重新开启积分；优先重新测量摩擦补偿。
- PID 保留条件积分抗饱和；PID 与低通滤波器内部时间戳为 64 位，低通先计算整数微秒差再转浮点。

启动位置模式时，如果主机尚未通过 `motor.set_position()` 显式设置目标，`!START` 在使能临界区内锁存启动瞬间的实际累计位置；若已显式设置目标，则保留该多圈绝对目标。不要用初始化完成时的旧位置替代启动瞬间位置。

本轮有效证据：

- 无扰动基线会话 `xhfoc_20260725_223701_925790`：`Started ok`，位置误差和速度为零，峰值 `|iq|≈0.043 A`，无自激。
- 最终扰动会话 `xhfoc_20260725_223846_051810`：人工扰动峰值约 `2.413 rad`，峰值速度约 `17.48 rad/s`，峰值 `|iq|≈0.857 A`；没有反向超调和有效误差过零，约 `1.156 s` 进入并持续保持在 `0.02 rad` 范围，末段误差约 `0.0134 rad（0.77°）`。
- 上述结论只覆盖当前电机、固定方式、母线电压和人工扰动。新的正反向 `±31.4 rad` 与更大多圈位置阶跃尚未用这组参数重新回归，不能写成全部位置工况已经验收。

需要人工扰动时，先完成一轮不弹窗、明确要求用户不要触碰电机的无扰动基线。正式扰动轮必须先确认 `Started ok` 且 PWM 遥测已从零切换为有效调制，再显示 Windows 弹窗。弹窗文字明确说明：用户先点击“确定”，随后等待两声短蜂鸣和一声长蜂鸣，只在长蜂鸣时施加一次中等、短促扰动并立即松手。未看到弹窗、未使能、重复施加扰动或顺序错误的会话一律标记为作废，不用于调参。

完整验收至少覆盖正反向 `±31.4 rad`、一个更大的多圈阶跃，以及从小到大的分级外力扰动。报告速度指令是否平滑减速、位置误差过零次数、反向峰值速度、稳定时间、末段 P95 误差、`iq` 是否保持在位置模式 `1.0 A` 限制内、限幅持续时间和肉眼可见卡顿；若仍有周期性卡顿，再检查 MT6816 校验失败、重复采样比例和机械摩擦，不要直接继续提高 PID。

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

CLion、STM32CubeCLT、GNU Arm、STM32CubeMX、调试服务器、探针接口或安装路径变化时，重新读取本机配置并更新“使用 CLion 开发 STM32 固件”部分。至少分别验证一次命令行 CMake Preset 构建和 CLion 对应 Profile；调试链路变化时再验证 ST-LINK 连接、下载与断点。不要仅根据旧的 `.idea` 或 `CMakeCache.txt` 更新版本信息。

固件 USB Native/Fibre JSON 对象接口、UART 参数、VOFA 通道数量或顺序、发送周期、帧格式发生变化时：

1. 物理串口参数改变时更新 `config.json` 默认值。
2. 同时更新 `decoder_config.json` 和本技能的“解释当前固件数据流”部分。
3. 明确说明原始采集兼容性，不要静默地用新格式重新解释旧会话。
4. 运行采集与解析测试：

```powershell
python -m unittest discover -s tests -v
```

5. 使用本地 `skill-creator` 的 `quick_validate.py` 校验本技能。

进行硬件冒烟测试时，使用 `--duration 3` 等短时覆盖参数，确认程序正常退出且 `bytes_written` 非零，检查 `serial_raw.bin` 大小与元数据一致，并确认测试后可以再次打开串口。
