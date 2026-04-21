#include "FWCore/Framework/interface/stream/EDProducer.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "DataFormats/ParticleFlowReco/interface/PFCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFClusterFwd.h"

class PFClusterECALEndcapFilter : public edm::stream::EDProducer<> {
public:
  explicit PFClusterECALEndcapFilter(const edm::ParameterSet& iConfig)
    : token_(consumes<reco::PFClusterCollection>(iConfig.getParameter<edm::InputTag>("src")))
  {
    produces<reco::PFClusterCollection>();
  }

  void produce(edm::Event& iEvent, const edm::EventSetup&) override {
    auto const& input = iEvent.get(token_);
    auto output = std::make_unique<reco::PFClusterCollection>();
    for (auto const& cluster : input) {
      if (cluster.layer() == PFLayer::ECAL_ENDCAP)
        output->push_back(cluster);
    }
    iEvent.put(std::move(output));
  }

private:
  edm::EDGetTokenT<reco::PFClusterCollection> token_;
};

DEFINE_FWK_MODULE(PFClusterECALEndcapFilter);