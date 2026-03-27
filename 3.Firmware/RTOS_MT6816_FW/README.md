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
fix(comm): 修复G4 USB Native通道自动连接失败（PMA地址重叠）

[背景]
- 问题/需求: FOC通过精确控制电机的磁场方向，实现高效、稳定的运行。
- 触发条件: 在“第一次仅拷贝通信架构文件”基础上，G4固件可枚举但CLI-Tool自动连接超时，表现为发现设备后握手无回包。

[改动]
- 新增目录:
  - 无

- 修改文件:
  - USB_Device\Target\usbd_conf.c

- 关键点:
  - 修正 USB FS PMA 内存分配，避免 EP0 与新增 EP3（Native接口）BTABLE/缓冲区重叠。
  - 将 EP0 OUT PMA 起始地址由 `0x18` 调整为 `0x20`，EP0 IN 由 `0x58` 调整为 `0x60`。
  - 保持 CDC + Native 双通道架构不变，仅修复底层内存布局冲突。

[影响评估]
- 硬件影响: 无新增硬件依赖，仍基于现有 USB FS。
- 实时性影响: 无新增阻塞；仅调整USB底层缓冲区地址映射。
- 资源影响: 可忽略（无新增功能模块，仅地址配置修正）。
- 兼容性影响: 保留 CubeMX 框架；上层协议与CLI-Tool接口不变。

[验证]
- 构建: cmake --preset Debug && cmake --build --preset Debug -j 8
  - [√] 本地构建通过
- 板测:
  - [√] USB枚举正常（VID/PID识别正常）
  - [√] CLI-Tool可自动发现设备
  - [√] 自动连接握手成功（不再“有发无收”）
  - [√] Native接口通信稳定，功能恢复正常

[回滚]
- git revert <this_commit_hash>
- 重点回滚文件:
  - USB_Device\Target\usbd_conf.c
```

