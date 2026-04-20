#include "MyAnalysis/PhotonGammaFilter/interface/PhotonGammaFilter.h"
#include "SimDataFormats/Track/interface/SimTrackContainer.h"
#include "SimDataFormats/Vertex/interface/SimVertexContainer.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

PhotonGammaFilter::PhotonGammaFilter(const edm::ParameterSet& iConfig)
    : simTrackToken_ (consumes<std::vector<SimTrack>>(
          iConfig.getParameter<edm::InputTag>("simTrackProduct"))),
      simVertexToken_(consumes<std::vector<SimVertex>>(
          iConfig.getParameter<edm::InputTag>("simVertexProduct"))),
      minPt_      (iConfig.getParameter<double>("minPt")),
      maxEta_     (iConfig.getParameter<double>("maxEta")),
      motherPdgId_(iConfig.getParameter<int>("motherPdgId")) {}

PhotonGammaFilter::~PhotonGammaFilter() {
  unsigned long nTotal = nTotal_.load();
  unsigned long nPassed = nPassed_.load();
  unsigned long nFailPt = nFailPt_.load();
  unsigned long nFailEta = nFailEta_.load();
  
  double eff    = (nTotal > 0) ? 100.0 * nPassed / nTotal : 0.0;
  double effErr = (nTotal > 0)
                ? 100.0 * std::sqrt(double(nPassed) * (nTotal - nPassed) / nTotal) / nTotal
                : 0.0;

  edm::LogPrint("PhotonGammaFilter")
    << "\n"
    << "========================================\n"
    << "  PhotonGammaFilter Efficiency Report\n"
    << "========================================\n"
    << "  Cuts applied:\n"
    << "    motherPdgId  = " << motherPdgId_ << "\n"
    << "    minPt        = " << minPt_       << " GeV/c\n"
    << "    maxEta       = " << maxEta_      << "\n"
    << "----------------------------------------\n"
    << "  Events examined : " << nTotal       << "\n"
    << "  Events passed   : " << nPassed      << "\n"
    << "  Events failed   : " << (nTotal - nPassed) << "\n"
    << "  Efficiency      : " << std::fixed << std::setprecision(2)
                              << eff << " +/- " << effErr << " %\n"
    << "----------------------------------------\n"
    << "  Failure breakdown:\n"
    << "    Failed pT    : " << nFailPt      << "\n"
    << "    Failed eta   : " << nFailEta     << "\n"
    << "========================================\n";
}

void PhotonGammaFilter::fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices) const {
  geantToIndex_.clear();
  unsigned nTks = simTracks.size();
  if (simVertices.empty()) return;
  
  // Create a map associating geant particle id and position in the SimTrack vector
  for (unsigned it = 0; it < nTks; ++it) {
    geantToIndex_[simTracks[it].trackId()] = it;
  }
}

bool PhotonGammaFilter::filter(edm::StreamID, edm::Event& iEvent, const edm::EventSetup& iSetup) const {

  ++nTotal_;

  edm::Handle<edm::SimTrackContainer> simTracks;
  iEvent.getByToken(simTrackToken_, simTracks);

  edm::Handle<edm::SimVertexContainer> simVertexes;
  iEvent.getByToken(simVertexToken_, simVertexes);

  std::vector<SimTrack> theSimTracks;
  std::vector<SimVertex> theSimVertices;
  theSimTracks.insert(theSimTracks.end(), simTracks->begin(), simTracks->end());
  theSimVertices.insert(theSimVertices.end(), simVertexes->begin(), simVertexes->end());

  fillMcTruth(theSimTracks, theSimVertices);

  // Find photons from pion decay vertex
  std::vector<SimTrack*> pionTracks;
  std::vector<SimTrack*> photonTracks;
  for (auto& simTk : theSimTracks) {
    if (simTk.noVertex()) continue;
    if (simTk.type() == 9000001) { // pi0 from primary vertex
      pionTracks.push_back(&simTk);
      std::cout << "Found SimTrack pion: trackId=" << simTk.trackId() << "(pt=" << simTk.momentum().pt()
                << ", eta=" << simTk.momentum().eta() << ", phi=" << simTk.momentum().phi() << ")"
                << " E=" << simTk.momentum().E() << std::endl;
    }
  }

  // For each primary pi0, look for conversion photons
  std::map<unsigned, std::vector<unsigned>> pionAssociation; // photon trackId <-> pion trackId

  for (auto* pionTk : pionTracks) {
    int firstVertexId = -1;
    float minR = 999.;
    
    for (auto& simTk : theSimTracks) {
      if (simTk.noVertex()) continue;
      //if (simTk.vertIndex() == iPV) continue;
      if (simTk.type() != 22) continue; 

      int vertexId = simTk.vertIndex();
      SimVertex vertex = theSimVertices[vertexId];
      
      if (!vertex.parentIndex()) continue;
      unsigned motherGeantId = vertex.parentIndex();
      auto association = geantToIndex_.find(motherGeantId);
      if (association == geantToIndex_.end()) continue;
      int motherId = association->second;

      // Find the earliest pi0 decay vertex
      if (theSimTracks[motherId].trackId() != pionTk->trackId()) continue;
      float r = vertex.position().pt();
      if (r < minR) {
        minR = r;
        firstVertexId = vertexId;
      }
    }

    if (firstVertexId >= 0) {

      for (auto& simTk : theSimTracks) {
        if (simTk.noVertex()) continue;
        if (simTk.vertIndex() != firstVertexId) continue;
        if (simTk.type() != 22) continue;

        photonTracks.push_back(&simTk);
        pionAssociation[pionTk->trackId()].push_back(simTk.trackId());
        std::cout << "  Found photon from pi0 decay: trackId=" << simTk.trackId() 
                  << " eta=" << simTk.momentum().eta() << " phi=" << simTk.momentum().phi() 
                  << " E=" << simTk.momentum().E() << std::endl;
      }
    }
  }

  // Loop over pion tracks and find associated photons, then check if they pass cuts
  bool both_pass = true;
  for (auto* pionTk : pionTracks) {
    unsigned bestTrackId = pionTk->trackId();
    auto it = pionAssociation.find(bestTrackId);
    if (it == pionAssociation.end()) continue;

    for (unsigned photonTrackId : it->second) {
      auto assoc = geantToIndex_.find(photonTrackId);
      const auto& pho = theSimTracks[assoc->second];

      float pho_pt = pho.momentum().pt();
      float pho_eta = pho.momentum().eta();
      if (pho_pt < minPt_) {
        both_pass = false;
        ++nFailPt_;
        break;
      }
      if (std::abs(pho_eta) > maxEta_) {
        both_pass = false;
        ++nFailEta_;
        break;
      }
    }
  }
  if (both_pass) {
    ++nPassed_;
    return true;
  }
  return false;
}

void PhotonGammaFilter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("simTrackProduct",  edm::InputTag("g4SimHits"));
  desc.add<edm::InputTag>("simVertexProduct", edm::InputTag("g4SimHits"));
  desc.add<double>("minPt",       5.0);
  desc.add<double>("maxEta",      2.5);
  desc.add<int>   ("motherPdgId", 0);   // 0 = no mother requirement
  descriptions.addDefault(desc);
}

DEFINE_FWK_MODULE(PhotonGammaFilter);
