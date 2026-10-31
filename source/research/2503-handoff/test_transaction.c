/* RAM-only fault injection of the actual transaction state machine. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef NULL
#include "transaction.h"
enum {OK,IDENTITY,DRIFT,INTENT_FAIL,DETACH_FAIL,PARTIAL,OBSERVE_FAIL,RECONNECT_FAIL,RESTORE_DRIFT,LOG_AFTER,UNRELEASED,COLD_BAD,
      NEW_ENTRY,ENTRY_LOG_FAIL,SS0_BADF,SS0_FFFF,SS0_UNLOCKED,SS1_CHANGED,ENTRY_DRIFT,RESTORE_SS0_DRIFT,GFW_BAD,PLM_BAD,MEMORY_BAD,CPU_BAD,
      CORE_NO_RECOVERY,INSPECT_NO_RECOVERY,CORE_HEADLESS_OK};
static int mode,reads,disconnects,reconnects,logs;
static HT_STATE baseline;
static void require(int ok,const char *message){if(!ok){fprintf(stderr,"FAIL mode=%d %s\n",mode,message);exit(1);}}
static EFI_STATUS read_state(VOID *ctx,HT_STATE *s){
  (void)ctx;reads++;*s=baseline;
  if(mode==IDENTITY)s->Config[0]=0x250410de;
  if(mode==COLD_BAD)s->Registers[10]=0x100;
  if(mode==NEW_ENTRY)s->Registers[12]=0x46446172;
  if(mode==SS0_BADF)s->Registers[12]=0xbadf5040;
  if(mode==SS0_FFFF)s->Registers[12]=0xffffffff;
  if(mode==SS0_UNLOCKED)s->Registers[12]=0x88888888;
  if(mode==SS1_CHANGED)s->Registers[13]=1;
  if(mode==ENTRY_DRIFT && reads==2)s->Registers[12]=0x46446172;
  if(mode==RESTORE_SS0_DRIFT && reads==4)s->Registers[12]=0x46446172;
  if(mode==GFW_BAD)s->Registers[7]=0;
  if(mode==PLM_BAD)s->Registers[11]=0;
  if(mode==MEMORY_BAD)s->Registers[0]=0x6000;
  if(mode==CPU_BAD)s->Registers[8]=0;
  if(mode==DRIFT && reads==2)s->Config[4]^=0x1000000;
  if(reads==3){
    if(mode==OBSERVE_FAIL)return EFI_DEVICE_ERROR;
    if(mode!=UNRELEASED){s->Owner=0;s->Children=0;s->Gops=mode==PARTIAL?1:0;}
  }
  if(mode==RESTORE_DRIFT && reads==4)s->Config[1]^=4;
  return EFI_SUCCESS;
}
static EFI_STATUS log_event(VOID *ctx,const char *name,UINT64 value){
  (void)ctx;(void)value;logs++;
  return (mode==INTENT_FAIL && !strcmp(name,"disconnect-intent")) ||
    (mode==LOG_AFTER && !strcmp(name,"disconnect-return")) ||
    (mode==ENTRY_LOG_FAIL && !strcmp(name,"entry-ss0"))?EFI_DEVICE_ERROR:EFI_SUCCESS;
}
static EFI_STATUS detach(VOID *ctx){(void)ctx;disconnects++;return mode==DETACH_FAIL?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS reconnect(VOID *ctx){(void)ctx;reconnects++;return mode==RECONNECT_FAIL?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS inspect(VOID *ctx,const HT_STATE *entry){(void)ctx;(void)entry;return mode==INSPECT_NO_RECOVERY?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static EFI_STATUS prepare(VOID *ctx,const HT_STATE *entry){(void)ctx;(void)entry;return mode==CORE_NO_RECOVERY?EFI_DEVICE_ERROR:EFI_SUCCESS;}
static BOOLEAN may_recover(VOID *ctx){(void)ctx;return FALSE;}
int main(void){
  baseline.Config[0]=baseline.Config[11]=0x250310de;baseline.Config[2]=0x030000a1;
  baseline.Config[1]=7;baseline.Attributes=0x718;baseline.Owner=1;baseline.Children=baseline.Gops=1;
  baseline.Registers[5]=0xb76000a1;baseline.Registers[12]=0x46042172;
  baseline.Registers[0]=0x3000;baseline.Registers[6]=0x8b8f;baseline.Registers[7]=0x3ff;
  baseline.Registers[8]=0x10;baseline.Registers[11]=0xffffff8f;
  for(mode=OK;mode<=CORE_HEADLESS_OK;mode++){
    reads=disconnects=reconnects=logs=0;
    HT_TEST t={0};t.Read=read_state;t.Log=log_event;t.Disconnect=detach;t.Reconnect=reconnect;
    if(mode>=CORE_NO_RECOVERY){t.InspectReleased=inspect;t.PrepareWindowsWithoutGop=prepare;t.MayRecoverDisplay=may_recover;}
    EFI_STATUS result=HtRun(&t);
    if(mode>=CORE_NO_RECOVERY){
      require(t.Released && t.Attempted && !t.Restored && disconnects==1 && !reconnects && reads==3,"no reconnect or further state read after caller forbids recovery");
      require((result==EFI_SUCCESS)==(mode==CORE_HEADLESS_OK) && t.HeadlessReady==(mode==CORE_HEADLESS_OK),"only complete core preparation can bypass recovery");
    }
    else if(mode==OK || mode==NEW_ENTRY){require(result==EFI_SUCCESS && t.Released && t.Restored,"successful release/recovery with this boot's entry");}
    else require(result!=EFI_SUCCESS,"fault cannot pass");
    if(mode>=CORE_NO_RECOVERY){}
    else if(mode==IDENTITY || mode==DRIFT || mode==INTENT_FAIL || mode==COLD_BAD || mode==ENTRY_LOG_FAIL ||
       (mode>=SS0_BADF && mode<=ENTRY_DRIFT) || mode>=GFW_BAD)require(!t.Attempted && !disconnects && !reconnects,"preflight has no mutation");
    else {
      require(t.Attempted && disconnects==1 && reconnects==1,"recover once even after errors");
      require(t.Restored==(mode!=RECONNECT_FAIL && mode!=RESTORE_DRIFT && mode!=RESTORE_SS0_DRIFT),"recovery requires exact readback");
    }
    int old_dis=disconnects,old_rec=reconnects;
    require(HtRun(&t)==EFI_INVALID_PARAMETER && disconnects==old_dis && reconnects==old_rec,"no retry in same context");
  }
  puts("PASS: 27 transaction scenarios including cold state and forbidden post-core recovery");
  return 0;
}
