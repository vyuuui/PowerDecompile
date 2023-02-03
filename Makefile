CC              := g++
CCFLAGS_SHARED  := --std=c++17 -Wall -Werror -fno-exceptions -fno-rtti -MD -MP
CCFLAGS_DEBUG   := $(CCFLAGS_SHARED) -g -O0
CCFLAGS_RELEASE := $(CCFLAGS_SHARED) -O2
CCFLAGS         := $(CCFLAGS_DEBUG)
BASE_LIBS       :=

BUILD_DIR       := build
BIN_DIR         := bin
SRC_DIR         := src
INC_DIR         := include

SRCS            =  $(wildcard $(SRC_DIR)/*.cc)
INTERMEDIATES   =  $(patsubst $(SRC_DIR)/%.cc, $(BUILD_DIR)/%.o, $(SRCS))


all : $(BIN_DIR)/decompile

$(BIN_DIR)/decompile : $(INTERMEDIATES)
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) $^ -o $@

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

.PHONY: clean
clean :
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

-include $(INTERMEDIATES:$(BUILD_DIR)/%.o=$(BUILD_DIR)/%.d)
