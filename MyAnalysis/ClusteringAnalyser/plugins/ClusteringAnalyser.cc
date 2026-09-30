#include "DataFormats/Math/interface/GeantUnits.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"
#include "MyAnalysis/ClusteringAnalyser/interface/ClusteringAnalyser.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include <fstream>

// ------------ constructor and destructor --------------
ClusteringAnalyser::ClusteringAnalyser(const edm::ParameterSet& iConfig)
  : jobId(iConfig.getParameter<std::string>("jobId"))
{
  pfClusterToken   = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECAL"));
  //mlpfClusterToken = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("mlPFClusters"));
  // mlpfClusterToken2 = consumes<std::vector<reco::BasicCluster>>(iConfig.getParameter<edm::InputTag>("mlPFClusters2"));
  mlpfClusterToken = consumes<std::vector<reco::BasicCluster>>(iConfig.getParameter<edm::InputTag>("mlPFClusters"));
  ticlClusterToken = consumes<std::vector<reco::BasicCluster>>(iConfig.getParameter<edm::InputTag>("ticlClusters"));
  ecalGeomToken = esConsumes<CaloGeometry, CaloGeometryRecord, edm::Transition::BeginRun>();
}

ClusteringAnalyser::~ClusteringAnalyser() {}

// ------------ method called for each event  ------------
void ClusteringAnalyser::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  using namespace std;
  using namespace geant_units::operators;

  clearEventData();

  edm::Handle<reco::PFClusterCollection> pfClusters;
  iEvent.getByToken(pfClusterToken, pfClusters);

//   edm::Handle<reco::PFClusterCollection> mlpfClusters;
//   iEvent.getByToken(mlpfClusterToken, mlpfClusters);

  edm::Handle<std::vector<reco::BasicCluster>> ticlClusters;
  iEvent.getByToken(ticlClusterToken, ticlClusters);

  edm::Handle<std::vector<reco::BasicCluster>> mlpfClusters;
  iEvent.getByToken(mlpfClusterToken, mlpfClusters);

  // edm::Handle<std::vector<reco::BasicCluster>> mlpfClusters2;
  // iEvent.getByToken(mlpfClusterToken2, mlpfClusters2);

  // ***************** Loop over the PFClusters ****************

  int nclust = 0;
  for (const auto& pf : *pfClusters)
  {
    if (pf.layer() != PFLayer::ECAL_BARREL) continue;
    nclust++;
    pfEvent.push_back(iEvent.id().event());
    float corr_E = static_cast<float>(pf.correctedEnergy());
    float raw_E = static_cast<float>(pf.energy());
    pfE.push_back(corr_E);
    pfRawE.push_back(raw_E);

    math::XYZPoint pfPos = pf.position();
    GlobalPoint gp(pfPos.x(), pfPos.y(), pfPos.z());
    DetId closestCell = barrelGeom_->getClosestCell(gp);
    EBDetId ebid(closestCell);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();

    float hitEta = pf.positionREP().Eta();
    float hitPhi = pf.positionREP().Phi();

    const CaloCellGeometry* cell = barrelGeom_->getGeometry(ebid);

    float dEta = hitEta - cell->etaPos();
    float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
    float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f; // [ieta, ieta+1] crystal center @ 0.5
    float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f; // [iphi, iphi+1] crystal center @ 0.5

    pfEta.push_back(ieta_f);
    pfPhi.push_back(iphi_f);

    LogDebug("ClusteringAnalyser") << " PFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" 
            << " with eta_f=" << ieta_f << ", phi_f=" << iphi_f << "\n";
  }

  // ***************** Loop over the MLPFClusters ****************
  
  nclust = 0;
  for (const auto& pf : *mlpfClusters)
  {
    nclust++;
    mlEvent.push_back(iEvent.id().event());
    float corr_E = static_cast<float>(pf.correctedEnergy());
    mlE.push_back(corr_E);

    math::XYZPoint pfPos = pf.position();
    GlobalPoint gp(pfPos.x(), pfPos.y(), pfPos.z());
    DetId closestCell = barrelGeom_->getClosestCell(gp);
    EBDetId ebid(closestCell);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();

    float hitEta = pf.eta();
    float hitPhi = pf.phi();

    const CaloCellGeometry* cell = barrelGeom_->getGeometry(ebid);

    float dEta = hitEta - cell->etaPos();
    float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
    float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f;
    float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f;

    mlEta.push_back(ieta_f);
    mlPhi.push_back(iphi_f);

    LogDebug("ClusteringAnalyser") << " MLPFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" 
        << " with eta_f=" << ieta_f << ", phi_f=" << iphi_f << "\n";
  }

  // ***************** Loop over the TICL Clusters ****************
  
  nclust = 0;
  for (const auto& pf : *ticlClusters)
  {
    nclust++;
    ticlEvent.push_back(iEvent.id().event());
    float corr_E = static_cast<float>(pf.correctedEnergy());
    ticlE.push_back(corr_E);

    math::XYZPoint pfPos = pf.position();
    GlobalPoint gp(pfPos.x(), pfPos.y(), pfPos.z());
    DetId closestCell = barrelGeom_->getClosestCell(gp);
    EBDetId ebid(closestCell);
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();

    float hitEta = pf.eta();
    float hitPhi = pf.phi();

    const CaloCellGeometry* cell = barrelGeom_->getGeometry(ebid);

    float dEta = hitEta - cell->etaPos();
    float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
    float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f;
    float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f;

    ticlEta.push_back(ieta_f);
    ticlPhi.push_back(iphi_f);

    LogDebug("ClusteringAnalyser") << " TICLCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" 
        << " with eta_f=" << ieta_f << ", phi_f=" << iphi_f << "\n";

    std::map<std::pair<int,int>, float> recoCellTotal;
    for (const auto& hitAndFraction : pf.hitsAndFractions())
    {
      DetId detId = hitAndFraction.first;
      EBDetId ebid(detId);
      int ieta = ebid.ieta();
      int iphi = ebid.iphi();
      float fraction  = hitAndFraction.second;
      recoCellTotal[std::make_pair(ieta, iphi)] += fraction;
    }
    for (const auto& [key, ei] : recoCellTotal) {
      ticlIEta.push_back(key.first);
      ticlIPhi.push_back(key.second);
      ticlIF.push_back(ei);
      ticlIEvent.push_back(iEvent.id().event());
    }
  }

  // ***************** Loop over the MLPFClusters ****************
  
  // nclust = 0;
  // for (const auto& pf : *mlpfClusters2)
  // {
  //   nclust++;
  //   mlEvent2.push_back(iEvent.id().event());
  //   float corr_E = static_cast<float>(pf.correctedEnergy());
  //   mlE2.push_back(corr_E);

  //   math::XYZPoint pfPos = pf.position();
  //   GlobalPoint gp(pfPos.x(), pfPos.y(), pfPos.z());
  //   DetId closestCell = barrelGeom_->getClosestCell(gp);
  //   EBDetId ebid(closestCell);
  //   int ieta = ebid.ieta();
  //   int iphi = ebid.iphi();

  //   float hitEta = pf.eta();
  //   float hitPhi = pf.phi();

  //   const CaloCellGeometry* cell = barrelGeom_->getGeometry(ebid);

  //   float dEta = hitEta - cell->etaPos();
  //   float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
  //   float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f;
  //   float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f;

  //   mlEta2.push_back(ieta_f);
  //   mlPhi2.push_back(iphi_f);
  // }

  // **************** Fill all trees ****************
  mlTree->Fill();
  pfTree->Fill();
  ticlTree->Fill();
} // --- end of analyze

