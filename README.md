# qemu-embedded-platform

基于 QEMU 的嵌入式平台开发环境，内置针对 ARM `mps2-an505`（Cortex-M33）目标板的定制 overlay 补丁，并提供一键打补丁 + 构建 + 运行的完整工作流（Windows / MSYS2）。

overlay 补丁在板级增加了 4 个 **MPSX Simple** 定制外设（LCD 显示、触摸、音频输出、麦克风录音），供裸机/RTOS 固件通过 MMIO 直接驱动，详见[定制外设](#定制外设mpsx-simple-设备)。

---

## 目录结构

```
qemu-embedded-platform/
├── configs/
│   └── qemu.json            # QEMU 版本配置（branch / commit / overlay）
├── overlay/
│   └── qemu/
│       └── master/          # 对 QEMU 源码的定制文件（hw/、include/）
├── patches/
│   └── qemu/
│       └── master/
│           └── overlay.patch  # 由 overlay 导出的补丁
├── qemu/                    # QEMU 源码（git submodule，受 overlay 补丁定制）
│   ├── qemu-configure/      # 构建配置目录（configure 产物）
│   └── qemu-build/          # 安装目录（--prefix，含 qemu-system-arm.exe）
├── scripts/
│   ├── qemu-build.ps1       # 一键：打补丁 + 构建（推荐入口）
│   ├── qemu-build-msys2.sh  # 实际构建脚本（MSYS2 MINGW64 内运行）
│   ├── qemu-apply-patch.ps1 # 应用 overlay 补丁
│   ├── qemu-sync.ps1        # 同步 qemu 子模块到配置的 commit
│   └── qemu-export-patch.ps1# 把 overlay 导出为补丁
├── testcase/
│   ├── an505-qemu.elf       # 测试固件
│   └── audio_test_*k.wav    # 麦克风录音测试音频（8k/16k/44k/48k）
└── docs/
    └── msys2_qemu_build_steps.md  # 详细的 MSYS2 环境搭建与 QEMU 构建步骤
```

---

## 环境准备

- **Windows** + **MSYS2**（含 MINGW64 环境）
- MSYS2 中需安装 QEMU 构建依赖（`base-devel`、`mingw-w64-x86_64-toolchain`、`glib2`、`pixman`、`gtk3`、`SDL2` 等）
- 详细的环境搭建步骤见 [`docs/msys2_qemu_build_steps.md`](docs/msys2_qemu_build_steps.md)

> 首次构建前请先 `git submodule update --init --recursive` 拉取 `qemu` 子模块。

---

## 快速开始（一键构建）

在仓库根目录执行：

```powershell
.\scripts\qemu-build.ps1
```

脚本会自动完成：
1. **打补丁**：调用 `qemu-apply-patch.ps1` 应用 overlay 补丁
2. **构建**：以管理员权限在 MSYS2 MINGW64 中执行 `qemu-build-msys2.sh`（configure → make → install → 运行测试）

> 注意：弹出 UAC 授权窗口时点击"是"。QEMU 的 configure 在 Windows 上需要创建符号链接（`scripts/symlink-install-tree.py`），未开启开发者模式时必须使用管理员权限。

### 常用参数

| 参数 | 说明 |
|------|------|
| `-SkipPatch` | 跳过打补丁，只构建（适合快速增量重建） |
| `-SkipAdmin` | 不申请管理员权限（仅当已开启 Windows 开发者模式时可用） |
| `-Msys2Root C:\msys64` | 手动指定 MSYS2 安装目录 |
| `-ConfigFile configs\qemu.json` | 指定配置文件（默认 `configs/qemu.json`） |

**MSYS2 目录的自动探测顺序**（不传 `-Msys2Root` 时）：
1. `$env:USERPROFILE\program\MSYS64`
2. `C:\msys64` or `C:\msys2`
3. 注册表（MSYS2 卸载信息）

若显式传入 `-Msys2Root`，则直接使用该路径，路径无效会立即报错。

---

## 完整工作流

### 1. 同步 QEMU 子模块到指定版本

```powershell
.\scripts\qemu-sync.ps1
```

按 `configs/qemu.json` 中的 `branch` / `commit` 同步 `qemu` 子模块。

### 2. 应用 overlay 补丁

```powershell
.\scripts\qemu-apply-patch.ps1
```

将 `patches/qemu/master/overlay.patch` 应用到 `qemu/` 源码。会先重置工作区再打补丁；**保留构建目录 `qemu-configure/`、`qemu-build/`**，便于增量构建。

### 3. 构建

```powershell
.\scripts\qemu-build-msys2.sh
```

> 需在 **MSYS2 MINGW64** 环境中执行（管理员权限，如未开启开发者模式）。也可以通过 `qemu-build.ps1` 自动完成。

脚本会：
- configure（若 `qemu-configure/config-host.mak` 已存在则跳过，实现增量构建）
- `make` 增量编译（`--target-list=arm-softmmu`，开启 debug，关闭 docs）
- `make install` 安装到 `qemu/qemu-build/`
- 自动运行测试

### 4. 运行测试

构建脚本会自动执行：

```powershell
.\qemu\qemu-build\qemu-system-arm.exe -machine mps2-an505 -cpu cortex-m33 -m 16M -kernel .\testcase\an505-qemu.elf -display sdl,show-cursor=on -serial stdio
```

也可以手动复制 `qemu/qemu-build/qemu-system-arm.exe` 到 `testcase/` 后运行：

```powershell
cd .\testcase
.\qemu-system-arm.exe -machine mps2-an505 -cpu cortex-m33 -m 16M -kernel .\an505-qemu.elf -display sdl,show-cursor=on -serial stdio
```

### 5. 修改定制并导出补丁（可选）

修改 `overlay/qemu/master/` 下的文件后，重新导出补丁：

```powershell
.\scripts\qemu-export-patch.ps1
```

---

## 定制外设（MPSX Simple 设备）

overlay 补丁在 `mps2-an505` 板级增加了 4 个 SysBus 设备，全部映射在 `0x51000000` 起始的 MMIO 空间（每设备 0x1000），guest 驱动通过 MMIO 寄存器直接控制：

| 设备 | 类型名 | MMIO 基址 | IRQ | 用途 |
|------|--------|-----------|-----|------|
| LCD | `mpsx-simple-lcd` | `0x51000000` | 33 | Framebuffer 显示 |
| Touch | `mpsx-simple-touch` | `0x51001000` | 32 | 触摸输入 |
| Audio（输出） | `mpsx-simple-audio` | `0x51002000` | 49 | PCM 播放 |
| Mic（输入） | `mpsx-simple-mic` | `0x51003000` | 50 | PCM 录音 |

> IRQ 号是 `get_sse_irq_in()` 传入的 SSE IRQ 编号（叠加到 NVIC 的向量号）。

### mpsx-simple-lcd（0x51000000）

| 偏移 | 寄存器 | 说明 |
|------|--------|------|
| 0x00 | WIDTH | 显示宽度 |
| 0x04 | HEIGHT | 显示高度 |
| 0x08 | FB_ADDR | framebuffer 物理地址 |
| 0x0C | CTRL | bit0=ENABLE, bit1=UPDATE, bit2=RESET |
| 0x10 | STATUS | bit0=BUSY, bit1=DONE |
| 0x14 | FORMAT | 0=ARGB8888, 1=RGB888, 2=RGB565 |
| 0x18 | STRIDE | 每行字节数 |
| 0x1C / 0x20 | INT_EN / INT_STATUS | 中断使能/状态 |

### mpsx-simple-touch（0x51001000）

| 偏移 | 寄存器 | 说明 |
|------|--------|------|
| 0x00 | STATUS | bit0=PRESSED, bit1=READY |
| 0x04 / 0x08 | X / Y | 12-bit 触摸坐标（0-4095） |
| 0x0C | CTRL | bit0=CLEAR_INT |
| 0x10 | ID | 设备 ID（"MPSX"） |
| 0x14 / 0x18 | RES_X / RES_Y | 分辨率 4096 |

由 QEMU 输入子系统驱动（鼠标/触摸），无需命令行参数。

### mpsx-simple-audio（0x51002000，输出）

| 偏移 | 寄存器 | 说明 |
|------|--------|------|
| 0x00 | CTRL | bit0=ENABLE, bit1=RESET, bit2=UPDATE |
| 0x04 | STATUS | bit0=BUSY, bit1=DONE, bit2=UNDERRUN |
| 0x08 | FORMAT | bits[1:0]: 0=U8,1=S16；bit2=stereo |
| 0x0C / 0x10 | BUF_ADDR / BUF_LEN | PCM 缓冲地址/长度 |
| 0x14 | SAMPLE_RATE | 采样率（1000-192000） |
| 0x18 | PLAY_POS | 播放位置（只读） |
| 0x1C / 0x20 | INT_EN / INT_STATUS | 中断使能/状态 |

guest 驱动把 PCM 数据写入 `BUF_ADDR` 指定内存，设备经音频后端播放；播放完一轮触发 DONE 中断请求补充数据。

### mpsx-simple-mic（0x51003000，输入）

寄存器布局与 audio 输出镜像（`REC_POS` 代替 `PLAY_POS`），设备把采集到的 PCM 数据 DMA 写入 `BUF_ADDR` 指向的 guest 内存，写满一轮触发 DONE 中断，guest 驱动读取录音数据。

采集源二选一：
- **WAV 文件**（测试用，无需真实音频硬件）：`-global mpsx-simple-mic.infile=<路径>`，设备按 WAV 采样率循环喂 PCM。
- **真实麦克风**：通过机器 `audiodev` 属性关联 `-audiodev` 后端（如 `dsound`/`sdl` 的输入通道）。

---

## 构建后运行 QEMU（使用示例）

构建产物为 `qemu/qemu-build/qemu-system-arm.exe`（也可直接使用 `qemu/qemu-configure/qemu-system-arm.exe`）。

### 基本运行（LCD + Touch）

```powershell
.\qemu\qemu-build\qemu-system-arm.exe `
  -machine mps2-an505 -cpu cortex-m33 -m 16M `
  -kernel .\testcase\an505-qemu.elf `
  -display sdl,show-cursor=on -serial stdio
```

### 音频播放（mpsx-simple-audio）

先定义音频后端并把机器 `audiodev` 指向它：

```powershell
.\qemu\qemu-build\qemu-system-arm.exe `
  -machine mps2-an505 -cpu cortex-m33 -m 16M `
  -kernel .\testcase\an505-qemu.elf `
  -audiodev dsound,id=aud0,out.frequency=44100,out.channels=2 `
  -global mps2-an505.audiodev=aud0 `
  -display sdl,show-cursor=on -serial stdio
```

### 麦克风录音（mpsx-simple-mic）

**方式一：WAV 文件源**（无需麦克风，测试最方便）：

```powershell
.\qemu\qemu-build\qemu-system-arm.exe `
  -machine mps2-an505 -cpu cortex-m33 -m 16M `
  -kernel .\testcase\an505-qemu.elf `
  -global mpsx-simple-mic.infile=testcase\audio_test_8k.wav `
  -display sdl,show-cursor=on -serial stdio
```

> 驱动侧 `SAMPLE_RATE` 需与所选 WAV 采样率一致（8k/16k/44k/48k 分别对应 8000/16000/44100/48000）。

**方式二：真实麦克风**（宿主机音频输入）：

```powershell
... -audiodev dsound,id=aud0,in.voices=1 `
    -global mps2-an505.audiodev=aud0 ...
```

### 测试音频文件（testcase/）

| 文件 | 采样率 | 扫频范围 | 对应 SAMPLE_RATE |
|------|--------|----------|------------------|
| `audio_test_8k.wav` | 8000 | 20Hz–3.6kHz | 8000 |
| `audio_test_16k.wav` | 16000 | 20Hz–7.2kHz | 16000 |
| `audio_test_44k.wav` | 44100 | 20Hz–20kHz | 44100 |
| `audio_test_48k.wav` | 48000 | 20Hz–20kHz | 48000 |

每段 5 秒、16-bit 单声道 PCM，内容依次为：0.5s 静音 → 1kHz 正弦 → 440Hz 正弦 → 对数扫频 → 白噪声，覆盖电平校准、标准音高、频率响应、信噪比等常见测试。

---

## 工作流程示意

```mermaid
flowchart LR
    A[overlay/ 定制文件] -->|qemu-export-patch.ps1| B[patches/.../overlay.patch]
    B -->|qemu-apply-patch.ps1| C[qemu/ 源码]
    C -->|qemu-build-msys2.sh| D[qemu-configure/ + qemu-build/]
    D -->|测试| E[qemu-system-arm.exe]
    C -.同步.-> F[qemu-sync.ps1]
```

---

## 配置说明（configs/qemu.json）

```json
{
    "branch": "master",
    "commit": "3e3ccab106f879b1512f8e0d51a827dd4de30e22",
    "overlay": "master"
}
```

| 字段 | 说明 |
|------|------|
| `branch` | QEMU 源码分支 |
| `commit` | 同步/回退到的 QEMU commit |
| `overlay` | 使用的 overlay 名称（对应 `overlay/qemu/<name>/` 与 `patches/qemu/<name>/`） |

---

## 常见问题

- **为什么需要管理员权限？** QEMU configure 在 Windows 上创建符号链接（`scripts/symlink-install-tree.py`）。除非开启 Windows 开发者模式，否则必须管理员运行。可用 `-SkipAdmin` 跳过（仅开发者模式开启时）。
- **每次构建都出现 `Regenerating build files`？** 正常。打补丁会重写 meson.build 等文件，Meson 自动重新生成构建文件（检测结果带 `cached`，很快），不是全量 configure。
- **如何避免全量重新 configure？** `qemu-configure/` 会被打补丁脚本保留；只要它存在，构建就会跳过 configure 直接增量 make。想强制全新配置时手动删除 `qemu/qemu-configure` 与 `qemu/qemu-build`。
- **快速重建**：只改了 QEMU 源码、未改 overlay/补丁时，用 `.\scripts\qemu-build.ps1 -SkipPatch` 最快。

---

## 相关文档

- [MSYS2 环境搭建与 QEMU 构建详细步骤](docs/msys2_qemu_build_steps.md)


