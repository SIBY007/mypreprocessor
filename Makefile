# Makefile for mypreprocessor
CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -O2 -MMD -MP
TARGET  := mypreprocessor
SRCDIR  := src
OBJDIR  := build

SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

test: $(TARGET)
	@chmod +x ./tests/run_tests.sh 2>/dev/null || true
	./tests/run_tests.sh

clean:
	rm -rf $(OBJDIR) $(TARGET) tests/*.i

-include $(OBJS:.o=.d)
