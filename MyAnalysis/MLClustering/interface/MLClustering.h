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
  TTree* caloTree;
  TTree* pfTree;
  TTree* genTree;
  TTree* mlTree;
  TTree* decayTTree;
  TTree* simTkTree;

  std::vector<int>      simTkEvent;
  std::vector<unsigned> simTkTrackId;
  std::vector<int>      simTkPDG;
  std::vector<float>    simTkE;
  std::vector<float>    simTkEta;      // momentum eta
  std::vector<float>    simTkPhi;      // momentum phi
  std::vector<float>    simTkIEta;     // propagated ECAL ieta (-999 if failed/endcap)
  std::vector<float>    simTkIPhi;     // propagated ECAL iphi (-999 if failed/endcap)
  std::vector<float>    simTkConvR;    // production vertex R
  std::vector<int>      simTkParentId; // parent trackId (-1 if primary)
  std::vector<unsigned> simTkAncestorId; // gen-level ancestor trackId
  std::vector<int>      simTkGenIdx;   // genpartIndex() — index into GenParticle collection
  std::vector<float>    simTkEBEnergy; // EB deposited energy (0 if no hits)

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
  std::vector<float>    simValues;

  std::vector<int>      recoEvent;
  std::vector<int>      recoIEta;
  std::vector<int>      recoIPhi;
  std::vector<float>    recoValues;

  std::vector<float>    caloR;
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
  std::vector<uint64_t> caloParentTrackId;
  std::vector<uint64_t> caloAncestorTrackId;
  std::vector<float>    caloEBEnergy;
  std::vector<float>    caloCentroidIEta;
  std::vector<float>    caloCentroidIPhi;
  std::vector<uint64_t> caloScaleId; // trackId for which we store the scale factor
  std::vector<float>    caloScaleFactor; 

  std::vector<float>    genE;
  std::vector<float>    genPPt;
  std::vector<float>    genPPhi;
  std::vector<float>    genPEta;
  std::vector<float>    genEta;
  std::vector<float>    genPhi;
  std::vector<uint64_t> genTrackId;
  std::vector<int>      genEvent;
  std::vector<int>      genIsConverted;
  std::vector<float>    genConvR;
  std::vector<float>    genConvZ;

  std::vector<int>    pfEvent;
  std::vector<int>    pfPhi;
  std::vector<int>    pfEta;
  std::vector<double> pfE;

  std::vector<int>   mlEvent;
  std::vector<int>   mlN;        // which sample (0..numClusters-1)
  std::vector<int>   mlK;        // which cluster slot (0..maxClusters-1)
  std::vector<float> mlCenterX;
  std::vector<float> mlCenterY;
  std::vector<float> mlEnergy;
  std::vector<float> mlSeed;

  std::vector<int>   decayTrackId;
  std::vector<int>   decayPDG;
  std::vector<int>   decayParentId;
  std::vector<int>   decayGenIdx;
  std::vector<float> decayPPhi;
  std::vector<float> decayPEta;
  std::vector<float> decayE;
  std::vector<float> decaySourceX;
  std::vector<float> decaySourceY;
  std::vector<float> decaySourceZ;
  std::vector<int>   decayEvent;
  std::vector<int>   decayOffset;
  std::vector<int>   decayList;

  std::map<unsigned, unsigned> geantToIndex_;
};

#endif
