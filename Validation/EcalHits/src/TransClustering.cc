// #include "DataFormats/Common/interface/ValidHandle.h"
#include "DataFormats/Math/interface/GeantUnits.h"
// #include "DataFormats/FTLRecHit/interface/FTLRecHitCollections.h"
// #include "DataFormats/FTLRecHit/interface/FTLClusterCollections.h"

// #include "SimDataFormats/CaloAnalysis/interface/CaloParticle.h"
// #include "SimDataFormats/CrossingFrame/interface/CrossingFrame.h"
// #include "SimDataFormats/CrossingFrame/interface/MixCollection.h"
// #include "SimDataFormats/TrackingHit/interface/PSimHit.h"
// #include "SimDataFormats/Vertex/interface/SimVertex.h"

#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
// #include "CondFormats/EcalObjects/interface/EcalADCToGeVConstant.h"

#include "Validation/EcalHits/interface/TransClustering.h"

EcalSimPhotonMCTruth::EcalSimPhotonMCTruth(int isAConversion,
                                           const math::XYZTLorentzVectorD &v,
                                           float rconv,
                                           float zconv,
                                           const math::XYZTLorentzVectorD &convVertex,
                                           const math::XYZTLorentzVectorD &pV,
                                           const std::vector<const SimTrack *> &tracks)
    : isAConversion_(isAConversion),
      thePhoton_(v),
      theR_(rconv),
      theZ_(zconv),
      theConvVertex_(convVertex),
      thePrimaryVertex_(pV),
      tracks_(tracks) {}

// ------------ constructor and destructor --------------
TransClustering::TransClustering(const edm::ParameterSet& iConfig)
  : g4InfoLabel(iConfig.getParameter<std::string>("moduleLabelG4")),
    EBHitsCollection(iConfig.getParameter<std::string>("EBHitsCollection")),
    ValidationCollection(iConfig.getParameter<std::string>("ValidationCollection")),
    jobId(iConfig.getParameter<std::string>("jobId"))
{
  EBrechitCollection_Token = consumes<EBRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBrechitCollection"));
  EBuncalibrechitCollection_Token = consumes<EBUncalibratedRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBuncalibrechitCollection"));
  EBHitsToken = consumes<edm::PCaloHitContainer>(edm::InputTag(std::string(g4InfoLabel), std::string(EBHitsCollection)));
  //reducedBarrelRecHitToken = consumes<EcalRecHitCollection>(iConfig.getParameter<edm::InputTag>("reducedBarrelRecHitCollection"));
  ValidationCollectionToken = consumes<PEcalValidInfo>(edm::InputTag(std::string(g4InfoLabel), std::string(ValidationCollection)));
  genParticleToken = consumes<reco::GenParticleCollection>(iConfig.getParameter<edm::InputTag>("genParticles"));
  SimTrackToken = consumes<edm::SimTrackContainer>(iConfig.getParameter<edm::InputTag>("simTrackCollection"));
  SimVertexToken = consumes<edm::SimVertexContainer>(iConfig.getParameter<edm::InputTag>("simVertexCollection"));
  CaloParticle_Token = consumes<CaloParticleCollection>(iConfig.getParameter<edm::InputTag>("CaloParticleCollection"));
  HepMCToken = consumes<edm::HepMCProduct>(iConfig.getParameter<edm::InputTag>("HepMCProductLabel"));
}

TransClustering::~TransClustering() {
  simTree->Fill();
  recoTree->Fill();
  caloTree->Fill();
  myFile->Write();
  myFile->Close();
}

