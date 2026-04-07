#include "DataFormats/Math/interface/GeantUnits.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Geometry/EcalAlgo/interface/EcalBarrelGeometry.h"
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"
#include "Validation/EcalHits/interface/TransClustering.h"
#include "Validation/EcalHits/interface/PreProcessing.h"

#define INFER 0
#define PRINT_DEBUG 0

std::vector<float> build_energy_map(
    const std::vector<int>&   ieta_vec,
    const std::vector<int>&   iphi_vec,
    const std::vector<float>& energy_vec)
{
    std::vector<float> map(361 * 171, 0.f);
    for (size_t i = 0; i < ieta_vec.size(); ++i) {
        int col = ieta_vec[i] + 85;           // ieta → [0,170]
        int row = iphi_vec[i];                // iphi → [1,360]
        if (row < 0 || row >= 361) continue;
        if (col < 0 || col >= 171) continue;
        map[row * 171 + col] += energy_vec[i];
    }
    return map;
}

void apply_blackout(
    std::vector<float>&       map,
    const std::vector<int>&   seed_iphi,
    const std::vector<int>&   seed_ieta,
    const std::vector<bool>&  is_converted)
{
    const int pad = 10; // blackout pad size in ieta and iphi
    for (size_t j = 0; j < is_converted.size(); ++j) {
        if (!is_converted[j]) continue; // continue if it has not been converted
        int r0 = std::max(0, seed_iphi[j] - pad);
        int r1 = std::min(361, seed_iphi[j] + pad);
        int c0 = std::max(0, seed_ieta[j] - pad);
        int c1 = std::min(171, seed_ieta[j] + pad);
        for (int r = r0; r < r1; ++r)
            for (int c = c0; c < c1; ++c)
                map[r * 171 + c] = 0.f;
    }
}

// ------------ constructor and destructor --------------
TransClustering::TransClustering(const edm::ParameterSet& iConfig)
  : g4InfoLabel(iConfig.getParameter<std::string>("moduleLabelG4")),
    EBHitsCollection(iConfig.getParameter<std::string>("EBHitsCollection")),
    ValidationCollection(iConfig.getParameter<std::string>("ValidationCollection")),
    jobId(iConfig.getParameter<std::string>("jobId")),
    maskedEcalChannelStatusThreshold(iConfig.getParameter<int>("maskedEcalChannelStatusThreshold")),
    graphPath(iConfig.getParameter<std::string>("graphPath")),
    inputTensorName(iConfig.getParameter<std::string>("inputTensorName")),
    outputTensorName(iConfig.getParameter<std::string>("outputTensorName")),
    cropSize(iConfig.getParameter<int>("cropSize")),
    maxClusters(iConfig.getParameter<int>("maxClusters")),
    overlapLimit(iConfig.getParameter<int>("overlapLimit")),
    seedThreshold(iConfig.getParameter<double>("seedThreshold"))
{
  usesResource(TFileService::kSharedResource);

  EBrechitCollection_Token = consumes<EBRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBrechitCollection"));
  EBuncalibrechitCollection_Token = consumes<EBUncalibratedRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBuncalibrechitCollection"));
  EBHitsToken = consumes<edm::PCaloHitContainer>(edm::InputTag(std::string(g4InfoLabel), std::string(EBHitsCollection)));
  ValidationCollectionToken = consumes<PEcalValidInfo>(edm::InputTag(std::string(g4InfoLabel), std::string(ValidationCollection)));
  genParticleToken = consumes<reco::GenParticleCollection>(iConfig.getParameter<edm::InputTag>("genParticles"));
  SimTrackToken = consumes<edm::SimTrackContainer>(iConfig.getParameter<edm::InputTag>("simTrackCollection"));
  SimVertexToken = consumes<edm::SimVertexContainer>(iConfig.getParameter<edm::InputTag>("simVertexCollection"));
  CaloParticle_Token = consumes<CaloParticleCollection>(iConfig.getParameter<edm::InputTag>("CaloParticleCollection"));
  barrelGeomToken = esConsumes<CaloSubdetectorGeometry, EcalBarrelGeometryRecord>(edm::ESInputTag("", "EcalBarrel"));
  ecalGeomToken = esConsumes<CaloGeometry, CaloGeometryRecord>();
  ecalStatusToken = esConsumes<EcalChannelStatus, EcalChannelStatusRcd>();
}

TransClustering::~TransClustering() {
}

void TransClustering::beginJob() {
  graphDef = tensorflow::loadGraphDef(graphPath);
  session = tensorflow::createSession(graphDef);
}

