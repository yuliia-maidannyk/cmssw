#include "DataFormats/Math/interface/GeantUnits.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Geometry/EcalAlgo/interface/EcalBarrelGeometry.h"
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"

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
  ValidationCollectionToken = consumes<PEcalValidInfo>(edm::InputTag(std::string(g4InfoLabel), std::string(ValidationCollection)));
  genParticleToken = consumes<reco::GenParticleCollection>(iConfig.getParameter<edm::InputTag>("genParticles"));
  SimTrackToken = consumes<edm::SimTrackContainer>(iConfig.getParameter<edm::InputTag>("simTrackCollection"));
  SimVertexToken = consumes<edm::SimVertexContainer>(iConfig.getParameter<edm::InputTag>("simVertexCollection"));
  CaloParticle_Token = consumes<CaloParticleCollection>(iConfig.getParameter<edm::InputTag>("CaloParticleCollection"));
  HepMCToken = consumes<edm::HepMCProduct>(iConfig.getParameter<edm::InputTag>("HepMCProductLabel"));
  pfClusterToken = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECAL"));
  barrelGeomToken = esConsumes<CaloSubdetectorGeometry, EcalBarrelGeometryRecord>(edm::ESInputTag("", "EcalBarrel"));
  ecalGeomToken = esConsumes<CaloGeometry, CaloGeometryRecord>();
}

TransClustering::~TransClustering() {
}

