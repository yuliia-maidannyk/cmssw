#include <cmath>

#include "IOMC/ParticleGuns/interface/MultiParticleInConeGunProducer.h"

#include "DataFormats/Math/interface/Vector3D.h"
#include "SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h"
#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "FWCore/Utilities/interface/RandomNumberGenerator.h"
#include "FWCore/Utilities/interface/Exception.h"

#include <CLHEP/Random/RandFlat.h>
#include <CLHEP/Units/GlobalPhysicalConstants.h>
#include <CLHEP/Units/SystemOfUnits.h>

using namespace edm;
using namespace std;

MultiParticleInConeGunProducer::MultiParticleInConeGunProducer(const ParameterSet& pset)
    : BaseFlatGunProducer(pset) {
  ParameterSet pgun_params = pset.getParameter<ParameterSet>("PGunParameters");
  fMinE = pgun_params.getParameter<double>("MinE");
  fMaxE = pgun_params.getParameter<double>("MaxE");

  fInConeIds = pgun_params.getParameter<vector<int> >("InConeID");
  fMinDeltaR = pgun_params.getParameter<double>("MinDeltaR");
  fMaxDeltaR = pgun_params.getParameter<double>("MaxDeltaR");

  fInConeMinEta = pgun_params.getParameter<double>("InConeMinEta");
  fInConeMaxEta = pgun_params.getParameter<double>("InConeMaxEta");
  fInConeMinPhi = pgun_params.getParameter<double>("InConeMinPhi");
  fInConeMaxPhi = pgun_params.getParameter<double>("InConeMaxPhi");
  fInConeMaxTry = pgun_params.getParameter<unsigned int>("InConeMaxTry");

  fStartRadius = pgun_params.getParameter<double>("StartRadius");

  fBackToBack = pgun_params.getParameter<bool>("BackToBack");

  if (fBackToBack && fPartIDs.size() != 2) {
    throw cms::Exception("Configuration")
        << "MultiParticleInConeGunProducer: BackToBack requires exactly 2 entries in PartID, got " << fPartIDs.size() << ".";
  }

  produces<HepMCProduct>("unsmeared");
  produces<GenEventInfoProduct>();
}

MultiParticleInConeGunProducer::~MultiParticleInConeGunProducer() {
  // no need to cleanup GenEvent memory - done in HepMCProduct
}
// Builds one particle that starts on a cylinder around the beam line,
// flying straight outward as if it came from the collision point (0,0,0).
// Only valid for neutral particles (e.g. photons), which travel in straight lines.
static HepMC::GenVertex* makeParticleStartingAtRadius(int PartID,
                                                      double mass,
                                                      double energy,
                                                      double eta,
                                                      double phi,
                                                      double startRadiusCm,
                                                      int barcode) {
  
  double momentum = (energy > mass) ? std::sqrt(energy * energy - mass * mass) : 0.;
  double theta = 2. * std::atan(std::exp(-eta));  // angle from the beam line
  double px = momentum * std::sin(theta) * std::cos(phi);
  double py = momentum * std::sin(theta) * std::sin(phi);
  double pz = momentum * std::cos(theta);

  double vx = startRadiusCm * std::cos(phi);
  double vy = startRadiusCm * std::sin(phi);
  double vz = startRadiusCm / std::tan(theta);

  // --- Start time: how long it would have taken to fly here from (0,0,0) ---
  // HepMC stores time as "speed of light x time" in mm.
  // For a particle moving at a fraction "beta" of light speed (1 for photons),
  // that is simply: distance travelled / beta.
  double distanceCm = std::sqrt(vx * vx + vy * vy + vz * vz);
  double beta = (energy > 0.) ? momentum / energy : 1.;
  double timeAsLengthMm = (beta > 0.) ? (distanceCm * CLHEP::cm) / beta : 0.;

  // HepMC positions are in mm; multiplying by CLHEP::cm converts cm -> mm
  HepMC::GenVertex* Vtx = new HepMC::GenVertex(HepMC::FourVector(vx * CLHEP::cm, vy * CLHEP::cm, vz * CLHEP::cm, timeAsLengthMm));
  
  HepMC::FourVector p(px, py, pz, energy);
  HepMC::GenParticle* Part = new HepMC::GenParticle(p, PartID, 1);
  Part->suggest_barcode(barcode);
  Vtx->add_particle_out(Part);
  return Vtx;
}