void TransClustering::endJob() {
  // close the session
  tensorflow::closeSession(session);
  // delete the graph
  delete graphDef;
  graphDef = nullptr;
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
  //const CaloSubdetectorGeometry& barrelGeom = iSetup.getData(barrelGeomToken);
  // Get barrel subgeometry from it - no separate token needed
  // const CaloSubdetectorGeometry* barrelGeom = geo.getSubdetectorGeometry(DetId::Ecal, EcalBarrel);
  const EcalBarrelGeometry* barrelGeom = dynamic_cast<const EcalBarrelGeometry*>(geo.getSubdetectorGeometry(DetId::Ecal, EcalBarrel));

  edm::ESHandle<EcalChannelStatus> ecalStatus;
  ecalStatus = iSetup.getHandle(ecalStatusToken);

  // XXX: All the following can be built at the beginning of a job
  // Store EB: DetId <==> vector<int> (subdet, ieta, iphi, status)
  std::map<DetId, std::vector<int> > EcalAllDeadChannelsBitMap;
  EcalAllDeadChannelsBitMap.clear();

  // Loop over EB ...
  for (int ieta = -85; ieta <= 85; ieta++) {
    for (int iphi = 0; iphi <= 360; iphi++) {
      if (!EBDetId::validDetId(ieta, iphi))
        continue;

      const EBDetId detid = EBDetId(ieta, iphi, EBDetId::ETAPHIMODE);
      EcalChannelStatus::const_iterator chit = ecalStatus->find(detid);
      // refer https://twiki.cern.ch/twiki/bin/viewauth/CMS/EcalChannelStatus
      int status = (chit != ecalStatus->end()) ? chit->getStatusCode() & 0x1F : -1;

      if (status >= maskedEcalChannelStatusThreshold) {
        // std::cout << "Masked EB channel: ieta=" << ieta << ", iphi=" << iphi << ", status=" << status << std::endl;
        std::vector<int> bitVec;
        bitVec.push_back(1);
        bitVec.push_back(ieta);
        bitVec.push_back(iphi);
        bitVec.push_back(status);
        EcalAllDeadChannelsBitMap.insert(std::make_pair(detid, bitVec));
      }
    }  // end loop iphi
  }  // end loop ieta

#if INFER
  std::vector<std::vector<int>> dead_grid(361, std::vector<int>(171, 1)); // default = 1

  for (const auto& [detid, bitVec] : EcalAllDeadChannelsBitMap) {
      int ieta = bitVec[1];
      int iphi = bitVec[2];
      int status = bitVec[3];

      int ieta_shifted = ieta + 85; // [0, 170]
      int iphi_shifted = iphi; // [1, 360]

      if (iphi_shifted < 0 || iphi_shifted >= 361) continue;
      if (ieta_shifted < 0 || ieta_shifted >= 171) continue;

      int val = 1; // ok
      if (status >= 3 && status <= 10)
          val = 2; // noisy/wrong gain
      else if (status > 10)
          val = 3; // completely dead

      dead_grid[iphi_shifted][ieta_shifted] = val;
  }
#endif

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
  // int iPV = -1;
  // SimVertex primVtx;
  // if (!theSimTracks.empty() && !theSimTracks[0].noVertex()) {
  //   iPV = theSimTracks[0].vertIndex();
  //   primVtx = theSimVertices[iPV];
  // }

  // Find photons from pion decay vertex
  std::vector<SimTrack*> pionTracks;
  std::vector<SimTrack*> photonTracks;
  for (auto& simTk : theSimTracks) {
    if (simTk.noVertex()) continue;
    if (simTk.type() == 9000001) { // pi0 from primary vertex
      pionTracks.push_back(&simTk);
      std::cout << "Found SimTrack pion: trackId=" << simTk.trackId() << "(pt=" << simTk.momentum().pt()
                << ", eta=" << simTk.momentum().eta() << ", phi=" << simTk.momentum().phi() << ")"
                << " E=" << simTk.momentum().E() << std::endl;
    }
  }
  std::cout << "Total SimTrack pi0 from primary vertex: " << pionTracks.size() << std::endl;

  std::unordered_map<int, std::vector<int>> trackToDecayVertices;
  for (size_t iv = 0; iv < theSimVertices.size(); ++iv) {
    const auto& vtx = theSimVertices[iv];
    if (!vtx.parentIndex()) continue;
    auto association = geantToIndex_.find(vtx.parentIndex());
    if (association == geantToIndex_.end()) continue;
    int parentSimTrackIdx = association->second;
    int parentTrackId = theSimTracks[parentSimTrackIdx].trackId();
    trackToDecayVertices[parentTrackId].push_back(iv);
  }

  // For each primary pi0, look for conversion photons
  // Map pi0 trackId to conversion info
  std::map<unsigned, std::pair<float, float>> pionConversionInfo; // trackId -> (R, Z)
  std::map<unsigned, std::pair<float, float>> photonConversionInfo; // trackId -> (R, Z)
  std::map<unsigned, std::vector<unsigned>> pionAssociation; // photon trackId <-> pion trackId
  std::map<unsigned, math::XYZTLorentzVectorD> photonVertexPositions;

  for (auto* pionTk : pionTracks) {
    float convR = 0., convZ = 0.;
    int firstVertexId = -1;
    float minR = 999.;
    
    for (auto& simTk : theSimTracks) {
      if (simTk.noVertex()) continue;
      //if (simTk.vertIndex() == iPV) continue;
      if (simTk.type() != 22) continue; 

      int vertexId = simTk.vertIndex();
      SimVertex vertex = theSimVertices[vertexId];
      
      if (!vertex.parentIndex()) continue;
      unsigned motherGeantId = vertex.parentIndex();
      auto association = geantToIndex_.find(motherGeantId);
      if (association == geantToIndex_.end()) continue;
      int motherId = association->second;

      // Find the earliest pi0 decay vertex
      if (theSimTracks[motherId].trackId() != pionTk->trackId()) continue;
      float r = vertex.position().pt();
      if (r < minR) {
        minR = r;
        firstVertexId = vertexId;
      }
    }

    if (firstVertexId >= 0) {
      const auto& vertex = theSimVertices[firstVertexId]; // pi0 conversion vertex
      convR = vertex.position().pt();
      convZ = vertex.position().z();

      for (auto& simTk : theSimTracks) {
        if (simTk.noVertex()) continue;
        if (simTk.vertIndex() != firstVertexId) continue;
        if (simTk.type() != 22) continue;

        photonTracks.push_back(&simTk);
        pionAssociation[pionTk->trackId()].push_back(simTk.trackId());
        photonVertexPositions[simTk.trackId()] = vertex.position();
        std::cout << "  Found photon from pi0 decay: trackId=" << simTk.trackId() 
                  << " eta=" << simTk.momentum().eta() << " phi=" << simTk.momentum().phi() 
                  << " E=" << simTk.momentum().E() << std::endl;
      }
    }
    pionConversionInfo[pionTk->trackId()] = std::make_pair(convR, convZ);
    std::cout << "Pi0 trackId=" << pionTk->trackId() << " conversion: R=" << convR << ", Z=" << convZ << std::endl;
  } // end loop over pionTracks
  
  // For each photon, look for conversions
  for (auto* photonTk : photonTracks) {
    float convR = -1., convZ = -1.;
    int photonId = photonTk->trackId();

    auto it = trackToDecayVertices.find(photonId);
    if (it != trackToDecayVertices.end() && !it->second.empty()) {
      int firstVtx = *std::min_element(it->second.begin(), it->second.end(),
        [&](int a, int b) {
          return theSimVertices[a].position().pt() < theSimVertices[b].position().pt();
        });

      convR = theSimVertices[firstVtx].position().pt();
      convZ = theSimVertices[firstVtx].position().z();
    }
    if (convR > 0) {
      std::cout << "    Photon trackId=" << photonId << " conversion: R=" << convR << ", Z=" << convZ << std::endl;
      photonConversionInfo[photonId] = std::make_pair(convR, convZ);
    }
    else {
      float minR = 999.;
      // Find first conversion
      for (auto& hit : theEBCaloHits) {
        if (hit.geantTrackId() != photonId) continue;

        EBDetId ebid(hit.id());
        const GlobalPoint& pos = barrelGeom->getGeometry(ebid)->getPosition();
        float r = pos.perp(); 
        
        if (r < minR) {
          minR = r;
          convR = r;
          convZ = pos.z();
        }
      }
      std::cout << "    Photon trackId=" << photonId << " conversion: R=" << convR << ", Z=" << convZ << std::endl;
      photonConversionInfo[photonId] = std::make_pair(convR, convZ);
    }
  } // end loop over photonTracks

  // ***************** Loop over the GEN particles *****************

  std::vector<int> seed_iphi;
  std::vector<int> seed_ieta;
  std::vector<bool> seed_isConverted;

  std::cout << "GenParticles" << std::endl;
  for (const auto& genParticle : *genParticles) {
    int pdgId    = genParticle.pdgId();
    float pt     = genParticle.pt();
    float eta    = genParticle.eta();
    float phi    = genParticle.phi();
    float energy = genParticle.energy();
    
    // Access vertex information:
    float vx = genParticle.vx();
    float vy = genParticle.vy();
    float vz = genParticle.vz();
    std::cout << " GenParticle PDG ID: " << pdgId 
              << ", pT: " << pt << ", eta: " << eta << ", phi: " << phi << ", energy: " << energy 
              << ", vertex: (" << vx << ", " << vy << ", " << vz << ")" << std::endl;

    if (pdgId == 9000001) {
      // Match GenParticle pi0 to SimTrack pi0 by kinematic proximity
      float minDR = 0.1;  // dR matching threshold
      unsigned bestTrackId = 0;

      for (SimTrack* pionTk : pionTracks) {
        // Calculate dR between GenParticle and SimTrack pi0
        float trackEta = pionTk->momentum().eta();
        float trackPhi = pionTk->momentum().phi();
        float dEta = eta - trackEta;
        float dPhi = phi - trackPhi;
        // Wrap dphi to [-pi, pi]
        while (dPhi > M_PI) dPhi -= 2*M_PI;
        while (dPhi < -M_PI) dPhi += 2*M_PI;
        float dR = sqrt(dEta*dEta + dPhi*dPhi);
        
        std::cout << "    Matching: GenParticle(eta=" << eta << ",phi=" << phi 
                  << ") vs SimTrack " << pionTk->trackId() << "(eta=" << trackEta 
                  << ",phi=" << trackPhi << ") dR=" << dR << std::endl;
        
        if (dR < minDR) {
          minDR = dR;
          bestTrackId = pionTk->trackId();
        }
      }
      std::cout << "  Best match: trackId=" << bestTrackId << " with dR=" << minDR << std::endl;

      auto it = pionAssociation.find(bestTrackId);
      if (it == pionAssociation.end()) continue;

      for (unsigned photonTrackId : it->second) {
        auto assoc = geantToIndex_.find(photonTrackId);
        const auto& pho = theSimTracks[assoc->second];

        float pho_pt = pho.momentum().pt();
        float pho_eta = pho.momentum().eta();
        float pho_phi = pho.momentum().phi();
        float pho_energy = pho.momentum().E();
        float pho_pdgId = pho.type();

        math::XYZTLorentzVectorD phoVertex = photonVertexPositions[photonTrackId];
        float pho_vx = phoVertex.X();
        float pho_vy = phoVertex.Y();
        float pho_vz = phoVertex.Z();

        std::cout << "    Daughter (pT=" << pho_pt << ", eta=" << pho_eta << ", phi=" << pho_phi << ", energy=" << pho_energy << ")" << std::endl;
      
        // Access matched photon conversion info
        int isConverted = 0;
        float pho_convR = 0., pho_convZ = 0.;
        float pho_ieta = 0;
        float pho_iphi = 0;

        if (photonTrackId > 0 && photonConversionInfo.find(photonTrackId) != photonConversionInfo.end()) {
          const auto& convInfo = photonConversionInfo[photonTrackId];
          if ((convInfo.first > 0 || convInfo.second != 0)) {
            pho_convR = convInfo.first;
            pho_convZ = convInfo.second;
            if (convInfo.first < 129) {isConverted = 1;}
            std::cout << "    -> Daughter converted at R=" << pho_convR << ", Z=" << pho_convZ << std::endl;
          }
        }

        // Get entry point on ECAL surface of the photon
        double tg_theta_over_2 = exp(-pho_eta);
        // avoid division by zero
        if (tg_theta_over_2 == 1.0)
            tg_theta_over_2 = 1.0 - 1e-10;
        double tg_theta = 2. * tg_theta_over_2 / (1. - tg_theta_over_2 * tg_theta_over_2);  // tg(a+b) = tg(a)+tg(b) / (1-tg(a)*tg(b))

        // calculations for EB
        const double R = 129.;
        double angle_x0_y0 = atan2(pho_vy, pho_vx);
        double alpha = angle_x0_y0 + (M_PI - pho_phi);
        double sin_beta = sqrt(pho_vx*pho_vx + pho_vy*pho_vy) / R * sin(alpha);
        double beta = abs(asin(sin_beta));
        double gamma = M_PI / 2. - alpha - beta;
        double length = sqrt(R*R + pho_vx*pho_vx + pho_vy*pho_vy - 2 * R * sqrt(pho_vx*pho_vx + pho_vy*pho_vy) * cos(gamma));
        double z0_zSC = length / tg_theta;

        double tg_sctheta = tg_theta;
        // correct values for EB
        tg_sctheta = R / (pho_vz + z0_zSC);
        double sctheta = atan(tg_sctheta);
        if (sctheta < 0) sctheta += M_PI; // ensure sctheta is in [0, pi]
        
        double ScEta = -log(tan(sctheta / 2.));
        //double ScPhi = phi < 0 ? phi + 2*M_PI : phi;

        pho_ieta = static_cast<float>(ScEta) / 0.0174; // convert to crystal index (float)
        pho_iphi = static_cast<float>(pho_phi); // convert to crystal index (float)

        seed_isConverted.push_back(static_cast<bool>(isConverted));
        seed_ieta.push_back(pho_ieta + 85); // shift to [0,170]
        seed_iphi.push_back(pho_iphi); // already in [1,360]
        std::cout << "  Photon momentum points to: ieta=" << pho_ieta << " iphi=" << pho_iphi << std::endl;

        gammaEvent.push_back(iEvent.id().event());
        gammaPDG.push_back(pho_pdgId);
        gammaEta.push_back(pho_ieta);
        gammaPhi.push_back(pho_iphi);
        gammaE.push_back(pho_energy);
        gammaConvR.push_back(pho_convR);
        gammaConvZ.push_back(pho_convZ);
        gammaPPt.push_back(pho_pt);
        gammaPEta.push_back(pho_eta);
        gammaPPhi.push_back(pho_phi);
        gammaTrackId.push_back(bestTrackId);
      } // end loop over photons
    genEvent.push_back(iEvent.id().event());
    genPDG.push_back(pdgId);
    genPPt.push_back(pt);
    genPEta.push_back(eta);
    genPPhi.push_back(phi);
    genE.push_back(energy);
    genTrackId.push_back(bestTrackId);
    }
  } // end loop over GenParticles
  
  // **************** Loop over the CaloParticles ****************

  // int nsimhits = 0;
  // CaloMapType caloMap;
  // int sc_number = 0;
  // for (const auto& cp : *caloParticles) {
  //   nsimhits = 0;
    
  //   // std::cout << "GenParticle ID " << cp.pdgId() << ", energy = " << cp.energy() << ", eta = " << cp.eta() << ", phi = " << cp.phi() << std::endl;
    
  //   // Access sim clusters associated with this calo particle
  //   const auto& simClusters = cp.simClusters();
  //   // std::cout << "GenParticle has " << simClusters.size() << " associated sim clusters." << std::endl;
  //   sc_number = 0;
  //   for (const auto& sc : simClusters) {

  //     if (PRINT_DEBUG) {
  //       std::cout << *sc << std::endl;
  //     }

  //     caloPDG.push_back(sc->pdgId());
  //     caloE.push_back(sc->energy());
  //     caloPEta.push_back(sc->eta());
  //     caloPPhi.push_back(sc->phi());
  //     caloPPt.push_back(sc->pt());
  //     caloTrackId.push_back(sc->g4Track_begin()->trackId());
  //     caloEvent.push_back(iEvent.id().event());

  //     // Get the sim hits associated with this sim cluster
  //       const auto& hitAndEnergies = sc->hits_and_energies();
  //       for (const auto& hitAndEnergy : hitAndEnergies) {
  //         DetId hitId = hitAndEnergy.first;
  //         EBDetId ebid(hitId);
  //         //std::cout << "    SimHit ieta: " << ebid.ieta() << ", iphi: " << ebid.iphi() << std::endl;
  //         // Store all (sc_number, simhit energy) associated with this (ieta, iphi) pair
  //         float simHitEnergy = hitAndEnergy.second;
  //         caloMap[std::make_pair(ebid.ieta(), ebid.iphi())].push_back(std::make_pair(sc_number, simHitEnergy));
  //         nsimhits++;
  //       }
  //     sc_number++;
  //   }
  //   if (PRINT_DEBUG) {
  //     std::cout << "CaloParticle has " << nsimhits << " associated sim hits." << std::endl;
  //   }
  // }

  // for (const auto& [key, scNumbers] : caloMap) {
  //   for (const auto& scNumAndE : scNumbers) {
  //     caloIEta.push_back(key.first);
  //     caloIPhi.push_back(key.second);
  //     caloValues.push_back(scNumAndE.first);
  //     caloValuesE.push_back(scNumAndE.second);
  //     caloSubEvent.push_back(iEvent.id().event());
  //   }
  // }

  // **************** Loop over the EB SIM hits ****************

  std::map<unsigned int, std::vector<PCaloHit *>, std::less<unsigned int>> CaloHitMap;
  MapType simMap;
  double EBEnergy_ = 0.;

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
  }

  for (const auto& [key, val] : simMap) {
    simIEta.push_back(key.first);
    simIPhi.push_back(key.second);
    simValues.push_back(val);
    simSubEvent.push_back(iEvent.id().event());
  }

  // **************** Loop over the EB REC hits ****************

  MapType recoMap;
  std::vector<int> ieta_vec;
  std::vector<int> iphi_vec;
  std::vector<float> energy_vec;

  for (EcalRecHitCollection::const_iterator recHit = EBRecHit->begin(); recHit != EBRecHit->end(); ++recHit) {
    EBDetId ebid = EBDetId(recHit->id());
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    recoMap[std::make_pair(ieta, iphi)] += recHit->energy();
    ieta_vec.push_back(ieta);
    iphi_vec.push_back(iphi);
    energy_vec.push_back(recHit->energy());
  }

  for (const auto& [key, val] : recoMap) {
    recoIEta.push_back(key.first);
    recoIPhi.push_back(key.second);
    recoValues.push_back(val);
    recoEvent.push_back(iEvent.id().event());
  }

