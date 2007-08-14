/*	$DragonFly: src/sys/dev/usbmisc/uark/uark.c,v 1.1 2007/08/14 22:28:11 hasso Exp $	*/
/*	$OpenBSD: uark.c,v 1.9 2007/06/13 06:25:03 mbalmer Exp $	*/

/*
 * Copyright (c) 2006 Jonathan Gray <jsg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/tty.h>
#include <sys/device.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/module.h>

#include <bus/usb/usb.h>
#include <bus/usb/usbdi.h>
#include <bus/usb/usbdi_util.h>
#include <bus/usb/usbdevs.h>

#include <dev/usbmisc/ucom/ucomvar.h>

#ifdef UARK_DEBUG
#define DPRINTFN(n, x)  do { if (uarkdebug > (n)) kprintf x; } while (0)
int	uarkebug = 0;
#else
#define DPRINTFN(n, x)
#endif
#define DPRINTF(x) DPRINTFN(0, x)

#define UARKBUFSZ		256
#define UARK_CONFIG_NO		0
#define UARK_IFACE_NO		0

#define UARK_SET_DATA_BITS(x)	(x - 5)

#define UARK_PARITY_NONE	0x00
#define UARK_PARITY_ODD		0x08
#define UARK_PARITY_EVEN	0x18

#define UARK_STOP_BITS_1	0x00
#define UARK_STOP_BITS_2	0x04

#define UARK_BAUD_REF		3000000

#define UARK_WRITE		0x40
#define UARK_READ		0xc0

#define UARK_REQUEST		0xfe

struct uark_softc {
	struct ucom_softc	sc_ucom;
	u_char			sc_msr;
	u_char			sc_lsr;
};

void	uark_get_status(void *, int portno, u_char *lsr, u_char *msr);
void	uark_set(void *, int, int, int);
int	uark_param(void *, int, struct termios *);
int	uark_open(void *sc, int);
void	uark_break(void *, int, int);
int	uark_cmd(struct uark_softc *, uint16_t, uint16_t);

struct ucom_callback uark_callback = {
	uark_get_status,
	uark_set,
	uark_param,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};

static const struct usb_devno uark_devs[] = {
	{ USB_VENDOR_ARKMICRO,		USB_PRODUCT_ARKMICRO_ARK3116 }
};

static device_probe_t uark_match;
static device_attach_t uark_attach;
static device_detach_t uark_detach;

static device_method_t uark_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, uark_match),
	DEVMETHOD(device_attach, uark_attach),
	DEVMETHOD(device_detach, uark_detach),
	{ 0, 0 }
};

static driver_t uark_driver = {
	"ucom",
	uark_methods,
	sizeof (struct uark_softc)
};

DRIVER_MODULE(uark, uhub, uark_driver, ucom_devclass, usbd_driver_load, 0);
MODULE_DEPEND(uark, usb, 1, 1, 1);
MODULE_DEPEND(uark, ucom, UCOM_MINVER, UCOM_PREFVER, UCOM_MAXVER);
MODULE_VERSION(uark, 1);

static int
uark_match(device_t self)
{
	struct usb_attach_arg *uaa = device_get_ivars(self);

	if (uaa->iface != NULL)
		return UMATCH_NONE;

	return (usb_lookup(uark_devs, uaa->vendor, uaa->product) != NULL) ?
	    UMATCH_VENDOR_PRODUCT : UMATCH_NONE;
}

static int
uark_attach(device_t self)
{
	struct uark_softc *sc = device_get_softc(self);
	struct usb_attach_arg *uaa = device_get_ivars(self);
	struct ucom_softc *ucom;
	usb_interface_descriptor_t *id;
	usb_endpoint_descriptor_t *ed;
	usbd_status error;
	char *devinfo;
	const char *devname;
	int i;

	devinfo = kmalloc(1024, M_USBDEV, M_INTWAIT);
	ucom = &sc->sc_ucom;

	bzero(sc, sizeof (struct uark_softc));

	usbd_devinfo(uaa->device, 0, devinfo);
	ucom->sc_dev = self;
	device_set_desc_copy(self, devinfo);

	ucom->sc_udev = uaa->device;
	ucom->sc_iface = uaa->iface;

	devname = device_get_nameunit(ucom->sc_dev);
	kprintf("%s: %s\n", devname, devinfo);
	kfree(devinfo, M_USBDEV);

	if (usbd_set_config_index(ucom->sc_udev, UARK_CONFIG_NO, 1) != 0) {
		kprintf("%s: could not set configuration no\n", devname);
		goto error;
	}

	/* get the first interface handle */
	error = usbd_device2interface_handle(ucom->sc_udev, UARK_IFACE_NO,
	    &ucom->sc_iface);
	if (error != 0) {
		kprintf("%s: could not get interface handle\n", devname);
		goto error;
	}

	id = usbd_get_interface_descriptor(ucom->sc_iface);

	ucom->sc_bulkin_no = ucom->sc_bulkout_no = -1;
	for (i = 0; i < id->bNumEndpoints; i++) {
		ed = usbd_interface2endpoint_descriptor(ucom->sc_iface, i);
		if (ed == NULL) {
			kprintf("%s: no endpoint descriptor found for %d\n",
			    devname, i);
			goto error;
		}

		if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_IN &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			ucom->sc_bulkin_no = ed->bEndpointAddress;
		else if (UE_GET_DIR(ed->bEndpointAddress) == UE_DIR_OUT &&
		    UE_GET_XFERTYPE(ed->bmAttributes) == UE_BULK)
			ucom->sc_bulkout_no = ed->bEndpointAddress;
	}

	if (ucom->sc_bulkin_no == -1 || ucom->sc_bulkout_no == -1) {
		kprintf("%s: missing endpoint\n", devname);
		goto error;
	}

	ucom->sc_parent = sc;
	ucom->sc_portno = UCOM_UNK_PORTNO;
	ucom->sc_ibufsize = UARKBUFSZ;
	ucom->sc_obufsize = UARKBUFSZ;
	ucom->sc_ibufsizepad = UARKBUFSZ;
	ucom->sc_opkthdrlen = 0;
	ucom->sc_callback = &uark_callback;

	usbd_add_drv_event(USB_EVENT_DRIVER_ATTACH, ucom->sc_udev,
			   ucom->sc_dev);
	
	DPRINTF(("uark: in = 0x%x, out = 0x%x, intr = 0x%x\n",
		 ucom->sc_bulkin_no, ucom->sc_bulkout_no, sc->sc_intr_number));

	ucom_attach(&sc->sc_ucom);

	return 0;

