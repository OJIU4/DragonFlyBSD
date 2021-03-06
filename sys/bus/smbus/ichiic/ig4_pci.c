/*
 * Copyright (c) 2014 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Intel 4th generation mobile cpus integrated I2C device, smbus driver.
 *
 * See ig4_reg.h for datasheet reference and notes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/serialize.h>
#include <sys/syslog.h>
#include <sys/bus.h>

#include <sys/rman.h>

#include <bus/pci/pcivar.h>
#include <bus/pci/pcireg.h>
#include <bus/smbus/smbconf.h>

#include "smbus_if.h"

#include "ig4_reg.h"
#include "ig4_var.h"

static int ig4iic_pci_detach(device_t dev);

#define PCI_CHIP_LYNXPT_LP_I2C_1	0x9c618086
#define PCI_CHIP_LYNXPT_LP_I2C_2	0x9c628086
#define PCI_CHIP_BRASWELL_I2C_1 	0x22c18086
#define PCI_CHIP_BRASWELL_I2C_2 	0x22c28086
#define PCI_CHIP_BRASWELL_I2C_3 	0x22c38086
#define PCI_CHIP_BRASWELL_I2C_5 	0x22c58086
#define PCI_CHIP_BRASWELL_I2C_6 	0x22c68086
#define PCI_CHIP_BRASWELL_I2C_7 	0x22c78086
#define PCI_CHIP_SKYLAKE_I2C_0		0x9d608086
#define PCI_CHIP_SKYLAKE_I2C_1		0x9d618086
#define PCI_CHIP_SKYLAKE_I2C_2		0x9d628086
#define PCI_CHIP_SKYLAKE_I2C_3		0x9d638086
#define PCI_CHIP_SKYLAKE_I2C_4		0x9d648086
#define PCI_CHIP_SKYLAKE_I2C_5		0x9d658086
#define PCI_CHIP_KABYLAKE_I2C_0		0xa1608086
#define PCI_CHIP_KABYLAKE_I2C_1		0xa1618086
#define PCI_CHIP_APL_I2C_0		0x5aac8086
#define PCI_CHIP_APL_I2C_1		0x5aae8086
#define PCI_CHIP_APL_I2C_2		0x5ab08086
#define PCI_CHIP_APL_I2C_3		0x5ab28086
#define PCI_CHIP_APL_I2C_4		0x5ab48086
#define PCI_CHIP_APL_I2C_5		0x5ab68086
#define PCI_CHIP_APL_I2C_6		0x5ab88086
#define PCI_CHIP_APL_I2C_7		0x5aba8086
#define PCI_CHIP_CANNONLAKE_LP_I2C_0	0x9dc58086
#define PCI_CHIP_CANNONLAKE_LP_I2C_1	0x9dc68086
#define PCI_CHIP_CANNONLAKE_LP_I2C_2	0x9de88086
#define PCI_CHIP_CANNONLAKE_LP_I2C_3	0x9de98086
#define PCI_CHIP_CANNONLAKE_LP_I2C_4	0x9dea8086
#define PCI_CHIP_CANNONLAKE_LP_I2C_5	0x9deb8086
#define PCI_CHIP_CANNONLAKE_H_I2C_0	0xa3688086
#define PCI_CHIP_CANNONLAKE_H_I2C_1	0xa3698086
#define PCI_CHIP_CANNONLAKE_H_I2C_2	0xa36a8086
#define PCI_CHIP_CANNONLAKE_H_I2C_3	0xa36b8086

struct ig4iic_pci_device {
	uint32_t	devid;
	const char	*desc;
	enum ig4_vers	version;
};

static struct ig4iic_pci_device ig4iic_pci_devices[] = {
	{ PCI_CHIP_LYNXPT_LP_I2C_1, "Intel Lynx Point-LP I2C Controller-1", IG4_HASWELL},
	{ PCI_CHIP_LYNXPT_LP_I2C_2, "Intel Lynx Point-LP I2C Controller-2", IG4_HASWELL},
	{ PCI_CHIP_BRASWELL_I2C_1, "Intel Braswell Serial I/O I2C Port 1", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_2, "Intel Braswell Serial I/O I2C Port 2", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_3, "Intel Braswell Serial I/O I2C Port 3", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_5, "Intel Braswell Serial I/O I2C Port 5", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_6, "Intel Braswell Serial I/O I2C Port 6", IG4_ATOM},
	{ PCI_CHIP_BRASWELL_I2C_7, "Intel Braswell Serial I/O I2C Port 7", IG4_ATOM},
	{ PCI_CHIP_SKYLAKE_I2C_0, "Intel Sunrise Point-LP I2C Controller-0", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_1, "Intel Sunrise Point-LP I2C Controller-1", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_2, "Intel Sunrise Point-LP I2C Controller-2", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_3, "Intel Sunrise Point-LP I2C Controller-3", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_4, "Intel Sunrise Point-LP I2C Controller-4", IG4_SKYLAKE},
	{ PCI_CHIP_SKYLAKE_I2C_5, "Intel Sunrise Point-LP I2C Controller-5", IG4_SKYLAKE},
	{ PCI_CHIP_KABYLAKE_I2C_0, "Intel Sunrise Point-H I2C Controller-0", IG4_SKYLAKE},
	{ PCI_CHIP_KABYLAKE_I2C_1, "Intel Sunrise Point-H I2C Controller-1", IG4_SKYLAKE},
	{ PCI_CHIP_APL_I2C_0, "Intel Apollo Lake I2C Controller-0", IG4_APL},
	{ PCI_CHIP_APL_I2C_1, "Intel Apollo Lake I2C Controller-1", IG4_APL},
	{ PCI_CHIP_APL_I2C_2, "Intel Apollo Lake I2C Controller-2", IG4_APL},
	{ PCI_CHIP_APL_I2C_3, "Intel Apollo Lake I2C Controller-3", IG4_APL},
	{ PCI_CHIP_APL_I2C_4, "Intel Apollo Lake I2C Controller-4", IG4_APL},
	{ PCI_CHIP_APL_I2C_5, "Intel Apollo Lake I2C Controller-5", IG4_APL},
	{ PCI_CHIP_APL_I2C_6, "Intel Apollo Lake I2C Controller-6", IG4_APL},
	{ PCI_CHIP_APL_I2C_7, "Intel Apollo Lake I2C Controller-7", IG4_APL},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_0, "Intel Cannon Lake-LP I2C Controller-0", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_1, "Intel Cannon Lake-LP I2C Controller-1", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_2, "Intel Cannon Lake-LP I2C Controller-2", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_3, "Intel Cannon Lake-LP I2C Controller-3", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_4, "Intel Cannon Lake-LP I2C Controller-4", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_LP_I2C_5, "Intel Cannon Lake-LP I2C Controller-5", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_0, "Intel Cannon Lake-H I2C Controller-0", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_1, "Intel Cannon Lake-H I2C Controller-1", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_2, "Intel Cannon Lake-H I2C Controller-2", IG4_CANNONLAKE},
	{ PCI_CHIP_CANNONLAKE_H_I2C_3, "Intel Cannon Lake-H I2C Controller-3", IG4_CANNONLAKE},
};

static int
ig4iic_pci_probe(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	uint32_t devid;
	int i;

	devid = pci_get_devid(dev);
	for (i = 0; i < nitems(ig4iic_pci_devices); i++) {
		if (ig4iic_pci_devices[i].devid == devid) {
			device_set_desc(dev, ig4iic_pci_devices[i].desc);
			sc->version = ig4iic_pci_devices[i].version;
			return (BUS_PROBE_DEFAULT);
		}
	}
	return (ENXIO);
}

static
int
ig4iic_pci_attach(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	u_int irq_flags;
	int msi_enable = 1;
	int error;

	bzero(sc, sizeof(*sc));

	lwkt_serialize_init(&sc->slz);

	sc->dev = dev;
	sc->regs_rid = PCIR_BAR(0);
	sc->regs_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
					  &sc->regs_rid, RF_ACTIVE);
	if (sc->regs_res == NULL) {
		device_printf(dev, "unable to map registers");
		ig4iic_pci_detach(dev);
		return (ENXIO);
	}
	sc->intr_type = pci_alloc_1intr(dev, msi_enable,
					&sc->intr_rid, &irq_flags);
	sc->intr_res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
					  &sc->intr_rid, irq_flags);
	if (sc->intr_res == NULL) {
		device_printf(dev, "unable to map interrupt");
		ig4iic_pci_detach(dev);
		return (ENXIO);
	}
	sc->regs_t = rman_get_bustag(sc->regs_res);
	sc->regs_h = rman_get_bushandle(sc->regs_res);
	sc->pci_attached = 1;

	error = ig4iic_attach(sc);
	if (error)
		ig4iic_pci_detach(dev);

	return error;
}

static
int
ig4iic_pci_detach(device_t dev)
{
	ig4iic_softc_t *sc = device_get_softc(dev);
	int error;

	if (sc->pci_attached) {
		error = ig4iic_detach(sc);
		if (error)
			return error;
		sc->pci_attached = 0;
	}

	if (sc->intr_res) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->intr_rid, sc->intr_res);
		sc->intr_res = NULL;
	}
	if (sc->intr_type == PCI_INTR_TYPE_MSI)
		pci_release_msi(dev);
	if (sc->regs_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->regs_rid, sc->regs_res);
		sc->regs_res = NULL;
	}
	sc->regs_t = 0;
	sc->regs_h = 0;

	return 0;
}

static device_method_t ig4iic_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ig4iic_pci_probe),
	DEVMETHOD(device_attach, ig4iic_pci_attach),
	DEVMETHOD(device_detach, ig4iic_pci_detach),

	/* Bus methods */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	/* SMBus methods from ig4_smb.c */
	DEVMETHOD(smbus_callback, ig4iic_smb_callback),
	DEVMETHOD(smbus_quick, ig4iic_smb_quick),
	DEVMETHOD(smbus_sendb, ig4iic_smb_sendb),
	DEVMETHOD(smbus_recvb, ig4iic_smb_recvb),
	DEVMETHOD(smbus_writeb, ig4iic_smb_writeb),
	DEVMETHOD(smbus_writew, ig4iic_smb_writew),
	DEVMETHOD(smbus_readb, ig4iic_smb_readb),
	DEVMETHOD(smbus_readw, ig4iic_smb_readw),
	DEVMETHOD(smbus_pcall, ig4iic_smb_pcall),
	DEVMETHOD(smbus_bwrite, ig4iic_smb_bwrite),
	DEVMETHOD(smbus_bread, ig4iic_smb_bread),
	DEVMETHOD(smbus_trans, ig4iic_smb_trans),
	DEVMETHOD_END
};

static driver_t ig4iic_pci_driver = {
        "ig4iic",
        ig4iic_pci_methods,
        sizeof(struct ig4iic_softc)
};

static devclass_t ig4iic_pci_devclass;

DRIVER_MODULE(ig4iic, pci, ig4iic_pci_driver, ig4iic_pci_devclass, NULL, NULL);
MODULE_DEPEND(ig4iic, pci, 1, 1, 1);
MODULE_DEPEND(ig4iic, smbus, SMBUS_MINVER, SMBUS_PREFVER, SMBUS_MAXVER);
MODULE_VERSION(ig4iic, 1);
