# qemu-embedded-platform
Hello this is qemu embedded platform


## one-step build (apply patch + build in MSYS2 MINGW64 as administrator)
```shell
.\scripts\qemu-build.ps1
```

> 说明：
> - 脚本会自动执行 `qemu-apply-patch.ps1` 打补丁，然后以管理员权限启动 MSYS2 MINGW64 执行 `qemu-build-msys2.sh` 构建。
> - 弹出 UAC 授权窗口时点击“是”（QEMU configure 在 Windows 上创建符号链接需要管理员权限）。
> - 常用参数：
>   - `-SkipPatch`：跳过打补丁，只构建
>   - `-SkipAdmin`：跳过管理员提权（仅当已开启 Windows 开发者模式时可用）
>   - `-Msys2Root C:\msys64`：手动指定 MSYS2 安装目录（不传时依次探测 默认值 → C:\msys64 or C:\msys2 → 注册表）


## sync qemu
```shell
.\scripts\qemu-sync.ps1
```


## apply patch
```shell
.\scripts\qemu-apply-patch.ps1
```



## build qemu
```shell
.\scripts\qemu-build-msys2.sh
```

## copy ./qemu/qemu-configure/qemu-system-arm.exe to the ./testcase directory


## test target
```shell
.\qemu-system-arm.exe -machine mps2-an505 -cpu cortex-m33 -m 16M -kernel .\an505-qemu.elf -display sdl,show-cursor=on -serial stdio
```


