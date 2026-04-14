#include "DataFormats/Math/interface/GeantUnits.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Geometry/EcalAlgo/interface/EcalBarrelGeometry.h"
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"
#include "Validation/EcalHits/interface/TransClustering.h"
#include "Validation/EcalHits/interface/PreProcessing.h"

#include <fstream>

#define INFER 1
#define PRINT_DEBUG 0

// namespace {
// int currentLinuxThreadCount() {
//   std::ifstream status("/proc/self/status");
//   std::string key;
//   while (status >> key) {
//     if (key == "Threads:") {
//       int n = 0;
//       status >> n;
//       return n;
//     }
//     status.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
//   }
//   return -1;
// }
// }

std::vector<float> build_energy_map(
    const std::vector<int>&   ieta_vec,
    const std::vector<int>&   iphi_vec,
    const std::vector<float>& energy_vec)
{
    std::vector<float> map(361 * 170, 0.f);
    for (size_t i = 0; i < ieta_vec.size(); ++i) {
        int col = ieta_vec[i];
        if (col < 0) {
          col += 85; // ieta in [-85,-1] → col in [0,84]
        } else {
          col += 84; // ieta in [1,85] → col in [85,169]
        }
        int row = iphi_vec[i];                // iphi → [1,360]
        if (row < 0 || row >= 361) continue;
        if (col < 0 || col >= 170) continue;
        map[row * 170 + col] += energy_vec[i];
    }
    return map;
}

void apply_blackout(
    std::vector<float>&       map,
    const std::vector<int>&   seed_iphi,
    const std::vector<int>&   seed_ieta,
    const std::vector<bool>&  is_converted)
{
    const int pad = 15; // blackout pad size in ieta and iphi
    for (size_t j = 0; j < is_converted.size(); ++j) {
        if (!is_converted[j]) continue; // continue if it has not been converted
        int r0 = std::max(0, seed_iphi[j] - pad);
        int r1 = std::min(361, seed_iphi[j] + pad);
        int c0 = std::max(0, seed_ieta[j] - pad);
        int c1 = std::min(170, seed_ieta[j] + pad);
        for (int r = r0; r < r1; ++r)
            for (int c = c0; c < c1; ++c)
                map[r * 170 + c] = 0.f;
    }
}

// ------------ constructor and destructor --------------
TransClustering::TransClustering(const edm::ParameterSet& iConfig)
  : input_names_(iConfig.getParameter<std::vector<std::string>>("input_names")),
    input_shapes_(),
    g4InfoLabel(iConfig.getParameter<std::string>("moduleLabelG4")),
    EBHitsCollection(iConfig.getParameter<std::string>("EBHitsCollection")),
    ValidationCollection(iConfig.getParameter<std::string>("ValidationCollection")),
    jobId(iConfig.getParameter<std::string>("jobId")),
    maskedEcalChannelStatusThreshold(iConfig.getParameter<int>("maskedEcalChannelStatusThreshold")),
    // graphPath(iConfig.getParameter<std::string>("graphPath")),
    // inputTensorName(iConfig.getParameter<std::string>("inputTensorName")),
    // outputTensorName(iConfig.getParameter<std::string>("outputTensorName")),
    cropSize(iConfig.getParameter<int>("cropSize")),
    maxClusters(iConfig.getParameter<int>("maxClusters")),
    overlapLimit(iConfig.getParameter<int>("overlapLimit")),
    seedThreshold(iConfig.getParameter<double>("seedThreshold"))
{
  const int onnxIntraOpThreads = iConfig.getUntrackedParameter<int>("onnxIntraOpThreads", 16);
  const int onnxInterOpThreads = iConfig.getUntrackedParameter<int>("onnxInterOpThreads", 1);
  // const int threadsBeforeOnnx = currentLinuxThreadCount();
  auto sessOpts = ONNXRuntime::defaultSessionOptions(Backend::cpu);
  sessOpts.SetIntraOpNumThreads(onnxIntraOpThreads);
  sessOpts.SetInterOpNumThreads(onnxInterOpThreads);
  onnx_ = std::make_unique<ONNXRuntime>(iConfig.getParameter<edm::FileInPath>("model_path").fullPath(), &sessOpts);
  // const int threadsAfterOnnx = currentLinuxThreadCount();
  // edm::LogVerbatim("TransClustering") << "ONNX thread config: intra=" << onnxIntraOpThreads
  //                                     << " inter=" << onnxInterOpThreads
  //                                     << " | Linux threads before=" << threadsBeforeOnnx
  //                                     << " after=" << threadsAfterOnnx
  //                                     << " delta=" << (threadsAfterOnnx - threadsBeforeOnnx);

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
  ecalStatusToken = esConsumes<EcalChannelStatus, EcalChannelStatusRcd>();
  // mtdgeoToken = esConsumes<MTDGeometry, MTDDigiGeometryRecord>();
  // mtdtopoToken = esConsumes<MTDTopology, MTDTopologyRcd>();
  // btlSimHitsToken = consumes<CrossingFrame<PSimHit>>(iConfig.getParameter<edm::InputTag>("btlSimHits"));
  // configure logging to show warnings (see table below)
  // tensorflow::setLogging("2");
}

