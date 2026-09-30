#include "RecoTICL/LayerClustering/plugins/BarrelClusTEXAlgo.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/CaloGeometry/interface/TruncatedPyramid.h"
#include "DataFormats/CaloRecHit/interface/CaloID.h"
#include <cmath>

using namespace hgcal_clustering;

BarrelClusTEXAlgo::BarrelClusTEXAlgo(const edm::ParameterSet& iConfig)
    : HGCalClusteringAlgoBase(
          (HGCalClusteringAlgoBase::VerbosityLevel)iConfig.getUntrackedParameter<unsigned int>("verbosity", 3),
          reco::CaloCluster::undefined),
      modelPath_(iConfig.getParameter<std::string>("modelPath")),
      inputNames_(iConfig.getParameter<std::vector<std::string>>("inputNames")),
      cropSize_(iConfig.getParameter<int>("cropSize")),
      maxClusters_(iConfig.getParameter<int>("maxClusters")),
      overlapLimit_(iConfig.getParameter<int>("overlapLimit")),
      seedThreshold_(iConfig.getParameter<double>("seedThreshold")),
      outputThreshold_(iConfig.getParameter<double>("outputThreshold")),
      maskedEcalChannelStatusThreshold_(iConfig.getParameter<int>("maskedEcalChannelStatusThreshold")) {
      // param_T0_EB_(iConfig.getParameter<double>("T0_EB")),
      // param_X0_(iConfig.getParameter<double>("X0")) {
 
  try {
    auto gpuOptions = cms::Ort::ONNXRuntime::defaultSessionOptions(cms::Ort::Backend::cuda);
    onnx_ = std::make_unique<cms::Ort::ONNXRuntime>(modelPath_, &gpuOptions);
    edm::LogInfo("BarrelClusTEXAlgo") << "ONNX: using CUDA backend";
  } catch (const std::exception& e) {
    auto cpuOptions = cms::Ort::ONNXRuntime::defaultSessionOptions(cms::Ort::Backend::cpu);
    onnx_ = std::make_unique<cms::Ort::ONNXRuntime>(modelPath_, &cpuOptions);
    edm::LogWarning("BarrelClusTEXAlgo") << "CUDA failed: " << e.what() << " - falling back to CPU";
  }
}

void BarrelClusTEXAlgo::setClusTEXTokens(edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeomToken,
                                         edm::ESGetToken<EcalChannelStatus, EcalChannelStatusRcd> ecalStatusToken) {
  caloGeomToken_ = caloGeomToken;
  ecalStatusToken_ = ecalStatusToken;
}

void BarrelClusTEXAlgo::getEventSetupPerAlgorithm(const edm::EventSetup& iSetup) {
  const CaloGeometry& caloGeom = iSetup.getData(caloGeomToken_);
  ebGeom_ = caloGeom.getSubdetectorGeometry(DetId::Ecal, EcalBarrel);
 
  // The channel status only changes between runs, so rebuild the map only then.
  if (ecalChannelStatusWatcher_.check(iSetup)) {
    updateDeadChannelGrid(iSetup);
  }
 
  cellsMap_.assign(nRows * nCols, 0.f);
}

void BarrelClusTEXAlgo::updateDeadChannelGrid(const edm::EventSetup& iSetup) {
  auto const& ecalStatus = iSetup.getData(ecalStatusToken_);
  deadChannelGrid_.assign(nRows * nCols, 1);  // default: usable channel
  // This runs once per EcalChannelStatusRcd IOV, rather than once per event.
  for (int ieta = -etaOffset; ieta <= etaOffset; ++ieta) {
    for (int iphi = 0; iphi < nRows; ++iphi) {
      if (!EBDetId::validDetId(ieta, iphi))
        continue;

      const EBDetId detid(ieta, iphi, EBDetId::ETAPHIMODE);
      const auto chit = ecalStatus.find(detid);
      const int status = (chit != ecalStatus.end()) ? chit->getStatusCode() & 0x1F : -1;
      if (status < maskedEcalChannelStatusThreshold_)
        continue; 

      int value = 1;
      if (status >= 3 && status <= 10)
        value = 2;  // noisy/wrong gain
      else if (status > 10)
        value = 3;  // completely dead
      deadChannelGrid_[iphi * nCols + ieta + etaOffset] = value;
    }
  }
}