// ------------ method called for each event  ------------
void TransClustering::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  using namespace std;
  using namespace geant_units::operators;

  static unsigned long eventCount = 0;
  ++eventCount;
  if (eventCount % 100 == 0) {
    std::cout << "Processed " << eventCount << " events" << std::endl;
  }
  // const EcalADCToGeVConstant *agc = &iSetup.getData(pAgc_);
  // const double barrelADCtoGeV_ = agc->getEBValue();

  edm::Handle<edm::PCaloHitContainer> EcalHitsEB;
  iEvent.getByToken(EBHitsToken, EcalHitsEB);

  std::vector<PCaloHit> theEBCaloHits;
  theEBCaloHits.insert(theEBCaloHits.end(), EcalHitsEB->begin(), EcalHitsEB->end());

  edm::Handle<CaloParticleCollection> caloParticles;
  iEvent.getByToken(CaloParticle_Token, caloParticles);

  // edm::Handle<EcalRecHitCollection> EcalRecHitsEB;
  // iEvent.getByToken(reducedBarrelRecHitToken, EcalRecHitsEB);

  // edm::Handle<EcalUncalibratedRecHitCollection> EcalRecHitsEB;
  // iEvent.getByToken(EBrechitCollection_Token, EcalRecHitsEB);

  // std::vector<EcalRecHit> theEBRecHits;
  // theEBRecHits.insert(theEBRecHits.end(), EcalRecHitsEB->begin(), EcalRecHitsEB->end());

  const EBUncalibratedRecHitCollection *EBUncalibRecHit = nullptr;
  edm::Handle<EBUncalibratedRecHitCollection> EcalUncalibRecHitEB;
  iEvent.getByToken(EBuncalibrechitCollection_Token, EcalUncalibRecHitEB);
  if (EcalUncalibRecHitEB.isValid()) {
    EBUncalibRecHit = EcalUncalibRecHitEB.product();
  }
  const EBRecHitCollection *EBRecHit = nullptr;
  edm::Handle<EBRecHitCollection> EcalRecHitEB;
  iEvent.getByToken(EBrechitCollection_Token, EcalRecHitEB);
  if (EcalRecHitEB.isValid()) {
    EBRecHit = EcalRecHitEB.product();
  }
  //const EcalRecHitCollection *theEBRecHits = nullptr;
  Labels l;
  // labelsForToken(reducedBarrelRecHitToken, l);
  // if (EcalRecHitsEB.isValid())
  //   theEBRecHits = EcalRecHitsEB.product();

  // edm::Handle<CaloParticleCollection> caloHandle;
  // iEvent.getByToken(caloToken, caloHandle);

  edm::Handle<reco::GenParticleCollection> genParticles;
  iEvent.getByToken(genParticleToken, genParticles);

  // Match using indices
  for (const auto& cp : *caloParticles) {
      for (auto g4t = cp.g4Track_begin(); g4t != cp.g4Track_end(); ++g4t) {
          int g4TrackId = g4t->trackId();
          // Match this to genParticle using genParticleIndices
          if (g4t->genpartIndex() >= 0 && 
              g4t->genpartIndex() < (int)genParticles->size()) {
              const auto& matchedGenP = (*genParticles)[g4t->genpartIndex()];
              std::cout << "Matched GenParticle: " << matchedGenP.pdgId() << std::endl;
          }
      }
  }

  edm::Handle<SimTrackContainer> SimTk;
  iEvent.getByToken(SimTrackToken, SimTk);
  labelsForToken(SimTrackToken, l);

  std::vector<SimTrack> theSimTracks;
  if (SimTk.isValid())
    theSimTracks.insert(theSimTracks.end(), SimTk->begin(), SimTk->end());

  edm::Handle<SimVertexContainer> SimVtx;
  iEvent.getByToken(SimVertexToken, SimVtx);
  labelsForToken(SimVertexToken, l);

  std::vector<SimVertex> theSimVertexes;
  if (SimVtx.isValid())
    theSimVertexes.insert(theSimVertexes.end(), SimVtx->begin(), SimVtx->end());

  // Get HepMC product
  edm::Handle<edm::HepMCProduct> MCEvt;
  iEvent.getByToken(HepMCToken, MCEvt);
  
  // Check if valid
  if (!MCEvt.isValid()) {
      edm::LogWarning("TransClustering") 
          << "HepMCProduct not found!  Skipping HepMC processing.";
      return;
  }
    
  // Access GenEvent
  const HepMC::GenEvent* genEvent = MCEvt->GetEvent();
  if (!genEvent) {
      edm::LogWarning("TransClustering") 
          << "GenEvent is null! ";
      return;
  }
  
  // // Now you can access particles
  // for (HepMC::GenEvent::particle_const_iterator p = genEvent->particles_begin();
  //       p != genEvent->particles_end(); ++p) {
      
  //     double energy = (*p)->momentum().e();
  //     double eta = (*p)->momentum().eta();
  //     double phi = (*p)->momentum().phi();
  //     int pdgId = (*p)->pdg_id();
      
  //     // Your clustering logic here using gen particle info
  //     std::cout
  //         << "Gen particle: PDG=" << pdgId 
  //         << " E=" << energy 
  //         << " eta=" << eta 
  //         << " phi=" << phi << std::endl;
  // }

  // ***************** Loop over the GEN particles *****************
  std::cout << "GenParticles" << std::endl;
  for (const auto& genParticle : *genParticles) {
    // Access properties:
    int pdgId = genParticle.pdgId();
    double pt = genParticle.pt();
    double eta = genParticle.eta();
    double phi = genParticle.phi();
    double energy = genParticle.energy();
    //int status = genParticle.status();
    
    // Access vertex information:
    double vx = genParticle.vx();
    double vy = genParticle.vy();
    double vz = genParticle.vz();
    std::cout << " GenParticle PDG ID: " << pdgId 
              << ", pT: " << pt << ", eta: " << eta << ", phi: " << phi << ", energy: " << energy 
              << ", vertex: (" << vx << ", " << vy << ", " << vz << ")" << std::endl;
    caloEvent.push_back(iEvent.id().event());
    caloPDG.push_back(pdgId);
    caloSourceX.push_back(vx);
    caloSourceY.push_back(vy);
    caloSourceZ.push_back(vz);
    caloEta.push_back(eta);
    caloPhi.push_back(phi);
    caloPt.push_back(pt);
    caloE.push_back(energy);
  }
  
  int nsimhits = 0;
  for (const auto& cp : *caloParticles) {
    nsimhits = 0;
    // Access calo particle properties
    double energy = cp.energy();
    double eta = cp.eta();
    double phi = cp.phi();
    
    std::cout << "CaloParticle ID " << cp.pdgId() << ", energy = " << energy << ", eta = " << eta << ", phi = " << phi << std::endl;
    
    // Access sim clusters associated with this calo particle
    const auto& simClusters = cp.simClusters();
    std::cout << "CaloParticle has " << simClusters.size() << " associated sim clusters." << std::endl;
    for (const auto& simCluster : simClusters) {
      // Process sim clusters
      //double clusterEnergy = simCluster->energy();
      // ... additional processing
      std::cout << *simCluster << std::endl;

      // for (auto g4Track = simCluster->g4Track_begin(); g4Track != simCluster->g4Track_end(); ++g4Track) {
      //   std::cout << "    G4Track: type=" << g4Track->type() 
      //                   << " E=" << g4Track->momentum().E() << std::endl;
      // }

      // Get the first sim hit (in time)
      const auto& hitAndFractions = simCluster->hits_and_fractions();
      for (const auto& hitAndFraction : hitAndFractions) {
          DetId hitId = hitAndFraction.first;
          EBDetId ebid(hitId);
          std::cout << "    SimHit ieta: " << ebid.ieta() << ", iphi: " << ebid.iphi() << std::endl;
          //float fraction = hitAndFraction.second;
          nsimhits++;
      }
    }
    std::cout << "CaloParticle has " << nsimhits << " associated sim hits." << std::endl;
  }

  fillMcTruth(theSimTracks, theSimVertexes);
  // print geantToIndex_ map
  std::cout << "Geant to Index Map:" << std::endl;
  for (const auto& entry : geantToIndex_) {
      std::cout << "  Geant ID: " << entry.first << " -> Index: " << entry.second << std::endl;
  }
  std::vector<EcalSimPhotonMCTruth> photons = findMcTruth(theSimTracks, theSimVertexes);

  // ***************** Loop over MC truth photons *****************
  int nMCphotons = 0;
  for (unsigned int ipho = 0; ipho < photons.size(); ipho++) {
    math::XYZTLorentzVectorD vtx = photons[ipho].primaryVertex();
    double phiTrue = photons[ipho].fourMomentum().phi();
    double vtxPerp = sqrt(vtx.x() * vtx.x() + vtx.y() * vtx.y());
    double etaTrue = ecalEta(photons[ipho].fourMomentum().eta(), vtx.z(), vtxPerp);
    double ptTrue = photons[ipho].fourMomentum().e() / cosh(etaTrue);
    double enTrue = photons[ipho].fourMomentum().e();
    if (std::fabs(etaTrue) < 1.479) {
      nMCphotons++;
      std::cout << " MC Photon " << nMCphotons << ": E = " << enTrue << ", pT = " << ptTrue
                << ", eta = " << etaTrue << ", phi = " << phiTrue << ", isConverted = " << photons[ipho].isAConversion()
                << ", vtx = (" << vtx.x() << ", " << vtx.y() << ", " << vtx.z() << ")"
                << std::endl;
    }
  }

  // **************** Loop over the EB REC hits ****************
  MapType recMap;
  double EBEnergy_ = 0.;
  uint32_t nEBHits = 0;

  // std::cout << "theEBCaloHits" << std::endl;
  // for (const PCaloHit& cp : *EcalHitsEB) {
  //    std::cout << cp << std::endl;
  // }

  // for (const EcalRecHit& irec : theEBRecHits) {
  //   //std::cout << EBDetId(irec.detid()) << ": " << irec.energy() << " GeV, " << irec.time() << " ns" << std::endl;
    
  //   EBDetId ebid(irec.detid());
  //   int ieta = ebid.ieta();
  //   int iphi = ebid.iphi();
  //   recMap[std::make_pair(ieta, iphi)] += irec.energy();
    
  //   EBEnergy_ += irec.energy();
  //   nEBHits++;
  // }

  for (EcalUncalibratedRecHitCollection::const_iterator uncalibRecHit = EBUncalibRecHit->begin();
         uncalibRecHit != EBUncalibRecHit->end();
         ++uncalibRecHit) {
      EBDetId ebid = EBDetId(uncalibRecHit->id());

      // Find corresponding recHit
      EcalRecHitCollection::const_iterator myRecHit = EBRecHit->find(ebid);
      if (myRecHit == EBRecHit->end())
        continue;
      int ieta = ebid.ieta();
      int iphi = ebid.iphi();
      recMap[std::make_pair(ieta, iphi)] += myRecHit->energy();
      EBEnergy_ += myRecHit->energy();
      nEBHits++;
  }
  std::cout << "Number of EB rec hits: " << nEBHits << std::endl;
  std::cout << "Total EB rec energy: " << EBEnergy_ << std::endl;

  for (const auto& [key, val] : recMap) {
    recIEta.push_back(key.first);
    recIPhi.push_back(key.second);
    recValues.push_back(val);
    recoEvent.push_back(iEvent.id().event());
  }

  // **************** Loop over the EB SIM hits ****************

  std::map<unsigned int, std::vector<PCaloHit *>, std::less<unsigned int>> CaloHitMap;
  MapType simMap;
  EBEnergy_ = 0.;
  nEBHits = 0;

  for (std::vector<PCaloHit>::iterator isim = theEBCaloHits.begin(); isim != theEBCaloHits.end(); ++isim) {
    if (isim->time() > 500.) {
      continue;
    }

    CaloHitMap[isim->id()].push_back(&(*isim));

    EBDetId ebid(isim->id());

    // std::cout << " CaloHit " << isim->getName() << "\n"
    //           << " DetID = " << isim->id() << " EBDetId = " << ebid.ieta() << " " << ebid.iphi() << "\n"
    //           << " Time = " << isim->time() << "\n"
    //           << " Track Id = " << isim->geantTrackId() << "\n"
    //           << " Energy = " << isim->energy() << std::endl;

    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    simMap[std::make_pair(ieta, iphi)] += isim->energy();

    simTrackId.push_back(isim->geantTrackId());
    simPDG.push_back(isim->getName());
    simT.push_back(isim->time());
    simEta.push_back(ieta);
    simPhi.push_back(iphi);
    simEvent.push_back(iEvent.id().event());

    EBEnergy_ += isim->energy();
    nEBHits++;
  }
  std::cout << "Number of EB sim hits: " << nEBHits << std::endl;
  std::cout << "Total EB sim energy: " << EBEnergy_ << std::endl;

  for (const auto& [key, val] : simMap) {
    mapIEta.push_back(key.first);
    mapIPhi.push_back(key.second);
    mapValues.push_back(val);
    simEvent.push_back(iEvent.id().event());
  }
} // --- end of analyze

