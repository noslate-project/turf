BUILDTYPE ?= Release

# arch depend
UNAME := $(shell uname)

CC ?= gcc

CFLAGS = -Wall -Wno-unused-function -fvisibility=hidden -g -O2 -fPIC

ifeq ($(UNAME), Linux)
CFLAGS += -ffunction-sections -fdata-sections -std=gnu99
#LDFLAGS += -Wl,--gc-sections -Wl,--print-gc-sections
LDFLAGS += -Wl,--gc-sections -Wl,--as-needed
CHKMEM = valgrind --leak-check=full -q --error-exitcode=1
endif

ifeq ($(UNAME), Darwin)
CFLAGS += -ffunction-sections -fdata-sections -std=c99
LDFLAGS += -Wl,-dead_strip
TEST_ENTRY = -e_ut_main
endif
WORKDIR = $(shell pwd)/workdir

ifeq ($(BUILDTYPE), Debug)
	DEP_DEBUG=-DDEBUG=1
endif

GIT_VERSION := $(shell git describe --abbrev=6 --dirty --always --tags)

# dependens
SUBD = cjson

# source
BIN_SRC := \
	src/misc.c \
	src/stat.c \
	src/realm.c \
	src/turf.c \
	src/sock.c \
	src/daemon.c \
	src/shell.c \
	src/cli.c \
	src/oci.c \
	src/crc32.c \
	src/ipc.c \
	src/warmfork.c

OBJ_DIR = build/Release
ifeq ($(BUILDTYPE), Debug)
	OBJ_DIR = build/Debug
endif

# oci spec
SPEC = src/spec.json
SPEC_OBJ = $(SPEC:src/%.json=$(OBJ_DIR)/%.o)

SRC := $(BIN_SRC)
OBJ := $(SRC:src/%.c=$(OBJ_DIR)/%.o)
DEP := $(OBJ:%.o=%.d) $(PRELOAD_OBJ:%.o=%.d)

# preload library
PRELOAD = $(OBJ_DIR)/libturf.so
PRELOAD_SRC := \
	src/misc.c \
	src/stat.c \
	src/realm.c \
	src/sock.c \
	src/shell.c \
	src/crc32.c \
	src/ipc.c \
	src/warmfork.c \
	src/preload.c
PRELOAD_OBJ :=$(PRELOAD_SRC:src/%.c=$(OBJ_DIR)/%.o)

# link spec
OBJ += $(SPEC_OBJ)

INC := -Isrc -Ideps -Iinclude
DEF := -DGIT_VERSION=\"$(GIT_VERSION)\" $(DEP_DEBUG)
LDFLAGS += $(SUBD:%=-Ldeps/%/) $(SUBD:%=-l%)

TARGET = $(OBJ_DIR)/turf

# test
TEST_CASE := $(wildcard test/*.c)
TEST_BIN := $(TEST_CASE:%.c=$(OBJ_DIR)/%)
TEST_OBJ := $(SRC:src/%.c=$(OBJ_DIR)/test/%.o) $(SPEC_OBJ)
TEST_DEP := $(TEST_OBJ:%.o=%.d) $(TEST_BIN:%=%.d)
TEST_CFLAGS := $(CFLAGS) -g -O0 -fPIC -fprofile-arcs -ftest-coverage -Wall
TEST_LDFLAGS := $(SUBD:%=-Ldeps/%/) $(SUBD:%=-l%) $(TEST_ENTRY) -ldl

TARGET_PRODUCT = build/turf
PRELOAD_PRODUCT = build/libturf.so
ifeq ($(BUILDTYPE), Debug)
	TARGET_PRODUCT = build/turf.debug
	PRELOAD_PRODUCT = build/libturf.so.debug
endif

ifeq ($(UNAME), Darwin)
all: $(TARGET_PRODUCT)
else
all: $(TARGET_PRODUCT) $(PRELOAD_PRODUCT)
endif

.PHONY: clean all subd_build subd_clean cleanall distclean test
.SUFFIXES:
.SECONDARY:

-include $(DEP)
-include $(TEST_DEP)

ifneq ($(BUILDTYPE), Debug)
all: debug
endif
debug:
	$(MAKE) BUILDTYPE=Debug -C .

# make deps (clean)
subd_build:
	@for dir in $(SUBD); do \
		$(MAKE) -C deps/$$dir; \
	done

subd_clean:
	@for dir in $(SUBD); do \
		$(MAKE) -C deps/$$dir clean; \
	done

# make spec
$(OBJ_DIR)/%.o: src/%.json
	@echo spec $<
	@xxd -i $< | $(CC) $(CFLAGS) -o $@ -xc - -c

# make target
$(OBJ_DIR)/%.o: src/%.c
	@echo cc $<
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INC) $(DEF) -MMD -MT $@ -MF $(OBJ_DIR)/$*.d -o $@ -c $<

$(TARGET): subd_build $(OBJ)
	@echo ld $(notdir $@)
	@$(CC) -o $@ $(CFLAGS) $(OBJ) $(LDFLAGS)

$(TARGET_PRODUCT): $(TARGET)
	@cp $(TARGET) $@

# make preload library
$(PRELOAD): subd_build $(PRELOAD_OBJ)
	@echo ld $(notdir $@)
	@$(CC) -shared -o $@ $(CFLAGS) $(PRELOAD_OBJ) $(LDFLAGS) -Wl,--version-script=preload.script -ldl

$(PRELOAD_PRODUCT): $(PRELOAD)
	@cp $(PRELOAD) $@

# make test
$(OBJ_DIR)/test/%.o: src/%.c
	@echo cc $<
	@mkdir -p $(dir $@)
	@$(CC) $(TEST_CFLAGS) $(INC) $(DEF) -MMD -MT $@ -MF $(OBJ_DIR)/test/$*.d -o $@ -c $<

$(OBJ_DIR)/test/%.o: test/%.c
	@echo cc $<
	@mkdir -p $(dir $@)
	@$(CC) $(TEST_CFLAGS) $(INC) $(DEF) -MMD -MT $@ -MF $(OBJ_DIR)/test/$*.d -o $@ -c $<

$(OBJ_DIR)/test/test_%: $(TEST_OBJ)  $(OBJ_DIR)/test/test_%.o
	@echo link $@
	@$(CC) -o $@ $(TEST_CFLAGS) $^ $(TEST_LDFLAGS)

test_%: export TURF_WORKDIR=$(WORKDIR)
test_%: $(OBJ_DIR)/test/test_%
	@echo action
	@$(CHKMEM) ./$< || exit 1

test: export TURF_WORKDIR=$(WORKDIR)
test: subd_build $(TEST_BIN)
	@for case in $(TEST_BIN); do \
		echo T $$case; ./$$case || exit 1; \
	done

valgrind: export TURF_WORKDIR=$(WORKDIR)
valgrind: subd_build $(TEST_BIN)
	@for case in $(TEST_BIN); do \
		echo T $$case; $(CHKMEM) ./$$case || exit 1; \
	done

# make clean
clean:
	@rm -rf build turf

distclean: cleanall

cleanall: subd_clean
	@rm -rf build turf

include ../build/Makefiles/toolchain.mk

SOURCE_FILES=$(shell find include src test -type f \
			-name '*.c' -or -name '*.h')
format: $(SOURCE_FILES)
	$(CLANG_FORMAT) -i --style=file $(SOURCE_FILES)

lint: $(SOURCE_FILES)
	@echo "Skipped"
