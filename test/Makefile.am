EXTRA_DIST = uri-decode.c test-wrapper globus-dsi-rest-gridftp-test.pl openssl.cnf

check_DATA = \
	gridmap \
        testcred.cakey \
        testcred.cacert \
        testcred.key \
        testcred.cert \
	testcred.link \
	testcred.signing_policy

check_PROGRAMS = \
	add-header-test \
	complete-callback-test \
	encode-form-data-test \
	handle-get-test \
	handle-release-test \
	progress-idle-timeout-test \
	read-json-test \
	read-multipart-test \
	request-test \
	response-test \
        retryable-test \
	set-request-test \
	write-block-test \
	write-blocks-test \
	write-form-test \
	write-json-test \
	write-multipart-test \
	uri-add-query-test \
	uri-escape-test

check_LTLIBRARIES = libglobus_gridftp_server_dsi_rest.la libtest_xio_server.la
LDADD = libtest_xio_server.la

libtest_xio_server_la_SOURCES = test-xio-server.c test-xio-server.h

X509_CERT_DIR = $$($(CYGPATH_W) $(abs_builddir))
GRIDMAP = $$($(CYGPATH_W) $(abs_builddir)/gridmap)
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
	$(GLOBUS_COMMON_CFLAGS) \
	$(LIBCURL_CPPFLAGS) \
	$(OPENSSL_CFLAGS) \
	$(JANSSON_CFLAGS) \
	-I$(top_srcdir)

AM_LDFLAGS = \
	$(GLOBUS_GRIDFTP_SERVER_LIBS) \
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

read_multipart_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
read_multipart_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

request_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
request_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

response_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
response_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

retryable_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
retryable_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_block_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_block_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_blocks_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_blocks_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_form_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_form_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_json_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_json_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

write_multipart_test_CPPFLAGS = $(AM_CPPFLAGS) $(GLOBUS_XIO_CFLAGS)
write_multipart_test_LDFLAGS = $(AM_LDFLAGS) $(GLOBUS_XIO_LIBS)

testcred.cacert: testcred.cnf
	umask 077; $(OPENSSL) req -passout pass:globus -subj "/CN=ca" -new -x509 -extensions v3_ca -keyout $*.cakey -out $@ -config $<

.cacert.cakey:
	:

.cacert.link:
	linkname="`$(OPENSSL) x509 -hash -noout -in $<`.0"; \
	rm -f "$$linkname"; \
	cp $< "$$linkname"; \
        echo "$$linkname" > $@

.link.signing_policy:
	echo 01 > $*.srl
	linkname=`cat $<`; \
	policyfile=$${linkname%.0}.signing_policy; \
	echo "access_id_CA      X509         '/CN=ca'" > $${policyfile}; \
	echo "pos_rights        globus        CA:sign" >> $${policyfile}; \
	echo "cond_subjects     globus       '\"/*\"'" >> $${policyfile}; \
	echo $${policyfile} >> $@

testcred.key:
	umask 077; $(OPENSSL) genrsa 2048 > $@

testcred.req: testcred.key testcred.signing_policy
	$(OPENSSL) req -subj "/CN=test" -new -config $*.cnf -key $< > $@

.req.cert:
	umask 022; $(OPENSSL) x509 -passin pass:globus -req -days 365 -in $< -CA $*.cacert -CAkey $*.cakey > $@
gridmap: testcred.cert
	subject=`openssl x509 -subject -nameopt rfc2253,-dn_rev -noout -in testcred.cert | sed -e 's|subject= *|/|' -e 's|,|/|g'` ; \
	echo "\"$$subject\" $${LOGNAME:-`id -un`}" > gridmap

CLEANFILES=$(check_DATA)
SUFFIXES = .key .req .cert .link