float TransClustering::ecalEta(float EtaParticle, float Zvertex, float plane_Radius) {
  const float R_ECAL = 136.5;
  const float Z_Endcap = 328.0;
  const float etaBarrelEndcap = 1.479;

  if (EtaParticle != 0.) {
    float Theta = 0.0;
    float ZEcal = (R_ECAL - plane_Radius) * sinh(EtaParticle) + Zvertex;

    if (ZEcal != 0.0)
      Theta = atan(R_ECAL / ZEcal);
    if (Theta < 0.0)
      Theta = Theta + Geom::pi();

    float ETA = -log(tan(0.5 * Theta));

    if (fabs(ETA) > etaBarrelEndcap) {
      float Zend = Z_Endcap;
      if (EtaParticle < 0.0)
        Zend = -Zend;
      float Zlen = Zend - Zvertex;
      float RR = Zlen / sinh(EtaParticle);
      Theta = atan((RR + plane_Radius) / Zend);
      if (Theta < 0.0)
        Theta = Theta + Geom::pi();
      ETA = -log(tan(0.5 * Theta));
    }

    return ETA;
  } else {
    std::cout << "[TransClustering::ecalEta] Warning: Eta "
                      "equals to zero, not correcting";
    return EtaParticle;
  }
}

// ------------ method for histogram booking ------------
void TransClustering::bookHistograms(DQMStore::IBooker& ibook, edm::Run const& run, edm::EventSetup const& iSetup) {
  ibook.setCurrentFolder("EcalHitsV/EcalSimHitsValidation");

  // Create a file with jobId
  std::stringstream ss;
  ss << "ecal_" << jobId << ".root";
  std::string filename = ss.str();
  myFile = new TFile(filename.c_str(), "RECREATE");
  
  simTree = new TTree("simTree", "A tree with simulation hit information");
  simTree->Branch("pdg",      &simPDG);
  simTree->Branch("time",     &simT);
  simTree->Branch("energy",   &simE);
  simTree->Branch("phi",      &simPhi);
  simTree->Branch("eta",      &simEta);
  simTree->Branch("event",    &simEvent);
  simTree->Branch("trackId",  &simTrackId);

  simTree->Branch("mapIEta", &mapIEta);
  simTree->Branch("mapIPhi", &mapIPhi);
  simTree->Branch("mapValues", &mapValues);

  recoTree = new TTree("recoTree", "A tree with reconstructed hit information");
  recoTree->Branch("time",    &recoT);
  recoTree->Branch("energy",  &recoE);
  recoTree->Branch("local_x", &recoLocX);
  recoTree->Branch("local_y", &recoLocY);
  recoTree->Branch("local_z", &recoLocZ);
  recoTree->Branch("theta",   &recoTheta);
  recoTree->Branch("phi",     &recoPhi);
  recoTree->Branch("eta",     &recoEta);
  recoTree->Branch("x",       &recoX);
  recoTree->Branch("y",       &recoY);
  recoTree->Branch("z",       &recoZ);
  recoTree->Branch("ID",      &recoID);
  recoTree->Branch("event",   &recoEvent);

  recoTree->Branch("mapIEta", &recIEta);
  recoTree->Branch("mapIPhi", &recIPhi);
  recoTree->Branch("mapValues", &recValues);

  caloTree = new TTree("caloTree", "A tree with calo hit information");
  caloTree->Branch("time",    &caloT);
  caloTree->Branch("energy",  &caloE);
  caloTree->Branch("pt",  &caloPt);
  // caloTree->Branch("theta",   &caloTheta);
  caloTree->Branch("phi",     &caloPhi);
  caloTree->Branch("eta",     &caloEta);
  // caloTree->Branch("x",       &caloX);
  // caloTree->Branch("y",       &caloY);
  // caloTree->Branch("z",       &caloZ);
  caloTree->Branch("pdg",     &caloPDG);
  caloTree->Branch("event",   &caloEvent);
  // caloTree->Branch("trackId", &caloTrackId);
  // caloTree->Branch("decay",   &caloDecay);
  caloTree->Branch("parentX", &caloSourceX);
  caloTree->Branch("parentY", &caloSourceY);
  caloTree->Branch("parentZ", &caloSourceZ);
  // caloTree->Branch("parentT", &caloSourceT);
}

