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

#include "DataFormats/ParticleFlowReco/interface/PFCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFClusterFwd.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyCalibration.h"
#include "RecoParticleFlow/PFClusterTools/interface/PFEnergyResolution.h"
#include "CommonTools/ParticleFlow/interface/PFClusterWidthAlgo.h"
#include "RecoParticleFlow/PFTracking/interface/PFTrackAlgoTools.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFraction.h" 

#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/EcalAlgo/interface/EcalBarrelGeometry.h"
#include "Geometry/Records/interface/EcalBarrelGeometryRecord.h"

#include "FWCore/Framework/interface/one/EDAnalyzer.h"

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
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;
  void beginJob() override;
  // void endJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void clearEventData();

private:

  std::string jobId;

  const EcalBarrelGeometry* barrelGeom_ = nullptr;
  edm::EDGetTokenT<reco::PFClusterCollection> pfClusterToken;
  edm::EDGetTokenT<reco::PFClusterCollection> mlpfClusterToken;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> ecalGeomToken;

  TTree* pfTree;
  TTree* mlTree;

  std::vector<int>    pfEvent;
  std::vector<float>  pfPhi;
  std::vector<float>  pfEta;
  std::vector<double> pfE;
  std::vector<double> pfRawE;

  std::vector<int>    mlEvent;
  std::vector<float>  mlPhi;
  std::vector<float>  mlEta;
  std::vector<double> mlE;

//   std::vector<int>    pfMapEvent;
//   std::vector<int>    pfMapIEta;
//   std::vector<int>    pfMapIPhi;
//   std::vector<double> pfMapEnergy;
//   std::vector<float>  pfMapFraction;

  MonitorElement *mlGunPhi_;
  MonitorElement *mlGunEta_;
  MonitorElement *mlGunE_;
  MonitorElement *mlGunN_;

  MonitorElement *pfGunPhi_;
  MonitorElement *pfGunEta_;
  MonitorElement *pfGunE_;
  MonitorElement *pfGunN_;
};

#endif
