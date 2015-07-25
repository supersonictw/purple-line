THRIFT_STATIC_DIR = thrift_static

ifdef THRIFT_STATIC
	THRIFT = $(THRIFT_STATIC_DIR)/compiler/cpp/thrift
	THRIFT_CXXFLAGS = -I$(THRIFT_STATIC_DIR)/lib/cpp/src
	THRIFT_LIBS = -L$(THRIFT_STATIC_DIR)/lib/cpp/.libs -Wl,-Bstatic -lthrift -Wl,-Bdynamic
	THRIFT_DEP = $(THRIFT)
else
	THRIFT = thrift
	THRIFT_CXXFLAGS = `pkg-config --cflags thrift`
	THRIFT_LIBS = `pkg-config --libs thrift`
endif

CXX = g++
CXXFLAGS = -g -Wall -shared -fPIC \
	-DHAVE_INTTYPES_H -DHAVE_CONFIG_H -DPURPLE_PLUGINS \
	`pkg-config --cflags purple` $(THRIFT_CXXFLAGS)

LIBS = `pkg-config --libs purple` $(THRIFT_LIBS)

PURPLE_PLUGIN_DIR:=$(shell pkg-config --variable=plugindir purple)
PURPLE_DATA_ROOT_DIR:=$(shell pkg-config --variable=datarootdir purple)

MAIN = libline.so

GEN_SRCS = thrift_line/line_constants.cpp thrift_line/line_types.cpp \
	thrift_line/TalkService.cpp
REAL_SRCS = pluginmain.cpp linehttptransport.cpp thriftclient.cpp httpclient.cpp \
	purpleline.cpp purpleline_blist.cpp purpleline_chats.cpp purpleline_cmds.cpp \
	purpleline_login.cpp purpleline_write.cpp \
	poller.cpp pinverifier.cpp
SRCS += $(GEN_SRCS)
SRCS += $(REAL_SRCS)

OBJS = $(SRCS:.cpp=.o)

all: $(MAIN)

$(MAIN): $(OBJS) $(THRIFT_DEP)
	$(CXX) $(CXXFLAGS) -Wl,-z,defs -o $(MAIN) $(OBJS) $(LIBS)
	strip $(MAIN)

.cpp.o:
	$(CXX) $(CXXFLAGS) -std=c++11 -c $< -o $@

thrift_line: line.thrift $(THRIFT_DEP)
	mkdir -p thrift_line
	$(THRIFT) --gen cpp -out thrift_line line.thrift

$(THRIFT):
	mkdir -p $(THRIFT_STATIC_DIR)
	wget -P $(THRIFT_STATIC_DIR) \
		http://mirror.netinch.com/pub/apache/thrift/0.9.2/thrift-0.9.2.tar.gz
	tar xf $(THRIFT_STATIC_DIR)/thrift-0.9.2.tar.gz	\
		-C $(THRIFT_STATIC_DIR) --strip-components 1
	cd $(THRIFT_STATIC_DIR); ./configure \
		--with-pic \
		--disable-tests --disable-tutorial \
		--without-libevent --without-zlib --without-qt4 --without-c_glib --without-csharp \
		--without-java --without-erlang --without-python --without-perl --without-php \
		--without-php_extension --without-ruby --without-haskell --without-go --without-d \
		--without-nodejs --without-lua  --without-openssl
	$(MAKE) -C $(THRIFT_STATIC_DIR)

.PHONY: clean
clean:
	rm -f $(MAIN)
	rm -f *.o
	rm -rf thrift_line
	rm -rf $(THRIFT_STATIC_DIR)

.PHONY: user-install
user-install: all
	install -D $(MAIN) ~/.purple/plugins/$(MAIN)

.PHONY: user-uninstall
user-uninstall:
	rm -f ~/.purple/plugins/$(MAIN)

.PHONY: install
install: all
	install -D $(MAIN) $(DESTDIR)$(PURPLE_PLUGIN_DIR)/$(MAIN)
	install -m 644 -D ../icons/16/line.png $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/16/line.png
	install -m 644 -D ../icons/22/line.png $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/22/line.png
	install -m 644 -D ../icons/48/line.png $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/48/line.png
	install -m 644 -D ../icons/scalable/line.svg $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/scalable/line.svg

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PURPLE_PLUGIN_DIR)/$(MAIN)
	rm -f $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/16/line.png
	rm -f $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/22/line.png
	rm -f $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/48/line.png
	rm -f $(DESTDIR)$(PURPLE_DATA_ROOT_DIR)/pixmaps/pidgin/protocols/scalable/line.svg

depend: .depend

.depend: thrift_line $(REAL_SRCS)
	rm -f .depend
	$(CXX) $(CXXFLAGS) -MM $(REAL_SRCS) >>.depend

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),user-install)
ifneq ($(MAKECMDGOALS),user-uninstall)
ifneq ($(MAKECMDGOALS),install)
ifneq ($(MAKECMDGOALS),uninstall)
-include .depend
endif
endif
endif
endif
endif
