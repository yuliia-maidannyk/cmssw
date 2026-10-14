// =============================================================================
//  SonicProducer
//
//  Same physics as MLPFClusterProducer, but the neural network runs on a
//  Triton inference server (on a GPU) instead of inside the CMSSW process.
//
//  The work is split into two steps:
//
//    acquire()  : read rec hits, build the four model inputs, send them.
//                 (CMSSW does other work while the GPU is busy.)
//
//    produce()  : receive the three model outputs, turn them into
//                 PFRecHits and PFClusters, add the endcap clusters,
//                 and store everything in the event.
//
//  Anything produce() needs from acquire() is kept in member variables
//  (see "Memory passed from acquire() to produce()" below).
//  This is safe: each CMSSW stream has its own copy of this module.
// =============================================================================

#include "HeterogeneousCore/SonicTriton/interface/TritonEDProducer.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/EventSetup.h"
#include "FWCore/Framework/interface/ESWatcher.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ParameterSet/interface/ParameterSetDescription.h"
#include "FWCore/ParameterSet/interface/ConfigurationDescriptions.h"

#include "CondFormats/DataRecord/interface/EcalChannelStatusRcd.h"
#include "CondFormats/EcalObjects/interface/EcalChannelStatus.h"
#include "DataFormats/EcalDetId/interface/EBDetId.h"
#include "DataFormats/EcalRecHit/interface/EcalRecHitCollections.h"
#include "DataFormats/ParticleFlowReco/interface/PFCluster.h"
#include "DataFormats/ParticleFlowReco/interface/PFClusterFwd.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHit.h"
#include "DataFormats/ParticleFlowReco/interface/PFRecHitFwd.h"
#include "Geometry/CaloGeometry/interface/CaloCellGeometryMayOwnPtr.h"
#include "Geometry/CaloGeometry/interface/CaloGeometry.h"
#include "Geometry/CaloGeometry/interface/CaloSubdetectorGeometry.h"
#include "Geometry/CaloGeometry/interface/TruncatedPyramid.h"
#include "Geometry/Records/interface/CaloGeometryRecord.h"
#include "RecoParticleFlow/PFClusterProducer/interface/MLPFClusterPreProcessing.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace {

  // ---------------------------------------------------------------------------
  //  The barrel is treated as an image:
  //    rows    = iphi          (0 .. 360)
  //    columns = ieta + 85     (0 .. 170)
  // ---------------------------------------------------------------------------
  const int kRows = 361;
  const int kColumns = 171;

  int cellNumber(int row, int column) { return row * kColumns + column; }

  const std::string kInputEnergies = "inp1";     // seed windows          (maxClusters, crop, crop)
  const std::string kInputRelativePos = "inp2";  // relative positions    (maxClusters, 2)
  const std::string kInputAbsolutePos = "inp3";  // absolute positions    (maxClusters)
  const std::string kInputDeadMasks = "inp4";    // dead-channel masks    (maxClusters, crop, crop)

  const std::string kOutputCentres = "center";   // (maxClusters, 2)
  const std::string kOutputEnergies = "energy";  // (maxClusters, 1)
  const std::string kOutputSeeds = "seed";       // (maxClusters, 1)

  // Add up rec-hit energies into the barrel image.
  std::vector<float> buildEnergyMap(const std::vector<int>& ietaList,
                                    const std::vector<int>& iphiList,
                                    const std::vector<float>& energyList) {
    std::vector<float> energyMap(kRows * kColumns, 0.f);
    for (size_t i = 0; i < ietaList.size(); ++i) {
      int column = ietaList[i] + 85;
      int row = iphiList[i];
      if (row < 0 || row >= kRows)
        continue;
      if (column < 0 || column >= kColumns)
        continue;
      energyMap[cellNumber(row, column)] += energyList[i];
    }
    return energyMap;
  }

  // Copy an endcap cluster, keeping only kinematics
  // (deliberately NOT copying PFRecHitFractions, to avoid InvalidReference).
  reco::PFCluster copyEndcapCluster(const reco::PFCluster& original) {
    reco::PFCluster copy;
    copy.setLayer(PFLayer::ECAL_ENDCAP);
    copy.setEnergy(original.energy());
    copy.setCorrectedEnergy(original.correctedEnergy());
    copy.setPosition(original.position());
    copy.setTime(original.time());
    copy.calculatePositionREP();  // needed by CalibratedPFCluster::eta/phi
    return copy;
  }

}  // namespace

