/*-
 * Copyright (c) 2010 Gleb Smirnoff <glebius@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: egalax.c,v 1.2 2010/07/01 21:05:30 glebius Exp $
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <X11/keysym.h>

#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <xf86.h>
#include <xf86Xinput.h>
#include <xf86_OSproc.h>
#include <exevents.h>
#include <xorgVersion.h>
#include <xkbsrv.h>

static XF86ModuleVersionInfo eGalaxVersionRec =
{
    "egalax",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR,
    PACKAGE_VERSION_PATCHLEVEL,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}
};

static void eGalaxUnplug(pointer p);
static pointer eGalaxPlug(pointer, pointer, int *, int *);
static int eGalaxPreInit(InputDriverPtr, InputInfoPtr, int);
static void eGalaxUnInit(InputDriverPtr, InputInfoPtr, int);
static void eGalaxReadInput(InputInfoPtr);
static int eGalaxControl(DeviceIntPtr, int);
static int eGalaxInitButtons(DeviceIntPtr);

static void eGalaxConfigAxes(DeviceIntPtr);
static void eGalaxCalib(InputInfoPtr, int, int);

_X_EXPORT InputDriverRec EGALAX = {
	1,			/* driver version */
	"egalax",		/* driver name */
	NULL,			/* identify */
	eGalaxPreInit,		/* pre-init */
	eGalaxUnInit,		/* un-init */
	NULL,			/* module */
	0			/* ref count */
};

_X_EXPORT XF86ModuleData egalaxModuleData =
{
	&eGalaxVersionRec,
	eGalaxPlug,
	eGalaxUnplug
};

struct eGalaxDeviceRec {
	char		*device;
	u_short		minx, maxx, miny, maxy;
	u_char		revy;

	/* Second button emulator. */
	u_char		button, area, pause;
	u_short		ox, oy;
	struct timeval	tv;
};

typedef struct eGalaxDeviceRec * eGalaxDevicePtr;

static void
eGalaxUnplug(pointer p)
{
}

static pointer
eGalaxPlug(pointer module, pointer options, int *errmaj, int *errmin)
{
	xf86AddInputDriver(&EGALAX, module, 0);
	return (module);
}

static int
eGalaxPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	eGalaxDevicePtr sc;

	if ((sc = calloc(1, sizeof(struct eGalaxDeviceRec))) == NULL)
		return (BadAlloc);

	pInfo->type_name = XI_MOUSE; /* see XI.h */
	pInfo->device_control = eGalaxControl; /* enable/disable dev */
	pInfo->read_input = eGalaxReadInput; /* new data avl */
	pInfo->control_proc = NULL;
	pInfo->switch_mode = NULL;
	pInfo->dev = NULL;
	pInfo->private = sc;

	/* process driver specific options */
	sc->device = xf86SetStrOption(pInfo->options, "Device", "/dev/uep0");
	xf86Msg(X_INFO, "%s: Using device %s.\n", pInfo->name, sc->device);

	/* process generic options */
	xf86CollectInputOptions(pInfo, NULL);
	xf86ProcessCommonOptions(pInfo, pInfo->options);

	/*
	 * Default values are initially much narrower than real,
	 * the screen will autocalibrate soon.
	 */
	sc->minx = xf86SetIntOption(pInfo->options, "MinX", 500);
	sc->maxx = xf86SetIntOption(pInfo->options, "MaxX", 1500);
	sc->miny = xf86SetIntOption(pInfo->options, "MinY", 500);
	sc->maxy = xf86SetIntOption(pInfo->options, "MaxY", 1500);

	sc->revy = xf86SetBoolOption(pInfo->options, "ReverseY", TRUE);

	sc->area = xf86SetIntOption(pInfo->options, "RightClickEmulArea", 5);
	sc->pause = xf86SetIntOption(pInfo->options, "RightClickEmulPause", 1);

	/* Open sockets, init device files, etc. */
	pInfo->fd = open(sc->device, O_RDWR | O_NONBLOCK);
	if (pInfo->fd == -1) {
		xf86Msg(X_ERROR, "%s: failed to open %s.", pInfo->name,
		    sc->device);
		pInfo->private = NULL;
		free(sc);
		xf86DeleteInput(pInfo, 0);
		return (BadValue);
	}
	close(pInfo->fd);
	pInfo->fd = -1;

	return (Success);
}

static void
eGalaxUnInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
	eGalaxDevicePtr sc = pInfo->private;

	if (sc->device)
		free(sc->device);
	free(sc);
	xf86DeleteInput(pInfo, 0);
}

static int
eGalaxControl(DeviceIntPtr device, int what)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	eGalaxDevicePtr sc = pInfo->private;
	int ret = Success;

	switch (what) {
	case DEVICE_INIT:
		ret = eGalaxInitButtons(device);
		if (ret == Success)
			eGalaxConfigAxes(device);
		break;

	/* Switch device on.  Establish socket, start event delivery.  */
	case DEVICE_ON:
		xf86Msg(X_INFO, "%s: On.\n", pInfo->name);
		if (device->public.on)
			break;

		pInfo->fd = open(sc->device, O_RDONLY | O_NONBLOCK);
		if (pInfo->fd < 0) {
			xf86Msg(X_ERROR, "%s: cannot open device.\n",
			    pInfo->name);
			return (BadRequest);
		}

		xf86FlushInput(pInfo->fd);
		xf86AddEnabledDevice(pInfo);
		device->public.on = TRUE;
		break;

	case DEVICE_OFF:
		xf86Msg(X_INFO, "%s: Off.\n", pInfo->name);
		if (!device->public.on)
			break;
		xf86RemoveEnabledDevice(pInfo);
		close(pInfo->fd);
		pInfo->fd = -1;
		device->public.on = FALSE;
		break;

	case DEVICE_CLOSE:
		/* free what we have to free */
		break;
	}

	return (ret);
}

