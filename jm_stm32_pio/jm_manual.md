JM.BAT 使用手册
===============

脚本位置: jm.bat
作用: 使用 PlatformIO + ST-LINK 完成 STM32 固件编译、烧录、串口监听。

一、基本用法
-----------
  jm.bat <ACTION> <BOARD_NAME>

二、参数说明
-----------
  ACTION    操作类型，必填
            compile  - 编译固件
            upload   - 编译并烧录固件到目标板（通过 ST-LINK）
            monitor  - 打开串口监视器，实时查看日志输出

  BOARD_NAME 目标板子名称，必填
             stm32f1_bluepill   - STM32F103C6（Bluepill 板）
             stm32f1_blackpill  - STM32F103C8（Blackpill 板，芯片比 bluepill 容量更大）
             stm32f103_nucleo64 - STM32F103RB（Nucleo-64 开发板）
             stm32f411_nucleo64 - STM32F411RE（Nucleo-64，Cortex-M4 高性能版）

三、使用示例
-----------
  编译 Bluepill 板固件
    jm.bat compile stm32f1_bluepill

  编译并烧录到 Blackpill 板
    jm.bat upload stm32f1_blackpill

  打开串口监视器查看日志
    jm.bat monitor stm32f103_nucleo64

四、编译输出
-----------
  固件编译后生成在:
    .pio/build/<ENV_NAME>/
  其中 <ENV_NAME> 对应 BOARD_NAME 的 PlatformIO 环境名称：
    stm32f1_bluepill   -> stm32f1_bluepill
    stm32f1_blackpill  -> stm32f1_blackpill
    stm32f103_nucleo64 -> stm32f103_nucleo64
    stm32f411_nucleo64 -> stm32f411_nucleo64

五、烧录说明
-----------
  使用 ST-LINK 进行烧录，连接方式：
    1. 将 ST-LINK 的 SWDIO、SWCLK、GND、VCC 连接到目标板对应引脚
    2. 运行 upload 命令，脚本会自动检测并烧录

  如果遇到连接失败，可尝试在 platformio.ini 中调整 upload_flags 的 CPUTAPID。

六、串口监视器说明
-----------------
  monitor 命令默认使用以下配置：
    波特率: 115200
    数据位: 8
    停止位: 1
    校验位: None
    流控: None

  退出监视器：按 Ctrl+C

七、编译模式选择
--------------
  默认使用寄存器直驱模式（无需 HAL 库），体积小、速度快。

  如需使用 HAL 库模式，请修改 platformio.ini：
    1. 将 USE_HAL_UART 相关的 build_flags 取消注释
    2. 重新运行 jm.bat compile <BOARD_NAME>

八、常见问题
----------
  Q: 提示 "Unknown board" 怎么办？
  A: 检查 BOARD_NAME 拼写是否正确，见上方支持的板子列表。

  Q: 提示 "Unknown action" 怎么办？
  A: ACTION 只能是 compile、upload、monitor 之一。

  Q: 烧录失败提示 "device not found"？
  A: 检查 ST-LINK 物理连接，确认目标板已上电。

  Q: 编译失败？
  A: 确认已安装 PlatformIO 扩展或命令行工具 pio 在 PATH 中。

九、相关文件
----------
  jm.bat         - 本脚本
  platformio.ini - PlatformIO 项目配置（板子映射、编译标志、烧录参数等）
  src/main.c     - STM32 程序入口
  lib/           - 本地库目录
  ../jm_stm32/   - 上游 jm_stm32 库路径（通过 lib_extra_dirs 引用）