// =============================================================================
class SonicProducer : public TritonEDProducer<> {
public:
  explicit SonicProducer(const edm::ParameterSet& config);

  void acquire(edm::Event const& event, edm::EventSetup const& setup, Input& modelInputs) override;
  void produce(edm::Event& event, edm::EventSetup const& setup, Output const& modelOutputs) override;

  static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

private:
  void updateDeadChannelGrid(edm::EventSetup const& setup);
  math::XYZPoint barrelPosition(
      const CaloSubdetectorGeometry* ebGeom, float ietaFloat, float iphiFloat, int ieta, int iphi) const;

  // ---- Settings from the python config ----
  const int maskedEcalChannelStatusThreshold_;
  const int cropSize_;
  const int maxClusters_;
  const int overlapLimit_;
  const double seedThreshold_;
  const double outputThreshold_;
  const double T0_EB_;  // kept for a future shower-depth correction
  const double X0_;     // kept for a future shower-depth correction

  // ---- What we read from the event / conditions ----
  edm::EDGetTokenT<EBRecHitCollection> ebRecHitsToken_;
  edm::EDGetTokenT<reco::PFClusterCollection> eeClusterToken_;
  edm::EDGetTokenT<reco::PFCluster::EEtoPSAssociation> eeToPSToken_;
  edm::ESGetToken<CaloGeometry, CaloGeometryRecord> caloGeomToken_;
  edm::ESGetToken<EcalChannelStatus, EcalChannelStatusRcd> ecalStatusToken_;

  // ---- Dead-channel grid, rebuilt only when the channel status changes ----
  //  1 = usable, 2 = noisy / wrong gain, 3 = completely dead
  edm::ESWatcher<EcalChannelStatusRcd> ecalChannelStatusWatcher_;
  std::vector<int> deadChannelGrid_;

  // ---- Memory passed from acquire() to produce() ----
  int numGroups_ = 0;  // how many groups of seeds were sent (the batch size)
  // For each group, the (row, column) of each seed; (-1, -1) means "empty slot".
  std::vector<std::vector<Eigen::MatrixXf>> seedCentres_;
};

// =============================================================================
SonicProducer::SonicProducer(const edm::ParameterSet& config)
    : TritonEDProducer<>(config),
      maskedEcalChannelStatusThreshold_(config.getParameter<int>("maskedEcalChannelStatusThreshold")),
      cropSize_(config.getParameter<int>("cropSize")),
      maxClusters_(config.getParameter<int>("maxClusters")),
      overlapLimit_(config.getParameter<int>("overlapLimit")),
      seedThreshold_(config.getParameter<double>("seedThreshold")),
      outputThreshold_(config.getParameter<double>("outputThreshold")),
      T0_EB_(config.getParameter<double>("T0_EB")),
      X0_(config.getParameter<double>("X0")) {
  ebRecHitsToken_ = consumes<EBRecHitCollection>(config.getParameter<edm::InputTag>("EBrechitCollection"));
  eeClusterToken_ = consumes<reco::PFClusterCollection>(config.getParameter<edm::InputTag>("inputEE"));
  eeToPSToken_ =
      consumes<reco::PFCluster::EEtoPSAssociation>(config.getParameter<edm::InputTag>("inputEEtoPSAssoc"));
  caloGeomToken_ = esConsumes<CaloGeometry, CaloGeometryRecord>();
  ecalStatusToken_ = esConsumes<EcalChannelStatus, EcalChannelStatusRcd>();

  produces<reco::PFClusterCollection>();
  produces<reco::PFCluster::EEtoPSAssociation>();
  produces<reco::PFRecHitCollection>("mlPFRecHitsEB");
}

