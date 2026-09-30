#ifndef ClusteringAnalyser_H
#define ClusteringAnalyser_H

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
#include "DataFormats/EgammaReco/interface/BasicCluster.h"

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

class ClusteringAnalyser : public DQMOneEDAnalyzer<> {

public:
  typedef dqm::legacy::DQMStore DQMStore;

  explicit ClusteringAnalyser(const edm::ParameterSet &);

  ~ClusteringAnalyser() override;

protected: 
  void beginJob() override;
  // void endJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void clearEventData();
  void bookHistograms(DQMStore::IBooker& ib, edm::Run const&, edm::EventSetup const& iSetup);

private:

  std::string jobId;

  const EcalBarrelGeometry* barrelGeom_ = nullptr;

  edm::EDGetTokenT<reco::PFClusterCollection> pfClusterToken;
  //edm::EDGetTokenT<reco::PFClusterCollection> mlpfClusterToken;
  edm::EDGetTokenT<std::vector<reco::BasicCluster>> mlpfClusterToken2;
  edm::EDGetTokenT<std::vector<reco::BasicCluster>> mlpfClusterToken;
  edm::EDGetTokenT<std::vector<reco::BasicCluster>> ticlClusterToken;
  //edm::ESGetToken<CaloSubdetectorGeometry, EcalBarrelGeometryRecord> barrelGeomToken;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> ecalGeomToken;

  TTree* pfTree;
  TTree* mlTree;
  TTree* ticlTree;

  std::vector<int>    pfEvent;
  std::vector<float>  pfPhi;
  std::vector<float>  pfEta;
  std::vector<double> pfE;
  std::vector<double> pfRawE;

  std::vector<int>    mlEvent;
  std::vector<float>  mlPhi;
  std::vector<float>  mlEta;
  std::vector<double> mlE;
  std::vector<int>    mlEvent2;
  std::vector<float>  mlPhi2;
  std::vector<float>  mlEta2;
  std::vector<double> mlE2;

//   std::vector<int>    pfMapEvent;
//   std::vector<int>    pfMapIEta;
//   std::vector<int>    pfMapIPhi;
//   std::vector<double> pfMapEnergy;
//   std::vector<float>  pfMapFraction;

  std::vector<int>    ticlEvent;
  std::vector<float>  ticlPhi;
  std::vector<float>  ticlEta;
  std::vector<double> ticlE;
  std::vector<int>    ticlIEvent;
  std::vector<int>    ticlIPhi;
  std::vector<int>    ticlIEta;
  std::vector<float>  ticlIF;
};

#endif
