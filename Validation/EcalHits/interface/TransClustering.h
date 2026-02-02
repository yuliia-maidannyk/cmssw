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

#include "DQMServices/Core/interface/DQMEDAnalyzer.h"
#include "DQMServices/Core/interface/DQMStore.h"

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

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/Records/interface/EcalBarrelGeometryRecord.h"

#include <map>
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <string>

class TransClustering : public DQMEDAnalyzer {
  typedef std::map<std::pair<int, int>, float> MapType;
  typedef std::map<std::pair<int, int>, std::vector<int>> CaloMapType;

public:
  typedef dqm::legacy::DQMStore DQMStore;

  explicit TransClustering(const edm::ParameterSet&);
  ~TransClustering() override;

protected: 
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;

  float ecalEta(float EtaParticle, float Zvertex, float plane_Radius);
  void fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices);

  void analyze(const edm::Event&, const edm::EventSetup&) override;

private:
  std::string g4InfoLabel;
  std::string EBHitsCollection;
  std::string ValidationCollection;
  std::string jobId;

  const CaloGeometry* geo;
  //const CaloSubdetectorGeometry* barrelGeom;

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
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> ecalGeometryToken;

  TFile* myFile;
  TTree* simTree;
  TTree* recoTree;
  TTree* caloTree;
  TTree* pfTree;
  TTree* genTree;

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
  std::vector<float>    caloPt;
  std::vector<float>    caloPhi;
  std::vector<float>    caloEta;
  std::vector<float>    caloPDG;
  std::vector<int>      caloEvent;
  std::vector<int>      caloSubEvent;
  std::vector<int>      caloIEta;
  std::vector<int>      caloIPhi;
  std::vector<int>      caloValues;
  std::vector<uint64_t> caloTrackId;

  std::vector<float> genT;
  std::vector<float> genE;
  std::vector<float> genPt;
  std::vector<float> genPhi;
  std::vector<float> genEta;
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

  std::map<unsigned, unsigned> geantToIndex_;
};

#endif
