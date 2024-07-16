
#SRC_DIR=src
SRC_DIR=tests
BIN_DIR=bin

.PHONY: all clean $(BIN_DIR)/dumb_reader.elf $(BIN_DIR)/gif_header.elf $(BIN_DIR)/gif_body.elf

all: $(BIN_DIR)/dumb_reader.elf $(BIN_DIR)/gif_header.elf $(BIN_DIR)/gif_body.elf

$(BIN_DIR)/dumb_reader.elf: $(SRC_DIR)/dumb_gif_reader.c $(BIN_DIR)
	gcc -Wall -o $@ $<

$(BIN_DIR)/gif_header.elf: $(SRC_DIR)/gif_header.c $(BIN_DIR)
	gcc -Wall -o $@ $<

$(BIN_DIR)/gif_body.elf: $(SRC_DIR)/gif_body.c $(BIN_DIR)
	gcc -Wall -o $@ $<

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BIN_DIR)

