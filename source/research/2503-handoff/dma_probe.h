/* GPL-2.0-only. Mapping-only dry run. Never enables BME or submits firmware. */
#pragma once
#include <Uefi.h>
#include <Protocol/PciIo.h>
typedef struct {VOID *Cpu,*Mapping;UINT64 Bus;UINTN Pages,Bytes;BOOLEAN Mapped;} DP_BUFFER;
typedef struct {
  EFI_PCI_IO_PROTOCOL *Io;
  VOID *User;
  BOOLEAN (*Guard)(VOID *);
  EFI_STATUS (*Log)(VOID *,const char *,UINT64);
  BOOLEAN Used,Retained;
  DP_BUFFER Buffers[6];
} DP_CONTEXT;
EFI_STATUS DpRun(DP_CONTEXT *Context);
