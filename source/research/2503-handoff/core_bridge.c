/* Actual unlock core and its resource lifetime; no display/boot callbacks here. */
#include "core_bridge.h"
#include "../../internal-boot/unlock/compat/compat.h"
#include "../../internal-boot/unlock/vendor/unlock_610.h"
EFI_STATUS UbRun(UB_CONTEXT *c) {
  if(!c || c->Used || !c->Guard)return EFI_INVALID_PARAMETER;
  c->Used=TRUE;c->Outcome=UB_RECOVER_DISPLAY;
  c->Pci=Gu.pci;c->WindowsAttempted=FALSE;c->WindowsPrepared=FALSE;
  /* No retry or reuse of a partly executed core. An existing allocation or
   * master transition belongs to its existing lifetime, never this bridge. */
  if(Gu.started || Gu.retained || Gu.allocations || Gu.dma_count || Gu.master_attributes_pending) {
    c->Outcome=UB_POWER_OFF;return EFI_ACCESS_DENIED;
  }
  if(Gu.fault || !Gu.profile_verified || !Gu.prom_verified || !Gu.owned || !Gu.armed || !Gu.display_ok ||
     !Gu.pci || !c->Guard(c->User,FALSE))return EFI_NOT_READY;
  c->DmaMask=0xffffffffffffULL;
  c->Device=(struct pci_dev){.dev={.dma_mask=&c->DmaMask,.coherent_dma_mask=0xfffffffffULL},.pm_cap=Gu.pm_cap,.refs=1};
  struct gpu_unlock_610_shared shared={0};
  c->CoreResult=gpu_unlock_610_load_shared(&c->Device,&shared);
  if(!c->CoreResult)c->CoreResult=gpu_unlock_610_unlock(&c->Device,&shared);
  gpu_unlock_610_release_shared(&shared);
  /* Once the GPU may have executed, firmware GOP reconnection is forbidden,
   * even after a clean core error. Its Start can reinitialize the card. */
  if(Gu.started || Gu.master_attributes_pending) {
    if(GuClearMaster()!=EFI_SUCCESS){c->Outcome=UB_POWER_OFF;return EFI_DEVICE_ERROR;}
  }
  if(!GuSafeToReturn() || Gu.allocations || Gu.dma_count || Gu.master_attributes_pending || Gu.retained) {
    c->Outcome=UB_POWER_OFF;return EFI_DEVICE_ERROR;
  }
  if(!Gu.started)return c->CoreResult?EFI_ABORTED:EFI_NOT_READY;
  c->Outcome=UB_POWER_OFF;
  if(c->CoreResult || Gu.fault || !Gu.quiesced || Gu.flrs!=1)return EFI_DEVICE_ERROR;
  UINT16 command=0xffff;UINT64 attributes=~0ULL;
  if(GuConfigRead(EfiPciIoWidthUint16,4,&command)!=EFI_SUCCESS || command==0xffff || (command&7)!=3 ||
     !Gu.pci->Attributes || Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attributes)!=EFI_SUCCESS ||
     attributes!=(EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY) ||
     GuRead32(0x82381c)!=0x88888888U || GuRead32(0x823820)!=8 || Gu.fault || !c->Guard(c->User,TRUE))return EFI_DEVICE_ERROR;
  c->Outcome=UB_CORE_COMPLETE;
  return EFI_SUCCESS;
}

static BOOLEAN WindowsState(UB_CONTEXT *c,UINT64 expectedAttributes) {
  UINT32 id=0,subsystem=0,bar=0;UINT16 command=0xffff;UINT64 attributes=~0ULL;
  if(Gu.fault || Gu.retained || !Gu.started || !Gu.quiesced || Gu.flrs!=1 ||
     Gu.allocations || Gu.dma_count || Gu.master_attributes_pending ||
     !Gu.profile_verified || !Gu.prom_verified || !Gu.owned || !Gu.armed || !Gu.display_ok ||
     !Gu.pci || Gu.pci!=c->Pci || !Gu.pci->Attributes || !c->Guard)return FALSE;
  if(GuConfigRead(EfiPciIoWidthUint32,0,&id)!=EFI_SUCCESS || id!=GU_BOARD_ID ||
     GuConfigRead(EfiPciIoWidthUint32,0x2c,&subsystem)!=EFI_SUCCESS || subsystem!=GU_BOARD_SUBSYSTEM ||
     GuConfigRead(EfiPciIoWidthUint32,0x10,&bar)!=EFI_SUCCESS || bar!=GU_BAR0_BASE ||
     GuConfigRead(EfiPciIoWidthUint16,4,&command)!=EFI_SUCCESS || command!=3 ||
     Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationGet,0,&attributes)!=EFI_SUCCESS ||
     attributes!=expectedAttributes || GuRead32(0x82381c)!=0x88888888U ||
     GuRead32(0x823820)!=8 || Gu.fault || !c->Guard(c->User,TRUE))return FALSE;
  return !Gu.fault;
}

EFI_STATUS UbPrepareWindows(UB_CONTEXT *c,UINT64 entryAttributes) {
  if(!c || !c->Used || c->WindowsAttempted || c->Outcome!=UB_CORE_COMPLETE)return EFI_INVALID_PARAMETER;
  c->WindowsAttempted=TRUE;c->WindowsPrepared=FALSE;c->Outcome=UB_POWER_OFF;
  const UINT64 vga=EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY|EFI_PCI_IO_ATTRIBUTE_VGA_IO;
  const UINT64 decode=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY;
  UINT64 supported=0;EFI_STATUS status=EFI_NOT_READY;
  /* This is the exact observed 2503 entry, not a portable board admission. */
  if(entryAttributes!=0x718 || !WindowsState(c,decode))goto fail;
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationSupported,0,&supported);
  if(status!=EFI_SUCCESS || (supported&vga)!=vga){status=EFI_UNSUPPORTED;goto fail;}
  GuLog("VGA forwarding intent: entry=%#llx restore=%#llx BME=0\n",
    (unsigned long long)entryAttributes,(unsigned long long)vga);
  if(!WindowsState(c,decode)){status=EFI_NOT_READY;goto fail;}
  /* Exactly one call; an error may follow a partial hardware change. */
  status=Gu.pci->Attributes(Gu.pci,EfiPciIoAttributeOperationEnable,vga,NULL);
  GuLog("VGA forwarding return: status=%#llx\n",(unsigned long long)status);
  if(status!=EFI_SUCCESS)goto fail;
  if(!WindowsState(c,decode|vga)){status=EFI_DEVICE_ERROR;goto fail;}
  GuLog("Core completed; VGA forwarding restored; Windows display not yet verified.\n");
  if(!WindowsState(c,decode|vga)){status=EFI_DEVICE_ERROR;goto fail;}
  c->WindowsPrepared=TRUE;c->Outcome=UB_BOOT_WITHOUT_FIRMWARE_GOP;
  return EFI_SUCCESS;
fail:
  /* Containment can clear an unexpectedly enabled hardware BME even after
   * Gu.fault. It never authorizes boot, retries VGA or reconnects firmware. */
  if(Gu.pci==c->Pci)GuClearMaster();
  return status;
}
