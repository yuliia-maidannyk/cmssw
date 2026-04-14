#ifndef TransClustering_H
#define TransClustering_H

#include "FWCore/Framework/interface/ConsumesCollector.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Utilities/interface/ESGetToken.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DQMServices/Core/interface/DQMOneEDAnalyzer.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "DataFormats/HepMCCandidate/interface/GenParticle.h"
#include "DataFormats/HepMCCandidate/interface/GenParticleFwd.h"
#include "DataFormats/EcalRecHit/interface/EcalUncalibratedRecHit.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHit.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"
#include "DataFormats/ParticleFlowReco/interface/PFCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFClusterFwd.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyCalibration.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyResolution.h"
#include "CommonTools/ParticleFlow/interface/PFClusterWidthAlgo.h"
#include "RecoParticleFlow/PFTracking/interface/PFTrackAlgoTools.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFraction.h" 

#include "SimDataFormats/CaloHit/interface/PCaloHit.h"
#include "SimDataFormats/CaloHit/interface/PCaloHitContainer.h"
#include "SimDataFormats/ValidationFormats/interface/PValidationFormats.h"
#include "SimDataFormats/Track/interface/SimTrackContainer.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticleFwd.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingVertexContainer.h"
#include "SimDataFormats/Vertex/interface/SimVertexContainer.h"
#include "SimDataFormats/CaloAnalysis/interface/CaloParticle.h"
#include "SimDataFormats/CaloAnalysis/interface/CaloParticleFwd.h"
#include "SimDataFormats/CaloAnalysis/interface/SimCluster.h"
#include "SimDataFormats/CaloAnalysis/interface/SimClusterFwd.h"
#include "SimDataFormats/GeneratorProducts/interface/HepMCProduct.h"
// #include "SimDataFormats/CrossingFrame/interface/CrossingFrame.h"
// #include "SimDataFormats/CrossingFrame/interface/MixCollection.h"
// #include "SimDataFormats/TrackingHit/interface/PSimHit.h"

// #include "Geometry/Records/interface/MTDDigiGeometryRecord.h"
// #include "Geometry/Records/interface/MTDTopologyRcd.h"
// #include "Geometry/MTDGeometryBuilder/interface/MTDGeometry.h"
// #include "Geometry/MTDGeometryBuilder/interface/MTDTopology.h"
// #include "Geometry/MTDGeometryBuilder/interface/ProxyMTDTopology.h"
// #include "Geometry/MTDGeometryBuilder/interface/RectangularMTDTopology.h"
// #include "Geometry/MTDCommonData/interface/MTDTopologyMode.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/Records/interface/EcalBarrelGeometryRecord.h"
#include "CondFormats/EcalObjects/interface/EcalChannelStatus.h"
#include "CondFormats/DataRecord/interface/EcalChannelStatusRcd.h"

// #include "PhysicsTools/TensorFlow/interface/TensorFlow.h"
#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "PhysicsTools/ONNXRuntime/interface/ONNXRuntime.h"

#include <map>
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <string>

// struct MTDHit {
//   float energy = 0.f;
//   float time = 0.f;
//   float x = 0.f;
//   float y = 0.f;
//   float z = 0.f;
//   int process = 0;
//   int type = 0;
//   int pdgId = 0;
//   int trackId = 0;
// };

using namespace cms::Ort;

class TransClustering : public DQMOneEDAnalyzer<> {
  typedef std::map<std::pair<int, int>, float> MapType;
  typedef std::map<std::pair<int, int>, std::vector<std::pair<int, float>>> CaloMapType;

public:
  typedef dqm::legacy::DQMStore DQMStore;

  explicit TransClustering(const edm::ParameterSet &);

  ~TransClustering() override;

protected: 
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;
  void fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices);
  // void beginJob() override;
  // void endJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void clearEventData();