// taken from an old version of RecoEgamma/EgammaMCTools/src/PhotonMCTruthFinder
std::vector<EcalSimPhotonMCTruth> TransClustering::findMcTruth(std::vector<SimTrack> &theSimTracks, std::vector<SimVertex> &theSimVertices) {
  std::vector<EcalSimPhotonMCTruth> result;

  if (theSimTracks.empty() || theSimVertices.empty()) {
    return result;
  }

  geantToIndex_.clear();

  const int ELECTRON_FLAV = 1;
  const int PIZERO_FLAV = 2;
  const int PHOTON_FLAV = 3;

  int ievflav = 0;
  std::vector<SimTrack *> photonTracks;
  std::vector<SimTrack *> pizeroTracks;
  std::vector<const SimTrack *> trkFromConversion;
  SimVertex primVtx;
  std::vector<int> convInd;

  fillMcTruth(theSimTracks, theSimVertices);
  int iPV = -1;
  int partType1 = 0;
  int partType2 = 0;
  std::vector<SimTrack>::iterator iFirstSimTk = theSimTracks.begin();
  if (!(*iFirstSimTk).noVertex()) {
    iPV = (*iFirstSimTk).vertIndex();
    int vtxId = (*iFirstSimTk).vertIndex();
    primVtx = theSimVertices[vtxId];
    partType1 = (*iFirstSimTk).type();
  }

  // Look at a second track
  iFirstSimTk++;
  if (iFirstSimTk != theSimTracks.end()) {
    if ((*iFirstSimTk).vertIndex() == iPV) {
      partType2 = (*iFirstSimTk).type();
    }
  }
  int npv = 0;
  int iPho = 0;
  for (std::vector<SimTrack>::iterator iSimTk = theSimTracks.begin(); iSimTk != theSimTracks.end(); ++iSimTk) {
    if ((*iSimTk).noVertex())
      continue;
    if ((*iSimTk).vertIndex() == iPV) {
      npv++;
      if ((*iSimTk).type() == 22) {
        convInd.push_back(0);
        photonTracks.push_back(&(*iSimTk));
      }
    }
  }

  if (npv > 4) {
  } else if (npv == 1) {
    if (abs(partType1) == 11) {
      ievflav = ELECTRON_FLAV;
    } else if (partType1 == 111) {
      ievflav = PIZERO_FLAV;
    } else if (partType1 == 22) {
      ievflav = PHOTON_FLAV;
    }
  } else if (npv == 2) {
    if (abs(partType1) == 11 && abs(partType2) == 11) {
      ievflav = ELECTRON_FLAV;
    } else if (partType1 == 111 && partType2 == 111) {
      ievflav = PIZERO_FLAV;
    } else if (partType1 == 22 && partType2 == 22) {
      ievflav = PHOTON_FLAV;
    }
  }

  //  Look into converted photons
  int isAconversion = 0;
  if (ievflav == PHOTON_FLAV) {
    int nConv = 0;
    iPho = 0;
    for (std::vector<SimTrack *>::iterator iPhoTk = photonTracks.begin(); iPhoTk != photonTracks.end(); ++iPhoTk) {
      trkFromConversion.clear();
      for (std::vector<SimTrack>::iterator iSimTk = theSimTracks.begin(); iSimTk != theSimTracks.end(); ++iSimTk) {
        if ((*iSimTk).noVertex())
          continue;
        if ((*iSimTk).vertIndex() == iPV)
          continue;
        if (abs((*iSimTk).type()) != 11)
          continue;
        int vertexId = (*iSimTk).vertIndex();
        SimVertex vertex = theSimVertices[vertexId];
        int motherId = -1;
        if (vertex.parentIndex()) {
          unsigned motherGeantId = vertex.parentIndex();
          std::map<unsigned, unsigned>::iterator association = geantToIndex_.find(motherGeantId);
          if (association != geantToIndex_.end())
            motherId = association->second;

          if (theSimTracks[motherId].trackId() == (*iPhoTk)->trackId()) {
            trkFromConversion.push_back(&(*iSimTk));
          }
        }
      }

      if (!trkFromConversion.empty()) {
        isAconversion = 1;
        nConv++;
        convInd[iPho] = nConv;
        int convVtxId = trkFromConversion[0]->vertIndex();
        SimVertex convVtx = theSimVertices[convVtxId];
        const math::XYZTLorentzVectorD &vtxPosition = convVtx.position();

        result.push_back(EcalSimPhotonMCTruth(isAconversion,
                                              (*iPhoTk)->momentum(),
                                              vtxPosition.pt(),
                                              vtxPosition.z(),
                                              vtxPosition,
                                              primVtx.position(),
                                              trkFromConversion));
      } else {
        isAconversion = 0;
        math::XYZTLorentzVectorD vtxPosition(0., 0., 0., 0.);
        result.push_back(EcalSimPhotonMCTruth(isAconversion,
                                              (*iPhoTk)->momentum(),
                                              vtxPosition.pt(),
                                              vtxPosition.z(),
                                              vtxPosition,
                                              primVtx.position(),
                                              trkFromConversion));
      }
      iPho++;
    }
  }

  return result;
}

void TransClustering::fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices) {
  unsigned nVtx = simVertices.size();
  unsigned nTks = simTracks.size();
  if (nVtx == 0)
    return;
  for (unsigned it = 0; it < nTks; ++it) {
    geantToIndex_[simTracks[it].trackId()] = it;
  }
}
