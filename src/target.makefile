TARGET_ARCHITECTURE_NAME=$(if $(SDKTARGETSYSROOT),$(notdir $(SDKTARGETSYSROOT)),$(shell uname -m))
OBJDIR=obj/$(TARGET_ARCHITECTURE_NAME)/
O=obj/$(TARGET_ARCHITECTURE_NAME)/

all::

show_target:
	@echo "TARGET_ARCHITECTURE_NAME:'$(TARGET_ARCHITECTURE_NAME)'"
	@echo "OBJDIR:'$(OBJDIR)'"
	@echo "O:'$(O)'"

$(OBJDIR)%.o : %.c
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o : %.cpp
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o : %.cc
	$(COMPILE.cc) $(OUTPUT_OPTION) $<

$(OBJDIR)%.o : %.C
	$(COMPILE.C) $(OUTPUT_OPTION) $<

$(OBJDIR)% : $(OBJDIR)%.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@
