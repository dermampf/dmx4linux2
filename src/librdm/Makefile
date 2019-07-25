CXXFLAGS+=-I.
CXXFLAGS+=-I../include/
CXXFLAGS+=-std=c++11
CXXFLAGS+=-g -ggdb
LDLIBS+=-lpthread
LDLIBS+=-lreadline

TARGETS=$(OBJDIR)rdmresponder $(OBJDIR)rdmtool $(OBJDIR)rdmshell
all :: $(TARGETS)

$(OBJDIR)rdmresponder : $(OBJDIR)rdmresponder.o $(OBJDIR)dmx4linux2.o $(OBJDIR)dmx4linux2interface.o dmx.o $(OBJDIR)rdmdevice.o $(OBJDIR)dmxbus.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(OBJDIR)rdmtool : $(OBJDIR)rdmtool.o $(OBJDIR)dmx4linux2.o $(OBJDIR)dmx4linux2interface.o $(OBJDIR)dmx.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(OBJDIR)rdmshell : $(OBJDIR)rdmshell.o $(OBJDIR)rdmcommands.o  $(OBJDIR)dmx4linux2.o $(OBJDIR)dmx4linux2interface.o $(OBJDIR)dmx.o $(OBJDIR)rdmapi.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

$(OBJDIR)rdmshell.o : rdmshell.cpp commandinterpreter.hpp rdmcommands.hpp
$(OBJDIR)rdmcommands.o : rdmcommands.cpp rdmcommands.hpp

clean:
	-rm -f $(OBJDIR)*.o *~ $(TARGETS)
