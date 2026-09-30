#include "DQMServices/Core/interface/DQMStore.h"
#include "FWCore/Utilities/interface/Exception.h"
#include <DataFormats/EcalDetId/interface/EBDetId.h>
#include "Calibration/IsolatedParticles/interface/DetIdFromEtaPhi.h"

#include <fstream>

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
#include "CommonTools/UtilAlgos/interface/TFileService.h"
#include "FWCore/ServiceRegistry/interface/Service.h"

#include "FWCore/Framework/interface/one/EDAnalyzer.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"

#include <map>
#include <vector>
#include <TFile.h>
#include <TTree.h>
#include <string>

class NoiseAnalyser : public DQMOneEDAnalyzer<> {

public:
  typedef dqm::legacy::DQMStore DQMStore;

  explicit NoiseAnalyser(const edm::ParameterSet &);

  ~NoiseAnalyser() override;

protected: 
  void bookHistograms(DQMStore::IBooker&, edm::Run const&, edm::EventSetup const&) override;
  void beginJob() override;
  // void endJob() override;
  void analyze(const edm::Event&, const edm::EventSetup&) override;
  void clearEventData();

private:

  std::string jobId;

  edm::EDGetTokenT<EBRecHitCollection> EBRecHitNoZSToken;

  TTree* noiseTree;

  std::vector<int>      noiseEvent;
  std::vector<int>      noiseIEta;
  std::vector<int>      noiseIPhi;
  std::vector<float>    noiseValues;
};

// ------------ constructor and destructor --------------
NoiseAnalyser::NoiseAnalyser(const edm::ParameterSet& iConfig)
  : jobId(iConfig.getParameter<std::string>("jobId"))
{
  EBRecHitNoZSToken = consumes<EBRecHitCollection>(iConfig.getParameter<edm::InputTag>("EBRecHitNoZSCollection"));
}

NoiseAnalyser::~NoiseAnalyser() {}

// ------------ method called for each event  ------------
void NoiseAnalyser::analyze(const edm::Event& iEvent, const edm::EventSetup& iSetup) {
  using namespace edm;
  using namespace std;

  clearEventData();

  // ***************** Get the collections *****************

  const EBRecHitCollection *EBRecHitNoZS = nullptr;
  edm::Handle<EBRecHitCollection> EBRecHitNoZSHandle;
  iEvent.getByToken(EBRecHitNoZSToken, EBRecHitNoZSHandle);
  if (EBRecHitNoZSHandle.isValid()) {
      EBRecHitNoZS = EBRecHitNoZSHandle.product();
  }

  // **************** Loop over the noise hits ****************

  std::map<std::pair<int,int>, float> noiseCellTotal; // key: (ieta, iphi) -> total energy

  for (EcalRecHitCollection::const_iterator recHit = EBRecHitNoZS->begin(); recHit != EBRecHitNoZS->end(); ++recHit) {
    EBDetId ebid = EBDetId(recHit->id());
    int ieta = ebid.ieta();
    int iphi = ebid.iphi();
    auto cell = std::make_pair(ebid.ieta(), ebid.iphi());
    noiseCellTotal[cell] += recHit->energy();
  }
  for (const auto& [key, ei] : noiseCellTotal) {
    noiseIEta.push_back(key.first);
    noiseIPhi.push_back(key.second);
    noiseValues.push_back(ei);
    noiseEvent.push_back(iEvent.id().event());
  }

  noiseTree->Fill();
} // --- end of analyze

void NoiseAnalyser::clearEventData() {
  noiseEvent.clear(); noiseIEta.clear(); noiseIPhi.clear(); noiseValues.clear();
}

void NoiseAnalyser::bookHistograms(DQMStore::IBooker& ib, edm::Run const&, edm::EventSetup const& iSetup) {}

void NoiseAnalyser::beginJob() {

  edm::Service<TFileService> fs;

  noiseTree = fs->make<TTree>("noiseTree", "A tree with reconstructed hit information from noise");
  noiseTree->Branch("event",  &noiseEvent);
  noiseTree->Branch("ieta",   &noiseIEta);
  noiseTree->Branch("iphi",   &noiseIPhi);
  noiseTree->Branch("energy", &noiseValues);
}

DEFINE_FWK_MODULE(NoiseAnalyser);