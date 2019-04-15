all : modules examples

modules:
	$(MAKE) -C drivers $@

.PHONY: examples
examples:
	$(MAKE) -C examples/chardev

clean:
	$(MAKE) -C examples/chardev $@
	$(MAKE) -C drivers $@
