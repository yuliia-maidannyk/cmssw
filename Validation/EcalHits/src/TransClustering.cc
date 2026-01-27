// #include "DataFormats/Common/interface/ValidHandle.h"
#include "DataFormats/Math/interface/GeantUnits.h"
// #include "DataFormats/FTLRecHit/interface/FTLRecHitCollections.h"
// #include "DataFormats/FTLRecHit/interface/FTLClusterCollections.h"

// #include "SimDataFormats/CrossingFrame/interface/CrossingFrame.h"
// #include "SimDataFormats/CrossingFrame/interface/MixCollection.h"
// #include "SimDataFormats/TrackingHit/interface/PSimHit.h"
// #include "SimDataFormats/Vertex/interface/SimVertex.h"

#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>

#include "Validation/EcalHits/interface/TransClustering.h"

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
  pfClusterToken = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECAL"));
}

TransClustering::~TransClustering() {
  simTree->Fill();
  recoTree->Fill();
  caloTree->Fill();
  genTree->Fill();
  pfTree->Fill();
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

  edm::Handle<edm::PCaloHitContainer> EcalHitsEB;
  iEvent.getByToken(EBHitsToken, EcalHitsEB);

  std::vector<PCaloHit> theEBCaloHits;
  theEBCaloHits.insert(theEBCaloHits.end(), EcalHitsEB->begin(), EcalHitsEB->end());

  edm::Handle<CaloParticleCollection> caloParticles;
  iEvent.getByToken(CaloParticle_Token, caloParticles);

  edm::Handle<reco::GenParticleCollection> genParticles;
  iEvent.getByToken(genParticleToken, genParticles);

  const EBRecHitCollection *EBRecHit = nullptr;
  edm::Handle<EBRecHitCollection> EcalRecHitEB;
  iEvent.getByToken(EBrechitCollection_Token, EcalRecHitEB);
  if (EcalRecHitEB.isValid()) {
    EBRecHit = EcalRecHitEB.product();
  }

  edm::Handle<edm::HepMCProduct> MCEvt;
  iEvent.getByToken(HepMCToken, MCEvt);
  // if (!MCEvt.isValid()) {
  //   edm::LogWarning("TransClustering") << "HepMCProduct not found!  Skipping HepMC processing.";
  //   return;
  // } 
  // const HepMC::GenEvent* genEvent = MCEvt->GetEvent();
  // if (!genEvent) {
  //   edm::LogWarning("TransClustering") << "GenEvent is null! ";
  //   return;
  // }

  // Match using indices
  // for (const auto& cp : *caloParticles) {
  //     for (auto g4t = cp.g4Track_begin(); g4t != cp.g4Track_end(); ++g4t) {
  //         int g4TrackId = g4t->trackId();
  //         // Match this to genParticle using genParticleIndices
  //         if (g4t->genpartIndex() >= 0 && 
  //             g4t->genpartIndex() < (int)genParticles->size()) {
  //             const auto& matchedGenP = (*genParticles)[g4t->genpartIndex()];
  //             std::cout << "Matched GenParticle: " << matchedGenP.pdgId() << std::endl;
  //         }
  //     }
  // }
  edm::Handle<reco::PFClusterCollection> pfClusters;
  iEvent.getByToken(pfClusterToken, pfClusters);

  // *************** Loop over the HEP MC products *****************
  
  // for (HepMC::GenEvent::particle_const_iterator p = genEvent->particles_begin(); p != genEvent->particles_end(); ++p) {
  //     std::cout << "Gen particle: PDG=" << (*p)->pdg_id() << " E=" << (*p)->momentum().e() << " eta=" << (*p)->momentum().eta() << " phi=" << (*p)->momentum().phi() << std::endl;
  // }

  // ***************** Check for photon conversions *****************
  
  edm::Handle<edm::SimTrackContainer> simTracks;
  iEvent.getByToken(SimTrackToken, simTracks);

  edm::Handle<edm::SimVertexContainer> simVertexes;
  iEvent.getByToken(SimVertexToken, simVertexes);

  std::vector<SimTrack> theSimTracks;
  std::vector<SimVertex> theSimVertices;
  theSimTracks.insert(theSimTracks.end(), simTracks->begin(), simTracks->end());
  theSimVertices.insert(theSimVertices.end(), simVertexes->begin(), simVertexes->end());

  fillMcTruth(theSimTracks, theSimVertices);

  // Find primary vertex
  int iPV = -1;
  SimVertex primVtx;
  if (!theSimTracks.empty() && !theSimTracks[0].noVertex()) {
    iPV = theSimTracks[0].vertIndex();
    primVtx = theSimVertices[iPV];
  }

  // Map photon trackId to conversion info
  std::map<unsigned, std::pair<float, float>> photonConversionInfo; // trackId -> (R, Z)

  // Find photons from primary vertex
  std::vector<SimTrack *> photonTracks;
  for (auto& simTk : theSimTracks) {
    if (simTk.noVertex()) continue;
    if (simTk.vertIndex() == iPV && simTk.type() == 22) {
      photonTracks.push_back(&simTk);
    }
  }

  // For each photon, look for conversion electrons
  for (auto* phoTk : photonTracks) {
    //bool converted = false;
    float convR = 0., convZ = 0.;
    
    for (auto& simTk : theSimTracks) {
      if (simTk.noVertex()) continue;
      if (simTk.vertIndex() == iPV) continue;
      if (abs(simTk.type()) != 11) continue; // electrons only
      
      int vertexId = simTk.vertIndex();
      SimVertex vertex = theSimVertices[vertexId];
      
      if (vertex.parentIndex()) {
        unsigned motherGeantId = vertex.parentIndex();
        auto association = geantToIndex_.find(motherGeantId);
        if (association != geantToIndex_.end()) {
          int motherId = association->second;
          if (theSimTracks[motherId].trackId() == phoTk->trackId()) {
            // This electron came from our photon
            //converted = true;
            const math::XYZTLorentzVectorD &vtxPosition = vertex.position();
            convR = vtxPosition.pt();
            convZ = vtxPosition.z();
            break;
          }
        }
      }
    }
    
    photonConversionInfo[phoTk->trackId()] = std::make_pair(convR, convZ);
  }

  // ***************** Loop over the GEN particles *****************

  std::cout << "GenParticles" << std::endl;
  for (const auto& genParticle : *genParticles) {
    int pdgId     = genParticle.pdgId();
    float pt     = genParticle.pt();
    float eta    = genParticle.eta();
    float phi    = genParticle.phi();
    float energy = genParticle.energy();
    //int status = genParticle.status();
    
    // Access vertex information:
    float vx = genParticle.vx();
    float vy = genParticle.vy();
    float vz = genParticle.vz();
    std::cout << " GenParticle PDG ID: " << pdgId 
              << ", pT: " << pt << ", eta: " << eta << ", phi: " << phi << ", energy: " << energy 
              << ", vertex: (" << vx << ", " << vy << ", " << vz << ")" << std::endl;
    
    // Check if this photon converted (by matching to SimTracks from primary vertex)
    int isConverted = 0;
    float convR = 0., convZ = 0.;
    if (pdgId == 22 && iPV >= 0) {
      // Try to find matching SimTrack photon from primary vertex
      for (const auto& [trackId, convInfo] : photonConversionInfo) {
        // Check if R, Z are non-zero (indicates conversion)
        if (convInfo.first > 0 || convInfo.second != 0) {
          isConverted = 1;
          convR = convInfo.first;
          convZ = convInfo.second;
          std::cout << "  -> Converted at R=" << convR << ", Z=" << convZ << std::endl;
          break; // Take first converted photon
        }
      }
    }
    
    genEvent.push_back(iEvent.id().event());
    genT.push_back(0.0);  // genParticle doesn't have a time, so set to 0
    genPDG.push_back(pdgId);
    genSourceX.push_back(vx);
    genSourceY.push_back(vy);
    genSourceZ.push_back(vz);
    genEta.push_back(eta);
    genPhi.push_back(phi);
    genPt.push_back(pt);
    genE.push_back(energy);
    genIsConverted.push_back(isConverted);
    genConvR.push_back(convR);
    genConvZ.push_back(convZ);
  }
  
  // **************** Loop over the CaloParticles ****************

  int nsimhits = 0;
  CaloMapType caloMap;
  int sc_number = 0;
  for (const auto& cp : *caloParticles) {
    nsimhits = 0;
    
    std::cout << "GenParticle ID " << cp.pdgId() << ", energy = " << cp.energy() << ", eta = " << cp.eta() << ", phi = " << cp.phi() << std::endl;
    
    // Access sim clusters associated with this calo particle
    const auto& simClusters = cp.simClusters();
    std::cout << "GenParticle has " << simClusters.size() << " associated sim clusters." << std::endl;
    sc_number = 0;
    for (const auto& sc : simClusters) {

      std::cout << *sc << std::endl;

      caloPDG.push_back(sc->pdgId());
      caloE.push_back(sc->energy());
      caloEta.push_back(sc->eta());
      caloPhi.push_back(sc->phi());
      caloPt.push_back(sc->pt());
      caloTrackId.push_back(sc->g4Track_begin()->trackId());
      caloEvent.push_back(iEvent.id().event());

      // for (auto g4Track = simCluster->g4Track_begin(); g4Track != simCluster->g4Track_end(); ++g4Track) {
      //   std::cout << "    G4Track: type=" << g4Track->type() 
      //                   << " E=" << g4Track->momentum().E() << std::endl;
      // }

      // Get the sim hits associated with this sim cluster
      const auto& hitAndFractions = sc->hits_and_fractions();
      for (const auto& hitAndFraction : hitAndFractions) {
          DetId hitId = hitAndFraction.first;
          EBDetId ebid(hitId);
          std::cout << "    SimHit ieta: " << ebid.ieta() << ", iphi: " << ebid.iphi() << std::endl;
          // Store all sc_numbers associated with this (ieta, iphi) pair
          caloMap[std::make_pair(ebid.ieta(), ebid.iphi())].push_back(sc_number);
          nsimhits++;
      }
      sc_number++;
    }
    std::cout << "CaloParticle has " << nsimhits << " associated sim hits." << std::endl;
  }

  for (const auto& [key, scNumbers] : caloMap) {
    for (int scNum : scNumbers) {
      caloIEta.push_back(key.first);
      caloIPhi.push_back(key.second);
      caloValues.push_back(scNum);
      caloSubEvent.push_back(iEvent.id().event());
    }
  }

  // **************** Loop over the EB SIM hits ****************

  std::map<unsigned int, std::vector<PCaloHit *>, std::less<unsigned int>> CaloHitMap;
  MapType simMap;
  double EBEnergy_ = 0.;
  uint32_t nEBHits = 0;

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
    simT.push_back(isim->time());
    simEta.push_back(ieta);
    simPhi.push_back(iphi);
    simEvent.push_back(iEvent.id().event());

    EBEnergy_ += isim->energy();
    nEBHits++;
  }
  std::cout << "Number of EB sim hits: " << nEBHits << std::endl;
  std::cout << "Total EB sim energy: " << EBEnergy_ << std::endl;

  float max_energy = 0.;
  std::pair<int, int> max_pos;
  for (const auto& [key, val] : simMap) {
    simIEta.push_back(key.first);
    simIPhi.push_back(key.second);
    simValues.push_back(val);
    if (val > max_energy) {
      max_pos.first = key.first;
      max_pos.second = key.second;
    }
    max_energy = val;
    simSubEvent.push_back(iEvent.id().event());
  }

  // **************** Loop over the EB REC hits ****************

  MapType recoMap;
  EBEnergy_ = 0.;
  nEBHits = 0;

  for (EcalRecHitCollection::const_iterator recHit = EBRecHit->begin(); recHit != EBRecHit->end(); ++recHit) {
    EBDetId ebid = EBDetId(recHit->id());
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    recoMap[std::make_pair(ieta, iphi)] += recHit->energy();
    EBEnergy_ += recHit->energy();
    nEBHits++;
  }
  std::cout << "Number of EB rec hits: " << nEBHits << std::endl;
  // std::cout << "Total EB rec energy: " << EBEnergy_ << std::endl;

  for (const auto& [key, val] : recoMap) {
    recoIEta.push_back(key.first);
    recoIPhi.push_back(key.second);
    recoValues.push_back(val);
    recoEvent.push_back(iEvent.id().event());
  }

  // Sum rec hit energy in 7x7 region around the max_pos
  float recoE_7x7 = 0.;
  for (int dEta = -3; dEta <= 3; ++dEta) {
      int ieta = max_pos.first + dEta;
      // EB ieta ranges from -85 to 85, skipping 0
      if (ieta == 0 || ieta < -85 || ieta > 85) continue;
      for (int dPhi = -3; dPhi <= 3; ++dPhi) {
          int iphi = max_pos.second + dPhi;
          // Wrap iphi around [1,360]
          if (iphi < 1) iphi += 360;
          if (iphi > 360) iphi -= 360;
          auto it = recoMap.find({ieta, iphi});
          if (it != recoMap.end()) {
              recoE_7x7 += it->second;
          }
      }
  }
  std::cout << std::endl;
  std::cout << "RecHit energy in 7x7 region around the maximum: " << recoE_7x7 << std::endl;
  std::cout << std::endl;

  // **************** Loop over the PFClusters ****************

  for (const auto& pf : *pfClusters)
  {
    DetId ebid = pf.seed();
    EBDetId ebdetid(ebid);
    pfEvent.push_back(iEvent.id().event());
    pfEta.push_back(ebdetid.ieta());
    pfPhi.push_back(ebdetid.iphi());
    pfE.push_back(pf.correctedEnergy());
    std::cout << " PFCluster E=" << pf.correctedEnergy() << " at (" 
    << ebdetid.ieta() << ", " << ebdetid.iphi() << ")" << std::endl;
  }

} // --- end of analyze

