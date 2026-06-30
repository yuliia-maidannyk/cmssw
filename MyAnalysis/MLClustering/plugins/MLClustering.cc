#include "DataFormats/Math/interface/GeantUnits.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"
#include "MyAnalysis/MLClustering/interface/MLClustering.h"
#include "MyAnalysis/MLClustering/interface/PreProcessing.h"

#include <fstream>

#define PRINT_DEBUG 0

// ------------ constructor and destructor --------------
MLClustering::MLClustering(const edm::ParameterSet& iConfig)
  : input_names_(iConfig.getParameter<std::vector<std::string>>("input_names")),
    input_shapes_(),
    g4InfoLabel(iConfig.getParameter<std::string>("moduleLabelG4")),
    EBSimHitCollection(iConfig.getParameter<std::string>("EBSimHitCollection")),
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

  EBRecHitToken = consumes<EBRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBRecHitCollection"));
  EBSimHitToken = consumes<edm::PCaloHitContainer>(edm::InputTag(std::string(g4InfoLabel), std::string(EBSimHitCollection)));
  genParticleToken = consumes<reco::GenParticleCollection>(iConfig.getParameter<edm::InputTag>("genParticles"));
  SimTrackToken = consumes<edm::SimTrackContainer>(iConfig.getParameter<edm::InputTag>("simTrackCollection"));
  SimVertexToken = consumes<edm::SimVertexContainer>(iConfig.getParameter<edm::InputTag>("simVertexCollection"));
  pfClusterToken   = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECAL"));
  mlpfClusterToken = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECALML"));
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

  // ***************** Get the collections *****************

  edm::Handle<edm::PCaloHitContainer> EBSimHitHandle;
  iEvent.getByToken(EBSimHitToken, EBSimHitHandle);

  std::vector<PCaloHit> EBSimHit;
  EBSimHit.insert(EBSimHit.end(), EBSimHitHandle->begin(), EBSimHitHandle->end());

  edm::Handle<reco::GenParticleCollection> genParticles;
  iEvent.getByToken(genParticleToken, genParticles);
  
  const EBRecHitCollection *EBRecHit = nullptr;
  edm::Handle<EBRecHitCollection> EBRecHitHandle;
  iEvent.getByToken(EBRecHitToken, EBRecHitHandle);
  if (EBRecHitHandle.isValid()) {
    EBRecHit = EBRecHitHandle.product();
  }

  edm::Handle<reco::PFClusterCollection> pfClusters;
  iEvent.getByToken(pfClusterToken, pfClusters);

  edm::Handle<reco::PFClusterCollection> mlpfClusters;
  iEvent.getByToken(mlpfClusterToken, mlpfClusters);

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

            int ieta = ebid.ieta();
            int iphi = ebid.iphi();

            auto cell = barrelGeom_->getGeometry(ebid);
            float dEta = hitEta - cell->etaPos();
            float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
            ieta_f = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
            iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5
        } else {
          // propagation failed, fall back
          edm::LogWarning("MLClustering") << "Propagation to ECAL entrance failed for GenParticle with trackId " << bestTrackId << ". Storing fallback values.";
          ieta_f = -999.0f; // invalid value
          iphi_f = -999.0f; // invalid value
        }
      }
      genEvent.push_back(iEvent.id().event());
      genTrackId.push_back(bestTrackId);
      genPEta.push_back(eta);
      genPPhi.push_back(phi);
      genPPt.push_back(pt);
      genIEta.push_back(ieta);
      genIPhi.push_back(iphi);
      genEtaF.push_back(ieta_f);
      genPhiF.push_back(iphi_f);
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

  // **************** Loop over the EB SIM hits ****************

  std::map<std::pair<std::pair<int,int>, int>, float> simCellTrack; // key: ((ieta, iphi), ancestorId) -> energy
  std::map<std::pair<int,int>, float> simCellTotal; // key: (ieta, iphi) -> total energy
  // Build a map from DetId -> geantTrackIds
  std::map<uint32_t, std::vector<uint32_t>> detIdToTrackIds;

  for (std::vector<PCaloHit>::iterator simHit = EBSimHit.begin(); simHit != EBSimHit.end(); ++simHit) {
    if (simHit->time() > 500.) {
      continue;
    }
    detIdToTrackIds[simHit->id()].push_back(simHit->geantTrackId());
    EBDetId ebid(simHit->id());

    // std::cout << " CaloHit " << simHit->getName() << "\n"
    //           << " DetID = " << simHit->id() << " EBDetId = " << ebid.ieta() << " " << ebid.iphi() << "\n"
    //           << " Time = " << simHit->time() << "\n"
    //           << " Track Id = " << simHit->geantTrackId() << "\n"
    //           << " Energy = " << simHit->energy() << std::endl;

    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    auto cell = std::make_pair(ebid.ieta(), ebid.iphi());
    simCellTotal[cell] += simHit->energy();

    int tkId = simHit->geantTrackId();
    int ancestorId = tkId, current = tkId;
    auto it = decayTree.find(tkId);
    if (it != decayTree.end()) {
        while (decayTree.count(current) && decayTree[current].parentTrackId >= 0) {
            current = decayTree[current].parentTrackId;
            if (decayTree.count(current) && decayTree[current].genPartIdx >= 0)
                ancestorId = current;
        }
    }
    simCellTrack[{cell, ancestorId}] += simHit->energy();

    simTrackId.push_back(simHit->geantTrackId());
    simEta.push_back(ieta);
    simPhi.push_back(iphi);
    simEvent.push_back(iEvent.id().event());
  }

  for (const auto& [key, ei] : simCellTrack) {
    const auto& [cell, ancestorId] = key;
    float total = simCellTotal.at(cell);
    simSubEvent.push_back(iEvent.id().event());
    simIEta.push_back(cell.first);
    simIPhi.push_back(cell.second);
    simValues.push_back(ei);
    simMapFraction.push_back(total > 0 ? ei / total : 0.f);
    simMapAncestor.push_back(ancestorId);
  }

  // **************** Loop over the EB REC hits ****************

  std::vector<int> ieta_vec;
  std::vector<int> iphi_vec;
  std::vector<float> energy_vec;
  std::map<std::pair<std::pair<int,int>, int>, float> recoCellTrack; // key: ((ieta, iphi), ancestorId) -> energy
  std::map<std::pair<int,int>, float> recoCellTotal; // key: (ieta, iphi) -> total energy

  for (EcalRecHitCollection::const_iterator recHit = EBRecHit->begin(); recHit != EBRecHit->end(); ++recHit) {
    EBDetId ebid = EBDetId(recHit->id());
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    auto cell = std::make_pair(ebid.ieta(), ebid.iphi());
    recoCellTotal[cell] += recHit->energy();

    ieta_vec.push_back(ieta);
    iphi_vec.push_back(iphi);
    energy_vec.push_back(recHit->energy());

    // Find the ancestor track ID for this hit
    uint32_t detId = recHit->detid().rawId();
    if (!detIdToTrackIds.count(detId)) {continue;} // no associated SimHits
    const auto& trackIds = detIdToTrackIds.at(detId);
    int tkId = trackIds.empty() ? -1 : trackIds[0]; // take the first associated track ID
    int ancestorId = tkId, current = tkId;
    auto it = decayTree.find(tkId);
    if (it != decayTree.end()) {
        while (decayTree.count(current) && decayTree[current].parentTrackId >= 0) {
            current = decayTree[current].parentTrackId;
            if (decayTree.count(current) && decayTree[current].genPartIdx >= 0)
                ancestorId = current;
        }
    }
    recoCellTrack[{cell, ancestorId}] += recHit->energy();
  }
  for (const auto& [key, ei] : recoCellTrack) {
    const auto& [cell, ancestorId] = key;
    float total = recoCellTotal.at(cell);
    recoMapIEta.push_back(cell.first);
    recoMapIPhi.push_back(cell.second);
    recoMapValues.push_back(ei);
    recoMapFraction.push_back(total > 0 ? ei / total : 0.f);
    recoMapAncestor.push_back(ancestorId);
    recoMapEvent.push_back(iEvent.id().event());
  }
  for (const auto& [key, ei] : recoCellTotal) {
    recoIEta.push_back(key.first);
    recoIPhi.push_back(key.second);
    recoValues.push_back(ei);
    recoEvent.push_back(iEvent.id().event());
  }

  // ************** Fine-calo: EB PCaloHits grouped by boundary-crossing track **************
  // trackId -> SimTrack* lookup (boundary vars live on the full SimTrackContainer)
  std::map<unsigned, const SimTrack*> tkById;
  for (const auto& tk : *simTracks) tkById[tk.trackId()] = &tk;

  // Accumulate EB deposits per fine track id (== boundary-crossing parent)
  struct FineAcc {
    float ei=0.f;
    int ni=0;
  };
  std::map<int, FineAcc> fineAcc;
  for (const auto& hit : EBSimHit) {
    if (hit.time() > 500.) continue;
    FineAcc& a = fineAcc[hit.geantTrackId()];
    a.ei += hit.energy();
    a.ni += 1;
  }

  for (const auto& [tkId, acc] : fineAcc) {
    fineEvent.push_back(iEvent.id().event());
    fineTrackId.push_back(tkId);
    fineNHits.push_back(acc.ni);
    fineE.push_back(acc.ei);

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

  // ---- Merge fine tracks that land in the same crystal ----
  // key: (ieta_bin, iphi_bin), value: surviving trackId
  std::map<std::pair<int,int>, int> binToSurvivor;
  // old trackId -> surviving trackId
  std::map<int, int> mergedTrackId;

  struct MergedTrack {
    float ei=0.f, entIEta=0.f, entIPhi=0.f;
    int ni=0;
  };
  std::map<int, MergedTrack> mergedTracks; // keyed by surviving trackId

  for (const auto& [tkId, acc] : fineAcc) {
    const SimTrack* tk = tkById.count((unsigned)tkId) ? tkById[(unsigned)tkId] : nullptr;
    if (!tk || !tk->crossedBoundary()) {
      mergedTrackId[tkId] = tkId;
      mergedTracks[tkId].ei += acc.ei;
      mergedTracks[tkId].ni += acc.ni;
      continue;
    }

    // recompute entIEta/entIPhi (same logic as before)
    const auto& pB = tk->getPositionAtBoundary();
    float hitEta = pB.eta(), hitPhi = pB.phi();
    float entIEta = -999.f, entIPhi = -999.f;

    if (std::abs(hitEta) < 1.479) {
      GlobalPoint gp(pB.x(), pB.y(), pB.z());
      EBDetId ebid(barrelGeom_->getClosestCell(gp));
      auto cell = barrelGeom_->getGeometry(ebid);
      float dEta = hitEta - cell->etaPos();
      float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
      entIEta = ebid.ieta() + dEta / cell->etaSpan() + 0.5f;
      entIPhi = ebid.iphi() + dPhi / cell->phiSpan() + 0.5f;
    }

    auto bin = std::make_pair((int)std::floor(entIEta), (int)std::floor(entIPhi));
    if (binToSurvivor.count(bin)) {
      // merge into survivor
      int survivor = binToSurvivor[bin];
      mergedTrackId[tkId] = survivor;
      mergedTracks[survivor].ei += acc.ei;
      mergedTracks[survivor].ni += acc.ni;
      mergedTracks[survivor].entIEta = (mergedTracks[survivor].entIEta * (mergedTracks[survivor].ni - acc.ni) + entIEta * acc.ni) / mergedTracks[survivor].ni;
      mergedTracks[survivor].entIPhi = (mergedTracks[survivor].entIPhi * (mergedTracks[survivor].ni - acc.ni) + entIPhi * acc.ni) / mergedTracks[survivor].ni;
    } else {
      binToSurvivor[bin] = tkId;
      mergedTrackId[tkId] = tkId;
      mergedTracks[tkId].ei = acc.ei;
      mergedTracks[tkId].ni = acc.ni;
      mergedTracks[tkId].entIEta = entIEta;
      mergedTracks[tkId].entIPhi = entIPhi;
    }
  }
  // ---- Fine-calo hits_and_fractions analog ----
  // Two-pass: first accumulate total energy per cell,
  // then compute per-(cell,track) fraction exactly like SimCluster::hits_and_fractions().

  std::map<std::pair<int,int>, float> cellTotalE;
  std::map<std::pair<std::pair<int,int>, int>, float> cellTrackE; // ((ieta,iphi), trackId) -> E

  for (const auto& hit : EBSimHit) {
      if (hit.time() > 500.) continue;
      EBDetId ebid(hit.id());
      auto cell = std::make_pair(ebid.ieta(), ebid.iphi());
      cellTotalE[cell] += hit.energy();
      //cellTrackE[{cell, hit.geantTrackId()}] += hit.energy();
      cellTrackE[{cell, mergedTrackId.count(hit.geantTrackId()) ? mergedTrackId.at(hit.geantTrackId()) : hit.geantTrackId()}] += hit.energy();
  }

  for (const auto& [key, ei] : cellTrackE) {
      const auto& [cell, tkId] = key;
      float total = cellTotalE.at(cell);
      fineMapEvent.push_back(iEvent.id().event());
      fineMapIEta.push_back(cell.first);
      fineMapIPhi.push_back(cell.second);
      fineMapTrackId.push_back(tkId);
      fineMapEnergy.push_back(ei);
      fineMapFraction.push_back(total > 0.f ? ei / total : 0.f);

      int ancestorId = tkId, current = tkId;
      auto it = decayTree.find(tkId);
      if (it != decayTree.end()) {
        while (decayTree.count(current) && decayTree[current].parentTrackId >= 0) {
          current = decayTree[current].parentTrackId;
          if (decayTree.count(current) && decayTree[current].genPartIdx >= 0) ancestorId = current;
        }
      }
      fineMapAncestor.push_back(ancestorId);
  }

  // ***************** Loop over the PFClusters ****************

  for (const auto& pf : *pfClusters)
  {
    pfEvent.push_back(iEvent.id().event());
    float corr_E = static_cast<float>(pf.correctedEnergy());
    //float corr_E = static_cast<float>(pf.energy());
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

    float dEta = hitEta - cell->etaPos();
    float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
    float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
    float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5

    pfEta.push_back(ieta_f);
    pfPhi.push_back(iphi_f);

    if (PRINT_DEBUG) {std::cout << " PFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" 
      << " with eta_f=" << ieta_f << ", phi_f=" << iphi_f << std::endl;}
  }

  // ***************** Loop over the MLPFClusters ****************

  for (const auto& pf : *mlpfClusters)
  {
    if (pf.layer() != PFLayer::ECAL_BARREL) continue;
    mlEvent.push_back(iEvent.id().event());
    float corr_E = static_cast<float>(pf.correctedEnergy());
    mlE.push_back(corr_E);

    math::XYZPoint pfPos = pf.position();
    GlobalPoint gp(pfPos.x(), pfPos.y(), pfPos.z());
    DetId closestCell = barrelGeom_->getClosestCell(gp);
    EBDetId ebid(closestCell);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();

    float hitEta = pf.positionREP().Eta();
    float hitPhi = pf.positionREP().Phi();

    const CaloCellGeometry* cell = barrelGeom_->getGeometry(ebid);

    float dEta = hitEta - cell->etaPos();
    float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
    float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f;
    float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f;

    mlEta.push_back(ieta_f);
    mlPhi.push_back(iphi_f);

    if (PRINT_DEBUG) {std::cout << " MLPFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" 
      << " with eta_f=" << ieta_f << ", phi_f=" << iphi_f << std::endl;}
  }

  // **************** Fill all trees ****************
  mlTree->Fill();
  simTree->Fill();
  recoTree->Fill();
  genTree->Fill();
  pfTree->Fill();
  fineTree->Fill();
} // --- end of analyze

