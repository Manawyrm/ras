SUBDIRS = .

# This is _NOT_ the library release version, it's an API version.
# Please read Chapter 6 "Library interface versions" of the libtool
# documentation before making any modification
LIBVERSION=2:1:1

AM_CPPFLAGS = $(all_includes) -I$(top_srcdir)/include -I$(top_builddir)
AM_CFLAGS=-Wall -Wextra -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unused-result $(LIBOSMOCORE_CFLAGS)

noinst_HEADERS = \
	$(NULL)

bin_PROGRAMS = \
	yate_hdlc_ppp \
	test \
	$(NULL)

bin_PROGRAMS = \
	test \
	yate_hdlc_ppp \
	$(NULL)

noinst_PROGRAMS = \
	$(NULL)

yate_hdlc_ppp_SOURCES = \
	pppd.c \
	pty.c \
	yate_message.c \
	yate.c \
	yate_hdlc_ppp.c \
	$(NULL)

yate_hdlc_ppp_LDADD = $(LIBOSMOCORE_LIBS)

test_SOURCES = \
	pppd.c \
	pty.c \
	test.c \
	$(NULL)

test_LDADD = $(LIBOSMOCORE_LIBS)