SRC_DIR=src
INCLUDE_DIR=includes
BUILD_DIR=build

CC=gcc
CFLAGS=-I$(INCLUDE_DIR) -lm -pthread

DOCKER_CMP=docker-compose

TARGET_EXEC ?= $(BUILD_DIR)/broker
SRCS := $(shell find $(SRC_DIR) -name *.c)
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

.PHONY: clean all run docker

all: $(TARGET_EXEC)

run: all
	$(TARGET_EXEC) 1883

docker:
	$(DOCKER_CMP) down
	$(DOCKER_CMP) up --build

$(TARGET_EXEC): $(OBJS)
	$(CC) $(OBJS) -o $@ $(CFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	$(MKDIR_P) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) -r $(BUILD_DIR)

-include $(DEPS)

MKDIR_P ?= mkdir -p
