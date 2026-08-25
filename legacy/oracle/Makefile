ROOT := ../..
OUTPUT := $(ROOT)/build/legacy-oracle
LEGACY_APP := $(ROOT)/legacy/macos-hook/NAKL
SOURCES := main.m OracleAppData.m $(LEGACY_APP)/KeyboardHandler.m

.PHONY: all
all: $(OUTPUT)

$(OUTPUT): $(SOURCES) $(LEGACY_APP)/KeyboardHandler.h $(LEGACY_APP)/utf.h $(LEGACY_APP)/telex-standard.h
	mkdir -p $(ROOT)/build
	xcrun clang -fno-objc-arc -DOBJC_OLD_DISPATCH_PROTOTYPES=1 \
		-I$(LEGACY_APP) $(SOURCES) -framework Cocoa -o $(OUTPUT)
