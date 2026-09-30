import FWCore.ParameterSet.Config as cms

from DQMServices.Core.DQMEDAnalyzer import DQMEDAnalyzer

clusteringAnalyser = DQMEDAnalyzer("ClusteringAnalyser",
    jobId                   = cms.string('NOJOBID'),
    particleFlowClusterECAL = cms.InputTag("particleFlowClusterECAL"),
    #mlPFClusters            = cms.InputTag('mlPFClusterProducer', 'mlPFClusters'),
    mlPFClusters2            = cms.InputTag('barrelLayerClustersEBML'),
    ticlClusters            = cms.InputTag('barrelLayerClustersEB'),
    mlPFClusters            = cms.InputTag('barrelLayerClustersEBClusTEX')
)


