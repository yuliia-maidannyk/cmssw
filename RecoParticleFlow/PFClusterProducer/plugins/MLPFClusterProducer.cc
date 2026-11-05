#include "DataFormats/Math/interface/GeantUnits.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHit.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFwd.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometryMayOwnPtr.h"
#include "RecoParticleFlow/PFClusterProducer/interface/MLPFClusterProducer.h"
#include "RecoParticleFlow/PFClusterProducer/interface/MLPFClusterPreProcessing.h"
#include "vdt/vdtMath.h"
#include "Geometry/CaloGeometry/interface/TruncatedPyramid.h"
#include <fstream>

#define PRINT_DEBUG 0

std::vector<float> build_energy_map(
    const std::vector<int>&   ieta_vec,
    const std::vector<int>&   iphi_vec,
    const std::vector<float>& energy_vec)
{
    std::vector<float> map(361 * 171, 0.f);
    for (size_t i = 0; i < ieta_vec.size(); ++i) {
        int col = ieta_vec[i] + 85; // shift ieta from [-85, 85] to [0, 170]
        int row = iphi_vec[i];      // iphi → [1,360]
        if (row < 0 || row >= 361) continue;
        if (col < 0 || col >= 171) continue;
        map[row * 171 + col] += energy_vec[i];
    }
    return map;
}

// Copy the EE clusters unchanged (crystals, seed, rechit refs) after the EB clusters,
// and rewrite the EE->PS association so it points to the new positions of the EE clusters.
void append_EE_clusters(const reco::PFClusterCollection& inputClusters,
                        const reco::PFCluster::EEtoPSAssociation& inputAssoc,
                        reco::PFClusterCollection& outputClusters,
                        reco::PFCluster::EEtoPSAssociation& outputAssoc)
{
    std::vector<int> newIndex(inputClusters.size(), -1);  // -1 = cluster not copied
    for (size_t i = 0; i < inputClusters.size(); ++i) {
        if (inputClusters[i].layer() != PFLayer::ECAL_ENDCAP) continue;
        newIndex[i] = outputClusters.size();
        outputClusters.push_back(inputClusters[i]);
    }
    for (auto& [oldIndex, psCluster] : inputAssoc) {
        if (newIndex[oldIndex] < 0) continue;
        outputAssoc.emplace_back(newIndex[oldIndex], psCluster);
    }
}

// ------------ constructor and destructor --------------
MLPFClusterProducer::MLPFClusterProducer(const edm::ParameterSet& iConfig)
  : input_names_(iConfig.getParameter<std::vector<std::string>>("input_names")),
    input_shapes_(),
    maskedEcalChannelStatusThreshold(iConfig.getParameter<int>("maskedEcalChannelStatusThreshold")),
    cropSize(iConfig.getParameter<int>("cropSize")),
    maxClusters(iConfig.getParameter<int>("maxClusters")),
    maxOutputClusters(iConfig.getParameter<int>("maxOutputClusters")),
    overlapLimit(iConfig.getParameter<int>("overlapLimit")),
    seedThreshold(iConfig.getParameter<double>("seedThreshold")),
    outputThreshold(iConfig.getParameter<double>("outputThreshold"))
{
  try {
    auto sessOpts = cms::Ort::ONNXRuntime::defaultSessionOptions(cms::Ort::Backend::cuda);
    onnx_ = std::make_unique<cms::Ort::ONNXRuntime>(
        iConfig.getParameter<std::string>("model_path"), &sessOpts);
    edm::LogInfo("MLPFClusterProducer") << "ONNX: using CUDA backend";
  } catch (const std::exception& e) {
      edm::LogWarning("MLPFClusterProducer") 
          << "CUDA failed: " << e.what() << " — falling back to CPU";
      auto cpuOpts = cms::Ort::ONNXRuntime::defaultSessionOptions(cms::Ort::Backend::cpu);
      cpuOpts.SetIntraOpNumThreads(
          iConfig.getUntrackedParameter<int>("onnxIntraOpThreads", 4));
      onnx_ = std::make_unique<cms::Ort::ONNXRuntime>(
          iConfig.getParameter<std::string>("model_path"), &cpuOpts);
  }
  EBrechitCollection_Token_ = consumes<reco::PFRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBrechitCollection"));
  caloGeomToken_ = esConsumes<CaloGeometry, CaloGeometryRecord>();
  ecalStatusToken_ = esConsumes<EcalChannelStatus, EcalChannelStatusRcd>();
  produces<reco::PFClusterCollection>();
  produces<reco::PFCluster::EEtoPSAssociation>();        // forwarded from corrector
  produces<reco::PFRecHitCollection>("mlPFRecHitsEB");
  eeClusterToken_ = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("inputEE"));
  eeToPSToken_ = consumes<reco::PFCluster::EEtoPSAssociation>(iConfig.getParameter<edm::InputTag>("inputEEtoPSAssoc"));
}

