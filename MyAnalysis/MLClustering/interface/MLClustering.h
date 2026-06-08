#ifndef MLClustering_H
#define MLClustering_H

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
#include "SimDataFormats/Track/interface/SimTrackContainer.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticleFwd.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingVertexContainer.h"
#include "SimDataFormats/Vertex/interface/SimVertexContainer.h"
#include "SimDataFormats/CaloAnalysis/interface/CaloParticle.h"
#include "SimDataFormats/CaloAnalysis/interface/CaloParticleFwd.h"
#include "SimDataFormats/CaloAnalysis/interface/SimCluster.h"
#include "SimDataFormats/CaloAnalysis/interface/SimClusterFwd.h"

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/EcalAlgo/interface/EcalBarrelGeometry.h"
#include "Geometry/Records/interface/EcalBarrelGeometryRecord.h"
#include "CondFormats/EcalObjects/interface/EcalChannelStatus.h"
#include "CondFormats/DataRecord/interface/EcalChannelStatusRcd.h"

#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "PhysicsTools/ONNXRuntime/interface/ONNXRuntime.h"

#include "TrackingTools/TrajectoryParametrization/interface/GlobalTrajectoryParameters.h"
#include "TrackPropagation/SteppingHelixPropagator/interface/SteppingHelixPropagator.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "MagneticField/Engine/interface/MagneticField.h"
#include "CommonTools/BaseParticlePropagator/interface/BaseParticlePropagator.h"

#include <map>
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <string>

using namespace cms::Ort;

class MLClustering : public DQMOneEDAnalyzer<> {
  typedef std::map<std::pair<int, int>, float> MapType;
  typedef std::map<std::pair<int, int>, std::vector<std::pair<int, float>>> CaloMapType;

public:
  typedef dqm::legacy::DQMStore DQMStore;

  explicit MLClustering(const edm::ParameterSet &);

  ~MLClustering() override;

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
  FloatArrays data_;
  std::unique_ptr<ONNXRuntime> onnx_;

  std::string g4InfoLabel;
  std::string EBHitsCollection;
  std::string jobId;
  int maskedEcalChannelStatusThreshold;

  int cropSize;
  int maxClusters;
  int overlapLimit;
  double seedThreshold;

  const EcalBarrelGeometry* barrelGeom_ = nullptr;
  // Store EB: DetId <==> vector<int> (subdet, ieta, iphi, status)
  std::map<DetId, std::vector<int>> EcalAllDeadChannelsBitMap_;

  edm::EDGetTokenT<edm::PCaloHitContainer> EBHitsToken;
  edm::EDGetTokenT<reco::GenParticleCollection> genParticleToken;
  edm::EDGetTokenT<reco::PFClusterCollection> pfClusterToken;
  edm::EDGetTokenT<edm::SimTrackContainer> SimTrackToken;
  edm::EDGetTokenT<edm::SimVertexContainer> SimVertexToken;
  edm::EDGetTokenT<EBRecHitCollection> EBrechitCollection_Token;
  edm::EDGetTokenT<CaloParticleCollection> CaloParticle_Token;
  //edm::ESGetToken<CaloSubdetectorGeometry, EcalBarrelGeometryRecord> barrelGeomToken;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> ecalGeomToken;
  edm::ESGetToken<EcalChannelStatus, EcalChannelStatusRcd> ecalStatusToken;
  edm::ESGetToken<MagneticField, IdealMagneticFieldRecord> magFieldToken;

  TTree* simTree;
  TTree* recoTree;
  TTree* pfTree;
  TTree* genTree;
  TTree* mlTree;
  TTree* fineTree;
  TTree* fineMapTree;

  std::vector<int>      simPDG;
  std::vector<float>    simT;
  std::vector<float>    simE;
  std::vector<int>      simPhi;
  std::vector<int>      simEta;
  std::vector<int>      simEvent;
  std::vector<int>      simSubEvent;
  std::vector<uint64_t> simTrackId;
  std::vector<int>      simIEta;
  std::vector<int>      simIPhi;
  std::vector<float>    simMapFraction;
  std::vector<uint64_t> simMapTrackId;
  std::vector<float>    simValues;
  std::vector<int> simMapAncestor;

  std::vector<int>      recoEvent;
  std::vector<int>      recoIEta;
  std::vector<int>      recoIPhi;
  std::vector<float>    recoValues;

  std::vector<float>    genE;
  std::vector<float>    genPPt;
  std::vector<float>    genPPhi;
  std::vector<float>    genPEta;
  std::vector<float>    genIEta;
  std::vector<float>    genIPhi;
  std::vector<float>    genEtaF;
  std::vector<float>    genPhiF;
  std::vector<uint64_t> genTrackId;
  std::vector<int>      genEvent;
  std::vector<int>      genIsConverted;
  std::vector<float>    genConvR;
  std::vector<float>    genConvZ;

  std::vector<int>    pfEvent;
  std::vector<float>  pfPhi;
  std::vector<float>  pfEta;
  std::vector<double> pfE;

  std::vector<int>   mlEvent;
  std::vector<int>   mlN;        // which sample (0..numClusters-1)
  std::vector<int>   mlK;        // which cluster slot (0..maxClusters-1)
  std::vector<float> mlCenterX;
  std::vector<float> mlCenterY;
  std::vector<float> mlEnergy;
  std::vector<float> mlSeed;

  std::vector<int>   fineEvent, fineTrackId, finePDG, fineNHits;
  std::vector<float> fineE;
  std::vector<float> fineInitialE, fineInitialEta, fineInitialPhi;
  std::vector<int>   fineCrossedBoundary;
  std::vector<float> fineEntIEta, fineEntIPhi, fineEntE;     // at ECAL entrance
  std::vector<float> fineEntX, fineEntY, fineEntZ;           // global cm
  std::vector<int>   fineParentId, fineAncestorId, fineGenIdx;
  
  std::vector<int>   fineMapEvent;
  std::vector<int>   fineMapIEta, fineMapIPhi;
  std::vector<uint64_t> fineMapTrackId;
  std::vector<float> fineMapEnergy;
  std::vector<float> fineMapFraction;
  std::vector<int> fineMapAncestor;

  std::map<unsigned, unsigned> geantToIndex_;
};

#endif
