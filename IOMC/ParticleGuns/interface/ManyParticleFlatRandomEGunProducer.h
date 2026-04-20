#ifndef ManyParticleFlatRandomEGunProducer_H
#define ManyParticleFlatRandomEGunProducer_H

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/one/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "SimGeneral/HepPDTRecord/interface/ParticleDataTable.h"
#include "HepPDT/ParticleDataTable.hh"

#include <vector>
#include <memory>

namespace edm {

  class ManyParticleFlatRandomEGunProducer : public edm::one::EDProducer<> {
  public:
    explicit ManyParticleFlatRandomEGunProducer(const ParameterSet& pset);
    ~ManyParticleFlatRandomEGunProducer();

    void produce(Event& e, const EventSetup& es) override;

  private:
    std::vector<int> fPartIDs;
    std::vector<double> fMinE, fMaxE;
    std::vector<double> fMinEta, fMaxEta;
    std::vector<double> fMinPhi, fMaxPhi;
    std::vector<double> fMass = {0.0, 0.0}; // default mass for all particles is 0
    bool fBackToBack = false;
    bool fAddAntiParticle;
    int fVerbosity;
    const ESGetToken<HepPDT::ParticleDataTable, edm::DefaultRecord> fPDGTableToken;
  
  protected:
    ESHandle<HepPDT::ParticleDataTable> fPDGTable;
  };
} // namespace edm

#endif