TransClustering::~TransClustering() {}

// void TransClustering::beginJob() {
//   graphDef = tensorflow::loadGraphDef(graphPath);
//   session = tensorflow::createSession(graphDef);
// }

// void TransClustering::endJob() {
//   // close the session
//   tensorflow::closeSession(session);
//   // delete the graph
//   delete graphDef;
//   graphDef = nullptr;
// }

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

  // auto geometryHandle = iSetup.getTransientHandle(mtdgeoToken);
  // const MTDGeometry* btlGeom = geometryHandle.product();
  // auto topologyHandle = iSetup.getTransientHandle(mtdtopoToken);
  // const MTDTopology* topology = topologyHandle.product();

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
  std::vector<std::vector<int>> dead_grid(361, std::vector<int>(170, 1)); // default = 1

  for (const auto& [detid, bitVec] : EcalAllDeadChannelsBitMap) {
      int ieta = bitVec[1];
      int iphi = bitVec[2];
      int status = bitVec[3];

      int ieta_shifted = ieta;
      if (ieta_shifted < 0) {
        ieta_shifted += 85; // ieta in [-85,-1] → [0,84]
      } else {
        ieta_shifted += 84; // ieta in [1,85] → [85,169]
      }
      int iphi_shifted = iphi; // [1, 360]

      if (iphi_shifted < 0 || iphi_shifted >= 361) continue;
      if (ieta_shifted < 0 || ieta_shifted >= 170) continue;

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

  // edm::Handle<CrossingFrame<PSimHit>> btlSimHandle;
  // iEvent.getByToken(btlSimHitsToken, btlSimHandle);
  // MixCollection<PSimHit> btlSimHits(btlSimHandle.product());
  
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

  // Build parent map (do this once, outside the gen particle loop)
  std::map<unsigned int, unsigned int> parentMap;
  for (const auto& simTk : theSimTracks) {
      if (!simTk.noVertex()) {
          const SimVertex& vtx = theSimVertices[simTk.vertIndex()];
          if (vtx.parentIndex() > 0) {
              parentMap[simTk.trackId()] = vtx.parentIndex();
          }
      }
  }
  // auto descendsFrom = [&](unsigned int hitTrackId, unsigned int ancestorId) -> bool {
  //   unsigned int current = hitTrackId;
  //   for (int depth = 0; depth < 20; ++depth) {
  //       if (current == ancestorId) return true;
  //       auto it = parentMap.find(current);
  //       if (it == parentMap.end()) break;
  //       current = it->second;
  //   }
  //   return false;
  // };

  // Find photons from primary vertex
  std::vector<SimTrack*> photonTracks;
  for (auto& simTk : theSimTracks) {
    if (simTk.noVertex()) continue;
    if (simTk.vertIndex() == iPV && simTk.type() == 22) {
      photonTracks.push_back(&simTk);
      if (PRINT_DEBUG) {
        std::cout << "Found SimTrack photon: trackId=" << simTk.trackId() 
                  << " eta=" << simTk.momentum().eta() 
                  << " phi=" << simTk.momentum().phi() 
                  << " E=" << simTk.momentum().E() << std::endl;
      }
    }
  }
  if (PRINT_DEBUG) {
    std::cout << "Total SimTrack photons from primary vertex: " << photonTracks.size() << std::endl;
  }

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
    if (PRINT_DEBUG) {
      std::cout << "  Photon trackId=" << phoTk->trackId() << " conversion: R=" << convR << ", Z=" << convZ << std::endl;
    }
  }

  // ***************** Loop over the GEN particles *****************

  std::vector<int> seed_iphi;
  std::vector<int> seed_ieta;
  std::vector<bool> seed_isConverted;

  if (PRINT_DEBUG) {
    std::cout << "GenParticles" << std::endl;
  }
  for (const auto& genParticle : *genParticles) {
    int pdgId    = genParticle.pdgId();
    float pt     = genParticle.pt();
    float eta    = genParticle.eta();
    float phi    = genParticle.phi();
    //float theta  = genParticle.theta();
    float energy = genParticle.energy();
    //int status = genParticle.status();
    
    // Access vertex information:
    float vx = genParticle.vx();
    float vy = genParticle.vy();
    float vz = genParticle.vz();
    if (PRINT_DEBUG) {
      std::cout << " GenParticle PDG ID: " << pdgId 
                << ", pT: " << pt << ", eta: " << eta << ", phi: " << phi << ", energy: " << energy 
                << ", vertex: (" << vx << ", " << vy << ", " << vz << ")" << std::endl;
    }

    // Check if this photon converted (by matching to SimTracks from primary vertex)
    int isConverted = 0;
    float convR = 0., convZ = 0.;
    float ieta = 0;
    float iphi = 0;
    // float ieta_f = 0.;
    // float iphi_f = 0.;
    if (pdgId == 22 && iPV >= 0) {
      // Match GenParticle photon to SimTrack photon by kinematic proximity
      float minDR = 0.1;  // dR matching threshold
      unsigned bestTrackId = 0;

      for (SimTrack* phoTk : photonTracks) {
        // Calculate dR between GenParticle and SimTrack photon
        float trackEta = phoTk->momentum().eta();
        float trackPhi = phoTk->momentum().phi();
        float dEta = eta - trackEta;
        float dPhi = phi - trackPhi;
        // Wrap dphi to [-pi, pi]
        while (dPhi > M_PI) dPhi -= 2*M_PI;
        while (dPhi < -M_PI) dPhi += 2*M_PI;
        float dR = sqrt(dEta*dEta + dPhi*dPhi);
        
        if (PRINT_DEBUG) {
          std::cout << "    Matching: GenPhoton(eta=" << eta << ",phi=" << phi 
                    << ") vs SimTrack " << phoTk->trackId() << "(eta=" << trackEta 
                    << ",phi=" << trackPhi << ") dR=" << dR << std::endl;
        }

        if (dR < minDR) {
          minDR = dR;
          bestTrackId = phoTk->trackId();
        }
      }
      if (PRINT_DEBUG) {
        std::cout << "  Best match: trackId=" << bestTrackId << " with dR=" << minDR << std::endl;
      }

      // Now check if the matched photon has conversion info
      if (bestTrackId > 0 && photonConversionInfo.find(bestTrackId) != photonConversionInfo.end()) {
        const auto& convInfo = photonConversionInfo[bestTrackId];
        if ((convInfo.first > 0 || convInfo.second != 0)) {
          convR = convInfo.first;
          convZ = convInfo.second;
          if (convInfo.first < 129) {isConverted = 1;}
          if (PRINT_DEBUG) {
            std::cout << "  -> Converted at R=" << convR << ", Z=" << convZ << std::endl;
          }
        }
      }

      double tg_theta_over_2 = exp(-eta);
      // avoid division by zero
      if (tg_theta_over_2 == 1.0)
          tg_theta_over_2 = 1.0 - 1e-10;
      double tg_theta = 2. * tg_theta_over_2 / (1. - tg_theta_over_2 * tg_theta_over_2);  // tg(a+b) = tg(a)+tg(b) / (1-tg(a)*tg(b))

      // calculations for EB
      const double R = 129.;
      double angle_x0_y0 = atan2(vy, vx);
      double alpha = angle_x0_y0 + (M_PI - phi);
      double sin_beta = sqrt(vx*vx + vy*vy) / R * sin(alpha);
      double beta = abs(asin(sin_beta));
      double gamma = M_PI / 2. - alpha - beta;
      double length = sqrt(R*R + vx*vx + vy*vy - 2 * R * sqrt(vx*vx + vy*vy) * cos(gamma));
      double z0_zSC = length / tg_theta;

      double tg_sctheta = tg_theta;
      // correct values for EB
      tg_sctheta = R / (vz + z0_zSC);
      double sctheta = atan(tg_sctheta);
      if (sctheta < 0) sctheta += M_PI; // ensure sctheta is in [0, pi]
      
      double ScEta = -log(tan(sctheta / 2.));
      //double ScPhi = phi < 0 ? phi + 2*M_PI : phi;

      ieta = static_cast<float>(ScEta) / 0.0174; // convert to crystal index (float)
      iphi = static_cast<float>(phi); // convert to crystal index (float)

      // Match gen photon SimTrack to its earliest PCaloHit in ECAL
      // EBDetId cpEBid;
      // float earliestTime = 1e9;
      // bool found = false;

      // for (const auto& hit : theEBCaloHits) {
      //   if (hit.time() > 500.) continue; 
        
      //   if (descendsFrom((unsigned int)hit.geantTrackId(), bestTrackId)) {
      //     if (hit.time() < earliestTime) {
      //       earliestTime = hit.time();
      //       cpEBid = EBDetId(hit.id());
      //       found = true;
      //     }
      //   }
      // }
      // if (found) {
      //   ieta = cpEBid.ieta();
      //   iphi = cpEBid.iphi();
      // }
      seed_isConverted.push_back(static_cast<bool>(isConverted));
      seed_ieta.push_back(ieta + 85);
      iphi = iphi * (180.0 / M_PI);         // (-180, +180) degrees
      iphi = std::fmod(iphi + 10.0, 360.0); // apply offset, wrap to [0, 360)
      iphi = iphi + 1.0;                    // 1-based index [1, 360]
      seed_iphi.push_back(iphi);
      if (PRINT_DEBUG) {
        std::cout << "  GenParticle momentum points to: ieta=" << ieta << " iphi=" << iphi << std::endl;
      }
    }
    genEvent.push_back(iEvent.id().event());
    genPDG.push_back(pdgId);
    genSourceX.push_back(vx);
    genSourceY.push_back(vy);
    genSourceZ.push_back(vz);
    genPEta.push_back(eta);
    genPPhi.push_back(phi);
    genPPt.push_back(pt);
    genEta.push_back(ieta);
    genPhi.push_back(iphi);
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
    
    // std::cout << "GenParticle ID " << cp.pdgId() << ", energy = " << cp.energy() << ", eta = " << cp.eta() << ", phi = " << cp.phi() << std::endl;
    
    // Access sim clusters associated with this calo particle
    const auto& simClusters = cp.simClusters();
    // std::cout << "GenParticle has " << simClusters.size() << " associated sim clusters." << std::endl;
    sc_number = 0;
    for (const auto& sc : simClusters) {

      if (PRINT_DEBUG) {
        std::cout << *sc << std::endl;
      }

      caloPDG.push_back(sc->pdgId());
      caloE.push_back(sc->energy());
      caloPEta.push_back(sc->eta());
      caloPPhi.push_back(sc->phi());
      caloPPt.push_back(sc->pt());
      caloTrackId.push_back(sc->g4Track_begin()->trackId());
      caloEvent.push_back(iEvent.id().event());

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
    if (PRINT_DEBUG) {
      std::cout << "CaloParticle has " << nsimhits << " associated sim hits." << std::endl;
    }
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
  // std::cout << "Total EB sim energy: " << EBEnergy_ << std::endl;

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

  std::cout << "Map size: " << map.size() << " (should be 61730 for 361x170)" << std::endl;
  std::cout << "Crop Size: " << cropSize << std::endl;
  std::cout << "Seed Threshold: " << seedThreshold << std::endl;
  std::cout << "Overlap Limit: " << overlapLimit << std::endl;
  std::cout << "Max Clusters: " << maxClusters << std::endl;

  auto result = get_model_samples(map, 361, 170, seedThreshold, cropSize, overlapLimit, maxClusters);
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
                  if (col < 0 || col >= 170)
                      window(dr + half, dc + half) = -1; // out of bounds in ieta
                  else
                      window(dr + half, dc + half) = dead_grid[row][col];
              }
          }
          event_masks.push_back(std::move(window));
      }
      dead_masks.push_back(std::move(event_masks));
  }
