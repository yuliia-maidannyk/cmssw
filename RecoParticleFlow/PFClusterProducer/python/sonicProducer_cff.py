import FWCore.ParameterSet.Config as cms


def useSonicForMLPFClusters(process):
    """Swap the ONNX-in-process module for the Triton/GPU version."""

    # --- 1. The Triton service: starts a local server on the GPU ---
    # process.TritonService = cms.Service("TritonService",
    #     fallback = cms.PSet(                            # tracked (the fix)
    #         enable  = cms.untracked.bool(True),         # start a local server
    #         verbose = cms.untracked.bool(True),        # True to see server logs
    #         device  = cms.untracked.string("gpu"),      # run on the GPU
    #     ),
    # )

    # --- 1. Use one shared, already-running server instead of a private one ---
    process.TritonService = cms.Service("TritonService",
        fallback = cms.PSet(
            enable = cms.untracked.bool(False),              # don't start a private server
        ),
        servers = cms.untracked.VPSet(
            cms.PSet(
                name    = cms.untracked.string("shared"),
                address = cms.untracked.string("feynmangpu04.extra.cea.fr"),
                port    = cms.untracked.uint32(8002),
            ),
        ),
    )
    
    # --- 2. Replace the module, keeping its physics settings ---
    old = process.mlPFClusterProducer

    process.mlPFClusterProducer = cms.EDProducer("SonicProducer",
        Client = cms.PSet(
            mode            = cms.string("Async"),
            modelName       = cms.string("clus_tex"),
            modelConfigPath = cms.FileInPath("RecoParticleFlow/PFClusterProducer/data/models/clus_tex/config.pbtxt"),
            timeout         = cms.untracked.uint32(30),   # seconds
            preferredServer = cms.untracked.string("shared"),
        ),
        jobId                            = old.jobId,
        maskedEcalChannelStatusThreshold = old.maskedEcalChannelStatusThreshold,
        EBrechitCollection               = old.EBrechitCollection,
        inputEE                          = old.inputEE,
        inputEEtoPSAssoc                 = old.inputEEtoPSAssoc,
        cropSize                         = old.cropSize,
        maxClusters                      = old.maxClusters,
        overlapLimit                     = old.overlapLimit,
        seedThreshold                    = old.seedThreshold,
        outputThreshold                  = old.outputThreshold,
        T0_EB                            = old.T0_EB,
        X0                               = old.X0,
    )

    return process