static double sampleLinearDistribution(CLHEP::HepRandomEngine* engine,
                                       double xmin, double xmax,
                                       double slope = 0.5, double offset = 10.0) {
  // Accumulated probability from 0 up to x (not normalised):
  //   F(x) = slope/2 * x^2 + offset * x
  auto accumulated = [&](double x) { return 0.5 * slope * x * x + offset * x; };

  // Pick a random point between F(xmin) and F(xmax)...
  double u = CLHEP::RandFlat::shoot(engine, 0., 1.);
  double target = accumulated(xmin) + u * (accumulated(xmax) - accumulated(xmin));

  // ...and find the x where F(x) equals it (solving the quadratic).
  if (slope == 0.)
    return target / offset;  // flat distribution: no quadratic needed
  double x = (-offset + std::sqrt(offset * offset + 2. * slope * target)) / slope;

  // Guard against tiny rounding errors at the edges
  return std::min(std::max(x, xmin), xmax);
}

void MultiParticleInConeGunProducer::produce(Event& e, const EventSetup& es) {
  edm::Service<edm::RandomNumberGenerator> rng;
  CLHEP::HepRandomEngine* engine = &rng->getEngine(e.streamID());

  fEvt = new HepMC::GenEvent();
  
  int barcode = 1;

  double eta0 = CLHEP::RandFlat::shoot(engine, fMinEta, fMaxEta);
  double phi0 = CLHEP::RandFlat::shoot(engine, fMinPhi, fMaxPhi);

  double eta1 = -eta0;
  double phi1 = (phi0 < 0) ? phi0 + M_PI : phi0 - M_PI;

  for (unsigned int ip = 0; ip < fPartIDs.size(); ++ip) {

    // ---------- Main particle ----------
    int pdgId = fPartIDs[ip];
    double mass = fPDGTable->particle(HepPDT::ParticleID(std::abs(pdgId)))->mass().value();
    double energy = CLHEP::RandFlat::shoot(engine, fMinE, fMaxE);
    //double energy = sampleLinearDistribution(engine, fMinE, fMaxE);
    // double eta = CLHEP::RandFlat::shoot(engine, fMinEta, fMaxEta);
    // double phi = CLHEP::RandFlat::shoot(engine, fMinPhi, fMaxPhi);

    double eta;
    double phi;
    if (!fBackToBack) {
      eta = CLHEP::RandFlat::shoot(engine, fMinEta, fMaxEta);
      phi = CLHEP::RandFlat::shoot(engine, fMinPhi, fMaxPhi);
    }
    else {
      eta = (ip == 0) ? eta0 : eta1;
      phi = (ip == 0) ? phi0 : phi1;
    }

    fEvt->add_vertex(makeParticleStartingAtRadius(pdgId, mass, energy, eta, phi, fStartRadius, barcode));
    barcode++;

    // ---------- Particles in a cone around it ----------
    for (unsigned int iCone = 0; iCone < fInConeIds.size(); ++iCone) {

      // Try a few random positions until one lands inside the allowed eta/phi range
      bool foundGoodPosition = false;
      double etaCone = 0., phiCone = 0.;

      for (unsigned int attempt = 0; attempt <= fInConeMaxTry; ++attempt) {
        double distance = CLHEP::RandFlat::shoot(engine, fMinDeltaR, fMaxDeltaR);
        double angle = CLHEP::RandFlat::shoot(engine, -M_PI, M_PI);

        etaCone = eta + distance * std::cos(angle);
        // std::remainder keeps phi between -pi and +pi
        phiCone = std::remainder(phi + distance * std::sin(angle), 2. * M_PI);

        bool etaOk = (etaCone >= fInConeMinEta && etaCone <= fInConeMaxEta);
        bool phiOk = (phiCone >= fInConeMinPhi && phiCone <= fInConeMaxPhi);
        if (etaOk && phiOk) {
          foundGoodPosition = true;
          break;
        }
      }

      if (!foundGoodPosition) {
        if (fVerbosity > 0)
          cout << " No valid cone position found; skipping this cone particle." << endl;
        continue;
      }

      int pdgIdCone = fInConeIds[iCone];
      double massCone = fPDGTable->particle(HepPDT::ParticleID(std::abs(pdgIdCone)))->mass().value();
      double energyCone = CLHEP::RandFlat::shoot(engine, fMinE, fMaxE);
      //double energyCone = sampleLinearDistribution(engine, fMinE, fMaxE);
      if (iCone == 1) 
      {
        energyCone = CLHEP::RandFlat::shoot(engine, 1.0, 10.0); 
      }

      // Its OWN start point, along its OWN direction
      fEvt->add_vertex(makeParticleStartingAtRadius(pdgIdCone, massCone, energyCone, etaCone, phiCone, fStartRadius, barcode));
      barcode++;
    }
  }

  fEvt->set_event_number(e.id().event());
  fEvt->set_signal_process_id(20);
  if (fVerbosity > 0)
    fEvt->print();

  unique_ptr<HepMCProduct> BProduct(new HepMCProduct());
  BProduct->addHepMCData(fEvt);
  e.put(std::move(BProduct), "unsmeared");

  unique_ptr<GenEventInfoProduct> genEventInfo(new GenEventInfoProduct(fEvt));
  e.put(std::move(genEventInfo));
}
