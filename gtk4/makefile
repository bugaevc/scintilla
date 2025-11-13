.PHONY: static shared all clean analyze depend gir doc

.SUFFIXES: .cxx .c .o .h .a .list

srcdir ?= .
export basedir = $(srcdir)/..

WARNINGS = -Wpedantic -Wall
ifdef CLANG
CXX = clang++
CC = clang
WARNINGS += -Wno-deprecated-register
ifdef windir
# Turn off some warnings that occur when Clang is being used on Windows where it
# is including Microsoft headers.
# incompatible-ms-struct is because more complex structs are not quite the same as MSVC
WARNINGS += -Wno-incompatible-ms-struct
# language-extension-token is because of __int64 in glib-2.0 glibconfig.h
WARNINGS += -Wno-language-extension-token
# register may be used in glib
# This produces a warning since -Wno-register is not valid for C files but it still works
WARNINGS += -Wno-register
endif
# Can choose aspect to sanitize: address and undefined can simply change SANITIZE but for
# thread also need to create Position Independent Executable -> search online documentation
SANITIZE = address
#SANITIZE = undefined
BASE_FLAGS += -fsanitize=$(SANITIZE)
endif
ARFLAGS = rc
RANLIB ?= ranlib
PKG_CONFIG ?= pkg-config

# Environment variable windir always defined on Win32

# Enable Position Independent Code except on Windows where it is the default so the flag produces a warning
ifndef windir
BASE_FLAGS += -fPIC
ifeq ($(shell uname),Darwin)
LDFLAGS += -dynamiclib
endif
endif

VERSION_SCRIPT = Versions.ld
LDFLAGS += -shared -Wl,--no-undefined -Wl,--version-script=$(VERSION_SCRIPT)

# Take care of changing Unix style '/' directory separator to '\' on Windows
normalize = $(if $(windir),$(subst /,\,$1),$1)

PYTHON = $(if $(windir),pyw,python3)

SHAREDEXTENSION = $(if $(windir),dll,so)

ifdef windir
CC = gcc
DEL = del /q
else
DEL = rm -rf
endif
export DEL
COMPLIB=$(basedir)/bin/scintilla.a
export COMPONENT=$(basedir)/bin/libscintilla.$(SHAREDEXTENSION)

GIR = Scintilla-5.gir
VAPI = scintilla.vapi

vpath %.h $(srcdir) $(basedir)/src $(basedir)/include
vpath %.c $(srcdir)
vpath %.cxx $(srcdir) $(basedir)/src

INCLUDES=-I $(basedir)/include -I $(basedir)/src
BASE_FLAGS += $(WARNINGS)
DEFINES += -DG_LOG_DOMAIN=\"Scintilla\" -DGETTEXT_PACKAGE=\"scintilla\" -D SCINTILLA_COMPILATION

ifdef NO_CXX11_REGEX
DEFINES += -DNO_CXX11_REGEX
endif

DEFINES += -D$(if $(DEBUG),DEBUG,NDEBUG)
BASE_FLAGS += $(if $(DEBUG),-g,-O3)

BASE_FLAGS += -fvisibility=hidden -fvisibility-inlines-hidden
CXX_BASE_FLAGS =--std=c++17 $(BASE_FLAGS)
CXX_ALL_FLAGS =$(DEFINES) $(INCLUDES) $(CXX_BASE_FLAGS) $(CONFIG_FLAGS)

CONFIG_FLAGS:=$(shell $(PKG_CONFIG) --cflags gtk4)
CONFIGLIB:=-Wl,--as-needed $(shell $(PKG_CONFIG) --libs gtk4)
MARSHALLER=scintilla-marshal.o

all: $(COMPLIB) $(COMPONENT) $(GIR) $(VAPI)

static: $(COMPLIB)

shared: $(COMPONENT)

clean:
	$(DEL) *.o *.plist \
		$(call normalize,$(COMPLIB)) \
		$(call normalize,$(COMPONENT)) \
		$(GIR) $(VAPI) \
		doc/Scintilla-5 \
		ScintillaEnums.h ScintillaEnums.c scintilla-marshal.h
	$(MAKE) -C ScintillaDemo clean

%.o: %.cxx
	$(CXX) $(CPPFLAGS) $(CXX_ALL_FLAGS) $(CXXFLAGS) -c $<
%.o: %.c
	$(CC) $(CPPFLAGS) $(DEFINES) $(INCLUDES) $(CONFIG_FLAGS) $(BASE_FLAGS) $(CFLAGS) -w -c $<