void MLClustering::clearEventData() {
  simPDG.clear(); simT.clear(); simE.clear(); simPhi.clear(); simEta.clear();
  simEvent.clear(); simSubEvent.clear(); simTrackId.clear(); simIEta.clear();
  simIPhi.clear(); simValues.clear(); simMapTrackId.clear(); simMapFraction.clear();
  simMapAncestor.clear();

  recoEvent.clear(); recoIEta.clear(); recoIPhi.clear(); recoValues.clear();
  recoMapEvent.clear(); recoMapIEta.clear(); recoMapIPhi.clear(); recoMapValues.clear();
  recoMapFraction.clear(); recoMapAncestor.clear();

  genE.clear(); genPPt.clear(); genPPhi.clear(); genPEta.clear();
  genIEta.clear(); genIPhi.clear(); genEtaF.clear(); genPhiF.clear();
  genTrackId.clear(); genEvent.clear(); genIsConverted.clear(); genConvR.clear(); genConvZ.clear();

  pfEvent.clear(); pfPhi.clear(); pfEta.clear(); pfE.clear();
  mlEvent.clear(); mlPhi.clear(); mlEta.clear(); mlE.clear();

  fineEvent.clear(); fineTrackId.clear(); finePDG.clear(); fineNHits.clear();
  fineE.clear(); fineInitialE.clear(); fineInitialEta.clear(); fineInitialPhi.clear();
  fineCrossedBoundary.clear();
  fineEntIEta.clear(); fineEntIPhi.clear(); fineEntE.clear();
  fineEntX.clear(); fineEntY.clear(); fineEntZ.clear();
  fineParentId.clear(); fineAncestorId.clear(); fineGenIdx.clear();
  fineMapEvent.clear(); fineMapIEta.clear(); fineMapIPhi.clear(); fineMapTrackId.clear();
  fineMapEnergy.clear(); fineMapFraction.clear(); fineMapAncestor.clear();
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
}

