/* GPL-2.0-only. Core lifecycle shared by the simulator and future EFI adapter.
 * Callers must separately pin firmware ownership, code, PROM and assets. */
#pragma once
#include "../../internal-boot/unlock/compat/compat.h"
typedef enum {UB_NOT_RUN,UB_RECOVER_DISPLAY,UB_CORE_COMPLETE,UB_BOOT_WITHOUT_FIRMWARE_GOP,UB_POWER_OFF} UB_OUTCOME;
typedef struct {
  VOID *User;
  BOOLEAN (*Guard)(VOID *,BOOLEAN AfterCore);
  BOOLEAN Used;
  BOOLEAN WindowsAttempted,WindowsPrepared;
  EFI_PCI_IO_PROTOCOL *Pci;
  int CoreResult;
  UB_OUTCOME Outcome;
  /* Keep the context alive until power-off on UB_POWER_OFF: retained core
   * work owns a device reference, including this DMA mask pointer. */
  UINT64 DmaMask;
  struct pci_dev Device;
} UB_CONTEXT;
EFI_STATUS UbRun(UB_CONTEXT *Context);
/* Separate commit after core cleanup: retain the original 2503 VGA forwarding
 * without reconnecting GOP. Guard must accept the expected post-core state
 * both before and after this operation. Failure after execution requires halt.
 * The caller must independently recheck after final file closes; this
 * function itself is one-shot and must never be retried. */
EFI_STATUS UbPrepareWindows(UB_CONTEXT *Context,UINT64 EntryAttributes);
