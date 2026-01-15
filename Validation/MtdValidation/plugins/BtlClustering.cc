#include <string>
#include "MTDHit.h"
#include <TFile.h>
#include <TTree.h>
#include <vector>

#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"

#include "DQMServices/Core/interface/DQMEDAnalyzer.h"
#include "DQMServices/Core/interface/DQMStore.h"

#include "DataFormats/Common/interface/ValidHandle.h"
#include "DataFormats/Math/interface/GeantUnits.h"
#include "DataFormats/ForwardDetId/interface/BTLDetId.h"
#include "DataFormats/FTLRecHit/interface/FTLRecHitCollections.h"
#include "DataFormats/FTLRecHit/interface/FTLClusterCollections.h"
#include "DataFormats/TrackerRecHit2D/interface/MTDTrackingRecHit.h"

#include "SimDataFormats/CaloAnalysis/interface/MtdCaloParticle.h"
#include "SimDataFormats/CaloAnalysis/interface/MtdSimTrackster.h"
#include "SimDataFormats/CaloAnalysis/interface/MtdCaloParticleFwd.h"
#include "SimDataFormats/CaloAnalysis/interface/MtdSimTracksterFwd.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticle.h"
#include "SimDataFormats/TrackingAnalysis/interface/TrackingParticleFwd.h"
#include "SimDataFormats/CaloAnalysis/interface/MtdSimLayerCluster.h"
#include "SimDataFormats/Associations/interface/MtdRecoClusterToSimLayerClusterAssociationMap.h"
#include "SimDataFormats/CrossingFrame/interface/CrossingFrame.h"
#include "SimDataFormats/CrossingFrame/interface/MixCollection.h"
#include "SimDataFormats/TrackingHit/interface/PSimHit.h"

#include "Geometry/Records/interface/MTDDigiGeometryRecord.h"
#include "Geometry/Records/interface/MTDTopologyRcd.h"
#include "Geometry/MTDGeometryBuilder/interface/MTDGeometry.h"
#include "Geometry/MTDGeometryBuilder/interface/MTDTopology.h"
#include "Geometry/MTDGeometryBuilder/interface/ProxyMTDTopology.h"
#include "Geometry/MTDGeometryBuilder/interface/RectangularMTDTopology.h"
#include "Geometry/MTDCommonData/interface/MTDTopologyMode.h"

#include "RecoLocalFastTime/Records/interface/MTDCPERecord.h"
#include "RecoLocalFastTime/FTLClusterizer/interface/MTDClusterParameterEstimator.h"

//#include "SimDataFormats/Vertex/interface/SimVertex.h"

class BtlClustering : public DQMEDAnalyzer {
public:
  explicit BtlClustering(const edm::ParameterSet&);
  ~BtlClustering() override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;

  void analyze(const edm::Event&, const edm::EventSetup&) override;

  // ------------ member data ------------

  const std::string folder_;
  const double hitMinEnergy_;
  const std::string jobId_;

  edm::EDGetTokenT<MtdSimTracksterCollection> simTracksterToken_;
  edm::EDGetTokenT<MtdCaloParticleCollection> caloToken_;
  edm::EDGetTokenT<TrackingParticleCollection> trackingParticleCollectionToken_;
  edm::EDGetTokenT<FTLRecHitCollection> btlRecHitsToken_;
  edm::EDGetTokenT<CrossingFrame<PSimHit>> btlSimHitsToken_;
  //edm::EDGetTokenT<std::vector<SimVertex>> simVertexToken_;
  edm::EDGetTokenT<FTLClusterCollection> btlRecCluToken_;
  edm::EDGetTokenT<MTDTrackingDetSetVector> mtdTrackingHitToken_;
  edm::EDGetTokenT<MtdRecoClusterToSimLayerClusterAssociationMap> r2sAssociationMapToken_;

  const edm::ESGetToken<MTDGeometry, MTDDigiGeometryRecord> mtdgeoToken_;
  const edm::ESGetToken<MTDTopology, MTDTopologyRcd> mtdtopoToken_;
  const edm::ESGetToken<MTDClusterParameterEstimator, MTDCPERecord> cpeToken_;

  TFile* myFile;
  TTree* simTree;
  TTree* recoTree;
  TTree* caloTree;
  TTree* simTracksterTree;
  TTree* simLayerClusterTree;

  std::vector<float> simLayerClusterE;
  std::vector<float> simLayerClusterT;
  std::vector<float> simLayerClusterLocX;
  std::vector<float> simLayerClusterLocY;
  std::vector<float> simLayerClusterLocZ;
  std::vector<float> simLayerClusterTheta;
  std::vector<float> simLayerClusterPhi;
  std::vector<float> simLayerClusterEta;
  std::vector<float> simLayerClusterZ;
  std::vector<float> simLayerClusterPDG;
  std::vector<uint32_t> simLayerClusterSeed;
  std::vector<int> simLayerClusterEvent;

