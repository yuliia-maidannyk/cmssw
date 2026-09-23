#ifndef RecoParticleFlow_PFClusterProducer_BarrelClusTEXAlgo_h
#define RecoParticleFlow_PFClusterProducer_BarrelClusTEXAlgo_h

#include "RecoLocalCalo/HGCalRecProducers/interface/HGCalClusteringAlgoBase.h"

#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"

#include "DataFormats/DetId/interface/DetId.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHit.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFwd.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"

#include "DataFormats/EgammaReco/interface/BasicCluster.h"
#include "DataFormats/Math/interface/Point3D.h"
#include "DataFormats/Math/interface/deltaPhi.h"

#include "RecoLocalCalo/HGCalRecAlgos/interface/RecHitTools.h"

#include "CondFormats/DataRecord/interface/EcalPFRecHitThresholdsRcd.h"
#include "CondFormats/EcalObjects/interface/EcalPFRecHitThresholds.h"
#include "CondFormats/DataRecord/interface/HcalPFCutsRcd.h"
#include "CondFormats/HcalObjects/interface/HcalPFCuts.h"

// C/C++ headers
#include <set>
#include <string>
#include <vector>

class BarrelClusTEXAlgo : public HGCalClusteringAlgoBase {
public:
  BarrelClusTEXAlgo(const edm::ParameterSet& ps)
      : HGCalClusteringAlgoBase(
            (HGCalClusteringAlgoBase::VerbosityLevel)ps.getUntrackedParameter<unsigned int>("verbosity", 3),
            reco::CaloCluster::undefined),
            doSharing_(ps.getParameter<bool>("doSharing")) {}
  ~BarrelClusTEXAlgo() override {}

  void getEventSetupPerAlgorithm(const edm::EventSetup& es) override;
  void setThresholds(edm::ESGetToken<EcalPFRecHitThresholds, EcalPFRecHitThresholdsRcd>,
                     edm::ESGetToken<HcalPFCuts, HcalPFCutsRcd>) override;

  void populate(const HGCRecHitCollection& hits) override {};
  void populate(const reco::PFRecHitCollection& hits) override;

  void makeClusters() override;

  std::vector<reco::BasicCluster> getClusters(bool) override;

  void reset() override {
    clusters_v_.clear();
    clusters_v_.shrink_to_fit();
    for (auto& cl : numberOfClustersPerLayer_) {
      cl = 0;
    }

    for (auto& cells : cells_) {
      cells.clear();
      cells.shrink_to_fit();
    }
  }

  void computeThreshold();

  static void fillPSetDescription(edm::ParameterSetDescription& iDesc) {
    iDesc.add<bool>("doSharing", true);
  }

  /// point in the space
  typedef math::XYZPoint Point;

private:
  // To get ECAL sigmaNoise
  edm::ESGetToken<EcalPFRecHitThresholds, EcalPFRecHitThresholdsRcd> tok_ebThresholds_;
  const EcalPFRecHitThresholds* ebThresholds_;
  bool doSharing_;
  const int maxClusters=100;

  struct BarrelCellsOnLayer {
    std::vector<DetId> detid;
    std::vector<float> eta;
    std::vector<float> phi;
    std::vector<float> r;

    std::vector<float> weight;
    std::vector<std::vector<int>> clusterIndex;

    void clear() {
      detid.clear();
      eta.clear();
      phi.clear();
      r.clear();
      weight.clear();
      clusterIndex.clear();
    }

    void shrink_to_fit() {
      detid.shrink_to_fit();
      r.shrink_to_fit();
      eta.shrink_to_fit();
      phi.shrink_to_fit();
      weight.shrink_to_fit();
      clusterIndex.shrink_to_fit();
    }
  };

  std::vector<BarrelCellsOnLayer> cells_; // NEEDED? maybe not a vector (one layer) OR a vector of BarrelCellsOnLayer (but removing vectors within BarrelCellsOnLayer, not SoA friendly)

  std::vector<int> numberOfClustersPerLayer_;
  math::XYZPoint calculatePosition(const std::vector<std::pair<int, float>>& v, const unsigned int layerId) const;

  void prepareDataStructures(const unsigned int layerId);
  inline float distance2(int cell1, int cell2, int layerId) const {  // distance squared
    const float dphi = reco::deltaPhi(cells_[layerId].phi[cell1], cells_[layerId].phi[cell2]);
    const float deta = cells_[layerId].eta[cell1] - cells_[layerId].eta[cell2];
    return (deta * deta + dphi * dphi);
  }

  inline float distance(int cell1, int cell2, int layerId) const {  // 2-d distance on the layer (x-y)
    return std::sqrt(distance2(cell1, cell2, layerId));
  }

};

#endif
