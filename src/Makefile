
GIT_TOP ?= $(shell git rev-parse --show-toplevel)

INSTALL_DIR := $(GIT_TOP)/install
BUILD_DIR   := $(GIT_TOP)/build
MODS_DIR    := $(GIT_TOP)/src

EXTRA_CFLAGS := 
MODS := thingy

# Install Headers

HDRS_INSTALL_TARS := $(addprefix hdrs.install.,$(MODS))

.PHONY: hdrs.install $(HDRS_INSTALL_TARS)
hdrs.install: $(HDRS_INSTALL_TARS)

$(HDRS_INSTALL_TARS): hdrs.install.%:
	make -C $(MODS_DIR)/$* hdrs.install

# Install Libs

LIB_INSTALL_TARS := $(addprefix lib.install.,$(MODS))
.PHONY: lib.unsafe_install lib.install $(LIB_INSTALL_TARS)

lib.unsafe_install: $(LIB_INSTALL_TARS)
$(LIB_INSTALL_TARS): lib.install.%:
	make -C $(MODS_DIR)/$* lib.install

lib.install: hdrs.install
	make -C $(MODS_DIR) lib.unsafe_install

# Install Test Headers

TEST_HDRS_INSTALL_TARS := $(addprefix test_hdrs.install.,$(MODS))

.PHONY: test_hdrs.install $(TEST_HDRS_INSTALL_TARS)
test_hdrs.install: $(TEST_HDRS_INSTALL_TARS)

$(TEST_HDRS_INSTALL_TARS): test_hdrs.install.%:
	make -C $(MODS_DIR)/$* test_hdrs.install

# Install Test Libs

TEST_LIB_INSTALL_TARS := $(addprefix test_lib.install.,$(MODS))
.PHONY: test_lib.unsafe_install test_lib.install $(TEST_LIB_INSTALL_TARS)

test_lib.unsafe_install: $(TEST_LIB_INSTALL_TARS)
$(TEST_LIB_INSTALL_TARS): test_lib.install.%:
	make -C $(MODS_DIR)/$* test_lib.install

test_lib.install: hdrs.install
	make -C $(MODS_DIR) test_lib.unsafe_install








