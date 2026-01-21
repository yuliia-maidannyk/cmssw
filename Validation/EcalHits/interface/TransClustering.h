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

// #include "CondFormats/EcalObjects/interface/EcalADCToGeVConstant.h"
// #include "CondFormats/DataRecord/interface/EcalADCToGeVConstantRcd.h"
// #include "CondFormats/EcalObjects/interface/EcalChannelStatus.h"
// #include "CondFormats/DataRecord/interface/EcalChannelStatusRcd.h"
// #include "Geometry/CaloTopology/interface/EcalTrigTowerConstituentsMap.h"
// #include "Geometry/Records/interface/IdealGeometryRecord.h"

//#include "Validation/EcalClusters/interface/EcalSimPhotonMCTruth.h"

#include <map>
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <string>

class EcalSimPhotonMCTruth {
public:
  EcalSimPhotonMCTruth()
      : isAConversion_(0), thePhoton_(0., 0., 0., 0.), theR_(0.), theZ_(0.), theConvVertex_(0., 0., 0., 0.) {}

  EcalSimPhotonMCTruth(const math::XYZTLorentzVectorD &v) : thePhoton_(v) {}

  EcalSimPhotonMCTruth(int isAConversion,
                       const math::XYZTLorentzVectorD &v,
                       float rconv,
                       float zconv,
                       const math::XYZTLorentzVectorD &convVertex,
                       const math::XYZTLorentzVectorD &pV,
                       const std::vector<const SimTrack *> &tracks);

  math::XYZTLorentzVectorD primaryVertex() const { return thePrimaryVertex_; }
  int isAConversion() const { return isAConversion_; }
  float radius() const { return theR_; }
  float z() const { return theZ_; }
  math::XYZTLorentzVectorD fourMomentum() const { return thePhoton_; }
  math::XYZTLorentzVectorD vertex() const { return theConvVertex_; }
  std::vector<const SimTrack *> simTracks() const { return tracks_; }

private:
  int isAConversion_;
  math::XYZTLorentzVectorD thePhoton_;
  float theR_;
  float theZ_;
  math::XYZTLorentzVectorD theConvVertex_;
  math::XYZTLorentzVectorD thePrimaryVertex_;
  std::vector<const SimTrack *> tracks_;
};

class TransClustering : public DQMEDAnalyzer {
  typedef std::map<std::pair<int, int>, float> MapType;

public:
  typedef dqm::legacy::DQMStore DQMStore;

  explicit TransClustering(const edm::ParameterSet&);
  ~TransClustering() override;

protected: 
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;

  std::vector<EcalSimPhotonMCTruth> findMcTruth(std::vector<SimTrack> &theSimTracks, std::vector<SimVertex> &theSimVertices);

  void fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices);

  float ecalEta(float EtaParticle, float Zvertex, float plane_Radius);

  void analyze(const edm::Event&, const edm::EventSetup&) override;

private:
  std::string g4InfoLabel;
  std::string EBHitsCollection;
  std::string ValidationCollection;
  std::string jobId;

  edm::EDGetTokenT<edm::PCaloHitContainer> EBHitsToken;
  edm::EDGetTokenT<PEcalValidInfo> ValidationCollectionToken;
  edm::EDGetTokenT<reco::GenParticleCollection> genParticleToken;
  edm::EDGetTokenT<edm::SimTrackContainer> SimTrackToken;
  edm::EDGetTokenT<edm::SimVertexContainer> SimVertexToken;
  //edm::EDGetTokenT<EcalRecHitCollection> reducedBarrelRecHitToken;
  //edm::ESGetToken<EcalADCToGeVConstant, EcalADCToGeVConstantRcd> pAgc_;
  edm::EDGetTokenT<EcalUncalibratedRecHitCollection> EBuncalibrechitCollection_Token;
  edm::EDGetTokenT<EBRecHitCollection> EBrechitCollection_Token;
  edm::EDGetTokenT<CaloParticleCollection> CaloParticle_Token;
  edm::EDGetTokenT<edm::HepMCProduct> HepMCToken;

  TFile* myFile;
  TTree* simTree;
  TTree* recoTree;
  TTree* caloTree;

  std::map<unsigned, unsigned> geantToIndex_;

  std::vector<int> mapIEta;
  std::vector<int> mapIPhi;
  std::vector<float> mapValues;

  std::vector<int> recIEta;
  std::vector<int> recIPhi;
  std::vector<float> recValues;

  std::vector<std::string>    simPDG;
  std::vector<float>    simT;
  std::vector<float>    simE;
  std::vector<int>      simPhi;
  std::vector<int>      simEta;
  std::vector<float>    simZ;
  std::vector<int>      simEvent;
  std::vector<uint64_t> simTrackId;

  std::vector<float> recoT;
  std::vector<float> recoE;
  std::vector<float> recoLocX;
  std::vector<float> recoLocY;
  std::vector<float> recoLocZ;
  std::vector<float> recoTheta;
  std::vector<float> recoPhi;
  std::vector<float> recoEta;
  std::vector<float> recoX;
  std::vector<float> recoY;
  std::vector<float> recoZ;
  std::vector<int> recoEvent;
  std::vector<uint32_t> recoID;

  std::vector<float> caloT;
  std::vector<float> caloE;
  std::vector<float> caloPt;
  // std::vector<float> caloX;
  // std::vector<float> caloY;
  // std::vector<float> caloZ;
  // std::vector<float> caloTheta;
  std::vector<float> caloPhi;
  std::vector<float> caloEta;
  std::vector<float> caloPDG;
  std::vector<int> caloEvent;
  // std::vector<int> caloSubEvent;
  // std::vector<uint64_t> caloTrackId;
  // std::vector<uint32_t> caloID;
  // std::vector<int> caloDecay;
  std::vector<float> caloSourceX;
  std::vector<float> caloSourceY;
  std::vector<float> caloSourceZ;
  // std::vector<float> caloSourceT;
};

#endif
