/* Pure host-visible zero buffers, never populated with an unlock payload. */
#include "dma_probe.h"
static const UINTN Sizes[]={84226048,43*4096,24576,0xe700,64000,4096};
/* Mapping is an opaque firmware cookie, not a required allocation pointer.
 * Always return exactly the cookie produced by a successful Map to Unmap. */
static EFI_STATUS Record(DP_CONTEXT *c,const char *key,UINT64 value,EFI_STATUS prior){
  EFI_STATUS logged=c->Log(c->User,key,value);
  return prior!=EFI_SUCCESS?prior:logged;
}
EFI_STATUS DpRun(DP_CONTEXT *c){
  if(!c || c->Used || !c->Io || !c->Guard || !c->Log || !c->Io->AllocateBuffer || !c->Io->Map || !c->Io->Unmap || !c->Io->FreeBuffer)return EFI_INVALID_PARAMETER;
  c->Used=TRUE;EFI_STATUS result=EFI_SUCCESS;
  for(UINTN i=0;i<6;i++){
    DP_BUFFER *b=&c->Buffers[i];b->Pages=(Sizes[i]+4095)/4096;b->Bytes=b->Pages*4096;
    result=c->Log(c->User,"dma-buffer-intent-index",i);if(result!=EFI_SUCCESS)break;
    result=c->Log(c->User,"dma-buffer-requested-bytes",b->Bytes);if(result!=EFI_SUCCESS)break;
    if(!c->Guard(c->User)){result=EFI_NOT_READY;break;}
    result=c->Io->AllocateBuffer(c->Io,AllocateAnyPages,EfiBootServicesData,b->Pages,&b->Cpu,0);
    result=Record(c,"dma-allocate-return",result,result);
    if(result!=EFI_SUCCESS || !b->Cpu){if(result==EFI_SUCCESS)result=EFI_OUT_OF_RESOURCES;break;}
    result=c->Log(c->User,"dma-buffer-cpu-address",(UINTN)b->Cpu);if(result!=EFI_SUCCESS)break;
    for(UINTN k=0;k<b->Bytes;k++)((UINT8 *)b->Cpu)[k]=0;
    if(!c->Guard(c->User)){result=EFI_NOT_READY;break;}
    UINTN bytes=b->Bytes;
    result=c->Io->Map(c->Io,EfiPciIoOperationBusMasterCommonBuffer,b->Cpu,&bytes,&b->Bus,&b->Mapping);
    if(result==EFI_SUCCESS)b->Mapped=TRUE;
    result=Record(c,"dma-map-return",result,result);
    if(result!=EFI_SUCCESS)break;
    result=c->Log(c->User,"dma-buffer-mapped-bytes",bytes);if(result!=EFI_SUCCESS)break;
    result=c->Log(c->User,"dma-buffer-bus-address",b->Bus);if(result!=EFI_SUCCESS)break;
    result=c->Log(c->User,"dma-buffer-mapping-cookie",(UINTN)b->Mapping);if(result!=EFI_SUCCESS)break;
    /* Keep all geometry checks. A NULL cookie alone says nothing about whether
     * a mapping succeeded; status, byte count and bus range decide that. */
    if(bytes!=b->Bytes || !b->Bus || (b->Bus&4095) || b->Bus>0xffffffffULL || b->Bytes-1>0xffffffffULL-b->Bus){result=EFI_UNSUPPORTED;result=Record(c,"dma-geometry-rejected",1,result);break;}
    if(!c->Guard(c->User)){result=EFI_NOT_READY;break;}
  }
  /* Even an allocation/map error can leave resources to release. No retries;
   * a failed guard or cleanup retains the remainder and forbids OS return. */
  for(UINTN i=6;i>0;i--){
    DP_BUFFER *b=&c->Buffers[i-1];if(!b->Cpu)continue;
    if(!c->Guard(c->User)){c->Retained=TRUE;return EFI_ACCESS_DENIED;}
    result=Record(c,"dma-cleanup-index",i-1,result);
    if(!c->Guard(c->User)){c->Retained=TRUE;return EFI_ACCESS_DENIED;}
    if(b->Mapped){EFI_STATUS s=c->Io->Unmap(c->Io,b->Mapping);result=Record(c,"dma-unmap-return",s,result);if(s!=EFI_SUCCESS){c->Retained=TRUE;return s;}b->Mapped=FALSE;}
    /* Recheck after Unmap and logging, before releasing the CPU pages. */
    if(!c->Guard(c->User)){c->Retained=TRUE;return EFI_ACCESS_DENIED;}
    EFI_STATUS s=c->Io->FreeBuffer(c->Io,b->Pages,b->Cpu);result=Record(c,"dma-free-return",s,result);if(s!=EFI_SUCCESS){c->Retained=TRUE;return s;}b->Cpu=NULL;
  }
  if(result==EFI_SUCCESS)result=c->Log(c->User,"dma-dry-run-cleanup-complete",1);
  return result;
}
