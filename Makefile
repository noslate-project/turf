BUILDTYPE ?= Release

# arch depend
UNAME := $(shell uname)

CC ?= gcc

CFLAGS = -std=c99 -Wall -Wno-unused-function -fvisibility=hidden -g -O2 -fPIC

ifeq ($(UNAME), Linux)
CFLAGS += -ffunction-sections -fdata-sections
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

ifdef DEBUG
	DEP_DEBUG=-DDEBUG=1
else
ifeq (DEBUG, $(wildcard DEBUG))
	DEP_DEBUG:=-DDEBUG=1
endif
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

# oci spec
SPEC = src/spec.json
SPEC_OBJ = $(SPEC:src/%.json=build/%.o)

SRC := $(BIN_SRC)
OBJ := $(SRC:src/%.c=build/%.o)
DEP := $(OBJ:%.o=%.d) $(PRELOAD_OBJ:%.o=%.d)

# preload library
PRELOAD = build/libturf.so
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
PRELOAD_OBJ :=$(PRELOAD_SRC:src/%.c=build/%.o)

# link spec
OBJ += $(SPEC_OBJ)

INC := -Isrc -Ideps -Iinclude
DEF := -DGIT_VERSION=\"$(GIT_VERSION)\" $(DEP_DEBUG)
LDFLAGS += $(SUBD:%=-Ldeps/%/) $(SUBD:%=-l%)

TARGET = build/turf

# test
TEST_CASE := $(wildcard test/*.c)
TEST_BIN := $(TEST_CASE:%.c=build/%)
TEST_OBJ := $(SRC:src/%.c=build/test/%.o) $(SPEC_OBJ)
TEST_DEP := $(TEST_OBJ:%.o=%.d) $(TEST_BIN:%=%.d)
TEST_CFLAGS := -g -O0 -fPIC -fprofile-arcs -ftest-coverage -Wall -std=c99
TEST_LDFLAGS := $(SUBD:%=-Ldeps/%/) $(SUBD:%=-l%) $(TEST_ENTRY) -ldl

ifeq ($(UNAME), Darwin)
all: $(TARGET)
else
all: $(TARGET) $(PRELOAD)
endif

.PHONY: clean all subd_build subd_clean cleanall distclean test
.SUFFIXES:
.SECONDARY:

-include $(DEP)
-include $(TEST_DEP)

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
build/%.o: src/%.json
	@echo spec $<
	@xxd -i $< | $(CC) $(CFLAGS) -o $@ -xc - -c

# make target
build/%.o: src/%.c
	@echo cc $<
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INC) $(DEF) -MMD -MT $@ -MF build/$*.d -o $@ -c $<

$(TARGET).debug: subd_build $(OBJ)
	@echo ld $(notdir $@)
	@$(CC) -o $@ $(CFLAGS) $(OBJ) $(LDFLAGS)

$(TARGET): $(TARGET).debug
	@cp -f $^ $@
	@strip $@
	@ls -al $@
	@ln -sf $@ turf

# make preload library
$(PRELOAD).debug: subd_build $(PRELOAD_OBJ)
	@echo ld $(notdir $@)
	@$(CC) -shared -o $@ $(CFLAGS) $(PRELOAD_OBJ) $(LDFLAGS) -Wl,--version-script=preload.script -ldl

$(PRELOAD): $(PRELOAD).debug
	@cp -f $^ $@
	@strip $@
	@ls -al $@

# make test
build/test/%.o: src/%.c
	@echo cc $<
	@mkdir -p $(dir $@)
	@$(CC) $(TEST_CFLAGS) $(INC) $(DEF) -MMD -MT $@ -MF build/test/$*.d -o $@ -c $<

build/test/%.o: test/%.c
	@echo cc $<
	@mkdir -p $(dir $@)
	@$(CC) $(TEST_CFLAGS) $(INC) $(DEF) -MMD -MT $@ -MF build/test/$*.d -o $@ -c $<

build/test/test_%: $(TEST_OBJ)  build/test/test_%.o
	@echo link $@
	@$(CC) -o $@ $(TEST_CFLAGS) $^ $(TEST_LDFLAGS)

test_%: export TURF_WORKDIR=$(WORKDIR)
test_%: build/test/test_%
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