void BarrelClusTEXAlgo::populate(const reco::PFRecHitCollection& hits) {
  for (const reco::PFRecHit& hit : hits) {
    DetId detId(hit.detId());
    if (detId.det() != DetId::Ecal || detId.subdetId() != EcalBarrel)
      continue;
    EBDetId ebid(detId);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    cellsMap_[iphi * nCols + ieta + etaOffset] += hit.energy();
  }
}

void BarrelClusTEXAlgo::makeClusters() {
  predictedClusters_.clear();

  killDeadChannels(); // set dead channels to zero in the energy map

  // Crop the energy map into seed windows around E>seedThreshold
  ProcessingResult result =
  get_model_samples(cellsMap_, nRows, nCols, seedThreshold_, cropSize_, overlapLimit_, maxClusters_);
  
  std::vector<std::vector<Eigen::MatrixXf>>& windows = result.X; // (N, maxClusters, cropSize, cropSize)
  std::vector<std::vector<Eigen::MatrixXf>>& indices = result.indices; // (N, maxClusters, 2)

  int numClusters = windows.size();

  if (numClusters == 0)
    return;

  // Prepare additional variables
  std::vector<std::vector<float>> abs_pos = get_abs_pos(numClusters, indices);
  std::vector<std::vector<std::array<float, 2>>> rel_pos = get_rel_pos(numClusters, indices);
  std::vector<std::vector<Eigen::MatrixXi>> dead_masks = get_channel_status_mask(indices);

  data_.clear();

  data_.emplace_back(numClusters * maxClusters_ * cropSize_ * cropSize_, 0.f); // inp1
  data_.emplace_back(numClusters * maxClusters_ * 2, 0.f); // inp2
  data_.emplace_back(numClusters * maxClusters_, 0.f); // inp3
  data_.emplace_back(numClusters * maxClusters_ * cropSize_ * cropSize_, 0.f); // inp4

  inputShapes_ = {
    {numClusters, maxClusters_, cropSize_, cropSize_}, // inp1
    {numClusters, maxClusters_, 2},                  // inp2
    {numClusters, maxClusters_},                     // inp3
    {numClusters, maxClusters_, cropSize_, cropSize_}  // inp4
  };

  // Reshaping functions for input
  // data_[0] and data_[3] are laid out as (N, maxClusters, cropSize, cropSize)
  auto idx4 = [this](int n, int k, int r, int c) {
    return ((n * maxClusters_ + k) * cropSize_ + r) * cropSize_ + c;
  };
  auto idx2 = [this](int n, int k, int d) {
    return (n * maxClusters_ + k) * 2 + d;
  };
  auto idx1 = [this](int n, int k) {
    return n * maxClusters_ + k;
  };

  // Fill from X, rel_pos, abs_pos, dead_masks
  for (int n = 0; n < numClusters; ++n) {
    for (int k = 0; k < maxClusters_; ++k) {
      // inp2 from rel_pos
      data_[1][idx2(n, k, 0)] = rel_pos[n][k][0];
      data_[1][idx2(n, k, 1)] = rel_pos[n][k][1];

      // inp3 from abs_pos (now float model)
      data_[2][idx1(n, k)] = abs_pos[n][k];

      // inp1 from X, inp4 from dead_masks
      const Eigen::MatrixXf& xk = windows[n][k];
      const Eigen::MatrixXi& mk = dead_masks[n][k];

      for (int r = 0; r < cropSize_; ++r) {
        for (int c = 0; c < cropSize_; ++c) {
          data_[0][idx4(n, k, r, c)] = xk(r, c);
          data_[3][idx4(n, k, r, c)] = static_cast<float>(mk(r, c));
        }
      }
    }
  }

  LogDebug("BarrelClusTEXAlgo") << "Running ClusTEX on " << numClusters << " window groups";
  std::vector<std::vector<float>> outputs = onnx_->run(inputNames_, data_, inputShapes_, {}, numClusters);

  // convert 
  std::vector<float> &center_pr = outputs[0]; // shape (batch, maxClusters, 2)
  std::vector<float> &energy_pr = outputs[1]; // shape (batch, maxClusters, 1)
  std::vector<float> &seed_pr   = outputs[2]; // shape (batch, maxClusters, 1)

  std::vector<std::vector<std::pair<float, float>>> centers(numClusters, std::vector<std::pair<float, float>>(maxClusters_));
  std::vector<std::vector<float>> energies(numClusters, std::vector<float>(maxClusters_));
  std::vector<std::vector<float>> seeds(numClusters, std::vector<float>(maxClusters_));

  // Reshaping functions for outputs
  auto idx_center = [this](int n, int i, int d) {
    return static_cast<size_t>(n) * this->maxClusters_ * 2 + i * 2 + d;
  };

  auto idx_scalar = [this](int n, int i) {
    return static_cast<size_t>(n) * this->maxClusters_ + i;
  };

  for (int n = 0; n < numClusters; ++n) {
    for (int i = 0; i < maxClusters_; ++i) {
      float cx = center_pr[idx_center(n, i, 0)];
      float cy = center_pr[idx_center(n, i, 1)];

      centers[n][i] = {
        cx * (cropSize_ / 2.0f) + indices[n][i](0, 0),
        cy * (cropSize_ / 2.0f) + indices[n][i](1, 0) - etaOffset // shift back to [-85, 85] for ieta
      };

      energies[n][i] = energy_pr[idx_scalar(n, i)] * energyScale; // scale back to GeV
      seeds[n][i] = seed_pr[idx_scalar(n, i)];
    }
  }

  predictedClusters_.reserve(numClusters * maxClusters_);
  for (int n = 0; n < numClusters; ++n) {
    for (int i = 0; i < maxClusters_; ++i) {
        float cx = centers[n][i].first;
        float cy = centers[n][i].second;
        float si = seeds[n][i];
        float eni = energies[n][i];
        if (si < outputThreshold_) continue;
        predictedClusters_.emplace_back(PredictedCluster{eni, -1.0f, cx, cy});
    }
  }
}

