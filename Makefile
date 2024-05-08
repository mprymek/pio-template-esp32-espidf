PLATFORMIO_DIR ?= ~/.platformio
PIO ?= $(PLATFORMIO_DIR)/penv/bin/pio
ifdef PIO_ENV
PIO_ENV_OPTS += -e $(PIO_ENV)
endif
CLANG_FORMAT ?= clang-format-7

.PHONY: menuconfig
menuconfig:
	$(PIO) run $(PIO_ENV_OPTS) -t menuconfig

.PHONY: build
build:
	$(PIO) run $(PIO_ENV_OPTS)

.PHONY: install
install:
	$(PIO) run $(PIO_ENV_OPTS) -t upload

.PHONY: clean
clean:
	$(PIO) run $(PIO_ENV_OPTS) -t clean

.PHONY: monitor
monitor:
	$(PIO) device monitor $(PIO_ENV_OPTS)

.PHONY: fmt
fmt:
	find include lib src test \
		-type f \( -iname *.h -o -iname *.cpp -o -iname *.c \) \
	| xargs $(CLANG_FORMAT) -style=file -i
