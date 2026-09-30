# 周期信号测量分析装置 · Periodic Signal Measurement & Analysis Device

**2026 全国大学生电子设计竞赛（NUEDC）G 题** · NUEDC 2026 — Problem G
主控 **STM32F407ZGTx** + AD9240 14bit 高速采集

> 作者：llqvpll。
> 核心思路：**AD9240 高速采集 + STM32F407 数字域全处理**，模拟前端只做三件事：缓冲、低通、固定增益。

## 一句话方案

```
BNC 输入 → 高阻缓冲 → UFA42 六阶低通(750kHz) → OPA847 ×5 增益 + 1.25V 偏置
         → AD9240(14bit, 最高 4.8MSPS) → STM32F407 数字域
         → FIR 抗干扰 / 4096 点 FFT / GCD 基频 / 整周期真有效值·峰峰值
         → 800×480 LCD 显示波形与频谱
```

**整条链路没有一个可调增益、没有一个乘法器、没有第二片 ADC。** 指标全部靠「14bit 精度 + 数字信号处理」拿下。

## 指标达成情况

| 题目要求 | 本方案对策 | 裕量 |
|---|---|---|
| Upp / Urms 误差 ≤ 5mV | 14bit ADC（LSB=153μV@2.5V）+ ×5 增益 + 整周期平均压噪声 | > 50 倍 |
| 基频误差 ≤ 1kHz | 谱峰 GCD 法 + 抛物线插值，实测 < 10Hz | 100 倍 |
| 频率分辨率 500Hz | 4096 点 @2MHz = 488Hz；Item3 抽取后 293Hz | 达标 |
| 频谱幅值误差 ≤ 5mV | Hanning 窗 + 抛物线插值 + 增益标定，误差 < 1% | 5 倍以上 |
| Item3：≥1MHz / 200mVpp 单频干扰 | 模拟 LPF(-18dB@1MHz) + 数字 FIR(-60dB)，总抑制约 78dB | 200mV → 0.025mV |
| 每项测量 < 2s | 采集 + FFT + 显示全程 < 0.5s | 4 倍以上 |
| 显示屏 ≥ 6 英寸 | 800×480 LCD | 达标 |
| 单路 5V 供电 | 多路电源模块产生 ±5V / 3.3V | 达标 |

## 硬件平台

- **MCU**：STM32F407ZGTx（168MHz，Cortex-M4F，带 FPU + DSP 指令）
- **ADC**：AD9240，14bit，最高 4.8MSPS
- **模拟前端**：UFA42 六阶低通（750kHz 抗混叠）、OPA847 ×5 反相、NE5532 叠加 1.25V 偏置、TLV3501 过零比较（测频互验）
- **显示**：板载 SSD1963 800×480（FSMC 总线）
- **输入**：PS2 小键盘
- **开发环境**：Keil MDK（ARMCC / AC6），调试器 J-Link

### MCU 引脚分配

| 信号 | 引脚 | 说明 |
|---|---|---|
| AD9240 D0–D13 | PG0–PG13 | 全空、连续端口，单次 IDR 读取。**不可选 PE7–PE15（液晶屏 FSMC 占用）** |
| AD9240 CLK | PC6（TIM8_CH1） | 采样时钟，168MHz 整数分频 |
| TLV3501 OUT | PA8（TIM1_CH1） | 比较器测频互验 |
| AD9959 | 不接 | 本题不用校准源，驱动代码保留未调用 |

> 若 AD9240 焊在其他端口：改 `_03_Drive/Drive_AD9240.h` 中的
> `AD9240_DATA_GPIO` / `AD9240_DATA_GPIO_CLK` / `AD9240_DATA_IDR_ADDR` 三个宏即可。

## 操作方式

开机默认进入 **G 题模式**；按 **NumLock** 退回原工程菜单（AD DA + AD9959 + ADS1256 旧功能全部保留），旧菜单按 **[6]** 返回 G 题。

