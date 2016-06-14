SUBDIRS = . test
ACLOCAL_AMFLAGS = -I m4

pcdir = $(libdir)/pkgconfig

lib_LTLIBRARIES = libglobus_dsi_rest.la
pc_DATA = globus-dsi-rest.pc
include_HEADERS = globus_dsi_rest.h
doc_DATA = GLOBUS_LICENSE

AM_CPPFLAGS = \
	$(GLOBUS_GRIDFTP_SERVER_CFLAGS) \
	$(GLOBUS_COMMON_CFLAGS) \
	$(LIBCURL_CPPFLAGS) \
	$(JANSSON_CFLAGS) \
	$(OPENSSL_CFLAGS)

AM_LDFLAGS = \
	$(GLOBUS_GRIDFTP_SERVER_LIBS) \
	$(GLOBUS_COMMON_LIBS) \
	$(LIBCURL) \
	$(JANSSON_LIBS) \
	$(OPENSSL_LIBS)

libglobus_dsi_rest_la_SOURCES = \
	globus_dsi_rest.h \
	globus_i_dsi_rest.h \
	version.h \
	add_header.c \
	buffer_get.c \
	compute_headers.c \
	encode_form_data.c \
	handle_get.c \
	handle_release.c \
	header.c \
	module.c \
	perform.c \
	progress.c \
	progress_idle_timeout.c \
	read_data.c \
	read_gridftp_op.c \
	read_json.c \
	request.c \
	set_request.c \
	uri_add_query.c \
	uri_escape.c \
	write_block.c \
	write_form.c \
	write_data.c \
	write_gridftp_op.c \
	write_json.c \
	xferinfo.c

EXTRA_DIST = $(doc_DATA)

DOC_STAMPS =

if ENABLE_DOXYGEN
DOC_STAMPS += doc-stamp

doc-stamp: Doxyfile $(libglobus_common_la_SOURCES)
	rm -rf doc
	$(DOXYGEN)
	touch $@

clean-local:
	rm -rf doc doc-stamp
endif

all-local: $(DOC_STAMPS)

install-data-local: $(DOC_STAMPS)
	if test -d doc/man/man3; then \
                install -d -m 755 $(DESTDIR)$(mandir)/man3; \
                $(INSTALL) -m 644 doc/man/man3/[Gg][Ll][Oo][Bb][Uu][Ss]_[Dd][Ss][Ii]*.3 $(DESTDIR)$(mandir)/man3 ; \
        fi
	if test -d doc/html; then \
                for dir in `(cd doc; find html -type d)`; do \
                        install -d -m 755 $(DESTDIR)$(docdir)/$$dir; \
                done ; \
                for f in `(cd doc; find html -type f)`; do \
                        $(INSTALL) -m 644 doc/$$f $(DESTDIR)$(docdir)/$$f ; \
                done ; \
        fi

