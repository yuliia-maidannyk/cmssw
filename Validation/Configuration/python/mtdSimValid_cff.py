import FWCore.ParameterSet.Config as cms

# --- Cluster associations maps producers
from SimFastTiming.MtdAssociatorProducers.mtdRecoClusterToSimLayerClusterAssociatorByHits_cfi import mtdRecoClusterToSimLayerClusterAssociatorByHits
from SimFastTiming.MtdAssociatorProducers.mtdRecoClusterToSimLayerClusterAssociation_cfi import mtdRecoClusterToSimLayerClusterAssociation
from SimFastTiming.MtdAssociatorProducers.mtdSimLayerClusterToTPAssociatorByTrackId_cfi import mtdSimLayerClusterToTPAssociatorByTrackId
from SimFastTiming.MtdAssociatorProducers.mtdSimLayerClusterToTPAssociation_cfi import mtdSimLayerClusterToTPAssociation
mtdAssociationProducers = cms.Sequence( mtdRecoClusterToSimLayerClusterAssociatorByHits +
                                        mtdRecoClusterToSimLayerClusterAssociation +
                                        mtdSimLayerClusterToTPAssociatorByTrackId +
                                        mtdSimLayerClusterToTPAssociation
                                       )

# MTD validation sequences
from Validation.MtdValidation.btlSimHitsValid_cfi import btlSimHitsValid
from Validation.MtdValidation.btlDigiHitsValid_cfi import btlDigiHitsValid
from Validation.MtdValidation.btlLocalRecoValid_cfi import btlLocalRecoValid
from Validation.MtdValidation.btlClustering_cfi import btlClustering

mtdSimValid  = cms.Sequence(btlSimHitsValid)
mtdDigiValid = cms.Sequence(btlDigiHitsValid)
mtdRecoValid = cms.Sequence(mtdAssociationProducers + btlLocalRecoValid  + btlClustering)

