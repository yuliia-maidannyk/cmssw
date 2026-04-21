import FWCore.ParameterSet.Config as cms

mlPFClusterProducer = cms.EDProducer("MLPFClusterProducer",
    model_path                  = cms.string("RecoParticleFlow/PFClusterProducer/data/clus_tex.onnx"),
    input_names                 = cms.vstring("inp1", "inp2", "inp3", "inp4"),
    jobId                       = cms.string('NOJOBID'),
    maskedEcalChannelStatusThreshold = cms.int32(1),
    EBrechitCollection          = cms.InputTag("ecalRecHit","EcalRecHitsEB"),
    cropSize                    = cms.int32(7),
    maxClusters                 = cms.int32(20),
    overlapLimit                = cms.int32(7),
    seedThreshold               = cms.double(0.54),
    outputThreshold             = cms.double(0.35),
    # inputEE                     = cms.InputTag("particleFlowClusterECALForEE"),
    # inputEEtoPSAssoc            = cms.InputTag("particleFlowClusterECALForEE"),
)


