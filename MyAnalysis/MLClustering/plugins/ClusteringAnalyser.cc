#include "DataFormats/Math/interface/GeantUnits.h"
#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"
#include "MyAnalysis/MLClustering/interface/ClusteringAnalyser.h"

#include <fstream>

#define PRINT_DEBUG 0
#define VALIDATE 0

// ------------ constructor and destructor --------------
ClusteringAnalyser::ClusteringAnalyser(const edm::ParameterSet& iConfig)
  : jobId(iConfig.getParameter<std::string>("jobId")),
{
  pfClusterToken = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECAL"));
  mlpfClusterToken = consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("particleFlowClusterECALML"));
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

  edm::Handle<reco::PFClusterCollection> mlpfClusters;
  iEvent.getByToken(mlpfClusterToken, mlpfClusters);

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

    if (VALIDATE) {
        pfGunE_->Fill(corr_E);
        pfGunPhi_->Fill(iphi_f);
        pfGunEta_->Fill(ieta_f);
    }

    if (PRINT_DEBUG) {std::cout << " PFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" 
      << " with eta_f=" << ieta_f << ", phi_f=" << iphi_f << std::endl;}
  }
  if (VALIDATE) {
      pfGunN_->Fill(nclust);
  }

  // ***************** Loop over the MLPFClusters ****************
  
  nclust = 0;
    for (const auto& pf : *mlpfClusters)
    {
        if (pf.layer() != PFLayer::ECAL_BARREL) continue;
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

        float hitEta = pf.positionREP().Eta();
        float hitPhi = pf.positionREP().Phi();

        const CaloCellGeometry* cell = barrelGeom_->getGeometry(ebid);

        float dEta = hitEta - cell->etaPos();
        float dPhi = reco::deltaPhi(hitPhi, cell->phiPos());
        float ieta_f = ieta + dEta / cell->etaSpan() + 0.5f;
        float iphi_f = iphi + dPhi / cell->phiSpan() + 0.5f;

        if (VALIDATE) {
            mlGunE_->Fill(corr_E);
            mlGunEta_->Fill(ieta_f);
            mlGunPhi_->Fill(iphi_f);
        }
        mlEta.push_back(ieta_f);
        mlPhi.push_back(iphi_f);

        if (PRINT_DEBUG) {std::cout << " MLPFCluster E=" << corr_E << " at (" << ieta << ", " << iphi << ")" 
        << " with eta_f=" << ieta_f << ", phi_f=" << iphi_f << std::endl;}
    }
  if (VALIDATE) {
      mlGunN_->Fill(nclust);
  }

  // **************** Fill all trees ****************
  mlTree->Fill();
  pfTree->Fill();

} // --- end of analyze

void ClusteringAnalyser::clearEventData() {
  pfEvent.clear(); pfPhi.clear(); pfEta.clear(); pfE.clear();
  mlEvent.clear(); mlPhi.clear(); mlEta.clear(); mlE.clear();
  pfRawE.clear(); //pfMapEvent.clear(); pfMapIEta.clear(); pfMapIPhi.clear(); pfMapEnergy.clear(); pfMapFraction.clear();
}

void ClusteringAnalyser::bookHistograms(DQMStore::IBooker& ib, edm::Run const&, edm::EventSetup const& iSetup) {
  
  // ***************** Get the Ecal Barrel Geometry *****************
  const CaloGeometry& geo = iSetup.getData(ecalGeomToken);
  barrelGeom_ = dynamic_cast<const EcalBarrelGeometry*>(geo.getSubdetectorGeometry(DetId::Ecal, EcalBarrel));

  if (VALIDATE) {
  std::string histo = "ClusteringAnalyser Gun ML Phi";
  mlGunPhi_ = ib.book1D(histo, histo, 360, 0., 360.);

  histo = "ClusteringAnalyser Gun ML Eta";
  mlGunEta_ = ib.book1D(histo, histo, 170, -85., 85.);

  histo = "ClusteringAnalyser Gun ML Energy";
  mlGunE_ = ib.book1D(histo, histo, 100, 0., 100.);

  histo = "ClusteringAnalyser Gun ML NClusters";
  mlGunN_ = ib.book1D(histo, histo, 50, 0., 50.);

  histo = "ClusteringAnalyser Gun PF Phi";
  pfGunPhi_ = ib.book1D(histo, histo, 360, 0., 360.);

  histo = "ClusteringAnalyser Gun PF Eta";
  pfGunEta_ = ib.book1D(histo, histo, 170, -85., 85.);

  histo = "ClusteringAnalyser Gun PF Energy";
  pfGunE_ = ib.book1D(histo, histo, 100, 0., 100.);
  
  histo = "ClusteringAnalyser Gun PF NClusters";
  pfGunN_ = ib.book1D(histo, histo, 50, 0., 50.);
  }
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
}

DEFINE_FWK_MODULE(ClusteringAnalyser);
