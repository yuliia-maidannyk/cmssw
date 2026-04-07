import FWCore.ParameterSet.Config as cms

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer
transClustering = DQMEDAnalyzer("TransClustering",
    moduleLabelG4               = cms.string('g4SimHits'),
    ValidationCollection        = cms.string('EcalValidInfo'),
    EBHitsCollection            = cms.string('EcalHitsEB'),
    genParticles                = cms.InputTag("genParticles"),
    simTrackCollection          = cms.InputTag("g4SimHits"),
    simVertexCollection         = cms.InputTag("g4SimHits"),
    EBrechitCollection          = cms.InputTag("ecalRecHit","EcalRecHitsEB"),
    EBuncalibrechitCollection   = cms.InputTag("ecalMultiFitUncalibRecHit","EcalUncalibRecHitsEB"),
    CaloParticleCollection      = cms.InputTag("mix", "MergedCaloTruth"),
    jobId                       = cms.string('NOJOBID'),
    maskedEcalChannelStatusThreshold = cms.int32(1),
    graphPath                   = cms.string("/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/src/graph.pb"),
    inputTensorName             = cms.string("input"),
    outputTensorName            = cms.string("output"),
    cropSize                    = cms.int32(7),
    maxClusters                 = cms.int32(20),
    overlapLimit                = cms.int32(7),
    seedThreshold               = cms.double(0.54)
)


