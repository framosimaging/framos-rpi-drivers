#!/bin/bash

# This script builds and install: kernel Image, kernel modules & DTBs


# color output
red=$'\e[31m'
grn=$'\e[32m'
yel=$'\e[33m'
blu=$'\e[34m'
mag=$'\e[35m'
cyn=$'\e[36m'
normal=$'\e[0m'

# check for exported variables
if [ -z "${LINUX_DIR}" ]; then
    echo "Location of the Raspberry Pi source code does not exist! Please export location of the LINUX_DIR."
    return
fi

# check for directories
if [ ! -d "${LINUX_DIR}/arch/arm64" ]; then
    echo "${red}Invalid directory${normal} $LINUX_DIR."
    echo "Please export location of the Raspberry Pi ${red}\"linux\"${normal} directory as a ${yel}LINUX_DIR${normal} variable."
    return
fi
    
# export env
export KERNEL="kernel_2712"

NPROC=$(($(nproc) * 15/10))
ROOTFS="/media/$(whoami)/rootfs"
BOOTFS="/media/$(whoami)/bootfs"
BUILD_MODULES="${LINUX_DIR}/drivers/media/i2c"
BUILD_DTBS="${LINUX_DIR}/arch/arm64/boot/dts/overlays"
BUILD_BROADCOM_DTBS="${LINUX_DIR}/arch/arm64/boot/dts/broadcom"
BUILD_IMAGE="${LINUX_DIR}/arch/arm64/boot"
MEDIA_DTBS="${BOOTFS}/overlays"

# build kernel defconfig
function build_defconfig() {(
    set -e
    pushd ${LINUX_DIR} &> /dev/null
    echo ; echo; echo -e "${yel}start defconfig build ... ${normal}"
    make -j${NPROC} ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="-framos" bcm2712_defconfig
    popd &> /dev/null
)}

# build kernel Image
function build_kernel() {(
    set -e
    pushd ${LINUX_DIR} &> /dev/null
    echo ; echo; echo -e "${yel}start kernel Image build ... ${normal}"
    make -j${NPROC} ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="-framos" Image
    popd &> /dev/null
)}

# build kernel modules
function build_modules() {(
    set -e
    pushd ${LINUX_DIR} &> /dev/null
    echo ; echo; echo -e "${yel}start kernel modules build ... ${normal}"
    make -j${NPROC} ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="-framos" modules
    popd &> /dev/null
)}

# build DTBs
function build_dtree() {(
    set -e
    pushd ${LINUX_DIR} &> /dev/null
    echo ; echo; echo -e "${yel}start DTBs build ... ${normal}"
    make -j${NPROC} ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION="-framos" dtbs
    popd &> /dev/null
)}

# build defconfig, kernel Image, kernel modules, DTBs
function build_all() {(
    set -e
    build_defconfig
    build_kernel
    build_modules
    build_dtree
)}

# clean all output-binary files
function clean_all() {(
	pushd ${LINUX_DIR} &> /dev/null
	echo ; echo; echo -e "${yel}cleaning ... ${normal}"
	sudo make -j${NPROC} ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- clean
	popd &> /dev/null
)}

# Install kernel Image
function install_image() {(
    set -e
    
    # Check if Raspberry Pi boot media is inserted
    if [ ! -d ${ROOTFS} ] ; then
		echo -e "${red}Can't find Raspberry Pi boot media${normal}"
		return
	fi
	
    pushd ${BUILD_IMAGE} &> /dev/null
    if [ -z "$(find . -maxdepth 1 -name "Image" -print -quit)" ]; then
    	echo -e "${red}Can't locate kernel Image!${normal}"
    	echo -e "${red}Make sure the build process has been completed!${normal}"
    	popd &> /dev/null
    	return
    fi
	
	pushd ${BUILD_IMAGE} &> /dev/null
	echo ; echo; echo -e "${yel}kernel Image install ... ${normal}"
	sudo cp -fv Image "${BOOTFS}/${KERNEL}.img"
	popd &> /dev/null
	
)}