#endif

  // **************** Loop over the PFClusters ****************

  for (const auto& pf : *pfClusters)
  {
    //DetId ebid = pf.seed();
    //EBDetId ebdetid(ebid);
    pfEvent.push_back(iEvent.id().event());
    float corr_E = static_cast<float>(pf.correctedEnergy());
    pfE.push_back(corr_E);

    GlobalPoint gp(pf.position().x(), pf.position().y(), pf.position().z());
    DetId closestCell = barrelGeom->getClosestCell(gp);
    EBDetId ebid(closestCell);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();

    // double exact_eta = gp.eta();
    // double exact_phi = gp.phi().value();
    
    // Get crystal center position
    // GlobalPoint cellCenter = barrelGeom->getGeometry(closestCell)->getPosition();

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

    if (PRINT_DEBUG) {std::cout << " PFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" << std::endl;}
  }

#if INFER
  // ------------------------------- EXAMPLE ------------------------------- 
  // create an input tensor
  // tensorflow::Tensor input1(tensorflow::DT_FLOAT, { 1, cropSize, cropSize, maxClusters});
  // tensorflow::Tensor input2(tensorflow::DT_FLOAT, { 1, maxClusters, 2});
  // tensorflow::Tensor input3(tensorflow::DT_INT32, { 1, maxClusters});
  // tensorflow::Tensor input4(tensorflow::DT_FLOAT, { 1, cropSize, cropSize, maxClusters});

  // // fill the tensor with your input data
  // for (int i = 0; i < maxClusters; i++) {
  //   for (int j = 0; j < cropSize; j++) {
  //     for (int k = 0; k < cropSize; k++) {
  //       input1.tensor<float, 4>()(0, j, k, i) = float(1);
  //       input4.tensor<float, 4>()(0, j, k, i) = float(1);
  //     }
  // }
  //   input2.tensor<float, 3>()(0, i, 0) = float(1);
  //   input2.tensor<float, 3>()(0, i, 1) = float(1);
  //   input3.matrix<int32_t>()(0, i) = int32_t(1);
  // }

  // // run the evaluation
  // std::vector<tensorflow::Tensor> outputs;
  // tensorflow::run(session, {{"inp1:0", input1}, {"inp2:0", input2}, {"inp3:0", input3}, {"inp4:0", input4}},
  //                          {"center:0", "energy:0", "seed:0"}, &outputs);

  // // process the output tensor
  // //auto center = outputs[0].tensor<float, 3>();  // shape [N, 20, 2]
  // auto energy = outputs[1].tensor<float, 3>();  // shape [N, 20, 1]
  // //auto seed   = outputs[2].tensor<float, 3>();  // shape [N, 20, 1]
  // for (int i = 0; i < maxClusters; i++) {
  //   std::cout << "Energy[" << i << "] = " << energy(0, i, 0) << std::endl;
  // }

  // // -----------------------------------------------------------------------------------------
  // int numClusters = X.size();
  // std::cout << "Number of clusters to run through the model: " << numClusters << std::endl;

  // std::vector<std::vector<int32_t>> abs_pos(numClusters, std::vector<int32_t>(maxClusters, 0)); // 1D flattened absolute positions
  // for (int n = 0; n < numClusters; ++n) {
  //     for (int k = 0; k < maxClusters; ++k) {
  //         int center_ieta = static_cast<int>(indices[n][k](0,0));
  //         int center_iphi = static_cast<int>(indices[n][k](1,0));
  //         int val = center_iphi + 170 * center_ieta;
  //         if (val == -171)  // corresponds to (-1, -1)
  //             val = 0;
  //         abs_pos[n][k] = val;
  //     }
  // }

  // int r_eff = (cropSize + overlapLimit) - 1;

  // // Position relative to the center of the effective window
  // std::vector<std::vector<std::array<float, 2>>> rel_pos(
  //     numClusters, std::vector<std::array<float, 2>>(maxClusters));
  // // Initialize with -1
  // for (int n = 0; n < numClusters; ++n) {
  //     for (int i = 0; i < maxClusters; ++i) {
  //         rel_pos[n][i][0] = -1.0f;
  //         rel_pos[n][i][1] = -1.0f;
  //     }
  // }
  // for (int n = 0; n < numClusters; ++n) {
  //     int center_ieta = static_cast<int>(indices[n][0](0,0)); // Highest Edep
  //     int center_iphi = static_cast<int>(indices[n][0](1,0)); // Highest Edep
  //     for (int k = 0; k < maxClusters; ++k) {
  //         int ieta = static_cast<int>(indices[n][k](0,0));
  //         int iphi = static_cast<int>(indices[n][k](1,0));
  //         if (ieta > -1) {rel_pos[n][k][0] = float(ieta - center_ieta) / float(r_eff);}
  //         if (iphi > -1) {rel_pos[n][k][1] = float(iphi - center_iphi) / float(r_eff);}
  //     }
  // }

  // tensorflow::Tensor input1(tensorflow::DT_FLOAT, { numClusters, cropSize, cropSize, maxClusters});
  // tensorflow::Tensor input2(tensorflow::DT_FLOAT, { numClusters, maxClusters, 2});
  // tensorflow::Tensor input3(tensorflow::DT_INT32, { numClusters, maxClusters});
  // tensorflow::Tensor input4(tensorflow::DT_FLOAT, { numClusters, cropSize, cropSize, maxClusters});

  // tensorflow::TTypes<float, 4>::Tensor input_tensor1 = input1.tensor<float, 4>();
  // tensorflow::TTypes<float, 3>::Tensor input_tensor2 = input2.tensor<float, 3>();
  // tensorflow::TTypes<float, 4>::Tensor input_tensor4 = input4.tensor<float, 4>();

  // for (int n = 0; n < numClusters; n++) {
  //   for (int i = 0; i < maxClusters; i++) {
  //     const Eigen::MatrixXf& mat1 = X[n][i]; // 7x7
  //     Eigen::MatrixXf mat2 = dead_masks[n][i].cast<float>(); // 7x7
  //     for (int j = 0; j < cropSize; j++) {
  //       for (int k = 0; k < cropSize; k++) {
  //           input_tensor1(n, j, k, i) = mat1(j, k);
  //           input_tensor4(n, j, k, i) = mat2(j, k);
  //       }
  //     }
  //     input_tensor2(n, i, 0) = rel_pos[n][i][0];
  //     input_tensor2(n, i, 1) = rel_pos[n][i][1];
  //     input3.matrix<int32_t>()(n, i) = abs_pos[n][i];
  //   }
  // }

  // if (PRINT_DEBUG) {
  //   std::cout << "=== INPUT SANITY CHECK ===" << std::endl;
  //   for (int i = 0; i < 3; i++) {
  //       std::cout << "input1(0, 3, 3, " << i << ") = " << input_tensor1(0, 3, 3, i) << std::endl;
  //       std::cout << "input2(0, " << i << ", 0) = " << input_tensor2(0, i, 0) << std::endl;
  //       std::cout << "input2(0, " << i << ", 1) = " << input_tensor2(0, i, 1) << std::endl;
  //       std::cout << "input3(0, " << i << ") = " << input3.matrix<int32_t>()(0, i) << std::endl;
  //       std::cout << "input4(0, 3, 3, " << i << ") = " << input_tensor4(0, 3, 3, i) << std::endl;
  //   }
  // }

  // // print input tensor shapes
  // std::cout << "Input1 shape: (" << input1.dim_size(0) << ", " << input1.dim_size(1) << ", " << input1.dim_size(2) << ", " << input1.dim_size(3) << ")" << std::endl;
  // std::cout << "Input2 shape: (" << input2.dim_size(0) << ", " << input2.dim_size(1) << ", " << input2.dim_size(2) << ")" << std::endl;
  // std::cout << "Input3 shape: (" << input3.dim_size(0) << ", " << input3.dim_size(1) << ")" << std::endl;
  // std::cout << "Input4 shape: (" << input4.dim_size(0) << ", " << input4.dim_size(1) << ", " << input4.dim_size(2) << ", " << input4.dim_size(3) << ")" << std::endl;

  // // run the evaluation
  // std::vector<tensorflow::Tensor> outputs;
  // tensorflow::run(session, {{"inp1:0", input1}, {"inp2:0", input2}, {"inp3:0", input3}, {"inp4:0", input4}},
  //                          {"center:0", "energy:0", "seed:0"}, &outputs);

  // // process the output tensor
  // tensorflow::TTypes<float, 3>::Tensor center_pr = outputs[0].tensor<float, 3>();  // shape [N, 20, 2]
  // tensorflow::TTypes<float, 3>::Tensor energy_pr = outputs[1].tensor<float, 3>();  // shape [N, 20, 1]
  // tensorflow::TTypes<float, 3>::Tensor seed_pr   = outputs[2].tensor<float, 3>();  // shape [N, 20, 1]
  // // convert 
  // std::vector<std::vector<std::pair<float, float>>> centers(numClusters, std::vector<std::pair<float, float>>(maxClusters));
  // std::vector<std::vector<float>> energies(numClusters, std::vector<float>(maxClusters));
  // std::vector<std::vector<float>> seeds(numClusters, std::vector<float>(maxClusters));

  // for (int n = 0; n < numClusters; n++) {
  //     for (int i = 0; i < maxClusters; i++) {
  //         centers[n][i] = {
  //             center_pr(n, i, 0) + indices[n][i](0) - cropSize / 2,
  //             center_pr(n, i, 1) + indices[n][i](1) - cropSize / 2
  //         };
  //         energies[n][i] = energy_pr(n, i, 0) * 100.0f;
  //         seeds[n][i] = seed_pr(n, i, 0);
  //     }
  // }

  // for (int i = 0; i < maxClusters; i++) {
  //   std::cout << "Energy[" << i << "] = " << energies[0][i] << std::endl;
  //   std::cout << "Seed[" << i << "] = " << seeds[0][i] << std::endl;
  //   std::cout << "Center[" << i << "] = (" << centers[0][i].first << ", " << centers[0][i].second << ")" << std::endl;
  // }

  // // print the truth
  // for (size_t i = 0; i < genPDG.size(); ++i) {
  //   std::cout << "GenParticle " << i << ": PDG=" << genPDG[i] 
  //             << ", E=" << genE[i] 
  //             << ", eta=" << genEta[i] + 85 
  //             << ", phi=" << genPhi[i] 
  //             << ", isConverted=" << genIsConverted[i] 
  //             << ", convR=" << genConvR[i] 
  //             << ", convZ=" << genConvZ[i] 
  //             << std::endl;
  // }

  // // Store
  // for (int n = 0; n < numClusters; n++) {
  //   for (int i = 0; i < maxClusters; i++) {
  //       mlEvent.push_back(iEvent.id().event());
  //       mlN.push_back(n);
  //       mlK.push_back(i);
  //       mlCenterX.push_back(centers[n][i].first);
  //       mlCenterY.push_back(centers[n][i].second);
  //       mlEnergy.push_back(energies[n][i]);
  //       mlSeed.push_back(seeds[n][i]);
  //   }
  // }
  // mlTree->Fill();

  // -------------------------------------------------------
  // inp1: (batch, 7, 7, 20) = 1 * 7 * 7 * 20 = 980 floats
  // inp2: (batch, 20, 2)    = 1 * 20 * 2     = 40 floats
  // inp3: (batch, 20)       = 1 * 20         = 20 floats  
  // inp4: (batch, 7, 7, 20) = 1 * 7 * 7 * 20 = 980 floats
  // -------------------------------------------------------

  data_.clear();
  int numClusters = X.size();
  data_.emplace_back(numClusters * cropSize * cropSize * maxClusters, 0.f); // inp1
  data_.emplace_back(numClusters * maxClusters * 2,                  0.f); // inp2
  data_.emplace_back(numClusters * maxClusters,                      0.f); // inp3
  data_.emplace_back(numClusters * cropSize * cropSize * maxClusters, 0.f); // inp4

  if (PRINT_DEBUG) {
    std::cout << "Number of clusters to run through the model: " << numClusters << std::endl;
  }

  std::vector<std::vector<float>> abs_pos(numClusters, std::vector<float>(maxClusters, 0.0f)); // 1D flattened absolute positions
  for (int n = 0; n < numClusters; ++n) {
      for (int k = 0; k < maxClusters; ++k) {
          float center_ieta = static_cast<float>(indices[n][k](0,0));
          float center_iphi = static_cast<float>(indices[n][k](1,0));
          float val = center_iphi + 170.0f * center_ieta;
          if (val == -171.0f)  // corresponds to (-1, -1)
              val = 0.0f;
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

  input_shapes_ = {
    {numClusters, cropSize, cropSize, maxClusters}, // inp1
    {numClusters, maxClusters, 2},                  // inp2
    {numClusters, maxClusters},                     // inp3
    {numClusters, cropSize, cropSize, maxClusters}  // inp4
  };

  auto idx4 = [this](int n, int r, int c, int k) {
    return ((n * cropSize + r) * cropSize + c) * maxClusters + k;
  };
  auto idx2 = [this](int n, int k, int d) {
    return (n * maxClusters + k) * 2 + d;
  };
  auto idx1 = [this](int n, int k) {
    return n * maxClusters + k;
  };

  // Fill from X, rel_pos, abs_pos, dead_masks
  for (int n = 0; n < numClusters; ++n) {
    for (int k = 0; k < maxClusters; ++k) {
      // inp2 from rel_pos
      data_[1][idx2(n, k, 0)] = rel_pos[n][k][0];
      data_[1][idx2(n, k, 1)] = rel_pos[n][k][1];

      // inp3 from abs_pos (now float model)
      data_[2][idx1(n, k)] = abs_pos[n][k];

      // inp1 from X, inp4 from dead_masks
      const Eigen::MatrixXf& xk = X[n][k];
      const Eigen::MatrixXi& mk = dead_masks[n][k];

      for (int r = 0; r < cropSize; ++r) {
        for (int c = 0; c < cropSize; ++c) {
          data_[0][idx4(n, r, c, k)] = xk(r, c);
          data_[3][idx4(n, r, c, k)] = static_cast<float>(mk(r, c));
        }
      }
    }
  }

  // // --- fill inp1: energy crops (batch, 7, 7, 20) ---
  // std::vector<float> &inp1 = data_[0];
  // for (int k = 0; k < maxClusters; ++k)
  //   for (int r = 0; r < cropSize; ++r)
  //     for (int c = 0; c < cropSize; ++c)
  //       inp1[r * cropSize * maxClusters + c * maxClusters + k] = 1.0f; // channel-last layout

  // // --- fill inp2: relative (ieta, iphi) positions (batch, 20, 2) ---
  // std::vector<float> &inp2 = data_[1];
  // for (int k = 0; k < maxClusters; ++k) {
  //   inp2[k * 2 + 0] = 0.0f; // relative ieta
  //   inp2[k * 2 + 1] = 0.0f; // relative iphi
  // }

  // // --- fill inp3: absolute positions (batch, 20) - stored as float ---
  // // ONNXRuntime FloatArrays is vector<vector<float>>, so int32 must be reinterpret-cast
  // std::vector<float> &inp3 = data_[2];
  // for (int k = 0; k < maxClusters; ++k) {
  //   inp3[k * 1 + 0] = 1.0f;
  // }

  // // --- fill inp4: dead channel masks (batch, 7, 7, 20) ---
  // std::vector<float> &inp4 = data_[3];
  // for (int k = 0; k < maxClusters; ++k)
  //   for (int r = 0; r < cropSize; ++r)
  //     for (int c = 0; c < cropSize; ++c)
  //       inp4[r * cropSize * maxClusters + c * maxClusters + k] = 1.0f;

  // --- run inference ---
  std::vector<std::vector<float>> outputs = onnx_->run(input_names_, data_, input_shapes_, {}, numClusters);

  // convert 
  std::vector<float> &center_pr = outputs[0]; // shape (batch, 20, 2)
  std::vector<float> &energy_pr = outputs[1]; // shape (batch, 20, 1)
  std::vector<float> &seed_pr   = outputs[2]; // shape (batch, 20, 1)

  std::vector<std::vector<std::pair<float, float>>> centers(numClusters, std::vector<std::pair<float, float>>(maxClusters));
  std::vector<std::vector<float>> energies(numClusters, std::vector<float>(maxClusters));
  std::vector<std::vector<float>> seeds(numClusters, std::vector<float>(maxClusters));

  auto idx_center = [this](int n, int i, int d) {
      return static_cast<size_t>(n) * this->maxClusters * 2 + i * 2 + d;
  };

  auto idx_scalar = [this](int n, int i) {
      return static_cast<size_t>(n) * this->maxClusters + i;
  };

  for (int n = 0; n < numClusters; ++n) {
      for (int i = 0; i < maxClusters; ++i) {
          float cx = center_pr[idx_center(n, i, 0)];
          float cy = center_pr[idx_center(n, i, 1)];

          centers[n][i] = {
              cx + indices[n][i](0, 0) - cropSize / 2.0f,
              cy + indices[n][i](1, 0) - cropSize / 2.0f
          };

          energies[n][i] = energy_pr[idx_scalar(n, i)] * 100.0f;
          seeds[n][i]    = seed_pr[idx_scalar(n, i)];
      }
  }

  if (PRINT_DEBUG) {
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

  // btlE.clear();
  // btlT.clear();
  // btlX.clear();
  // btlY.clear();
  // btlZ.clear();
  // btlLocX.clear();
  // btlLocY.clear();
  // btlLocZ.clear();
  // btlPDG.clear();
  // btlEvent.clear();

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

void TransClustering::bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) {
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
