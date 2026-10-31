/* RAM-only exact-partition Windows loader tests; never starts an OS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/LoadedImage.h>
#include "windows.h"
static EFI_BOOT_SERVICES boot;
static EFI_LOADED_IMAGE_PROTOCOL loaded;
static struct {HARDDRIVE_DEVICE_PATH Hd;EFI_DEVICE_PATH_PROTOCOL End;} path;
static int fault,loads,unloads,allocations;
static void check(int ok,const char *what){if(!ok){fprintf(stderr,"FAIL Windows loader %d %s\n",fault,what);exit(1);}}
static EFI_STATUS EFIAPI alloc(EFI_MEMORY_TYPE type,UINTN n,VOID **p){(void)type;*p=malloc(n);allocations++;return *p?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;}
static EFI_STATUS EFIAPI release(VOID *p){free(p);allocations--;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI locate(EFI_LOCATE_SEARCH_TYPE type,EFI_GUID *g,VOID *key,UINTN *n,EFI_HANDLE **h){
  (void)g;check(type==ByProtocol && !key,"filesystem search");*n=fault==2?2:1;
  EFI_STATUS s=alloc(EfiBootServicesData,*n*sizeof(**h),(VOID **)h);if(s!=EFI_SUCCESS)return s;
  (*h)[0]=(VOID *)11;if(*n==2)(*h)[1]=(VOID *)12;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI protocol(EFI_HANDLE h,EFI_GUID *g,VOID **out){
  (void)g;if(h==(VOID *)11 || h==(VOID *)12){*out=&path;return EFI_SUCCESS;}
  if(h==(VOID *)99){*out=&loaded;return EFI_SUCCESS;}return EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI load(BOOLEAN policy,EFI_HANDLE parent,EFI_DEVICE_PATH_PROTOCOL *p,VOID *source,UINTN bytes,EFI_HANDLE *image){
  check(!policy && parent==(VOID *)1 && !source && !bytes,"load existing Windows file only");
  check(GuPathIsPartition(p,&GuWindowsPartition),"pinned partition in load path");
  const CHAR16 expected[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
  const UINT8 *node=(const UINT8 *)p+sizeof(HARDDRIVE_DEVICE_PATH);
  check(node[0]==MEDIA_DEVICE_PATH && node[1]==MEDIA_FILEPATH_DP && !memcmp(node+4,expected,sizeof(expected)),"exact Windows boot filename");
  loads++;if(fault==5)return EFI_LOAD_ERROR;*image=(VOID *)99;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI unload(EFI_HANDLE h){check(h==(VOID *)99,"unload wrong-device image");unloads++;return EFI_SUCCESS;}
int main(void){
  boot.LocateHandleBuffer=locate;boot.HandleProtocol=protocol;boot.AllocatePool=alloc;boot.FreePool=release;boot.LoadImage=load;boot.UnloadImage=unload;
  for(fault=0;fault<7;fault++){
    memset(&path,0,sizeof(path));loads=unloads=allocations=0;
    path.Hd.Header.Type=MEDIA_DEVICE_PATH;path.Hd.Header.SubType=MEDIA_HARDDRIVE_DP;path.Hd.Header.Length[0]=sizeof(HARDDRIVE_DEVICE_PATH);
    path.Hd.PartitionSize=1;path.Hd.MBRType=2;path.Hd.SignatureType=2;memcpy(path.Hd.Signature,&GuWindowsPartition,16);
    path.End.Type=END_DEVICE_PATH_TYPE;path.End.SubType=END_ENTIRE_DEVICE_PATH_SUBTYPE;path.End.Length[0]=4;
    loaded.DeviceHandle=fault==4?(VOID *)44:(VOID *)11;
    if(fault==1)path.Hd.Signature[0]^=1;
    if(fault==6)path.End.SubType=END_INSTANCE_DEVICE_PATH_SUBTYPE;
    EFI_HANDLE image=NULL;
#ifdef HT_INTERNAL_AUTO
    EFI_HANDLE source=fault==3?(VOID *)44:(VOID *)11;
#else
    EFI_HANDLE source=fault==3?(VOID *)11:(VOID *)44;
#endif
    EFI_STATUS s=PrepareWindows(&boot,(VOID *)1,source,&image);
    if(!fault)check(s==EFI_SUCCESS && image==(VOID *)99 && loads==1,"correct Windows image prepared");
    else check(s!=EFI_SUCCESS,"bad Windows path refused");
    if(fault==4)check(unloads==1 && !image,"wrong-device image discarded");
    if(fault==1 || fault==2 || fault==3 || fault==6)check(!loads,"reject before image load");
    check(!allocations,"temporary allocations freed");
  }
  puts("PASS: 7 Windows loader cases, exact partition/file, duplicate/invalid paths and wrong image device");return 0;
}