// =============================================================================
//  STEP 1: build the model inputs and send them to the server
// =============================================================================
void SonicProducer::acquire(edm::Event const& event, edm::EventSetup const& setup, Input& modelInputs) {
  updateDeadChannelGrid(setup);

  // ---- 1a. Read barrel rec hits ----
  std::vector<int> ietaList;
  std::vector<int> iphiList;
  std::vector<float> energyList;

  auto recHits = event.getHandle(ebRecHitsToken_);
  if (recHits.isValid()) {
    for (const auto& hit : *recHits) {
      EBDetId id(hit.id());
      ietaList.push_back(id.ieta());
      iphiList.push_back(id.iphi());
      energyList.push_back(hit.energy());
    }
  }

  // ---- 1b. Make the energy image and blank out completely dead crystals ----
  std::vector<float> energyMap = buildEnergyMap(ietaList, iphiList, energyList);
  for (size_t cell = 0; cell < energyMap.size(); ++cell) {
    if (deadChannelGrid_[cell] == 3)
      energyMap[cell] = 0.0f;
  }

  // ---- 1c. Find seeds and cut out small windows around them ----
  auto samples =
      get_model_samples(energyMap, kRows, kColumns, seedThreshold_, cropSize_, overlapLimit_, maxClusters_);
  // samples.X       : energy crops,  shape (numGroups, maxClusters, crop, crop)
  // samples.indices : seed centres,  shape (numGroups, maxClusters, 2)

  numGroups_ = samples.X.size();
  seedCentres_ = std::move(samples.indices);

  // Tell Triton how many groups we are sending in this event.
  // With 0 groups nothing is sent, and produce() is still called.
  client_->setBatchSize(numGroups_);
  if (numGroups_ == 0)
    return;

  // ---- 1d. Get empty containers for the four inputs ----
  auto& energiesInput = modelInputs.at(kInputEnergies);
  auto& relativePosInput = modelInputs.at(kInputRelativePos);
  auto& absolutePosInput = modelInputs.at(kInputAbsolutePos);
  auto& deadMasksInput = modelInputs.at(kInputDeadMasks);

  auto energiesData = energiesInput.allocate<float>();
  auto relativePosData = relativePosInput.allocate<float>();
  auto absolutePosData = absolutePosInput.allocate<float>();
  auto deadMasksData = deadMasksInput.allocate<float>();

  const int halfCrop = cropSize_ / 2;
  const int effectiveWindow = (cropSize_ + overlapLimit_) - 1;

  // ---- 1e. Fill the inputs, one group at a time ----
  // Numbers are appended in the order [seed k][row r][column c],
  // which is the same layout the ONNX version used.
  for (int n = 0; n < numGroups_; ++n) {
    auto& energies = (*energiesData)[n];
    auto& relativePos = (*relativePosData)[n];
    auto& absolutePos = (*absolutePosData)[n];
    auto& deadMasks = (*deadMasksData)[n];

    const auto& centres = seedCentres_[n];
    // The first seed in each group has the highest energy; the others are measured relative to it.
    const int firstRow = static_cast<int>(centres[0](0, 0));
    const int firstColumn = static_cast<int>(centres[0](1, 0));

    for (int k = 0; k < maxClusters_; ++k) {
      const int row = static_cast<int>(centres[k](0, 0));
      const int column = static_cast<int>(centres[k](1, 0));
      const bool isEmptySlot = (row == -1 && column == -1);

      // inp1: energy crop
      for (int r = 0; r < cropSize_; ++r)
        for (int c = 0; c < cropSize_; ++c)
          energies.push_back(samples.X[n][k](r, c));

      // inp2: position relative to the first seed (-1 if missing)
      relativePos.push_back(row > -1 ? float(row - firstRow) / float(effectiveWindow) : -1.0f);
      relativePos.push_back(column > -1 ? float(column - firstColumn) / float(effectiveWindow) : -1.0f);

      // inp3: absolute cell number in the image (0 for empty slots)
      float cellValue = static_cast<float>(centres[k](1, 0)) + 171.0f * static_cast<float>(centres[k](0, 0));
      if (cellValue == -172.0f)
        cellValue = 0.0f;
      absolutePos.push_back(cellValue);

      // inp4: dead-channel mask around the seed
      for (int dr = -halfCrop; dr <= halfCrop; ++dr) {
        const int maskRow = (row + dr - 1 + (kRows - 1)) % (kRows - 1) + 1;  // iphi wraps: 360 is next to 1
        for (int dc = -halfCrop; dc <= halfCrop; ++dc) {
          const int maskColumn = column + dc;
          int value;
          if (isEmptySlot)
            value = -1;
          else if (maskColumn < 0 || maskColumn >= kColumns)
            value = -1;  // outside the barrel in ieta
          else
            value = deadChannelGrid_[cellNumber(maskRow, maskColumn)];
          deadMasks.push_back(static_cast<float>(value));
        }
      }
    }
  }

  // ---- 1f. Send everything ----
  energiesInput.toServer(energiesData);
  relativePosInput.toServer(relativePosData);
  absolutePosInput.toServer(absolutePosData);
  deadMasksInput.toServer(deadMasksData);
}

