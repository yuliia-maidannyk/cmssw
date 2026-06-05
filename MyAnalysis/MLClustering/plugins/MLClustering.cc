#include "DataFormats/Math/interface/GeantUnits.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"
#include "MyAnalysis/MLClustering/interface/MLClustering.h"
#include "MyAnalysis/MLClustering/interface/PreProcessing.h"

#include <fstream>

#define INFER 0
#define PRINT_DEBUG 0

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
MLClustering::MLClustering(const edm::ParameterSet& iConfig)
  : input_names_(iConfig.getParameter<std::vector<std::string>>("input_names")),
    input_shapes_(),
    g4InfoLabel(iConfig.getParameter<std::string>("moduleLabelG4")),
    EBHitsCollection(iConfig.getParameter<std::string>("EBHitsCollection")),
    jobId(iConfig.getParameter<std::string>("jobId")),
    maskedEcalChannelStatusThreshold(iConfig.getParameter<int>("maskedEcalChannelStatusThreshold")),
    cropSize(iConfig.getParameter<int>("cropSize")),
    maxClusters(iConfig.getParameter<int>("maxClusters")),
    overlapLimit(iConfig.getParameter<int>("overlapLimit")),
    seedThreshold(iConfig.getParameter<double>("seedThreshold"))
{
  try {
    auto sessOpts = ONNXRuntime::defaultSessionOptions(Backend::cuda);
    onnx_ = std::make_unique<ONNXRuntime>(
        iConfig.getParameter<std::string>("model_path"), &sessOpts);
    edm::LogInfo("MLClustering") << "ONNX: using CUDA backend";
  } catch (const std::exception& e) {
      edm::LogWarning("MLClustering") 
          << "CUDA failed: " << e.what() << " — falling back to CPU";
      auto cpuOpts = ONNXRuntime::defaultSessionOptions(Backend::cpu);
      cpuOpts.SetIntraOpNumThreads(
          iConfig.getUntrackedParameter<int>("onnxIntraOpThreads", 4));
      onnx_ = std::make_unique<ONNXRuntime>(
          iConfig.getParameter<std::string>("model_path"), &cpuOpts);
  }

  EBrechitCollection_Token = consumes<EBRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBrechitCollection"));
  EBHitsToken = consumes<edm::PCaloHitContainer>(edm::InputTag(std::string(g4InfoLabel), std::string(EBHitsCollection)));
  genParticleToken = consumes<reco::GenParticleCollection>(iConfig.getParameter<edm::InputTag>("genParticles"));
  SimTrackToken = consumes<edm::SimTrackContainer>(iConfig.getParameter<edm::InputTag>("simTrackCollection"));
  SimVertexToken = consumes<edm::SimVertexContainer>(iConfig.getParameter<edm::InputTag>("simVertexCollection"));
  CaloParticle_Token = consumes<CaloParticleCollection>(iConfig.getParameter<edm::InputTag>("CaloParticleCollection"));
  pfClusterToken = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECAL"));
  //barrelGeomToken = esConsumes<CaloSubdetectorGeometry, EcalBarrelGeometryRecord>(edm::ESInputTag("", "EcalBarrel"));
  // ecalGeomToken = esConsumes<CaloGeometry, CaloGeometryRecord>();
  // ecalStatusToken = esConsumes<EcalChannelStatus, EcalChannelStatusRcd>();
  ecalGeomToken = esConsumes<CaloGeometry, CaloGeometryRecord, edm::Transition::BeginRun>();
  ecalStatusToken = esConsumes<EcalChannelStatus, EcalChannelStatusRcd, edm::Transition::BeginRun>();
  magFieldToken = esConsumes<MagneticField, IdealMagneticFieldRecord>();
}

MLClustering::~MLClustering() {}

// ------------ method called for each event  ------------
void MLClustering::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  using namespace std;
  using namespace geant_units::operators;

  clearEventData();

  static unsigned long eventCount = 0;
  ++eventCount;
  if (eventCount % 100 == 0) {
    std::cout << "Processed " << eventCount << " events" << std::endl;
  }

