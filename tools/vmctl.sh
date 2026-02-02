#!/bin/bash

# a one liner to test that a kernel build will boot properly. I stole the args from google/syzkaller.

printhelp() {
    echo "Usage: ./vmctl.sh -[k|sc] -[p]"
    echo "    -k : the directory containing the kernel to boot (relative to home)"
    echo "    -s : ssh into an already booted vm"
    echo "    -c : scp a file into the home directory of the vm (/root/)"
    echo "    -r : command to run in the guest"
    echo "    -p : the port to host the vm on, or to ssh into"
}

# ===
# Change these to point to your own directories
# ===

home="/mnt/sdd/author/meerkat/"

# this is the directory housing the images, so my bullseye is at .../images/bullseye-sift/bullseye.img
defaultImagedir="/mnt/sdd/author/meerkat/image/"
imagedir="stretch/"
imagename="stretch"

# change if conflicts. also can set with -p
port=12000

# ===

si=0 # "ssh into" flag
c=0 # scp flag
r=0
copyfile=""
imagedir="${defaultImagedir}${imagedir}"

while getopts "k:i:c:p:r:sh" flag
do
    case $flag in
        k)
            k="${home}${OPTARG}" ;;
        i)
            imagedir="${defaultImagedir}${OPTARG}" ;;
        p)
            port=${OPTARG} ;;
        s)
            si=1 ;;
        c)
            c=1
            copyfile="${OPTARG}" ;;
        r)
            r=1
            cmd="\"${OPTARG}\"" ;;
        h)
            printhelp
            exit ;;
        *)
            printhelp
            exit
    esac
done

if (( ${c} == 1 )); then
    scp -P ${port} -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i ${imagedir}${imagename}.id_rsa ${copyfile} root@localhost:/root
fi

if (( ${si} == 1 )); then
    ssh -i ${imagedir}${imagename}.id_rsa -p ${port} -o "StrictHostKeyChecking no" root@localhost
    exit
fi

if (( ${r} == 1 )); then
    ssh -i ${imagedir}${imagename}.id_rsa -p ${port} -o "StrictHostKeyChecking no" root@localhost ${cmd}
    exit
fi

if (( ${c} == 1 )); then
    exit
fi

if [[ ${k} == "" ]]; then
    echo "No kernel given."
    exit
fi

fromsyzargs="console=ttyS0 root=/dev/sda"
needargs="$fromsyzargs earlyprintk=serial net.ifnames=0"
defaultargs="$needargs debug slub_debug=QUZ panic_on_warn=1 oops=panic panic=1" 
args="$defaultargs nmi_watchdog=panic ftrace_dump_on_oops=orig_cpu rodata=n vsyscall=native biosdevname=0"
kvmargs="$args kvm-intel.nested=1 kvm-intel.unrestricted_guest=1 kvm-intel.vmm_exclusive=1 kvm-intel.fasteoi=1 kvm-intel.ept=1 kvm-intel.flexpriority=1 kvm-intel.vpid=1 kvm-intel.emulate_invalid_guest_state=1 kvm-intel.eptad=1 kvm-intel.enable_shadow_vmcs=1 kvm-intel.pml=1 kvm-intel.enable_apicv=1"

# panic_on_warn=1

crashargs="earlyprintk=serial oops=panic nmi_watchdog=panic panic=1 ftrace_dump_on_oops=orig_cpu rodata=n vsyscall=native net.ifnames=0 biosdevname=0 root=/dev/sda console=ttyS0 kvm-intel.nested=1 kvm-intel.unrestricted_guest=1 kvm-intel.vmm_exclusive=1 kvm-intel.fasteoi=1 kvm-intel.ept=1 kvm-intel.flexpriority=1 kvm-intel.vpid=1 kvm-intel.emulate_invalid_guest_state=1 kvm-intel.eptad=1 kvm-intel.enable_shadow_vmcs=1 kvm-intel.pml=1 kvm-intel.enable_apicv=1"
image=${imagedir}${imagename}.img
kernel=${k}/arch/x86/boot/bzImage

# Debugged:
# qemu-system-x86_64 -m 2G -smp 2 -display none -serial stdio -no-reboot -enable-kvm -cpu host -net nic,model=e1000 -net user,host=10.0.2.10,hostfwd=tcp::1569-:22 -hda $image -snapshot -kernel $kernel -append "$crashargs"

# Raw from Syzkaller:
qemu-system-x86_64 -m 4096 -smp 2,sockets=2,cores=1 -display none -serial stdio -no-reboot -enable-kvm -cpu host,migratable=off -net nic,model=e1000 -net user,host=10.0.2.10,hostfwd=tcp::${port}-:22 -hda ${image} -snapshot -kernel ${kernel} -append "${crashargs}"

# qemu runs on port 12000
# ssh -i ${LOOM_IMAGE}bullseye/bullseye.id_rsa -p 12000 -o "StrictHostKeyChecking no" root@localhost

# To reproduce a crash using a syz reproducer:
# Copy syz-execprog, syz-executor, and the POC into the VM
# scp -P 12000 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i ${LOOM_IMAGE}bullseye/bullseye.id_rsa syzkaller/bin/linux_amd64/syz-execprog syzkaller/bin/linux_amd64/syz-executor bugs/#/repro.syz root@localhost:/root
# Inside the VM, run syz-execprog
# ./syz-execprog -executor=/root/syz-executor -threaded -collide -repeat=0 -procs=8 -slowdown=1 -disable=close_fds repro.syz

# My current copy:
# scp -P 12000 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -i ${LOOM_IMAGE}bullseye/bullseye.id_rsa local/files/ root@localhost:/root
