AM_CFLAGS = $(XORG_CFLAGS)

@DRIVER_NAME@_drv_la_LTLIBRARIES = @DRIVER_NAME@_drv.la
@DRIVER_NAME@_drv_la_LDFLAGS = -module -avoid-version
@DRIVER_NAME@_drv_ladir = @inputdir@

INCLUDES=-I$(top_srcdir)/include/

@DRIVER_NAME@_drv_la_SOURCES = @DRIVER_NAME@.c \
                               @DRIVER_NAME@.h
