
<h1 align="center">
  <br>
  <img src="https://raw.githubusercontent.com/Gojikovi/kernel_samsung_universal9611/Grass-Unified/img/logo.jpg" alt="Markdownify" width="200">
  <br>
  GrassKernel
  <br>
</h1>

<h4 align="center">A custom kernel for the Exynos9611 devices.</h4>

<p align="center">
  <a href="#key-features">Key Features</a> •
  <a href="#how-to-build">How To Build</a> •
  <a href="#how-to-flash">How To Flash</a> •
  <a href="#credits">Credits</a>
</p>

## Key Features

* Disable Samsung securities, debug drivers, etc modifications
* Checkout and rebase against Android common kernel source, Removing Samsung additions to drivers like ext4,f2fs and more
* Compiled with bleeding edge Neutron Clang 17, with full LLVM binutils, LTO (Link time optimization) and -O3  
* Import Erofs, Incremental FS, BinderFS and several backports.
* Supports DeX touchpad for corresponding OneUI ports that have DeX ported.
* Lot of debug codes/configuration Samsung added are removed.
* Added [wireguard](https://www.wireguard.com/) driver, an open-source VPN driver in-kernel
* Added [KernelSU](https://kernelsu.org/)

## How To Build

You will need ubuntu, git, around 8GB RAM and bla-bla-bla...

```bash
# Install dependencies
$ sudo apt install -y bash git make libssl-dev curl bc pkg-config m4 libtool automake autoconf

# Clone this repository
$ git clone https://github.com/Gojikovi/kernel_samsung_universal9611

# Go into the repository
$ cd kernel_samsung_universal9611

# Install toolchain
$ ./download_toolchain.sh 

# If you want to compile the kernel not for A51 then change the 23 line in build_kernel.sh to f41, m21, m31, m31s
# Build the kernel
$ ./build_kernel.sh aosp ksu (for AOSP)
$ ./build_kernel.sh oneui ksu (for OneUI)
```

After build the image of the kernel will be in out/arch/arm64/boot/Image

## How To Flash

To install the kernel use AnyKernel3.
1. Download [this](https://github.com/Exynos9611-Development/kernel_samsung_universal9611/releases/download/4.14.150-Grass-5/Grass-AOSP_20230324.zip) kernel
2. Unzip
3. Delete the old Image file
4. And move the new Image file
5. Zip that all
6. Flash via TWRP or adb sideload

## Credits

This software uses the following open source packages:

- [roynatech2544](https://github.com/roynatech2544)
- [Samsung Open Source](https://opensource.samsung.com/)
- [Android Open Source Project](https://source.android.com/)
- [The Linux Kernel](https://www.kernel.org/)


