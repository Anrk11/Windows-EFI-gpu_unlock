/* Actual UEFI DMA allocator, RAM-only mock services. Machine identity checks
 * are intentionally outside this isolated test; this is not a 2503 approval. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "../../internal-boot/unlock/port.h"
static EFI_BOOT_SERVICES boot;
static EFI_PCI_IO_PROTOCOL pci;
static VOID *buffer,*pool;
static int mode,allocations,maps,unmaps,frees;
enum {OK,ALLOC_FAIL,MAP_FAIL,SHORT_MAP,HIGH_MAP,UNALIGNED_MAP,ZERO_MAP,CROSS_4G,UNMAP_FAIL,FREE_FAIL,PRE_RETAINED,RUNNING,FAULTED,MASTER_PENDING,COUNT};
static void check(int ok,const char *what){if(!ok){fprintf(stderr,"DMA mode %d failed: %s\n",mode,what);exit(1);}}
VOID GuLog(const char *format,...){(void)format;}
static EFI_STATUS EFIAPI alloc_pool(EFI_MEMORY_TYPE type,UINTN size,VOID **out){(void)type;check(!pool,"one allocator record");*out=pool=calloc(1,size);return pool?EFI_SUCCESS:EFI_OUT_OF_RESOURCES;}
static EFI_STATUS EFIAPI free_pool(VOID *p){check(p==pool,"free allocator record");free(pool);pool=NULL;return EFI_SUCCESS;}
static EFI_STATUS EFIAPI alloc_buffer(EFI_PCI_IO_PROTOCOL *p,EFI_ALLOCATE_TYPE type,EFI_MEMORY_TYPE mem,UINTN pages,VOID **out,UINT64 attrs){
  check(p==&pci && type==AllocateAnyPages && mem==EfiBootServicesData && !attrs,"UEFI common buffer request");allocations++;
  if(mode==ALLOC_FAIL)return EFI_OUT_OF_RESOURCES;
  if(posix_memalign(&buffer,4096,pages*4096))return EFI_OUT_OF_RESOURCES;
  *out=buffer;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI map_buffer(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_OPERATION op,VOID *cpu,UINTN *bytes,EFI_PHYSICAL_ADDRESS *dma,VOID **mapping){
  check(p==&pci && op==EfiPciIoOperationBusMasterCommonBuffer && cpu==buffer && *bytes==8192,"exact map geometry");maps++;
  if(mode==MAP_FAIL)return EFI_DEVICE_ERROR;
  *dma=mode==HIGH_MAP?0x100000000ULL:mode==CROSS_4G?0xfffff000ULL:mode==ZERO_MAP?0:0x10000000;
  if(mode==UNALIGNED_MAP)(*dma)++;
  if(mode==SHORT_MAP)*bytes-=4096;
  *mapping=(VOID *)0x55;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI unmap_buffer(EFI_PCI_IO_PROTOCOL *p,VOID *mapping){check(p==&pci && mapping==(VOID *)0x55,"mapping token preserved");unmaps++;return mode==UNMAP_FAIL?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI free_buffer(EFI_PCI_IO_PROTOCOL *p,UINTN pages,VOID *cpu){check(p==&pci && pages==2 && cpu==buffer,"CPU buffer preserved");frees++;if(mode==FREE_FAIL)return EFI_DEVICE_ERROR;free(buffer);buffer=NULL;return EFI_SUCCESS;}
int main(void){
  boot.AllocatePool=alloc_pool;boot.FreePool=free_pool;
  pci.AllocateBuffer=alloc_buffer;pci.FreeBuffer=free_buffer;pci.Map=map_buffer;pci.Unmap=unmap_buffer;
  for(mode=0;mode<COUNT;mode++){
    allocations=maps=unmaps=frees=0;buffer=pool=NULL;GuInit(&boot,&pci);
    Gu.profile_verified=Gu.armed=Gu.owned=Gu.display_ok=TRUE;
    if(mode==PRE_RETAINED)Gu.retained=TRUE;
    VOID *cpu=(VOID *)1;UINT64 dma=1;EFI_STATUS s=GuDmaAllocate(8192,&cpu,&dma);
    if(mode==PRE_RETAINED)check(s!=EFI_SUCCESS && !cpu && !dma && !allocations && !maps,"retained transaction cannot allocate");
    else if(mode>=ALLOC_FAIL && mode<=CROSS_4G)check(s!=EFI_SUCCESS && !cpu && !dma && !buffer && !pool && !Gu.allocations,"bad mapping cleaned without execution");
    else {
      check(s==EFI_SUCCESS && cpu==buffer && dma==0x10000000 && dma!=(UINTN)cpu && Gu.dma_count==1,"device address distinct from CPU pointer");
      if(mode==RUNNING)Gu.started=TRUE;
      if(mode==FAULTED)Gu.started=Gu.quiesced=Gu.fault=TRUE;
      if(mode==MASTER_PENDING)Gu.master_attributes_pending=TRUE;
      s=GuDmaFree(cpu,dma);
      if(mode==OK)check(s==EFI_SUCCESS && !buffer && !pool && !Gu.allocations && !Gu.dma_count,"normal unmap/free");
      else {
        check(s!=EFI_SUCCESS && Gu.retained && Gu.allocations && !GuSafeToReturn(),"unsafe cleanup retains resources and forbids return");
        if(mode==RUNNING || mode==FAULTED || mode==MASTER_PENDING)check(!unmaps && !frees,"never free live DMA memory");
        if(mode==UNMAP_FAIL)check(unmaps==1 && !frees,"unmap error prevents freeing buffer");
        if(mode==FREE_FAIL)check(unmaps==1 && frees==1 && !Gu.allocations->mapped,"failed free retains record after unmap");
      }
    }
    /* Dispose only mock process memory after assertions; no production retry. */
    free(buffer);free(pool);buffer=pool=NULL;
  }
  printf("PASS: %d actual DMA-port scenarios; no device access, BME or firmware execution\n",COUNT);return 0;
}