private:
  std::vector<std::string> input_names_;
  std::vector<std::vector<int64_t>> input_shapes_;
  FloatArrays data_; // each stream hosts its own data
  std::unique_ptr<ONNXRuntime> onnx_;

  std::string g4InfoLabel;
  std::string EBHitsCollection;
  std::string ValidationCollection;
  std::string jobId;
  int maskedEcalChannelStatusThreshold;

  // tensorflow::GraphDef* graphDef;
  // tensorflow::Session* session;
  // std::string graphPath;
  // std::string inputTensorName;
  // std::string outputTensorName;

  int cropSize;
  int maxClusters;
  int overlapLimit;
  double seedThreshold;

  edm::EDGetTokenT<edm::PCaloHitContainer> EBHitsToken;
  edm::EDGetTokenT<PEcalValidInfo> ValidationCollectionToken;
  edm::EDGetTokenT<reco::GenParticleCollection> genParticleToken;
  edm::EDGetTokenT<reco::PFClusterCollection> pfClusterToken;
  edm::EDGetTokenT<edm::SimTrackContainer> SimTrackToken;
  edm::EDGetTokenT<edm::SimVertexContainer> SimVertexToken;
  edm::EDGetTokenT<EcalUncalibratedRecHitCollection> EBuncalibrechitCollection_Token;
  edm::EDGetTokenT<EBRecHitCollection> EBrechitCollection_Token;
  edm::EDGetTokenT<CaloParticleCollection> CaloParticle_Token;
  edm::EDGetTokenT<edm::HepMCProduct> HepMCToken;
  edm::ESGetToken<CaloSubdetectorGeometry, EcalBarrelGeometryRecord> barrelGeomToken;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> ecalGeomToken;
  edm::ESGetToken<EcalChannelStatus, EcalChannelStatusRcd> ecalStatusToken;
  // edm::EDGetTokenT<CrossingFrame<PSimHit>> btlSimHitsToken;
  // edm::ESGetToken<MTDGeometry, MTDDigiGeometryRecord> mtdgeoToken;
  // edm::ESGetToken<MTDTopology, MTDTopologyRcd> mtdtopoToken;

  TTree* simTree;
  TTree* recoTree;
  TTree* caloTree;
  TTree* pfTree;
  TTree* genTree;
  TTree* mlTree;
  // TTree* btlTree;

  std::vector<int>      simPDG;
  std::vector<float>    simT;
  std::vector<float>    simE;
  std::vector<int>      simPhi;
  std::vector<int>      simEta;
  std::vector<float>    simZ;
  std::vector<int>      simEvent;
  std::vector<int>      simSubEvent;
  std::vector<uint64_t> simTrackId;
  std::vector<int>      simIEta;
  std::vector<int>      simIPhi;
  std::vector<float>    simValues;

  std::vector<float>    recoT;
  std::vector<float>    recoE;
  std::vector<float>    recoPhi;
  std::vector<float>    recoEta;
  std::vector<int>      recoEvent;
  std::vector<uint32_t> recoID;
  std::vector<int>      recoIEta;
  std::vector<int>      recoIPhi;
  std::vector<float>    recoValues;

  std::vector<float>    caloT;
  std::vector<float>    caloE;
  std::vector<float>    caloPPt;
  std::vector<float>    caloPPhi;
  std::vector<float>    caloPEta;
  std::vector<float>    caloEta;
  std::vector<float>    caloPhi;
  std::vector<float>    caloPDG;
  std::vector<int>      caloEvent;
  std::vector<int>      caloSubEvent;
  std::vector<int>      caloIEta;
  std::vector<int>      caloIPhi;
  std::vector<int>      caloValues;
  std::vector<float>    caloValuesE;
  std::vector<uint64_t> caloTrackId;

  std::vector<float> genT;
  std::vector<float> genE;
  std::vector<float> genPPt;
  std::vector<float> genPPhi;
  std::vector<float> genPEta;
  std::vector<float> genEta;
  std::vector<float> genPhi;
  std::vector<int>   genPDG;
  std::vector<int>   genEvent;
  std::vector<float> genSourceX;
  std::vector<float> genSourceY;
  std::vector<float> genSourceZ;
  std::vector<int>   genIsConverted;
  std::vector<float> genConvR;
  std::vector<float> genConvZ;

  std::vector<int>    pfEvent;
  std::vector<int>    pfPhi;
  std::vector<int>    pfEta;
  std::vector<double> pfE;

  // std::vector<float> btlPDG;
  // std::vector<float> btlT;
  // std::vector<float> btlE;
  // std::vector<float> btlLocX;
  // std::vector<float> btlLocY;
  // std::vector<float> btlLocZ;
  // std::vector<float> btlX;
  // std::vector<float> btlY;
  // std::vector<float> btlZ;
  // std::vector<int>   btlEvent;
  // std::vector<uint32_t> btlType;

  std::vector<int>   mlEvent;
  std::vector<int>   mlN;        // which sample (0..numClusters-1)
  std::vector<int>   mlK;        // which cluster slot (0..maxClusters-1)
  std::vector<float> mlCenterX;
  std::vector<float> mlCenterY;
  std::vector<float> mlEnergy;
  std::vector<float> mlSeed;

  std::map<unsigned, unsigned> geantToIndex_;
};

#endif
