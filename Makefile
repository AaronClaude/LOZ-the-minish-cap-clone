.PHONY: all clean run

all:
	@$(MAKE) -C native all

clean:
	@$(MAKE) -C native clean

run:
	@$(MAKE) -C native run

