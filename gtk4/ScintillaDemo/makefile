all: demo

VALAC = valac
BLUEPRINT_COMPILER = blueprint-compiler
GLIB_COMPILE_RESOURCES = glib-compile-resources

VALAFLAGS = --target-glib=auto --enable-experimental-non-null
ifdef DEBUG
VALAFLAGS += -g
endif

DEMO_SRC = \
	main.vala \
	window.vala \
	tab.vala \
	application.vala

%.ui: %.blp
	$(BLUEPRINT_COMPILER) compile $^ --output $@

GRESOURCES = gresources.xml
RESOURCES = \
	menus.ui \
	window.ui \
	tab.ui

gresources.c: $(GRESOURCES) $(RESOURCES)
	$(GLIB_COMPILE_RESOURCES) --target=$@ --generate-source $(GRESOURCES)

VALAEXTRAFLAGS = \
	--vapidir .. \
	-X -I.. \
	-X -L../$(basedir)/bin \
	-X -Wl,-rpath,'$$ORIGIN/../$(basedir)/bin' \
	-X -lscintilla \
	-X -Wno-discarded-qualifiers

demo: $(DEMO_SRC) gresources.c
	$(VALAC) $(VALAFLAGS) $^ -o $@ --gresources=$(GRESOURCES) \
		--pkg gtk4 --pkg scintilla \
		-X -DGETTEXT_PACKAGE=\"ScintillaDemo\" \
		$(VALAEXTRAFLAGS)

clean:
	$(DEL) demo *.ui gresources.c *.vala.c

.PHONY: clean all
