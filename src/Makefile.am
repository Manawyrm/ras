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
    file_hdlc_decoder \
	yate_hdlc_ppp \
	yate_tty \
	test \
	$(NULL)

noinst_PROGRAMS = \
	$(NULL)

yate_hdlc_ppp_SOURCES = \
    gsmtap.c \
	pppd.c \
	pty.c \
	yate_message.c \
	yate.c \
	yate_hdlc_ppp.c \
	$(NULL)

yate_hdlc_ppp_LDADD = $(LIBOSMOCORE_LIBS)

yate_tty_SOURCES = \
	yate_message.c \
	yate.c \
	minimodem.c \
	yate_tty.c \
	$(NULL)

yate_tty_LDADD = $(LIBOSMOCORE_LIBS)

file_hdlc_decoder_SOURCES = \
    gsmtap.c \
	file_hdlc_decoder.c \
	$(NULL)

file_hdlc_decoder_LDADD = $(LIBOSMOCORE_LIBS)

test_SOURCES = \
	pppd.c \
	pty.c \
	test.c \
	$(NULL)

test_LDADD = $(LIBOSMOCORE_LIBS)