#ifndef _IOTSARADIOPOLICY_H_
#define _IOTSARADIOPOLICY_H_

// Intended to be included from iotsaController.h

//
// IotsaRadioPolicy -- which of {WiFi, BLE} may be powered right now
// (cwi-dis/iotsa#106). Runtime desired state only. IotsaController seeds it from
// the persisted boot policy at begin() (seedFromBootPolicy), moves it via
// setWifiEnabled() / setBleEnabled() (the REST/BLE toggles, battery sleep), and
// layers the mode forcing on top -- {wifi,ble}Wanted() take the mode-effect
// boolean from IotsaModeMachine so this class never sees the iotsa_mode enum.
//
class IotsaRadioPolicy {
public:
  void seedFromBootPolicy(bool wifiDisabledOnBoot, bool bleDisabledOnBoot) {
    _wifiEnabled = !wifiDisabledOnBoot;
    _bleEnabled  = !bleDisabledOnBoot;
  }
  void setWifiEnabled(bool on) { _wifiEnabled = on; }
  void setBleEnabled(bool on)  { _bleEnabled = on; }
  // modeForcesOn: IotsaModeMachine::forcesWifiOn() / forcesBleOn().
  bool wifiWanted(bool modeForcesOn) const { return modeForcesOn || _wifiEnabled; }
  bool bleWanted(bool modeForcesOn) const  { return modeForcesOn || _bleEnabled; }

private:
  bool _wifiEnabled = true;
  bool _bleEnabled = true;
};
#endif
