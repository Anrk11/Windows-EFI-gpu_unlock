/* Actual EFI adapter exercised with RAM-backed protocols, never hardware. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#undef NULL
#include "main.c"
#define TEST_GPU ((VOID *)(UINTN)0x7673a918)
static EFI_BOOT_SERVICES boot;
static EFI_PCI_IO_PROTOCOL pci;
static EFI_DRIVER_BINDING_PROTOCOL binding;
static EFI_LOADED_IMAGE_PROTOCOL loaded;
static EFI_FILE_PROTOCOL file;
static EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR bar;
static EFI_OPEN_PROTOCOL_INFORMATION_ENTRY entries[2];
static EFI_HANDLE gpu_handles[2],gop_handles[1];
static UINT8 path[]={4,6,20,0,0xf8,0x55,0xcf,0x29,0x75,0xb6,0x5d,0x4f,0x8f,0x2f,0xb8,0x7a,0x3e,0xcf,0xd0,0x63,0x7f,0xff,4,0};
static UINT32 cfg[64],regs[16];
static UINT32 OriginalRegs[16];
static UINT32 CapturedRegs[16];
static BOOLEAN HaveCapturedRegs;
static UINT8 PromFixture[0x100000];
static int detached,disconnect_count,reconnect_count,fault,enable_count;
static int vga_enabled,vga_calls;
static UINTN dmaAllocations;
static VOID *dmaBuffers[6];
static void check(int ok,const char *message);
static EFI_STATUS EFIAPI dma_allocate(EFI_PCI_IO_PROTOCOL *p,EFI_ALLOCATE_TYPE t,EFI_MEMORY_TYPE m,UINTN pages,VOID **out,UINT64 attrs){
  (void)p;check(t==AllocateAnyPages && m==EfiBootServicesData && !attrs && detached && !(cfg[1]&4),"DMA buffer allocation while BME off");
  check(dmaAllocations<6,"bounded DMA allocations");
  if(fault==33)return EFI_UNSUPPORTED;
  if(posix_memalign(out,4096,pages*4096))return EFI_OUT_OF_RESOURCES;
  dmaBuffers[dmaAllocations++]=*out;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI dma_map(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_OPERATION op,VOID *cpu,UINTN *n,EFI_PHYSICAL_ADDRESS *bus,VOID **token){
  (void)p;check(op==EfiPciIoOperationBusMasterCommonBuffer && detached && !(cfg[1]&4) && cpu && *n,"mapping only, never submit GPU work");
  if(fault==28 && dmaAllocations==3)return EFI_DEVICE_ERROR;
  if(fault==34)return EFI_UNSUPPORTED;
  *bus=0x10000000ULL*dmaAllocations;*token=fault==31 || fault==32 || fault==35?NULL:cpu;
  if(fault==35)*n-=4096;
  if(fault==30)cfg[1]|=4;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI dma_unmap(EFI_PCI_IO_PROTOCOL *p,VOID *token){(void)p;check((fault==31 || fault==32 || fault==35?token==NULL:token!=NULL) && detached && !(cfg[1]&4),"unmap exact cookie while BME off");return fault==27 || fault==32?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI dma_free(EFI_PCI_IO_PROTOCOL *p,UINTN pages,VOID *cpu){(void)p;check(pages && detached && !(cfg[1]&4),"free only during stopped state");for(UINTN i=0;i<dmaAllocations;i++)if(dmaBuffers[i]==cpu)dmaBuffers[i]=NULL;free(cpu);return EFI_SUCCESS;}
static void check(int ok,const char *message){if(!ok){fprintf(stderr,"FAIL adapter fault=%d %s\n",fault,message);exit(1);}}
static int guid_eq(EFI_GUID *a,EFI_GUID *b){return !memcmp(a,b,sizeof(*a));}
static EFI_STATUS EFIAPI protocol(EFI_HANDLE h,EFI_GUID *g,VOID **out){
  if(fault==10 && detached && h==TEST_GPU && guid_eq(g,&PciGuid))return EFI_NOT_FOUND;
  if(h==TEST_GPU && guid_eq(g,&PciGuid))*out=&pci;
  else if(h==Driver && guid_eq(g,&BindGuid))*out=&binding;
  else if(h==Driver && guid_eq(g,&ImageGuid))*out=&loaded;
  else return EFI_NOT_FOUND;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI free_pool(VOID *p){(void)p;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI location(EFI_PCI_IO_PROTOCOL *p,UINTN *seg,UINTN *bus,UINTN *dev,UINTN *fun){(void)p;*seg=0;*bus=7;*dev=*fun=0;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI config_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 at,UINTN n,VOID *out){
  (void)p;check(w==EfiPciIoWidthUint32 && at%4==0 && at/4+n<=64,"bounded config read");memcpy(out,cfg+at/4,n*4);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mem_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 b,UINT64 at,UINTN n,VOID *out){
  (void)p;check(w==EfiPciIoWidthUint32 && b==0,"DWORD BAR0 read");
  check(cfg[1]&2,"never read MMIO with MEM decode disabled");
  if(detached)check(!(cfg[1]&4),"MMIO inspection never runs with bus mastering enabled");
  if(at>=0x300000 && at<=0x400000 && n==256 && at+n*4<=0x400000){memcpy(out,PromFixture+at-0x300000,n*4);if(fault==26 && at==0x300000)((UINT8 *)out)[0]^=1;return EFI_SUCCESS;}
  check(n==1,"single register read");
  for(UINTN i=0;i<16;i++)if(Offsets[i]==at){*(UINT32 *)out=regs[i];return fault==6?EFI_DEVICE_ERROR:EFI_SUCCESS;}
  check(0,"unlisted register");return EFI_UNSUPPORTED;
}
static EFI_STATUS EFIAPI attrs(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_ATTRIBUTE_OPERATION op,UINT64 mask,UINT64 *out){
  (void)p;
  if(op==EfiPciIoAttributeOperationGet){check(!mask && out,"attribute get");*out=((cfg[1]&7)<<8)|((!detached && (cfg[1]&7)) || vga_enabled?0x18:0);return EFI_SUCCESS;}
  if(op==EfiPciIoAttributeOperationSupported){check(!mask && out,"supported attribute read");*out=fault==15?0x100:fault==40?0x700:0x718;return EFI_SUCCESS;}
  if(op==EfiPciIoAttributeOperationEnable && mask==0x18){
    check(detached && !(cfg[1]&4) && !out,"VGA ranges only, no BME");vga_calls++;
    if(fault==41)return EFI_DEVICE_ERROR;
    vga_enabled=fault!=43;
    if(fault==42)return EFI_DEVICE_ERROR;
    if(fault==44)cfg[1]|=4;
    return EFI_SUCCESS;
  }
  check(op==EfiPciIoAttributeOperationEnable && mask==0x300 && !out && detached,"only IO/MEM enabled with no driver");
  enable_count++;
  if(fault==16)return EFI_DEVICE_ERROR;
  if(fault==18)return EFI_SUCCESS;
  cfg[1]|=3;
  if(fault==17)return EFI_DEVICE_ERROR;
  if(fault==19)cfg[1]|=4;
  if(fault==20)regs[12]^=1;
  if(fault==22)cfg[4]^=0x1000000;
  if(fault==25)((UINT8 *)(UINTN)0x7911b000)[0x280]^=1;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI get_bar(EFI_PCI_IO_PROTOCOL *p,UINT8 index,UINT64 *supported,VOID **out){(void)p;check(!index && !supported,"BAR0 only");*out=&bar;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI open_info(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *n){
  check(h==Gpu && guid_eq(g,&PciGuid),"target ownership only");
  *out=entries;*n=detached && !(fault==23 && enable_count)?0:2;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI locate(EFI_LOCATE_SEARCH_TYPE kind,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **out){
  check(kind==ByProtocol && !key,"protocol search");
  if(guid_eq(g,&PciGuid)){*out=gpu_handles;*n=fault==3?2:1;return EFI_SUCCESS;}
  if(guid_eq(g,&GopGuid)){if(detached && !(fault==24 && enable_count))return EFI_NOT_FOUND;*out=gop_handles;*n=1;return EFI_SUCCESS;}
  return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI write_log(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data){
  (void)f;
  if(*n>=29 && !memcmp(data,"headless-windows-start-intent",29)){
    if(fault==37)return EFI_DEVICE_ERROR;
    if(fault==38)cfg[4]^=0x1000000;
  }
  if(fault==29 && *n>=22 && !memcmp(data,"dma-buffer-bus-address",22))return EFI_DEVICE_ERROR;
  return fault==21 && *n>=20 && !memcmp(data,"decode-enable-intent",20)?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI flush_log(EFI_FILE_PROTOCOL *f){(void)f;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI disconnect_mock(EFI_HANDLE h,EFI_HANDLE d,EFI_HANDLE child){
  check(h==Gpu && d==Driver && !child,"only exact driver detached");disconnect_count++;detached=1;
  cfg[1]=fault==8?5:0;
  UINT8 *image=(VOID *)(UINTN)0x7911b000;
  if(fault!=14)memset(image+0x4308,0,8);
  if(fault==9)image[0x280]^=1;
  if(fault==11)image[0x4308]=1;
  if(fault==12)image[0x4310]^=1;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI reconnect_mock(EFI_HANDLE h,EFI_HANDLE *drivers,EFI_DEVICE_PATH_PROTOCOL *path_arg,BOOLEAN recurse){
  check(h==Gpu && drivers[0]==Driver && !drivers[1] && !path_arg && recurse,"exact driver recovery");reconnect_count++;detached=0;cfg[1]=7;
  UINT8 *image=(VOID *)(UINTN)0x7911b000;
  if(fault!=13)memcpy(image+0x4308,EntryCode+0x4308-0x280,8);
  if(fault==9)image[0x280]^=1;
  if(fault==12)image[0x4310]^=1;
  if(fault==25 && enable_count)image[0x280]^=1;
  memcpy(regs,OriginalRegs,sizeof(regs));cfg[4]=0xfa000000;
  return EFI_SUCCESS;
}
static void setup(void){
  detached=disconnect_count=reconnect_count=enable_count=0;SnapshotSequence=0;HaveEntryCode=FALSE;ExpectStopped=QuietObservation=DecodeInspectionActive=FALSE;Gpu=TEST_GPU;Driver=(VOID *)2;Io=&pci;Bs=&boot;LogFile=&file;
  memset(cfg,0,sizeof(cfg));memset(regs,0,sizeof(regs));
  cfg[0]=cfg[11]=0x250310de;cfg[1]=7;cfg[2]=0x030000a1;cfg[4]=0xfa000000;
  regs[0]=0x3000;regs[5]=0xb76000a1;regs[6]=0x8b8f;regs[7]=0x3ff;regs[8]=0x10;
  regs[11]=0xffffff8f;regs[12]=0x46446172; /* actual first handoff capture */
  if(HaveCapturedRegs)memcpy(regs,CapturedRegs,sizeof(regs));
  memcpy(OriginalRegs,regs,sizeof(regs));
  memset(&DmaProbe,0,sizeof(DmaProbe));dmaAllocations=0;memset(dmaBuffers,0,sizeof(dmaBuffers));VgaDecodeActive=FALSE;vga_enabled=vga_calls=0;
  WindowsImage=fault==39?NULL:(VOID *)11;
  pci.GetLocation=location;pci.Pci.Read=config_read;pci.Mem.Read=mem_read;pci.Attributes=attrs;pci.GetBarAttributes=get_bar;
  pci.AllocateBuffer=dma_allocate;pci.Map=dma_map;pci.Unmap=dma_unmap;pci.FreeBuffer=dma_free;
  boot.HandleProtocol=protocol;boot.FreePool=free_pool;boot.OpenProtocolInformation=open_info;boot.LocateHandleBuffer=locate;boot.DisconnectController=disconnect_mock;boot.ConnectController=reconnect_mock;
  file.Write=write_log;file.Flush=flush_log;
  bar.Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;bar.Len=0x2b;bar.ResType=ACPI_ADDRESS_SPACE_TYPE_MEM;bar.AddrRangeMin=0xfa000000;bar.AddrLen=0x1000000;
  binding.Version=0x24;binding.DriverBindingHandle=Driver;binding.ImageHandle=Driver;binding.Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)0x7911c03c;
  loaded.ImageBase=(VOID *)(UINTN)0x7911b000;loaded.ImageSize=0x47e0;loaded.ImageCodeType=EfiBootServicesCode;loaded.FilePath=(VOID *)path;
  entries[0].AgentHandle=Driver;entries[0].Attributes=EFI_OPEN_PROTOCOL_BY_DRIVER;entries[0].ControllerHandle=Gpu;entries[0].OpenCount=1;
  entries[1].AgentHandle=Driver;entries[1].Attributes=EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER;entries[1].ControllerHandle=(VOID *)3;entries[1].OpenCount=1;
  gpu_handles[0]=gpu_handles[1]=Gpu;gop_handles[0]=(VOID *)3;
}
int main(int argc,char **argv){
  check(argc==4,"supply captured IMG36.BIN, handoff log and PROM.BIN");
  FILE *prom=fopen(argv[3],"rb");check(prom!=NULL,"open captured PROM");check(fread(PromFixture,1,sizeof(PromFixture),prom)==sizeof(PromFixture),"read captured PROM");fclose(prom);
  if(argc>=3){
    FILE *log=fopen(argv[2],"r");check(log!=NULL,"open real handoff log");
    char line[200];unsigned long long offset=0,value=0;UINT32 found=0;BOOLEAN pending=FALSE;
    while(fgets(line,sizeof(line),log)){
      if(!strncmp(line,"entry-ss0=",10))break; /* Only replay the first snapshot. */
      if(sscanf(line,"layout-register-offset=0x%llx",&offset)==1)pending=TRUE;
      else if(pending && sscanf(line,"layout-register-value=0x%llx",&value)==1){
        check(value<=0xffffffffULL,"captured DWORD in range");
        for(UINTN i=0;i<16;i++)if(Offsets[i]==offset){check(!(found&(1U<<i)),"unique captured register");CapturedRegs[i]=(UINT32)value;found|=1U<<i;}
        pending=FALSE;
      }
    }
    fclose(log);check(found==0xffff,"all 16 real captured registers present");HaveCapturedRegs=TRUE;
  }
  VOID *mapping=mmap((VOID *)(UINTN)0x7911b000,0x5000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
  check(mapping!=(VOID *)-1,"map captured driver in test process only");
  FILE *input=fopen(argv[1],"rb");check(input!=NULL,"open capture");check(fread(mapping,1,0x47e0,input)==0x47e0,"read capture");fclose(input);
#ifdef HT_HEADLESS_WINDOWS_TEST
#ifdef HT_RESTORE_VGA_DECODE
  const int last_fault=44;
#else
  const int last_fault=39;
#endif
#else
  const int last_fault=35;
#endif
  for(fault=0;fault<=last_fault;fault++){
    setup();
    if(fault==1)binding.Version++;
    if(fault==2)((UINT8 *)mapping)[0x280]^=1;
    if(fault==4)entries[0].AgentHandle=(VOID *)4;
    if(fault==5)gop_handles[0]=(VOID *)4;
    if(fault==7)bar.AddrLen=0x2000000;
    EFI_STATUS s=FindGpu();
    HT_TEST t={0};t.Read=State;t.Log=Event;t.Disconnect=Disconnect;t.Reconnect=Reconnect;
    t.InspectReleased=InspectReleased;
#ifdef HT_HEADLESS_WINDOWS_TEST
    if(fault>=36)t.PrepareWindowsWithoutGop=PrepareHeadless;
#endif
    if(s==EFI_SUCCESS)s=HtRun(&t);
    if(fault==36)check(s==EFI_SUCCESS && t.HeadlessReady && !t.Restored && t.Released && disconnect_count==1 && !reconnect_count && enable_count==1,"headless handoff only after full passing inspection; no GOP reconnect");
    else if(fault==0 || fault==31)check(s==EFI_SUCCESS && t.Restored && t.Released && disconnect_count==1 && reconnect_count==1 && enable_count==1,"release, decode inspection, opaque mapping cookie and recovery");
    else if(fault==27 || fault==30 || fault==32)check(s!=EFI_SUCCESS && DmaProbe.Retained && !t.Restored && t.Released && !reconnect_count,"unsafe DMA cleanup forbids reconnect and OS return");
    else if(fault==8 || fault>=15){check(s!=EFI_SUCCESS && t.Restored && t.Released && disconnect_count==1 && reconnect_count==1,"inspection error cannot pass; still recover");check(enable_count<=1,"no enable retry");if(fault==15 || fault==21 || fault==8)check(!enable_count,"no enable after failed prerequisite or intent log");}
    else if(fault==13)check(s!=EFI_SUCCESS && !t.Restored && t.Released && reconnect_count==1,"restored image must exactly match entry");
    else if(fault>=9)check(s!=EFI_SUCCESS && t.Restored && !t.Released && disconnect_count==1 && reconnect_count==1,"unexpected post-Stop image/interface transition refused");
    else check(s!=EFI_SUCCESS && !disconnect_count && !reconnect_count,"adapter rejects fault before mutation");
    if(fault==2)((UINT8 *)mapping)[0x280]^=1;
    if(fault==13)memcpy((UINT8 *)mapping+0x4308,EntryCode+0x4308-0x280,8);
    if(t.HeadlessReady)memcpy((UINT8 *)mapping+0x4308,EntryCode+0x4308-0x280,8); /* test-process disposal only */
    if(fault>=36)check(vga_calls<=1,"no retry after VGA restore failure");
#ifdef HT_RESTORE_VGA_DECODE
    if(fault==36)check(vga_calls==1 && vga_enabled && VgaDecodeActive && !(cfg[1]&4),"restored original VGA forwarding with BME off");
#endif
    if(DmaProbe.Retained){for(UINTN i=0;i<dmaAllocations;i++)free(dmaBuffers[i]);memcpy((UINT8 *)mapping+0x4308,EntryCode+0x4308-0x280,8);}
  }
  munmap(mapping,0x5000);printf("PASS: %d actual-adapter scenarios, DMA, headless preparation and failure recovery; no hardware accessed\n",last_fault+1);return 0;
}
