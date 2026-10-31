/* RAM-only activation, file failures and durable one-attempt policy tests. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "auto_policy.h"
static EFI_FILE_PROTOCOL root,file;
static EFI_BOOT_SERVICES bs;
static EFI_SYSTEM_TABLE table;
static EFI_SIMPLE_TEXT_INPUT_PROTOCOL input;
static int mode,input_mode,reads,stalls,creates,writes,closes;
static void check(int ok,const char *what){if(!ok){fprintf(stderr,"FAIL auto mode=%d input=%d: %s\n",mode,input_mode,what);exit(1);}}
static int same(const CHAR16 *a,const CHAR16 *b){while(*a && *a==*b){a++;b++;}return *a==*b;}
static EFI_STATUS EFIAPI close_file(EFI_FILE_PROTOCOL *f){check(f==&file,"close correct file");closes++;return mode==10?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI flush_file(EFI_FILE_PROTOCOL *f){check(f==&file,"flush correct file");return mode==9?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI write_file(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data){
  check(f==&file && *n==sizeof(AUTO_PENDING_CONTENT)-1 && !memcmp(data,AUTO_PENDING_CONTENT,*n),"fixed pending marker");
  writes++;if(mode==7)return EFI_DEVICE_ERROR;if(mode==8)(*n)--;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI open_file(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **out,CHAR16 *path,UINT64 access,UINT64 attr){
  check(r==&root && !attr,"own root only");*out=NULL;
  if(access==EFI_FILE_MODE_READ){
    if(mode==4)return EFI_DEVICE_ERROR;
    if(mode==5){*out=&file;return EFI_NOT_FOUND;}
    if((mode==1 && same(path,AUTO_DISABLED_PATH)) || ((mode==2 || mode==12) && same(path,AUTO_PENDING_PATH)) ||
       (mode==3 && same(path,AUTO_LOG_PATH))){*out=&file;return EFI_SUCCESS;}
    return EFI_NOT_FOUND;
  }
  check(access==(EFI_FILE_MODE_CREATE|EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE) && same(path,AUTO_PENDING_PATH),"only create own marker");
  creates++;if(mode==6)return EFI_OUT_OF_RESOURCES;*out=&file;return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI key(EFI_SIMPLE_TEXT_INPUT_PROTOCOL *p,EFI_INPUT_KEY *k){
  check(p==&input,"correct input");reads++;
  if(input_mode==3)return EFI_DEVICE_ERROR;
  if(input_mode==1 || input_mode==2 || (input_mode==4 && reads==50)){
    k->ScanCode=input_mode==1?SCAN_ESC:0;k->UnicodeChar=input_mode==2?'x':0;return EFI_SUCCESS;
  }
  return EFI_NOT_READY;
}
static EFI_STATUS EFIAPI stall(UINTN us){check(us==100000,"bounded skip interval");stalls++;return input_mode==5?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static void setup(void){
  root=(EFI_FILE_PROTOCOL){0};file=(EFI_FILE_PROTOCOL){0};AutoPendingSpent=FALSE;
  root.Open=open_file;file.Close=close_file;file.Write=write_file;file.Flush=flush_file;
  reads=stalls=creates=writes=closes=0;
}
int main(void){
  unsigned cases=0;
  for(mode=0;mode<=5;mode++){setup();check(AutoCanStart(&root)==(mode==0),"disabled, pending, old log and unreadable state skip");check(!creates && !writes,"admission is read only");cases++;}
  for(mode=0;mode<=12;mode++){if(mode>0 && mode<6)continue;setup();if(mode==11)file.Flush=NULL;
    EFI_STATUS s=AutoMarkPending(&root);check((s==EFI_SUCCESS)==(mode==0),"failed or partial marker cannot arm");
    int prior=creates;check(AutoMarkPending(&root)==EFI_ACCESS_DENIED && creates==prior,"no marker retry");
    check(!AutoCanStart(&root),"spent attempt is not rearmed");cases++;
  }
  input.ReadKeyStroke=key;bs.Stall=stall;table.ConIn=&input;
  for(input_mode=0;input_mode<=6;input_mode++){mode=0;setup();table.ConIn=input_mode==6?NULL:&input;
    check(AutoSelect(&table,&bs)==(input_mode==0),"timeout selects; keys and input/stall failures skip");
    if(!input_mode)check(reads==50 && stalls==50,"five-second skip window");cases++;
  }
  printf("PASS: %u automatic activation and pending-state scenarios; no hardware accessed\n",cases);
  return 0;
}
