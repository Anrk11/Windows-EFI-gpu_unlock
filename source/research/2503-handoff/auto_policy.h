/* GPL-2.0-only. File gate: an unconfirmed run never repeats automatically. */
#pragma once
#include <Uefi.h>
#include <Protocol/SimpleFileSystem.h>
#define AUTO_LOG_PATH L"\\EFI\\Codex2503\\LASTRUN.TXT"
#define AUTO_PENDING_PATH L"\\EFI\\Codex2503\\PENDING.TXT"
#define AUTO_DISABLED_PATH L"\\EFI\\Codex2503\\DISABLED.TXT"
#define AUTO_PENDING_CONTENT "2503-AUTO-PENDING-v11\r\n"
static BOOLEAN AutoPendingSpent;
static BOOLEAN AutoSelect(EFI_SYSTEM_TABLE *table,EFI_BOOT_SERVICES *bs){
  if(!table || !table->ConIn || !table->ConIn->ReadKeyStroke || !bs || !bs->Stall)return FALSE;
  for(UINTN i=0;i<50;i++){
    EFI_INPUT_KEY key;EFI_STATUS s=table->ConIn->ReadKeyStroke(table->ConIn,&key);
    if(s!=EFI_NOT_READY)return FALSE;
    if(bs->Stall(100000)!=EFI_SUCCESS)return FALSE;
  }
  return TRUE;
}
static BOOLEAN AutoCanStart(EFI_FILE_PROTOCOL *root){
  if(!root || !root->Open || AutoPendingSpent)return FALSE;
  const CHAR16 *paths[]={AUTO_DISABLED_PATH,AUTO_PENDING_PATH,AUTO_LOG_PATH};
  for(UINTN i=0;i<3;i++){
    EFI_FILE_PROTOCOL *file=NULL;
    EFI_STATUS s=root->Open(root,&file,(CHAR16 *)paths[i],EFI_FILE_MODE_READ,0);
    if(s==EFI_NOT_FOUND && !file)continue;
    if(file && file->Close)file->Close(file);
    return FALSE;
  }
  return TRUE;
}
static EFI_STATUS AutoMarkPending(EFI_FILE_PROTOCOL *root){
  if(!root || !root->Open || AutoPendingSpent)return EFI_ACCESS_DENIED;
  AutoPendingSpent=TRUE;
  EFI_FILE_PROTOCOL *file=NULL;
  EFI_STATUS s=root->Open(root,&file,AUTO_PENDING_PATH,EFI_FILE_MODE_READ,0);
  if(s!=EFI_NOT_FOUND || file){if(file && file->Close)file->Close(file);return EFI_ACCESS_DENIED;}
  s=root->Open(root,&file,AUTO_PENDING_PATH,EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,0);
  if(s!=EFI_SUCCESS || !file)return s==EFI_SUCCESS?EFI_DEVICE_ERROR:s;
  if(!file->Write || !file->Flush || !file->Close){if(file->Close)file->Close(file);return EFI_UNSUPPORTED;}
  static const char marker[]=AUTO_PENDING_CONTENT;
  UINTN bytes=sizeof(marker)-1,written=bytes;
  s=file->Write(file,&written,(VOID *)marker);
  if(s==EFI_SUCCESS && written!=bytes)s=EFI_DEVICE_ERROR;
  if(s==EFI_SUCCESS)s=file->Flush(file);
  EFI_STATUS closed=file->Close(file);if(s==EFI_SUCCESS)s=closed;
  return s;
}
