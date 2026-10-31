/* GPL-2.0-only. Actual captured inputs + upstream pure preparation functions.
 * Ordinary process RAM only: no EFI services, device files, MMIO or GPU access. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prep_610.h"
#include "vbios_610.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"offline check failed at line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned char *read_file(const char *path,size_t *size) {
  FILE *f=fopen(path,"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long n=ftell(f);
  CHECK(n>0 && n<=128L*1024*1024);CHECK(!fseek(f,0,SEEK_SET));
  unsigned char *p=malloc((size_t)n);CHECK(p);CHECK(fread(p,1,(size_t)n,f)==(size_t)n);CHECK(!fclose(f));*size=(size_t)n;return p;
}
static unsigned char *asset(const char *dir,const char *name,size_t *size){char p[4096];CHECK(snprintf(p,sizeof(p),"%s/%s",dir,name)>0);return read_file(p,size);}
static u32 r32(const u8 *p){return (u32)p[0]|(u32)p[1]<<8|(u32)p[2]<<16|(u32)p[3]<<24;}
static void snapshot(const char *path,u32 regs[16]) {
  const u32 offsets[]={0x1183a4,0x625f04,0x1fa7c8,0x1fa82c,0x1fa830,0,0x118128,0x118234,0x110100,0x1fa824,0x1fa828,0x823804,0x82381c,0x823820,0x824148,0x8241e0};
  FILE *f=fopen(path,"r");CHECK(f);char line[256];unsigned long long off=0,value=0;u32 found=0;int pending=0;
  while(fgets(line,sizeof(line),f)){
    if(!strncmp(line,"entry-ss0=",10))break;
    if(sscanf(line,"layout-register-offset=0x%llx",&off)==1)pending=1;
    else if(pending && sscanf(line,"layout-register-value=0x%llx",&value)==1){
      CHECK(value<=0xffffffffULL);
      for(unsigned i=0;i<16;i++)if(off==offsets[i]){CHECK(!(found&(1U<<i)));regs[i]=(u32)value;found|=1U<<i;}
      pending=0;
    }
  }
  CHECK(found==0xffff);rewind(f);int results=0;
  while(fgets(line,sizeof(line),f))if(sscanf(line,"test-result=0x%llx",&value)==1){CHECK(value==0);results++;}
  CHECK(results==1);CHECK(!fclose(f));CHECK(regs[0]==0x3000 && regs[5]==0xb76000a1 && regs[10]==0 && regs[11]==0xffffff8f);
}
int main(int argc,char **argv){
  CHECK(argc==4);u32 regs[16]={0};snapshot(argv[3],regs);
  size_t gsp_size,desc_size,boot_size,booter_size,prom_size,payload_size;
  u8 *gsp=asset(argv[1],"gsp_ga10x.bin",&gsp_size),*desc=asset(argv[1],"gsp_boot_desc_ga102.bin",&desc_size);
  u8 *boot=asset(argv[1],"gsp_boot_ga102.bin",&boot_size),*booter=asset(argv[1],"booter_load_ga102.bin",&booter_size);
  u8 *payload=asset(argv[1],"v67-ga102-610.43.03.bin",&payload_size),*prom=read_file(argv[2],&prom_size);
  CHECK(payload_size==GU_V67_SIZE && prom_size==0x100000);
  struct gpu_unlock_span image_span,signature_span;
  CHECK(!gpu_unlock_elf_section(gsp,gsp_size,".fwimage",&image_span));
  CHECK(!gpu_unlock_elf_section(gsp,gsp_size,".fwsignature_ga10x",&signature_span));CHECK(signature_span.size==4096);
  struct gpu_unlock_boot_desc bd;CHECK(!gpu_unlock_boot_desc_parse(desc,desc_size,(u32)boot_size,&bd));
  CHECK(booter_size>72 && r32(booter)==0x314c5442 && r32(booter+4)==1);
  struct gpu_unlock_booter_hdr bh;
  _Static_assert(sizeof(bh)==56,"14 DWORD Booter header");
  u32 fields[14];for(unsigned i=0;i<14;i++)fields[i]=r32(booter+8+i*4);memcpy(&bh,fields,sizeof(bh));
  u32 image_bytes=r32(booter+64),signature_bytes=r32(booter+68);
  CHECK((u64)72+image_bytes+signature_bytes==booter_size);
  CHECK(!gpu_unlock_booter_validate(GU_BOOTER_GA10X,&bh,image_bytes,signature_bytes));
  CHECK(gpu_unlock_booter_geometry_match(&bh,image_bytes,signature_bytes));
  CHECK(!gpu_unlock_booter_patch(booter+72,image_bytes,&bh,booter+72+image_bytes,signature_bytes,gpu_unlock_fuse_version(regs[14])));
  struct gpu_unlock_layout_in in={0};in.usable_fb=(u64)regs[0]<<20;in.vga_base=in.usable_fb-(1ULL<<20);
  if(regs[1]&8){in.vga_base=(u64)(regs[1]>>8)<<16;if(in.vga_base<in.usable_fb-(1ULL<<20))in.vga_base=in.usable_fb-0x20000;}
  in.mmu_lock_low=(u64)(regs[3]>>4)<<12;u64 high=(u64)(regs[4]>>4)<<12;
  in.mmu_lock_valid=high>=in.mmu_lock_low && (((high+0xffff)&~0xffffULL)>>20)==(in.usable_fb>>20);
  CHECK(regs[2]&1);in.frts_size=1ULL<<20;in.boot_size=bd.image_size;in.image_size=image_span.size;in.nonwpr_size=1ULL<<20;in.heap_request=88ULL<<20;
  struct gpu_unlock_layout_out out;CHECK(!gpu_unlock_layout(&in,&out));CHECK(out.nonwpr_start>=in.usable_fb-(256ULL<<20));
  struct gpu_unlock_radix_plan radix;CHECK(!gpu_unlock_radix_plan(image_span.size,&radix));
  struct gpu_unlock_fwsec fw;CHECK(!gpu_unlock_vbios_fwsec(prom,prom_size,&fw));
  CHECK(!gpu_unlock_fwsec_patch_frts(prom+fw.image_off,&fw,out.frts_offset));
  printf("{\n  \"offline_preparation_passed\": true,\n  \"hardware_executed\": false,\n  \"unlock_approved\": false,\n");
  printf("  \"fwimage_bytes\": %llu,\n  \"boot_bytes\": %u,\n  \"radix_table_pages\": %llu,\n",(unsigned long long)image_span.size,bd.image_size,(unsigned long long)radix.table_pages);
  printf("  \"nonwpr_start\": \"0x%llx\",\n  \"wpr_start\": \"0x%llx\",\n  \"heap_start\": \"0x%llx\",\n",(unsigned long long)out.nonwpr_start,(unsigned long long)out.wpr_start,(unsigned long long)out.heap_offset);
  printf("  \"gsp_start\": \"0x%llx\",\n  \"boot_start\": \"0x%llx\",\n  \"frts_start\": \"0x%llx\",\n  \"wpr_end\": \"0x%llx\"\n}\n",(unsigned long long)out.gsp_offset,(unsigned long long)out.boot_offset,(unsigned long long)out.frts_offset,(unsigned long long)out.wpr_end);
  free(gsp);free(desc);free(boot);free(booter);free(payload);free(prom);return 0;
}
