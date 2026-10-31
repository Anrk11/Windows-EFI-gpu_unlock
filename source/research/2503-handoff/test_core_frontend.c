/* Real standalone adapter + real core, RAM-backed EFI and captured code only. */
#define main core_fixture_main
#include "../../internal-boot/unlock/tests/test_efi_transaction.c"
#undef main
#include <sys/mman.h>
#define GU_PROM_SHA256 "023fc329d52741164ccb1632206883b87a778af6f5f8532a1dd5426fa09a13c5"
#include "main.c"
enum front_case {FA_OK,FA_ASSET_MISSING,FA_ASSET_HASH,FA_ASSET_SHORT,FA_ASSET_CLOSE,
  FA_CLAIM_FAIL,FA_OTHER_OWNER,FA_PRE_DRIFT,FA_CORE_TIMEOUT,FA_POST_WPR,
  FA_VGA_FAIL,FA_LOG_AFTER_CORE,FA_RELEASE_FAIL,FA_FINAL_DRIFT,FA_PERMISSION_DRIFT,FA_COUNT};
static enum front_case front_mode;
static GU_ASSET fixture_assets[5];
static EFI_FILE_PROTOCOL front_root,front_log,front_files[5];
static UINTN front_positions[5],front_opens,front_closes,front_connects;
static BOOLEAN front_exclusive;
static EFI_DRIVER_BINDING_PROTOCOL front_binding;
static EFI_LOADED_IMAGE_PROTOCOL front_image;
static EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR front_bar;
static EFI_OPEN_PROTOCOL_INFORMATION_ENTRY front_record;
static UINT8 front_driver[0x47e0];
static VOID *front_mapping;
static UINT8 front_path[]={4,6,20,0,0xf8,0x55,0xcf,0x29,0x75,0xb6,0x5d,0x4f,0x8f,0x2f,0xb8,0x7a,0x3e,0xcf,0xd0,0x63,0x7f,0xff,4,0};
static UINT8 front_usb_path[]={3,5,6,0,0,0,127,255,4,0};
#ifdef HT_INTERNAL_AUTO
static struct {HARDDRIVE_DEVICE_PATH Hd;EFI_DEVICE_PATH_PROTOCOL End;} front_esp_path;
static BOOLEAN front_usb_override;
#endif
static EFI_STATUS EFIAPI front_protocol(EFI_HANDLE h,EFI_GUID *g,VOID **out){
  if(h==Gpu && !memcmp(g,&PciGuid,sizeof(*g)))*out=&pci;
  else if(h==Driver && !memcmp(g,&BindGuid,sizeof(*g)))*out=&front_binding;
  else if(h==Driver && !memcmp(g,&ImageGuid,sizeof(*g)))*out=&front_image;
  else if(h==(EFI_HANDLE)0x99){EFI_GUID path=EFI_DEVICE_PATH_PROTOCOL_GUID;if(memcmp(g,&path,sizeof(path)))return EFI_NOT_FOUND;
#ifdef HT_INTERNAL_AUTO
    *out=front_usb_override?(VOID *)front_usb_path:(VOID *)&front_esp_path;
#else
    *out=front_usb_path;
#endif
  }
  else return EFI_NOT_FOUND;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_open_info(EFI_HANDLE h,EFI_GUID *g,EFI_OPEN_PROTOCOL_INFORMATION_ENTRY **out,UINTN *count){
  CHECK(h==Gpu && !memcmp(g,&PciGuid,sizeof(*g)));*count=0;*out=NULL;
  if(front_exclusive || front_mode==FA_OTHER_OWNER){
    front_record=(EFI_OPEN_PROTOCOL_INFORMATION_ENTRY){0};
    front_record.AgentHandle=front_exclusive?Gu.image:(EFI_HANDLE)0xdead;
    front_record.Attributes=front_exclusive?EFI_OPEN_PROTOCOL_EXCLUSIVE:EFI_OPEN_PROTOCOL_BY_DRIVER;
    front_record.OpenCount=1;*out=&front_record;*count=1;
  }
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_locate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *guid,VOID *key,UINTN *count,EFI_HANDLE **out){
  CHECK(type==ByProtocol && !memcmp(guid,&GopGuid,sizeof(*guid)) && !key);*count=0;*out=NULL;return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI front_free_pool(VOID *p){
  if(p==&front_record || p==&front_bar)return EFI_SUCCESS;
  return free_pool(p);
}
static EFI_STATUS EFIAPI front_bar_info(EFI_PCI_IO_PROTOCOL *p,UINT8 bar,UINT64 *attr,VOID **out){
  CHECK(p==&pci && !bar && !attr);*out=&front_bar;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_location(EFI_PCI_IO_PROTOCOL *p,UINTN *s,UINTN *b,UINTN *d,UINTN *f){
  CHECK(p==&pci);*s=0;*b=7;*d=*f=0;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_config_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT32 offset,UINTN count,VOID *out){
  if(front_mode==FA_POST_WPR && Gu.quiesced && !fake.live_buffers)registers[0x1fa828/4]=0x10;
  if(front_mode==FA_PERMISSION_DRIFT && Gu.quiesced && !fake.live_buffers)registers[PLM/4]=0xffffff8f;
  if(w==EfiPciIoWidthUint32 && offset==0 && count==64){CHECK(p==&pci);quiet_check();memcpy(out,config,sizeof(config));return EFI_SUCCESS;}
  return read_config(p,w,offset,count,out);
}
static EFI_STATUS EFIAPI front_mem_read(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_WIDTH w,UINT8 bar,UINT64 offset,UINTN count,VOID *out){
  if(w==EfiPciIoWidthUint32 && count==256){CHECK(p==&pci && !bar && offset>=0x300000 && offset+1024<=0x400000);memcpy(out,(UINT8 *)registers+offset,1024);return EFI_SUCCESS;}
  return read_memory(p,w,bar,offset,count,out);
}
static EFI_STATUS EFIAPI front_claim(EFI_HANDLE h,EFI_GUID *g,VOID **out,EFI_HANDLE agent,EFI_HANDLE controller,UINT32 attr){
  CHECK(h==Gpu && !memcmp(g,&PciGuid,sizeof(*g)) && agent==Gu.image && !controller && attr==EFI_OPEN_PROTOCOL_EXCLUSIVE);
  front_opens++;CHECK(front_opens==1);
  if(front_mode==FA_CLAIM_FAIL)return EFI_ACCESS_DENIED;
  front_exclusive=TRUE;*out=&pci;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_release(EFI_HANDLE h,EFI_GUID *g,EFI_HANDLE agent,EFI_HANDLE controller){
  CHECK(h==Gpu && !memcmp(g,&PciGuid,sizeof(*g)) && agent==Gu.image && !controller && front_exclusive);
  front_closes++;
  if(front_mode==FA_RELEASE_FAIL)return EFI_DEVICE_ERROR;
  front_exclusive=FALSE;return EFI_SUCCESS;
}
static EFI_TPL EFIAPI front_raise(EFI_TPL level){CHECK(level==TPL_NOTIFY);return TPL_APPLICATION;}
static VOID EFIAPI front_restore(EFI_TPL level){CHECK(level==TPL_APPLICATION);}
static EFI_STATUS EFIAPI front_connect(EFI_HANDLE h,EFI_HANDLE *drivers,EFI_DEVICE_PATH_PROTOCOL *p,BOOLEAN recursive){
  (void)h;(void)drivers;(void)p;(void)recursive;front_connects++;return EFI_DEVICE_ERROR;
}
static EFI_STATUS EFIAPI front_write(EFI_FILE_PROTOCOL *file,UINTN *n,VOID *p){
  CHECK(file==&front_log);
  if(front_mode==FA_LOG_AFTER_CORE && *n>=19 && !memcmp(p,"unlock-core-return=",19))return EFI_DEVICE_ERROR;
  UINTN used=strlen(fake.log);CHECK(*n<sizeof(fake.log)-used);memcpy(fake.log+used,p,*n);fake.log[used+*n]=0;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_flush(EFI_FILE_PROTOCOL *file){CHECK(file==&front_log);return EFI_SUCCESS;}
static UINTN front_index(EFI_FILE_PROTOCOL *file){CHECK(file>=front_files && file<front_files+5);return (UINTN)(file-front_files);}
static EFI_STATUS EFIAPI front_file_close(EFI_FILE_PROTOCOL *file){UINTN i=front_index(file);return front_mode==FA_ASSET_CLOSE && !i?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI front_info(EFI_FILE_PROTOCOL *file,EFI_GUID *guid,UINTN *n,VOID *out){
  EFI_GUID info=EFI_FILE_INFO_ID;CHECK(!memcmp(guid,&info,sizeof(info)) && *n>=sizeof(EFI_FILE_INFO));
  EFI_FILE_INFO *data=out;memset(data,0,sizeof(*data));data->Size=sizeof(*data);data->FileSize=fixture_assets[front_index(file)].firmware.size;*n=sizeof(*data);return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_read(EFI_FILE_PROTOCOL *file,UINTN *n,VOID *out){
  UINTN i=front_index(file),remaining=fixture_assets[i].firmware.size-front_positions[i];
  if(*n>remaining)*n=remaining;
  memcpy(out,fixture_assets[i].firmware.data+front_positions[i],*n);
  if(*n && !i && !front_positions[i] && front_mode==FA_ASSET_HASH)((UINT8 *)out)[0]^=1;
  if(*n && !i && front_mode==FA_ASSET_SHORT)(*n)--;
  front_positions[i]+=*n;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI front_file_open(EFI_FILE_PROTOCOL *root,EFI_FILE_PROTOCOL **out,CHAR16 *path,UINT64 mode,UINT64 attr){
  CHECK(root==&front_root && mode==EFI_FILE_MODE_READ && !attr);*out=NULL;
  for(UINTN i=0;i<5;i++)if(path==CoreSpecs[i].Path){
    if(front_mode==FA_ASSET_MISSING && !i)return EFI_NOT_FOUND;
    front_positions[i]=0;*out=&front_files[i];return EFI_SUCCESS;
  }
  CHECK(0);return EFI_NOT_FOUND;
}
static VOID front_setup(enum front_case mode){
  test_entry_ss0=model_regs[12];test_entry_ss1=model_regs[13];model_null_cookie=TRUE;
  setup(mode==FA_CORE_TIMEOUT?FWSEC_TIMEOUT:SUCCESS);front_mode=mode;
  for(UINTN i=0;i<5;i++)GuAssets[i]=(GU_ASSET){0};
  Bs=&bs;Io=&pci;Gpu=(EFI_HANDLE)(UINTN)0x7673a918;Driver=(EFI_HANDLE)2;WindowsImage=(EFI_HANDLE)3;
  LogFile=&front_log;Root=&front_root;SnapshotSequence=0;HaveEntryCode=FALSE;
  ExpectStopped=QuietObservation=DecodeInspectionActive=VgaDecodeActive=FALSE;
  CoreBridge=(UB_CONTEXT){0};CoreEntry=(HT_STATE){0};CoreInitialized=CoreAssetsVerified=CorePrepared=CoreLogFailed=FALSE;
  CoreClaimHeld=CoreClaimUncertain=front_exclusive=FALSE;front_opens=front_closes=front_connects=0;
  bs.HandleProtocol=front_protocol;bs.OpenProtocolInformation=front_open_info;bs.LocateHandleBuffer=front_locate;
  bs.FreePool=front_free_pool;bs.OpenProtocol=front_claim;bs.CloseProtocol=front_release;
  bs.RaiseTPL=front_raise;bs.RestoreTPL=front_restore;bs.ConnectController=front_connect;
  pci.Pci.Read=front_config_read;pci.Mem.Read=front_mem_read;pci.GetBarAttributes=front_bar_info;pci.GetLocation=front_location;
  front_bar=(EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR){0};front_bar.Desc=ACPI_ADDRESS_SPACE_DESCRIPTOR;
  front_bar.Len=0x2b;front_bar.ResType=ACPI_ADDRESS_SPACE_TYPE_MEM;front_bar.AddrRangeMin=GU_BAR0_BASE;front_bar.AddrLen=0x1000000;
  front_binding=(EFI_DRIVER_BINDING_PROTOCOL){0};front_binding.Version=0x24;front_binding.DriverBindingHandle=Driver;front_binding.ImageHandle=Driver;
  front_binding.Stop=(EFI_DRIVER_BINDING_STOP)(UINTN)0x7911c03c;
  front_image=(EFI_LOADED_IMAGE_PROTOCOL){0};front_image.ImageBase=front_mapping;front_image.ImageSize=0x47e0;
  front_image.ImageCodeType=EfiBootServicesCode;front_image.FilePath=(VOID *)front_path;
  memcpy(front_mapping,front_driver,sizeof(front_driver));CHECK(CodePinned());
  memset((UINT8 *)front_mapping+0x4308,0,8);ExpectStopped=TRUE;DecodeInspectionActive=TRUE;
  front_log=(EFI_FILE_PROTOCOL){0};front_log.Write=front_write;front_log.Flush=front_flush;
  front_root=(EFI_FILE_PROTOCOL){0};front_root.Open=front_file_open;
  for(UINTN i=0;i<5;i++){front_files[i]=(EFI_FILE_PROTOCOL){0};front_files[i].GetInfo=front_info;front_files[i].Read=front_read;front_files[i].Close=front_file_close;}
  DmaProbe=(DP_CONTEXT){0};DmaProbe.Used=TRUE;
#ifdef HT_INTERNAL_AUTO
  memset(&front_esp_path,0,sizeof(front_esp_path));
  front_esp_path.Hd.Header.Type=MEDIA_DEVICE_PATH;front_esp_path.Hd.Header.SubType=MEDIA_HARDDRIVE_DP;
  front_esp_path.Hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);front_esp_path.Hd.PartitionSize=1;
  front_esp_path.Hd.MBRType=2;front_esp_path.Hd.SignatureType=2;
  memcpy(front_esp_path.Hd.Signature,&GuWindowsPartition,16);
  front_esp_path.End.Type=END_DEVICE_PATH_TYPE;front_esp_path.End.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;front_esp_path.End.Length[0]=4;
  AutoPendingSpent=FALSE;
  front_usb_override=FALSE;
#endif
  memcpy(PromBytes,prom,sizeof(PromBytes)); /* Already verified by InspectReleased in EFI. */
}
int main(int argc,char **argv){
  CHECK(argc==6);load_fixture_assets(argv[1],argv[2]);memcpy(fixture_assets,GuAssets,sizeof(fixture_assets));
  FILE *f=fopen(argv[3],"r");CHECK(f);char row[512];unsigned count=0,at,a,b,c,d;
  while(fgets(row,sizeof(row),f))if(sscanf(row,"config[%x]=%x %x %x %x",&at,&a,&b,&c,&d)==5){CHECK(at==count*4 && count<=60);core_captured_config[count++]=a;core_captured_config[count++]=b;core_captured_config[count++]=c;core_captured_config[count++]=d;}fclose(f);CHECK(count==64);core_use_capture=TRUE;
  f=fopen(argv[4],"r");CHECK(f);unsigned long long offset=0,value=0;UINT32 found=0;
  while(fgets(row,sizeof(row),f)){
    if(!strncmp(row,"entry-ss0=",10))break;
    if(sscanf(row,"layout-register-offset=0x%llx",&offset)==1)continue;
    if(sscanf(row,"layout-register-value=0x%llx",&value)==1)for(UINTN i=0;i<16;i++)if(model_offsets[i]==offset){CHECK(!(found&(1U<<i)));model_regs[i]=(UINT32)value;found|=1U<<i;}
  }fclose(f);CHECK(found==0xffff);
  f=fopen(argv[5],"rb");CHECK(f);CHECK(fread(front_driver,1,sizeof(front_driver),f)==sizeof(front_driver));fclose(f);
  front_mapping=mmap((VOID *)(UINTN)0x7911b000,0x5000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);CHECK(front_mapping!=(VOID *)-1);
  setvbuf(stdout,NULL,_IONBF,0);
  for(unsigned mode=FA_OK;mode<FA_COUNT;mode++){
    front_setup((enum front_case)mode);
    HT_STATE entry={0};memcpy(entry.Config,config,sizeof(config));entry.Config[1]|=4;
    memcpy(entry.Registers,model_regs,sizeof(model_regs));entry.Attributes=0x718;entry.Owner=(UINTN)Driver;entry.Children=entry.Gops=1;
    EFI_STATUS status=CoreInitialize((EFI_HANDLE)0x1234,(EFI_HANDLE)0x99);CHECK(status==EFI_SUCCESS);
    status=CoreLoadAssets();
    if(mode>=FA_ASSET_MISSING && mode<=FA_ASSET_CLOSE){CHECK(status!=EFI_SUCCESS && !Gu.started && !front_opens);}
    else {
      CHECK(status==EFI_SUCCESS);
      if(mode==FA_PRE_DRIFT)registers[SS0/4]^=1;
      if(mode==FA_VGA_FAIL)windows_fault=WH_ENABLE_FAIL;
      status=PrepareHeadless(NULL,&entry);
      if(mode==FA_OK || mode==FA_FINAL_DRIFT){
        CHECK(status==EFI_SUCCESS && CorePrepared && CoreFinalGuard());
        CHECK(!front_exclusive && front_closes==1 && !Gu.owned && !Gu.allocations && !fake.pool_live);
        CHECK(fake.pci_attributes==0x318 && !(config[4]&4) && fake.flrs==1);
        CHECK(PrepareHeadless(NULL,&entry)!=EFI_SUCCESS && fake.flrs==1);
        if(mode==FA_FINAL_DRIFT){registers[SS1/4]^=1;CHECK(!FinalHandoffGuard());}
      } else CHECK(status!=EFI_SUCCESS && !CorePrepared);
    }
    if(Gu.started){
      CHECK(!CoreMayRecover(NULL));CHECK(Reconnect(NULL)==EFI_ACCESS_DENIED && !front_connects);
      CoreContain();CHECK(!(config[4]&4));
    } else if(mode!=FA_OTHER_OWNER)CHECK(CoreMayRecover(NULL));
    if(!Gu.started)CoreReleaseAssets();
    CHECK(fake.gsp_starts<=1 && fake.sec2_starts<=1 && fake.flrs<=1);
    printf("PASS full adapter case %u: status=%llx started=%u windows_prepared=%u claim=%zu/%zu\n",
      mode,(unsigned long long)status,Gu.started,CorePrepared,(size_t)front_opens,(size_t)front_closes);
    dispose_host_fixture();
  }
#ifdef HT_INTERNAL_AUTO
  for(UINTN source_fault=0;source_fault<4;source_fault++){
    front_setup(FA_OK);
    if(source_fault==0)front_usb_override=TRUE;
    if(source_fault==1)front_esp_path.Hd.Signature[0]^=1;
    if(source_fault==2)front_esp_path.Hd.MBRType=1;
    if(source_fault==3)front_esp_path.End.SubType=END_INSTANCE_DEVICE_PATH_SUBTYPE;
    CHECK(CoreInitialize((EFI_HANDLE)0x1234,(EFI_HANDLE)0x99)!=EFI_SUCCESS && !CoreInitialized && !front_opens && !Gu.started);
    dispose_host_fixture();
  }
  puts("PASS: 4 internal source-media rejection scenarios");
#endif
  for(UINTN i=0;i<5;i++)free((VOID *)fixture_assets[i].firmware.data);
  munmap(front_mapping,0x5000);
  puts("PASS: 15 full-adapter cases with real core, captured AMI code, exact assets and RAM-only EFI");
  return 0;
}
