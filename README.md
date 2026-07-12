# qemu-embedded-platform
Hello this is qemu embedded platform


## sync qemu
```shell
.\scripts\qemu-sync.ps1
```


## apply patch
```shell
.\scripts\qemu-apply-patch.ps1
```


## open the MSYS-MingW64 terminal in administrator mode


## build qemu
```shell
.\scripts\qemu-build.sh
```

## copy ./qemu/qemu-configure/qemu-system-arm.exe to the ./testcase directory


## test target
```shell
.\qemu-system-arm.exe -machine mps2-an505 -cpu cortex-m33 -m 16M -kernel .\an505-qemu.elf -display sdl -serial stdio
```

