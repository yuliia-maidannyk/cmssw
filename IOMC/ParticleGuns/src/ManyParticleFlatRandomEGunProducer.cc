#include <ostream>

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/AbstractServices/interface/RandomNumberGenerator.h"

#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/one/EDProducer.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "SimGeneral/HepPDTRecord/interface/ParticleDataTable.h"
#include "HepPDT/ParticleDataTable.hh"

#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"

#include "CLHEP/Random/RandFlat.h"
#include <CLHEP/Units/GlobalPhysicalConstants.h>
#include <CLHEP/Units/SystemOfUnits.h>

#include <vector>
#include <memory>

using namespace edm;
using namespace std;

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
    std::vector<double> fMass = {0.0, 0.0};
    bool fBackToBack = false;
    bool fAddAntiParticle;
    int fVerbosity;
    double fStartRadius = 0.0;
    const ESGetToken<HepPDT::ParticleDataTable, edm::DefaultRecord> fPDGTableToken;
  
  protected:
    ESHandle<HepPDT::ParticleDataTable> fPDGTable;
  };
    
  ManyParticleFlatRandomEGunProducer::ManyParticleFlatRandomEGunProducer(const ParameterSet& pset)
      : fPDGTableToken(esConsumes()) {
    ParameterSet pgun = pset.getParameter<ParameterSet>("PGunParameters");

    fPartIDs  = pgun.getParameter<std::vector<int>>("PartID");
    fMinE     = pgun.getParameter<std::vector<double>>("MinE");
    fMaxE     = pgun.getParameter<std::vector<double>>("MaxE");
    fMinEta   = pgun.getParameter<std::vector<double>>("MinEta");
    fMaxEta   = pgun.getParameter<std::vector<double>>("MaxEta");
    fMinPhi   = pgun.getParameter<std::vector<double>>("MinPhi");
    fMaxPhi   = pgun.getParameter<std::vector<double>>("MaxPhi");
    fMass     = pgun.getParameter<std::vector<double>>("Mass");
    fStartRadius = pgun.getParameter<double>("StartRadius");

    fAddAntiParticle = pset.getParameter<bool>("AddAntiParticle");
    fVerbosity = pset.getUntrackedParameter<int>("Verbosity", 0);
    fBackToBack = pset.getParameter<bool>("BackToBack");

    produces<HepMCProduct>("unsmeared");
    produces<GenEventInfoProduct>();

    cout << "ManyParticleFlatRandomEGunProducer initialized with " << fMinE.size() << " particles" << endl;
  }

  ManyParticleFlatRandomEGunProducer::~ManyParticleFlatRandomEGunProducer() {
    // no need to cleanup fEvt since it's done in HepMCProduct
  }

  void ManyParticleFlatRandomEGunProducer::produce(Event& e, const EventSetup& es) {
    edm::Service<edm::RandomNumberGenerator> rng;
    CLHEP::HepRandomEngine* engine = &rng->getEngine(e.streamID());

    if (fVerbosity > 0) {
      cout << " ManyParticleFlatRandomEGunProducer : Begin New Event Generation" << endl;
    }

    HepMC::GenEvent* fEvt = new HepMC::GenEvent();
    fPDGTable = es.getHandle(fPDGTableToken);

    int barcode = 1;

    double phi0 = CLHEP::RandFlat::shoot(engine, fMinPhi[0], fMaxPhi[0]);
    double phi1;
    if (fBackToBack && fPartIDs.size() == 2) {
      phi1 = (phi0 < 0) ? phi0 + M_PI : phi0 - M_PI;
    }
    else {
      phi1 = CLHEP::RandFlat::shoot(engine, fMinPhi[1], fMaxPhi[1]);
    }

    for (unsigned int ip = 0; ip < fPartIDs.size(); ip++) {
      double energy = CLHEP::RandFlat::shoot(engine, fMinE[ip], fMaxE[ip]);
      double eta    = CLHEP::RandFlat::shoot(engine, fMinEta[ip], fMaxEta[ip]);
      double phi;
      if (!fBackToBack) {
        phi = CLHEP::RandFlat::shoot(engine, fMinPhi[ip], fMaxPhi[ip]);
      }
      else {
        phi = (ip == 0) ? phi0 : phi1;
      }

      int PartID = fPartIDs[ip];

      const HepPDT::ParticleData* PData = fPDGTable->particle(HepPDT::ParticleID(abs(PartID)));
      
      double mass = (fMass[ip] > 0.) ? fMass[ip] : PData->mass().value();
      double mom2 = energy * energy - mass * mass;
      double mom  = (mom2 > 0.) ? sqrt(mom2) : 0.;

      double theta = 2. * atan(exp(-eta));

      double px = mom * sin(theta) * cos(phi);
      double py = mom * sin(theta) * sin(phi);
      double pz = mom * cos(theta);

      HepMC::FourVector p(px, py, pz, energy);

      double vx = fStartRadius * cos(phi);
      double vy = fStartRadius * sin(phi);
      double vz = fStartRadius / tan(theta);

      // --- Start time: how long it would have taken to fly here from (0,0,0) ---
      // HepMC stores time as "speed of light x time" in mm.
      // For a particle moving at a fraction "beta" of light speed (1 for photons),
      // that is simply: distance travelled / beta.
      double distanceCm = std::sqrt(vx * vx + vy * vy + vz * vz);
      double beta = (energy > 0.) ? mom / energy : 1.;
      double timeAsLengthMm = (beta > 0.) ? (distanceCm * CLHEP::cm) / beta : 0.;

      // HepMC positions are in mm; multiplying by CLHEP::cm converts cm -> mm
      HepMC::GenVertex* Vtx = new HepMC::GenVertex(HepMC::FourVector(vx * CLHEP::cm, vy * CLHEP::cm, vz * CLHEP::cm, timeAsLengthMm));
      
      // Create parent particle
      HepMC::GenParticle* Part = new HepMC::GenParticle(p, PartID, 1);
      Part->suggest_barcode(barcode);
      barcode++;
      Vtx->add_particle_out(Part);
      fEvt->add_vertex(Vtx);

      if (fAddAntiParticle) {
      //   HepMC::FourVector ap(-px, -py, -pz, energy);
      //   int APartID = (PartID == 22 || PartID == 23) ? PartID : -PartID;

      //   HepMC::GenParticle* APart = new HepMC::GenParticle(ap, APartID, 1);
      //   APart->suggest_barcode(barcode);
      //   barcode++;
      //   Vtx->add_particle_out(APart);
      }
    }

    fEvt->set_event_number(e.id().event());
    fEvt->set_signal_process_id(20);

    if (fVerbosity > 0) {
      fEvt->print();
    }

    std::unique_ptr<HepMCProduct> BProduct(new HepMCProduct());
    BProduct->addHepMCData(fEvt);
    e.put(std::move(BProduct), "unsmeared");

    std::unique_ptr<GenEventInfoProduct> genEventInfo(new GenEventInfoProduct(fEvt));
    e.put(std::move(genEventInfo));

    if (fVerbosity > 0) {
      cout << " ManyParticleFlatRandomEGunProducer : Event Generation Done " << endl;
    }
  }
}

using edm::ManyParticleFlatRandomEGunProducer;
DEFINE_FWK_MODULE(ManyParticleFlatRandomEGunProducer);