// ------------ helper method for photon conversion tracking ------------
void TransClustering::fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices) {
  geantToIndex_.clear();
  unsigned nTks = simTracks.size();
  if (simVertices.empty()) return;
  
  // Create a map associating geant particle id and position in the SimTrack vector
  for (unsigned it = 0; it < nTks; ++it) {
    geantToIndex_[simTracks[it].trackId()] = it;
  }
}

// ------------ method for writing to output file ------------
void TransClustering::bookHistograms(DQMStore::IBooker& ibook, edm::Run const& run, edm::EventSetup const& iSetup) {
  ibook.setCurrentFolder("EcalHitsV/EcalSimHitsValidation");

  // Create a file with jobId
  std::stringstream ss;
  ss << "ecal_" << jobId << ".root";
  std::string filename = ss.str();
  myFile = new TFile(filename.c_str(), "RECREATE");
  
  simTree = new TTree("simTree", "A tree with simulation hit information");
  simTree->Branch("pdg",        &simPDG);
  simTree->Branch("time",       &simT);
  simTree->Branch("energy",     &simE);
  simTree->Branch("phi",        &simPhi);
  simTree->Branch("eta",        &simEta);
  simTree->Branch("event",      &simEvent);
  simTree->Branch("trackId",    &simTrackId);
  simTree->Branch("subEvent",   &simSubEvent);
  simTree->Branch("mapIEta",    &simIEta);
  simTree->Branch("mapIPhi",    &simIPhi);
  simTree->Branch("mapValues",  &simValues);

  recoTree = new TTree("recoTree", "A tree with reconstructed hit information");
  recoTree->Branch("time",      &recoT);
  recoTree->Branch("energy",    &recoE);
  recoTree->Branch("phi",       &recoPhi);
  recoTree->Branch("eta",       &recoEta);
  recoTree->Branch("event",     &recoEvent);
  recoTree->Branch("mapIEta",   &recoIEta);
  recoTree->Branch("mapIPhi",   &recoIPhi);
  recoTree->Branch("mapValues", &recoValues);

  caloTree = new TTree("caloTree", "A tree with calo hit information");
  caloTree->Branch("time",      &caloT);
  caloTree->Branch("energy",    &caloE);
  caloTree->Branch("pt",        &caloPt);
  caloTree->Branch("phi",       &caloPhi);
  caloTree->Branch("eta",       &caloEta);
  caloTree->Branch("pdg",       &caloPDG);
  caloTree->Branch("event",     &caloEvent);
  caloTree->Branch("subEvent",  &caloSubEvent);
  caloTree->Branch("mapIEta",   &caloIEta);
  caloTree->Branch("mapIPhi",   &caloIPhi);
  caloTree->Branch("mapValues", &caloValues);
  caloTree->Branch("trackId",   &caloTrackId);

  genTree = new TTree("genTree", "A tree with gen information");
  genTree->Branch("time",        &genT);
  genTree->Branch("energy",      &genE);
  genTree->Branch("pt",          &genPt);
  genTree->Branch("phi",         &genPhi);
  genTree->Branch("eta",         &genEta);
  genTree->Branch("pdg",         &genPDG);
  genTree->Branch("sourceX",     &genSourceX);
  genTree->Branch("sourceY",     &genSourceY);
  genTree->Branch("sourceZ",     &genSourceZ);
  genTree->Branch("event",       &genEvent);
  genTree->Branch("isConverted", &genIsConverted);
  genTree->Branch("convR",       &genConvR);
  genTree->Branch("convZ",       &genConvZ);

  pfTree = new TTree("pfTree", "A tree with PFCluster information");
  pfTree->Branch("energy", &pfE);
  pfTree->Branch("eta",    &pfEta);
  pfTree->Branch("phi",    &pfPhi);
  pfTree->Branch("event",  &pfEvent);
}