namespace {
  // Move ieta by one crystal, skipping the non-existent ieta = 0.
  // Returns false if we walked off the end of the barrel.
  bool stepIeta(int ieta, int step, int& moved) {
    moved = ieta + step;
    if (moved == 0)
      moved += step;  // there is no ieta = 0, so jump straight over it
    return std::abs(moved) <= 85;
  }

  // Move iphi by one crystal, wrapping around the barrel (iphi is 1..360).
  int stepIphi(int iphi, int step) {
    return (iphi - 1 + step + 360) % 360 + 1;
  }
}  // namespace

std::vector<reco::BasicCluster> BarrelClusTEXAlgo::getClusters(bool) {

  clusters_v_.clear();
  clusters_v_.reserve(predictedClusters_.size());

  for (const PredictedCluster& cluster : predictedClusters_) {
    // DetId seed = EBDetId(1, 1); 
    // std::vector<std::pair<DetId, float>> hitsAndFractions;
    // for (int it = 0; it < 1; it++) {
    //   hitsAndFractions.emplace_back(seed, 1.0f);
    // }
    math::XYZPoint position = calculatePosition(cluster.cx, cluster.cy);

    // The seed is the crystal containing the predicted centre.
    // Round the same way calculatePosition does, so the two agree.
    int seedIEta = static_cast<int>(std::round(cluster.cy));
    int seedIPhi = static_cast<int>(std::round(cluster.cx));
    if (!EBDetId::validDetId(seedIEta, seedIPhi))
      continue;
    EBDetId seed(seedIEta, seedIPhi);

    std::vector<std::pair<DetId, float>> hitsAndFractions;
    hitsAndFractions.reserve(9);

    for (int dEta = -1; dEta <= 1; ++dEta) {
      int ieta = 0;
      if (!stepIeta(seedIEta, dEta, ieta))
        continue;  // off the end of the barrel in eta

      for (int dPhi = -1; dPhi <= 1; ++dPhi) {
        int iphi = stepIphi(seedIPhi, dPhi);
        if (!EBDetId::validDetId(ieta, iphi))
          continue;
        if (cellsMap_[iphi * nCols + ieta + etaOffset] > 0.f) {
          hitsAndFractions.emplace_back(EBDetId(ieta, iphi), 1.0f);
        }
      }
    }
    clusters_v_.emplace_back(cluster.energy, position, reco::CaloID::DET_ECAL_BARREL, hitsAndFractions, algoId_);
    clusters_v_.back().setSeed(seed);
    clusters_v_.back().setCorrectedEnergy(cluster.energy);
  }
  return clusters_v_;
}

