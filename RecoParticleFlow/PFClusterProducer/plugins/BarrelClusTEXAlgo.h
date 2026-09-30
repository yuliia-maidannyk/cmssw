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

#include "FWCore/Framework/interface/ESWatcher.h"
#include "CondFormats/DataRecord/interface/EcalChannelStatusRcd.h"
#include "CondFormats/EcalObjects/interface/EcalChannelStatus.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "DataFormats/EcalDetId/interface/EBDetId.h"
#include "DataFormats/EcalDetId/interface/EcalSubdetector.h"

// C/C++ headers
#include "PhysicsTools/ONNXRuntime/interface/ONNXRuntime.h"
#include "RecoParticleFlow/PFClusterProducer/interface/BarrelClusTEXPreprocessing.h"
#include <array>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

class BarrelClusTEXAlgo : public HGCalClusteringAlgoBase {
public:
  BarrelClusTEXAlgo(const edm::ParameterSet &);
  ~BarrelClusTEXAlgo() override {}

  void getEventSetupPerAlgorithm(const edm::EventSetup& iSetup) override;
  void updateDeadChannelGrid(const edm::EventSetup& iSetup);
  void setThresholds(edm::ESGetToken<EcalPFRecHitThresholds, EcalPFRecHitThresholdsRcd>,
                     edm::ESGetToken<HcalPFCuts, HcalPFCutsRcd>) override {};

  void setClusTEXTokens(edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeomToken,
                        edm::ESGetToken<EcalChannelStatus, EcalChannelStatusRcd> ecalStatusToken) override;

  void populate(const HGCRecHitCollection& hits) override {};
  void populate(const reco::PFRecHitCollection& hits) override;

  void makeClusters() override;

  std::vector<reco::BasicCluster> getClusters(bool) override;

  void reset() override {
    clusters_v_.clear();
    clusters_v_.shrink_to_fit();
    predictedClusters_.clear();
    cellsMap_.clear();
    data_.clear();
    inputShapes_.clear();
    // deadChannelGrid_ is deliberately NOT cleared: the ESWatcher only
    // refills it when the channel status changes.
  }

  static void fillPSetDescription(edm::ParameterSetDescription& iDesc) {
    iDesc.add<std::string>("modelPath", "RecoParticleFlow/PFClusterProducer/data/clus_tex.onnx");
    iDesc.add<std::vector<std::string>>("inputNames", {"inp1", "inp2", "inp3", "inp4"});
    iDesc.add<int>("cropSize", 7);
    iDesc.add<int>("maxClusters", 20);
    iDesc.add<int>("overlapLimit", 7);
    iDesc.add<double>("seedThreshold", 0.23);
    iDesc.add<double>("outputThreshold", 0.3);
    iDesc.add<int>("maskedEcalChannelStatusThreshold", 1);
    // iDesc.add<double>("T0_EB", 7.4);
    // iDesc.add<double>("X0", 0.89);
  }

private:

  // Helper functions
  void killDeadChannels();
  std::vector<std::vector<float>> get_abs_pos(const int, const std::vector<std::vector<Eigen::MatrixXf>>&);
  std::vector<std::vector<std::array<float, 2>>> get_rel_pos(const int, const std::vector<std::vector<Eigen::MatrixXf>>&);
  std::vector<std::vector<Eigen::MatrixXi>> get_channel_status_mask(const std::vector<std::vector<Eigen::MatrixXf>>&);
  // End of helper functions

  std::string modelPath_;
  std::vector<std::string> inputNames_;
  std::vector<std::vector<int64_t>> inputShapes_;
  cms::Ort::FloatArrays data_;
  std::unique_ptr<cms::Ort::ONNXRuntime> onnx_;
  
  int cropSize_;
  int maxClusters_;
  int overlapLimit_;
  double seedThreshold_;
  double outputThreshold_;
  int maskedEcalChannelStatusThreshold_;
  // double param_T0_EB_;
  // double param_X0_;

  // Flat [iphi * 171 + (ieta + 85)] grid. It is invariant within an
  // EcalChannelStatusRcd IOV, so do not reconstruct it for every event.
  std::vector<int> deadChannelGrid_;
  edm::ESWatcher<EcalChannelStatusRcd> ecalChannelStatusWatcher_;
  const CaloSubdetectorGeometry* ebGeom_ = nullptr;

  // edm::EDGetTokenT<EBRecHitCollection> EBrechitCollection_Token_;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeomToken_;
  edm::ESGetToken<EcalChannelStatus, EcalChannelStatusRcd> ecalStatusToken_;

  std::vector<float> cellsMap_;
  static constexpr int nRows = 361;          // iphi, 1..360
  static constexpr int nCols = 171;          // ieta + 85, 0..170
  static constexpr int etaOffset = 85;
  static constexpr float energyScale = 100.f;  // the network predicts energy / 100 GeV

  struct PredictedCluster {
      float energy, time;
      float cx, cy;
  };
  std::vector<PredictedCluster> predictedClusters_;
  
  math::XYZPoint calculatePosition(const float& cx, const float& cy) const;
};

#endif
