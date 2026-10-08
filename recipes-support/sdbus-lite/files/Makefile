CC ?= gcc
AR ?= ar
AM_CFLAGS = -D_FILE_OFFSET_BITS=64 -D_FORTIFY_SOURCE=2 -fPIC
CFLAGS ?= -g -O2
CHECKFLAGS = -Wall -Werror -Wuninitialized -Wundef
DEPFLAGS = -Wp,-MMD,.$(@F).d,-MT,$@
CFLAGS := $(CHECKFLAGS) $(AM_CFLAGS) $(CFLAGS)
PKG_CONFIG ?= pkg-config

SDBUS_CFLAGS = $(shell $(PKG_CONFIG) --cflags libsystemd)
SDBUS_LIBS = $(shell $(PKG_CONFIG) --libs libsystemd)

INCLUDES = -I. $(SDBUS_CFLAGS)

LIB_NAME = sdbus_lite
SONAME_MAJOR = 1
LIB_VERSION = 1.0.0

SHLIB_FULL = lib$(LIB_NAME).so.$(LIB_VERSION)
SHLIB_SONAME = lib$(LIB_NAME).so.$(SONAME_MAJOR)
SHLIB_LINK = lib$(LIB_NAME).so
STATICLIB = lib$(LIB_NAME).a

OBJECTS = sdbus_lite.o
DEPS := $(patsubst %.o, .%.o.d, $(OBJECTS))

all: $(SHLIB_FULL) $(STATICLIB)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

$(SHLIB_FULL): $(OBJECTS)
	$(CC) -shared -Wl,-soname,$(SHLIB_SONAME) -o $@ $(OBJECTS) $(LDFLAGS) $(SDBUS_LIBS)
	ln -sf $(SHLIB_FULL) $(SHLIB_SONAME)
	ln -sf $(SHLIB_SONAME) $(SHLIB_LINK)

$(STATICLIB): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

clean:
	rm -f $(OBJECTS) $(DEPS) $(SHLIB_FULL) $(SHLIB_SONAME) $(SHLIB_LINK) $(STATICLIB)

-include $(DEPS)

.PHONY: all clean
