import FWCore.ParameterSet.Config as cms

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer

truthCollector = DQMEDAnalyzer("TruthCollector",
    moduleLabelG4               = cms.string('g4SimHits'),
    EBSimHitCollection          = cms.string('EcalHitsEB'),
    genParticles                = cms.InputTag("genParticles"),
    simTrackCollection          = cms.InputTag("g4SimHits"),
    simVertexCollection         = cms.InputTag("g4SimHits"),
    EBRecHitCollection          = cms.InputTag("particleFlowRecHitECALRaw"), #cms.InputTag("ecalRecHit","EcalRecHitsEB"),
    jobId                       = cms.string('NOJOBID')
)