#if INFER
  std::vector<std::vector<int>> dead_grid(361, std::vector<int>(170, 1)); // default = 1

  for (const auto& [detid, bitVec] : EcalAllDeadChannelsBitMap_) {
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
  
  const EBRecHitCollection *EBRecHit = nullptr;
  edm::Handle<EBRecHitCollection> EcalRecHitEB;
  iEvent.getByToken(EBrechitCollection_Token, EcalRecHitEB);
  if (EcalRecHitEB.isValid()) {
    EBRecHit = EcalRecHitEB.product();
  }

  edm::Handle<reco::PFClusterCollection> pfClusters;
  iEvent.getByToken(pfClusterToken, pfClusters);

  const MagneticField* magField_ = &iSetup.getData(magFieldToken);

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
  for (int iv = 0; iv < (int)theSimVertices.size(); ++iv) {
      if (theSimVertices[iv].parentIndex() == -1 || theSimVertices[iv].noParent()) {
          iPV = iv;
          break;
      }
  }

  // Map photon trackId to conversion info
  std::map<unsigned, std::pair<float, float>> photonConversionInfo; // trackId -> (R, Z)

  // Build parent map (do this once, outside the gen particle loop)
  std::map<unsigned int, unsigned int> parentMap;
  for (const auto& simTk : theSimTracks) {
      if (!simTk.noVertex()) {
          const SimVertex& vtx = theSimVertices[simTk.vertIndex()];
          if (!vtx.noParent()) {
              parentMap[simTk.trackId()] = vtx.parentIndex();
          }
      }
  }

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
    float energy = genParticle.energy();
    
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
    float ieta_f = 0.;
    float iphi_f = 0.;
    float ieta_2f = 0.;
    float iphi_2f = 0.;
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

      for (SimTrack* phoTk : photonTracks) {
        if (phoTk->trackId() != bestTrackId) continue;

        // Build RawParticle
        math::XYZTLorentzVector mom = phoTk->momentum();
        math::XYZTLorentzVector pos(vx, vy, vz, 0.);

        RawParticle particle(mom, pos, phoTk->charge());

        float strength = magField_->inTesla(GlobalPoint(vx,vy,vz)).z();
        BaseParticlePropagator prop(particle, 0., 0., strength);
        prop.setMagneticField(strength);
        prop.propagateToEcalEntrance(false);

        if (prop.getSuccess() != 0) {
            math::XYZTLorentzVector ecalPos = prop.particle().vertex();
            double hitEta = ecalPos.eta();
            double hitPhi = ecalPos.phi();
            GlobalPoint gp(ecalPos.x(), ecalPos.y(), ecalPos.z());
            DetId closestCell = barrelGeom_->getClosestCell(gp);
            EBDetId ebid(closestCell);

            ieta = static_cast<float>(ebid.ieta());
            iphi = static_cast<float>(ebid.iphi());

            std::pair<float, float> fractional = barrelGeom_->getClosestCellFractional(gp);
            ieta_f = fractional.first;
            iphi_f = fractional.second;

            auto cell = barrelGeom_->getGeometry(ebid);
            float dEta = hitEta - cell->etaPos();
            float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
            ieta_2f = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
            iphi_2f = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5
        } else {
          // propagation failed, fall back
          edm::LogWarning("MLClustering") << "Propagation to ECAL entrance failed for GenParticle with trackId " << bestTrackId << ". Storing fallback values.";
          ieta_2f = -999.0f; // invalid value
          iphi_2f = -999.0f; // invalid value
        }
      }
#if INFER
      seed_isConverted.push_back(static_cast<bool>(isConverted));
      seed_ieta.push_back(ieta + 85);
      float iphi_shifted = iphi * (180.0 / M_PI);   // (-180, +180) degrees
      iphi_shifted = std::fmod(iphi + 10.0, 360.0); // apply offset, wrap to [0, 360)
      iphi_shifted = iphi + 1.0;                    // 1-based index [1, 360]
      seed_iphi.push_back(iphi_shifted);
#endif
      if (PRINT_DEBUG) {
        std::cout << "  GenParticle momentum points to: ieta=" << ieta << " iphi=" << iphi << std::endl;
      }
      genEvent.push_back(iEvent.id().event());
      genTrackId.push_back(bestTrackId);
      genPEta.push_back(eta);
      genPPhi.push_back(phi);
      genPPt.push_back(pt);
      genEta.push_back(ieta);
      genPhi.push_back(iphi);
      genEtaF.push_back(ieta_f);
      genPhiF.push_back(iphi_f);
      genEta2F.push_back(ieta_2f);
      genPhi2F.push_back(iphi_2f);
      genE.push_back(energy);
      genIsConverted.push_back(isConverted);
      genConvR.push_back(convR);
      genConvZ.push_back(convZ);
    } // end of primary photon loop
  } // end of gen particle loop

  struct TrackNode {
    int      trackId;
    int      pdgId;
    int      parentTrackId;   // -1 if primary
    std::vector<int> daughterTrackIds;
    float    pt, eta, phi, energy;
    int      genPartIdx;       // >=0 if linked to GenParticle
  };

  // ------- Build the decay map -------
  std::map<int, TrackNode> decayTree;  // keyed by trackId

  // First pass: fill each track's own info + find parent
  for (const auto& tk : *simTracks) {
    TrackNode node;
    node.trackId      = tk.trackId();
    node.pdgId        = tk.type();
    node.genPartIdx   = tk.genpartIndex();   // -1 if not a gen particle
    node.parentTrackId = -1;

    const auto& p = tk.momentum();
    node.eta = p.eta();
    node.phi = p.phi();
    node.pt = p.pt();
    node.energy = p.e();

    // Production vertex
    int vtxIdx = tk.vertIndex();
    if (vtxIdx >= 0 && vtxIdx < (int)simVertexes->size()) {
      const SimVertex& vtx = (*simVertexes)[vtxIdx];
      // The vertex's parent is THIS track's parent
      int parentTkId = vtx.parentIndex();  // this is a trackId, not a vector index
      node.parentTrackId = parentTkId;
    }
    decayTree[node.trackId] = node;
  }

  // Second pass: fill daughter lists
  for (auto& [tkId, node] : decayTree) {
    if (node.parentTrackId >= 0) {
      auto it = decayTree.find(node.parentTrackId);
      if (it != decayTree.end()) {
        it->second.daughterTrackIds.push_back(tkId);
      }
    }
  }

  // **************** Loop over the CaloParticles ****************

  int nsimhits = 0;
  CaloMapType caloMap;
  std::map<uint64_t, float> caloScaleMap; // trackId -> energy scale factor
  for (const auto& cp : *caloParticles) {
    nsimhits = 0;
    
    // std::cout << "GenParticle ID " << cp.pdgId() << ", energy = " << cp.energy() << ", eta = " << cp.eta() << ", phi = " << cp.phi() << std::endl;
    
    // Access sim clusters associated with this calo particle
    const auto& simClusters = cp.simClusters();

    // std::cout << "GenParticle has " << simClusters.size() << " associated sim clusters." << std::endl;
    for (const auto& sc : simClusters) {

      if (PRINT_DEBUG) {
        std::cout << *sc << std::endl;
      }
      caloEvent.push_back(iEvent.id().event());
      caloPDG.push_back(sc->pdgId());
      double eta = sc->eta();
      double phi = sc->phi();
      float simClusterTrueE = sc->energy();
      caloPEta.push_back(eta);
      caloPPhi.push_back(phi);
      caloE.push_back(simClusterTrueE);
      caloPPt.push_back(sc->pt());

      const auto& g4tk = *(sc->g4Track_begin());
      int tkId = g4tk.trackId();
      caloTrackId.push_back(tkId);

      float ebEnergy = 0.f;
      float sumWEta  = 0.f;
      float sumSin  = 0.f;
      float sumCos  = 0.f;

      // Get the sim hits associated with this sim cluster
      const auto& hitAndEnergies = sc->hits_and_energies();
      for (const auto& hitAndEnergy : hitAndEnergies) {
        DetId hitId(hitAndEnergy.first);
        float hitE = hitAndEnergy.second;
        if (hitId.subdetId() != EcalBarrel) continue;

        EBDetId ebid(hitId);
        ebEnergy += hitE;
        sumWEta  += hitE * ebid.ieta();
        // Convert iphi to angle, compute circular mean
        float angle = ebid.iphi() * (2.f * M_PI / 360.f);
        sumSin += hitE * std::sin(angle);
        sumCos += hitE * std::cos(angle);

        caloMap[{ebid.ieta(), ebid.iphi()}].push_back({tkId, hitE});
        nsimhits++;
      }
      caloScaleMap[tkId] = (ebEnergy > 0) ? simClusterTrueE / ebEnergy : 1.0f;

      // Energy-weighted centroid in crystal coordinates
      float centroidIEta = (ebEnergy > 0) ? sumWEta / ebEnergy : -999.f;
      float centroidIPhi = -999.f;
      if (ebEnergy > 0) {
          float meanAngle = std::atan2(sumSin / ebEnergy, sumCos / ebEnergy);
          if (meanAngle < 0) meanAngle += 2.f * M_PI;  // wrap to [0, 2pi]
          centroidIPhi = meanAngle * (360.f / (2.f * M_PI));  // back to iphi units [0, 360]
          if (centroidIPhi < 1.f) centroidIPhi += 360.f;      // EB iphi is 1-based
      }
      caloEBEnergy.push_back(ebEnergy);
      caloCentroidIEta.push_back(centroidIEta);
      caloCentroidIPhi.push_back(centroidIPhi);

      auto it = decayTree.find(tkId);
      if (it != decayTree.end()) {
        const TrackNode& node = it->second;
        
        // Walk up to find the gen-level ancestor
        int ancestorId = tkId;
        int current = tkId;
        while (decayTree.count(current) && decayTree[current].parentTrackId >= 0) {
          current = decayTree[current].parentTrackId;
          if (decayTree[current].genPartIdx >= 0)
            ancestorId = current;
        }
        caloParentTrackId.push_back(node.parentTrackId);
        caloAncestorTrackId.push_back(ancestorId);
      }

      // Get production vertex
      double vx = 0., vy = 0., vz = 0.;
      int vtxIdx = g4tk.vertIndex();
      if (vtxIdx >= 0 && vtxIdx < (int)simVertexes->size()) {
          const SimVertex& vtx = (*simVertexes)[vtxIdx];
          vx = vtx.position().x();
          vy = vtx.position().y();
          vz = vtx.position().z();
      }
      caloR.push_back(sqrt(vx*vx + vy*vy));

      // Build RawParticle
      math::XYZTLorentzVector mom = g4tk.momentum();
      math::XYZTLorentzVector pos(vx, vy, vz, 0.);

      RawParticle particle(mom, pos, g4tk.charge());

      float strength = magField_->inTesla(GlobalPoint(vx,vy,vz)).z();
      BaseParticlePropagator prop(particle, 0., 0., strength);
      prop.setMagneticField(strength);
      prop.propagateToEcalEntrance(false);

      if (prop.getSuccess() != 0) {
          math::XYZTLorentzVector ecalPos = prop.particle().vertex();
          double hitEta = ecalPos.eta();
          double hitPhi = ecalPos.phi();

          // Only store if propagated into barrel eta range
          if (std::abs(hitEta) < 1.479) {
            
            GlobalPoint gp(ecalPos.x(), ecalPos.y(), ecalPos.z());
            DetId closestCell = barrelGeom_->getClosestCell(gp);
            EBDetId ebid(closestCell);
            // std::pair<float, float> fractional = barrelGeom_->getClosestCellFractional(gp);

            // caloEta.push_back(fractional.first);
            // caloPhi.push_back(fractional.second);

            int ieta = ebid.ieta();
            int iphi = ebid.iphi();

            auto cell = barrelGeom_->getGeometry(ebid);

            float dEta = hitEta - cell->etaPos();
            float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
            float ieta_2f = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
            float iphi_2f = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5

            caloEta.push_back(ieta_2f);
            caloPhi.push_back(iphi_2f);

          } else {
              // endcap
              caloEta.push_back(-999.0f); // invalid value
              caloPhi.push_back(-999.0f); // invalid value
          }
      } else {
        // propagation failed, fall back
        edm::LogWarning("MLClustering") << "Propagation to ECAL entrance failed for CaloParticle with trackId " << g4tk.trackId() << ". Storing fallback values.";
        caloEta.push_back(-999.0f); // invalid value
        caloPhi.push_back(-999.0f); // invalid value
      }
    } // end of sim cluster loop
  } // end of calo particle loop

  for (const auto& [key, scNumbers] : caloMap) {
    for (const auto& scNumAndE : scNumbers) {
      caloIEta.push_back(key.first);
      caloIPhi.push_back(key.second);
      caloValues.push_back(scNumAndE.first);
      caloValuesE.push_back(scNumAndE.second);
      caloSubEvent.push_back(iEvent.id().event());
    }
  }
  for (const auto& [key, scale] : caloScaleMap) {
    caloScaleId.push_back(key);
    caloScaleFactor.push_back(scale);
  }

  // **************** Loop over the EB SIM hits ****************

  MapType simMap;

  for (std::vector<PCaloHit>::iterator isim = theEBCaloHits.begin(); isim != theEBCaloHits.end(); ++isim) {
    if (isim->time() > 500.) {
      continue;
    }

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
  }

  for (const auto& [key, val] : simMap) {
    simIEta.push_back(key.first);
    simIPhi.push_back(key.second);
    simValues.push_back(val);
    simSubEvent.push_back(iEvent.id().event());
  }

  // **************** Build SimTrack table ****************

  // Map from SimTrack trackId -> index in genParticle collection
  // Only filled for gen-level particles (genPartIdx >= 0)
  std::map<unsigned, int> simTrackIdToGenIdx;
  for (const auto& tk : *simTracks) {
      if (tk.genpartIndex() >= 0) {
          simTrackIdToGenIdx[tk.trackId()] = tk.genpartIndex();
      }
  }

  auto findGenAncestor = [&](unsigned tkId) -> std::pair<unsigned, int> {
    unsigned current = tkId;
    for (int depth = 0; depth < 100; ++depth) {
        auto git = simTrackIdToGenIdx.find(current);
        if (git != simTrackIdToGenIdx.end())
            return {current, git->second};
        auto nit = decayTree.find(current);
        if (nit == decayTree.end() || nit->second.parentTrackId <= 0)
            break;
        current = (unsigned)nit->second.parentTrackId;
    }
    return {0u, -1};
  };

  // Build set of SimTrack trackIds that have EB deposits
  // (from SimClusters, which you still need for hit-level info)
  std::map<unsigned, std::pair<float,float>> tkIdToEBPos; // trackId -> (ieta, iphi) from propagation
  std::map<unsigned, float> tkIdToEBEnergy;               // trackId -> EB deposited energy

  for (const auto& cp : *caloParticles) {
      for (const auto& sc : cp.simClusters()) {
          unsigned tkId = sc->g4Track_begin()->trackId();
          float ebE = 0.f;
          for (const auto& [rawId, hitEnergy] : sc->hits_and_energies()) {
            DetId hitId(rawId);  // wrap the uint32_t in a DetId first
            if (hitId.subdetId() == EcalBarrel)
                ebE += hitEnergy;
          }
          tkIdToEBEnergy[tkId] += ebE;
      }
  }

  // Loop over ALL SimTracks
  for (const auto& tk : *simTracks) {
      unsigned tkId = tk.trackId();

      simTkEvent.push_back(iEvent.id().event());
      simTkTrackId.push_back(tkId);
      simTkPDG.push_back(tk.type());
      simTkE.push_back(tk.momentum().e());
      simTkEta.push_back(tk.momentum().eta());
      simTkPhi.push_back(tk.momentum().phi());
      simTkGenIdx.push_back(tk.genpartIndex());

      // Production vertex
      float vx = 0.f, vy = 0.f, vz = 0.f;
      int vtxIdx = tk.noVertex() ? -1 : tk.vertIndex();
      if (vtxIdx >= 0 && vtxIdx < (int)simVertexes->size()) {
          const SimVertex& vtx = (*simVertexes)[vtxIdx];
          vx = vtx.position().x();
          vy = vtx.position().y();
          vz = vtx.position().z();
      }
      simTkConvR.push_back(std::sqrt(vx*vx + vy*vy));

      // Parent from decayTree
      auto nit = decayTree.find(tkId);
      int parentId = (nit != decayTree.end()) ? nit->second.parentTrackId : -1;
      simTkParentId.push_back(parentId);

      // Gen ancestor
      auto [genAncTkId, genIdx] = findGenAncestor(tkId);
      simTkAncestorId.push_back(genAncTkId);

      // EB energy deposit (0 if this track has no ECAL hits)
      auto eit = tkIdToEBEnergy.find(tkId);
      simTkEBEnergy.push_back(eit != tkIdToEBEnergy.end() ? eit->second : 0.f);

      // Propagate to ECAL entrance for ieta/iphi
      math::XYZTLorentzVector mom = tk.momentum();
      math::XYZTLorentzVector pos(vx, vy, vz, 0.);
      RawParticle particle(mom, pos, tk.charge());
      float strength = magField_->inTesla(GlobalPoint(vx, vy, vz)).z();
      BaseParticlePropagator prop(particle, 0., 0., strength);
      prop.setMagneticField(strength);
      prop.propagateToEcalEntrance(false);

      if (prop.getSuccess() != 0) {
          math::XYZTLorentzVector ecalPos = prop.particle().vertex();
          double hitEta = ecalPos.eta();
          double hitPhi = ecalPos.phi();

          if (std::abs(hitEta) < 1.479) {
            GlobalPoint gp(ecalPos.x(), ecalPos.y(), ecalPos.z());
            DetId closestCell = barrelGeom_->getClosestCell(gp);
            EBDetId ebid(closestCell);

            auto cell = barrelGeom_->getGeometry(ebid);

            int ieta = ebid.ieta();
            int iphi = ebid.iphi();

            float dEta = hitEta - cell->etaPos();
            float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
            float ieta_2f = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
            float iphi_2f = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5

            simTkIEta.push_back(ieta_2f);
            simTkIPhi.push_back(iphi_2f);

            // std::pair<float, float> fractional = barrelGeom_->getClosestCellFractional(gp);
            // simTkIEta.push_back(fractional.first);
            // simTkIPhi.push_back(fractional.second);

          } else {
            simTkIEta.push_back(-999.f);
            simTkIPhi.push_back(-999.f);
          }
      } else {
        simTkIEta.push_back(-999.f);
        simTkIPhi.push_back(-999.f);
      }
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

  std::cout << "Map size: " << map.size() << " (should be 61370 for 361x170)" << std::endl;
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

  // ************** Fine-calo: EB PCaloHits grouped by boundary-crossing track **************
  // trackId -> SimTrack* lookup (boundary vars live on the full SimTrackContainer)
  std::map<unsigned, const SimTrack*> tkById;
  for (const auto& tk : *simTracks) tkById[tk.trackId()] = &tk;

  // Accumulate EB deposits per fine track id (== boundary-crossing parent)
  struct FineAcc { float e=0.f, eEM=0.f, eHad=0.f; int n=0; };
  std::map<int, FineAcc> fineAcc;
  for (const auto& hit : theEBCaloHits) {
    if (hit.time() > 500.) continue;
    FineAcc& a = fineAcc[hit.geantTrackId()];
    a.e    += hit.energy();
    a.eEM  += hit.energyEM();
    a.eHad += hit.energyHad();
    a.n    += 1;
  }

  for (const auto& [tkId, acc] : fineAcc) {
    fineEvent.push_back(iEvent.id().event());
    fineTrackId.push_back(tkId);
    fineNHits.push_back(acc.n);
    fineEBEnergy.push_back(acc.e);

    const SimTrack* tk = tkById.count((unsigned)tkId) ? tkById[(unsigned)tkId] : nullptr;
    finePDG.push_back(tk ? tk->type() : 0);
    fineInitialE.push_back(tk ? (float)tk->momentum().e() : -999.f);
    fineInitialEta.push_back(tk ? (float)tk->momentum().eta() : -999.f);
    fineInitialPhi.push_back(tk ? (float)tk->momentum().phi() : -999.f);
    fineGenIdx.push_back(tk ? tk->genpartIndex() : -1);

    // ECAL entrance straight from the Geant4 boundary crossing
    float entIEta=-999.f, entIPhi=-999.f, entE=-999.f, entX=-999.f, entY=-999.f, entZ=-999.f;
    int crossed = 0;

    if (tk && tk->crossedBoundary()) {
      crossed = 1;
      const auto& pB = tk->getPositionAtBoundary();   // global, cm
      const auto& mB = tk->getMomentumAtBoundary();
      entX = pB.x(); entY = pB.y(); entZ = pB.z(); entE = mB.e();

      float hitEta = pB.eta();
      float hitPhi = pB.phi();

      if (std::abs(hitEta) < 1.479) {
        GlobalPoint gp(pB.x(), pB.y(), pB.z());
        EBDetId ebid(barrelGeom_->getClosestCell(gp));
        int ieta = ebid.ieta();
        int iphi = ebid.iphi();
        auto cell = barrelGeom_->getGeometry(ebid);
        float dEta = hitEta - cell->etaPos();
        float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
        entIEta = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
        entIPhi = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5
      }
    }
    fineCrossedBoundary.push_back(crossed);
    fineEntIEta.push_back(entIEta);
    fineEntIPhi.push_back(entIPhi);
    fineEntE.push_back(entE);
    fineEntX.push_back(entX);
    fineEntY.push_back(entY);
    fineEntZ.push_back(entZ);

    // Parent + gen-level ancestor (reuse decayTree built earlier in analyze)
    int parentId = -1, ancestorId = tkId, current = tkId;
    auto it = decayTree.find(tkId);
    if (it != decayTree.end()) {
      parentId = it->second.parentTrackId;
      while (decayTree.count(current) && decayTree[current].parentTrackId >= 0) {
        current = decayTree[current].parentTrackId;
        if (decayTree.count(current) && decayTree[current].genPartIdx >= 0) ancestorId = current;
      }
    }
    fineParentId.push_back(parentId);
    fineAncestorId.push_back(ancestorId);
  }

  // **************** Loop over the PFClusters ****************

  for (const auto& pf : *pfClusters)
  {
    pfEvent.push_back(iEvent.id().event());
    //float corr_E = static_cast<float>(pf.correctedEnergy());
    float corr_E = static_cast<float>(pf.energy());
    pfE.push_back(corr_E);

    math::XYZPoint pfPos = pf.position();
    GlobalPoint gp(pfPos.x(), pfPos.y(), pfPos.z());
    DetId closestCell = barrelGeom_->getClosestCell(gp);
    EBDetId ebid(closestCell);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();

    float hitEta = pf.positionREP().Eta();
    float hitPhi = pf.positionREP().Phi();

    const CaloCellGeometry* cell = barrelGeom_->getGeometry(ebid);
    //std::cout << *cell << std::endl;

    // fractional position within crystal: -1 (low edge) to +1 (high edge)
    // float localEta = (hitEta - cell->etaPos()) / (cell->etaSpan() / 2.f);
    // float localPhi = (hitPhi - cell->phiPos());
    // while (localPhi >  M_PI) localPhi -= 2*M_PI;
    // while (localPhi < -M_PI) localPhi += 2*M_PI;
    // localPhi = localPhi / (cell->phiSpan() / 2.f);

    // float ieta_2f = ieta + 0.5f * (localEta + 1.0f); // [ieta, ieta+1] crystal center @ 0.5
    // float iphi_2f = iphi + 0.5f * (localPhi + 1.0f); // [iphi, iphi+1] crystal center @ 0.5

    float dEta = hitEta - cell->etaPos();
    float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
    float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
    float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5

    // std::pair<float, float> fractional = barrelGeom_->getClosestCellFractional(gp);
    // float ieta_f = fractional.first;
    // float iphi_f = fractional.second;

    pfEta.push_back(ieta_f);
    pfPhi.push_back(iphi_f);
    pfX.push_back(ieta_f);
    pfY.push_back(iphi_f);

    if (PRINT_DEBUG) {std::cout << " PFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" << std::endl;}
  }

#if INFER

  data_.clear();
  int numClusters = X.size();
  data_.emplace_back(numClusters * cropSize * cropSize * maxClusters, 0.f); // inp1
  data_.emplace_back(numClusters * maxClusters * 2, 0.f); // inp2
  data_.emplace_back(numClusters * maxClusters, 0.f); // inp3
  data_.emplace_back(numClusters * cropSize * cropSize * maxClusters, 0.f); // inp4

  input_shapes_ = {
    {numClusters, cropSize, cropSize, maxClusters}, // inp1
    {numClusters, maxClusters, 2},                  // inp2
    {numClusters, maxClusters},                     // inp3
    {numClusters, cropSize, cropSize, maxClusters}  // inp4
  };

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
  simTkTree->Fill();
  fineTree->Fill();
} // --- end of analyze