math::XYZPoint BarrelClusTEXAlgo::calculatePosition(const float& cx, const float& cy) const {
  int ieta = static_cast<int>(std::round(cy));
  int iphi = static_cast<int>(std::round(cx));
  if (!EBDetId::validDetId(ieta, iphi)) {
    return math::XYZPoint(0.f, 0.f, 0.f);
  }
  EBDetId ebId(ieta, iphi);
  auto cellGeom = ebGeom_->getGeometry(ebId);
  //const GlobalPoint& pos = cellGeom->getPosition();
  //const float maxDepth = _param_X0 * (_param_T0_EB + vdt::fast_log(eni));
  const float maxDepth = 0.0f; // Use the front face of the crystal for now
  const GlobalPoint p0 = static_cast<const TruncatedPyramid*>(cellGeom.get())->getPosition(maxDepth);

  float deta = cy - ieta;
  float dphi = cx - iphi;

  // Eta neighbor at shower depth
  GlobalPoint pEta = p0;
  if (EBDetId::validDetId(ieta + (deta >= 0 ? 1 : -1), iphi)) {
    auto cell_eta = ebGeom_->getGeometry(EBDetId(ieta + (deta >= 0 ? 1 : -1), iphi));
    pEta = static_cast<const TruncatedPyramid*>(cell_eta.get())->getPosition(maxDepth);
  }

  // Phi neighbor at shower depth
  GlobalPoint pPhi = p0;
  if (EBDetId::validDetId(ieta, iphi + (dphi >= 0 ? 1 : -1))) {
    auto cell_phi = ebGeom_->getGeometry(EBDetId(ieta, iphi + (dphi >= 0 ? 1 : -1)));
    pPhi = static_cast<const TruncatedPyramid*>(cell_phi.get())->getPosition(maxDepth);
  }

  // Combine: depth-corrected center + lateral sub-crystal offset
  float x = p0.x() + std::abs(deta) * (pEta.x() - p0.x()) + std::abs(dphi) * (pPhi.x() - p0.x());
  float y = p0.y() + std::abs(deta) * (pEta.y() - p0.y()) + std::abs(dphi) * (pPhi.y() - p0.y());
  float z = p0.z() + std::abs(deta) * (pEta.z() - p0.z()) + std::abs(dphi) * (pPhi.z() - p0.z());

  return math::XYZPoint(x, y, z);
}

// ----------------------------------------------------------------------------
//                          start of helper functions
// ----------------------------------------------------------------------------
void BarrelClusTEXAlgo::killDeadChannels() {
  for (int r = 0; r < nRows; ++r) {
    for (int c = 0; c < nCols; ++c) {
      if (deadChannelGrid_[r * nCols + c] == 3) {
        cellsMap_[r * nCols + c] = 0.0f;
      }
    }
  }
}