ScintillaEnums.h: ScintillaEnums.h.template EnumGen.py $(basedir)/include/Scintilla.iface
	$(PYTHON) EnumGen.py

ScintillaView.o ScintillaDocument.o PlatGTK.o ScintillaGTK.o: ScintillaEnums.h

GLIB_MKENUMS = glib-mkenums
GLIB_MKENUMS_FLAGS = --identifier-prefix=Scintilla

ScintillaEnums.c: ScintillaEnums.c.template ScintillaEnums.h
	$(GLIB_MKENUMS) $(GLIB_MKENUMS_FLAGS) --template ScintillaEnums.c.template --output ScintillaEnums.c ScintillaEnums.h

GLIB_GENMARSHAL = glib-genmarshal
GLIB_GENMARSHAL_FLAGS = --prefix=scintilla_marshal --valist-marshallers

%.h: %.list
	$(GLIB_GENMARSHAL) --header $(GLIB_GENMARSHAL_FLAGS) $< > $@
%.c: %.list
	$(GLIB_GENMARSHAL) --body $(GLIB_GENMARSHAL_FLAGS) $< > $@

G_IR_SCANNER = g-ir-scanner
G_IR_SCANNER_FLAGS = --warn-all --strict --no-libtool \
	--include=Gtk-4.0 \
	-l scintilla -L ../bin/ \
	--namespace Scintilla --nsversion 5 \
	--identifier-prefix Scintilla --identifier-prefix SC --symbol-prefix scintilla --accept-unprefixed \
	--c-include ScintillaView.h --c-include ScintillaDocument.h

$(GIR): ScintillaView.h ScintillaView.cxx ScintillaDocument.h ScintillaDocument.cxx ScintillaEnums.h $(COMPONENT)
	$(G_IR_SCANNER) $(G_IR_SCANNER_FLAGS) --output=$(GIR) \
		ScintillaView.h ScintillaView.cxx \
		ScintillaDocument.h ScintillaDocument.cxx \
		ScintillaEnums.h
gir: $(GIR)

GI_DOCGEN = gi-docgen
doc: $(GIR)
	$(GI_DOCGEN) generate -C doc/scintilla.toml --output-dir doc/ $(GIR)

VAPIGEN = vapigen
VAPIGEN_FLAGS = --library scintilla --pkg gtk4 --metadatadir .

$(VAPI): $(GIR) Scintilla-5.metadata
	$(VAPIGEN) $(VAPIGEN_FLAGS) $(GIR)

vapi: $(VAPI)

analyze:
	clang --analyze $(DEFINES) $(INCLUDES) $(CONFIG_FLAGS) $(CXX_BASE_FLAGS) $(CXXFLAGS) $(srcdir)/*.cxx $(basedir)/src/*.cxx

depend deps.mak:
	$(PYTHON) DepGen.py

# Required for base Scintilla
SRC_OBJS = \
	AutoComplete.o \
	CallTip.o \
	CaseConvert.o \
	CaseFolder.o \
	CellBuffer.o \
	ChangeHistory.o \
	CharacterCategoryMap.o \
	CharacterType.o \
	CharClassify.o \
	ContractionState.o \
	DBCS.o \
	Decoration.o \
	Document.o \
	EditModel.o \
	Editor.o \
	EditView.o \
	Geometry.o \
	Indicator.o \
	KeyMap.o \
	LineMarker.o \
	MarginView.o \
	PerLine.o \
	PositionCache.o \
	RESearch.o \
	RunStyles.o \
	Selection.o \
	Style.o \
	UndoHistory.o \
	UniConversion.o \
	UniqueString.o \
	ViewStyle.o \
	XPM.o

GTK_OBJS = \
	ScintillaBase.o \
	ScintillaView.o \
	ScintillaDocument.o \
	ScintillaEnums.o \
	ScintillaGTK.o \
	PlatGTK.o \
	SurfaceGTK.o \
	ListBoxGTK.o
	# ScintillaGTKAccessible.o

$(COMPLIB): $(SRC_OBJS) $(GTK_OBJS) $(MARSHALLER)
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

$(COMPONENT): $(SRC_OBJS) $(GTK_OBJS) $(MARSHALLER)
	$(CXX) $(CXX_ALL_FLAGS) $(CXXFLAGS) $(LDFLAGS) $^ -o $@ $(CONFIGLIB)

# $(COMPONENT): $(VERSION_SCRIPT)

demo: $(COMPONENT) $(VAPI)
	$(MAKE) -C ScintillaDemo

# Automatically generate header dependencies with "make depend"
include deps.mak