// ------------ method called for each event  ------------
void TransClustering::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  using namespace std;
  using namespace geant_units::operators;

  clearEventData();

  static unsigned long eventCount = 0;
  ++eventCount;
  if (eventCount % 100 == 0) {
    std::cout << "Processed " << eventCount << " events" << std::endl;
  }


  // ***************** Get the Ecal Barrel Geometry *****************

  const CaloGeometry& geo = iSetup.getData(ecalGeomToken);
  const CaloSubdetectorGeometry& barrelGeom = iSetup.getData(barrelGeomToken);

  // ***************** Get the collections *****************

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
      std::cout << "Found SimTrack photon: trackId=" << simTk.trackId() 
                << " eta=" << simTk.momentum().eta() 
                << " phi=" << simTk.momentum().phi() 
                << " E=" << simTk.momentum().E() << std::endl;
    }
  }
  std::cout << "Total SimTrack photons from primary vertex: " << photonTracks.size() << std::endl;

  // For each photon, look for conversion electrons
  for (auto* phoTk : photonTracks) {
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
            const math::XYZTLorentzVectorD &vtxPosition = vertex.position();
            convR = vtxPosition.pt();
            convZ = vtxPosition.z();
            break;
          }
        }
      }
    }
    
    photonConversionInfo[phoTk->trackId()] = std::make_pair(convR, convZ);
    std::cout << "  Photon trackId=" << phoTk->trackId() << " conversion: R=" << convR << ", Z=" << convZ << std::endl;
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
      // Match GenParticle photon to SimTrack photon by kinematic proximity
      float minDR = 0.1;  // dR matching threshold
      unsigned bestTrackId = 0;
      
      for (auto* phoTk : photonTracks) {
        // Calculate dR between GenParticle and SimTrack photon
        float trackEta = phoTk->momentum().eta();
        float trackPhi = phoTk->momentum().phi();
        float dEta = eta - trackEta;
        float dPhi = phi - trackPhi;
        // Wrap dphi to [-pi, pi]
        while (dPhi > M_PI) dPhi -= 2*M_PI;
        while (dPhi < -M_PI) dPhi += 2*M_PI;
        float dR = sqrt(dEta*dEta + dPhi*dPhi);
        
        std::cout << "    Matching: GenPhoton(eta=" << eta << ",phi=" << phi 
                  << ") vs SimTrack " << phoTk->trackId() << "(eta=" << trackEta 
                  << ",phi=" << trackPhi << ") dR=" << dR << std::endl;
        
        if (dR < minDR) {
          minDR = dR;
          bestTrackId = phoTk->trackId();
        }
      }
      std::cout << "  Best match: trackId=" << bestTrackId << " with dR=" << minDR << std::endl;
      
      // Now check if the matched photon has conversion info
      if (bestTrackId > 0 && photonConversionInfo.find(bestTrackId) != photonConversionInfo.end()) {
        const auto& convInfo = photonConversionInfo[bestTrackId];
        if ((convInfo.first > 0 || convInfo.second != 0)) {
          convR = convInfo.first;
          convZ = convInfo.second;
          if (convInfo.first < 129) {isConverted = 1;}
          std::cout << "  -> Converted at R=" << convR << ", Z=" << convZ << std::endl;
        }
      }
    }

    // Calculate position on ECAL surface
    double ECAL_RADIUS = 129.0; // cm

    double theta = 2.0 * std::atan(std::exp(-eta));
    double x = ECAL_RADIUS * std::cos(phi);
    double y = ECAL_RADIUS * std::sin(phi);
    double z = ECAL_RADIUS / std::tan(theta);

    GlobalPoint ecalPoint(x, y, z);
    DetId closestCell = barrelGeom.getClosestCell(ecalPoint);
    EBDetId cpEBid(closestCell);

    std::cout << "  GenParticle momentum points to: ieta=" << cpEBid.ieta() 
              << " iphi=" << cpEBid.iphi() << std::endl;
    
    genEvent.push_back(iEvent.id().event());
    genPDG.push_back(pdgId);
    genSourceX.push_back(vx);
    genSourceY.push_back(vy);
    genSourceZ.push_back(vz);
    genPEta.push_back(eta);
    genPPhi.push_back(phi);
    genPPt.push_back(pt);
    genEta.push_back(cpEBid.ieta());
    genPhi.push_back(cpEBid.iphi());
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
      caloPEta.push_back(sc->eta());
      caloPPhi.push_back(sc->phi());
      caloPPt.push_back(sc->pt());
      caloTrackId.push_back(sc->g4Track_begin()->trackId());
      caloEvent.push_back(iEvent.id().event());

      // Calculate position on ECAL surface
      double cpEta = sc->eta();
      double cpPhi = sc->phi();
      double ECAL_RADIUS = 129.0; // cm

      double theta = 2.0 * std::atan(std::exp(-cpEta));
      double x = ECAL_RADIUS * std::cos(cpPhi);
      double y = ECAL_RADIUS * std::sin(cpPhi);
      double z = ECAL_RADIUS / std::tan(theta);

      GlobalPoint ecalPoint(x, y, z);
      DetId closestCell = barrelGeom.getClosestCell(ecalPoint);
      EBDetId cpEBid(closestCell);

      std::cout << "  CaloParticle momentum points to: ieta=" << cpEBid.ieta() 
                << " iphi=" << cpEBid.iphi() << std::endl;

      caloEta.push_back(cpEBid.ieta());
      caloPhi.push_back(cpEBid.iphi());

      // Get the sim hits associated with this sim cluster
        const auto& hitAndEnergies = sc->hits_and_energies();
        for (const auto& hitAndEnergy : hitAndEnergies) {
          DetId hitId = hitAndEnergy.first;
          EBDetId ebid(hitId);
          //std::cout << "    SimHit ieta: " << ebid.ieta() << ", iphi: " << ebid.iphi() << std::endl;
          // Store all (sc_number, simhit energy) associated with this (ieta, iphi) pair
          float simHitEnergy = hitAndEnergy.second;
          caloMap[std::make_pair(ebid.ieta(), ebid.iphi())].push_back(std::make_pair(sc_number, simHitEnergy));
          nsimhits++;
        }
      sc_number++;
    }
    std::cout << "CaloParticle has " << nsimhits << " associated sim hits." << std::endl;
  }

  for (const auto& [key, scNumbers] : caloMap) {
    for (const auto& scNumAndE : scNumbers) {
      caloIEta.push_back(key.first);
      caloIPhi.push_back(key.second);
      caloValues.push_back(scNumAndE.first);
      caloValuesE.push_back(scNumAndE.second);
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

  for (const auto& [key, val] : simMap) {
    simIEta.push_back(key.first);
    simIPhi.push_back(key.second);
    simValues.push_back(val);
    simSubEvent.push_back(iEvent.id().event());
  }

  // **************** Loop over the EB REC hits ****************

  MapType recoMap;
  nEBHits = 0;

  for (EcalRecHitCollection::const_iterator recHit = EBRecHit->begin(); recHit != EBRecHit->end(); ++recHit) {
    EBDetId ebid = EBDetId(recHit->id());
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    recoMap[std::make_pair(ieta, iphi)] += recHit->energy();
    nEBHits++;
  }
  std::cout << "Number of EB rec hits: " << nEBHits << std::endl;

  for (const auto& [key, val] : recoMap) {
    recoIEta.push_back(key.first);
    recoIPhi.push_back(key.second);
    recoValues.push_back(val);
    recoEvent.push_back(iEvent.id().event());
  }

  // **************** Loop over the PFClusters ****************

  for (const auto& pf : *pfClusters)
  {
    //DetId ebid = pf.seed();
    //EBDetId ebdetid(ebid);
    pfEvent.push_back(iEvent.id().event());
    pfE.push_back(pf.correctedEnergy());

    GlobalPoint gp(pf.position().x(), pf.position().y(), pf.position().z());
    DetId closestCell = barrelGeom.getClosestCell(gp);
    EBDetId ebid(closestCell);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();

    // double exact_eta = gp.eta();
    // double exact_phi = gp.phi().value();
    
    // // Get crystal center position
    // GlobalPoint cellCenter = barrelGeom.getGeometry(closestCell)->getPosition();

    // // Calculate fractional offset from crystal center
    // double deltaEta = exact_eta - cellCenter.eta();
    // double deltaPhi = exact_phi - cellCenter.phi().value();
    
    // // Wrap deltaPhi to [-π, π]
    // while (deltaPhi > M_PI) deltaPhi -= 2*M_PI;
    // while (deltaPhi < -M_PI) deltaPhi += 2*M_PI;

    // double ieta_fractional = ieta + deltaEta / 0.0174;
    // double iphi_fractional = iphi + deltaPhi / (2.0 * M_PI / 360.0);

    pfEta.push_back(ieta);
    pfPhi.push_back(iphi);

    std::cout << " PFCluster E=" << pf.correctedEnergy() << " at (" << ieta << ", " << iphi << ")" << std::endl;
  }

  simTree->Fill();
  recoTree->Fill();
  caloTree->Fill();
  genTree->Fill();
  pfTree->Fill();

} // --- end of analyze

void TransClustering::clearEventData() {
  simPDG.clear();
  simT.clear();
  simE.clear();
  simPhi.clear();
  simEta.clear();
  simZ.clear();
  simEvent.clear();
  simSubEvent.clear();
  simTrackId.clear();
  simIEta.clear();
  simIPhi.clear();
  simValues.clear();

  recoT.clear();
  recoE.clear();
  recoPhi.clear();
  recoEta.clear();
  recoEvent.clear();
  recoID.clear();
  recoIEta.clear();
  recoIPhi.clear();
  recoValues.clear();

  caloT.clear();
  caloE.clear();
  caloPPt.clear();
  caloPPhi.clear();
  caloPEta.clear();
  caloEta.clear();
  caloPhi.clear();
  caloPDG.clear();
  caloEvent.clear();
  caloSubEvent.clear();
  caloIEta.clear();
  caloIPhi.clear();
  caloValues.clear();
  caloValuesE.clear();
  caloTrackId.clear();

  genT.clear();
  genE.clear();
  genPPt.clear();
  genPPhi.clear();
  genPEta.clear();
  genEta.clear();
  genPhi.clear();
  genPDG.clear();
  genEvent.clear();
  genSourceX.clear();
  genSourceY.clear();
  genSourceZ.clear();
  genIsConverted.clear();
  genConvR.clear();
  genConvZ.clear();

  pfEvent.clear();
  pfPhi.clear();
  pfEta.clear();
  pfE.clear();
}

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

  edm::Service<TFileService> fs;

  simTree = fs->make<TTree>("simTree", "A tree with simulation hit information");
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

  recoTree = fs->make<TTree>("recoTree", "A tree with reconstructed hit information");
  recoTree->Branch("time",      &recoT);
  recoTree->Branch("energy",    &recoE);
  recoTree->Branch("phi",       &recoPhi);
  recoTree->Branch("eta",       &recoEta);
  recoTree->Branch("event",     &recoEvent);
  recoTree->Branch("mapIEta",   &recoIEta);
  recoTree->Branch("mapIPhi",   &recoIPhi);
  recoTree->Branch("mapValues", &recoValues);

  caloTree = fs->make<TTree>("caloTree", "A tree with calo hit information");
  caloTree->Branch("time",      &caloT);
  caloTree->Branch("energy",    &caloE);
  caloTree->Branch("pt",        &caloPPt);
  caloTree->Branch("phi",       &caloPPhi);
  caloTree->Branch("eta",       &caloPEta);
  caloTree->Branch("iphi",      &caloPhi);
  caloTree->Branch("ieta",      &caloEta);
  caloTree->Branch("pdg",       &caloPDG);
  caloTree->Branch("event",     &caloEvent);
  caloTree->Branch("subEvent",  &caloSubEvent);
  caloTree->Branch("mapIEta",   &caloIEta);
  caloTree->Branch("mapIPhi",   &caloIPhi);
  caloTree->Branch("mapValues", &caloValues);
  caloTree->Branch("mapValuesE", &caloValuesE);
  caloTree->Branch("trackId",   &caloTrackId);

  genTree = fs->make<TTree>("genTree", "A tree with gen information");
  genTree->Branch("time",        &genT);
  genTree->Branch("energy",      &genE);
  genTree->Branch("pt",          &genPPt);
  genTree->Branch("phi",         &genPPhi);
  genTree->Branch("eta",         &genPEta);
  genTree->Branch("iphi",        &genPhi);
  genTree->Branch("ieta",        &genEta);
  genTree->Branch("pdg",         &genPDG);
  genTree->Branch("sourceX",     &genSourceX);
  genTree->Branch("sourceY",     &genSourceY);
  genTree->Branch("sourceZ",     &genSourceZ);
  genTree->Branch("event",       &genEvent);
  genTree->Branch("isConverted", &genIsConverted);
  genTree->Branch("convR",       &genConvR);
  genTree->Branch("convZ",       &genConvZ);

  pfTree = fs->make<TTree>("pfTree", "A tree with PFCluster information");
  pfTree->Branch("energy", &pfE);
  pfTree->Branch("eta",    &pfEta);
  pfTree->Branch("phi",    &pfPhi);
  pfTree->Branch("event",  &pfEvent);
}
