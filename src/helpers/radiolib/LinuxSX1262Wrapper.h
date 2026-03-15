#pragma once

#include "LinuxSX1262.h"
#include "RadioLibWrappers.h"

class LinuxSX1262Wrapper : public RadioLibWrapper {
public:
  LinuxSX1262Wrapper(LinuxSX1262& radio, mesh::MainBoard& board) : RadioLibWrapper(radio, board) { }
  bool isReceivingPacket() override {
    return ((LinuxSX1262 *)_radio)->isReceiving();
  }
  float getCurrentRSSI() override {
    return ((LinuxSX1262 *)_radio)->getRSSI(false);
  }
  float getLastRSSI() const override { return ((LinuxSX1262 *)_radio)->getRSSI(); }
  float getLastSNR() const override { return ((LinuxSX1262 *)_radio)->getSNR(); }

  float packetScore(float snr, int packet_len) override {
    // spreadingFactor is private in RadioLib 7.4+; read the value tracked by
    // LinuxSX1262::setSpreadingFactor(), which stays in sync with radio_set_params()
    // and reflects CLI/prefs changes after first boot.
    int sf = ((LinuxSX1262 *)_radio)->currentSF;
    return packetScoreInt(snr, sf, packet_len);
  }
};
