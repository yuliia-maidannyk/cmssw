import FWCore.ParameterSet.Config as cms

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer

clusteringAnalyser = DQMEDAnalyzer("ClusteringAnalyser",
    jobId                       = cms.string('NOJOBID'),
    particleFlowClusterECAL     = cms.InputTag("particleFlowClusterECAL"),
    particleFlowClusterECALML   = cms.InputTag("mlPFClusterProducer")
)
