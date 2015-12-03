CFLAGS=-g -O0

TARGET=nested
SOURCES=$(wildcard *.c)
OBJECTS=$(patsubst %.c,%.o,$(SOURCES))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

ifneq ($(MAKECMDGOALS),clean)
-include $(SOURCES:%.c=.depends/%.d)
endif

.depends/%.d: %.c .depends
	$(CC) $(CFLAGS) -MM -c $< -o $@

.depends:
	mkdir .depends

.PHONY: clean
clean:
	rm -rf $(TARGET).dSYM .depends
	rm -f $(TARGET) *.o a.out