MLPFClusterProducer::~MLPFClusterProducer() {}

// ------------ method called for each event  ------------
void MLPFClusterProducer::produce(edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  using namespace std;
  using namespace geant_units::operators;

  if (PRINT_DEBUG) {
      std::cout << "DEBUG MLPFCluster: start produce" << std::endl;
  }

  auto const& eeClusters = iEvent.get(eeClusterToken_);
  if (PRINT_DEBUG) {
      std::cout << "DEBUG MLPFCluster: got EE clusters, size=" << eeClusters.size() << std::endl;
  }

  auto const& assoc = iEvent.get(eeToPSToken_);
  if (PRINT_DEBUG) {
      std::cout << "DEBUG MLPFCluster: got EEtoPSAssoc, size=" << assoc.size() << std::endl;
  }

  const reco::PFRecHitCollection *EBRecHit = nullptr;
  edm::Handle<reco::PFRecHitCollection> EcalRecHitEB;
  iEvent.getByToken(EBrechitCollection_Token_, EcalRecHitEB);
  if (EcalRecHitEB.isValid()) {
    EBRecHit = EcalRecHitEB.product();
  }

  const auto& caloGeom = iSetup.getData(caloGeomToken_);
  const CaloSubdetectorGeometry* ebGeom = caloGeom.getSubdetectorGeometry(DetId::Ecal, EcalBarrel);

  if (ecalChannelStatusWatcher_.check(iSetup)) {
    auto const& ecalStatus = iSetup.getData(ecalStatusToken_);
    deadChannelGrid_.assign(361 * 171, 1);  // default: usable channel

    // This runs once per EcalChannelStatusRcd IOV, rather than once per event.
    for (int ieta = -85; ieta <= 85; ++ieta) {
      for (int iphi = 0; iphi <= 360; ++iphi) {
        if (!EBDetId::validDetId(ieta, iphi))
          continue;

        EBDetId const detid(ieta, iphi, EBDetId::ETAPHIMODE);
        auto const chit = ecalStatus.find(detid);
        int const status = (chit != ecalStatus.end()) ? chit->getStatusCode() & 0x1F : -1;
        if (status < maskedEcalChannelStatusThreshold)
          continue;

        int value = 1;
        if (status >= 3 && status <= 10)
          value = 2;  // noisy/wrong gain
        else if (status > 10)
          value = 3;  // completely dead
        deadChannelGrid_[iphi * 171 + ieta + 85] = value;
      }
    }
  }

  // **************** Loop over the EB REC hits ****************

  std::vector<int> ieta_vec;
  std::vector<int> iphi_vec;
  std::vector<float> energy_vec;

  for (const reco::PFRecHit& recHit : *EBRecHit) {
    DetId hitId(recHit.detId());
    if (hitId.det() != DetId::Ecal || hitId.subdetId() != EcalBarrel) continue;
    EBDetId ebid(hitId);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    ieta_vec.push_back(ieta);
    iphi_vec.push_back(iphi);
    energy_vec.push_back(recHit.energy());
  }

  // **************** Prepare inference data ****************

  std::vector<float> map = build_energy_map(ieta_vec, iphi_vec, energy_vec);

  if (PRINT_DEBUG) {
    std::cout << "Map size: " << map.size() << " (should be 61370 for 361x171)" << std::endl;
    std::cout << "Crop Size: " << cropSize << std::endl;
    std::cout << "Seed Threshold: " << seedThreshold << std::endl;
    std::cout << "Overlap Limit: " << overlapLimit << std::endl;
    std::cout << "Max Clusters: " << maxClusters << std::endl;
    std::cout << "Max Output Clusters: " << maxOutputClusters << std::endl;
  }

  for (int r = 0; r < 361; ++r) {
    for (int c = 0; c < 171; ++c) {
        if (deadChannelGrid_[r * 171 + c] == 3) {
            map[r * 171 + c] = 0.0f;
        }
    }
  }

  auto result = get_model_samples(map, 361, 171, seedThreshold, cropSize, overlapLimit, maxClusters);
  std::vector<std::vector<Eigen::MatrixXf>>& X = result.X; // (N, maxClusters, cropSize, cropSize)
  std::vector<std::vector<Eigen::MatrixXf>>& indices = result.indices; // (N, maxClusters, 2)

  if (PRINT_DEBUG) {
    std::cout << "X shape: (" << X.size() << ", " << (X.empty() ? 0 : X[0].size()) << ", " 
                << (X.empty() || X[0].empty() ? 0 : X[0][0].rows()) << ", " 
                << (X.empty() || X[0].empty() ? 0 : X[0][0].cols()) << ")" << std::endl;
    std::cout << "Indices shape: (" << indices.size() << ", " << (indices.empty() ? 0 : indices[0].size()) << ", 2)" << std::endl;
  }

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
                  if (col < 0 || col >= 171)
                      window(dr + half, dc + half) = -1; // out of bounds in ieta
                  else
                      window(dr + half, dc + half) = deadChannelGrid_[row * 171 + col];
              }
          }
          event_masks.push_back(std::move(window));
      }
      dead_masks.push_back(std::move(event_masks));
  }

  data_.clear();
  int numClusters = X.size();
  std::unique_ptr<std::vector<reco::PFCluster>> clusters = std::make_unique<reco::PFClusterCollection>();
  if (numClusters == 0) {
    // No EB ML clusters — put empty PFRecHit collection
    iEvent.put(std::make_unique<reco::PFRecHitCollection>(), "mlPFRecHitsEB");
    auto psAssoc = std::make_unique<reco::PFCluster::EEtoPSAssociation>();
    append_EE_clusters(eeClusters, assoc, *clusters, *psAssoc);
    iEvent.put(std::move(clusters));
    iEvent.put(std::move(psAssoc));
    return;
}
  data_.emplace_back(numClusters * maxClusters * cropSize * cropSize, 0.f); // inp1
  data_.emplace_back(numClusters * maxClusters * 2, 0.f); // inp2
  data_.emplace_back(numClusters * maxClusters, 0.f); // inp3
  data_.emplace_back(numClusters * maxClusters * cropSize * cropSize, 0.f); // inp4

  input_shapes_ = {
    {numClusters, maxClusters, cropSize, cropSize}, // inp1
    {numClusters, maxClusters, 2},                  // inp2
    {numClusters, maxClusters},                     // inp3
    {numClusters, maxClusters, cropSize, cropSize}  // inp4
  };

  std::vector<std::vector<float>> abs_pos(numClusters, std::vector<float>(maxClusters, 0.0f)); // 1D flattened absolute positions
  for (int n = 0; n < numClusters; ++n) {
      for (int k = 0; k < maxClusters; ++k) {
          float center_ieta = static_cast<float>(indices[n][k](0,0));
          float center_iphi = static_cast<float>(indices[n][k](1,0));
          float val = center_iphi + 171.0f * center_ieta;
          if (val == -172.0f)  // corresponds to (-1, -1)
              val = 0.0f;
          abs_pos[n][k] = val;
      }
  }

  int r_eff = (cropSize + overlapLimit) - 1;

  // Position relative to the center of the effective window
  std::vector<std::vector<std::array<float, 2>>> rel_pos(numClusters, std::vector<std::array<float, 2>>(maxClusters));

  // Initialize with -1
  for (int n = 0; n < numClusters; ++n) {
      for (int i = 0; i < maxClusters; ++i) {
          rel_pos[n][i][0] = -1.0f;
          rel_pos[n][i][1] = -1.0f;
      }
  }
  // (d + 180) % 360 - 180, Python-style modulo
  auto wrap180 = [](float d) {
    float w = std::fmod(d + 180.0f, 360.0f);
    if (w < 0.f)
      w += 360.0f;
    return w - 180.0f;
  };
  for (int n = 0; n < numClusters; ++n) {
    int center_iphi = static_cast<int>(indices[n][0](0, 0)); // Highest Edep
    int center_ieta = static_cast<int>(indices[n][0](1, 0)); // Highest Edep
    for (int k = 0; k < maxClusters; ++k) {
      int iphi = static_cast<int>(indices[n][k](0, 0));
      int ieta = static_cast<int>(indices[n][k](1, 0));
      if (iphi > -1) rel_pos[n][k][0] = wrap180(float(iphi - center_iphi)) / float(r_eff);
      if (ieta > -1) rel_pos[n][k][1] = float(ieta - center_ieta) / float(r_eff);
    }
  }

  // Reshaping functions for input
  // data_[0] and data_[3] are laid out as (N, maxClusters, cropSize, cropSize)
  auto idx4 = [this](int n, int k, int r, int c) {
    return ((n * maxClusters + k) * cropSize + r) * cropSize + c;
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
      data_[1][idx2(n, k, 0)] = rel_pos[n][k][0];
      data_[1][idx2(n, k, 1)] = rel_pos[n][k][1];
      data_[2][idx1(n, k)] = abs_pos[n][k];

      // inp1 from X, inp4 from dead_masks
      const Eigen::MatrixXf& xk = X[n][k];
      const Eigen::MatrixXi& mk = dead_masks[n][k];

      for (int r = 0; r < cropSize; ++r) {
        for (int c = 0; c < cropSize; ++c) {
          data_[0][idx4(n, k, r, c)] = xk(r, c);
          data_[3][idx4(n, k, r, c)] = static_cast<float>(mk(r, c));
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

  std::vector<std::vector<std::pair<float, float>>> centers(numClusters, std::vector<std::pair<float, float>>(maxOutputClusters));
  std::vector<std::vector<float>> energies(numClusters, std::vector<float>(maxOutputClusters));
  std::vector<std::vector<float>> seeds(numClusters, std::vector<float>(maxOutputClusters));

  // Reshaping functions for outputs
  auto wrap360 = [](float v) {
    float w = std::fmod(v - 1.0f, 360.0f);
    if (w < 0.f)
    w += 360.0f;
    return w + 1.0f;
  };
  auto out2 = [this](int n, int k, int d) { return (n * maxOutputClusters + k) * 2 + d; };
  auto out1 = [this](int n, int k) { return n * maxOutputClusters + k; };

  for (int n = 0; n < numClusters; ++n) {
    float indexCenter0 = indices[n][0](0, 0);  // highest Edep, same for all i
    float indexCenter1 = indices[n][0](1, 0);

    for (int i = 0; i < maxOutputClusters; ++i) {
      float cx = center_pr[out2(n, i, 0)];
      float cy = center_pr[out2(n, i, 1)];

      float yc0 = cx * (13 / 2.0f) + wrap360(indexCenter0);
      float yc1 = cy * (13 / 2.0f) + indexCenter1 - 85.0f;  // back to [-85, 85]
      yc0 = wrap360(yc0);

      centers[n][i] = {yc0, yc1};
      energies[n][i] = energy_pr[out1(n, i)] * 120.;  // back to GeV
      seeds[n][i] = seed_pr[out1(n, i)];
    }
  } // end of convert loop

  // if (PRINT_DEBUG) {
  //   for (int i = 0; i < maxClusters; i++) {
  //     std::cout << "Energy[" << i << "] = " << energies[0][i] << std::endl;
  //     std::cout << "Seed[" << i << "] = " << seeds[0][i] << std::endl;
  //     std::cout << "Center[" << i << "] = (" << centers[0][i].first << ", " << centers[0][i].second << ")" << std::endl;
  //   }
  // }

  // First pass: collect valid clusters and build PFRecHits
  struct ClusterData {
      float energy, time;
      math::XYZPoint pos;
      EBDetId ebId;
  };
  std::vector<ClusterData> validClusters;

  for (int n = 0; n < numClusters; ++n) {
      for (int i = 0; i < maxOutputClusters; ++i) {
        float cx = centers[n][i].first;
        float cy = centers[n][i].second;
        float si = seeds[n][i];
        float eni = energies[n][i];
        if (si <= outputThreshold) continue;
        int ieta = static_cast<int>(std::round(cy));
        int iphi = static_cast<int>(std::round(cx));
        if (PRINT_DEBUG) {
            std::cout << "DEBUG Cluster " << n << "," << i 
                        << ": seed=" << si 
                        << ", energy=" << eni 
                        << ", center=(ieta=" << cy << ", iphi=" << cx << ")" 
                        << std::endl;
        }
        ieta = (ieta == 0) ? ((cy < 0.5f) ? -1 : 1) : ieta; // ensure valid ieta
        if (!EBDetId::validDetId(ieta, iphi)) continue;
        EBDetId ebId(ieta, iphi);
        auto cellGeom = ebGeom->getGeometry(ebId);
        //const GlobalPoint& pos = cellGeom->getPosition();
        //const float maxDepth = _param_X0 * (_param_T0_EB + vdt::fast_log(eni));
        const float maxDepth = 0.0f; // Use the front face of the crystal for now
        const GlobalPoint p0 = static_cast<const TruncatedPyramid*>(cellGeom.get())->getPosition(maxDepth);

        float deta = cy - ieta;
        float dphi = cx - iphi;

        // Eta neighbor at shower depth
        GlobalPoint pEta = p0;
        if (EBDetId::validDetId(ieta + (deta >= 0 ? 1 : -1), iphi)) {
            auto cell_eta = ebGeom->getGeometry(EBDetId(ieta + (deta >= 0 ? 1 : -1), iphi));
            pEta = static_cast<const TruncatedPyramid*>(cell_eta.get())->getPosition(maxDepth);
        }

        // Phi neighbor at shower depth
        GlobalPoint pPhi = p0;
        if (EBDetId::validDetId(ieta, iphi + (dphi >= 0 ? 1 : -1))) {
            auto cell_phi = ebGeom->getGeometry(EBDetId(ieta, iphi + (dphi >= 0 ? 1 : -1)));
            pPhi = static_cast<const TruncatedPyramid*>(cell_phi.get())->getPosition(maxDepth);
        }

        // Combine: depth-corrected center + lateral sub-crystal offset
        float x = p0.x() + std::abs(deta) * (pEta.x() - p0.x()) + std::abs(dphi) * (pPhi.x() - p0.x());
        float y = p0.y() + std::abs(deta) * (pEta.y() - p0.y()) + std::abs(dphi) * (pPhi.y() - p0.y());
        float z = p0.z() + std::abs(deta) * (pEta.z() - p0.z()) + std::abs(dphi) * (pPhi.z() - p0.z());

        int seedIPhi = indices[n][i](0, 0);
        int seedIEta = indices[n][i](1, 0) - 85;
        seedIPhi = (seedIPhi - 1 + 360) % 360 + 1; // ensure valid iphi
        seedIEta = (seedIEta == 0) ? 1 : seedIEta; // ensure valid ieta
        EBDetId seed(seedIEta, seedIPhi);

        validClusters.push_back({eni, -1.0f, math::XYZPoint(x, y, z), seed});
      }
  }

  // Build PFRecHit collection
  auto pfRecHits = std::make_unique<reco::PFRecHitCollection>();
  pfRecHits->reserve(validClusters.size());
  for (auto const& cd : validClusters) {
      reco::PFRecHit hit(
          CaloCellGeometryMayOwnPtr(ebGeom->getGeometry(cd.ebId)),
          cd.ebId.rawId(),
          PFLayer::ECAL_BARREL,
          cd.energy
      );
      pfRecHits->push_back(std::move(hit));
  }
  auto pfRecHitHandle = iEvent.put(std::move(pfRecHits), "mlPFRecHitsEB");

  // Build PFCluster collection with valid refs
  for (size_t idx = 0; idx < validClusters.size(); ++idx) {
      auto const& cd = validClusters[idx];
      reco::PFCluster cluster;
      cluster.setLayer(PFLayer::ECAL_BARREL);
      cluster.setEnergy(cd.energy);
      cluster.setTime(cd.time);
      cluster.setCorrectedEnergy(cd.energy);
      cluster.setPosition(cd.pos);
      cluster.setSeed(cd.ebId);
    //   cluster.addHitAndFraction(cd.ebId.rawId(), 1.0f);
      reco::PFRecHitRef ref(pfRecHitHandle, idx);
      cluster.addRecHitFraction(reco::PFRecHitFraction(ref, 1.0f));
      if (PRINT_DEBUG) {
          std::cout << "DEBUG cluster: eta=" << cluster.eta() 
              << " phi=" << cluster.phi()
              << " recHitFractions=" << cluster.recHitFractions().size()
              << " ref.isNonnull=" << ref.isNonnull()
              << " ref->position=(" << ref->position().x() << "," 
              << ref->position().y() << "," << ref->position().z() << ")"
          << std::endl;
      }
      cluster.calculatePositionREP();  // populate positionREP_ used by CalibratedPFCluster::eta/phi
      clusters->push_back(cluster);
  }

  // Append EE clusters — copy kinematics + hitsAndFractions only,
  // deliberately NOT copying PFRecHitFractions to avoid InvalidReference
  if (PRINT_DEBUG) {
      std::cout << "DEBUG EE clusters total=" << eeClusters.size() << std::endl;
  }
  int nEB=0, nEE=0;
  for (auto const& c : eeClusters) {
      if (c.layer() == PFLayer::ECAL_BARREL) nEB++;
      if (c.layer() == PFLayer::ECAL_ENDCAP) nEE++;
  }
  if (PRINT_DEBUG) {
      std::cout << "DEBUG EB=" << nEB << " EE=" << nEE << std::endl;
  }

  auto psAssoc = std::make_unique<reco::PFCluster::EEtoPSAssociation>();
  append_EE_clusters(eeClusters, assoc, *clusters, *psAssoc);
  iEvent.put(std::move(clusters));
  iEvent.put(std::move(psAssoc));

} // --- end of produce

DEFINE_FWK_MODULE(MLPFClusterProducer);
