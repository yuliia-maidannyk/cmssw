#include <ostream>

#include "IOMC/ParticleGuns/interface/ManyParticleFlatRandomEGunProducer.h"

#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"

#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"

#include "CLHEP/Random/RandFlat.h"

using namespace edm;
using namespace std;

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

  fAddAntiParticle = pset.getParameter<bool>("AddAntiParticle");
  fVerbosity = pset.getUntrackedParameter<int>("Verbosity", 0);

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
  HepMC::GenVertex* Vtx = new HepMC::GenVertex(HepMC::FourVector(0., 0., 0.));
  fPDGTable = es.getHandle(fPDGTableToken);

  int barcode = 1;

  for (unsigned int ip = 0; ip < fPartIDs.size(); ip++) {

    // Linear distribution f(x) = m*x + b on [fMinE[ip], fMaxE[ip]]
    // Parameters
    // double a = fMinE[ip];
    // double xMax = fMaxE[ip];
    // double m = -12.0;
    // double bCoef = 1200.0;

    // // Normalization: integral of f(x) from a to xMax
    // double N = (m / 2.0) * (xMax * xMax - a * a) + bCoef * (xMax - a);

    // // Draw a uniform random number
    // double u = CLHEP::RandFlat::shoot(engine, 0.0, 1.0);

    // // Solve quadratic: (m/2)*x^2 + bCoef*x - C = 0
    // // where C = u*N + (m/2)*a^2 + bCoef*a
    // double C = u * N + (m / 2.0) * a * a + bCoef * a;

    // // Quadratic formula: x = (-bCoef ± sqrt(bCoef^2 + 2*m*C)) / m
    // double discriminant = bCoef * bCoef + 2.0 * m * C;
    // double energy = (-bCoef + std::sqrt(discriminant)) / m;
    
    double energy = CLHEP::RandFlat::shoot(engine, fMinE[ip], fMaxE[ip]);
    double eta    = CLHEP::RandFlat::shoot(engine, fMinEta[ip], fMaxEta[ip]);
    double phi    = CLHEP::RandFlat::shoot(engine, fMinPhi[ip], fMaxPhi[ip]);

    int PartID = fPartIDs[ip];

    const HepPDT::ParticleData* PData = fPDGTable->particle(HepPDT::ParticleID(abs(PartID)));
    double mass = PData->mass().value();
    double mom2 = energy * energy - mass * mass;
    double mom  = (mom2 > 0.) ? sqrt(mom2) : 0.;

    double theta = 2. * atan(exp(-eta));

    double px = mom * sin(theta) * cos(phi);
    double py = mom * sin(theta) * sin(phi);
    double pz = mom * cos(theta);

    HepMC::FourVector p(px, py, pz, energy);
    HepMC::GenParticle* Part = new HepMC::GenParticle(p, PartID, 1);
    Part->suggest_barcode(barcode);
    barcode++;
    Vtx->add_particle_out(Part);

    if (fAddAntiParticle) {
      HepMC::FourVector ap(-px, -py, -pz, energy);
      int APartID = (PartID == 22 || PartID == 23) ? PartID : -PartID;

      HepMC::GenParticle* APart = new HepMC::GenParticle(ap, APartID, 1);
      APart->suggest_barcode(barcode);
      barcode++;
      Vtx->add_particle_out(APart);
    }
  }

  fEvt->add_vertex(Vtx);
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
