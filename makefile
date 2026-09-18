# === Compiler and flags ===
COMPILER = g++
CFLAGS = -Wall -std=c++17 -I$(HEADER_DIR)

# === Project structure ===
SRC_DIR = src
HEADER_DIR = headers
BUILD_DIR = build
TARGET = $(BUILD_DIR)/main

# === Source and object files ===
SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp, $(BUILD_DIR)/%.o, $(SRC_FILES))

# === Default target ===
all: $(TARGET)

# === Link all objects into final executable ===
$(TARGET): $(OBJ_FILES) | $(BUILD_DIR)
	$(COMPILER) $(CFLAGS) -o $@ $^

# === Compile each .cpp into a .o ===
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(COMPILER) $(CFLAGS) -c $< -o $@

# === Ensure build directory exists ===
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# === Clean build artifacts ===
clean:
	rm -rf $(BUILD_DIR)

# === Run the program ===
run: $(TARGET)
	./$(TARGET)

clear-logs:
	rm -f logs/*.log