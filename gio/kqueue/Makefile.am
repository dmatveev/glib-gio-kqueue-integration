include $(top_srcdir)/Makefile.decl

NULL =

noinst_LTLIBRARIES = libkqueue.la

libkqueue_la_SOURCES = 		\
	gkqueuefilemonitor.c		\
	gkqueuefilemonitor.h		\
	gkqueuedirectorymonitor.c	\
	gkqueuedirectorymonitor.h	\
	$(NULL)

libkqueue_la_CFLAGS = \
	-DG_LOG_DOMAIN=\"GLib-GIO\"	\
	$(gio_INCLUDES) 		\
	$(GLIB_DEBUG_FLAGS)		\
	-DGIO_MODULE_DIR=\"$(GIO_MODULE_DIR)\"	\
	-DGIO_COMPILATION		\
	-DG_DISABLE_DEPRECATED
