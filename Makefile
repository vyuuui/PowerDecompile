CC              := g++
CCFLAGS_SHARED  := --std=c++20 -Wall -Werror -fno-exceptions -fno-rtti -MD -MP
CCFLAGS_DEBUG   := $(CCFLAGS_SHARED) -g -O0
CCFLAGS_RELEASE := $(CCFLAGS_SHARED) -g -O2
CCFLAGS         := $(CCFLAGS_DEBUG)
BASE_LIBS       := -lfmt

BUILD_DIR       := build
BIN_DIR         := bin
SRC_DIR         := src
INC_DIR         := include

SRCS            =  $(wildcard $(SRC_DIR)/*.cc)
SRCS_DBGUTIL    =  $(wildcard $(SRC_DIR)/dbgutil/*.cc)
SRCS_HLL        =  $(wildcard $(SRC_DIR)/hll/*.cc)
SRCS_IR         =  $(wildcard $(SRC_DIR)/ir/*.cc)
SRCS_PPC        =  $(wildcard $(SRC_DIR)/ppc/*.cc)
SRCS_PRODUCER   =  $(wildcard $(SRC_DIR)/producers/*.cc)
SRCS_UTL        =  $(wildcard $(SRC_DIR)/utl/*.cc)
INTER           =  $(patsubst $(SRC_DIR)/%.cc, $(BUILD_DIR)/%.o, $(SRCS))
INTER_DBGUTIL   =  $(patsubst $(SRC_DIR)/dbgutil/%.cc, $(BUILD_DIR)/dbgutil/%.o, $(SRCS_DBGUTIL))
INTER_HLL       =  $(patsubst $(SRC_DIR)/hll/%.cc, $(BUILD_DIR)/hll/%.o, $(SRCS_HLL))
INTER_IR        =  $(patsubst $(SRC_DIR)/ir/%.cc, $(BUILD_DIR)/ir/%.o, $(SRCS_IR))
INTER_PPC       =  $(patsubst $(SRC_DIR)/ppc/%.cc, $(BUILD_DIR)/ppc/%.o, $(SRCS_PPC))
INTER_PRODUCER  =  $(patsubst $(SRC_DIR)/producers/%.cc, $(BUILD_DIR)/producers/%.o, $(SRCS_PRODUCER))
INTER_UTL       =  $(patsubst $(SRC_DIR)/utl/%.cc, $(BUILD_DIR)/utl/%.o, $(SRCS_UTL))


all : $(BIN_DIR)/decompile

$(BIN_DIR)/decompile : $(INTER) $(INTER_DBGUTIL) $(INTER_HLL) $(INTER_IR) $(INTER_PPC) $(INTER_PRODUCER) $(INTER_UTL)
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) $(BASE_LIBS) $^ -o $@

$(BUILD_DIR)/%.o : $(SRC_DIR)/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/dbgutil/%.o : $(SRC_DIR)/dbgutil/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/hll/%.o : $(SRC_DIR)/hll/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/ir/%.o : $(SRC_DIR)/ir/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/ppc/%.o : $(SRC_DIR)/ppc/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/producers/%.o : $(SRC_DIR)/producers/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

$(BUILD_DIR)/utl/%.o : $(SRC_DIR)/utl/%.cc
	@mkdir -p $(@D)
	$(CC) $(CCFLAGS) -I$(INC_DIR) -c $< -o $@

.PHONY: clean
clean :
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)

-include $(INTER:$(BUILD_DIR)/%.o=$(BUILD_DIR)/%.d)
-include $(INTER_DBGUTIL:$(BUILD_DIR)/dbgutil/%.o=$(BUILD_DIR)/dbgutil/%.d)
-include $(INTER_HLL:$(BUILD_DIR)/hll/%.o=$(BUILD_DIR)/hll/%.d)
-include $(INTER_IR:$(BUILD_DIR)/ir/%.o=$(BUILD_DIR)/ir/%.d)
-include $(INTER_PPC:$(BUILD_DIR)/ppc/%.o=$(BUILD_DIR)/ppc/%.d)
-include $(INTER_PRODUCER:$(BUILD_DIR)/producers/%.o=$(BUILD_DIR)/producers/%.d)
-include $(INTER_UTL:$(BUILD_DIR)/utl/%.o=$(BUILD_DIR)/utl/%.d)
