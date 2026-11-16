#ifndef MultiParticleInConeGunProducer_H
#define MultiParticleInConeGunProducer_H

#include "IOMC/ParticleGuns/interface/BaseFlatGunProducer.h"

namespace edm {

  class MultiParticleInConeGunProducer : public BaseFlatGunProducer {
  public:
    MultiParticleInConeGunProducer(const ParameterSet&);
    ~MultiParticleInConeGunProducer() override;

  private:
    void produce(Event& e, const EventSetup& es) override;

  protected:
    // data members
    double fMinE;
    double fMaxE;

    std::vector<int> fInConeIds;
    double fMinDeltaR;
    double fMaxDeltaR;

    double fInConeMinEta;
    double fInConeMaxEta;
    double fInConeMinPhi;
    double fInConeMaxPhi;
    unsigned int fInConeMaxTry;
    double fInConeMinE;
    double fInConeMaxE;

    bool fUseCrystalWindow;        // true = use the eta/phi crystal window instead of the circular cone
    double fMaxDeltaEtaCrystals;   // max eta offset, in crystals
    double fMaxDeltaPhiCrystals;   // max phi offset, in crystals

    double fStartRadius;
    bool fBackToBack;
  };
}  // namespace edm

#endif
