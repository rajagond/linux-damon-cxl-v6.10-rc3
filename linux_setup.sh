#!/bin/bash

sudo apt-get install git fakeroot build-essential ncurses-dev xz-utils libssl-dev bc flex libelf-dev bison -y
git clone git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git
cd linux
git checkout v6.10-rc3

git apply --stat --apply damon_cxl_2_tier_patch_files/damon_cxl_v6_1.patch
git apply --stat --apply damon_cxl_2_tier_patch_files/damon_cxl_v6_2.patch
git apply --stat --apply damon_cxl_2_tier_patch_files/damon_cxl_v6_3.patch
git apply --stat --apply damon_cxl_2_tier_patch_files/damon_cxl_v6_4.patch
git apply --stat --apply damon_cxl_2_tier_patch_files/damon_cxl_v6_5.patch
git apply --stat --apply damon_cxl_2_tier_patch_files/damon_cxl_v6_6.patch
git apply --stat --apply damon_cxl_2_tier_patch_files/damon_cxl_v6_7.patch
make menuconfig # check damon -- memory management -> damon and drivers -> uncore freq

cat .config | grep -n "canonical"
vim .config # comment out the security flags

make -j$(nproc)
make modules
sudo make modules_install
sudo make install