ROOT := ../..
OUTPUT := $(ROOT)/build/legacy-oracle
SOURCES := main.m OracleAppData.m $(ROOT)/NAKL/KeyboardHandler.m

.PHONY: all
all: $(OUTPUT)

$(OUTPUT): $(SOURCES) $(ROOT)/NAKL/KeyboardHandler.h $(ROOT)/NAKL/utf.h $(ROOT)/NAKL/telex-standard.h
	mkdir -p $(ROOT)/build
	xcrun clang -fno-objc-arc -DOBJC_OLD_DISPATCH_PROTOTYPES=1 \
		-I$(ROOT)/NAKL $(SOURCES) -framework Cocoa -o $(OUTPUT)
