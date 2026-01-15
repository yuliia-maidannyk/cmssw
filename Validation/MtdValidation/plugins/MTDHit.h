#ifndef Validation_MtdValidation_MTDHit_h
#define Validation_MtdValidation_MTDHit_h

struct MTDHit {
  float energy;
  float time;
  float x;
  float y;
  float z;
  int process;
  int type;
  int pdgId;
  int trackId;
};

#endif  //Validation_MtdValidation_MTDHit_h
