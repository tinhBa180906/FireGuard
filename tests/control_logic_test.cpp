#include <cassert>
#include <iostream>
#include "../FireGuard/ControlLogic.h"
int main(){
  Inputs x; x.ready=true;
  ControlLogic c;
  c.step(x,0); assert(!c.pump && !c.fan);
  x.hot=true;c.step(x,100);assert(c.fan);
  x.smokeRaw=true;c.step(x,200);assert(!c.fan);
  x.smoke=true;c.step(x,2000);assert(c.fireLatched && !c.pump);
  x.armSwitch=true;x.flame=x.flameRaw=true;c.step(x,2100);
  assert(!c.pump); // late ARM cannot enable pump for this incident
  x={};x.ready=true;c.step(x,3000);x.resetPressed=true;c.step(x,13000);
  assert(c.resetAccepted && !c.fireLatched);
  x.resetPressed=false;x.armSwitch=true;c.step(x,13100);
  x.flame=x.flameRaw=true;c.step(x,14000);assert(c.fireLatched && !c.pump);
  x.smoke=x.smokeRaw=true;c.step(x,15000);assert(c.pump);
  c.step(x,17999);assert(c.pump);c.step(x,18000);assert(!c.pump);
  c.step(x,19000);assert(!c.pump); // no repeat pulse
  x.resetPressed=true;c.step(x,19100);assert(c.resetRejected && c.fireLatched);
  // Fault and ARM OFF interrupt a running pump.
  ControlLogic d;Inputs y;y.ready=true;d.step(y,0);
  y.armSwitch=true;y.flame=y.flameRaw=y.smoke=y.smokeRaw=true;d.step(y,100);
  assert(d.pump);y.fault=true;d.step(y,200);assert(!d.pump);
  y.fault=false;d.step(y,300);assert(!d.pump); // no automatic restart
  ControlLogic g;Inputs b;b.ready=true;g.step(b,0);
  b.armSwitch=true;b.flame=b.flameRaw=b.hot=true;g.step(b,100);assert(g.pump);
  b.armSwitch=false;g.step(b,110);assert(!g.pump);
  ControlLogic j;Inputs v;v.ready=true;j.step(v,0);
  v.armSwitch=true;v.flame=v.flameRaw=v.hot=true;j.step(v,100);assert(j.pump);
  v.flameRaw=false;j.step(v,110);assert(!j.pump);
  // A switch left ON at boot cannot arm the pump.
  ControlLogic e;Inputs z;z.ready=z.armSwitch=z.flame=z.flameRaw=z.hot=true;
  e.step(z,1);assert(e.fireLatched && !e.pump);
  // Flame detection works while MQ is warming up.
  ControlLogic f;Inputs w;w.flame=w.flameRaw=true;f.step(w,10);
  assert(f.fireLatched && !f.pump);
  // HELP is latched, reset requires safe 10 seconds AND ARM OFF.
  ControlLogic h;Inputs a;a.ready=true;a.helpPressed=true;h.step(a,0);
  assert(h.helpLatched);a.helpPressed=false;h.step(a,100);
  a.resetPressed=true;h.step(a,10099);assert(h.resetRejected);
  a.resetPressed=false;h.step(a,10100);a.resetPressed=true;h.step(a,10101);
  assert(h.resetAccepted && !h.helpLatched);
  // Confirm timer is resistant to uint32_t millis overflow.
  HoldSignal timer;assert(!timer.update(true,0xfffffff0u,100));
  assert(timer.update(true,0x60u,100));
  std::cout<<"PASS: fan interlock, fire latch, corroboration, one-shot timeout, late ARM, boot ARM lock, fault stop, warmup, reset and timer rollover\n";
}
