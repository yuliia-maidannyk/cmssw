#ifndef MyAnalysis_PhotonGammaFilter_h
#define MyAnalysis_PhotonGammaFilter_h

#include "FWCore/Framework/interface/global/EDFilter.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "SimDataFormats/TrackingHit/interface/PSimHit.h"
#include "SimDataFormats/Vertex/interface/SimVertex.h"
#include "SimDataFormats/Track/interface/SimTrack.h"
#include <cmath>
#include <iomanip>
#include <atomic>

class PhotonGammaFilter : public edm::global::EDFilter<> {
public:
  explicit PhotonGammaFilter(const edm::ParameterSet&);
  ~PhotonGammaFilter() override;

  static void fillDescriptions(edm::ConfigurationDescriptions&);
  void fillMcTruth(std::vector<SimTrack> &simTracks, std::vector<SimVertex> &simVertices) const;
  bool filter(edm::StreamID, edm::Event&, const edm::EventSetup&) const override;

private:
  edm::EDGetTokenT<std::vector<SimTrack>> simTrackToken_;
  edm::EDGetTokenT<std::vector<SimVertex>> simVertexToken_;
  double minPt_;
  double maxEta_;
  int    motherPdgId_;
  mutable std::atomic<unsigned long> nTotal_{0};
  mutable std::atomic<unsigned long> nPassed_{0};
  mutable std::atomic<unsigned long> nFailPt_{0};
  mutable std::atomic<unsigned long> nFailEta_{0};
  mutable std::map<unsigned, unsigned> geantToIndex_;
};

#endif
