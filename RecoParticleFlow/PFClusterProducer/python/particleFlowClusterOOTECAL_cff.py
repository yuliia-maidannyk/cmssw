import FWCore.ParameterSet.Config as cms
from RecoParticleFlow.PFClusterProducer.particleFlowClusterECAL_cff import _particleFlowClusterECAL

particleFlowClusterOOTECAL = _particleFlowClusterECAL.clone(
    inputECAL = "particleFlowClusterOOTECALUncorrected"
)


# from RecoParticleFlow.PFClusterProducer.particleFlowClusterECAL_cfi import particleFlowClusterECAL as _correctedECALProducer

# particleFlowClusterOOTECAL = _correctedECALProducer.clone(
#     inputECAL = "particleFlowClusterOOTECALUncorrected"
# )