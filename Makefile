CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
LIBS = -lcurl -lcjson

TARGET = perplexity-mcp-server
SOURCES = main.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET)

install-deps-ubuntu:
	sudo apt-get update
	sudo apt-get install libcurl4-openssl-dev libcjson-dev

install-deps-macos:
	brew install curl cjson

.PHONY: clean install-deps-ubuntu install-deps-macos
