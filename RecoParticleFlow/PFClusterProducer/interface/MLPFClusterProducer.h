#ifndef MLPFClusterProducer_H
#define MLPFClusterProducer_H

#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Utilities/interface/ESGetToken.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "DataFormats/ParticleFlowReco/interface/PFCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFClusterFwd.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHit.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "CondFormats/EcalObjects/interface/EcalChannelStatus.h"
#include "CondFormats/DataRecord/interface/EcalChannelStatusRcd.h"

#include "PhysicsTools/ONNXRuntime/interface/ONNXRuntime.h"

#include <vector>
#include <string>

class MLPFClusterProducer : public edm::stream::EDProducer<> {

public:
  explicit MLPFClusterProducer(const edm::ParameterSet &);
  ~MLPFClusterProducer() override;

protected: 
  // void beginJob() override;
  // void endJob() override;
  void produce(edm::Event&, const edm::EventSetup&) override;

private:
  std::vector<std::string> input_names_;
  std::vector<std::vector<int64_t>> input_shapes_;
  cms::Ort::FloatArrays data_;
  std::unique_ptr<cms::Ort::ONNXRuntime> onnx_;
  
  std::string jobId;
  int maskedEcalChannelStatusThreshold;
  int cropSize;
  int maxClusters;
  int overlapLimit;
  double seedThreshold;
  double outputThreshold;

  // Store EB: DetId <==> vector<int> (subdet, ieta, iphi, status)
  std::map<DetId, std::vector<int>> EcalAllDeadChannelsBitMap_;

  edm::EDGetTokenT<EBRecHitCollection> EBrechitCollection_Token_;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeomToken_;
  edm::ESGetToken<EcalChannelStatus, EcalChannelStatusRcd> ecalStatusToken_;
  edm::EDGetTokenT<reco::PFClusterCollection> eeClusterToken_;
  edm::EDGetTokenT<reco::PFCluster::EEtoPSAssociation> eeToPSToken_;
};

#endif