#if INFER
  std::vector<float> map = build_energy_map(ieta_vec, iphi_vec, energy_vec);
  apply_blackout(map, seed_iphi, seed_ieta, seed_isConverted);

  std::cout << "Map size: " << map.size() << " (should be 61731 for 361x171)" << std::endl;
  std::cout << "Crop Size: " << cropSize << std::endl;
  std::cout << "Seed Threshold: " << seedThreshold << std::endl;
  std::cout << "Overlap Limit: " << overlapLimit << std::endl;
  std::cout << "Max Clusters: " << maxClusters << std::endl;

  auto result = get_model_samples(map, 361, 171, seedThreshold, cropSize, overlapLimit, maxClusters);
  std::vector<std::vector<Eigen::MatrixXf>>& X = result.X; // (N, maxClusters, cropSize, cropSize)
  std::vector<std::vector<Eigen::MatrixXf>>& indices = result.indices; // (N, maxClusters, 2)

  // print X and indices shapes
  std::cout << "X shape: (" << X.size() << ", " << (X.empty() ? 0 : X[0].size()) << ", " 
            << (X.empty() || X[0].empty() ? 0 : X[0][0].rows()) << ", " 
            << (X.empty() || X[0].empty() ? 0 : X[0][0].cols()) << ")" << std::endl;
  std::cout << "Indices shape: (" << indices.size() << ", " << (indices.empty() ? 0 : indices[0].size()) << ", 2)" << std::endl;

  // print each cluster
  // for (size_t i = 0; i < X.size(); ++i) {
  //       std::cout << "Sample " << i << ":\n";
  //       for (size_t j = 0; j < X[i].size(); ++j) {
  //           std::cout << "  Cluster " << j << ":\n";
  //           std::cout << X[i][j] << "\n";
  //       }
  //   }

  int half = cropSize / 2;
  std::vector<std::vector<Eigen::MatrixXi>> dead_masks; // (N, maxClusters, cropSize, cropSize)

  for (const auto& event : indices) { // loop over N clusters
      std::vector<Eigen::MatrixXi> event_masks;

      for (const auto& idx : event) { // loop over maxClusters per cluster
          int center_iphi = static_cast<int>(idx(0,0));
          int center_ieta = static_cast<int>(idx(1,0));

          // padding case
          if (center_ieta == -1 && center_iphi == -1) {
              event_masks.push_back(Eigen::MatrixXi::Constant(cropSize, cropSize, -1));
              continue;
          }

          Eigen::MatrixXi window(cropSize, cropSize);

          for (int dr = -half; dr <= half; ++dr) {
              int row = (center_iphi + dr - 1 + (361 - 1)) % (361 - 1) + 1; // wrap around iphi
              for (int dc = -half; dc <= half; ++dc) {
                  int col = center_ieta + dc;
                  if (col < 1 || col >= 171)
                      window(dr + half, dc + half) = -1; // out of bounds in ieta
                  else
                      window(dr + half, dc + half) = dead_grid[row][col];
              }
          }
          event_masks.push_back(std::move(window));
      }
      dead_masks.push_back(std::move(event_masks));
  }

  int numClusters = X.size();
  std::cout << "Number of clusters to run through the model: " << numClusters << std::endl;

  std::vector<std::vector<int32_t>> abs_pos(numClusters, std::vector<int32_t>(maxClusters, 0)); // 1D flattened absolute positions
  for (int n = 0; n < numClusters; ++n) {
      for (int k = 0; k < maxClusters; ++k) {
          int center_ieta = static_cast<int>(indices[n][k](0,0));
          int center_iphi = static_cast<int>(indices[n][k](1,0));
          int val = center_iphi + 171 * center_ieta;
          if (val == -172)  // corresponds to (-1, -1)
              val = 0;
          abs_pos[n][k] = val;
      }
  }

  int r_eff = (cropSize + overlapLimit) - 1;

  // Position relative to the center of the effective window
  std::vector<std::vector<std::array<float, 2>>> rel_pos(
      numClusters, std::vector<std::array<float, 2>>(maxClusters));
  // Initialize with -1
  for (int n = 0; n < numClusters; ++n) {
      for (int i = 0; i < maxClusters; ++i) {
          rel_pos[n][i][0] = -1.0f;
          rel_pos[n][i][1] = -1.0f;
      }
  }
  for (int n = 0; n < numClusters; ++n) {
      int center_ieta = static_cast<int>(indices[n][0](0,0)); // Highest Edep
      int center_iphi = static_cast<int>(indices[n][0](1,0)); // Highest Edep
      for (int k = 0; k < maxClusters; ++k) {
          int ieta = static_cast<int>(indices[n][k](0,0));
          int iphi = static_cast<int>(indices[n][k](1,0));
          if (ieta > -1) {rel_pos[n][k][0] = float(ieta - center_ieta) / float(r_eff);}
          if (iphi > -1) {rel_pos[n][k][1] = float(iphi - center_iphi) / float(r_eff);}
      }
  }

  tensorflow::Tensor input1(tensorflow::DT_FLOAT, { numClusters, cropSize, cropSize, maxClusters});
  tensorflow::Tensor input2(tensorflow::DT_FLOAT, { numClusters, maxClusters, 2});
  tensorflow::Tensor input3(tensorflow::DT_INT32, { numClusters, maxClusters});
  tensorflow::Tensor input4(tensorflow::DT_FLOAT, { numClusters, cropSize, cropSize, maxClusters});

  tensorflow::TTypes<float, 4>::Tensor input_tensor1 = input1.tensor<float, 4>();
  tensorflow::TTypes<float, 3>::Tensor input_tensor2 = input2.tensor<float, 3>();
  tensorflow::TTypes<float, 4>::Tensor input_tensor4 = input4.tensor<float, 4>();

  for (int n = 0; n < numClusters; n++) {
    for (int i = 0; i < maxClusters; i++) {
      const Eigen::MatrixXf& mat1 = X[n][i]; // 7x7
      Eigen::MatrixXf mat2 = dead_masks[n][i].cast<float>(); // 7x7
      for (int j = 0; j < cropSize; j++) {
        for (int k = 0; k < cropSize; k++) {
            input_tensor1(n, j, k, i) = mat1(j, k);
            input_tensor4(n, j, k, i) = mat2(j, k);
        }
      }
      input_tensor2(n, i, 0) = rel_pos[n][i][0];
      input_tensor2(n, i, 1) = rel_pos[n][i][1];
      input3.matrix<int32_t>()(n, i) = abs_pos[n][i];
    }
  }

  if (PRINT_DEBUG) {
    std::cout << "=== INPUT SANITY CHECK ===" << std::endl;
    for (int i = 0; i < 3; i++) {
        std::cout << "input1(0, 3, 3, " << i << ") = " << input_tensor1(0, 3, 3, i) << std::endl;
        std::cout << "input2(0, " << i << ", 0) = " << input_tensor2(0, i, 0) << std::endl;
        std::cout << "input2(0, " << i << ", 1) = " << input_tensor2(0, i, 1) << std::endl;
        std::cout << "input3(0, " << i << ") = " << input3.matrix<int32_t>()(0, i) << std::endl;
        std::cout << "input4(0, 3, 3, " << i << ") = " << input_tensor4(0, 3, 3, i) << std::endl;
    }
  }

  // print input tensor shapes
  std::cout << "Input1 shape: (" << input1.dim_size(0) << ", " << input1.dim_size(1) << ", " << input1.dim_size(2) << ", " << input1.dim_size(3) << ")" << std::endl;
  std::cout << "Input2 shape: (" << input2.dim_size(0) << ", " << input2.dim_size(1) << ", " << input2.dim_size(2) << ")" << std::endl;
  std::cout << "Input3 shape: (" << input3.dim_size(0) << ", " << input3.dim_size(1) << ")" << std::endl;
  std::cout << "Input4 shape: (" << input4.dim_size(0) << ", " << input4.dim_size(1) << ", " << input4.dim_size(2) << ", " << input4.dim_size(3) << ")" << std::endl;

  // run the evaluation
  std::vector<tensorflow::Tensor> outputs;
  tensorflow::run(session, {{"inp1:0", input1}, {"inp2:0", input2}, {"inp3:0", input3}, {"inp4:0", input4}},
                           {"center:0", "energy:0", "seed:0"}, &outputs);

  // process the output tensor
  tensorflow::TTypes<float, 3>::Tensor center_pr = outputs[0].tensor<float, 3>();  // shape [N, 20, 2]
  tensorflow::TTypes<float, 3>::Tensor energy_pr = outputs[1].tensor<float, 3>();  // shape [N, 20, 1]
  tensorflow::TTypes<float, 3>::Tensor seed_pr   = outputs[2].tensor<float, 3>();  // shape [N, 20, 1]
  // convert 
  std::vector<std::vector<std::pair<float, float>>> centers(numClusters, std::vector<std::pair<float, float>>(maxClusters));
  std::vector<std::vector<float>> energies(numClusters, std::vector<float>(maxClusters));
  std::vector<std::vector<float>> seeds(numClusters, std::vector<float>(maxClusters));

  for (int n = 0; n < numClusters; n++) {
      for (int i = 0; i < maxClusters; i++) {
          centers[n][i] = {
              center_pr(n, i, 0) + indices[n][i](0) - cropSize / 2,
              center_pr(n, i, 1) + indices[n][i](1) - cropSize / 2
          };
          energies[n][i] = energy_pr(n, i, 0) * 100.0f;
          seeds[n][i] = seed_pr(n, i, 0);
      }
  }

  for (int i = 0; i < maxClusters; i++) {
    std::cout << "Energy[" << i << "] = " << energies[0][i] << std::endl;
    std::cout << "Seed[" << i << "] = " << seeds[0][i] << std::endl;
    std::cout << "Center[" << i << "] = (" << centers[0][i].first << ", " << centers[0][i].second << ")" << std::endl;
  }

  // print the truth
  for (size_t i = 0; i < genPDG.size(); ++i) {
    std::cout << "GenParticle " << i << ": PDG=" << genPDG[i] 
              << ", E=" << genE[i] 
              << ", eta=" << genEta[i] + 85 
              << ", phi=" << genPhi[i] 
              << ", isConverted=" << genIsConverted[i] 
              << ", convR=" << genConvR[i] 
              << ", convZ=" << genConvZ[i] 
              << std::endl;
  }

  // Store
  for (int n = 0; n < numClusters; n++) {
    for (int i = 0; i < maxClusters; i++) {
        mlEvent.push_back(iEvent.id().event());
        mlN.push_back(n);
        mlK.push_back(i);
        mlCenterX.push_back(centers[n][i].first);
        mlCenterY.push_back(centers[n][i].second);
        mlEnergy.push_back(energies[n][i]);
        mlSeed.push_back(seeds[n][i]);
    }
  }
  mlTree->Fill();
