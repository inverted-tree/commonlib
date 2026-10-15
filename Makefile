.DEFAULT_GOAL := all

# Build configuration: release (default) or debug.
BUILD ?= release

ifeq ($(BUILD),debug)
    MODE_CFLAGS   := -O0 -g3
    MODE_CPPFLAGS := -DDEBUG
else ifeq ($(BUILD),release)
    MODE_CFLAGS   := -O2
    MODE_CPPFLAGS := -DNDEBUG
else
    $(error Unsupported BUILD='$(BUILD)'; expected debug or release)
endif

CC     ?= cc
AR     ?= ar
CSTD   ?= c23
CFLAGS ?= -Wall -Wextra -Wpedantic

# Treat compiler warnings as errors when requested.
WERROR ?= 0

ifeq ($(WERROR),1)
    STRICT_CFLAGS := -Werror
else ifneq ($(WERROR),0)
    $(error Unsupported WERROR='$(WERROR)'; expected 0 or 1)
endif

# Each library directory contains a library.mk.
LIBRARY_MKS := $(wildcard */library.mk)
LIBRARIES := $(patsubst %/library.mk,%,$(LIBRARY_MKS))

ifeq ($(strip $(LIBRARIES)),)
    $(error No library.mk files found)
endif

include $(LIBRARY_MKS)

# Fail early for misspelled library dependencies.
$(foreach lib,$(LIBRARIES),$(foreach dep,$(LIB_$(lib)_DEPS),$(if $(filter $(dep),$(LIBRARIES)),,$(error Unknown dependency '$(dep)' of '$(lib)'))))

# Find transitive dependencies; reject dependency cycles.
# The second argument tracks the current traversal path.
closure = $(if $(filter $(1),$(2)),$(error Dependency cycle: $(2) -> $(1)))$(1) $(foreach dep,$(LIB_$(1)_DEPS),$(call closure,$(dep),$(strip $(2) $(1))))
transitive = $(sort $(call closure,$(1),))

# Compile only .c files directly under <library>/src/.
# Prefix object filenames with the library name so archive members are unique.
sources = $(wildcard $(1)/src/*.c)
objects = $(patsubst $(1)/src/%.c,build/$(BUILD)/obj/$(1)_%.o,$(call sources,$(1)))
archive_objects = $(sort $(foreach lib,$(call transitive,$(1)),$(call objects,$(lib))))
include_flags = $(addprefix -I,$(addsuffix /include,$(call transitive,$(1))))

.PHONY: all clean
all: $(LIBRARIES)

define library_rules
# The public library target is the complete distributable package.
.PHONY: $(1)
$(1): build/$(BUILD)/lib$(1).a
	@rm -rf dist/$(BUILD)/$(1)
	@mkdir -p dist/$(BUILD)/$(1)/lib dist/$(BUILD)/$(1)/include
	cp $$< dist/$(BUILD)/$(1)/lib/
	$(foreach dep,$(call transitive,$(1)),cp -R $(dep)/include/. dist/$(BUILD)/$(1)/include/;)

build/$(BUILD)/obj/$(1)_%.o: $(1)/src/%.c $(1)/library.mk
	@mkdir -p $$(@D)
	$(CC) $(CPPFLAGS) $(MODE_CPPFLAGS) $(call include_flags,$(1)) $(LIB_$(1)_CPPFLAGS) -std=$(CSTD) $(CFLAGS) $(MODE_CFLAGS) $(LIB_$(1)_CFLAGS) $(STRICT_CFLAGS) -MMD -MP -c $$< -o $$@

# Recreate the archive when its objects or local dependency metadata change.
build/$(BUILD)/lib$(1).a: $(call archive_objects,$(1)) $(1)/library.mk
	@mkdir -p $$(@D)
	rm -f $$@
	$(AR) rcs $$@ $$(filter %.o,$$^)
endef

$(foreach lib,$(LIBRARIES),$(eval $(call library_rules,$(lib))))

# Auto-generated prerequisites also track #included headers.
ALL_OBJECTS := $(foreach lib,$(LIBRARIES),$(call objects,$(lib)))
-include $(ALL_OBJECTS:.o=.d)

clean:
	rm -rf build dist
