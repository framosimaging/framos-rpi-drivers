KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

SRCS := $(filter-out $(M)/drivers/%.mod.c, $(wildcard $(M)/drivers/*.c))
OBJS := $(SRCS:$(M)/drivers/%.c=drivers/%.o)
obj-m += $(OBJS)

DTS_DIR := ./overlays
PREPROCESSED_DIR := ./overlays/preprocessed
DTS_FILES := $(wildcard $(DTS_DIR)/*.dts)
PREPROCESSED_FILES := $(DTS_FILES:$(DTS_DIR)/%.dts=$(PREPROCESSED_DIR)/%-preprocessed.dts)
DTB_FILES := $(DTS_FILES:$(DTS_DIR)/%-overlay.dts=$(DTS_DIR)/%.dtbo)

NPROC := $(shell echo $$((`nproc` * 15/10)))

modules:
	@make -j $(NPROC) -C $(KDIR) M=$(PWD) modules
	
dtbs: $(DTB_FILES)

$(PREPROCESSED_DIR)/%-preprocessed.dts: $(DTS_DIR)/%-overlay.dts
	@mkdir -p $(PREPROCESSED_DIR)
	@cpp -nostdinc -I include -undef -x assembler-with-cpp $< > $@

$(DTS_DIR)/%.dtbo: $(PREPROCESSED_DIR)/%-preprocessed.dts
	@echo "Compiling $@"
	@dtc -q -@ -I dts -O dtb -o $@ $<
	@rm -rf $(PREPROCESSED_DIR)

modules_install:
	@sudo -E make -j $(NPROC) -C $(KDIR) M=$(PWD) modules_install
	@sudo -E depmod -a

dtbs_install:
	@echo "Installing dtbs:"
	@sudo -E cp -v overlays/fr_*.dtbo /boot/firmware/overlays

clean:
	@make -C $(KDIR) M=$(PWD) clean
	@rm -f $(DTS_DIR)/*.dtbo

