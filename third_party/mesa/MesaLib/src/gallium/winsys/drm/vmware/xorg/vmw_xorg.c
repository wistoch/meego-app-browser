/**********************************************************
 * Copyright 2008-2009 VMware, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 **********************************************************/

/**
 * @file
 * Glue file for Xorg State Tracker.
 *
 * @author Alan Hourihane <alanh@tungstengraphics.com>
 * @author Jakob Bornecrantz <wallbraker@gmail.com>
 */

#include "vmw_hook.h"

static void vmw_xorg_identify(int flags);
static Bool vmw_xorg_pci_probe(DriverPtr driver,
			       int entity_num,
			       struct pci_device *device,
			       intptr_t match_data);

static const struct pci_id_match vmw_xorg_device_match[] = {
    {0x15ad, PCI_MATCH_ANY, PCI_MATCH_ANY, PCI_MATCH_ANY, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
};

static SymTabRec vmw_xorg_chipsets[] = {
    {PCI_MATCH_ANY, "VMware SVGA Device"},
    {-1, NULL}
};

static PciChipsets vmw_xorg_pci_devices[] = {
    {PCI_MATCH_ANY, PCI_MATCH_ANY, NULL},
    {-1, -1, NULL}
};

static XF86ModuleVersionInfo vmw_xorg_version = {
    "vmwgfx",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    0, 1, 0, /* major, minor, patch */
    ABI_CLASS_VIDEODRV,
    ABI_VIDEODRV_VERSION,
    MOD_CLASS_VIDEODRV,
    {0, 0, 0, 0}
};

/*
 * Xorg driver exported structures
 */

_X_EXPORT DriverRec vmwgfx = {
    1,
    "vmwgfx",
    vmw_xorg_identify,
    NULL,
    xorg_tracker_available_options,
    NULL,
    0,
    NULL,
    vmw_xorg_device_match,
    vmw_xorg_pci_probe
};

static MODULESETUPPROTO(vmw_xorg_setup);

_X_EXPORT XF86ModuleData vmwgfxModuleData = {
    &vmw_xorg_version,
    vmw_xorg_setup,
    NULL
};

/*
 * Xorg driver functions
 */

static pointer
vmw_xorg_setup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    static Bool setupDone = 0;

    /* This module should be loaded only once, but check to be sure.
     */
    if (!setupDone) {
	setupDone = 1;
	xf86AddDriver(&vmwgfx, module, HaveDriverFuncs);

	/*
	 * The return value must be non-NULL on success even though there
	 * is no TearDownProc.
	 */
	return (pointer) 1;
    } else {
	if (errmaj)
	    *errmaj = LDR_ONCEONLY;
	return NULL;
    }
}

static void
vmw_xorg_identify(int flags)
{
    xf86PrintChipsets("vmwgfx", "Driver for VMware SVGA device",
		      vmw_xorg_chipsets);
}

static Bool
vmw_xorg_pci_probe(DriverPtr driver,
	  int entity_num, struct pci_device *device, intptr_t match_data)
{
    ScrnInfoPtr scrn = NULL;
    EntityInfoPtr entity;

    scrn = xf86ConfigPciEntity(scrn, 0, entity_num, vmw_xorg_pci_devices,
			       NULL, NULL, NULL, NULL, NULL);
    if (scrn != NULL) {
	scrn->driverVersion = 1;
	scrn->driverName = "vmwgfx";
	scrn->name = "vmwgfx";
	scrn->Probe = NULL;

	entity = xf86GetEntityInfo(entity_num);

	/* Use all the functions from the xorg tracker */
	xorg_tracker_set_functions(scrn);

	vmw_screen_set_functions(scrn);
    }
    return scrn != NULL;
}
