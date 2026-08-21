export NPROC ?= $(shell (nproc))
export NJOB  ?= $(shell expr '(' $(NPROC) + 1 ')')

.PHONY: default
default: release/all

SANITIZER_FLAGS :=
# Sanitizer processing template
# $(1) — command line argument; $(2) — cmake option name
define check_sanitizer
ifeq ($$(filter $(1),$$(MAKECMDGOALS)),$(1))
    SANITIZER_FLAGS += -D$(2)=ON
    # Hide flag for make
    $$(eval $(1):;@:)
else
    SANITIZER_FLAGS += -D$(2)=OFF
endif
endef

$(eval $(call check_sanitizer,--sanitizer_addr,USE_ADDR_SANITIZER))
$(eval $(call check_sanitizer,--sanitizer_leak,USE_LEAK_SANITIZER))
$(eval $(call check_sanitizer,--sanitizer_ub,USE_BEHAVIOR_SANITIZER))
$(eval $(call check_sanitizer,--sanitizer_thread,USE_THREAD_SANITIZER))

# Base rule declaration
define base_rule

.PHONY: $(1)/configure
$(1)/configure: build_$(1)/Makefile

$(1)/%: $(1)/configure
	@make -j $(NJOB) --output-sync=target --no-print-directory -C build_$(1) $$*

.PHONY: $(1)
$(1): $(1)/all

.PHONY: $(1)/clean
$(1)/clean: check_gitignore
	@rm -rf build_$(1)

.PHONY: $(1)/rebuild
$(1)/rebuild: $(1)/clean $(1)/all

.PHONY: $(1)/help
$(1)/help: $(1)/configure
	@echo "--- List of all cmake targets ---"
	@make --no-print-directory -C build_$(1) help

.PHONY: $(1)/test
$(1)/test: $(1)/all
	@make -j $(NJOB) -C build_$(1) test

# Dynamic import of cmake targets for auto completion.
$(if $(wildcard build_$(1)/Makefile),
    $(eval _RAW_TARGETS := $(shell $(MAKE) --no-print-directory -C build_$(1) help 2>/dev/null | awk '/^\.\.\./ {print $$2}'))
    $(foreach t,$(_RAW_TARGETS),
        $(if $(filter-out configure clean rebuild help test,$(t)),
            $(eval .PHONY: $(1)/$(t))
            $(eval $(1)/$(t): build_$(1)/Makefile ; @\$$(MAKE) -j \$$(NJOB) --output-sync=target --no-print-directory -C build_$(1) $(t))
        )
    )
)

endef

# Common rule declaration
define common_rule

build_$(1)/Makefile: check_gitignore
	@mkdir -p $$(@D) && \
	touch $$(@D)/.kdev_ignore && \
	cd $$(@D) && \
	cmake -DCMAKE_BUILD_TYPE="$(1)" $$(SANITIZER_FLAGS) ../

$$(eval $$(call base_rule,$(1)))

endef

# Coverage rule declaration
define coverage_rule

build_$(1)/Makefile: check_gitignore
	@mkdir -p $$(@D) && \
	touch $$(@D)/.kdev_ignore && \
	cd $$(@D) && \
	cmake -DCMAKE_BUILD_TYPE="debug" -DCOVERAGE_BUILD=ON -DBUILD_TESTS=ON $$(SANITIZER_FLAGS) ../

$$(eval $$(call base_rule,$(1)))

endef

# Debug and release build types definition
$(eval $(call common_rule,debug))
$(eval $(call common_rule,release))
$(eval $(call coverage_rule,coverage))

.PHONY: all
all: release/all

.PHONY: clean
clean: release/clean

.PHONY: configure
configure: release/configure

.PHONY: rebuild
rebuild: release/clean release/all

.PHONY: help
help: release/help

.PHONY: coverage
coverage: coverage/all coverage/coverage

#.PHONY: coverage
#coverage: coverage/coverage

.PHONY: check_gitignore
check_gitignore:
	@if [ -f .gitignore ]; then \
		if ! grep -Fxq "build*/" .gitignore; then \
			echo "Updating .gitignore with build and IDE paths..."; \
			(echo "# Build generated"; \
			 echo "build*/"; \
			 echo ".kdev4/"; \
			 echo "*.kdev4"; \
			 echo ""; \
			 cat .gitignore) > .gitignore.tmp && mv .gitignore.tmp .gitignore; \
		fi; \
		if grep -Fxq "Makefile" .gitignore; then \
			echo "Removing Makefile from .gitignore..."; \
			sed -i.tmp '/^Makefile$$/d' .gitignore && rm -f .gitignore.tmp; \
		fi \
	fi

.PHONY: test
test: release/test