  std::vector<float> simTracksterE;
  std::vector<float> simTracksterT;
  std::vector<float> simTracksterLocX;
  std::vector<float> simTracksterLocY;
  std::vector<float> simTracksterLocZ;
  std::vector<float> simTracksterX;
  std::vector<float> simTracksterY;
  std::vector<float> simTracksterZ;
  std::vector<float> simTracksterTheta;
  std::vector<float> simTracksterPhi;
  std::vector<float> simTracksterEta;
  std::vector<float> simTracksterPDG;
  std::vector<int> simTracksterEvent;
  std::vector<uint64_t> simTracksterTrackId;

  std::vector<float> simPDG;
  std::vector<float> simT;
  std::vector<float> simE;
  std::vector<float> simLocX;
  std::vector<float> simLocY;
  std::vector<float> simLocZ;
  std::vector<float> simX;
  std::vector<float> simY;
  std::vector<float> simTheta;
  std::vector<float> simPhi;
  std::vector<float> simEta;
  std::vector<float> simZ;
  std::vector<int> simEvent;
  std::vector<uint32_t> simID;
  std::vector<uint32_t> simType;
  std::vector<uint64_t> simTrackId;
  std::vector<uint16_t> simProcess;

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
  std::vector<float> caloX;
  std::vector<float> caloY;
  std::vector<float> caloZ;
  std::vector<float> caloTheta;
  std::vector<float> caloPhi;
  std::vector<float> caloEta;
  std::vector<float> caloPDG;
  std::vector<int> caloEvent;
  std::vector<int> caloSubEvent;
  std::vector<uint64_t> caloTrackId;
  std::vector<uint32_t> caloID;
  std::vector<int> caloDecay;
  std::vector<float> caloSourceX;
  std::vector<float> caloSourceY;
  std::vector<float> caloSourceZ;
  std::vector<float> caloSourceT;

  // Per-event storage - each entry corresponds to ONE event
  // std::vector<std::vector<std::vector<int>>> tp_pdg;      // [event][tp][g4track]
  // std::vector<std::vector<std::vector<float>>> tp_pt;
  // std::vector<std::vector<std::vector<int>>> tp_trackId;

  // std::vector<std::vector<float>> tp_sourceX;  // [event][tp]
  // std::vector<std::vector<float>> tp_sourceY;
  // std::vector<std::vector<float>> tp_sourceZ;
  // std::vector<std::vector<float>> tp_sourceT;

  // std::vector<std::vector<std::vector<float>>> tp_decayX;  // [event][tp][decay]
  // std::vector<std::vector<std::vector<float>>> tp_decayY;
  // std::vector<std::vector<std::vector<float>>> tp_decayZ;
  // std::vector<std::vector<std::vector<float>>> tp_decayT;

  // int currentEventId;  // Store event ID separately

};

// ------------ constructor and destructor --------------
BtlClustering::BtlClustering(const edm::ParameterSet& iConfig)
  : folder_(iConfig.getParameter<std::string>("folder")),
    hitMinEnergy_(iConfig.getParameter<double>("HitMinimumEnergy")),
    jobId_(iConfig.getParameter<std::string>("jobId")),
    mtdgeoToken_(esConsumes<MTDGeometry, MTDDigiGeometryRecord>()),
    mtdtopoToken_(esConsumes<MTDTopology, MTDTopologyRcd>()),
    cpeToken_(esConsumes<MTDClusterParameterEstimator, MTDCPERecord>(edm::ESInputTag("", "MTDCPEBase"))) {
  caloToken_ = consumes<MtdCaloParticleCollection>(iConfig.getParameter<edm::InputTag>("MtdCaloParticles"));
  simTracksterToken_ = consumes<MtdSimTracksterCollection>(iConfig.getParameter<edm::InputTag>("MtdSimTracksters"));
  trackingParticleCollectionToken_ = consumes<TrackingParticleCollection>(iConfig.getParameter<edm::InputTag>("SimTag"));
  btlRecHitsToken_ = consumes<FTLRecHitCollection>(iConfig.getParameter<edm::InputTag>("recHitsTag"));
  btlSimHitsToken_ = consumes<CrossingFrame<PSimHit>>(iConfig.getParameter<edm::InputTag>("simHitsTag"));
  // simVertexToken_ = consumes<std::vector<SimVertex>>(iConfig.getParameter<edm::InputTag>("SimVertexCollection"));
  btlRecCluToken_ = consumes<FTLClusterCollection>(iConfig.getParameter<edm::InputTag>("recCluTag"));
  mtdTrackingHitToken_ = consumes<MTDTrackingDetSetVector>(iConfig.getParameter<edm::InputTag>("trkHitTag"));
  r2sAssociationMapToken_ = consumes<MtdRecoClusterToSimLayerClusterAssociationMap>(
    iConfig.getParameter<edm::InputTag>("r2sAssociationMapTag"));
}

