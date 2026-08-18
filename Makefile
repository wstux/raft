export NPROC ?= $(shell (nproc))
export NJOB  ?= $(shell expr '(' $(NPROC) + 1 ')')

.PHONY: default
default: release/all

# Common rule declaration
define common_rule

build_$(1)/Makefile: check_gitignore
	@mkdir -p $$(@D) && cd $$(@D) && cmake -DCMAKE_BUILD_TYPE="$(1)" ../

$(1)/%: build_$(1)/Makefile
	@make -j $(NJOB) --output-sync=target --no-print-directory -C build_$(1) $$*

.PHONY: $(1)
$(1): $(1)/all
#	@make -j $(NJOB) --output-sync=target --no-print-directory -C build_$(1) $$*

.PHONY: $(1)/clean
$(1)/clean: check_gitignore
	@rm -rf build_$(1)

.PHONY: $(1)/rebuild
$(1)/rebuild: $(1)/clean $(1)/all

.PHONY: $(1)/help
$(1)/help: build_$(1)/Makefile
	@echo "--- List of all cmake targets ---"
	@make --no-print-directory -C build_$(1) help

.PHONY: $(1)/test
$(1)/test: $(1)/all
	@make -j $(NJOB) -C build_$(1) test

endef

# Coverage rule declaration
define coverage_rule

build_$(1)/Makefile: check_gitignore
	@mkdir -p $$(@D) && cd $$(@D) && cmake -DCMAKE_BUILD_TYPE="debug" -DCOVERAGE_BUILD=ON -DBUILD_TESTS=ON ../

$(1)/%: build_$(1)/Makefile
	@make -j $(NJOB) --output-sync=target --no-print-directory -C build_$(1) $$*

.PHONY: $(1)/clean
$(1)/clean: check_gitignore
	@rm -rf build_$(1)

.PHONY: $(1)/rebuild
$(1)/rebuild: $(1)/clean $(1)/all

.PHONY: $(1)/help
$(1)/help: build_$(1)/Makefile
	@echo "--- List of all cmake targets ---"
	@make --no-print-directory -C build_$(1) help

.PHONY: $(1)/test
$(1)/test: $(1)/all
	@make -j $(NJOB) -C build_$(1) test

endef

# Debug and release build types definition
$(eval $(call common_rule,debug))
$(eval $(call common_rule,release))
$(eval $(call coverage_rule,coverage))

.PHONY: all
all: release/all

.PHONY: clean
clean: release/clean

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

