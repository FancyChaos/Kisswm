include config.mk

SRC_DIR := src
OBJ_DIR := obj

EXE := kisswm

CONF_DEFAULT := $(SRC_DIR)/$(EXE).h
CONF_CUSTOM := config.h

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $^ $(LIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c config | $(OBJ_DIR)
	mv $(CONF_DEFAULT) $(CONF_DEFAULT).default
	cp $(CONF_CUSTOM) $(CONF_DEFAULT)
	$(CC) $(CFLAGS) -c $< -o $@ ; mv $(CONF_DEFAULT).default $(CONF_DEFAULT)

$(OBJ_DIR):
	mkdir -p $@

config:
	test -f $(CONF_CUSTOM) || cp $(CONF_DEFAULT) $(CONF_CUSTOM)

install: kisswm
	rm $(DESTDIR)$(PREFIX)/bin/kisswm || true
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp kisswm $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/kisswm

uninstall:
	rm -rf $(DESTDIR)$(PREFIX)/bin/kisswm

clean:
	@$(RM) -rv $(EXE) $(OBJ_DIR)

-include $(OBJ:.o=.d)

.PHONY: all install uninstall clean
