/* GPL-2.0-only. The caller pins identity, image and topology on every Read. */
#include "transaction.h"
static BOOLEAN Equal(const HT_STATE *a,const HT_STATE *b) {
  for(UINTN i=0;i<64;i++)if(a->Config[i]!=b->Config[i])return FALSE;
  for(UINTN i=0;i<16;i++)if(a->Registers[i]!=b->Registers[i])return FALSE;
  return a->Attributes==b->Attributes && a->Owner==b->Owner && a->Children==b->Children && a->Gops==b->Gops;
}
static BOOLEAN Initial(const HT_STATE *s) {
  /* Match the upstream entry-selector policy (unlock/port.c and ColdRead).
   * SS0 is an observed entry value, not an immutable board identity. This
   * display-only test never writes selectors. Equal() still compares all bits
   * against this boot's entry before disconnect and after reconnection. */
  UINT32 ss0=s->Registers[12];
  return s->Config[0]==0x250310de && s->Config[2]==0x030000a1 && s->Config[11]==0x250310de &&
    (s->Config[1]&0xffff)==7 && s->Attributes==0x718 && s->Owner && s->Children==1 && s->Gops==1 &&
    s->Registers[0]==0x3000 && s->Registers[5]==0xb76000a1 &&
    s->Registers[6]==0x8b8f && s->Registers[7]==0x3ff && s->Registers[8]==0x10 &&
    s->Registers[10]==0 && s->Registers[11]==0xffffff8f &&
    ss0!=0xffffffffU && (ss0&0xffff0000U)!=0xbadf0000U && ss0!=0x88888888U && !s->Registers[13];
}
EFI_STATUS HtRun(HT_TEST *t) {
  if(!t || t->Used || !t->Read || !t->Log || !t->Disconnect || !t->Reconnect)return EFI_INVALID_PARAMETER;
  t->Used=TRUE;t->Attempted=FALSE;t->Restored=FALSE;t->Released=FALSE;t->HeadlessReady=FALSE;
  HT_STATE before={0},fresh={0},after={0},restored={0};
  EFI_STATUS s=t->Read(t->Context,&before);
  if(s!=EFI_SUCCESS){t->Log(t->Context,"initial-state-read-failed",s);return s;}
  s=t->Log(t->Context,"entry-ss0",before.Registers[12]);if(s!=EFI_SUCCESS)return s;
  s=t->Log(t->Context,"entry-ss1",before.Registers[13]);if(s!=EFI_SUCCESS)return s;
  if(!Initial(&before)){t->Log(t->Context,"initial-state-refused",EFI_UNSUPPORTED);return EFI_UNSUPPORTED;}
  s=t->Log(t->Context,"disconnect-intent",0);
  if(s!=EFI_SUCCESS)return s;
  /* Last read after durable intent, before the only disconnect attempt. */
  s=t->Read(t->Context,&fresh);
  if(s!=EFI_SUCCESS || !Equal(&before,&fresh)){
    EFI_STATUS reason=s==EFI_SUCCESS?EFI_NOT_READY:s;
    t->Log(t->Context,"pre-disconnect-state-changed-or-unreadable",reason);return reason;
  }
  t->Attempted=TRUE;
  EFI_STATUS detached=t->Disconnect(t->Context);
  EFI_STATUS logged=t->Log(t->Context,"disconnect-return",detached);
  EFI_STATUS observed=t->Read(t->Context,&after);
  if(t->Log(t->Context,"post-disconnect-read-status",observed)!=EFI_SUCCESS)logged=EFI_DEVICE_ERROR;
  if(observed==EFI_SUCCESS && detached==EFI_SUCCESS && !after.Owner && !after.Children && !after.Gops &&
     after.Config[0]==before.Config[0] && after.Config[2]==before.Config[2] && after.Config[11]==before.Config[11])t->Released=TRUE;
  if(t->Log(t->Context,"released-observed",t->Released)!=EFI_SUCCESS)logged=EFI_DEVICE_ERROR;
  EFI_STATUS inspected=EFI_SUCCESS;
  if(t->Released && logged==EFI_SUCCESS && t->InspectReleased){
    inspected=t->InspectReleased(t->Context,&before);
    if(t->Log(t->Context,"released-inspection-result",inspected)!=EFI_SUCCESS)logged=EFI_DEVICE_ERROR;
  }
  /* Explicit diagnostic mode: all inspections must pass, followed by a fresh
   * final guard. The caller owns the direct Windows handoff; no console or
   * firmware GOP reconnect is permitted on this success path. */
  if(t->Released && logged==EFI_SUCCESS && inspected==EFI_SUCCESS && t->InspectReleased && t->PrepareWindowsWithoutGop) {
    inspected=t->PrepareWindowsWithoutGop(t->Context,&before);
    if(inspected==EFI_SUCCESS){t->HeadlessReady=TRUE;return EFI_SUCCESS;}
  }
  /* Attempt recovery even after a partial disconnect or a logging/read error.
   * Reconnect is not evidence of recovery: require a fresh exact readback. */
  if(t->MayRecoverDisplay && !t->MayRecoverDisplay(t->Context)) {
    t->Log(t->Context,"recovery-forbidden-after-core-or-retained-resources",1);
    return EFI_ACCESS_DENIED;
  }
  EFI_STATUS connected=t->Reconnect(t->Context);
  if(t->Log(t->Context,"reconnect-return",connected)!=EFI_SUCCESS)logged=EFI_DEVICE_ERROR;
  EFI_STATUS checked=t->Read(t->Context,&restored);
  if(t->Log(t->Context,"post-reconnect-read-status",checked)!=EFI_SUCCESS)logged=EFI_DEVICE_ERROR;
  t->Restored=connected==EFI_SUCCESS && checked==EFI_SUCCESS && Equal(&before,&restored);
  if(t->Log(t->Context,"restored-verified",t->Restored)!=EFI_SUCCESS)logged=EFI_DEVICE_ERROR;
  if(!t->Restored)return EFI_DEVICE_ERROR;
  if(logged!=EFI_SUCCESS)return logged;
  if(detached!=EFI_SUCCESS)return detached;
  if(inspected!=EFI_SUCCESS)return inspected;
  return t->Released?EFI_SUCCESS:EFI_NOT_READY;
}
