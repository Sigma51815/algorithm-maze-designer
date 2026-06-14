BUILD_DIR  := build
BUILD_TYPE := Release
TARGET     := $(BUILD_DIR)/maze_designer

.PHONY: all configure build clean run test help

all: build

configure:
	@cmake -S . -B $(BUILD_DIR) -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	@cmake --build $(BUILD_DIR) --parallel

clean:
	@rm -rf $(BUILD_DIR)
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
