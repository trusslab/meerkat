#!/bin/bash

# a one liner to test that a kernel build will boot properly. I stole it from google/syzkaller.
# change the kernel and image path as needed.

fromsyzargs="console=ttyS0 root=/dev/sda"
needargs="$fromsyzargs earlyprintk=serial net.ifnames=0"
defaultargs="$needargs debug slub_debug=QUZ panic_on_warn=1 oops=panic panic=1" 
args="$defaultargs nmi_watchdog=panic ftrace_dump_on_oops=orig_cpu rodata=n vsyscall=native biosdevname=0"
kvmargs="$args kvm-intel.nested=1 kvm-intel.unrestricted_guest=1 kvm-intel.vmm_exclusive=1 kvm-intel.fasteoi=1 kvm-intel.ept=1 kvm-intel.flexpriority=1 kvm-intel.vpid=1 kvm-intel.emulate_invalid_guest_state=1 kvm-intel.eptad=1 kvm-intel.enable_shadow_vmcs=1 kvm-intel.pml=1 kvm-intel.enable_apicv=1"

crashargs="earlyprintk=serial oops=panic nmi_watchdog=panic panic_on_warn=1 panic=1 ftrace_dump_on_oops=orig_cpu rodata=n vsyscall=native net.ifnames=0 biosdevname=0 root=/dev/sda console=ttyS0 kvm-intel.nested=1 kvm-intel.unrestricted_guest=1 kvm-intel.vmm_exclusive=1 kvm-intel.fasteoi=1 kvm-intel.ept=1 kvm-intel.flexpriority=1 kvm-intel.vpid=1 kvm-intel.emulate_invalid_guest_state=1 kvm-intel.eptad=1 kvm-intel.enable_shadow_vmcs=1 kvm-intel.pml=1 kvm-intel.enable_apicv=1"
image=/mnt/sda/jtbursey/SyzInspector/image/stretch/stretch.img
kernel=/mnt/sda/jtbursey/SyzInspector/wd-inspector-8/kernel/arch/x86/boot/bzImage

# Debugged:
# qemu-system-x86_64 -m 2G -smp 2 -display none -serial stdio -no-reboot -enable-kvm -cpu host -net nic,model=e1000 -net user,host=10.0.2.10,hostfwd=tcp::1569-:22 -hda $image -snapshot -kernel $kernel -append "$crashargs"

# Raw from Syzkaller:
qemu-system-x86_64 -m 4096 -smp 2 -display none -serial stdio -no-reboot -enable-kvm -cpu host,migratable=off -net nic,model=e1000 -net user,host=10.0.2.10,hostfwd=tcp::12000-:22 -hda $image -snapshot -kernel $kernel -append "$crashargs"

# qemu runs on port 12000
# ssh -i image/stretch/stretch.id_rsa -p 12000 -o "StrictHostKeyChecking no" root@localhost

# To reproduce a crash using a syz reproducer:
# Copy syz-execprog, syz-executor, and the POC into the VM
# scp -P 12000 -i image/stretch/stretch.id_rsa wd/syzkaller/bin/linux_amd64/syz-execprog wd/syzkaller/bin/linux_amd64/syz-executor wd/repro-bug#.prog  root@localhost:/root
# Inside the VM, run syz-execprog
# ./syz-execprog -executor=/root/syz-executor -threaded -collide -repeat=0 -procs=8 -slowdown=1 -disable=close_fds repro-bug#.prog
