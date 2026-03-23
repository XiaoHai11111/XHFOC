# RTOS_MT6816_FW 

## 通信框架移植

- 将3rdParty、Platform、UserApp文件夹拷贝进当前目录

## MT6816移植



## LED移植



## KEY按键移植



## git提交备注

```text
feat(comm): 第一次提交，只将需要移植的通信架构文件拷贝进了项目，还为实现移植

[背景]
- 问题/需求: FOC通过精确控制电机的磁场方向，实现高效、稳定的运行。
- 触发条件: 现工程仅有外设初始化，缺少统一通信架构、异步收发与命令解析链路。

[改动]
- 新增目录:

- 修改文件:

- 关键点:


[影响评估]
- 硬件影响: 
- 实时性影响: 
- 资源影响: 
- 兼容性影响: 保留 CubeMX 框架

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
- 板测:
  - [ ] 
  - [ ] 
  - [ ] 
  - [ ] 

[回滚]
- 
- 重点回滚文件:
  - 
  - 
  - 
  - 
```

```
feat(comm): 完善通信架构移植，修复USB单端点首次发送丢包（首次/二次发送无回包）

[背景]
- 问题/需求: FOC通过精确控制电机的磁场方向，实现高效、稳定的运行。
- 触发条件: 上一版通信链路在USB重连/首次打开后，前几次发送存在“有发无收”现象，回包不稳定。

[改动]
- 新增目录:
  - 无

- 修改文件:
  - Middlewares\ST\STM32_USB_Device_Library\Class\CDC\Src\usbd_cdc.c

- 关键点:
  - 在 `USBD_CDC_Init` 中补齐自定义端点状态初始化（`CDC_Tx/CDC_Rx/REF_Tx/ODRIVE_Rx`），避免上电或重连后状态不确定。
  - 首次 `PrepareReceive` 由 `hcdc->RxBuffer` 对齐到实际使用的 `hcdc->CDC_Rx.Buffer`，修复首包接收缓冲不一致。
  - `USBD_CDC_SetTxBuffer` 在 CDC 端点同步维护 `hcdc->TxBuffer/TxLength`，保证完成回调链路一致。
  - `USBD_CDC_TransmitPacket` 增加 `ep_in.total_length` 与 `hcdc->TxBuffer/TxLength` 同步，避免 DataIn/ZLP 逻辑与自定义发送状态脱节。
  - 本次差异统计：`1 file changed, 13 insertions(+), 2 deletions(-)`。

[影响评估]
- 硬件影响: 无新增硬件依赖，仍基于现有 USB FS。
- 实时性影响: 无明显新增阻塞；主要为状态与缓冲一致性修复。
- 资源影响: 可忽略（仅少量状态赋值与字段同步）。
- 兼容性影响: 保留 CubeMX 框架与现有 CDC 类接口，不改变上层协议格式。

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
  - [√] 本地构建通过
- 板测:
  - [√] USB枚举正常
  - [√ ] USB上电后首次发送回包稳定
  - [√ ] USB上电后二次发送回包稳定
  - [√ ] 连续发送压测无有发无收

[回滚]
- git revert <this_commit_hash>
- 重点回滚文件:
  - Middlewares\ST\STM32_USB_Device_Library\Class\CDC\Src\usbd_cdc.c
```

