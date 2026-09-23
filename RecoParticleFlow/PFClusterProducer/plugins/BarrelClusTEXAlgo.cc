#include "RecoParticleFlow/PFClusterProducer/plugins/BarrelClusTEXAlgo.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
//#include "DataFormats/CaloRecHit/interface/CaloID.h"

//#include "oneapi/tbb/task_arena.h"
//#include "oneapi/tbb.h"
//#include <limits>

using namespace hgcal_clustering;

void BarrelClusTEXAlgo::setThresholds(edm::ESGetToken<EcalPFRecHitThresholds, EcalPFRecHitThresholdsRcd> EBThres,
                                       edm::ESGetToken<HcalPFCuts, HcalPFCutsRcd> HCALThres) {
  tok_ebThresholds_ = EBThres;
}

void BarrelClusTEXAlgo::getEventSetupPerAlgorithm(const edm::EventSetup& es) {
  ebThresholds_ = &es.getData(tok_ebThresholds_);
  cells_.clear();
  cells_.resize(1); //ECAL has one layer
}

void BarrelClusTEXAlgo::populate(const reco::PFRecHitCollection& hits) {
  for (unsigned int i = 0; i < hits.size(); ++i) {
    const reco::PFRecHit& hit = hits[i];
    DetId detid(hit.detId());
    const GlobalPoint position = rhtools_.getPosition(detid);
    int layer = 0;

    cells_[layer].detid.push_back(detid);
    cells_[layer].eta.push_back(position.eta());
    cells_[layer].phi.push_back(position.phi());
    cells_[layer].weight.push_back(hit.energy());
    cells_[layer].r.push_back(position.mag());
  }
}

void BarrelClusTEXAlgo::prepareDataStructures(unsigned int l) {
  auto cellsSize = cells_[l].detid.size();
  cells_[l].clusterIndex.resize(cellsSize);
  cells_[l].eta.resize(cellsSize, 0.f);
  cells_[l].phi.resize(cellsSize, 0.f);
  cells_[l].r.resize(cellsSize, 0.f);
}

void BarrelClusTEXAlgo::makeClusters() {
  // run the clusterig here
  std::cout << "Running the clustering\n";
  // temporary, for testing
  auto& cellsOnLayer = cells_[0];
  for (unsigned int i = 0; i < cellsOnLayer.detid.size(); ++i) {
    cellsOnLayer.clusterIndex[i] = {0};
  }
}

std::vector<reco::BasicCluster> BarrelClusTEXAlgo::getClusters(bool) { // to be checked

// Store the cellId and energy for each cluster
  // We need to store the energy as a pair here because we perform the energy splitting
  std::vector<std::vector<std::pair<int, float>>> cellsIdInCluster;
  cellsIdInCluster.reserve(maxClusters);

   const unsigned int layerId = 0;
    auto& cellsOnLayer = cells_[layerId];
    unsigned int numberOfCells = cellsOnLayer.detid.size();

    for (unsigned int i = 0; i < numberOfCells; ++i) {
      auto clusterIndex = cellsOnLayer.clusterIndex[i];

      if (clusterIndex.size() == 1) {
        cellsIdInCluster[clusterIndex[0]].push_back(std::make_pair(i, 1.f));
      } else if (clusterIndex.size() > 1) {
//        std::vector<float> fractions(clusterIndex.size());
//
//        for (unsigned int j = 0; j < clusterIndex.size(); j++) {
//          const auto& seed = clusterIndex[j];
//          const auto& seedCell = cellsOnLayer.seedToCellIndex[seed];
//          const auto& seedEnergy = cellsOnLayer.weight[seedCell];
//          // compute the distance
//          float dist = distance(i, seedCell, layerId) / T::type::cellWidthEta;
//          fractions[j] = seedEnergy * std::exp(-(std::pow(dist, 2)) / (2 * std::pow(T::type::showerSigma, 2)));
//        }
//        auto tot_norm_fractions = std::accumulate(std::begin(fractions), std::end(fractions), 0.);
//
//        for (unsigned int j = 0; j < clusterIndex.size(); j++) {
//          float norm_fraction = fractions[j] / tot_norm_fractions;
//          if (norm_fraction < fractionCutoff_) {
//            auto clusterIndex_it = std::find(clusterIndex.begin(), clusterIndex.end(), clusterIndex[j]);
//            auto fraction_it = std::find(fractions.begin(), fractions.end(), fractions[j]);
//            clusterIndex.erase(clusterIndex_it);
//            fractions.erase(fraction_it);
//          }
//          if (norm_fraction >= fractionCutoff_) {
//            cellsIdInCluster[clusterIndex[j]].push_back(std::make_pair(i, norm_fraction));
//          }
//        }
//        auto tot_norm_cleaned_fractions = std::accumulate(fractions.begin(), fractions.end(), 0.);
//        for (unsigned int j = 0; j < clusterIndex.size(); j++) {
//          float norm_fraction = fractions[j] / tot_norm_cleaned_fractions;
//          cellsIdInCluster[clusterIndex[j]].push_back(std::make_pair(i, norm_fraction));
//        }
      }
  }

    clusters_v_.resize(cellsIdInCluster.size());
    std::vector<std::pair<DetId, float>> thisCluster;
    for (int clIndex = 0; clIndex < (int)cellsIdInCluster.size(); clIndex++) {
      auto& cl = cellsIdInCluster[clIndex];
      auto position = calculatePosition(cl, layerId);
      float energy = 0.f;
      int seedDetId = -1;

      for (auto [cellIdx, fraction] : cl) {
        energy += cellsOnLayer.weight[cellIdx] * fraction;
        thisCluster.emplace_back(cellsOnLayer.detid[cellIdx], fraction);
       // if (cellsOnLayer.isSeed[cellIdx]) { //FIXME does clustex has seed?
       //   seedDetId = cellsOnLayer.detid[cellIdx];
       // }
      }
      clusters_v_[clIndex] =
          reco::BasicCluster(energy, position, reco::CaloID::DET_ECAL_BARREL, thisCluster, algoId_);
      clusters_v_[clIndex].setSeed(seedDetId);
      thisCluster.clear();
    }
  return clusters_v_;
}

math::XYZPoint BarrelClusTEXAlgo::calculatePosition(const std::vector<std::pair<int, float>>& v,
                                                     const unsigned int layerId) const {
  float total_weight = 0.f;
  float x = 0.f;
  float y = 0.f;
  float z = 0.f;

  auto& cellsOnLayer = cells_[layerId];
  for (const auto& [i, fraction] : v) {
    float rhEnergy = cellsOnLayer.weight[i] * fraction;
    total_weight += rhEnergy;
    float theta = 2 * std::atan(std::exp(-cellsOnLayer.eta[i]));
    const GlobalPoint gp(GlobalPoint::Polar(theta, cellsOnLayer.phi[i], cellsOnLayer.r[i]));
    x += gp.x() * rhEnergy;
    y += gp.y() * rhEnergy;
    z += gp.z() * rhEnergy;
  }
  if (total_weight != 0.f) {
    float inv_total_weight = 1.f / total_weight;
    return math::XYZPoint(x * inv_total_weight, y * inv_total_weight, z * inv_total_weight);
  } else {
    return math::XYZPoint(0.f, 0.f, 0.f);
  }
}

