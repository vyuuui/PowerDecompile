CC              := g++
CCFLAGS_SHARED  := --std=c++17 -Wall -Werror -fno-exceptions -fno-rtti -MD -MP
CCFLAGS_DEBUG   := $(CCFLAGS_SHARED) -g -O0
CCFLAGS_RELEASE := $(CCFLAGS_SHARED) -O2
CCFLAGS         := $(CCFLAGS_DEBUG)
BASE_LIBS       := -lfmt

BUILD_DIR       := build
BIN_DIR         := bin
SRC_DIR         := src
INC_DIR         := include

SRCS            =  $(wildcard $(SRC_DIR)/*.cc)
SRCS_PRODUCER   =  $(wildcard $(SRC_DIR)/producers/*.cc)
SRCS_DBGUTIL    =  $(wildcard $(SRC_DIR)/dbgutil/*.cc)
INTER           =  $(patsubst $(SRC_DIR)/%.cc, $(BUILD_DIR)/%.o, $(SRCS))
INTER_PRODUCER  =  $(patsubst $(SRC_DIR)/producers/%.cc, $(BUILD_DIR)/producers/%.o, $(SRCS_PRODUCER))
INTER_DBGUTIL   =  $(patsubst $(SRC_DIR)/dbgutil/%.cc, $(BUILD_DIR)/dbgutil/%.o, $(SRCS_DBGUTIL))


all : $(BIN_DIR)/decompile

$(BIN_DIR)/decompile : $(INTER) $(INTER_PRODUCER) $(INTER_DBGUTIL)
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) $(BASE_LIBS) $^ -o $@

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/producers/%.o : $(SRC_DIR)/producers/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/dbgutil/%.o : $(SRC_DIR)/dbgutil/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

.PHONY: clean
clean :
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

-include $(INTER:$(BUILD_DIR)/%.o=$(BUILD_DIR)/%.d)
-include $(INTER_PRODUCER:$(BUILD_DIR)/producers/%.o=$(BUILD_DIR)/producers/%.d)
-include $(INTER_DBGUTIL:$(BUILD_DIR)/dbgutil/%.o=$(BUILD_DIR)/dbgutil/%.d)
