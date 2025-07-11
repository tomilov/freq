NPROC ?= $(shell nproc)
FORK ?= $(shell echo $$(( $(NPROC) / 2 )))
ROOT_DIR := $(shell dirname "$(realpath $(firstword $(MAKEFILE_LIST)))")
BUILD_DIR ?= /tmp/build-freq
BUILD_TYPE ?= Release
BUILD_SHARED_LIBS ?= ON
LINKER ?= $(shell which lld)
C_COMPILER ?= $(shell which clang)
C_FLAGS ?= -march=native
CXX_COMPILER ?= $(shell which clang++)
CXX_FLAGS ?= -march=native -stdlib=libc++
TIMES ?= 3
TARGET ?= oaph

.DEFAULT_GOAL := build

.PHONY: configure
configure:
	@cmake -E make_directory $(BUILD_DIR)
	@nice cmake \
		-S $(ROOT_DIR) \
		-B $(BUILD_DIR) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DBUILD_SHARED_LIBS=$(BUILD_SHARED_LIBS) \
		-DCMAKE_LINKER=$(LINKER) \
		-DCMAKE_C_COMPILER=$(C_COMPILER) \
		-DCMAKE_C_FLAGS="$(C_FLAGS)" \
		-DCMAKE_CXX_COMPILER=$(CXX_COMPILER) \
		-DCMAKE_CXX_FLAGS="$(CXX_FLAGS)" \
		-DCMAKE_CUDA_HOST_COMPILER=$(CXX_COMPILER) \
		-DCMAKE_CUDA_ARCHITECTURES=$(CUDA_ARCH) \
		-DCMAKE_CUDA_FLAGS="$(CUDA_FLAGS)" \
		-DCMAKE_VERBOSE_MAKEFILE=ON \
		-DTHRUST_DEVICE_SYSTEM=$(THRUST_DEVICE_SYSTEM)

.PHONY: build
build: configure
	@nice cmake \
		--build $(BUILD_DIR) \
		--parallel $(NPROC) \
		--target all

.PHONY:
rebuild: configure
	@nice cmake \
		--build $(BUILD_DIR) \
		--parallel $(NPROC) \
		--clean-first \
		--target all

.PHONY: clean
clean: configure
	@nice cmake \
		--build $(BUILD_DIR) \
		--parallel $(NPROC) \
		--target clean

.PHONY: run
run: build
	@bash run.bash $(BUILD_DIR)/$(TARGET) $(TIMES)
