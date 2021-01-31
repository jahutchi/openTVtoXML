OBJS += src/memory.o
OBJS += src/core/log.o
OBJS += src/core/dvb_text.o
OBJS += src/dvb/dvb.o
OBJS += src/opentv/opentv.o
OBJS += src/opentv/huffman.o
OBJS += src/providers/providers.o
OBJS += src/epgdb/epgdb.o
OBJS += src/epgdb/epgdb_channels.o
OBJS += src/epgdb/epgdb_titles.o

DOWNLOADER_OBJS += src/openTVtoXML.o

DOWNLOADER_BIN = bin/openTVtoXML

BIN_DIR = bin

CFLAGS += -Wall -Werror

all: clean $(DOWNLOADER_BIN)

$(BIN_DIR):
	mkdir -p $@

$(OBJS): $(BIN_DIR)
	$(CC) $(CFLAGS) -c -fpic -o $@ $(@:.o=.c)

$(DOWNLOADER_OBJS):
	$(CC) $(CFLAGS) -c -o $@ $(@:.o=.c)

$(DOWNLOADER_BIN): $(OBJS) $(DOWNLOADER_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(DOWNLOADER_OBJS)

clean:
	rm -f $(OBJS) $(DOWNLOADER_OBJS) $(DOWNLOADER_BIN)

install:
	install -d $(D)/opt/openTVtoXML/providers
	install -m 755 bin/openTVtoXML $(D)/opt/openTVtoXML/
	install -m 644 providers/* $(D)/opt/openTVtoXML/providers/

