import FWCore.ParameterSet.Config as cms

mlPFClusterProducer = cms.EDProducer("MLPFClusterProducer",
    model_path                  = cms.string("RecoParticleFlow/PFClusterProducer/data/clus_tex.onnx"),
    input_names                 = cms.vstring("inp1", "inp2", "inp3", "inp4"),
    maskedEcalChannelStatusThreshold = cms.int32(1),
    EBrechitCollection          = cms.InputTag("particleFlowRecHitECAL"),
    cropSize                    = cms.int32(7),
    maxClusters                 = cms.int32(20),
    maxOutputClusters           = cms.int32(5),
    overlapLimit                = cms.int32(7),
    seedThreshold               = cms.double(0.23),
    outputThreshold             = cms.double(0.30),
    inputEE                     = cms.InputTag("particleFlowClusterECALForEE"),
    inputEEtoPSAssoc            = cms.InputTag("particleFlowClusterECALForEE")
)