# Install kernel modules
function install_modules() {(
    set -e
    
    # Check if Raspberry Pi boot media is inserted
    if [ ! -d ${ROOTFS} ] ; then
		echo -e "${red}Can't find Raspberry Pi boot media${normal}"
		return
	fi
	
    pushd ${BUILD_MODULES} &> /dev/null
    if [ -z "$(find . -maxdepth 1 -name "fr_*.ko" -print -quit)" ]; then
    	echo -e "${red}Can't locate FRAMOS kernel modules!${normal}"
    	echo -e "${red}Make sure the build process has been completed!${normal}"
    	popd &> /dev/null
    	return
    fi
	
	pushd ${LINUX_DIR} &> /dev/null
	echo ; echo; echo -e "${yel}kernel modules install ... ${normal}"
	sudo make -j${NPROC} ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=${ROOTFS} modules_install
	popd &> /dev/null
	
)}

# Install DTBs
function install_dtree() {(
    set -e
    
    # Check if Raspberry Pi boot media is inserted
    if [ ! -d ${BOOTFS} ]; then
		echo -e "${red}Can't find Raspberry Pi boot media${normal}"
		return
	fi
	
	# Copy DTBs to Raspberry Pi boot media
    pushd ${BUILD_DTBS} &> /dev/null
    if [ -z "$(find . -maxdepth 1 -name "fr_*.dtbo" -print -quit)" ]; then
    	echo -e "${red}Can't locate FRAMOS device trees!${normal}"
    	echo -e "${red}Make sure the build process has been completed!${normal}"
    	popd &> /dev/null
    	return
    fi
    echo ; echo; echo -e "${yel}device tree install ... ${normal}"
    sudo cp -fv *.dtb* ${MEDIA_DTBS}
    sudo cp README ${MEDIA_DTBS}
    popd &> /dev/null
    
    pushd ${BUILD_BROADCOM_DTBS} &> /dev/null
    sudo cp -fv *.dtb ${BOOTFS}
    popd &> /dev/null
    
)}

# Install kernel modules and DTBs
function install_all() {(
    set -e
    # Check if Raspberry Pi boot media is inserted
    if [ ! -d ${ROOTFS} ] || [ ! -d ${BOOTFS} ]; then
		echo -e "${red}Can't find Raspberry Pi boot media${normal}"
		return
	fi
	install_image
    install_modules
    install_dtree
    unmount_media
)}

function unmount_media() {(
    set -e
    # Check if Raspberry Pi boot media is inserted
    if [ ! -d ${ROOTFS} ] || [ ! -d ${BOOTFS} ]; then
		echo -e "${red}Can't find Raspberry Pi boot media${normal}"
		return
	fi
	echo ; echo; echo -e "${yel}unmounting Raspberry Pi media device ${normal}"
    sudo umount ${ROOTFS}
    sudo umount ${BOOTFS}    
)}

echo; echo; echo -e "${grn}LINUX_DIR = ${LINUX_DIR}"


echo; echo "${cyn}C O M M A N D S:"

echo -e "${red}build_defconfig${normal}:\t\tbuild defconfig to configure the kernel build"
echo -e "${red}build_kernel${normal}:	\t\tbuild kernel Image & kernel modules"
echo -e "${red}build_modules${normal}:	\t\tbuild kernel modules"
echo -e "${red}build_dtree${normal}:	\t\tbuild device tree & device tree overlays"
echo -e "${red}build_all${normal}:	\t\tbuild all"
echo -e "${red}install_kernel${normal}:      \t\tinstall kernel Image on media device"
echo -e "${red}install_modules${normal}:\t\tinstall kernel modules on media device"
echo -e "${red}install_dtree${normal}:	\t\tinstall device tree on media device"
echo -e "${red}install_all${normal}:	\t\tinstall device tree and modules on media device"
echo -e "${red}clean_all${normal}:	\t\tclean all"
echo -e "${red}unmount_media${normal}:	\t\tunmount Raspberry Pi media device"

echo
