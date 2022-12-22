include config.mk

SRC_DIR := src
OBJ_DIR := obj

EXE := kisswm

CONF_DEFAULT := $(SRC_DIR)/$(EXE).h
CONF_CUSTOM := config.h

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: config $(EXE)
	mv $(CONF_DEFAULT).default $(CONF_DEFAULT)

$(EXE): $(OBJ)
	$(CC) $^ $(LIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $@

config:
	test -f $(CONF_CUSTOM) || cp $(CONF_DEFAULT) $(CONF_CUSTOM)
	test -f $(CONF_DEFAULT).default || mv $(CONF_DEFAULT) $(CONF_DEFAULT).default
	cp $(CONF_CUSTOM) $(CONF_DEFAULT)

install:
	test -f $(EXE)
	rm $(DESTDIR)$(PREFIX)/bin/$(EXE) || true
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp kisswm $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(EXE)

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/bin/kisswm

clean:
	@$(RM) -rv $(EXE) $(OBJ_DIR)

-include $(OBJ:.o=.d)

.PHONY: all install uninstall clean