static void
eGalaxReadInput(InputInfoPtr pInfo)
{
	eGalaxDevicePtr sc = pInfo->private;
	char buf[5];
	int x, y, nread, reinit;

	while(xf86WaitForInput(pInfo->fd, 0) > 0) {
		nread = read(pInfo->fd, &buf, 5);
		if (nread <= 0) {
			if (errno == ENXIO) {
				xf86Msg(X_ERROR, "%s: Device disappeared\n",
				    pInfo->name);
				xf86RemoveEnabledDevice(pInfo);
				close(pInfo->fd);
				pInfo->fd = -1;
			} else if (errno != EAGAIN)
				xf86Msg(X_ERROR, "%s: Read error: %s\n",
				    pInfo->name, strerror(errno));
			return;
		}
		if (nread < 5) {
			xf86Msg(X_ERROR, "%s: bad packet len %u.\n",
			    pInfo->name, nread);
			return;
		}

		x = (buf[1] << 7) | buf[2];
		y = (buf[3] << 7) | buf[4];

		if (x < sc->minx || x > sc->maxx ||
		    y < sc->miny || y > sc->maxy)
			eGalaxCalib(pInfo, x, y);

		if (sc->revy)
			y = sc->maxy - y + sc->miny;

		xf86PostMotionEvent(pInfo->dev,
		    TRUE, /* is_absolute */
		    0, /* first_valuator */
		    2, /* num_valuators */
		    x, y);

		/* Touchpad is released. */
		if ((buf[0] & 0x01) == 0 && sc->button == 0)
			continue;

		if ((buf[0] & 0x01) == 0 && sc->button > 0) {
			/* Release button. */
			xf86PostButtonEvent(pInfo->dev, TRUE, sc->button, 0,
			    0, 2, x, y);
			sc->button = 0;
			timerclear(&sc->tv);
			continue;
		}

		/* Touchpad is pressed. */
		if ((buf[0] & 0x01) == 0x01 && sc->button == 0) {
			struct timeval tv;

			xf86PostButtonEvent(pInfo->dev, TRUE, 1, 1, 0, 2, x, y);
			sc->button = 1;

			gettimeofday(&sc->tv, NULL);
			sc->ox = x;
			sc->oy = y;
			continue;
		}
		

		if ((buf[0] & 0x01) == 0x01 && sc->button == 1 &&
		    (abs(x - sc->ox) < sc->area ) &&
		    (abs(y - sc->oy) < sc->area )) {
			struct timeval tv, res;

			gettimeofday(&tv, NULL);
			timersub(&tv, &sc->tv, &res);
			if (res.tv_sec >= sc->pause) {
				xf86PostButtonEvent(pInfo->dev, TRUE, 1, 0,
				    0, 2, x, y);
				xf86PostButtonEvent(pInfo->dev, TRUE, 3, 1,
				    0, 2, x, y);
				sc->button = 3;
			}
		}

	} /* while */
}

static int
eGalaxInitButtons(DeviceIntPtr device)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	unsigned char map[] = {0, 1, 2, 3};
#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
	Atom btn_label;
#endif


#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) >= 7
	if (!InitButtonClassDeviceStruct(device, 3, &btn_label, map)) {
#else
	if (!InitButtonClassDeviceStruct(device, 3, map)) {
#endif
		xf86Msg(X_ERROR, "%s: Failed to register button.\n",
		    pInfo->name);
		return (BadAlloc);
	}

	return (Success);
}

static void
eGalaxConfigAxes(DeviceIntPtr device)
{
	InputInfoPtr pInfo = device->public.devicePrivate;
	eGalaxDevicePtr sc = pInfo->private;
	Atom axis_labels[2] = { 0, 0 };

	xf86InitValuatorAxisStruct(device, 0, axis_labels[0],
	    sc->minx, sc->maxx, 1, 1, 1, Absolute);
	xf86InitValuatorDefaults(device, 0);

	xf86InitValuatorAxisStruct(device, 1, axis_labels[1],
	    sc->miny, sc->maxy, 1, 1, 1, Absolute);
	xf86InitValuatorDefaults(device, 1);
}

static void
eGalaxCalib(InputInfoPtr pInfo, int x, int y)
{
	eGalaxDevicePtr sc = pInfo->private;

	if (x < sc->minx)
		sc->minx = x;

	if (x > sc->maxx)
		sc->maxx = x;

	if (y < sc->miny)
		sc->miny = y;

	if (y > sc->maxy)
		sc->maxy = y;

	eGalaxConfigAxes(pInfo->dev);

	xf86Msg(X_WARNING,
	    "%s: adjusted calibration MinX=%u, MaxX=%u, MinY=%u, MaxY=%u.\n",
	    pInfo->name, sc->minx, sc->maxx, sc->miny, sc->maxy);
}