BtlClustering::~BtlClustering() {
  simTree->Fill();
  recoTree->Fill();
  caloTree->Fill();
  simTracksterTree->Fill();
  simLayerClusterTree->Fill();
  myFile->Write();
  myFile->Close();

  // Then clear vectors for next event
  /*
  tp_pdg.clear();
  tp_pt.clear();
  tp_trackId.clear();
  tp_sourceX.clear();
  tp_sourceY.clear();
  tp_sourceZ.clear();
  tp_sourceT.clear();
  tp_decayX.clear();
  tp_decayY.clear();
  tp_decayZ.clear();
  tp_decayT.clear();
  */
}

// ------------ method called for each event  ------------
void BtlClustering::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  using namespace std;
  using namespace geant_units::operators;

  static unsigned long eventCount = 0;
  ++eventCount;
  if (eventCount % 100 == 0) {
    std::cout << "Processed " << eventCount << " events" << std::endl;
  }

  auto geometryHandle = iSetup.getTransientHandle(mtdgeoToken_);
  const MTDGeometry* geom = geometryHandle.product();

  auto topologyHandle = iSetup.getTransientHandle(mtdtopoToken_);
  const MTDTopology* topology = topologyHandle.product();

  edm::Handle<MtdCaloParticleCollection> caloHandle;
  iEvent.getByToken(caloToken_, caloHandle);
  edm::Handle<MtdSimTracksterCollection> simTracksterHandle;
  iEvent.getByToken(simTracksterToken_, simTracksterHandle);
  edm::Handle<TrackingParticleCollection> trackingParticleCollectionHandle;
  iEvent.getByToken(trackingParticleCollectionToken_, trackingParticleCollectionHandle);
  // edm::Handle<std::vector<SimVertex>> simVerticesHandle;
  // iEvent.getByToken(simVertexToken_, simVerticesHandle);
  auto btlRecHitsHandle = makeValid(iEvent.getHandle(btlRecHitsToken_));
  auto btlSimHitsHandle = makeValid(iEvent.getHandle(btlSimHitsToken_));
  auto btlRecCluHandle = makeValid(iEvent.getHandle(btlRecCluToken_));
  auto mtdTrkHitHandle = makeValid(iEvent.getHandle(mtdTrackingHitToken_));
  const auto& r2sAssociationMap = iEvent.get(r2sAssociationMapToken_);
  MixCollection<PSimHit> btlSimHits(btlSimHitsHandle.product());

  // for (const auto& vtx : *simVerticesHandle) {
  //   std::cout << std::endl;
  //   std::cout << "******** Vertex ******* " << vtx.position() << std::endl;
  //   std::cout << "******** Parent vertex ID ******* " << vtx.parentIndex() << std::endl;
  //   std::cout << "******** Vertex ID ******* " << vtx.vertexId() << std::endl;
  //   //std::cout << "******** G4 Track ID ******* " << vtx.trackId() << std::endl;
  // }

  // --- Loop over the TrackingParticles
  // Each TrackingParticle can be associated to multiple G4 Tracks
  // Iterate over G4 Tracks to get all relevant information
  // Then add the vertex information
  // --- Loop over the TrackingParticles
  /*
  // Store the current event ID
  currentEventId = iEvent.id().event();
  
  // Temporary storage for this event's TPs
  std::vector<std::vector<int>> event_tp_pdg;
  std::vector<std::vector<float>> event_tp_pt;
  std::vector<std::vector<int>> event_tp_trackId;
  
  std::vector<float> event_tp_sourceX;
  std::vector<float> event_tp_sourceY;
  std::vector<float> event_tp_sourceZ;
  std::vector<float> event_tp_sourceT;
  
  std::vector<std::vector<float>> event_tp_decayX;
  std::vector<std::vector<float>> event_tp_decayY;
  std::vector<std::vector<float>> event_tp_decayZ;
  std::vector<std::vector<float>> event_tp_decayT;
  
  // --- Loop over the TrackingParticles in this event
  for (const auto& tp : *trackingParticleCollectionHandle) {
    
    // Temporary storage for this TP
    std::vector<int> pdg_list;
    std::vector<float> pt_list;
    std::vector<int> id_list;
    
    std::vector<float> decayX_list, decayY_list, decayZ_list, decayT_list;
    
    // Collect all G4 tracks for this TP
    for (auto g4T = tp.g4Track_begin(); g4T != tp.g4Track_end(); ++g4T) {
      pdg_list.push_back(g4T->type());
      pt_list.push_back(g4T->momentum().pt());
      id_list.push_back(g4T->trackId());
    }
    
    // Store this TP's G4Track info
    event_tp_pdg.push_back(pdg_list);
    event_tp_pt.push_back(pt_list);
    event_tp_trackId.push_back(id_list);
    
    // Parent vertex (one per TP)
    event_tp_sourceX.push_back(tp.parentVertex()->position().x());
    event_tp_sourceY.push_back(tp.parentVertex()->position().y());
    event_tp_sourceZ.push_back(tp.parentVertex()->position().z());
    event_tp_sourceT.push_back(tp.parentVertex()->position().t());
    
    // Decay vertices (can be multiple per TP)
    for (auto dv = tp.decayVertices_begin(); dv != tp.decayVertices_end(); ++dv) {
      decayX_list.push_back((*dv)->position().x());
      decayY_list.push_back((*dv)->position().y());
      decayZ_list.push_back((*dv)->position().z());
      decayT_list.push_back((*dv)->position().t());
    }
    
    event_tp_decayX.push_back(decayX_list);
    event_tp_decayY.push_back(decayY_list);
    event_tp_decayZ.push_back(decayZ_list);
    event_tp_decayT.push_back(decayT_list);
  }
  
  // Store this event's data
  tp_pdg.push_back(event_tp_pdg);
  tp_pt.push_back(event_tp_pt);
  tp_trackId.push_back(event_tp_trackId);
  
  tp_sourceX.push_back(event_tp_sourceX);
  tp_sourceY.push_back(event_tp_sourceY);
  tp_sourceZ.push_back(event_tp_sourceZ);
  tp_sourceT.push_back(event_tp_sourceT);
  
  tp_decayX.push_back(event_tp_decayX);
  tp_decayY.push_back(event_tp_decayY);
  tp_decayZ.push_back(event_tp_decayZ);
  tp_decayT.push_back(event_tp_decayT);
  */
  
  std::cout << "TrackingParticleCollection" << std::endl;
  for (const auto& tp : *trackingParticleCollectionHandle) {
    std::cout << tp << std::endl;
    int k = 0;
    for (TrackingParticle::g4t_iterator g4T = tp.g4Track_begin(); g4T != tp.g4Track_end(); ++g4T) {
      // G4Track info
      caloE.push_back(g4T->momentum().pt());
      caloTheta.push_back(g4T->momentum().theta());
      caloPhi.push_back(g4T->momentum().phi());
      caloEta.push_back(g4T->momentum().eta());
      caloPDG.push_back(g4T->type());
      caloTrackId.push_back(g4T->trackId());
      // Event info
      caloEvent.push_back(iEvent.id().event());
      caloSubEvent.push_back(k); // Number within this TP. Reset every TP
      k = k + 1;
      // std::cout << std::endl;
      // std::cout << "******** Parent vertex positions ******* " << tp.parentVertex()->position() << std::endl;
      // std::cout << "******** Vertex in tracker volume ******* " << tp.parentVertex()->inVolume() << std::endl;
      // std::cout << "******** G4 Track ID ******* " << g4T->trackId() << std::endl;
    }
    // Parent vertex info
    caloSourceX.push_back(tp.parentVertex()->position().x());
    caloSourceY.push_back(tp.parentVertex()->position().y());
    caloSourceZ.push_back(tp.parentVertex()->position().z());
    caloSourceT.push_back(tp.parentVertex()->position().t());
  } // --- end of TrackingParticle loop

  // --- Loop over the BTL SIM hits
  std::unordered_map<uint32_t, MTDHit> m_btlSimHits;
  for (auto const& simHit : btlSimHits) {
    // --- Use only hits compatible with the in-time bunch-crossing
    if (simHit.tof() < 0 || simHit.tof() > 25.)
      continue;

    DetId id = simHit.detUnitId();

    auto simHitIt = m_btlSimHits.emplace(id.rawId(), MTDHit()).first;

    // --- Accumulate the energy (in MeV) of SIM hits in the same detector cell
    (simHitIt->second).energy += convertUnitsTo(0.001_MeV, simHit.energyLoss());

    // --- Get the time of the first SIM hit in the cell
    if ((simHitIt->second).time == 0 || simHit.tof() < (simHitIt->second).time) {
      (simHitIt->second).time = simHit.tof();
    
      auto hit_pos = simHit.localPosition();
      (simHitIt->second).x = hit_pos.x();
      (simHitIt->second).y = hit_pos.y();
      (simHitIt->second).z = hit_pos.z();

      (simHitIt->second).time = simHit.tof();
      (simHitIt->second).pdgId = simHit.particleType();
      (simHitIt->second).type = simHit.offsetTrackId();
      (simHitIt->second).process = simHit.processType();
      (simHitIt->second).trackId = simHit.originalTrackId();
    }
    // BTLDetId detId = BTLDetId(id);
    // DetId geoId = detId.geographicalId(MTDTopologyMode::crysLayoutFromTopoMode(topology->getMTDTopologyMode()));
    // const MTDGeomDet* thedet = geom->idToDet(geoId);
    // const ProxyMTDTopology& topoproxy = static_cast<const ProxyMTDTopology&>(thedet->topology());
    // const RectangularMTDTopology& topo = static_cast<const RectangularMTDTopology&>(topoproxy.specificTopology());

    // Local3DPoint local_point_sim(convertMmToCm(hit_pos.x()), convertMmToCm(hit_pos.y()), convertMmToCm(hit_pos.z()));
    // local_point_sim = topo.pixelToModuleLocalPoint(local_point_sim, detId.row(topo.nrows()), detId.column(topo.nrows()));
    // const auto& global_point_sim = thedet->toGlobal(local_point_sim);

    // // --- Get the times of the every SIM hit
    // simLocX.push_back(local_point_sim.x());
    // simLocY.push_back(local_point_sim.y());
    // simLocZ.push_back(local_point_sim.z());
    // simTheta.push_back(global_point_sim.perp());
    // simPhi.push_back(global_point_sim.phi());
    // simEta.push_back(global_point_sim.eta());
    // simZ.push_back(global_point_sim.z());
    // simT.push_back(simHit.tof());
    // simE.push_back(convertUnitsTo(0.001_MeV, simHit.energyLoss()));
    // simPDG.push_back(simHit.particleType());
    // simProcess.push_back(simHit.processType());
    // simType.push_back(simHit.offsetTrackId());
    // simID.push_back(id.rawId());
    // simEvent.push_back(iEvent.id().event());
    // simTrackId.push_back(simHit.originalTrackId());
  } // --- end of simHit loop

  // --- Loop over the simTracksters
  std::cout << "MtdSimTracksters" << std::endl;
  for (const auto& tp : *simTracksterHandle) {
    std::cout << tp << std::endl;
    simTracksterE.push_back(tp.energy());
    simTracksterT.push_back(tp.time());
    simTracksterTheta.push_back(tp.position().perp());
    simTracksterPhi.push_back(tp.position().phi());
    simTracksterEta.push_back(tp.position().eta());
    simTracksterX.push_back(tp.position().x());
    simTracksterY.push_back(tp.position().y());
    simTracksterZ.push_back(tp.position().z());
    simTracksterEvent.push_back(iEvent.id().event());
    simTracksterPDG.push_back(tp.pdgId());
    simTracksterTrackId.push_back(tp.particleId());
  } // --- end of simTrackster loop

  // --- Loop over the BTL RECO hits
  std::cout << "BTL Sim Hits" << std::endl;
  for (const auto& recHit : *btlRecHitsHandle) {
    LogTrace("BtlLocalRecoValidation") << "@RH detid " << recHit.id().rawId() << " r/c/X/dX " << recHit.row() << " "
                                       << recHit.column() << " " << recHit.position() << " " << recHit.positionError()
                                       << " E,T,dT " << recHit.energy() << " " << recHit.time() << " "
                                       << recHit.timeError();

    BTLDetId detId = recHit.id();
    DetId geoId = detId.geographicalId(MTDTopologyMode::crysLayoutFromTopoMode(topology->getMTDTopologyMode()));
    //std::cout << "Side, Tray, DM, SM, RU, crystal : " 
    //<< detId.mtdSide() << " " << detId.mtdRR() << " " << detId.dmodule() << " " << detId.smodule() << " " 
    //<< detId.runit() << " " << detId.row() << std::endl;
    const MTDGeomDet* thedet = geom->idToDet(geoId);
    const ProxyMTDTopology& topoproxy = static_cast<const ProxyMTDTopology&>(thedet->topology());
    const RectangularMTDTopology& topo = static_cast<const RectangularMTDTopology&>(topoproxy.specificTopology());

    Local3DPoint local_point(0., 0., 0.);
    local_point = topo.pixelToModuleLocalPoint(local_point, detId.row(topo.nrows()), detId.column(topo.nrows()));
    const auto& global_point = thedet->toGlobal(local_point);

    recoLocX.push_back(local_point.z());
    recoLocY.push_back(local_point.y());
    recoLocZ.push_back(local_point.z());
    recoTheta.push_back(global_point.perp());
    recoPhi.push_back(global_point.phi());
    recoEta.push_back(global_point.eta());
    recoX.push_back(global_point.x());
    recoY.push_back(global_point.y());
    recoZ.push_back(global_point.z());
    recoE.push_back(recHit.energy());
    recoT.push_back(recHit.time());
    recoID.push_back(recHit.id().rawId());
    recoEvent.push_back(iEvent.id().event());

    if (m_btlSimHits.count(detId.rawId()) == 1) {//&& m_btlSimHits[detId.rawId()].energy > hitMinEnergy_) {

      Local3DPoint local_point_sim(convertMmToCm(m_btlSimHits[detId.rawId()].x),
                                   convertMmToCm(m_btlSimHits[detId.rawId()].y),
                                   convertMmToCm(m_btlSimHits[detId.rawId()].z));
      local_point_sim = topo.pixelToModuleLocalPoint(local_point_sim, detId.row(topo.nrows()), detId.column(topo.nrows()));
      const auto& global_point_sim = thedet->toGlobal(local_point_sim);

      simLocX.push_back(local_point_sim.x());
      simLocY.push_back(local_point_sim.y());
      simLocZ.push_back(local_point_sim.z());
      simTheta.push_back(global_point_sim.perp());
      simPhi.push_back(global_point_sim.phi());
      simEta.push_back(global_point_sim.eta());
      simX.push_back(global_point_sim.x());
      simY.push_back(global_point_sim.y());
      simZ.push_back(global_point_sim.z());
      simT.push_back(m_btlSimHits[detId.rawId()].time);
      simE.push_back(m_btlSimHits[detId.rawId()].energy);
      simPDG.push_back(m_btlSimHits[detId.rawId()].pdgId);
      simProcess.push_back(m_btlSimHits[detId.rawId()].process);
      simType.push_back(m_btlSimHits[detId.rawId()].type);
      simID.push_back(detId.rawId());
      simEvent.push_back(iEvent.id().event());
      simTrackId.push_back(m_btlSimHits[detId.rawId()].trackId);

      std::cout << "SimHit ID, trackId: " << m_btlSimHits[detId.rawId()].pdgId << ", " << m_btlSimHits[detId.rawId()].trackId << std::endl;
      std::cout << "SimHit E, T: " << m_btlSimHits[detId.rawId()].energy << ", " << m_btlSimHits[detId.rawId()].time << std::endl;
      std::cout << std::endl;
    } // --- end of recHit loop
  }

  std::cout << "MtdSimLayerClusters" << std::endl;

  // --- Loop over the BTL RECO clusters
  for (const auto& DetSetClu : *btlRecCluHandle) {
    for (const auto& cluster : DetSetClu) {
      //if (cluster.energy() < hitMinEnergy_)
      //  continue;
      BTLDetId cluId = cluster.id();
      DetId detIdObject(cluId);
      const auto& genericDet = geom->idToDetUnit(detIdObject);

      // --- Use MtdSimLayerClusters as mtd truth
      edm::Ref<edmNew::DetSetVector<FTLCluster>, FTLCluster> clusterRef = edmNew::makeRefTo(btlRecCluHandle, &cluster);
      auto itp = r2sAssociationMap.equal_range(clusterRef);
      if (itp.first != itp.second) {
        std::vector<MtdSimLayerClusterRef> simClustersRefs = (*itp.first).second;  // the range of itp.first, itp.second should be always 1
        for (unsigned int i = 0; i < simClustersRefs.size(); i++) {
          auto simClusterRef = simClustersRefs[i];

          float simClusEnergy = convertUnitsTo(0.001_MeV, (*simClusterRef).simLCEnergy());  // GeV --> MeV
          float simClusTime = (*simClusterRef).simLCTime();
          LocalPoint simClusLocalPos = (*simClusterRef).simLCPos();
          const auto& simClusGlobalPos = genericDet->toGlobal(simClusLocalPos);

          std::cout << "CP momentum, ID, pos : " << (*simClusterRef).p4() << " " << (*simClusterRef).pdgId() << std::endl;
          std::cout << "Accumulated E, pos: " << simClusEnergy << " " << simClusGlobalPos << std::endl;

          //for (auto simclusterhit : (*simClusterRef).hits_and_positions()) {
            //uint64_t hit_ID = simclusterhit.first;
            //caloID.push_back(hit_ID>>32);
            //caloX.push_back(simclusterhit.second.x());
            //caloY.push_back(simclusterhit.second.y());
            //caloZ.push_back(simclusterhit.second.z());
          //}
          // First SimTrack only
          //caloPDG.push_back((*simClusterRef).pdgId());
          //caloTheta.push_back((*simClusterRef).theta());
          //caloPhi.push_back((*simClusterRef).phi());
          //caloEta.push_back((*simClusterRef).eta());
          //caloE.push_back((*simClusterRef).energy());
          //caloT.push_back((*simClusterRef).time());

          // Accumulated for the cluster
          simLayerClusterPDG.push_back((*simClusterRef).pdgId());
          simLayerClusterSeed.push_back((*simClusterRef).seedId());
          simLayerClusterTheta.push_back(simClusGlobalPos.theta());
          simLayerClusterPhi.push_back(simClusGlobalPos.phi());
          simLayerClusterEta.push_back(simClusGlobalPos.eta());
          simLayerClusterZ.push_back(simClusGlobalPos.z());
          simLayerClusterLocX.push_back(simClusLocalPos.x());
          simLayerClusterLocY.push_back(simClusLocalPos.y());
          simLayerClusterLocZ.push_back(simClusLocalPos.z());
          simLayerClusterE.push_back(simClusEnergy);
          simLayerClusterT.push_back(simClusTime);
          simLayerClusterEvent.push_back(iEvent.id().event());
        }
      } // --- end of MtdSimLayerClusters
    }
  } // --- end of BTL RECO clusters
} // --- end of analyze