void ClusteringAnalyser::clearEventData() {
  pfEvent.clear(); pfPhi.clear(); pfEta.clear(); pfE.clear();
  mlEvent.clear(); mlPhi.clear(); mlEta.clear(); mlE.clear();
  ticlEvent.clear(); ticlPhi.clear(); ticlEta.clear(); ticlE.clear();
  ticlIEvent.clear(); ticlIPhi.clear(); ticlIEta.clear(); ticlIF.clear();
  mlEvent2.clear(); mlPhi2.clear(); mlEta2.clear(); mlE2.clear();
  pfRawE.clear(); //pfMapEvent.clear(); pfMapIEta.clear(); pfMapIPhi.clear(); pfMapEnergy.clear(); pfMapFraction.clear();
}

void ClusteringAnalyser::bookHistograms(DQMStore::IBooker& ib, edm::Run const&, edm::EventSetup const& iSetup) {
  // ***************** Get the Ecal Barrel Geometry *****************
  const CaloGeometry& geo = iSetup.getData(ecalGeomToken);
  barrelGeom_ = dynamic_cast<const EcalBarrelGeometry*>(geo.getSubdetectorGeometry(DetId::Ecal, EcalBarrel));
}

void ClusteringAnalyser::beginJob() {

  edm::Service<TFileService> fs;

  pfTree = fs->make<TTree>("pfTree", "A tree with PFCluster information");
  pfTree->Branch("energy", &pfE);
  pfTree->Branch("eta",    &pfEta);
  pfTree->Branch("phi",    &pfPhi);
  pfTree->Branch("event",  &pfEvent);
  pfTree->Branch("raw",    &pfRawE);
//   pfTree->Branch("mapEvent",    &pfMapEvent);
//   pfTree->Branch("mapIEta",     &pfMapIEta);
//   pfTree->Branch("mapIPhi",     &pfMapIPhi);
//   pfTree->Branch("mapEnergy",   &pfMapEnergy);
//   pfTree->Branch("mapFraction", &pfMapFraction);

  mlTree = fs->make<TTree>("mlTree", "A tree with MLPFCluster information");
  mlTree->Branch("energy", &mlE);
  mlTree->Branch("eta",    &mlEta);
  mlTree->Branch("phi",    &mlPhi);
  mlTree->Branch("event",  &mlEvent);
  // mlTree->Branch("energy2", &mlE2);
  // mlTree->Branch("eta2",    &mlEta2);
  // mlTree->Branch("phi2",    &mlPhi2);
  // mlTree->Branch("event2",  &mlEvent2);
  
  ticlTree = fs->make<TTree>("ticlTree", "A tree with TICL Cluster information");
  ticlTree->Branch("energy", &ticlE);
  ticlTree->Branch("eta",    &ticlEta);
  ticlTree->Branch("phi",    &ticlPhi);
  ticlTree->Branch("event",  &ticlEvent);
  ticlTree->Branch("mapEta",   &ticlIEta);
  ticlTree->Branch("mapPhi",   &ticlIPhi);
  ticlTree->Branch("fraction",  &ticlIF);
  ticlTree->Branch("mapEvent",  &ticlIEvent);
}

DEFINE_FWK_MODULE(ClusteringAnalyser);
