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

# build kernel modules
function build_modules() {(
    set -e
    pushd "../" &> /dev/null
    echo ; echo; echo -e "${yel}start kernel modules build ... ${normal}"
    make modules
    popd &> /dev/null
)}

# build DTBs
function build_dtree() {(
    set -e
	pushd "../" &> /dev/null
    echo ; echo; echo -e "${yel}start DTBs build ... ${normal}"
    make dtbs
    popd &> /dev/null
)}

# build kernel modules, DTBs
function build_all() {(
    set -e
    build_modules
    build_dtree
)}

# Install kernel modules
function install_modules() {(
    set -e
	pushd "../drivers" &> /dev/null
    if [ -z "$(find . -name "fr_*.ko" -print -quit)" ]; then
    	echo -e "${red}Can't locate FRAMOS kernel modules!${normal}"
    	echo -e "${red}Make sure the build process has been completed!${normal}"
    	popd &> /dev/null
    	return
    fi
	
	pushd "../" &> /dev/null
	echo ; echo; echo -e "${yel}kernel modules install ... ${normal}"
	sudo make modules_install
	popd &> /dev/null

)}

# Install DTBs
function install_dtree() {(
    set -e
    pushd "../overlays" &> /dev/null
    if [ -z "$(find . -name "fr_*.dtbo" -print -quit)" ]; then
    	echo -e "${red}Can't locate FRAMOS device trees!${normal}"
    	echo -e "${red}Make sure the build process has been completed!${normal}"
    	popd &> /dev/null
    	return
    fi
    
    pushd "../" &> /dev/null
    echo ; echo; echo -e "${yel}device tree install ... ${normal}"
    make dtbs_install
    popd &> /dev/null
)}

# Install kernel modules and DTBs
function install_all() {(
    set -e
    install_modules
    install_dtree
)}

# Clean
function clean_all() {(
    set -e
    pushd "../" &> /dev/null
    echo ; echo; echo -e "${yel}cleaning ... ${normal}"
    make clean
    popd &> /dev/null
)}

echo; echo "${cyn}C O M M A N D S:"

echo -e "${red}build_modules${normal}:	\t\tbuild kernel modules"
echo -e "${red}build_dtree${normal}:	\t\tbuild device tree & device tree overlays"
echo -e "${red}build_all${normal}:	\t\tbuild all"
echo -e "${red}install_modules${normal}:\t\tinstall kernel modules on media device"
echo -e "${red}install_dtree${normal}:	\t\tinstall device tree on media device"
echo -e "${red}install_all${normal}:	\t\tinstall device tree and modules on media device"
echo -e "${red}clean_all${normal}:	\t\tclean all"

echo
