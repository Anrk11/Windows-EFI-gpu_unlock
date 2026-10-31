/* GPL-2.0-only. Included only by the captured-machine full EFI adapter.
 * No host-only model or reviewed 2504 profile is used by this image. */
#pragma once
#include <Guid/FileInfo.h>
#include "../../internal-boot/unlock/format.h"
#include "../../internal-boot/unlock/vendor/gfw_eval.h"
static UB_CONTEXT CoreBridge; /* Survives retained DMA/device references. */
static HT_STATE CoreEntry;
static BOOLEAN CoreInitialized,CoreAssetsVerified,CorePrepared,CoreLogFailed;
static BOOLEAN CoreClaimHeld,CoreClaimUncertain;
static UINT32 CoreGuardReason,CoreGuardIndex,CoreGuardActual,CoreGuardExpected;
typedef struct {const char *Name;const CHAR16 *Path;UINTN Bytes;const char *Hash;} CORE_ASSET_SPEC;
#ifdef HT_INTERNAL_AUTO
#define CORE_ASSET_BASE L"\\EFI\\Codex2503\\ASSETS\\"
#else
#define CORE_ASSET_BASE L"\\EFI\\2503-UNLOCK\\ASSETS\\"
#endif
static const CORE_ASSET_SPEC CoreSpecs[]={
 {"gpu-unlock/610.43.03/booter_load_ga102.bin",CORE_ASSET_BASE L"booter_load_ga102.bin",61256,"056cbc064a888f80717726c54d7f64fcd68044d123870c95a74f37f4975dcaa4"},
 {"gpu-unlock/610.43.03/v67-ga102-610.43.03.bin",CORE_ASSET_BASE L"v67-ga102-610.43.03.bin",64000,"2a0aa1f86d0b6afff5214d641d87d46dc98ef06ed908ff05d0ddbad5ffb7e197"},
 {"gpu-unlock/610.43.03/gsp_ga10x.bin",CORE_ASSET_BASE L"gsp_ga10x.bin",84277400,"572373620a37418f24dc16b5031c39338778c3257e48e8408de9a57291b24f3a"},
 {"gpu-unlock/610.43.03/gsp_boot_ga102.bin",CORE_ASSET_BASE L"gsp_boot_ga102.bin",24576,"3df651acb174fd5f5561a1996965b192f50c5d763168446ed0d4ead19d895768"},
 {"gpu-unlock/610.43.03/gsp_boot_desc_ga102.bin",CORE_ASSET_BASE L"gsp_boot_desc_ga102.bin",84,"cc5a3fc1f4c47f2fe107b1b68f0176d8a448e6dec84b575e5560ccef5262bf15"}
};
VOID GuLogText(const char *text){
  if(!text || !LogFile || CoreLogFailed){CoreLogFailed=TRUE;Gu.fault=TRUE;return;}
  UINTN expected=strlen(text),written=expected;
  EFI_STATUS s=LogFile->Write(LogFile,&written,(VOID *)text);
  if(s!=EFI_SUCCESS || written!=expected || LogFile->Flush(LogFile)!=EFI_SUCCESS){CoreLogFailed=TRUE;Gu.fault=TRUE;}
}
VOID GuLog(const char *format,...){
  char text[1024];va_list args;va_start(args,format);
  size_t n=GuFormatV(text,sizeof(text),format,args);va_end(args);
  if(n>=sizeof(text)){CoreLogFailed=TRUE;Gu.fault=TRUE;return;}
  GuLogText(text);
}
static EFI_STATUS CoreEvent(const char *name,UINT64 value){
  EFI_STATUS s=Event(NULL,name,value);
  if(s!=EFI_SUCCESS){CoreLogFailed=TRUE;Gu.fault=TRUE;}
  return s;
}
static EFI_STATUS CoreInitialize(EFI_HANDLE image,EFI_HANDLE source){
  if(CoreInitialized || !Bs->OpenProtocol || !Bs->CloseProtocol || !Bs->RaiseTPL || !Bs->RestoreTPL ||
     !Bs->AllocatePool || !Bs->FreePool || !Io->Pci.Write || !Io->Mem.Write || !Io->Flush)return EFI_UNSUPPORTED;
  EFI_GUID pathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID;EFI_DEVICE_PATH_PROTOCOL *path=NULL;
  if(Bs->HandleProtocol(source,&pathGuid,(VOID **)&path)!=EFI_SUCCESS || !path)return EFI_UNSUPPORTED;
#ifdef HT_INTERNAL_AUTO
  EFI_DEVICE_PATH_PROTOCOL *nativePath=path;
#endif
  BOOLEAN usb=FALSE,ended=FALSE;UINTN total=0;
  for(UINTN nodes=0;nodes<64 && total+4<=4096;nodes++){
    UINTN n=path->Length[0]+((UINTN)path->Length[1]<<8);
    if(n<4 || n>4096-total)return EFI_UNSUPPORTED;
    if(path->Type==MESSAGING_DEVICE_PATH && path->SubType==MSG_USB_DP && n==sizeof(USB_DEVICE_PATH))usb=TRUE;
    if(path->Type==END_DEVICE_PATH_TYPE){ended=n==4 && path->SubType==END_ENTIRE_DEVICE_PATH_SUBTYPE;break;}
    total+=n;path=(VOID *)((UINT8 *)path+n);
  }
#ifdef HT_INTERNAL_AUTO
  if(usb || !ended || !GuPathIsPartition(nativePath,&GuWindowsPartition))return EFI_UNSUPPORTED;
#else
  if(!usb || !ended)return EFI_UNSUPPORTED;
#endif
  GuInit(Bs,Io);Gu.image=image;Gu.handle=Gpu;Gu.bar_base=GU_BAR0_BASE;Gu.bar_size=0x1000000;
  CoreGuardReason=CoreGuardIndex=CoreGuardActual=CoreGuardExpected=0;
  CoreInitialized=TRUE;
  return CoreEvent("full-unlock-experiment-enabled",1);
}
static VOID CoreReleaseAssets(VOID){
  if(!CoreInitialized || !GuSafeToReturn() || Gu.allocations || Gu.dma_count)return;
  for(UINTN i=0;i<5;i++){
    if(GuAssets[i].firmware.data)GuFree((VOID *)GuAssets[i].firmware.data);
    GuAssets[i]=(GU_ASSET){0};
  }
}
static EFI_STATUS CoreLoadAssets(VOID){
  if(!CoreInitialized || CoreAssetsVerified || Gu.started || Gu.armed)return EFI_NOT_READY;
  EFI_GUID infoGuid=EFI_FILE_INFO_ID;
  for(UINTN i=0;i<5;i++){
    const CORE_ASSET_SPEC *spec=&CoreSpecs[i];EFI_FILE_PROTOCOL *file=NULL;
    EFI_STATUS s=Root->Open(Root,&file,(CHAR16 *)spec->Path,EFI_FILE_MODE_READ,0);
    if(s!=EFI_SUCCESS || !file)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
    if(!file->GetInfo || !file->Read || !file->Close){if(file->Close)file->Close(file);return EFI_UNSUPPORTED;}
    union {EFI_FILE_INFO Info;UINT8 Bytes[512];} metadata;UINTN n=sizeof(metadata);
    s=file->GetInfo(file,&infoGuid,&n,&metadata);
    if(s==EFI_SUCCESS && (n<SIZE_OF_EFI_FILE_INFO || n>sizeof(metadata) || metadata.Info.Size<SIZE_OF_EFI_FILE_INFO ||
       metadata.Info.Size>n || (metadata.Info.Attribute&EFI_FILE_DIRECTORY) || metadata.Info.FileSize!=spec->Bytes))s=EFI_COMPROMISED_DATA;
    UINT8 *data=s==EFI_SUCCESS?GuAlloc(spec->Bytes):NULL;
    if(s==EFI_SUCCESS && !data)s=EFI_OUT_OF_RESOURCES;
    UINTN done=0;
    while(s==EFI_SUCCESS && done<spec->Bytes){
      UINTN count=spec->Bytes-done;if(count>1048576)count=1048576;UINTN want=count;
      s=file->Read(file,&count,data+done);
      if(s==EFI_SUCCESS && count!=want)s=EFI_END_OF_FILE;
      if(s==EFI_SUCCESS)done+=count;
    }
    if(s==EFI_SUCCESS){UINT8 extra=0;UINTN count=1;s=file->Read(file,&count,&extra);if(s==EFI_SUCCESS && count)s=EFI_COMPROMISED_DATA;}
    EFI_STATUS closed=file->Close(file);if(s==EFI_SUCCESS)s=closed;
    if(s==EFI_SUCCESS && !GuHashMatches(data,spec->Bytes,spec->Hash))s=EFI_CRC_ERROR;
    if(s!=EFI_SUCCESS){GuFree(data);return s;}
    GuAssets[i]=(GU_ASSET){spec->Name,{spec->Bytes,data}};
    if(CoreEvent("firmware-asset-verified-index",i)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  }
  CoreAssetsVerified=TRUE;return EFI_SUCCESS;
}
/* Inspect topology without logging and without touching BAR registers. The
 * only active record permitted during execution is this image's exclusive open. */
static BOOLEAN CoreTopology(BOOLEAN own){
  EFI_PCI_IO_PROTOCOL *current=NULL;
  if(Bs->HandleProtocol(Gpu,&PciGuid,(VOID **)&current)!=EFI_SUCCESS || current!=Io)return FALSE;
  UINTN seg,bus,dev,fun;
  if(Io->GetLocation(Io,&seg,&bus,&dev,&fun)!=EFI_SUCCESS || seg || bus!=7 || dev || fun)return FALSE;
  BOOLEAN quiet=QuietObservation;QuietObservation=TRUE;BOOLEAN pinned=ExpectStopped && CodePinned();QuietObservation=quiet;
  if(!pinned)return FALSE;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opens=NULL;UINTN count=0,mine=0;
  EFI_STATUS s=Bs->OpenProtocolInformation(Gpu,&PciGuid,&opens,&count);
  if(s!=EFI_SUCCESS)return FALSE;
  BOOLEAN good=count<=512 && (!count || opens);
  for(UINTN i=0;good && i<count;i++){
    const EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *e=&opens[i];
    if(e->Attributes==EFI_OPEN_PROTOCOL_EXCLUSIVE && e->AgentHandle==Gu.image && !e->ControllerHandle && e->OpenCount==1)mine++;
    else if(e->Attributes!=EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL && e->Attributes!=EFI_OPEN_PROTOCOL_GET_PROTOCOL && e->Attributes!=EFI_OPEN_PROTOCOL_TEST_PROTOCOL)good=FALSE;
  }
  if(opens && Bs->FreePool(opens)!=EFI_SUCCESS)good=FALSE;
  if(!good || mine!=(own?1U:0U))return FALSE;
  EFI_HANDLE *gops=NULL;count=0;s=Bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&gops);
  good=(s==EFI_NOT_FOUND || s==EFI_SUCCESS) && !count;
  if(gops && Bs->FreePool(gops)!=EFI_SUCCESS)good=FALSE;
  return good;
}
static BOOLEAN CoreConfig(UINT32 config[64],GU_CAPS *caps){
  UINT64 attributes=0;
  if(Io->Pci.Read(Io,EfiPciIoWidthUint32,0,64,config)!=EFI_SUCCESS ||
     (config[1]&0xffff)!=3 || config[2]!=0x030000a1 || config[4]!=GU_BAR0_BASE ||
     !GuCheckConfig(config,0,7,0,0,caps) ||
     Io->Attributes(Io,EfiPciIoAttributeOperationGet,0,&attributes)!=EFI_SUCCESS ||
     (attributes!=0x300 && attributes!=0x318))return FALSE;
  VOID *raw=NULL;EFI_STATUS s=Io->GetBarAttributes(Io,0,NULL,&raw);
  if(s!=EFI_SUCCESS || !raw)return FALSE;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *bar=raw;
  BOOLEAN good=bar->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && bar->Len==0x2b && bar->ResType==ACPI_ADDRESS_SPACE_TYPE_MEM &&
    bar->AddrRangeMin==GU_BAR0_BASE && bar->AddrLen==0x1000000;
  if(Bs->FreePool(raw)!=EFI_SUCCESS)good=FALSE;
  return good;
}
static UINT16 CoreWord(const UINT32 *config,UINTN offset){
  return (UINT16)(config[offset/4]>>((offset&3)*8));
}
static BOOLEAN CoreBeforeHardware(VOID){
  UINT32 config[64];GU_CAPS caps;UINT64 attributes=0;
  if(CoreLogFailed || Gu.fault || CoreClaimUncertain || !CoreTopology(CoreClaimHeld) ||
     !CoreConfig(config,&caps) || !ConfigMatches(config,CoreEntry.Config,3) ||
     Io->Attributes(Io,EfiPciIoAttributeOperationGet,0,&attributes)!=EFI_SUCCESS || attributes!=0x300)return FALSE;
  for(UINTN i=0;i<16;i++){
    UINT32 value=0xffffffff;
    if(Io->Mem.Read(Io,EfiPciIoWidthUint32,0,Offsets[i],1,&value)!=EFI_SUCCESS || value!=CoreEntry.Registers[i])return FALSE;
  }
  return TRUE;
}
static BOOLEAN CoreAfterHardware(VOID){
  UINT32 config[64];GU_CAPS caps;
  CoreGuardReason=1;
  if(CoreLogFailed || Gu.fault || CoreClaimUncertain || !Gu.started || !Gu.quiesced || Gu.retained ||
     Gu.allocations || Gu.dma_count || Gu.master_attributes_pending || Gu.flrs!=1 ||
     Gu.owned!=CoreClaimHeld)return FALSE;
  CoreGuardReason=2;if(!CoreTopology(CoreClaimHeld))return FALSE;
  CoreGuardReason=3;if(!CoreConfig(config,&caps))return FALSE;
  CoreGuardReason=4;
  if(caps.pm!=Gu.pm_cap || caps.pcie!=Gu.pcie_cap || caps.pcie_version!=Gu.pcie_version ||
     caps.msi!=Gu.msi_cap || caps.msix!=Gu.msix_cap)return FALSE;
  /* Status words may change across FLR. Pin identities, all BARs, ROM, cache/
   * latency, IRQ routing and device controls rather than treating status as RW. */
  static const UINT8 pinned[]={0,2,3,4,5,6,7,8,9,11,12,13,15};
  CoreGuardReason=5;
  for(UINTN i=0;i<sizeof(pinned);i++)if(config[pinned[i]]!=CoreEntry.Config[pinned[i]]){
    CoreGuardIndex=pinned[i];CoreGuardActual=config[pinned[i]];CoreGuardExpected=CoreEntry.Config[pinned[i]];return FALSE;
  }
  CoreGuardReason=6;
  if(CoreWord(config,caps.pcie+8)!=Gu.saved_devctl ||
     (caps.pcie_version>=2 && CoreWord(config,caps.pcie+0x28)!=Gu.saved_devctl2))return FALSE;
  struct gpu_unlock_gfw_sample sample={.config_ok=true,.plm=GuRead32(GU_GFW_PLM_OFFSET),
    .boot=GuRead32(GU_GFW_BOOT_OFFSET),.cpuctl=GuRead32(GU_GFW_CPUCTL_OFFSET)};
  CoreGuardReason=7;
  const UINT32 offsets[]={0,0x1183a4,0x1fa828,GU_FEAT_PLM_OFFSET,GU_FEAT_SS0_OFFSET,GU_FEAT_SS1_OFFSET};
  /* The payload deliberately grants selector access: its PLM becomes all ones.
   * This register's valid permission value is not an inaccessible BAR sample. */
  const UINT32 expected[]={GU_CHIP_ID,GU_DEVICE_MEMORY_SIZE,0,0xffffffffU,0x88888888U,8};
  BOOLEAN good=!gpu_unlock_gfw_assess(&sample) && (sample.cpuctl&GU_GFW_CPUCTL_HALTED);
  if(!good){CoreGuardIndex=GU_GFW_PLM_OFFSET;CoreGuardActual=sample.plm;CoreGuardExpected=1;}
  for(UINTN i=0;good && i<6;i++){
    UINT32 actual=GuRead32(offsets[i]);
    if(actual!=expected[i]){CoreGuardIndex=offsets[i];CoreGuardActual=actual;CoreGuardExpected=expected[i];good=FALSE;}
  }
  good=good && !Gu.fault;
  if(good)CoreGuardReason=0;
  return good;
}
static BOOLEAN CoreBridgeGuard(VOID *unused,BOOLEAN after){(void)unused;return after?CoreAfterHardware():CoreBeforeHardware();}
static BOOLEAN CoreDisplayCheck(VOID){return !CoreClaimUncertain && CoreClaimHeld && Gu.owned && CoreTopology(TRUE);}
static BOOLEAN CoreReleaseClaim(VOID){
  if(CoreClaimUncertain)return FALSE;
  if(!CoreClaimHeld)return !Gu.owned;
  if(Gu.allocations || Gu.dma_count || Gu.master_attributes_pending || !GuSafeToReturn())return FALSE;
  EFI_STATUS s=Bs->CloseProtocol(Gpu,&PciGuid,Gu.image,NULL);
  if(s!=EFI_SUCCESS){CoreClaimUncertain=TRUE;return FALSE;}
  CoreClaimHeld=FALSE;Gu.owned=FALSE;
  return CoreTopology(FALSE);
}
static BOOLEAN CoreMayRecover(VOID *unused){
  (void)unused;
  if(!CoreInitialized)return TRUE;
  return !Gu.started && !Gu.retained && !Gu.allocations && !Gu.dma_count && !Gu.master_attributes_pending &&
    GuSafeToReturn() && CoreReleaseClaim() && (!ExpectStopped || CoreTopology(FALSE));
}
static EFI_STATUS CoreClaim(VOID){
  if(!CoreBeforeHardware() || CoreClaimHeld || Gu.owned)return EFI_NOT_READY;
  if(CoreEvent("pci-exclusive-claim-intent",1)!=EFI_SUCCESS || !CoreBeforeHardware())return EFI_NOT_READY;
  EFI_PCI_IO_PROTOCOL *claimed=NULL;CoreClaimUncertain=TRUE;
  EFI_TPL old=Bs->RaiseTPL(TPL_NOTIFY);
  EFI_STATUS s=Bs->OpenProtocol(Gpu,&PciGuid,(VOID **)&claimed,Gu.image,NULL,EFI_OPEN_PROTOCOL_EXCLUSIVE);
  if(s==EFI_SUCCESS){CoreClaimHeld=TRUE;Gu.owned=TRUE;CoreClaimUncertain=FALSE;}
  Bs->RestoreTPL(old);
  if(s!=EFI_SUCCESS){if(CoreTopology(FALSE))CoreClaimUncertain=FALSE;return s;}
  if(claimed!=Io || !CoreTopology(TRUE))return EFI_COMPROMISED_DATA;
  return CoreEvent("pci-exclusive-claim-complete",1);
}
static EFI_STATUS PrepareCoreWindows(const HT_STATE *entry){
  if(!CoreInitialized || !CoreAssetsVerified || CorePrepared || CoreBridge.Used || CoreLogFailed)return EFI_NOT_READY;
  CoreEntry=*entry;
  EFI_STATUS s=CoreClaim();if(s!=EFI_SUCCESS)return s;
  UINT32 config[64];GU_CAPS caps;
  if(!CoreBeforeHardware() || !CoreConfig(config,&caps))return EFI_NOT_READY;
  Gu.pm_cap=caps.pm;Gu.pcie_cap=caps.pcie;Gu.pcie_version=caps.pcie_version;Gu.msi_cap=caps.msi;Gu.msix_cap=caps.msix;
  Gu.prom_verified=GuHashMatches(PromBytes,sizeof(PromBytes),GU_PROM_SHA256);
  if(!Gu.prom_verified || !GuLatchEntrySelectors(entry->Registers[12],entry->Registers[13]))return EFI_UNSUPPORTED;
  Gu.profile_verified=TRUE;Gu.armed=TRUE;Gu.display_ok=TRUE;Gu.display_check=CoreDisplayCheck;
  CoreBridge.Guard=CoreBridgeGuard;
  if(CoreEvent("unlock-core-intent",1)!=EFI_SUCCESS || !CoreBeforeHardware())return EFI_NOT_READY;
  s=UbRun(&CoreBridge);
  if(s!=EFI_SUCCESS && CoreGuardReason){
    CoreEvent("core-final-guard-failure-stage",CoreGuardReason);
    CoreEvent("core-final-guard-pinned-config-index",CoreGuardIndex);
    CoreEvent("core-final-guard-actual",CoreGuardActual);
    CoreEvent("core-final-guard-expected",CoreGuardExpected);
  }
  if(CoreEvent("unlock-core-return",s)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(CoreEvent("unlock-core-linux-result",(UINT64)(INT64)CoreBridge.CoreResult)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(s!=EFI_SUCCESS)return s;
  s=UbPrepareWindows(&CoreBridge,entry->Attributes);if(s!=EFI_SUCCESS)return s;
  VgaDecodeActive=TRUE;
  if(CoreEvent("unlock-core-complete",1)!=EFI_SUCCESS ||
     CoreEvent("vga-forwarding-restored-bme-still-off",1)!=EFI_SUCCESS || !CoreAfterHardware())return EFI_DEVICE_ERROR;
  CoreReleaseAssets();
  if(!CoreAfterHardware() || !CoreReleaseClaim())return EFI_DEVICE_ERROR;
  CorePrepared=TRUE;
  if(CoreEvent("headless-windows-start-intent",1)!=EFI_SUCCESS || !CoreFinalGuard())return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static BOOLEAN CoreFinalGuard(VOID){
  UINT64 attributes=0;
  return CorePrepared && CoreBridge.WindowsPrepared && CoreBridge.Outcome==UB_BOOT_WITHOUT_FIRMWARE_GOP &&
    !CoreClaimHeld && !Gu.owned && CoreAfterHardware() &&
    Io->Attributes(Io,EfiPciIoAttributeOperationGet,0,&attributes)==EFI_SUCCESS && attributes==0x318;
}
static VOID CoreContain(VOID){
  if(!CoreInitialized || !Gu.pci)return;
  EFI_TPL old=Bs->RaiseTPL(TPL_NOTIFY);EFI_PCI_IO_PROTOCOL *current=NULL;UINT32 id=0,subsystem=0;
  if(Bs->HandleProtocol(Gpu,&PciGuid,(VOID **)&current)==EFI_SUCCESS && current==Io && current==Gu.pci &&
     Io->Pci.Read(Io,EfiPciIoWidthUint32,0,1,&id)==EFI_SUCCESS && id==GU_BOARD_ID &&
     Io->Pci.Read(Io,EfiPciIoWidthUint32,0x2c,1,&subsystem)==EFI_SUCCESS && subsystem==GU_BOARD_SUBSYSTEM)GuClearMaster();
  Bs->RestoreTPL(old);
}
