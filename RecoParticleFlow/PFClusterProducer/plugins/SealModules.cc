#include "RecoLocalCalo/HGCalRecProducers/interface/HGCalLayerClusterAlgoFactory.h"
#include "RecoLocalCalo/HGCalRecProducers/interface/HGCalClusteringAlgoBase.h"
#include "RecoParticleFlow/PFClusterProducer/plugins/BarrelCLUEAlgo.h"
#include "RecoParticleFlow/PFClusterProducer/plugins/BarrelClusTEXAlgo.h"
#include "FWCore/ParameterSet/interface/ValidatedPluginMacros.h"

DEFINE_EDM_VALIDATED_PLUGIN(HGCalLayerClusterAlgoFactory, EBCLUEAlgo, "EBCLUE");
DEFINE_EDM_VALIDATED_PLUGIN(HGCalLayerClusterAlgoFactory, BarrelClusTEXAlgo, "EBClusTEX");
DEFINE_EDM_VALIDATED_PLUGIN(HGCalLayerClusterAlgoFactory, HBCLUEAlgo, "HBCLUE");
