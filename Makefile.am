ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = data po src

AM_DISTCHECK_CONFIGURE_FLAGS = \
	--disable-schemas-install						\
	--with-nautilusdir='$${libdir}/nautilus/extensions-2.0-distcheck'	\
	--with-systemduserunitdir='$${libdir}/systemd/user-distcheck'

# Build ChangeLog from GIT history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		pushd $(top_srcdir) ; \
		git log --stat --after="Thu Jun 11 22:19:01 2009" > $@; \
		popd ; \
	fi

dist: ChangeLog

.PHONY: ChangeLog


-include $(top_srcdir)/git.mk
