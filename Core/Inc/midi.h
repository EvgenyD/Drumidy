void sendMidiMessage(uint8_t *_buffer, uint16_t len);

void sendNoteOn(uint8_t ch, uint8_t note, uint8_t vel){
  uint8_t bff[4];
  bff[0] = 0x09;
  bff[1] = 0x90 | ch;
  bff[2] = 0x7f & note;
  bff[3] = 0x7f & vel;
  sendMidiMessage((uint8_t *)bff,4);
}

void sendNoteOff(uint8_t ch, uint8_t note){
  uint8_t bff[4];
  bff[0] = 0x08;
  bff[1] = 0x80 | ch;
  bff[2] = 0x7f & note;
  bff[3] = 0;

  sendMidiMessage((uint8_t *)bff,4);
}

void sendAftertouch(uint8_t ch, uint8_t note){
  uint8_t bff[4];
  bff[0] = 0x09;
  bff[1] = 0xA0 | ch;
  bff[2] = 0x7f & note;
  bff[3] = 0x7f;

  sendMidiMessage((uint8_t *)bff,4);
}


void sendCtlChange(uint8_t ch, uint8_t num, uint8_t value){
  uint8_t bff[4];
  bff[0] = 0x0b;
  bff[1] = 0xb0 | ch;
  bff[2] = 0x7f & num;
  bff[3] = 0x7f & value;

  sendMidiMessage((uint8_t *)bff,4);
}


void sendActSens(){
  uint8_t bff[4];
  bff[0] = 0x0f;
  bff[1] = 0xfe;
  bff[2] = 0;
  bff[3] = 0;

  sendMidiMessage((uint8_t *)bff,4);
}

void sendNoteOnOff(uint8_t ch, uint8_t note, uint8_t vel){
  uint8_t bff[8];
  bff[0] = 0x09;
  bff[1] = 0x90 | ch;
  bff[2] = 0x7f & note;
  bff[3] = 0x7f & vel;
  bff[4] = 0x09;
  bff[5] = 0x90 | ch;
  bff[6] = 0x7f & note;
  bff[7] = 0x00;
  sendMidiMessage((uint8_t *)bff,8);
}