void MLClustering::beginJob() {

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
  simTree->Branch("mapFraction",&simMapFraction);
  simTree->Branch("mapTrackId", &simMapTrackId);
  simTree->Branch("mapAncestor", &simMapAncestor);

  recoTree = fs->make<TTree>("recoTree", "A tree with reconstructed hit information");
  recoTree->Branch("event",  &recoEvent);
  recoTree->Branch("ieta",   &recoIEta);
  recoTree->Branch("iphi",   &recoIPhi);
  recoTree->Branch("energy", &recoValues);
  recoTree->Branch("mapEvent",    &recoMapEvent);
  recoTree->Branch("mapIEta",     &recoMapIEta);
  recoTree->Branch("mapIPhi",     &recoMapIPhi);
  recoTree->Branch("mapValues",   &recoMapValues);
  recoTree->Branch("mapFraction", &recoMapFraction);
  recoTree->Branch("mapAncestor", &recoMapAncestor);

  genTree = fs->make<TTree>("genTree", "A tree with gen information");
  genTree->Branch("energy",      &genE);
  genTree->Branch("pt",          &genPPt);
  genTree->Branch("phi",         &genPPhi);
  genTree->Branch("eta",         &genPEta);
  genTree->Branch("iphi",        &genIPhi);
  genTree->Branch("ieta",        &genIEta);
  genTree->Branch("iphiF",       &genPhiF);
  genTree->Branch("ietaF",       &genEtaF);
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

  mlTree = fs->make<TTree>("mlTree", "A tree with MLPFCluster information");
  mlTree->Branch("energy", &mlE);
  mlTree->Branch("eta",    &mlEta);
  mlTree->Branch("phi",    &mlPhi);
  mlTree->Branch("event",  &mlEvent);

  fineTree = fs->make<TTree>("fineTree", "One row per fine-calo track depositing in EB");
  fineTree->Branch("event",           &fineEvent);
  fineTree->Branch("trackId",         &fineTrackId);
  fineTree->Branch("pdg",             &finePDG);
  fineTree->Branch("nHits",           &fineNHits);
  fineTree->Branch("energy",          &fineE);
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
  fineTree->Branch("mapEvent",       &fineMapEvent);
  fineTree->Branch("mapIEta",        &fineMapIEta);
  fineTree->Branch("mapIPhi",        &fineMapIPhi);
  fineTree->Branch("mapTrackId",     &fineMapTrackId);
  fineTree->Branch("mapEnergy",      &fineMapEnergy);
  fineTree->Branch("mapFraction",    &fineMapFraction);
  fineTree->Branch("mapAncestor",    &fineMapAncestor);
}

DEFINE_FWK_MODULE(MLClustering);
