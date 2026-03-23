# UART_DEMO

## UART_DMA移植流程

- 配置开启UART的DMA传输，RX和TX均为normal，开启串口、DMA中断，设置中断优先级为3

- 将文件加入项目

  <img src="README.assets/image-20260323171836139.png" alt="image-20260323171836139" style="zoom:33%;" />

- CMakeLists.txt修改

  ```
  # Add sources to executable
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE
      # Add user sources here
          Platform/Uart/retarget.c
          Platform/Utils/st_hardware.c
          UserApp/Protocols/interface_uart.cpp
          UserApp/main.cpp
  )
  
  # Add include paths
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
      # Add user defined include paths
          Platform/Uart
          Platform/Utils
          UserApp/Protocols
          UserApp
  )
  ```

  ![image-20260323171809714](README.assets/image-20260323171809714.png)

- gcc-arm-none-eabi.cmake文件修改，使其支持浮点数

  ```
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _scanf_float -u _printf_float")
  ```

  ![image-20260323172026620](README.assets/image-20260323172026620.png)

- main.c修改

  ![image-20260323172209549](README.assets/image-20260323172209549.png)

- usart.c修改

  ![image-20260323172348330](README.assets/image-20260323172348330.png)

  ![image-20260323172411958](README.assets/image-20260323172411958.png)

- usart.h修改

  ![image-20260323172658367](README.assets/image-20260323172658367.png)

- stm32g4xx_it.c修改

  ![image-20260323172455610](README.assets/image-20260323172455610.png)

- 
