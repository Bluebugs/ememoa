# -*- Makefile -*-

SUBDIRS = build-aux include src/lib/ememoa test doc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = ememoa.pc

ACLOCAL_AMFLAGS = -I build-aux

EXTRA_DIST	= bootstrap

MAINTAINERCLEANFILES =				\
	configure				\
	Makefile.in				\
	config.log				\
	config.status				\
	stamp-h1				\
	config.h				\
	config.h.in				\
	libtool					\
	aclocal.m4

if ENABLE_COVERAGE
lcov-reset:
	@rm -rf coverage
	@find . -name "*.gcda" -exec rm {} \;
	@lcov --directory . --zerocounters

lcov-report:
	@mkdir coverage
	@lcov --compat-libtool --directory . --capture --output-file coverage/coverage.info
	@lcov -l coverage/coverage.info | grep -v "`cd $(top_srcdir) && pwd`" | cut -d: -f1 > coverage/remove
	@lcov -r coverage/coverage.info `cat coverage/remove` > coverage/coverage.cleaned.info
	@rm coverage/remove
	@mv coverage/coverage.cleaned.info coverage/coverage.info
	@genhtml -t "$(PACKAGE_STRING)" -o coverage coverage/coverage.info

coverage:
	@make lcov-reset
	@make check
	@make lcov-report

clean-local:
	@rm -rf coverage
else
lcov-reset:
	@echo "reconfigure with --enable-gcov"

lcov-report:
	@echo "reconfigure with --enable-gcov"

coverage:
	@echo "reconfigure with --enable-gcov"
endif
