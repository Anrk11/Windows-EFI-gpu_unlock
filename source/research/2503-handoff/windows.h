/* GPL-2.0-only. Load the existing, exact-partition Windows Boot Manager.
 * No BootOrder/BootNext changes, reset, filesystem writes or repair commands. */
#pragma once
#include "../../internal-boot/boot/media.h"
static EFI_STATUS PrepareWindows(EFI_BOOT_SERVICES *bs,EFI_HANDLE self,EFI_HANDLE usb,EFI_HANDLE *image) {
  *image=NULL;EFI_GUID fsGuid=EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID,pathGuid=EFI_DEVICE_PATH_PROTOCOL_GUID,loadedGuid=EFI_LOADED_IMAGE_PROTOCOL_GUID;
  EFI_HANDLE *handles=NULL,partition=NULL;UINTN count=0,matches=0;
  EFI_STATUS s=bs->LocateHandleBuffer(ByProtocol,&fsGuid,NULL,&count,&handles);if(s!=EFI_SUCCESS)return s;
  if(count>4096){if(handles)bs->FreePool(handles);return EFI_UNSUPPORTED;}
  for(UINTN i=0;i<count;i++){
    EFI_DEVICE_PATH_PROTOCOL *p=NULL;
    if(bs->HandleProtocol(handles[i],&pathGuid,(VOID **)&p)==EFI_SUCCESS && GuPathIsPartition(p,&GuWindowsPartition)){partition=handles[i];matches++;}
  }
  if(handles && bs->FreePool(handles)!=EFI_SUCCESS)return EFI_DEVICE_ERROR;
  if(matches!=1)return EFI_UNSUPPORTED;
#ifdef HT_INTERNAL_AUTO
  if(partition!=usb)return EFI_UNSUPPORTED;
#else
  if(partition==usb)return EFI_UNSUPPORTED;
#endif
  EFI_DEVICE_PATH_PROTOCOL *base=NULL;UINTN bytes=0;
  if(bs->HandleProtocol(partition,&pathGuid,(VOID **)&base)!=EFI_SUCCESS || !GuInspectPath(base,&GuWindowsPartition,&bytes,NULL,NULL))return EFI_UNSUPPORTED;
  const CHAR16 file[]=L"\\EFI\\Microsoft\\Boot\\bootmgfw.efi";
  UINTN nodeBytes=4+sizeof(file),total=bytes-4+nodeBytes+4;UINT8 *path=NULL;
  s=bs->AllocatePool(EfiBootServicesData,total,(VOID **)&path);if(s!=EFI_SUCCESS || !path)return EFI_OUT_OF_RESOURCES;
  for(UINTN i=0;i<bytes-4;i++)path[i]=((UINT8 *)base)[i];
  UINT8 *node=path+bytes-4;node[0]=MEDIA_DEVICE_PATH;node[1]=MEDIA_FILEPATH_DP;node[2]=(UINT8)nodeBytes;node[3]=(UINT8)(nodeBytes>>8);
  for(UINTN i=0;i<sizeof(file);i++)node[4+i]=((const UINT8 *)file)[i];
  node+=nodeBytes;node[0]=END_DEVICE_PATH_TYPE;node[1]=END_ENTIRE_DEVICE_PATH_SUBTYPE;node[2]=4;node[3]=0;
  s=bs->LoadImage(FALSE,self,(VOID *)path,NULL,0,image);
  EFI_STATUS freed=bs->FreePool(path);if(s!=EFI_SUCCESS)return s;if(freed!=EFI_SUCCESS)return freed;
  EFI_LOADED_IMAGE_PROTOCOL *loaded=NULL;
  s=bs->HandleProtocol(*image,&loadedGuid,(VOID **)&loaded);
  if(s!=EFI_SUCCESS || !loaded || loaded->DeviceHandle!=partition){if(*image)bs->UnloadImage(*image);*image=NULL;return EFI_UNSUPPORTED;}
  return EFI_SUCCESS;
}