// =============================================================================
//  STEP 2: read the model outputs and build the clusters
// =============================================================================
void SonicProducer::produce(edm::Event& event, edm::EventSetup const& setup, Output const& modelOutputs) {
  const auto& eeClusters = event.get(eeClusterToken_);
  const auto& eeToPS = event.get(eeToPSToken_);

  auto clusters = std::make_unique<reco::PFClusterCollection>();
  auto pfRecHits = std::make_unique<reco::PFRecHitCollection>();

  // ---- Nothing was found in the barrel: store endcap clusters only ----
  if (numGroups_ == 0) {
    event.put(std::move(pfRecHits), "mlPFRecHitsEB");
    for (const auto& c : eeClusters)
      if (c.layer() == PFLayer::ECAL_ENDCAP)
        clusters->push_back(copyEndcapCluster(c));
    event.put(std::move(clusters));
    event.put(std::make_unique<reco::PFCluster::EEtoPSAssociation>(eeToPS));
    return;
  }

  // ---- 2a. Get the model outputs ----
  // Each is a list with one entry per group; each entry is a flat list of numbers.
  const auto& centreOffsets = modelOutputs.at(kOutputCentres).fromServer<float>();   // 2 numbers per seed
  const auto& energyOutputs = modelOutputs.at(kOutputEnergies).fromServer<float>();  // 1 number per seed
  const auto& seedScores = modelOutputs.at(kOutputSeeds).fromServer<float>();        // 1 number per seed

  const auto* ebGeom = setup.getData(caloGeomToken_).getSubdetectorGeometry(DetId::Ecal, EcalBarrel);

  // ---- 2b. Keep predictions with a high enough seed score ----
  struct BarrelCluster {
    float energy;
    math::XYZPoint position;
    EBDetId crystal;
  };
  std::vector<BarrelCluster> barrelClusters;

  for (int n = 0; n < numGroups_; ++n) {
    for (int k = 0; k < maxClusters_; ++k) {
      const float seedScore = seedScores[n][k];
      if (seedScore <= outputThreshold_)
        continue;

      // The model gives offsets in units of half a crop; add them to the seed centre.
      const float iphiFloat = centreOffsets[n][2 * k + 0] * (cropSize_ / 2.0f) + seedCentres_[n][k](0, 0);
      const float ietaFloat = centreOffsets[n][2 * k + 1] * (cropSize_ / 2.0f) + seedCentres_[n][k](1, 0) - 85.0f;
      const float energy = energyOutputs[n][k] * 100.0f;

      const int ieta = static_cast<int>(std::round(ietaFloat));
      const int iphi = static_cast<int>(std::round(iphiFloat));
      if (!EBDetId::validDetId(ieta, iphi))
        continue;

      barrelClusters.push_back(
          {energy, barrelPosition(ebGeom, ietaFloat, iphiFloat, ieta, iphi), EBDetId(ieta, iphi)});
    }
  }

  // ---- 2c. One PFRecHit per barrel cluster ----
  pfRecHits->reserve(barrelClusters.size());
  for (const auto& bc : barrelClusters) {
    pfRecHits->push_back(reco::PFRecHit(CaloCellGeometryMayOwnPtr(ebGeom->getGeometry(bc.crystal)),
                                        bc.crystal.rawId(),
                                        PFLayer::ECAL_BARREL,
                                        bc.energy));
  }
  auto pfRecHitHandle = event.put(std::move(pfRecHits), "mlPFRecHitsEB");

  // ---- 2d. Barrel PFClusters, each pointing to its PFRecHit ----
  for (size_t i = 0; i < barrelClusters.size(); ++i) {
    const auto& bc = barrelClusters[i];
    reco::PFCluster cluster;
    cluster.setLayer(PFLayer::ECAL_BARREL);
    cluster.setEnergy(bc.energy);
    cluster.setTime(-1.0f);
    cluster.setCorrectedEnergy(bc.energy);
    cluster.setPosition(bc.position);
    cluster.addHitAndFraction(bc.crystal.rawId(), 1.0f);
    cluster.addRecHitFraction(reco::PFRecHitFraction(reco::PFRecHitRef(pfRecHitHandle, i), 1.0f));
    cluster.calculatePositionREP();
    clusters->push_back(cluster);
  }

  // ---- 2e. Add endcap clusters and store ----
  for (const auto& c : eeClusters)
    if (c.layer() == PFLayer::ECAL_ENDCAP)
      clusters->push_back(copyEndcapCluster(c));

  event.put(std::move(clusters));
  event.put(std::make_unique<reco::PFCluster::EEtoPSAssociation>(eeToPS));
}

