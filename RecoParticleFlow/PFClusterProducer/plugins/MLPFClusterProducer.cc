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

// ------------ constructor and destructor --------------
MLPFClusterProducer::MLPFClusterProducer(const edm::ParameterSet& iConfig)
  : input_names_(iConfig.getParameter<std::vector<std::string>>("input_names")),
    input_shapes_(),
    jobId(iConfig.getParameter<std::string>("jobId")),
    maskedEcalChannelStatusThreshold(iConfig.getParameter<int>("maskedEcalChannelStatusThreshold")),
    cropSize(iConfig.getParameter<int>("cropSize")),
    maxClusters(iConfig.getParameter<int>("maxClusters")),
    overlapLimit(iConfig.getParameter<int>("overlapLimit")),
    seedThreshold(iConfig.getParameter<double>("seedThreshold")),
    outputThreshold(iConfig.getParameter<double>("outputThreshold")),
    _param_T0_EB(iConfig.getParameter<double>("T0_EB")),
    _param_X0(iConfig.getParameter<double>("X0"))
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
  EBrechitCollection_Token_ = consumes<EBRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBrechitCollection"));
  caloGeomToken_ = esConsumes<CaloGeometry, CaloGeometryRecord>();
  ecalStatusToken_ = esConsumes<EcalChannelStatus, EcalChannelStatusRcd>();
  produces<reco::PFClusterCollection>();
  produces<reco::PFCluster::EEtoPSAssociation>();        // forwarded from corrector
  produces<reco::PFRecHitCollection>("mlPFRecHitsEB");
  eeClusterToken_ = consumes<reco::PFClusterCollection>(
    iConfig.getParameter<edm::InputTag>("inputEE"));
  eeToPSToken_ = consumes<reco::PFCluster::EEtoPSAssociation>(
    iConfig.getParameter<edm::InputTag>("inputEEtoPSAssoc"));
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

  static unsigned long eventCount = 0;
  ++eventCount;

  const EBRecHitCollection *EBRecHit = nullptr;
  edm::Handle<EBRecHitCollection> EcalRecHitEB;
  iEvent.getByToken(EBrechitCollection_Token_, EcalRecHitEB);
  if (EcalRecHitEB.isValid()) {
    EBRecHit = EcalRecHitEB.product();
  }

  const auto& caloGeom = iSetup.getData(caloGeomToken_);
  const CaloSubdetectorGeometry* ebGeom = caloGeom.getSubdetectorGeometry(DetId::Ecal, EcalBarrel);

  edm::ESHandle<EcalChannelStatus> ecalStatus;
  ecalStatus = iSetup.getHandle(ecalStatusToken_);

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

  std::vector<std::vector<int>> dead_grid(361, std::vector<int>(171, 1)); // default = 1

  for (const auto& [detid, bitVec] : EcalAllDeadChannelsBitMap_) {
      int ieta = bitVec[1];
      int iphi = bitVec[2];
      int status = bitVec[3];

      int ieta_shifted = ieta + 85;
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

  // **************** Loop over the EB REC hits ****************

  std::vector<int> ieta_vec;
  std::vector<int> iphi_vec;
  std::vector<float> energy_vec;

  for (EcalRecHitCollection::const_iterator recHit = EBRecHit->begin(); recHit != EBRecHit->end(); ++recHit) {
    EBDetId ebid = EBDetId(recHit->id());
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    ieta_vec.push_back(ieta);
    iphi_vec.push_back(iphi);
    energy_vec.push_back(recHit->energy());
  }

  // **************** Prepare inference data ****************

  std::vector<float> map = build_energy_map(ieta_vec, iphi_vec, energy_vec);

  if (PRINT_DEBUG) {
    std::cout << "Map size: " << map.size() << " (should be 61370 for 361x171)" << std::endl;
    std::cout << "Crop Size: " << cropSize << std::endl;
    std::cout << "Seed Threshold: " << seedThreshold << std::endl;
    std::cout << "Overlap Limit: " << overlapLimit << std::endl;
    std::cout << "Max Clusters: " << maxClusters << std::endl;
  }

  for (int r = 0; r < 361; ++r) {
    for (int c = 0; c < 171; ++c) {
        if (dead_grid[r][c] == 3) {
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
                      window(dr + half, dc + half) = dead_grid[row][col];
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

    // Put EE clusters only (no EB)
    for (auto const& c : eeClusters) {
        if (c.layer() != PFLayer::ECAL_ENDCAP) continue;
        reco::PFCluster eeCluster;
        eeCluster.setLayer(PFLayer::ECAL_ENDCAP);
        eeCluster.setEnergy(c.energy());
        eeCluster.setCorrectedEnergy(c.correctedEnergy());
        eeCluster.setPosition(c.position());
        eeCluster.setTime(c.time());
        eeCluster.calculatePositionREP();  // populate positionREP_ used by CalibratedPFCluster::eta/phi
        clusters->push_back(eeCluster);
    }
    iEvent.put(std::move(clusters));
    iEvent.put(std::make_unique<reco::PFCluster::EEtoPSAssociation>(assoc));
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

  if (PRINT_DEBUG) {
    std::cout << "Number of clusters to run through the model: " << numClusters << std::endl;
  }

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
          data_[0][idx4(n, k, r, c)] = xk(r, c);
          data_[3][idx4(n, k, r, c)] = static_cast<float>(mk(r, c));
        }
      }
    }
  }

  // --- run inference ---
  if (PRINT_DEBUG) {
    std::cout << "Input shapes:" << std::endl;
    for (size_t i = 0; i < input_shapes_.size(); ++i) {
      std::cout << "  inp" << i+1 << ": [";
      for (auto d : input_shapes_[i]) std::cout << d << ",";
      std::cout << "] size=" << data_[i].size() << std::endl;
    }
  }
  std::vector<std::vector<float>> outputs = onnx_->run(input_names_, data_, input_shapes_, {}, numClusters);

  // convert 
  std::vector<float> &center_pr = outputs[0]; // shape (batch, 20, 2)
  std::vector<float> &energy_pr = outputs[1]; // shape (batch, 20, 1)
  std::vector<float> &seed_pr   = outputs[2]; // shape (batch, 20, 1)

  std::vector<std::vector<std::pair<float, float>>> centers(numClusters, std::vector<std::pair<float, float>>(maxClusters));
  std::vector<std::vector<float>> energies(numClusters, std::vector<float>(maxClusters));
  std::vector<std::vector<float>> seeds(numClusters, std::vector<float>(maxClusters));

  // Reshaping functions for outputs
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
              cx * (cropSize / 2.0f) + indices[n][i](0, 0),
              cy * (cropSize / 2.0f) + indices[n][i](1, 0)
          };

          energies[n][i] = energy_pr[idx_scalar(n, i)] * 100.0f;
          seeds[n][i] = seed_pr[idx_scalar(n, i)];
      }
  }

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
      for (int i = 0; i < maxClusters; ++i) {
          float cx = centers[n][i].first;
          float cy = centers[n][i].second - 85.0f; // shift back to [-85, 85] for ieta
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

          validClusters.push_back({eni, -1.0f, math::XYZPoint(x, y, z), ebId});
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
      cluster.addHitAndFraction(cd.ebId.rawId(), 1.0f);
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

  for (auto const& c : eeClusters) {
    if (c.layer() != PFLayer::ECAL_ENDCAP) continue;
    reco::PFCluster eeCluster;
    eeCluster.setLayer(PFLayer::ECAL_ENDCAP);
    eeCluster.setEnergy(c.energy());
    eeCluster.setCorrectedEnergy(c.correctedEnergy());
    eeCluster.setPosition(c.position());
    eeCluster.setTime(c.time());
    eeCluster.calculatePositionREP();  // populate positionREP_ used by CalibratedPFCluster::eta/phi
    clusters->push_back(eeCluster);
  }
  iEvent.put(std::move(clusters));  // empty instance name — IS particleFlowClusterECAL

  // Forward EEtoPSAssociation unchanged
  iEvent.put(std::make_unique<reco::PFCluster::EEtoPSAssociation>(assoc));
} // --- end of produce

DEFINE_FWK_MODULE(MLPFClusterProducer);
