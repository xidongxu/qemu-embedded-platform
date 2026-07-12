# Win11本地构建QEMU二进制文件步骤



## 1. 安装 MSYS2 

安装：msys2-x86_64-20240113.exe

> 工程的 ./qemu-packs 目录下包含 msys2-x86_64-20240113.exe 文件，可以直接安装使用，如果需要安装最新版本请参考下面的安装方式，这里以：msys2-x86_64-20240113.exe 文件为例：

网页：https://www.msys2.org/ 中，找到然后下载：msys2-x86_64-20240113.exe 文件，下载并安装即可。 
链接：https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe



## 2. 配置 MSYS2 

> 这里安装目录：C:\msys64 为例，进入安装目录：C:\msys64\etc\pacman.d 

- 在文件 mirrorlist.msys 的前面插入

  ```shell
  Server = http://mirrors.ustc.edu.cn/msys2/msys/$arch
  ```

- 在文件 mirrorlist.mingw32 的前面插入

  ```shell
  Server = http://mirrors.ustc.edu.cn/msys2/mingw/i686
  ```

- 在文件 mirrorlist.mingw64 的前面插入

  ```shell
  Server = http://mirrors.ustc.edu.cn/msys2/mingw/x86_64
  ```



## 3. 使用 MSYS2 

> 使用管理员权限打开终端

开始菜单找到 MSYS2 文件夹，然后找到：MSYS2 MINGW64，使用管理员权限打开此终端



## 4. 在 MSYS2 安装 qemu 依赖库

> 期间可能会询问是否安装，直接全部 Y 即可，安装过程中可能会自动关闭终端，需要再次使用管理员权限打开，继续执行其它安装步骤

```shell
pacman -Syu
pacman -Su
```

```shell
pacman -S base-devel mingw-w64-x86_64-toolchain git python ninja
pacman -S mingw-w64-x86_64-glib2 mingw-w64-x86_64-pixman python-setuptools
pacman -S mingw-w64-x86_64-gtk3 mingw-w64-x86_64-SDL2 mingw-w64-x86_64-libslirp
```

```shell
pacman -Sy mingw-w64-x86_64-meson mingw-w64-x86_64-ninja \
           mingw-w64-x86_64-python \
           mingw-w64-x86_64-python-sphinx \
           mingw-w64-x86_64-python-sphinx_rtd_theme \
           mingw-w64-x86_64-autotools \
           mingw-w64-x86_64-tools-git \
           mingw-w64-x86_64-cc \
           mingw-w64-x86_64-angleproject \
           mingw-w64-x86_64-capstone \
           mingw-w64-x86_64-curl \
           mingw-w64-x86_64-cyrus-sasl \
           mingw-w64-x86_64-expat \
           mingw-w64-x86_64-fontconfig \
           mingw-w64-x86_64-freetype \
           mingw-w64-x86_64-fribidi \
           mingw-w64-x86_64-gcc-libs \
           mingw-w64-x86_64-gdk-pixbuf2 \
           mingw-w64-x86_64-gettext \
           mingw-w64-x86_64-glib2 \
           mingw-w64-x86_64-gmp \
           mingw-w64-x86_64-gnutls \
           mingw-w64-x86_64-graphite2 \
           mingw-w64-x86_64-gst-plugins-base \
           mingw-w64-x86_64-gstreamer \
           mingw-w64-x86_64-gtk3 \
           mingw-w64-x86_64-harfbuzz \
           mingw-w64-x86_64-jbigkit \
           mingw-w64-x86_64-lerc \
           mingw-w64-x86_64-libc++ \
           mingw-w64-x86_64-libdatrie \
           mingw-w64-x86_64-libdeflate \
           mingw-w64-x86_64-libepoxy \
           mingw-w64-x86_64-libffi \
           mingw-w64-x86_64-libiconv \
           mingw-w64-x86_64-libidn2 \
           mingw-w64-x86_64-libjpeg-turbo \
           mingw-w64-x86_64-libnfs \
           mingw-w64-x86_64-libpng \
           mingw-w64-x86_64-libpsl \
           mingw-w64-x86_64-libslirp \
           mingw-w64-x86_64-libssh \
           mingw-w64-x86_64-libssh2 \
           mingw-w64-x86_64-libtasn1 \
           mingw-w64-x86_64-libthai \
           mingw-w64-x86_64-libtiff \
           mingw-w64-x86_64-libunistring \
           mingw-w64-x86_64-libunwind \
           mingw-w64-x86_64-libusb \
           mingw-w64-x86_64-libwebp \
           mingw-w64-x86_64-libwinpthread-git \
           mingw-w64-x86_64-lz4 \
           mingw-w64-x86_64-lzo2 \
           mingw-w64-x86_64-nettle \
           mingw-w64-x86_64-openssl \
           mingw-w64-x86_64-opus \
           mingw-w64-x86_64-orc \
           mingw-w64-x86_64-p11-kit \
           mingw-w64-x86_64-pango \
           mingw-w64-x86_64-pixman \
           mingw-w64-x86_64-SDL2 \
           mingw-w64-x86_64-SDL2_image \
           mingw-w64-x86_64-snappy \
           mingw-w64-x86_64-spice \
           mingw-w64-x86_64-usbredir \
           mingw-w64-x86_64-xz \
           mingw-w64-x86_64-zlib \
           mingw-w64-x86_64-zstd
```