// =============================================================================
//  Helpers
// =============================================================================

// Rebuild the dead-channel grid, but only when the ECAL channel status changes.
void SonicProducer::updateDeadChannelGrid(edm::EventSetup const& setup) {
  if (!ecalChannelStatusWatcher_.check(setup))
    return;

  const auto& ecalStatus = setup.getData(ecalStatusToken_);
  deadChannelGrid_.assign(kRows * kColumns, 1);

  for (int ieta = -85; ieta <= 85; ++ieta) {
    for (int iphi = 0; iphi <= 360; ++iphi) {
      if (!EBDetId::validDetId(ieta, iphi))
        continue;
      EBDetId id(ieta, iphi, EBDetId::ETAPHIMODE);
      auto entry = ecalStatus.find(id);
      int status = (entry != ecalStatus.end()) ? (entry->getStatusCode() & 0x1F) : -1;
      if (status < maskedEcalChannelStatusThreshold_)
        continue;

      int value = 1;
      if (status >= 3 && status <= 10)
        value = 2;  // noisy / wrong gain
      else if (status > 10)
        value = 3;  // completely dead
      deadChannelGrid_[cellNumber(iphi, ieta + 85)] = value;
    }
  }
}

// Turn a fractional (ieta, iphi) into a 3D point on the crystal front faces,
// interpolating towards the neighbouring crystals.
math::XYZPoint SonicProducer::barrelPosition(
    const CaloSubdetectorGeometry* ebGeom, float ietaFloat, float iphiFloat, int ieta, int iphi) const {
  const float depth = 0.0f;  // front face for now; later maybe X0_ * (T0_EB_ + log(energy))

  auto frontFace = [&](int eta, int phi) {
    auto cell = ebGeom->getGeometry(EBDetId(eta, phi));
    return static_cast<const TruncatedPyramid*>(cell.get())->getPosition(depth);
  };

  const GlobalPoint centre = frontFace(ieta, iphi);
  const float deta = ietaFloat - ieta;
  const float dphi = iphiFloat - iphi;

  const int etaNeighbour = ieta + (deta >= 0 ? 1 : -1);
  const int phiNeighbour = iphi + (dphi >= 0 ? 1 : -1);
  const GlobalPoint pEta = EBDetId::validDetId(etaNeighbour, iphi) ? frontFace(etaNeighbour, iphi) : centre;
  const GlobalPoint pPhi = EBDetId::validDetId(ieta, phiNeighbour) ? frontFace(ieta, phiNeighbour) : centre;

  const float x = centre.x() + std::abs(deta) * (pEta.x() - centre.x()) + std::abs(dphi) * (pPhi.x() - centre.x());
  const float y = centre.y() + std::abs(deta) * (pEta.y() - centre.y()) + std::abs(dphi) * (pPhi.y() - centre.y());
  const float z = centre.z() + std::abs(deta) * (pEta.z() - centre.z()) + std::abs(dphi) * (pPhi.z() - centre.z());
  return math::XYZPoint(x, y, z);
}

// List of allowed config parameters (CMSSW checks the python config against this).
void SonicProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  TritonClient::fillPSetDescription(desc);  // the "Client" block
  desc.add<std::string>("jobId", "NOJOBID");
  desc.add<int>("maskedEcalChannelStatusThreshold", 1);
  desc.add<edm::InputTag>("EBrechitCollection", edm::InputTag("ecalRecHit", "EcalRecHitsEB"));
  desc.add<edm::InputTag>("inputEE");
  desc.add<edm::InputTag>("inputEEtoPSAssoc");
  desc.add<int>("cropSize", 7);
  desc.add<int>("maxClusters", 20);
  desc.add<int>("overlapLimit", 7);
  desc.add<double>("seedThreshold", 0.23);
  desc.add<double>("outputThreshold", 0.30);
  desc.add<double>("T0_EB", 7.4);
  desc.add<double>("X0", 0.89);
  descriptions.addWithDefaultLabel(desc);
}

DEFINE_FWK_MODULE(SonicProducer);