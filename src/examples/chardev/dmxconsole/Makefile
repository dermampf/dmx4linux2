BININSTALLPATH=$(DMXPREFIX)/bin
MANINSTALLPATH=$(DMXPREFIX)/man/man1

DMXCONSOLELIBS+= -Lpointer pointer/pointer.a
CFLAGS+= -Ipointer
CXXFLAGS+= -Ipointer

ifdef CONFIG_HAVE_LIRC
CFLAGS+=	-DHAVE_LIRC=1
CXXFLAGS+=	-DHAVE_LIRC=1
DMXCONSOLELIBS+=-llirc_client
endif


CFLAGS += -I. -I../../../include/
CXXFLAGS += -I. -I../../../include/



ifdef CONFIG_HAVE_GPM
DMXCONSOLELIBS+= -lgpm
endif

TARGETS=	dmxconsole
MANPAGES=	dmxconsole.1

all:	$(TARGETS)

dmxconsole : dmxconsole.o pointer/getarg.o
	$(MAKE) -C pointer all
	$(CXX) $(LDFLAGS) -o $@ $^ -lncurses  $(DMXCONSOLELIBS)

clean:
	$(RM) $(TARGETS) *.o *.chn
	$(MAKE) -C pointer $@

distclean: uninstall clean

uninstall:
	cd $(BININSTALLPATH) && $(RM) $(TARGETS)
	cd $(MANINSTALLPATH) && $(RM) $(MANPAGES)
	$(RM) /etc/bash_completion.d/dmx4linux

install:	$(TARGETS) $(MANPAGES)
	mkdir -p $(BININSTALLPATH)
	install -m 755 $(TARGETS) $(BININSTALLPATH)
	mkdir -p $(MANINSTALLPATH)
	install -m 644 $(MANPAGES) $(MANINSTALLPATH)
	if [ -d /etc/bash_completion.d ]; then install -m 755 bash_completion.sh /etc/bash_completion.d/dmx4linux; fi




obj/%.o : %.cpp
	$(CXX) -c -o $@ $(CXXFLAGS) $<

obj/%.o : ../%.cpp
	$(CXX) -c -o $@ $(CXXFLAGS) $<

obj/%.o : ../dmxfrontend/%.cpp
	$(CXX) -c -o $@ $(CXXFLAGS) $<

obj/%.o : ../../IoCore/%.cpp
	$(CXX) -c -o $@ $(CXXFLAGS) $<
