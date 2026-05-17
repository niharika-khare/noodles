SRC_DIR        := ./src
INC_ROOT_DIR   := ./include
BIN_DIR        := ./bin
BUILD_DIR      := ./build

INC_DIR   := $(shell find $(INC_ROOT_DIR) -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIR))

CC_FLAGS  := $(INC_FLAGS) -g -Wall


SRC := $(shell find $(SRC_DIR) -name '*.c')
DEP := $(shell find $(INC_ROOT_DIR) -name '*.h')
OBJ := $(patsubst $(BUILD_DIR)/%.o,$(SRC_DIR)/%.c,$(SRC))


.PHONY: clean build all help


all: build
	@sudo $(BIN_DIR)/noodles

help:
	@echo "Makefile targets: "
	@echo "  all.  	- 	Build and run all the changes (default)"
	@echo "  build 	- 	Build everything but do not run the application"
	@echo "  clean 	- 	clean all build targets and executables"
	@echo "  help  	- 	Display this help menu"

	
clean:
	@sudo rm -rf $(BIN_DIR) $(BUILD_DIR)


build: $(BUILD_DIR) $(BIN_DIR) noodles


$(BIN_DIR) $(BUILD_DIR):
	@sudo mkdir -p $@


$(BUILD_DIR)/%.o: $(SRC)/%.c $(DEP)
	@sudo mkdir -p $(dir $@)
	@sudo $(CC) -c $< -o $@ $(CC_FLAGS)


noodles: $(OBJ)
	@sudo $(CC) $^ -o $(BIN_DIR)/$@ $(CC_FLAGS)