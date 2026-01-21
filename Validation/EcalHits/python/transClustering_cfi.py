import FWCore.ParameterSet.Config as cms

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer
transClustering = DQMEDAnalyzer("TransClustering",
    moduleLabelG4               = cms.string('g4SimHits'),
    ValidationCollection        = cms.string('EcalValidInfo'),
    EBHitsCollection            = cms.string('EcalHitsEB'),
    genParticles                = cms.InputTag("genParticles"),
    simTrackCollection          = cms.InputTag("simTrackCollection"),
    simVertexCollection         = cms.InputTag("simVertexCollection"),
    # reducedBarrelRecHitCollection = cms.InputTag("reducedEcalRecHitsEB"),
    EBrechitCollection          = cms.InputTag("ecalRecHit","EcalRecHitsEB"),
    EBuncalibrechitCollection   = cms.InputTag("ecalMultiFitUncalibRecHit","EcalUncalibRecHitsEB"),
    CaloParticleCollection      = cms.InputTag("mix", "MergedCaloTruth"),
    jobId                       = cms.string('NOJOBID'),
    HepMCProductLabel           = cms.InputTag('generatorSmeared')
)


