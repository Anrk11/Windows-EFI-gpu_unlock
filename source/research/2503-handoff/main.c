/* GPL-2.0-only. Exact captured X99/2503 display handoff experiment.
 * HT_EXECUTE_CORE selects the separately built full unlock experiment.
 * Ordinary builds have no payload, reset or direct PCI/MMIO writes.
 * DisconnectController/ConnectController WILL invoke firmware drivers which may
 * change GPU state. This image is experimental and has not been hardware tested.
 */
#include <Uefi.h>
#include <Protocol/PciIo.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleFileSystem.h>
#include <IndustryStandard/Acpi.h>
#include "../../internal-boot/unlock/sha256.h"
#include "transaction.h"
#include "windows.h"
#include "dma_probe.h"
#ifdef HT_EXECUTE_CORE
#include "core_bridge.h"
#include "../../internal-boot/unlock/checks.h"
#ifdef HT_INTERNAL_AUTO
#include "auto_policy.h"
#define HT_LOG_PATH AUTO_LOG_PATH
#else
#define HT_LOG_PATH L"\\2503-UNLOCK.TXT"
#endif
static BOOLEAN CoreMayRecover(VOID *unused);
static BOOLEAN CoreFinalGuard(VOID);
static EFI_STATUS PrepareCoreWindows(const HT_STATE *entry);
#else
#define HT_LOG_PATH L"\\2503-HANDOFF.TXT"
#endif
static DP_CONTEXT DmaProbe;
static EFI_HANDLE WindowsImage;
static UINT8 PromBytes[0x100000];
static EFI_BOOT_SERVICES *Bs;
static EFI_FILE_PROTOCOL *LogFile,*Root;
static EFI_HANDLE Gpu,Driver;
static EFI_PCI_IO_PROTOCOL *Io;
static UINTN SnapshotSequence;
static UINT8 EntryCode[0x4180];
static BOOLEAN HaveEntryCode;
static BOOLEAN ExpectStopped,QuietObservation,DecodeInspectionActive;
static BOOLEAN VgaDecodeActive;
static UINT64 InspectionAttributes(VOID){return VgaDecodeActive?0x318:0x300;}
static EFI_GUID PciGuid=EFI_PCI_IO_PROTOCOL_GUID,BindGuid=EFI_DRIVER_BINDING_PROTOCOL_GUID;
static EFI_GUID ImageGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID,FsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;
static EFI_GUID GopGuid=EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table);
/* Keep an explicit PE base relocation for firmware loaders and the PE audit. */
static EFI_STATUS (EFIAPI *volatile EntryAnchor)(EFI_HANDLE,EFI_SYSTEM_TABLE *)=efi_main;
static const UINT32 Offsets[]={0x1183a4,0x625f04,0x1fa7c8,0x1fa82c,0x1fa830,
  0,0x118128,0x118234,0x110100,0x1fa824,0x1fa828,0x823804,0x82381c,0x823820,0x824148,0x8241e0};