void MLClustering::clearEventData() {
  simPDG.clear(); simT.clear(); simE.clear(); simPhi.clear(); simEta.clear();
  simEvent.clear(); simSubEvent.clear(); simTrackId.clear(); simIEta.clear();
  simIPhi.clear(); simValues.clear();

  recoEvent.clear(); recoIEta.clear(); recoIPhi.clear(); recoValues.clear();

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
  caloR.clear();
  caloAncestorTrackId.clear();
  caloParentTrackId.clear();
  caloEBEnergy.clear();
  caloCentroidIEta.clear();
  caloCentroidIPhi.clear();
  caloScaleId.clear();
  caloScaleFactor.clear();

  genE.clear(); genPPt.clear(); genPPhi.clear(); genPEta.clear(); genEta.clear();
  genPhi.clear(); genEtaF.clear(); genPhiF.clear(); genEta2F.clear(); genPhi2F.clear();
  genTrackId.clear(); genEvent.clear(); genIsConverted.clear(); genConvR.clear(); genConvZ.clear();

  pfEvent.clear(); pfPhi.clear(); pfEta.clear(); 
  pfE.clear(); pfX.clear(); pfY.clear(); pfZ.clear();

  simTkEvent.clear(); simTkTrackId.clear(); simTkPDG.clear(); simTkE.clear(); simTkEta.clear();
  simTkPhi.clear(); simTkIEta.clear(); simTkIPhi.clear(); simTkConvR.clear();
  simTkParentId.clear(); simTkAncestorId.clear(); simTkGenIdx.clear(); simTkEBEnergy.clear();

  fineEvent.clear(); fineTrackId.clear(); finePDG.clear(); fineNHits.clear();
  fineEBEnergy.clear();
  fineInitialE.clear(); fineInitialEta.clear(); fineInitialPhi.clear();
  fineCrossedBoundary.clear();
  fineEntIEta.clear(); fineEntIPhi.clear(); fineEntE.clear();
  fineEntX.clear(); fineEntY.clear(); fineEntZ.clear();
  fineParentId.clear(); fineAncestorId.clear(); fineGenIdx.clear();

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
void MLClustering::fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices) {
  geantToIndex_.clear();
  unsigned nTks = simTracks.size();
  if (simVertices.empty()) return;
  
  // Create a map associating geant particle id and position in the SimTrack vector
  for (unsigned it = 0; it < nTks; ++it) {
    geantToIndex_[simTracks[it].trackId()] = it;
  }
}

