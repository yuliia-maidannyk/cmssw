#ifndef ManyParticleFlatRandomEGunProducer_H
#define ManyParticleFlatRandomEGunProducer_H

#include "IOMC/ParticleGuns/interface/BaseFlatGunProducer.h"

namespace edm {

  class ManyParticleFlatRandomEGunProducer : public BaseFlatGunProducer {
  public:
    explicit ManyParticleFlatRandomEGunProducer(const ParameterSet& pset);
    ~ManyParticleFlatRandomEGunProducer() override;

    void produce(Event& e, const EventSetup& es) override;

  private:
    std::vector<double> fMinE;
    std::vector<double> fMaxE;
    std::vector<double> fMinEta;
    std::vector<double> fMaxEta;
    std::vector<double> fMinPhi;
    std::vector<double> fMaxPhi;
  };
} // namespace edm

#endif
