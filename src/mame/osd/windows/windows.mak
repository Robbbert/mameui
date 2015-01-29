###########################################################################
#
#   windows.mak
#
#   MAME Windows-specific makefile
#
###########################################################################

MAME_WINSRC = src/MAME/osd/windows
MAME_WINOBJ = $(OBJ)/MAME/osd/windows

OBJDIRS += \
	$(MAMEOBJ)/osd \
	$(MAMEOBJ)/osd/windows

RESFILE = $(MAME_WINOBJ)/mame.res

$(LIBOSD): $(OSDOBJS)

$(LIBOCORE): $(OSDCOREOBJS)

$(LIBOCORE_NOMAIN): $(OSDCOREOBJS:$(WINOBJ)/main.o=)

$(RESFILE): $(MAME_WINSRC)/mame.rc $(WINOBJ)/mamevers.rc

#-------------------------------------------------
# generic rules for the resource compiler
#-------------------------------------------------

$(MAME_WINOBJ)/%.res: $(MAME_WINSRC)/%.rc
	@echo Compiling resources $<...
	$(RC) $(RCDEFS) $(RCFLAGS) --include-dir MAME/$(OSD) -o $@ -i $<
