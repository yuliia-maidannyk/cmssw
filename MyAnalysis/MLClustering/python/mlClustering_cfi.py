import FWCore.ParameterSet.Config as cms

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer

mlClustering = DQMEDAnalyzer("MLClustering",
    model_path                  = cms.string("MyAnalysis/MLClustering/data/clus_tex.onnx"),
    input_names                 = cms.vstring("inp1", "inp2", "inp3", "inp4"),
    onnxIntraOpThreads          = cms.untracked.int32(4),
    onnxInterOpThreads          = cms.untracked.int32(1),
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
    maskedEcalChannelStatusThreshold = cms.int32(1),
    cropSize                    = cms.int32(7),
    maxClusters                 = cms.int32(20),
    overlapLimit                = cms.int32(7),
    seedThreshold               = cms.double(0.66)
)


