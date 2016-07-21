EXTRA_DIST = uri-decode.c test-wrapper globus-dsi-rest-gridftp-test.pl openssl.cnf

check_DATA = testcred.cert testcred.key testcred.link gridmap

check_PROGRAMS = \
	add-header-test \
	complete-callback-test \
	encode-form-data-test \
	handle-get-test \
	handle-release-test \
	progress-idle-timeout-test \
	read-json-test \
	request-test \
	response-test \
        retryable-test \
	set-request-test \
	write-block-test \
	write-form-test \
	write-json-test \
	write-multipart-test \
	uri-add-query-test \
	uri-escape-test

check_LTLIBRARIES = libglobus_gridftp_server_dsi_rest.la libtest_xio_server.la
LDADD = libtest_xio_server.la

libtest_xio_server_la_SOURCES = test-xio-server.c test-xio-server.h

X509_CERT_DIR = $(abs_builddir)
GRIDMAP = $(abs_builddir)/gridmap
TESTS_ENVIRONMENT = \
    export X509_USER_CERT=$$PWD/testcred.cert \
    X509_USER_KEY=$$PWD/testcred.key \
    X509_USER_PROXY=/dev/null \
    X509_CERT_DIR="$(X509_CERT_DIR)" \
    LIBTOOL="$(LIBTOOL)" \
    GLOBUS_HOSTNAME="localhost" \
    VALGRIND="$(VALGRIND)" \
    GRIDMAP="$(GRIDMAP)" ;

TEST_EXTENSIONS = .pl
LOG_COMPILER = $(LIBTOOL) --mode=execute $(VALGRIND)
PL_LOG_COMPILER = $(abs_srcdir)/test-wrapper

libglobus_gridftp_server_dsi_rest_la_SOURCES = test-dsi.c
libglobus_gridftp_server_dsi_rest_la_LDFLAGS = $(AM_LDFLAGS) -rpath $(libdir) -module

TESTS = $(check_PROGRAMS) \
	globus-dsi-rest-gridftp-test.pl

AM_CPPFLAGS = \
	-I $(top_srcdir) \
	$(GLOBUS_GRIDFTP_SERVER_CFLAGS) \
	$(GLOBUS_XIO_CFLAGS) \
	$(GLOBUS_COMMON_CFLAGS) \
	$(LIBCURL_CPPFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(JANSSON_CFLAGS) \
	-I$(top_srcdir)

AM_LDFLAGS = \
	$(GLOBUS_GRIDFTP_SERVER_LIBS) \
	$(GLOBUS_XIO_LIBS) \
	$(GLOBUS_COMMON_LIBS) \
	$(LIBCURL) \
	$(JANSSON_LIBS) \
	$(OPENSSL_LIBS) \
	../libglobus_dsi_rest.la


complete_callback_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
complete_callback_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

progress_idle_timeout_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
progress_idle_timeout_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

read_json_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
read_json_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

request_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
request_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

response_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
response_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

retryable_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
retryable_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_block_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_block_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_form_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_form_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_json_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_json_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_multipart_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_multipart_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

testcred.key:
	umask 077; openssl genrsa 1024 > $@

.key.req:
	printf "NA\nNA\nNA\nNA\nNA\nNA\n\n\n\n" | openssl req -new -key $< -config $(srcdir)/openssl.cnf > $@

.req.cert:
	umask 077; openssl x509 -req -days 365 -in testcred.req -signkey testcred.key > $@

.cert.link:
	linkname="$$(openssl x509 -hash -noout -in testcred.cert).0"; \
	rm -f "$$linkname"; \
	cp testcred.cert "$$linkname" ; \
	echo "$$linkname" > $@

gridmap: testcred.cert
	subject=`openssl x509 -subject -noout -in testcred.cert | sed -e 's/subject= *//'` ; \
	echo "\"$$subject\" $${LOGNAME:-`id -un`}" > gridmap

SUFFIXES = .key .req .cert .link
