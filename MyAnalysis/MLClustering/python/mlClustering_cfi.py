import FWCore.ParameterSet.Config as cms

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer

mlClustering = DQMEDAnalyzer("MLClustering",
    moduleLabelG4               = cms.string('g4SimHits'),
    EBSimHitCollection          = cms.string('EcalHitsEB'),
    genParticles                = cms.InputTag("genParticles"),
    simTrackCollection          = cms.InputTag("g4SimHits"),
    simVertexCollection         = cms.InputTag("g4SimHits"),
    EBRecHitCollection          = cms.InputTag("ecalRecHit","EcalRecHitsEB"),
    CaloParticleCollection      = cms.InputTag("mix", "MergedCaloTruth"),
    jobId                       = cms.string('NOJOBID'),
    particleFlowClusterECAL     = cms.InputTag("particleFlowClusterECAL"),
    particleFlowClusterECALML   = cms.InputTag("mlPFClusterProducer"),
    maskedEcalChannelStatusThreshold = cms.int32(1)
)


