/* Actual returned-boot logger, RAM-backed files only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#define HT_HEADLESS_WINDOWS_TEST 1
#include "main.c"
static EFI_SIMPLE_FILE_SYSTEM_PROTOCOL fs;
static EFI_FILE_PROTOCOL root_mock,log_mock;
static int fault,opens,seeks,writes,flushes,log_closes,root_closes;
static char text[160],first_text[160];
static void check(int condition){if(!condition){fprintf(stderr,"returned-boot log case %d failed\n",fault);exit(1);}}
static EFI_STATUS EFIAPI open_volume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *f,EFI_FILE_PROTOCOL **out){check(f==&fs);*out=fault==2?NULL:&root_mock;return fault==2?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI open_log(EFI_FILE_PROTOCOL *r,EFI_FILE_PROTOCOL **out,CHAR16 *name,UINT64 mode,UINT64 attrs){
  check(r==&root_mock && name[0]=='\\' && mode==(EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE) && !attrs);opens++;*out=fault==3?NULL:&log_mock;return fault==3?EFI_NOT_FOUND:EFI_SUCCESS;
}
static EFI_STATUS EFIAPI seek_log(EFI_FILE_PROTOCOL *f,UINT64 pos){check(f==&log_mock && pos==~0ULL);seeks++;return fault==4?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI write_log(EFI_FILE_PROTOCOL *f,UINTN *n,VOID *data){check(f==&log_mock && *n<sizeof(text));writes++;memcpy(text,data,*n);text[*n]=0;if(writes==1)strcpy(first_text,text);return fault==5?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI flush_log(EFI_FILE_PROTOCOL *f){check(f==&log_mock);flushes++;return fault==6?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS EFIAPI close_file(EFI_FILE_PROTOCOL *f){if(f==&log_mock)log_closes++;else{check(f==&root_mock);root_closes++;}return EFI_SUCCESS;}
int main(void){
  fs.OpenVolume=open_volume;root_mock.Open=open_log;root_mock.Close=close_file;
  log_mock.SetPosition=seek_log;log_mock.Write=write_log;log_mock.Flush=flush_log;log_mock.Close=close_file;
  for(fault=0;fault<7;fault++){
    opens=seeks=writes=flushes=log_closes=root_closes=0;memset(text,0,sizeof(text));
    RecordWindowsReturn(&fs,fault==1?EFI_UNSUPPORTED:EFI_SUCCESS);
    if(fault==2)check(!opens && !root_closes && !log_closes && !writes);
    else if(fault==3)check(opens==1 && root_closes==1 && !log_closes && !writes);
    else {check(opens==1 && seeks==1 && log_closes==1 && root_closes==1);check(writes==(fault==4?0:fault==5 || fault==6?1:2));}
    if(fault<2){check(strstr(first_text,fault==1?"headless-windows-returned=0x8000000000000003":"headless-windows-returned=0x0000000000000000")!=NULL);check(strstr(text,"test-result=0x8000000000000007")!=NULL);}
    if(fault==5)check(!flushes);
  }
  fault=0;LogFile=&log_mock;writes=flushes=0;
  check(RecordPrebootResult(EFI_SUCCESS,TRUE)==EFI_SUCCESS && writes==2);
  check(strstr(first_text,"preboot-check-result=0x0000000000000000") && strstr(text,"windows-visible-handoff-unverified=0x0000000000000001"));
  writes=0;check(RecordPrebootResult(EFI_DEVICE_ERROR,FALSE)==EFI_SUCCESS && writes==1);
  check(strstr(text,"test-result=0x8000000000000007")!=NULL);
  puts("PASS: 9 boot logging cases; preboot success never claims Windows display success");return 0;
}