// ------------ method for histogram booking ------------
void BtlClustering::bookHistograms(DQMStore::IBooker& ibook, edm::Run const& run, edm::EventSetup const& iSetup) {
  ibook.setCurrentFolder(folder_);

  // Create a file with jobId
  std::stringstream ss;
  ss << "tree_" << jobId_ << ".root";
  std::string filename = ss.str();
  myFile = new TFile(filename.c_str(), "RECREATE");
  
  simTree = new TTree("simTree", "A tree with simulation hit information");
  simTree->Branch("pdg",      &simPDG);
  simTree->Branch("time",     &simT);
  simTree->Branch("energy",   &simE);
  simTree->Branch("local_x",  &simLocX);
  simTree->Branch("local_y",  &simLocY);
  simTree->Branch("local_z",  &simLocZ);
  simTree->Branch("theta",    &simTheta);
  simTree->Branch("phi",      &simPhi);
  simTree->Branch("eta",      &simEta);
  simTree->Branch("x",        &simX);
  simTree->Branch("y",        &simY);
  simTree->Branch("z",        &simZ);
  simTree->Branch("ID",       &simID);
  simTree->Branch("event",    &simEvent);
  simTree->Branch("process",  &simProcess);
  simTree->Branch("type",     &simType);
  simTree->Branch("trackId",  &simTrackId);

  recoTree = new TTree("recoTree", "A tree with reconstructed hit information");
  recoTree->Branch("time",    &recoT);
  recoTree->Branch("energy",  &recoE);
  recoTree->Branch("local_x", &recoLocX);
  recoTree->Branch("local_y", &recoLocY);
  recoTree->Branch("local_z", &recoLocZ);
  recoTree->Branch("theta",   &recoTheta);
  recoTree->Branch("phi",     &recoPhi);
  recoTree->Branch("eta",     &recoEta);
  recoTree->Branch("x",       &recoX);
  recoTree->Branch("y",       &recoY);
  recoTree->Branch("z",       &recoZ);
  recoTree->Branch("ID",      &recoID);
  recoTree->Branch("event",   &recoEvent);

  caloTree = new TTree("caloTree", "A tree with calo hit information");
  caloTree->Branch("time",    &caloT);
  caloTree->Branch("energy",  &caloE);
  caloTree->Branch("theta",   &caloTheta);
  caloTree->Branch("phi",     &caloPhi);
  caloTree->Branch("eta",     &caloEta);
  caloTree->Branch("x",       &caloX);
  caloTree->Branch("y",       &caloY);
  caloTree->Branch("z",       &caloZ);
  caloTree->Branch("pdg",     &caloPDG);
  caloTree->Branch("ID",      &caloID);
  caloTree->Branch("event",   &caloEvent);
  caloTree->Branch("subEvent",&caloSubEvent);
  caloTree->Branch("trackId", &caloTrackId);
  caloTree->Branch("decay",   &caloDecay);
  caloTree->Branch("parentX", &caloSourceX);
  caloTree->Branch("parentY", &caloSourceY);
  caloTree->Branch("parentZ", &caloSourceZ);
  caloTree->Branch("parentT", &caloSourceT);
  
  /*
  caloTree = new TTree("caloTree", "A tree with TrackingParticle information");
  caloTree->Branch("eventId", &currentEventId);
  // G4Track info (vector of vector of vectors)
  caloTree->Branch("tp_pdg", &tp_pdg);
  caloTree->Branch("tp_pt", &tp_pt);
  caloTree->Branch("tp_trackId", &tp_trackId);
  // Parent vertex (vector of vectors)
  caloTree->Branch("tp_sourceX", &tp_sourceX);
  caloTree->Branch("tp_sourceY", &tp_sourceY);
  caloTree->Branch("tp_sourceZ", &tp_sourceZ);
  caloTree->Branch("tp_sourceT", &tp_sourceT);
  // Decay vertices (vector of vector of vectors)
  caloTree->Branch("tp_decayX", &tp_decayX);
  caloTree->Branch("tp_decayY", &tp_decayY);
  caloTree->Branch("tp_decayZ", &tp_decayZ);
  caloTree->Branch("tp_decayT", &tp_decayT);
  */

  simLayerClusterTree = new TTree("simLayerClusterTree", "A tree with SimLayerCluster information");
  simLayerClusterTree->Branch("pdg",    &simLayerClusterPDG);
  simLayerClusterTree->Branch("energy", &simLayerClusterE);
  simLayerClusterTree->Branch("time",   &simLayerClusterT);
  simLayerClusterTree->Branch("seed",   &simLayerClusterSeed);
  simLayerClusterTree->Branch("theta",  &simLayerClusterTheta);
  simLayerClusterTree->Branch("phi",    &simLayerClusterPhi);
  simLayerClusterTree->Branch("eta",    &simLayerClusterEta);
  simLayerClusterTree->Branch("z",      &simLayerClusterZ);
  simLayerClusterTree->Branch("local_x",&simLayerClusterLocX);
  simLayerClusterTree->Branch("local_y",&simLayerClusterLocY);
  simLayerClusterTree->Branch("local_z",&simLayerClusterLocZ);
  simLayerClusterTree->Branch("event",  &simLayerClusterEvent);

  simTracksterTree = new TTree("simTracksterTree", "A tree with SimTrackster information");
  simTracksterTree->Branch("pdg",     &simTracksterPDG);
  simTracksterTree->Branch("energy",  &simTracksterE);
  simTracksterTree->Branch("time",    &simTracksterT);
  simTracksterTree->Branch("local_x", &simTracksterLocX);
  simTracksterTree->Branch("local_y", &simTracksterLocY);
  simTracksterTree->Branch("local_z", &simTracksterLocZ);
  simTracksterTree->Branch("theta",   &simTracksterTheta);
  simTracksterTree->Branch("phi",     &simTracksterPhi);
  simTracksterTree->Branch("eta",     &simTracksterEta);
  simTracksterTree->Branch("x",       &simTracksterX);
  simTracksterTree->Branch("y",       &simTracksterY);
  simTracksterTree->Branch("z",       &simTracksterZ);
  simTracksterTree->Branch("event",   &simTracksterEvent);
  simTracksterTree->Branch("trackId", &simTracksterTrackId);
}

// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void BtlClustering::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;

  desc.add<std::string>("folder", "MTD/BTL/LocalReco");
  desc.add<std::string>("jobId", "NOJOBID");
  desc.add<edm::InputTag>("MtdCaloParticles", edm::InputTag("mix", "MergedMtdTruth"));
  desc.add<edm::InputTag>("MtdSimTracksters", edm::InputTag("mix", "MergedMtdTruthST"));
  desc.add<edm::InputTag>("SimTag", edm::InputTag("mix", "MergedTrackTruth"));
  // desc.add<edm::InputTag>("simVertexCollection", edm::InputTag("mix", "simVertexCollection"));
  desc.add<edm::InputTag>("recHitsTag", edm::InputTag("mtdRecHits", "FTLBarrel"));
  desc.add<edm::InputTag>("simHitsTag", edm::InputTag("mix", "g4SimHitsFastTimerHitsBarrel"));
  desc.add<edm::InputTag>("recCluTag", edm::InputTag("mtdClusters", "FTLBarrel"));
  desc.add<edm::InputTag>("trkHitTag", edm::InputTag("mtdTrackingRecHits"));
  desc.add<edm::InputTag>("r2sAssociationMapTag", edm::InputTag("mtdRecoClusterToSimLayerClusterAssociation"));
  desc.add<double>("HitMinimumEnergy", 1.);  // [MeV]

  descriptions.add("btlClustering", desc);
}

DEFINE_FWK_MODULE(BtlClustering);