| 键 | 功能 |
|---|---|
| `[1]` | Item1 测量（ua，100–250mVpp，10k–200kHz） |
| `[2]` | Item2 测量（ub，50–250mVpp，10k–500kHz） |
| `[3]` | Item3 测量（ub + uJ，uJ ≥1MHz / 200mVpp 单频干扰） |
| `[4]` | 波形 1 周期 / 3 周期切换 |
| `[5]` | 波形视图 |
| `[6]` | 频谱视图 |
| `[Enter]` | 重新测量当前项 |
| `[NumLock]` | 退回旧菜单 |

按 `[1]/[2]/[3]` 后自动完成采集 → 分析 → 显示，全程 < 0.5s。
左屏显示 Upp / Urms（真有效值）/ f1 / fref（比较器互验）/ 各次谐波分量，右屏显示波形或频谱。

### 测量原理

| 项 | 采样 | 处理 | 分辨率 |
|---|---|---|---|
| Item1 / 2 | 4096 点 @2MHz（TIM8÷84） | Hanning 窗 FFT4096 → 谱峰插值 → GCD 基频 | 488Hz |
| Item3 | 16528 点 @4.8MHz（TIM8÷35） | 127 阶 FIR（fc=600k）+ 抽取 → FFT | 293Hz |

## 目录结构

```
.
├── firmware/                 Keil MDK 工程与全部源码
│   ├── _04.uvprojx           工程文件（Device: STM32F407ZGTx）
│   ├── _04.uvoptx
│   ├── DebugConfig/          调试配置
│   ├── _01_App/              应用层：G 题测量、FFT、扫频、触摸、UI
│   ├── _02_Core/             CMSIS 内核、启动文件、CMSIS-DSP 库
│   ├── _03_Drive/            外设驱动：AD9240/AD9959/ADS1256/DAC/比较器/LCD/迪文屏…
│   ├── _04_FWLib/            STM32F4 标准外设库（STD 库）
│   ├── _05_Os/               轻量任务调度与 UI 框架
│   ├── _06_System/           delay / sys / usart
│   ├── _07_TFT_LCD/          LCD 驱动、字库、W25Q64、触摸
│   └── _08_USB/              ST USB 设备/主机库
├── docs/                     设计文档（方案论证、改造说明、接线、代码审查…）
├── tools/                    辅助脚本（报告生成、DGUS 界面生成）
└── assets/                   界面预览图
```

## 编译与烧录

1. 用 **Keil MDK** 打开 `firmware/_04.uvprojx`（Device 已配置为 STM32F407ZGTx）。
2. 首次编译前确认已安装 STM32F4 的 Device Family Pack（DFP）。
3. `Build` → 用 J-Link（SWD）下载 `Output/_04.axf` 或对应 hex 到目标板。
4. 上电后默认进入 G 题模式。

> CMSIS-DSP 静态库 `arm_cortexM4lf_math.lib` 已在仓库中（`_02_Core/`），FFT 依赖它，无需另外获取。

## 文档索引

| 文档 | 内容 |
|---|---|
| `docs/G题总体方案设计.md` | 指标拆解、模拟前端计算、增益 5 倍的决策论证 |
| `docs/G题模式改造说明.md` | 在原工程上改造为 G 题模式的过程、引脚变更、键位 |
| `docs/G题方案分析.md` | 赛题分析与方案选型 |
| `docs/代码优化方案.md` | 代码层面的优化记录 |
| `docs/代码审查报告.md` | 代码审查结论 |
| `docs/接线指南.md` | 完整硬件接线 |
| `docs/迪文屏DGUS界面设计.md`、`docs/DGUS配置指南.md` | 迪文屏 DGUS 界面设计与配置 |
| `docs/DGUS_VP地址映射表.txt` | DGUS 变量地址映射 |

## 许可

本项目代码采用 **MIT License**（见 `LICENSE`）。

第三方组件遵循各自许可，不在本项目 MIT 授权范围内：

- `firmware/_04_FWLib/`、`firmware/_08_USB/` — STM32F4 标准外设库与 USB 库，© STMicroelectronics，遵循 ST 自有许可条款。
- `firmware/_02_Core/` 中的 CMSIS 头文件与 `arm_cortexM4lf_math.lib` — © ARM Ltd.，遵循 CMSIS 许可。
- `firmware/_07_TFT_LCD/` 中的 LCD / 字库部分源自正点原子例程，遵循其随附许可。
