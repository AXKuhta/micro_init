# micro_init
A tiny init binary intended to boot Ubuntu Base from an ext2 .img file on a FAT32 usb flash drive

![Booting into bash and executing some commands](micro_init.PNG)

---

Required filesystem structure:

```
dev             Folder for the kernel to mount devtmpfs
newroot         Folder where we will mount the ext2 image
ext2.img        Ext2 image produced using mke2fs
bzImage         Your kernel image
micro_init
```

Kernel command line:

```
root=/dev/sda1 rw rootwait init=/micro_init
```

The kernel needs to have all the device drivers built-in; this init will do nothing to load modules.

---

Ordinarily you can't boot a Linux Distro from a FAT32 filesystem, at least not Ubuntu or Debian -like. FAT32 doesn't have POSIX access control bits and doesn't support symlinks. While you _maybe_ can do without proper access control, symlinks are really crucial. Every ELF executable in Ubuntu needs to load a linker library:

```bash
$ file /usr/bin/bash
/usr/bin/bash: ELF 64-bit LSB shared object, x86-64, version 1 (SYSV), dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, BuildID[sha1]=a6cb40078351e05121d46daa768e271846d5cc54, for GNU/Linux 3.2.0, stripped
```

And `/lib64/` folder is a symlink!:

```
$ ls -lah /
...
lrwxrwxrwx  1 root root    7 Aug  5  2020 lib -> usr/lib
lrwxrwxrwx  1 root root    9 Aug  5  2020 lib32 -> usr/lib32
lrwxrwxrwx  1 root root    9 Aug  5  2020 lib64 -> usr/lib64
lrwxrwxrwx  1 root root   10 Aug  5  2020 libx32 -> usr/libx32
...
```

So, there's just no hope of running Ubuntu from a FAT32 filesystem. But what if we could pack it into an ext2 image, store the image on the FAT32 filesystem, mount it as a loop device and boot from it instead?

That's exactly what this init binary does:

1. Get a loop device
2. Associate ext2.img with the loop device
3. Mount the loop into /newroot
4. Bind /dev and mount some sysfs folders into /newroot
5. Chroot into /newroot
6. Start getty on tty2 to tty12
7. Start bash on initial console

This gets you a somewhat usable system.

I use [ubuntu-base-21.04-base-amd64.tar.gz](http://cdimage.ubuntu.com/ubuntu-base/releases/21.04/release/)

```
mkdir ubuntu
cd ubuntu
wget http://cdimage.ubuntu.com/ubuntu-base/releases/21.04/release/ubuntu-base-21.04-base-amd64.tar.gz
tar -xf ubuntu-base-20.04.1-base-amd64.tar.gz
cd ..
mke2fs -t ext2 -d ubuntu/ "ext2.img" 1G
```

You will likely want to have `dhclient` and `ip`, but the Ubuntu Base lacks them. [Here](https://github.com/AXKuhta/micro_init/releases/download/v0.1/ubuntu_21.04_net_packages.tar) is a bunch of .deb files that you can add into the image and install with `apt install ./*.deb`.

---

Thanks to https://github.com/pranith/nolibc; it helped me figure out how to print stuff in nolibc.
