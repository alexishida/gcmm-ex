.PHONY: default all clean wii wii-clean gc gc-clean runwii rungc run

default: all

all: wii gc

clean: gc-clean wii-clean
	@rm -rf build_GC_dark build_WII_dark

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

gc:
	$(MAKE) -f Makefile.gc

gc-clean:
	$(MAKE) -f Makefile.gc clean

runwii:
	$(MAKE) -f Makefile.wii run

rungc:
	$(MAKE) -f Makefile.gc run

run: runwii
