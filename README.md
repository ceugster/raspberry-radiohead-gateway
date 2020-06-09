# raspberry-radiohead-gateway
Raspberry Pi Gateway with Dragino LoRA/GPS Hat and RadioHead

A Raspberry Pi Gateway based on the RadioHead library. Customized for an environment where LoraWAN would have been oversized.

As this thread https://www.raspberrypi.org/forums/viewtopic.php?t=224035 shows, there exists a problem with raspberry, dragino lora/gps hat and radiohead. A solution is given at https://github.com/raspberrypi/linux/issues/2550#issuecomment-398412562:

1. Update your Raspberry Pi OS
2. Check if /boot/overlays/gpio-no-irq.dtbo exists. If not follow the instructions at https://github.com/raspberrypi/linux/issues/2550#issuecomment-398412562
3. Add in /boot/config.txt the line dtoverlay=gpio-no-irq
4. Reboot

That should do it
