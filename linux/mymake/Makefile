CURRENT_DIR := $(shell pwd)

KERNEL := ../arch/x86/boot/bzImage
INITRD := ../../initramfs.cpio.gz

BACKUP_DIR := /tmp/linux-parfs-tmp
BACKUP_PATH := $(BACKUP_DIR)/linux-parfs.bak
BACKUP_FS := $(BACKUP_DIR)/fs

MODULES_INSTALL_PREFIX := $(HOME)/code/linux-parfs-mods
KERNEL_INSTALL_PREFIX := $(HOME)/code/linux-parfs-kernel

BUILD_THREADS := 32

VM_PORT := 23456

VM_HOST := root@localhost
VM_PASS := 

QFLAGS := -nographic 
QFLAGS += -kernel $(KERNEL)  
QFLAGS += -initrd $(INITRD)  
QFLAGS += -append "console=ttyS0" 
QFLAGS += -serial mon:stdio  
QFLAGS += -cpu host
QFLAGS += -enable-kvm

KCFLAGS := -DPARFS

KERNEL_SUFFIX := -parfs

SUDO_MODE:=echo $(VM_PASS) | sudo -S echo
INSTALL_SCRIPT:=$(SUDO_MODE);\
				 cd ~; \
				 sudo rm -rf pkgs; \
				 sudo mkdir -p pkgs; \
				 sudo tar xf dummyFS.tar; \
				 sudo rm -rf dummyFS.tar; \
				 sudo mv kernel_pkgs.tar module_pkgs.tar pkgs; \
				 cd pkgs; \
				 sudo tar xf kernel_pkgs.tar; \
				 sudo tar xf module_pkgs.tar; \
				 sudo cp -f config-6.1.56+ System.map-6.1.56+ vmlinuz-6.1.56+ /boot; \
				 sudo cp -rf lib/modules/6.1.56+ /lib/modules; \
				 sudo update-grub; \
				 cd ..; \
				 sudo rm -rf pkgs; \
				 sudo reboot;

build:
	# cd .. && make LOCALVERSION="$(KERNEL_SUFFIX)" -j$(BUILD_THREADS)
	cd .. && make LOCALVERSION="$(KERNEL_SUFFIX)" "KCFLAGS+=$(KCFLAGS)" -j$(BUILD_THREADS)
		
build-vm:
	cd .. && make LOCALVERSION="$(KERNEL_SUFFIX)" -j$(BUILD_THREADS)
# cd .. && make "KCFLAGS+=-DWSP_QEMU $(KCFLAGS)" -j$(BUILD_THREADS)

# run: build-vm
run: 
	qemu-system-x86_64 $(QFLAGS)

# run: build
# 	qemu-system-x86_64 $(QFLAGS)

debug:
	qemu-system-x86_64 $(QFLAGS) -s -S

build-deb:
	rm -rf $(BACKUP_DIR)
	mkdir -p $(BACKUP_DIR)
	cp -r ../../linux $(BACKUP_PATH)
	cp -r ../../fs $(BACKUP_FS)
	cd $(BACKUP_PATH) && rm -rf .clangd
	cd $(BACKUP_PATH) && rm -rf .cache
	cd $(BACKUP_PATH) && rm -rf vmlinux-gdb.py
	cd $(BACKUP_PATH) && rm -rf compile_commands.json
	cd $(BACKUP_PATH) && make bindeb-pkg -j$(BUILD_THREADS)
	cp $(BACKUP_DIR)/*.deb $(CURRENT_DIR)

modules_install:
	rm -rf $(MODULES_INSTALL_PREFIX)
	mkdir -p $(MODULES_INSTALL_PREFIX)
	cd .. && INSTALL_MOD_PATH=$(MODULES_INSTALL_PREFIX) make INSTALL_MOD_STRIP=1 modules_install -j$(BUILD_THREADS)

install:
	mkdir -p $(KERNEL_INSTALL_PREFIX)
	cd .. && INSTALL_PATH=$(KERNEL_INSTALL_PREFIX) make install

# Install modules
vm-modules: build-vm modules_install 
	cd $(MODULES_INSTALL_PREFIX) && rm -rf module_pkgs.tar 
	cd $(MODULES_INSTALL_PREFIX) && tar cf module_pkgs.tar *
	cd $(MODULES_INSTALL_PREFIX) && mv module_pkgs.tar $(CURRENT_DIR)

# Only install kernel
vm-kernel: build-vm install
	cd $(KERNEL_INSTALL_PREFIX) && rm -rf kernel_pkgs.tar
	cd $(KERNEL_INSTALL_PREFIX) && tar cf kernel_pkgs.tar *
	cd $(KERNEL_INSTALL_PREFIX) && mv kernel_pkgs.tar $(CURRENT_DIR)

vm-all: vm-kernel vm-modules
	scp -P $(VM_PORT) kernel_pkgs.tar  $(VM_HOST):~
	scp -P $(VM_PORT) module_pkgs.tar  $(VM_HOST):~
# sshpass -p $(VM_PASS) scp -P $(VM_PORT) kernel_pkgs.tar  $(VM_HOST):~
# sshpass -p $(VM_PASS) scp -P $(VM_PORT) module_pkgs.tar  $(VM_HOST):~
# sshpass -p $(VM_PASS) ssh -p $(VM_PORT) $(VM_HOST) "$(INSTALL_SCRIPT)"

vm-install:
	ssh -p $(VM_PORT) $(VM_HOST) "$(INSTALL_SCRIPT)"

modules: build modules_install 
	cd $(MODULES_INSTALL_PREFIX) && rm -rf module_pkgs.tar 
	cd $(MODULES_INSTALL_PREFIX) && tar cf module_pkgs.tar *
	cd $(MODULES_INSTALL_PREFIX) && mv module_pkgs.tar $(CURRENT_DIR)

kernel: build install
	cd $(KERNEL_INSTALL_PREFIX) && rm -rf kernel_pkgs.tar
	cd $(KERNEL_INSTALL_PREFIX) && tar cf kernel_pkgs.tar *
	cd $(KERNEL_INSTALL_PREFIX) && mv kernel_pkgs.tar $(CURRENT_DIR)

phy-all: kernel modules