static EFI_STATUS Event(VOID *unused,const char *name,UINT64 value) {
  (void)unused;char line[160];UINTN n=0;
  while(*name && n<110)line[n++]=*name++;
  line[n++]='=';line[n++]='0';line[n++]='x';
  for(int i=15;i>=0;i--)line[n++]="0123456789abcdef"[(value>>(4*i))&15];
  line[n++]='\r';line[n++]='\n';UINTN written=n;
  EFI_STATUS s=LogFile->Write(LogFile,&written,line);
  return s==EFI_SUCCESS && written==n?LogFile->Flush(LogFile):EFI_DEVICE_ERROR;
}
static EFI_STATUS Observation(const char *name,UINT64 value) {
  /* The final pre-disconnect snapshot performs no disk/log callbacks. */
  return SnapshotSequence==2 || QuietObservation?EFI_SUCCESS:Event(NULL,name,value);
}
static BOOLEAN CodePinned(VOID) {
  EFI_DRIVER_BINDING_PROTOCOL *b=NULL;EFI_LOADED_IMAGE_PROTOCOL *im=NULL;
  EFI_STATUS bindingStatus=Bs->HandleProtocol(Driver,&BindGuid,(VOID **)&b);
  if(bindingStatus!=EFI_SUCCESS || !b){Observation("driver-binding-query-status",bindingStatus);Observation("driver-binding-missing",b==NULL);return FALSE;}
  if(b->Version!=0x24 || b->DriverBindingHandle!=Driver || b->ImageHandle!=Driver || (UINTN)b->Stop!=0x7911c03c){
    Observation("driver-binding-version",b->Version);Observation("driver-binding-handle",(UINTN)b->DriverBindingHandle);
    Observation("driver-binding-image",(UINTN)b->ImageHandle);Observation("driver-stop-address",(UINTN)b->Stop);return FALSE;
  }
  EFI_STATUS imageStatus=Bs->HandleProtocol(b->ImageHandle,&ImageGuid,(VOID **)&im);
  if(imageStatus!=EFI_SUCCESS || !im){Observation("driver-image-query-status",imageStatus);Observation("driver-image-missing",im==NULL);return FALSE;}
  if((UINTN)im->ImageBase!=0x7911b000 || im->ImageSize!=0x47e0 || im->ImageCodeType!=EfiBootServicesCode || !im->FilePath){
    Observation("driver-image-base",(UINTN)im->ImageBase);Observation("driver-image-size",im->ImageSize);
    Observation("driver-image-code-type",im->ImageCodeType);Observation("driver-image-path-missing",im->FilePath==NULL);return FALSE;
  }
  static const UINT8 expected[]={4,6,20,0,0xf8,0x55,0xcf,0x29,0x75,0xb6,0x5d,0x4f,0x8f,0x2f,0xb8,0x7a,0x3e,0xcf,0xd0,0x63,0x7f,0xff,4,0};
  const UINT8 *path=(const VOID *)im->FilePath;
  for(UINTN i=0;i<sizeof(expected);i++)if(path[i]!=expected[i]){Observation("driver-path-check-failed",1);return FALSE;}
  /* Loaded .text contains relocations: pin the observed load base too.
   * This fingerprint is intentionally not valid for arbitrary load addresses. */
  const UINT8 *code=(UINT8 *)im->ImageBase+0x280;
  BOOLEAN match=GuHashMatches(code,0x4180,
    "a93dc5c3b5640d568b103c445f20d36e24bdc4341a91bee7e5c289d736fdbabb");
  /* This image puts a writable controller handle inside .text at RVA 0x4308.
   * Start stores its controller there at 0x1507; Stop clears the qword at
   * 0x1717. Verified against captured machine code and handoff-test-03.
   * Accept exactly that data transition only in the post-Stop observation.
   * No byte is masked in preflight or restored-image verification. */
  const UINTN slot=0x4308-0x280;
  if(ExpectStopped && HaveEntryCode){
    BOOLEAN stopped=TRUE;
    for(UINTN i=0;i<8;i++)if(EntryCode[slot+i]!=(UINT8)((UINTN)Gpu>>(8*i)))stopped=FALSE;
    for(UINTN i=0;i<sizeof(EntryCode);i++){
      UINT8 expectedByte=i>=slot && i<slot+8?0:EntryCode[i];
      if(code[i]!=expectedByte)stopped=FALSE;
    }
    if(stopped){Observation("driver-stop-controller-handle-cleared",1);return TRUE;}
    Observation("driver-stop-image-transition-refused",1);
    match=FALSE;
  }
  if(match && !HaveEntryCode){for(UINTN i=0;i<sizeof(EntryCode);i++)EntryCode[i]=code[i];HaveEntryCode=TRUE;}
  if(!match){
    Observation("driver-code-hash-check-failed",1);
    if(HaveEntryCode){
      UINTN differences=0;
      for(UINTN i=0;i<sizeof(EntryCode);i++)if(EntryCode[i]!=code[i]){
        if(differences<32){Observation("driver-code-change-rva",i+0x280);Observation("driver-code-change-entry-byte",EntryCode[i]);Observation("driver-code-change-observed-byte",code[i]);}
        differences++;
      }
      Observation("driver-code-change-byte-count",differences);
    }
  }
  return match;
}
static EFI_STATUS State(VOID *unused,HT_STATE *s) {
  (void)unused;EFI_PCI_IO_PROTOCOL *current=NULL;
  SnapshotSequence++;
  if(Observation("snapshot-phase",SnapshotSequence)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  EFI_STATUS interfaceStatus=Bs->HandleProtocol(Gpu,&PciGuid,(VOID **)&current);
  if(interfaceStatus!=EFI_SUCCESS || current!=Io){Observation("pci-interface-query-status",interfaceStatus);Observation("pci-interface-changed",current!=Io);return EFI_UNSUPPORTED;}
  BOOLEAN pinned=CodePinned();
  /* After Stop, collect passive state even if its image fingerprint changed.
   * The final return remains an error: observations never authorize success. */
  if(!pinned && !ExpectStopped)return EFI_UNSUPPORTED;
  UINTN seg,bus,dev,fun;
  if(Io->GetLocation(Io,&seg,&bus,&dev,&fun)!=EFI_SUCCESS || seg || bus!=7 || dev || fun)return EFI_UNSUPPORTED;
  EFI_STATUS r=Io->Pci.Read(Io,EfiPciIoWidthUint32,0,64,s->Config);
  if(r!=EFI_SUCCESS)return r;
  if(s->Config[0]!=0x250310de || s->Config[2]!=0x030000a1 || s->Config[11]!=0x250310de || s->Config[4]!=0xfa000000)return EFI_UNSUPPORTED;
  r=Io->Attributes(Io,EfiPciIoAttributeOperationGet,0,&s->Attributes);if(r!=EFI_SUCCESS)return r;
  if(DecodeInspectionActive && ((s->Config[1]&0xffff)!=3 || s->Attributes!=InspectionAttributes()))return EFI_NOT_READY;
  if(Observation("pci-command",s->Config[1]&0xffff)!=EFI_SUCCESS || Observation("pci-attributes",s->Attributes)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  VOID *raw=NULL;r=Io->GetBarAttributes(Io,0,NULL,&raw);
  if(r!=EFI_SUCCESS || !raw)return r==EFI_SUCCESS?EFI_UNSUPPORTED:r;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR *bar=raw;
  BOOLEAN valid=bar->Desc==ACPI_ADDRESS_SPACE_DESCRIPTOR && bar->Len==0x2b && bar->ResType==ACPI_ADDRESS_SPACE_TYPE_MEM &&
    bar->AddrRangeMin==0xfa000000 && bar->AddrLen==0x1000000;
  r=Bs->FreePool(raw);if(r!=EFI_SUCCESS || !valid)return EFI_UNSUPPORTED;
  /* A successful Stop may disable MEM decode. Do not touch BAR registers in
   * that state; still collect ownership/GOP evidence and attempt recovery. */
  if(!(s->Config[1]&2) && Observation("registers-unavailable-mem-disabled",1)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  for(UINTN i=0;(s->Config[1]&2) && i<16;i++) {
    r=Io->Mem.Read(Io,EfiPciIoWidthUint32,0,Offsets[i],1,&s->Registers[i]);
    if(r!=EFI_SUCCESS || s->Registers[i]==0xffffffff)return EFI_DEVICE_ERROR;
    if(Observation("layout-register-offset",Offsets[i])!=EFI_SUCCESS || Observation("layout-register-value",s->Registers[i])!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  }
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opens=NULL;UINTN count=0;EFI_HANDLE child=NULL;
  r=Bs->OpenProtocolInformation(Gpu,&PciGuid,&opens,&count);if(r!=EFI_SUCCESS)return r;
  if(count>512){if(opens)Bs->FreePool(opens);return EFI_UNSUPPORTED;}
  for(UINTN i=0;i<count;i++) {
    if(opens[i].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER) {
      if(opens[i].AgentHandle!=Driver || s->Owner){r=EFI_UNSUPPORTED;break;}
      s->Owner=(UINTN)Driver;
    }
    if(opens[i].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) {
      if(opens[i].AgentHandle!=Driver || s->Children){r=EFI_UNSUPPORTED;break;}
      s->Children++;child=opens[i].ControllerHandle;
    }
  }
  if(opens && Bs->FreePool(opens)!=EFI_SUCCESS)r=EFI_DEVICE_ERROR;
  if(r!=EFI_SUCCESS)return r;
  EFI_HANDLE *gops=NULL;count=0;r=Bs->LocateHandleBuffer(ByProtocol,&GopGuid,NULL,&count,&gops);
  if(r==EFI_NOT_FOUND)r=EFI_SUCCESS;
  else if(r==EFI_SUCCESS) {
    s->Gops=count;
    if(count!=1 || !child || gops[0]!=child)r=EFI_UNSUPPORTED;
  }
  if(gops && Bs->FreePool(gops)!=EFI_SUCCESS)r=EFI_DEVICE_ERROR;
  if(Observation("gop-count",s->Gops)!=EFI_SUCCESS || Observation("owner-present",s->Owner!=0)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  return pinned?r:EFI_UNSUPPORTED;
}
static EFI_STATUS Disconnect(VOID *unused){(void)unused;EFI_STATUS s=Bs->DisconnectController(Gpu,Driver,NULL);ExpectStopped=TRUE;return s;}
static EFI_STATUS Reconnect(VOID *unused){(void)unused;
#ifdef HT_EXECUTE_CORE
  if(!CoreMayRecover(NULL))return EFI_ACCESS_DENIED;
#endif
  if(DmaProbe.Retained)return EFI_ACCESS_DENIED;DecodeInspectionActive=FALSE;VgaDecodeActive=FALSE;EFI_HANDLE images[]={Driver,NULL};EFI_STATUS s=Bs->ConnectController(Gpu,images,NULL,TRUE);ExpectStopped=FALSE;return s;}
static BOOLEAN ConfigMatches(const UINT32 *actual,const UINT32 *entry,UINT32 command) {
  for(UINTN i=0;i<64;i++){
    UINT32 expected=i==1?(entry[i]&0xffff0000U)|command:entry[i];
    if(actual[i]!=expected)return FALSE;
  }
  return TRUE;
}
static BOOLEAN DmaGuard(VOID *user){
  const HT_STATE *entry=user;HT_STATE now={0};QuietObservation=TRUE;EFI_STATUS s=State(NULL,&now);QuietObservation=FALSE;
  if(s!=EFI_SUCCESS || now.Owner || now.Children || now.Gops || now.Attributes!=InspectionAttributes() || !ConfigMatches(now.Config,entry->Config,3))return FALSE;
  for(UINTN i=0;i<16;i++)if(now.Registers[i]!=entry->Registers[i])return FALSE;
  return TRUE;
}
static EFI_STATUS InspectReleased(VOID *unused,const HT_STATE *entry) {
  (void)unused;
  /* One IO/MEM-only enable. No BUS_MASTER, VGA, raw config or MMIO writes.
   * A driver reconnect is always attempted by the outer transaction afterward. */
  const UINT64 mask=EFI_PCI_IO_ATTRIBUTE_IO|EFI_PCI_IO_ATTRIBUTE_MEMORY;
  UINT64 supported=0;EFI_STATUS s=Io->Attributes(Io,EfiPciIoAttributeOperationSupported,0,&supported);
  if(s!=EFI_SUCCESS || (supported&mask)!=mask)return s==EFI_SUCCESS?EFI_UNSUPPORTED:s;
  s=Event(NULL,"decode-enable-intent-io-mem-only",mask);if(s!=EFI_SUCCESS)return s;
  HT_STATE stopped={0};QuietObservation=TRUE;s=State(NULL,&stopped);QuietObservation=FALSE;
  if(s!=EFI_SUCCESS)return s;
  if(stopped.Owner || stopped.Children || stopped.Gops || stopped.Attributes || !ConfigMatches(stopped.Config,entry->Config,0))return EFI_NOT_READY;
  s=Io->Attributes(Io,EfiPciIoAttributeOperationEnable,mask,NULL);
  EFI_STATUS logged=Event(NULL,"decode-enable-return",s);
  if(s!=EFI_SUCCESS || logged!=EFI_SUCCESS)return s!=EFI_SUCCESS?s:logged;
  DecodeInspectionActive=TRUE;
  /* Check BME and exact config before allowing any BAR register reads. */
  UINT32 cfg[64]={0};UINT64 attributes=0;
  s=Io->Pci.Read(Io,EfiPciIoWidthUint32,0,64,cfg);if(s!=EFI_SUCCESS)return s;
  s=Io->Attributes(Io,EfiPciIoAttributeOperationGet,0,&attributes);if(s!=EFI_SUCCESS)return s;
  if(Event(NULL,"decode-readback-command",cfg[1]&0xffff)!=EFI_SUCCESS || Event(NULL,"decode-readback-attributes",attributes)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(!ConfigMatches(cfg,entry->Config,3) || attributes!=mask)return EFI_NOT_READY;
  HT_STATE observed={0};s=State(NULL,&observed);if(s!=EFI_SUCCESS)return s;
  if(observed.Owner || observed.Children || observed.Gops || observed.Attributes!=mask || !ConfigMatches(observed.Config,entry->Config,3))return EFI_NOT_READY;
  for(UINTN i=0;i<16;i++)if(observed.Registers[i]!=entry->Registers[i])return EFI_NOT_READY;
  UINT64 fb=(UINT64)observed.Registers[0]<<20,vga=fb-(1ULL<<20);
  UINT32 vr=observed.Registers[1];
  if(vr&8){vga=(UINT64)(vr>>8)<<16;if(vga<fb-(1ULL<<20))vga=fb-0x20000;}
  UINT64 low=(UINT64)(observed.Registers[3]>>4)<<12,high=(UINT64)(observed.Registers[4]>>4)<<12;
  BOOLEAN top=high>=low && (((high+0xffff)&~0xffffULL)>>20)==(fb>>20);
  if(!(observed.Registers[2]&1) || fb<(256ULL<<20) || vga>=fb)return EFI_UNSUPPORTED;
  UINT64 end=(top && low<vga?low:vga)&~0x1ffffULL;
  if(end<(1ULL<<20) || end>fb || end-(1ULL<<20)<fb-(256ULL<<20))return EFI_UNSUPPORTED;
  if(Event(NULL,"layout-candidate-frts-offset",end-(1ULL<<20))!=EFI_SUCCESS ||
     Event(NULL,"layout-candidate-wpr-end-offset",end)!=EFI_SUCCESS ||
     Event(NULL,"decode-bme-off-registers-unchanged",1)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  /* Read and hash the actual PROM, already parsed offline from the first
   * capture. Matching it does not validate the future DMA/unlock transaction. */
  for(UINTN i=0;i<sizeof(PromBytes);i+=1024){
    s=Io->Mem.Read(Io,EfiPciIoWidthUint32,0,0x300000+i,256,PromBytes+i);
    if(s!=EFI_SUCCESS)return s;
  }
  BOOLEAN prom=GuHashMatches(PromBytes,sizeof(PromBytes),"023fc329d52741164ccb1632206883b87a778af6f5f8532a1dd5426fa09a13c5");
  if(Event(NULL,"live-prom-matches-offline-parsed-rom",prom)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(!prom)return EFI_UNSUPPORTED;
  DmaProbe.Io=Io;DmaProbe.User=(VOID *)entry;DmaProbe.Guard=DmaGuard;DmaProbe.Log=Event;
  return DpRun(&DmaProbe); /* Zero-buffer mapping only; no device commands. */
}
#ifdef HT_HEADLESS_WINDOWS_TEST
#ifndef HT_EXECUTE_CORE
static HT_STATE HeadlessEntry;
#endif
static EFI_STATUS RecordPrebootResult(EFI_STATUS status,BOOLEAN headlessReady){
  if(!headlessReady)return Event(NULL,"test-result",status);
  EFI_STATUS s=Event(NULL,"preboot-check-result",status);
  return s==EFI_SUCCESS?Event(NULL,"windows-visible-handoff-unverified",1):s;
}
static EFI_STATUS PrepareHeadless(VOID *unused,const HT_STATE *entry){
  (void)unused;
  if(!WindowsImage || DmaProbe.Retained || !DmaProbe.Used || !DmaGuard((VOID *)entry))return EFI_NOT_READY;
  for(UINTN i=0;i<6;i++)if(DmaProbe.Buffers[i].Cpu || DmaProbe.Buffers[i].Mapped)return EFI_NOT_READY;
#ifdef HT_EXECUTE_CORE
  return PrepareCoreWindows(entry);
#else
#ifdef HT_RESTORE_VGA_DECODE
  /* AMI Stop cleared the VGA forwarding attributes too. Restore exactly the
   * entry VGA ranges, independently of BME, without starting a GOP driver. */
  const UINT64 vga=EFI_PCI_IO_ATTRIBUTE_VGA_MEMORY|EFI_PCI_IO_ATTRIBUTE_VGA_IO;
  UINT64 supported=0;
  if((entry->Attributes&vga)!=vga || VgaDecodeActive)return EFI_UNSUPPORTED;
  EFI_STATUS s=Io->Attributes(Io,EfiPciIoAttributeOperationSupported,0,&supported);
  if(s!=EFI_SUCCESS || (supported&vga)!=vga)return EFI_UNSUPPORTED;
  if(Event(NULL,"restore-entry-vga-decode-intent",vga)!=EFI_SUCCESS || !DmaGuard((VOID *)entry))return EFI_NOT_READY;
  VgaDecodeActive=TRUE; /* A failed call can already have changed attributes. */
  s=Io->Attributes(Io,EfiPciIoAttributeOperationEnable,vga,NULL);
  EFI_STATUS logged=Event(NULL,"restore-entry-vga-decode-return",s);
  if(s!=EFI_SUCCESS || logged!=EFI_SUCCESS || !DmaGuard((VOID *)entry))return EFI_NOT_READY;
  if(Event(NULL,"vga-forwarding-restored-bme-still-off",1)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
#endif
  if(Event(NULL,"headless-windows-start-intent",1)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(!DmaGuard((VOID *)entry))return EFI_NOT_READY;
  HeadlessEntry=*entry;return EFI_SUCCESS;
#endif /* HT_EXECUTE_CORE */
}
static VOID RecordWindowsReturn(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs,EFI_STATUS status){
  /* StartImage returning is a failed OS handoff, even with EFI_SUCCESS.
   * Reopen only the existing log for an appended result; never touch console. */
  Root=NULL;LogFile=NULL;
  if(fs->OpenVolume(fs,&Root)!=EFI_SUCCESS || !Root)return;
  if(Root->Open(Root,&LogFile,HT_LOG_PATH,EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0)==EFI_SUCCESS && LogFile){
    if(LogFile->SetPosition && LogFile->SetPosition(LogFile,~0ULL)==EFI_SUCCESS &&
       Event(NULL,"headless-windows-returned",status)==EFI_SUCCESS)Event(NULL,"test-result",EFI_DEVICE_ERROR);
    LogFile->Close(LogFile);
  }
  Root->Close(Root);
}
#endif
static EFI_STATUS FindGpu(VOID) {
  Gpu=NULL;Io=NULL;Driver=NULL;
  EFI_HANDLE *handles=NULL;UINTN n=0,found=0;
  EFI_STATUS r=Bs->LocateHandleBuffer(ByProtocol,&PciGuid,NULL,&n,&handles);if(r!=EFI_SUCCESS)return r;
  if(n>4096){Bs->FreePool(handles);return EFI_UNSUPPORTED;}
  for(UINTN i=0;i<n;i++) {
    EFI_PCI_IO_PROTOCOL *io=NULL;UINT32 id=0,cls=0;
    if(Bs->HandleProtocol(handles[i],&PciGuid,(VOID **)&io)!=EFI_SUCCESS || !io || !io->Pci.Read)continue;
    if(io->Pci.Read(io,EfiPciIoWidthUint32,0,1,&id)!=EFI_SUCCESS || io->Pci.Read(io,EfiPciIoWidthUint32,8,1,&cls)!=EFI_SUCCESS){r=EFI_DEVICE_ERROR;break;}
    if((cls>>24)==3) {found++;if(id==0x250310de){Gpu=handles[i];Io=io;}}
  }
  if(Bs->FreePool(handles)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(r!=EFI_SUCCESS || found!=1 || !Io || !Io->Attributes || !Io->GetLocation || !Io->Mem.Read || !Io->GetBarAttributes)return EFI_UNSUPPORTED;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY *opens=NULL;n=0;
  r=Bs->OpenProtocolInformation(Gpu,&PciGuid,&opens,&n);if(r!=EFI_SUCCESS)return r;
  if(n>512){if(opens)Bs->FreePool(opens);return EFI_UNSUPPORTED;}
  for(UINTN i=0;i<n;i++)if(opens[i].Attributes & EFI_OPEN_PROTOCOL_BY_DRIVER){if(Driver){r=EFI_UNSUPPORTED;break;}Driver=opens[i].AgentHandle;}
  if(opens && Bs->FreePool(opens)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  return r==EFI_SUCCESS && Driver && CodePinned()?EFI_SUCCESS:EFI_UNSUPPORTED;
}
#ifdef HT_EXECUTE_CORE
#include "core_efi.h"
#endif
#ifdef HT_HEADLESS_WINDOWS_TEST
static BOOLEAN FinalHandoffGuard(VOID){
#ifdef HT_EXECUTE_CORE
  return CoreFinalGuard();
#else
  return DmaGuard(&HeadlessEntry);
#endif
}
#endif
static VOID StopHandoff(VOID){
#ifdef HT_EXECUTE_CORE
  CoreContain();
#endif
  for(;;)Bs->Stall(1000000);
}
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image,EFI_SYSTEM_TABLE *table) {
  if(EntryAnchor!=efi_main || !table)return EFI_UNSUPPORTED;
  Bs=table->BootServices;
  if(!Bs || !table->ConOut || !table->ConIn || !table->ConIn->ReadKeyStroke || !Bs->Stall || !Bs->DisconnectController || !Bs->ConnectController)return EFI_UNSUPPORTED;
  EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
  if(Bs->HandleProtocol(image,&ImageGuid,(VOID **)&loaded)!=EFI_SUCCESS || !loaded)return EFI_UNSUPPORTED;
  EFI_STATUS windows=PrepareWindows(Bs,image,loaded->DeviceHandle,&WindowsImage);
  if(windows!=EFI_SUCCESS){table->ConOut->OutputString(table->ConOut,L"Cannot prepare Windows return path. GPU test cancelled.\r\n");return windows;}
#ifdef HT_EXECUTE_CORE
#ifdef HT_INTERNAL_AUTO
  table->ConOut->OutputString(table->ConOut,L"RTX 3060 2503 AUTOMATIC COMPUTE UNLOCK\r\nPress ESC or ANY KEY within 5 seconds to skip unlock this boot.\r\nAn unconfirmed previous run prevents further automatic attempts.\r\n");
#else
  table->ConOut->OutputString(table->ConOut,L"RTX 3060 2503 FULL UNLOCK EXPERIMENT - FIRST HARDWARE RUN\r\nU runs firmware, writes unlock selectors and performs one GPU reset.\r\nOnly the captured X99/2503 card is accepted. No VBIOS flash or boot-variable changes.\r\nScreen may stay blank until Windows. Allow two minutes.\r\nIf Windows does not appear: power off, remove USB, cold boot Windows.\r\nPress U within 15 seconds. Any other key starts Windows without unlock.\r\n");
#endif
#elif defined(HT_HEADLESS_WINDOWS_TEST)
  table->ConOut->OutputString(table->ConOut,L"2503 WINDOWS HANDOFF TEST WITHOUT FIRMWARE GOP - EXPERIMENTAL\r\nAfter passing the batch, starts Windows WITHOUT reconnecting display firmware.\r\nNo unlock or firmware execution. Screen may stay black until Windows loads.\r\nIf black for two minutes: power off, remove USB, power on.\r\nPress T within 15 seconds. Any other key starts Windows.\r\n");
#else
  table->ConOut->OutputString(table->ConOut,L"RTX 3060 2503 BATCH + DMA MAPPING TEST - EXPERIMENTAL\r\nTests: identity, release, BME OFF, IO/MEM, layout, ROM, DMA map/unmap, recovery.\r\nNo unlock or firmware execution. Keep power on while testing.\r\nIf black for two minutes: power off, remove USB, power on.\r\nPress T within 15 seconds. Any other key starts Windows.\r\n");
#endif
  BOOLEAN selected=FALSE;
#ifdef HT_INTERNAL_AUTO
  selected=AutoSelect(table,Bs);
#else
  for(UINTN i=0;i<150;i++){EFI_INPUT_KEY key;EFI_STATUS k=table->ConIn->ReadKeyStroke(table->ConIn,&key);if(k==EFI_SUCCESS){
#ifdef HT_EXECUTE_CORE
    selected=key.UnicodeChar=='u'||key.UnicodeChar=='U';
#else
    selected=key.UnicodeChar=='t'||key.UnicodeChar=='T';
#endif
    break;}if(k!=EFI_NOT_READY)return k;Bs->Stall(100000);}
#endif
  if(!selected)return Bs->StartImage(WindowsImage,NULL,NULL);
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *fs=NULL;EFI_STATUS r;
  r=Bs->HandleProtocol(loaded->DeviceHandle,&FsGuid,(VOID **)&fs);if(r!=EFI_SUCCESS || !fs)return Bs->StartImage(WindowsImage,NULL,NULL);
  r=fs->OpenVolume(fs,&Root);if(r!=EFI_SUCCESS || !Root)return Bs->StartImage(WindowsImage,NULL,NULL);
#ifdef HT_INTERNAL_AUTO
  if(!AutoCanStart(Root)){Root->Close(Root);table->ConOut->OutputString(table->ConOut,L"Automatic unlock is disabled, unconfirmed, or unreadable. Starting Windows.\r\n");return Bs->StartImage(WindowsImage,NULL,NULL);}
#endif
  EFI_FILE_PROTOCOL *prior=NULL;
  r=Root->Open(Root,&prior,HT_LOG_PATH,EFI_FILE_MODE_READ,0);
  if(r!=EFI_NOT_FOUND || prior){if(prior)prior->Close(prior);Root->Close(Root);table->ConOut->OutputString(table->ConOut,L"Previous log exists. No repeat test. Starting Windows.\r\n");return Bs->StartImage(WindowsImage,NULL,NULL);}
  r=Root->Open(Root,&LogFile,HT_LOG_PATH,EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
  if(r!=EFI_SUCCESS || !LogFile){Root->Close(Root);return Bs->StartImage(WindowsImage,NULL,NULL);}
  if(!LogFile->Write || !LogFile->Flush || !LogFile->Close){if(LogFile->Close)LogFile->Close(LogFile);Root->Close(Root);return Bs->StartImage(WindowsImage,NULL,NULL);}
#ifdef HT_EXECUTE_CORE
#ifdef HT_INTERNAL_AUTO
  r=Event(NULL,"test-version",11);
  if(r==EFI_SUCCESS)r=AutoMarkPending(Root);
  if(r==EFI_SUCCESS)r=Event(NULL,"automatic-pending-marker-written",1);
#else
  r=Event(NULL,"test-version",10);
#endif
#elif defined(HT_HEADLESS_WINDOWS_TEST)
#ifdef HT_RESTORE_VGA_DECODE
  r=Event(NULL,"test-version",9);
#else
  r=Event(NULL,"test-version",8);
#endif
#else
  r=Event(NULL,"test-version",7);
#endif
  if(r==EFI_SUCCESS)r=FindGpu();
#ifdef HT_EXECUTE_CORE
  if(r==EFI_SUCCESS)r=CoreInitialize(image,loaded->DeviceHandle);
  if(r==EFI_SUCCESS)r=CoreLoadAssets();
#endif
  HT_TEST test={0};test.Read=State;test.Log=Event;test.Disconnect=Disconnect;test.Reconnect=Reconnect;
  test.InspectReleased=InspectReleased;
#ifdef HT_EXECUTE_CORE
  test.MayRecoverDisplay=CoreMayRecover;
#endif
#ifdef HT_HEADLESS_WINDOWS_TEST
  test.PrepareWindowsWithoutGop=PrepareHeadless;
#endif
  if(r==EFI_SUCCESS)r=HtRun(&test);
#ifdef HT_HEADLESS_WINDOWS_TEST
  EFI_STATUS finalLog=RecordPrebootResult(r,test.HeadlessReady);
#else
  EFI_STATUS finalLog=Event(NULL,"test-result",r);
#endif
  if(finalLog!=EFI_SUCCESS)r=finalLog;
#ifdef HT_HEADLESS_WINDOWS_TEST
  if(test.HeadlessReady){
    /* No calls through the detached console from here, even on boot failure.
     * A returned StartImage is not proof that Windows took over the GPU. */
    if(r!=EFI_SUCCESS || Event(NULL,"headless-ready",1)!=EFI_SUCCESS || !FinalHandoffGuard())StopHandoff();
    EFI_STATUS logClose=LogFile->Close(LogFile),rootClose=Root->Close(Root);
    if(logClose!=EFI_SUCCESS || rootClose!=EFI_SUCCESS || !FinalHandoffGuard())StopHandoff();
    EFI_STATUS bootReturned=Bs->StartImage(WindowsImage,NULL,NULL);
    RecordWindowsReturn(fs,bootReturned);
    StopHandoff();
  }
#endif
#ifdef HT_EXECUTE_CORE
  CoreReleaseAssets();
#endif
  if(DmaProbe.Retained || (test.Attempted && !test.Restored)) {
    Event(NULL,"manual-power-off-required",1);LogFile->Flush(LogFile);
    StopHandoff(); /* No console access, retry, reset or Windows handoff. */
  }
  EFI_STATUS close=LogFile->Close(LogFile);Root->Close(Root);
#ifdef HT_EXECUTE_CORE
  table->ConOut->OutputString(table->ConOut,L"UNLOCK DID NOT EXECUTE OR WAS CANCELLED BEFORE GPU EXECUTION.\r\nRecovery verified where needed. Read 2503-UNLOCK.TXT in Windows.\r\n");
#else
  table->ConOut->OutputString(table->ConOut,r==EFI_SUCCESS?
    L"BATCH PASS: handoff, IO/MEM, BME OFF, layout, ROM, DMA map/unmap and recovery.\r\nNEXT: review firmware execution integration in Windows. Unlock is not installed.\r\n":
    L"BATCH FAILED: recovery verified or no GPU change attempted.\r\nRead 2503-HANDOFF.TXT in Windows for the failed stage.\r\n");
#endif
  table->ConOut->OutputString(table->ConOut,L"Starting Windows in 10 seconds.\r\n");Bs->Stall(10000000);
  if(close!=EFI_SUCCESS)table->ConOut->OutputString(table->ConOut,L"Warning: log close failed.\r\n");
  return Bs->StartImage(WindowsImage,NULL,NULL);
}