#endif

  simTree->Fill();
  recoTree->Fill();
  caloTree->Fill();
  genTree->Fill();
  gammaTree->Fill();

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

  genE.clear();
  genPPt.clear();
  genPPhi.clear();
  genPEta.clear();
  genPDG.clear();
  genEvent.clear();
  genTrackId.clear();

  gammaPDG.clear();
  gammaPEta.clear();
  gammaPPhi.clear();
  gammaPPt.clear();
  gammaEta.clear();
  gammaPhi.clear();
  gammaE.clear();
  gammaConvR.clear();
  gammaConvZ.clear();
  gammaEvent.clear();
  gammaTrackId.clear();

#if INFER
  mlEvent.clear();
  mlN.clear();
  mlK.clear();
  mlCenterX.clear();
  mlCenterY.clear();
  mlEnergy.clear();
  mlSeed.clear();
#endif
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
  genTree->Branch("energy",      &genE);
  genTree->Branch("pt",          &genPPt);
  genTree->Branch("phi",         &genPPhi);
  genTree->Branch("eta",         &genPEta);
  genTree->Branch("pdg",         &genPDG);
  genTree->Branch("event",       &genEvent);
  genTree->Branch("trackId",     &genTrackId);

  gammaTree = fs->make<TTree>("gammaTree", "A tree with photon information");
  gammaTree->Branch("pdg",         &gammaPDG);
  gammaTree->Branch("ieta",        &gammaEta);
  gammaTree->Branch("iphi",        &gammaPhi);
  gammaTree->Branch("pt",          &gammaPPt);
  gammaTree->Branch("eta",         &gammaPEta);
  gammaTree->Branch("phi",         &gammaPPhi);
  gammaTree->Branch("energy",      &gammaE);
  gammaTree->Branch("convR",       &gammaConvR);
  gammaTree->Branch("convZ",       &gammaConvZ);
  gammaTree->Branch("event",       &gammaEvent);
  gammaTree->Branch("trackId",     &gammaTrackId);

#if INFER
  mlTree = fs->make<TTree>("mlTree", "ML inference output");
  mlTree->Branch("event",   &mlEvent);
  mlTree->Branch("n",       &mlN);
  mlTree->Branch("k",       &mlK);
  mlTree->Branch("centerX", &mlCenterX);
  mlTree->Branch("centerY", &mlCenterY);
  mlTree->Branch("energy",  &mlEnergy);
  mlTree->Branch("seed",    &mlSeed);
#endif
}
