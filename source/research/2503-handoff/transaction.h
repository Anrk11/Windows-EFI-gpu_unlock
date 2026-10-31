/* GPL-2.0-only. Experimental display handoff test; no unlock transaction. */
#pragma once
#include <Uefi.h>
typedef struct {
  UINT32 Config[64], Registers[16];
  UINT64 Attributes;
  UINTN Owner, Children, Gops;
} HT_STATE;
typedef struct {
  VOID *Context;
  EFI_STATUS (*Read)(VOID *,HT_STATE *);
  EFI_STATUS (*Log)(VOID *,const char *,UINT64);
  EFI_STATUS (*Disconnect)(VOID *);
  EFI_STATUS (*Reconnect)(VOID *);
  EFI_STATUS (*InspectReleased)(VOID *,const HT_STATE *Entry);
  EFI_STATUS (*PrepareWindowsWithoutGop)(VOID *,const HT_STATE *Entry);
  BOOLEAN (*MayRecoverDisplay)(VOID *);
  BOOLEAN Used, Attempted, Restored, Released, HeadlessReady;
} HT_TEST;
EFI_STATUS HtRun(HT_TEST *Test);
