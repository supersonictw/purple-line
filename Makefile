all:
	$(MAKE) -C libpurple

.PHONY: clean
clean:
	$(MAKE) -C libpurple clean

.PHONY: user-install
user-install:
	$(MAKE) -C libpurple user-install

.PHONY: user-uninstall
user-uninstall:
	$(MAKE) -C libpurple user-uninstall

.PHONY: install
install: all
	$(MAKE) -C libpurple install

.PHONY: uninstall
uninstall:
	$(MAKE) -C libpurple uninstall