error:
	ucom->sc_dying = 1;
	return ENXIO;
}

static int
uark_detach(device_t self)
{
	struct uark_softc *sc = device_get_softc(self);
	int rv = 0;

	DPRINTF(("uark_detach: sc=%p\n", sc));
	sc->sc_ucom.sc_dying = 1;
	rv = ucom_detach(&sc->sc_ucom);
	usbd_add_drv_event(USB_EVENT_DRIVER_DETACH, sc->sc_ucom.sc_udev,
			   sc->sc_ucom.sc_dev);

	return (rv);
}

#if 0 /* not yet */
int
uark_activate(struct device *self, enum devact act)
{
	struct uark_softc *sc = (struct uark_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_subdev != NULL)
			rv = config_deactivate(sc->sc_subdev);
		sc->sc_dying = 1;
		break;
	}
	return (rv);
}
#endif

void
uark_set(void *vsc, int portno, int reg, int onoff)
{
	struct uark_softc *sc = vsc;

	switch (reg) {
	case UCOM_SET_BREAK:
		uark_break(sc, portno, onoff);
		return;
	case UCOM_SET_DTR:
	case UCOM_SET_RTS:
	default:
		return;
	}
}

int
uark_param(void *vsc, int portno, struct termios *t)
{
	struct uark_softc *sc = (struct uark_softc *)vsc;
	int data;

	switch (t->c_ospeed) {
	case 300:
	case 600:
	case 1200:
	case 1800:
	case 2400:
	case 4800:
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:
		uark_cmd(sc, 3, 0x83);
		uark_cmd(sc, 0, (UARK_BAUD_REF / t->c_ospeed) & 0xFF);
		uark_cmd(sc, 1, (UARK_BAUD_REF / t->c_ospeed) >> 8);
		uark_cmd(sc, 3, 0x03);
		break;
	default:
		return (EINVAL);
	}

	if (ISSET(t->c_cflag, CSTOPB))
		data = UARK_STOP_BITS_2;
	else
		data = UARK_STOP_BITS_1;

	if (ISSET(t->c_cflag, PARENB)) {
		if (ISSET(t->c_cflag, PARODD))
			data |= UARK_PARITY_ODD;
		else
			data |= UARK_PARITY_EVEN;
	} else
		data |= UARK_PARITY_NONE;

	switch (ISSET(t->c_cflag, CSIZE)) {
	case CS5:
		data |= UARK_SET_DATA_BITS(5);
		break;
	case CS6:
		data |= UARK_SET_DATA_BITS(6);
		break;
	case CS7:
		data |= UARK_SET_DATA_BITS(7);
		break;
	case CS8:
		data |= UARK_SET_DATA_BITS(8);
		break;
	}

	uark_cmd(sc, 3, 0x00);
	uark_cmd(sc, 3, data);

#if 0
	/* XXX flow control */
	if (ISSET(t->c_cflag, CRTSCTS))
		/*  rts/cts flow ctl */
	} else if (ISSET(t->c_iflag, IXON|IXOFF)) {
		/*  xon/xoff flow ctl */
	} else {
		/* disable flow ctl */
	}
#endif

	return (0);
}

void
uark_get_status(void *vsc, int portno, u_char *lsr, u_char *msr)
{
	struct uark_softc *sc = vsc;
	
	if (msr != NULL)
		*msr = sc->sc_msr;
	if (lsr != NULL)
		*lsr = sc->sc_lsr;
}

void
uark_break(void *vsc, int portno, int onoff)
{
#ifdef UARK_DEBUG
	struct uark_softc *sc = vsc;

	kprintf("%s: break %s!\n", device_get_nameunit(sc->sc_ucom->sc_dev),
	    onoff ? "on" : "off");

	if (onoff)
		/* break on */
		uark_cmd(sc, 4, 0x01);
	else
		uark_cmd(sc, 4, 0x00);
#endif
}

int
uark_cmd(struct uark_softc *sc, uint16_t index, uint16_t value)
{
	usb_device_request_t req;
	usbd_status err;

	req.bmRequestType = UARK_WRITE;
	req.bRequest = UARK_REQUEST;
	USETW(req.wValue, value);
	USETW(req.wIndex, index);
	USETW(req.wLength, 0);
	err = usbd_do_request(sc->sc_ucom.sc_udev, &req, NULL);

	if (err)
		return (EIO);

	return (0);
}