std::vector<std::vector<float>> BarrelClusTEXAlgo::get_abs_pos(const int numClusters, const std::vector<std::vector<Eigen::MatrixXf>>& indices) {

  std::vector<std::vector<float>> abs_pos(numClusters, std::vector<float>(maxClusters_, 0.0f)); // 1D flattened absolute positions
  for (int n = 0; n < numClusters; ++n) {
      for (int k = 0; k < maxClusters_; ++k) {
          float center_ieta = static_cast<float>(indices[n][k](0,0));
          float center_iphi = static_cast<float>(indices[n][k](1,0));
          float val = center_iphi + static_cast<float>(nCols) * center_ieta;
          if (val == -static_cast<float>(nCols+1))  // corresponds to (-1, -1)
              val = 0.0f;
          abs_pos[n][k] = val;
      }
  }
  return abs_pos;
}

std::vector<std::vector<std::array<float, 2>>> BarrelClusTEXAlgo::get_rel_pos(const int numClusters, const std::vector<std::vector<Eigen::MatrixXf>>& indices) {

  int r_eff = (cropSize_ + overlapLimit_) - 1;

  // Position relative to the center of the effective window
  std::vector<std::vector<std::array<float, 2>>> rel_pos(
      numClusters, std::vector<std::array<float, 2>>(maxClusters_));
  
  // Initialize with -1
  for (int n = 0; n < numClusters; ++n) {
      for (int i = 0; i < maxClusters_; ++i) {
          rel_pos[n][i][0] = -1.0f;
          rel_pos[n][i][1] = -1.0f;
      }
  }
  for (int n = 0; n < numClusters; ++n) {
      int center_ieta = static_cast<int>(indices[n][0](0,0)); // Highest Edep
      int center_iphi = static_cast<int>(indices[n][0](1,0)); // Highest Edep
      for (int k = 0; k < maxClusters_; ++k) {
          int ieta = static_cast<int>(indices[n][k](0,0));
          int iphi = static_cast<int>(indices[n][k](1,0));
          if (ieta > -1) {rel_pos[n][k][0] = float(ieta - center_ieta) / float(r_eff);}
          if (iphi > -1) {rel_pos[n][k][1] = float(iphi - center_iphi) / float(r_eff);}
      }
  }
  return rel_pos;
}

std::vector<std::vector<Eigen::MatrixXi>> BarrelClusTEXAlgo::get_channel_status_mask(
  const std::vector<std::vector<Eigen::MatrixXf>>& indices) {

  int half = cropSize_ / 2;
  std::vector<std::vector<Eigen::MatrixXi>> dead_masks; // (N, maxClusters, cropSize, cropSize)

  for (const auto& event : indices) { // loop over N clusters
      std::vector<Eigen::MatrixXi> event_masks;

      for (const auto& idx : event) { // loop over maxClusters per cluster
          int center_iphi = static_cast<int>(idx(0,0));
          int center_ieta = static_cast<int>(idx(1,0));

          // padding case
          if (center_ieta == -1 && center_iphi == -1) {
              event_masks.push_back(Eigen::MatrixXi::Constant(cropSize_, cropSize_, -1));
              continue;
          }

          Eigen::MatrixXi window(cropSize_, cropSize_);

          for (int dr = -half; dr <= half; ++dr) {
              int row = (center_iphi + dr - 1 + (nRows - 1)) % (nRows - 1) + 1; // wrap around iphi
              for (int dc = -half; dc <= half; ++dc) {
                  int col = center_ieta + dc;
                  if (col < 0 || col >= nCols)
                      window(dr + half, dc + half) = -1; // out of bounds in ieta
                  else
                      window(dr + half, dc + half) = deadChannelGrid_[row * nCols + col];
              }
          }
          event_masks.push_back(std::move(window));
      }
      dead_masks.push_back(std::move(event_masks));
  }
  return dead_masks;
}
// ----------------------------------------------------------------------------
//                            end of helper functions
// ----------------------------------------------------------------------------
