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
	yate_x75 \
	yate_v110 \
	fax_test \
	test \
	$(NULL)

noinst_PROGRAMS = \
	$(NULL)

yate_hdlc_ppp_SOURCES = \
    gsmtap.c \
	ppp/pppd.c \
	ppp/pty.c \
	yate_message.c \
	yate.c \
	yate_hdlc_ppp.c \
	$(NULL)

yate_hdlc_ppp_LDADD = $(LIBOSMOCORE_LIBS)

yate_tty_SOURCES = \
	yate_message.c \
	yate.c \
	ttytdd/minimodem.c \
	telnet.c \
	yate_tty.c \
	$(NULL)

yate_tty_LDADD = $(LIBOSMOCORE_LIBS)

yate_x75_SOURCES = \
	yate_message.c \
	yate.c \
	telnet.c \
	x75/x75_in.c \
	x75/x75_out.c \
    x75/x75_subr.c \
    x75/x75_timer.c \
    x75/x75_iface.c \
	yate_x75.c \
	gsmtap.c \
	$(NULL)

yate_x75_LDADD = $(LIBOSMOCORE_LIBS)

yate_v110_SOURCES = \
	yate_message.c \
	yate.c \
	telnet.c \
	yate_v110.c \
	gsmtap.c \
	$(NULL)

yate_v110_LDADD = $(LIBOSMOCORE_LIBS) $(LIBOSMOTRAU_LIBS) $(LIBOSMOISDN_LIBS)

fax_test_SOURCES = \
	yate_message.c \
	yate.c \
	telnet.c \
	fax_decode.c \
    fax_test.c \
	gsmtap.c \
	$(NULL)

fax_test_LDADD = $(LIBOSMOCORE_LIBS) $(LIBOSMOTRAU_LIBS) $(LIBOSMOISDN_LIBS) $(LIBSPANDSP_LIBS)

file_hdlc_decoder_SOURCES = \
    gsmtap.c \
	file_hdlc_decoder.c \
	$(NULL)

file_hdlc_decoder_LDADD = $(LIBOSMOCORE_LIBS)

test_SOURCES = \
	ppp/pppd.c \
	ppp/pty.c \
	test.c \
	$(NULL)

test_LDADD = $(LIBOSMOCORE_LIBS)