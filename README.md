# smi2021

For in-tree building

## Minimal requirement

- Kernel version must be greater than or equal to 3.12

## Building

Add

	obj-$(CONFIG_VIDEO_SMI2021) += smi2021/

to *drivers/media/usb/Makefile*.

Add

	source "drivers/media/usb/smi2021/Kconfig"

under

	if MEDIA_ANALOG_TV_SUPPORT
		comment "Analog TV USB devices"

in *drivers/media/usb/Kconfig*.

Set `CONFIG_VIDEO_SMI2021` in your config and build your kernel.

## Firmware

Find the firmware for your device in the [firmware/](firmware/) directory and
send it to the kernel when the driver requests it. On most distros, udev is
configured to handle this for you if you place the firmware in a distro-specific
firmware directory.

## Module parameters

- forceasgm - default 0. If set to 1 chipset version be set as gm7113C - supported now version in saa7115 modules.
In kernel command line it look like smi2021.forceasgm=1. In build as internal module we can set VIDEO\_SMI2021\_INIT\_AS\_GM7113C, but that not recommended, as module parameter present.
- monochrome - default 0. If set to 1 on init output be in monochrome mode only.
- chiptype - default 0. You can skip autodetection and set chip version directly: 1 = gm7113C, 2 = saa7113.
    - NOTICE: if you set 1 (gm7113C) - saa7115 module can incorrectly detect you chip, because now present some change in registry out and that part in under development. For module work as before - **NEED** use `forceasgm=1`
    - NOTICE: chiptype **NOT** override version for other kernel module. For module work as before - **NEED** use `forceasgm=1`

## Troubleshooting

- monochrome output or no output - you need check, what `saa7115` module proper init you device (need once on first install, or new linux distrib, or with new smi2021 device(for proper check what they detected correct)).
For that check, you need
    1. load saa7115 modules with debug: ```modprobe -r saa7115; modprobe saa7115 debug=1```
    2. load smi2021 and check dmesg: must be string like: ```saa7115 8-004a: gm7113c found @ 0x94 (smi2021)```
If detection not work, or work notproper - try use `forceasgm=1` module option. For build as part of kernel and use override - be added additional options in Kconfig.


## Credits

David Manouchehri - david@davidmanouchehri.com

Jon Arne Jørgensen - jonjon.arnearne@gmail.com

mastervolkov - mastervolkov@gmail.com

https://github.com/Manouchehri/smi2021

https://github.com/jonjonarnearne/smi2021