## 5. 拉取最新工程

> 使用官方最新版本，请使用以下命令：

```shell
git clone https://gitlab.com/qemu-project/qemu.git
cd qemu
git submodule init
git submodule update --recursive
mkdir ./qemu-configure
```



## 7. 进入临时配置目录

```shell
cd ./qemu-configure
```



## 8. 配置工程

### 查看配置选项

```shell
../configure --help

Usage: configure [options]
Options: [defaults in brackets after descriptions]

Standard options:
  --help                   print this message
  --target-list=LIST       set target list (default: build all)
                           Available targets: aarch64-softmmu alpha-softmmu
                           arm-softmmu avr-softmmu cris-softmmu hppa-softmmu
                           i386-softmmu loongarch64-softmmu m68k-softmmu
                           microblaze-softmmu microblazeel-softmmu mips-softmmu
                           mips64-softmmu mips64el-softmmu mipsel-softmmu
                           nios2-softmmu or1k-softmmu ppc-softmmu ppc64-softmmu
                           riscv32-softmmu riscv64-softmmu rx-softmmu
                           s390x-softmmu sh4-softmmu sh4eb-softmmu
                           sparc-softmmu sparc64-softmmu tricore-softmmu
                           x86_64-softmmu xtensa-softmmu xtensaeb-softmmu
  --target-list-exclude=LIST exclude a set of targets from the default target-list

Advanced options (experts only):
  -Dmesonoptname=val       passthrough option to meson unmodified
  --cross-prefix=PREFIX    use PREFIX for compile tools, PREFIX can be blank []
  --cc=CC                  use C compiler CC [cc]
  --host-cc=CC             when cross compiling, use C compiler CC for code run
                           at build time [cc]
  --cxx=CXX                use C++ compiler CXX [c++]
  --objcc=OBJCC            use Objective-C compiler OBJCC [cc]
  --extra-cflags=CFLAGS    append extra C compiler flags CFLAGS
  --extra-cxxflags=CXXFLAGS append extra C++ compiler flags CXXFLAGS
  --extra-objcflags=OBJCFLAGS append extra Objective C compiler flags OBJCFLAGS
  --extra-ldflags=LDFLAGS  append extra linker flags LDFLAGS
  --cross-cc-ARCH=CC       use compiler when building ARCH guest test cases
  --cross-cc-cflags-ARCH=  use compiler flags when building ARCH guest tests
  --cross-prefix-ARCH=PREFIX cross compiler prefix when building ARCH guest test cases
  --python=PYTHON          use specified python [/mingw64/bin/python3]
  --ninja=NINJA            use specified ninja []
  --static                 enable static build [no]
  --without-default-features default all --enable-* options to "disabled"
  --without-default-devices  do not include any device that is not needed to
                           start the emulator (only use if you are including
                           desired devices in configs/devices/)
  --with-devices-ARCH=NAME override default configs/devices
  --enable-debug           enable common debug build options
  --cpu=CPU                Build for host CPU [x86_64]
  --disable-containers     don't use containers for cross-building
  --container-engine=TYPE  which container engine to use [auto]
  --gdb=GDB-path           gdb to use for gdbstub tests [/mingw64/bin/gdb-multiarch]
  --audio-drv-list=CHOICES Set audio driver list [default] (choices: alsa/co
                           reaudio/default/dsound/jack/oss/pa/pipewire/sdl/s
                           ndio)
  --bindir=VALUE           Executable directory [bin]
  --block-drv-ro-whitelist=VALUE
                           set block driver read-only whitelist (by default
                           affects only QEMU, not tools like qemu-img)
  --block-drv-rw-whitelist=VALUE
                           set block driver read-write whitelist (by default
                           affects only QEMU, not tools like qemu-img)
  --datadir=VALUE          Data file directory [share]
  --disable-coroutine-pool coroutine freelist (better performance)
  --disable-debug-info     Enable debug symbols and other information
  --disable-hexagon-idef-parser
                           use idef-parser to automatically generate TCG
                           code for the Hexagon frontend
  --disable-install-blobs  install provided firmware blobs
  --disable-qom-cast-debug cast debugging support
  --disable-relocatable    toggle relocatable install
  --docdir=VALUE           Base directory for documentation installation
                           (can be empty) [share/doc]
  --enable-block-drv-whitelist-in-tools
                           use block whitelist also in tools instead of only
                           QEMU
  --enable-cfi             Control-Flow Integrity (CFI)
  --enable-cfi-debug       Verbose errors in case of CFI violation
  --enable-debug-graph-lock
                           graph lock debugging support
  --enable-debug-mutex     mutex debugging support
  --enable-debug-stack-usage
                           measure coroutine stack usage
  --enable-debug-tcg       TCG debugging
  --enable-fdt[=CHOICE]    Whether and how to find the libfdt library
                           (choices: auto/disabled/enabled/internal/system)
  --enable-fuzzing         build fuzzing targets
  --enable-gcov            Enable coverage tracking.
  --enable-lto             Use link time optimization
  --enable-malloc=CHOICE   choose memory allocator to use [system] (choices:
                           jemalloc/system/tcmalloc)
  --enable-module-upgrades try to load modules from alternate paths for
                           upgrades
  --enable-rng-none        dummy RNG, avoid using /dev/(u)random and
                           getrandom()
  --enable-safe-stack      SafeStack Stack Smash Protection (requires
                           clang/llvm and coroutine backend ucontext)
  --enable-sanitizers      enable default sanitizers
  --enable-strip           Strip targets on install
  --enable-tcg-interpreter TCG with bytecode interpreter (slow)
  --enable-trace-backends=CHOICES
                           Set available tracing backends [log] (choices:
                           dtrace/ftrace/log/nop/simple/syslog/ust)
  --enable-tsan            enable thread sanitizer
  --firmwarepath=VALUES    search PATH for firmware files [share/qemu-
                           firmware]
  --iasl=VALUE             Path to ACPI disassembler
  --includedir=VALUE       Header file directory [include]
  --interp-prefix=VALUE    where to find shared libraries etc., use %M for
                           cpu name [/usr/gnemul/qemu-%M]
  --libdir=VALUE           Library directory [system default]
  --libexecdir=VALUE       Library executable directory [libexec]
  --localedir=VALUE        Locale data directory [share/locale]
  --localstatedir=VALUE    Localstate data directory [/var/local]
  --mandir=VALUE           Manual page directory [share/man]
  --prefix=VALUE           Installation prefix [/usr/local]
  --qemu-ga-distro=VALUE   second path element in qemu-ga registry entries
                           [Linux]
  --qemu-ga-manufacturer=VALUE
                           "manufacturer" name for qemu-ga registry entries
                           [QEMU]
  --qemu-ga-version=VALUE  version number for qemu-ga installer
  --smbd=VALUE             Path to smbd for slirp networking
  --sysconfdir=VALUE       Sysconf data directory [etc]
  --tls-priority=VALUE     Default TLS protocol/cipher priority string
                           [NORMAL]
  --with-coroutine=CHOICE  coroutine backend to use (choices:
                           auto/sigaltstack/ucontext/windows)
  --with-pkgversion=VALUE  use specified string as sub-version of the
                           package
  --with-suffix=VALUE      Suffix for QEMU data/modules/config directories
                           (can be empty) [qemu]
  --with-trace-file=VALUE  Trace file prefix for simple backend [trace]

Optional features, enabled with --enable-FEATURE and
disabled with --disable-FEATURE, default is enabled if available
(unless built with --without-default-features):

  af-xdp          AF_XDP network backend support
  alsa            ALSA sound support
  attr            attr/xattr support
  auth-pam        PAM access control
  avx2            AVX2 optimizations
  avx512bw        AVX512BW optimizations
  avx512f         AVX512F optimizations
  blkio           libblkio block device driver
  bochs           bochs image format support
  bpf             eBPF support
  brlapi          brlapi character device driver
  bzip2           bzip2 support for DMG images
  canokey         CanoKey support
  cap-ng          cap_ng support
  capstone        Whether and how to find the capstone library
  cloop           cloop image format support
  cocoa           Cocoa user interface (macOS only)
  colo-proxy      colo-proxy support
  coreaudio       CoreAudio sound support
  crypto-afalg    Linux AF_ALG crypto backend driver
  curl            CURL block device driver
  curses          curses UI
  dbus-display    -display dbus support
  dmg             dmg image format support
  docs            Documentations build support
  dsound          DirectSound sound support
  fuse            FUSE block device export
  fuse-lseek      SEEK_HOLE/SEEK_DATA support for FUSE exports
  gcrypt          libgcrypt cryptography support
  gettext         Localization of the GTK+ user interface
  gio             use libgio for D-Bus support
  glusterfs       Glusterfs block device driver
  gnutls          GNUTLS cryptography support
  gtk             GTK+ user interface
  gtk-clipboard   clipboard support for the gtk UI (EXPERIMENTAL, MAY HANG)
  guest-agent     Build QEMU Guest Agent
  guest-agent-msi Build MSI package for the QEMU Guest Agent
  hv-balloon      hv-balloon driver (requires Glib 2.68+ GTree API)
  hvf             HVF acceleration support
  iconv           Font glyph conversion support
  jack            JACK sound support
  keyring         Linux keyring support
  kvm             KVM acceleration support
  l2tpv3          l2tpv3 network backend support
  libdaxctl       libdaxctl support
  libdw           debuginfo support
  libiscsi        libiscsi userspace initiator
  libkeyutils     Linux keyutils support
  libnfs          libnfs block device driver
  libpmem         libpmem support
  libssh          ssh block device support
  libudev         Use libudev to enumerate host devices
  libusb          libusb support for USB passthrough
  libvduse        build VDUSE Library
  linux-aio       Linux AIO support
  linux-io-uring  Linux io_uring support
  live-block-migration
                  block migration in the main migration stream
  lzfse           lzfse support for DMG images
  lzo             lzo compression support
  malloc-trim     enable libc malloc_trim() for memory optimization
  membarrier      membarrier system call (for Linux 4.14+ or Windows
  modules         modules support (non Windows)
  mpath           Multipath persistent reservation passthrough
  multiprocess    Out of process device emulation support
  netmap          netmap network backend support
  nettle          nettle cryptography support
  numa            libnuma support
  nvmm            NVMM acceleration support
  opengl          OpenGL support
  oss             OSS sound support
  pa              PulseAudio sound support
  parallels       parallels image format support
  pipewire        PipeWire sound support
  pixman          pixman support
  plugins         TCG plugins via shared library loading
  png             PNG support with libpng
  pvrdma          Enable PVRDMA support
  qcow1           qcow1 image format support
  qed             qed image format support
  qga-vss         build QGA VSS support (broken with MinGW)
  rbd             Ceph block device driver
  rdma            Enable RDMA-based migration
  replication     replication support
  rutabaga-gfx    rutabaga_gfx support
  sdl             SDL user interface
  sdl-image       SDL Image support for icons
  seccomp         seccomp support
  selinux         SELinux support in qemu-nbd
  slirp           libslirp user mode network backend support
  slirp-smbd      use smbd (at path --smbd=*) in slirp networking
  smartcard       CA smartcard emulation support
  snappy          snappy compression support
  sndio           sndio sound support
  sparse          sparse checker
  spice           Spice server support
  spice-protocol  Spice protocol support
  stack-protector compiler-provided stack protection
  tcg             TCG support
  tools           build support utilities that come with QEMU
  tpm             TPM support
  u2f             U2F emulation support
  usb-redir       libusbredir support
  vde             vde network backend support
  vdi             vdi image format support
  vduse-blk-export
                  VDUSE block export support
  vfio-user-server
                  vfio-user server support
  vhdx            vhdx image format support
  vhost-crypto    vhost-user crypto backend support
  vhost-kernel    vhost kernel backend support
  vhost-net       vhost-net kernel acceleration support
  vhost-user      vhost-user backend support
  vhost-user-blk-server
                  build vhost-user-blk server
  vhost-vdpa      vhost-vdpa kernel backend support
  virglrenderer   virgl rendering support
  virtfs          virtio-9p support
  virtfs-proxy-helper
                  virtio-9p proxy helper support
  vmdk            vmdk image format support
  vmnet           vmnet.framework network backend support
  vnc             VNC server
  vnc-jpeg        JPEG lossy compression for VNC server
  vnc-sasl        SASL authentication for VNC server
  vpc             vpc image format support
  vte             vte support for the gtk UI
  vvfat           vvfat image format support
  werror          Treat warnings as errors
  whpx            WHPX acceleration support
  xen             Xen backend support
  xen-pci-passthrough
                  Xen PCI passthrough support
  xkbcommon       xkbcommon support
  zstd            zstd compression support
  system          all system emulation targets
  user            supported user emulation targets
  linux-user      all linux usermode emulation targets
  bsd-user        all BSD usermode emulation targets
  pie             Position Independent Executables

NOTE: The object files are built at the place where configure is launched
```

### 指定目标架构

>  使用 --target-list=<target1,target2,target3> 指定目标结构，如果不指定则编译所有目标

```shell
../configure --prefix=<安装目录> --target-list=aarch64-softmmu,arm-softmmu
```

>  这里安装目录以：C:\Users\xidon\code\github\qemu-machine\qemu-build 为例

``` shell
../configure --prefix=/C/Users/xidon/code/github/qemu-machine/qemu-build --target-list=arm-softmmu --enable-debug --disable-docs
```



## 9. 构建工程

```shell
make -j16
```



## 10. 安装二进制文件

```shell
make install
```