void MLClustering::bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const& iSetup) {

  // ***************** Get the Ecal Barrel Geometry *****************

  const CaloGeometry& geo = iSetup.getData(ecalGeomToken);
  //const CaloSubdetectorGeometry& barrelGeom_ = iSetup.getData(barrelGeomToken);
  // Get barrel subgeometry from it - no separate token needed
  // const CaloSubdetectorGeometry* barrelGeom_ = geo.getSubdetectorGeometry(DetId::Ecal, EcalBarrel);
  barrelGeom_ = dynamic_cast<const EcalBarrelGeometry*>(geo.getSubdetectorGeometry(DetId::Ecal, EcalBarrel));

  edm::ESHandle<EcalChannelStatus> ecalStatus;
  ecalStatus = iSetup.getHandle(ecalStatusToken);

  // XXX: All the following can be built at the beginning of a job
  // Store EB: DetId <==> vector<int> (subdet, ieta, iphi, status)
  EcalAllDeadChannelsBitMap_.clear();

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
        EcalAllDeadChannelsBitMap_.insert(std::make_pair(detid, bitVec));
      }
    }  // end loop iphi
  }  // end loop ieta

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
  recoTree->Branch("event",     &recoEvent);
  recoTree->Branch("mapIEta",   &recoIEta);
  recoTree->Branch("mapIPhi",   &recoIPhi);
  recoTree->Branch("mapValues", &recoValues);

  caloTree = fs->make<TTree>("caloTree", "A tree with calo hit information");
  caloTree->Branch("radius",    &caloR);
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
  caloTree->Branch("ancestorTrackId", &caloAncestorTrackId);
  caloTree->Branch("parentTrackId", &caloParentTrackId);
  caloTree->Branch("EBEnergy", &caloEBEnergy);
  caloTree->Branch("centroidIEta", &caloCentroidIEta);
  caloTree->Branch("centroidIPhi", &caloCentroidIPhi);
  caloTree->Branch("scaleId", &caloScaleId);
  caloTree->Branch("scaleFactor", &caloScaleFactor);

  genTree = fs->make<TTree>("genTree", "A tree with gen information");
  genTree->Branch("energy",      &genE);
  genTree->Branch("pt",          &genPPt);
  genTree->Branch("phi",         &genPPhi);
  genTree->Branch("eta",         &genPEta);
  genTree->Branch("iphi",        &genPhi);
  genTree->Branch("ieta",        &genEta);
  genTree->Branch("iphiF",       &genPhiF);
  genTree->Branch("ietaF",       &genEtaF);
  genTree->Branch("iphi2F",      &genPhi2F);
  genTree->Branch("ieta2F",      &genEta2F);
  genTree->Branch("trackId",     &genTrackId);
  genTree->Branch("event",       &genEvent);
  genTree->Branch("isConverted", &genIsConverted);
  genTree->Branch("convR",       &genConvR);
  genTree->Branch("convZ",       &genConvZ);

  pfTree = fs->make<TTree>("pfTree", "A tree with PFCluster information");
  pfTree->Branch("energy", &pfE);
  pfTree->Branch("eta",    &pfEta);
  pfTree->Branch("phi",    &pfPhi);
  pfTree->Branch("event",  &pfEvent);
  pfTree->Branch("x",      &pfX);
  pfTree->Branch("y",      &pfY);
  pfTree->Branch("z",      &pfZ);

  simTkTree = fs->make<TTree>("simTkTree", "One row per SimTrack");
  simTkTree->Branch("event",      &simTkEvent);
  simTkTree->Branch("trackId",    &simTkTrackId);
  simTkTree->Branch("pdg",        &simTkPDG);
  simTkTree->Branch("energy",     &simTkE);
  simTkTree->Branch("eta",        &simTkEta);
  simTkTree->Branch("phi",        &simTkPhi);
  simTkTree->Branch("ieta",       &simTkIEta);
  simTkTree->Branch("iphi",       &simTkIPhi);
  simTkTree->Branch("convR",      &simTkConvR);
  simTkTree->Branch("parentId",   &simTkParentId);
  simTkTree->Branch("ancestorId", &simTkAncestorId);
  simTkTree->Branch("genIdx",     &simTkGenIdx);
  simTkTree->Branch("ebEnergy",   &simTkEBEnergy);

  fineTree = fs->make<TTree>("fineTree", "One row per fine-calo track depositing in EB");
  fineTree->Branch("event",           &fineEvent);
  fineTree->Branch("trackId",         &fineTrackId);
  fineTree->Branch("pdg",             &finePDG);
  fineTree->Branch("nHits",           &fineNHits);
  fineTree->Branch("ebEnergy",        &fineEBEnergy);
  fineTree->Branch("initialE",        &fineInitialE);
  fineTree->Branch("initialEta",      &fineInitialEta);
  fineTree->Branch("initialPhi",      &fineInitialPhi);
  fineTree->Branch("crossedBoundary", &fineCrossedBoundary);
  fineTree->Branch("entranceIEta",    &fineEntIEta);
  fineTree->Branch("entranceIPhi",    &fineEntIPhi);
  fineTree->Branch("entranceE",       &fineEntE);
  fineTree->Branch("entranceX",       &fineEntX);
  fineTree->Branch("entranceY",       &fineEntY);
  fineTree->Branch("entranceZ",       &fineEntZ);
  fineTree->Branch("parentId",        &fineParentId);
  fineTree->Branch("ancestorId",      &fineAncestorId);
  fineTree->Branch("genIdx",          &fineGenIdx);

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

DEFINE_FWK_MODULE(MLClustering);
