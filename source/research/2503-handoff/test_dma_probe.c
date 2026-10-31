/* RAM-only injection of real mapping dry-run code. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "dma_probe.h"
static EFI_PCI_IO_PROTOCOL pci;
static VOID *buffers[6];static int mode,allocated,mapped,unmapped,freed,guards;
static void check(int ok,const char *s){if(!ok){fprintf(stderr,"dry-run mode %d %s\n",mode,s);exit(1);}}
static BOOLEAN guard(VOID *u){(void)u;guards++;return !((mode==7 && guards>=3) || (mode==17 && unmapped));}
static EFI_STATUS log_event(VOID *u,const char *s,UINT64 v){
  (void)u;(void)v;
  return ((mode==8 && !strcmp(s,"dma-buffer-intent-index")) ||
    (mode==14 && !strcmp(s,"dma-allocate-return")) ||
    (mode==15 && !strcmp(s,"dma-map-return")) ||
    (mode==16 && !strcmp(s,"dma-cleanup-index")))?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI allocate(EFI_PCI_IO_PROTOCOL *p,EFI_ALLOCATE_TYPE t,EFI_MEMORY_TYPE m,UINTN n,VOID **out,UINT64 attrs){
  (void)p;check(t==AllocateAnyPages && m==EfiBootServicesData && !attrs,"allocation contract");
  if(mode==1)return EFI_OUT_OF_RESOURCES;
  if(posix_memalign(out,4096,n*4096))return EFI_OUT_OF_RESOURCES;
  buffers[allocated++]=*out;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI map(EFI_PCI_IO_PROTOCOL *p,EFI_PCI_IO_PROTOCOL_OPERATION op,VOID *cpu,UINTN *n,EFI_PHYSICAL_ADDRESS *bus,VOID **token){
  (void)p;check(op==EfiPciIoOperationBusMasterCommonBuffer && cpu && *n,"common-buffer mapping only");mapped++;
  if(mode==2)return EFI_DEVICE_ERROR;
  *bus=0x10000000ULL*mapped;*token=cpu;
  if(mode==3)*n-=4096;
  if(mode==4)*bus=0x100000000ULL;
  if(mode==9)(*bus)++;
  if(mode==10)*bus=0;
  if(mode==11)*bus=0xfffff000ULL;
  if(mode==12 || mode==13 || mode==18)*token=NULL;
  if(mode==18)*n-=4096;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI unmap(EFI_PCI_IO_PROTOCOL *p,VOID *token){(void)p;check((mode==12 || mode==13 || mode==18)?token==NULL:token!=NULL,"exact opaque mapping cookie returned");unmapped++;return mode==5 || mode==13?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI release(EFI_PCI_IO_PROTOCOL *p,UINTN n,VOID *cpu){(void)p;check(n && cpu,"free buffer identity");freed++;if(mode==6)return EFI_DEVICE_ERROR;for(int i=0;i<allocated;i++)if(buffers[i]==cpu)buffers[i]=NULL;free(cpu);return EFI_SUCCESS;}
int main(void){
  pci.AllocateBuffer=allocate;pci.Map=map;pci.Unmap=unmap;pci.FreeBuffer=release;
  for(mode=0;mode<19;mode++){
    allocated=mapped=unmapped=freed=guards=0;memset(buffers,0,sizeof(buffers));
    DP_CONTEXT c={0};c.Io=&pci;c.Guard=guard;c.Log=log_event;EFI_STATUS s=DpRun(&c);
    if(!mode || mode==12)check(s==EFI_SUCCESS && !c.Retained && mapped==6 && unmapped==6 && freed==6,"all six buffers cleaned, including NULL cookies");
    else check(s!=EFI_SUCCESS,"fault must not pass");
    if(mode==5 || mode==6 || mode==7 || mode==13 || mode==17)check(c.Retained,"cleanup/guard failure prevents OS return");
    else check(!c.Retained,"recoverable error cleaned");
    if(mode==8)check(!allocated && !mapped,"no allocation without durable intent");
    if(mode==14)check(allocated==1 && !mapped && freed==1,"allocation retained until freed after logging failure");
    if(mode==15)check(mapped==1 && unmapped==1 && freed==1,"successful mapping cleaned after status log fails");
    if(mode==16)check(mapped==6 && unmapped==6 && freed==6,"cleanup logging failure still cleans all buffers");
    if(mode==17)check(unmapped==1 && !freed,"state drift after unmap prevents free");
    if(mode==18)check(mapped==1 && unmapped==1 && freed==1,"NULL cookie does not bypass short mapping rejection");
    int old=mapped;check(DpRun(&c)==EFI_INVALID_PARAMETER && mapped==old,"no repeat attempt");
    /* Discard test-process RAM only; production retains on failure. */
    for(int i=0;i<allocated;i++)free(buffers[i]);
  }
  puts("PASS: 19 mapping dry-run scenarios, opaque cookies, exact geometry and cleanup");return 0;
}
