# Maze Designer — cross-platform Makefile
#   macOS / Linux : make [target]
#   Windows (MSYS2): make [target]
#   Windows (native): use build.bat instead
BUILD_DIR  := build
BUILD_TYPE := Release

# Platform detection
UNAME_S := $(shell uname -s 2>/dev/null || echo Windows)

# On macOS/Linux the binary has no extension; on Windows it's .exe
ifeq ($(UNAME_S),Windows)
  EXE_EXT := .exe
  RM       := cmd /c del /q /s
  RMDIR    := cmd /c rmdir /s /q
else
  EXE_EXT :=
  RM       := rm -rf
  RMDIR    := rm -rf
endif

TARGET := $(BUILD_DIR)/maze_designer$(EXE_EXT)

.PHONY: all configure build clean run test help

all: build

configure:
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	@cmake --build $(BUILD_DIR) --parallel

clean:
	@$(RMDIR) $(BUILD_DIR)
	@echo "Build directory removed."

run: build
	@./$(TARGET)

test: build
	@./$(TARGET) --self-test

help:
	@echo "Usage:"
	@echo "  make          Build the project"
	@echo "  make clean    Remove build directory"
	@echo "  make run      Build and run GUI"
	@echo "  make test     Build and run self-test"
	@echo "  make help     Show this help"
	@echo ""
	@echo "Windows native users: run 'build.bat' instead."
