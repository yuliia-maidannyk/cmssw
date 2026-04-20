# Auto generated configuration file
# using: 
# Revision: 1.19 
# Source: /local/reps/CMSSW/CMSSW/Configuration/Applications/python/ConfigBuilder.py,v 
# with command line options: step3 -n 10 -s RAW2DIGI,RECO,RECOSIM,PAT,VALIDATION:@phase2Validation+@miniAODValidation --conditions auto:phase2_realistic_T33 --datatier GEN-SIM-RECO,MINIAODSIM,DQMIO --eventcontent FEVTDEBUGHLT,MINIAODSIM,DQM --geometry DD4hepExtendedRun4D110 --era Phase2C17I13M9 --procModifiers dd4hep --filein file:step1_3.root --fileout file:step2_444.root --no_exec --python_filename step3_444.py
import FWCore.ParameterSet.Config as cms

from Configuration.Eras.Era_Phase2C17I13M9_cff import Phase2C17I13M9
from Configuration.ProcessModifiers.dd4hep_cff import dd4hep

import argparse
import os
parser = argparse.ArgumentParser(description="")
parser.add_argument('--n', type=int, help="Number of events", default=444)
parser.add_argument('--jobId', type=int, help="File number", default=444)
parser.add_argument('--threads', type=int, help="Number of threads", default=os.cpu_count() or 1)
parser.add_argument('--streams', type=int, help="Number of streams (0=auto)", default=0)
parser.add_argument('--pileup', type=int, help="Enable pileup mode", default=0)
args = parser.parse_args()
print("Number of events: ", args.n)
print("Job ID: ", args.jobId)
print("Threads: ", args.threads)
print("Streams: ", args.streams)
print("Pileup: ", args.pileup)

from Configuration.Eras.Era_Run3_2025_cff import Run3_2025

process = cms.Process('RECO',Run3_2025)

# import of standard configurations
process.load('Configuration.StandardSequences.Services_cff')
process.load('SimGeneral.HepPDTESSource.pythiapdt_cfi') # REQUIRED for HepMC
process.load('FWCore.MessageService.MessageLogger_cfi')
process.load('Configuration.EventContent.EventContent_cff')
if args.pileup == 0:
    process.load('SimGeneral.MixingModule.mixNoPU_cfi') # no pileup
else:
    process.load('SimGeneral.MixingModule.mix_POISSON_average_cfi') # with pileup
process.load('Configuration.StandardSequences.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.RawToDigi_cff')
process.load('Configuration.StandardSequences.L1Reco_cff')
process.load('Configuration.StandardSequences.Reconstruction_cff')
process.load('Configuration.StandardSequences.RecoSim_cff')
process.load('PhysicsTools.PatAlgos.slimming.metFilterPaths_cff')
process.load('Configuration.StandardSequences.PATMC_cff')
process.load('PhysicsTools.NanoAOD.nano_cff')
process.load('Configuration.StandardSequences.Validation_cff')
process.load('DQMServices.Core.DQMStoreNonLegacy_cff')
process.load('DQMOffline.Configuration.DQMOfflineMC_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')

# ADD:  Load the generatorSmeared producer to make HepMC available
# This creates a "generatorSmeared" product from the existing generator in the input file
from GeneratorInterface.Core.generatorSmeared_cfi import generatorSmeared
process.generatorSmeared = generatorSmeared.clone(
    currentTag = cms.untracked.InputTag("generator", "", "SIM"),  # From step1
    previousTag = cms.untracked. InputTag("generator", "", "SIM")
)

# Modify your TransClustering module to use HepMC
# Assuming TransClustering is defined in Validation/EcalHits/transClustering_cfi
#process.transClustering.jobId = f'{args.jobId}'
# ADD THIS LINE to tell TransClustering where to find HepMC: 
#process.transClustering.HepMCProductLabel = cms.InputTag('generatorSmeared')

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(args.n),
    output = cms.optional.untracked.allowed(cms.int32,cms.PSet)
)

# Input source
process.source = cms.Source("PoolSource",
    fileNames = cms.untracked.vstring(f'file:step2_{args.jobId}.root'),
    secondaryFileNames = cms.untracked.vstring()
)

process.options = cms.untracked.PSet(
    IgnoreCompletely = cms.untracked.vstring(),
    Rethrow = cms.untracked.vstring(),
    TryToContinue = cms.untracked.vstring(),
    accelerators = cms.untracked.vstring('*'),
    allowUnscheduled = cms.obsolete.untracked.bool,
    canDeleteEarly = cms.untracked.vstring(),
    deleteNonConsumedUnscheduledModules = cms.untracked.bool(True),
    dumpOptions = cms.untracked.bool(False),
    emptyRunLumiMode = cms.obsolete.untracked.string,
    eventSetup = cms.untracked.PSet(
        forceNumberOfConcurrentIOVs = cms.untracked.PSet(
            allowAnyLabel_=cms.required.untracked.uint32
        ),
        numberOfConcurrentIOVs = cms.untracked.uint32(0)
    ),
    fileMode = cms.untracked.string('FULLMERGE'),
    forceEventSetupCacheClearOnNewRun = cms.untracked.bool(False),
    holdsReferencesToDeleteEarly = cms.untracked.VPSet(),
    makeTriggerResults = cms.obsolete.untracked.bool,
    modulesToCallForTryToContinue = cms.untracked.vstring(),
    modulesToIgnoreForDeleteEarly = cms.untracked.vstring(),
    numberOfConcurrentLuminosityBlocks = cms.untracked.uint32(0),
    numberOfConcurrentRuns = cms.untracked.uint32(1),
    numberOfStreams = cms.untracked.uint32(args.streams),
    numberOfThreads = cms.untracked.uint32(args.threads),
    printDependencies = cms.untracked.bool(False),
    sizeOfStackForThreadsInKB = cms.optional.untracked.uint32,
    throwIfIllegalParameter = cms.untracked.bool(True),
    wantSummary = cms.untracked.bool(False)
)

# Production Info
process.configurationMetadata = cms.untracked.PSet(
    annotation = cms.untracked.string(f'step3 nevts:{args.n}'),
    name = cms.untracked.string('Applications'),
    version = cms.untracked.string('$Revision: 1.19 $')
)

process.RECOSIMoutput = cms.OutputModule("PoolOutputModule",
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('GEN-SIM-RECO'),
        filterName = cms.untracked.string('')
    ),
    fileName = cms.untracked.string('step3_RAW2DIGI_L1Reco_RECO_RECOSIM_PAT_NANO_VALIDATION_DQM.root'),
    outputCommands = process.RECOSIMEventContent.outputCommands,
    splitLevel = cms.untracked.int32(0)
)

process.MINIAODSIMoutput = cms.OutputModule("PoolOutputModule",
    compressionAlgorithm = cms.untracked.string('LZMA'),
    compressionLevel = cms.untracked.int32(4),
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('MINIAODSIM'),
        filterName = cms.untracked.string('')
    ),
    dropMetaData = cms.untracked.string('ALL'),
    eventAutoFlushCompressedSize = cms.untracked.int32(-900),
    fastCloning = cms.untracked.bool(False),
    fileName = cms.untracked.string('step3_RAW2DIGI_L1Reco_RECO_RECOSIM_PAT_NANO_VALIDATION_DQM_inMINIAODSIM.root'),
    outputCommands = process.MINIAODSIMEventContent.outputCommands,
    overrideBranchesSplitLevel = cms.untracked.VPSet(
        cms.untracked.PSet(
            branch = cms.untracked.string('patPackedCandidates_packedPFCandidates__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('recoGenParticles_prunedGenParticles__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('patTriggerObjectStandAlones_slimmedPatTrigger__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('patPackedGenParticles_packedGenParticles__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('patJets_slimmedJets__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('recoVertexs_offlineSlimmedPrimaryVertices__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('recoVertexs_offlineSlimmedPrimaryVerticesWithBS__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('recoCaloClusters_reducedEgamma_reducedESClusters_*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('EcalRecHitsSorted_reducedEgamma_reducedEBRecHits_*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('EcalRecHitsSorted_reducedEgamma_reducedEERecHits_*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('recoGenJets_slimmedGenJets__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('patJets_slimmedJetsPuppi__*'),
            splitLevel = cms.untracked.int32(99)
        ),
        cms.untracked.PSet(
            branch = cms.untracked.string('EcalRecHitsSorted_reducedEgamma_reducedESRecHits_*'),
            splitLevel = cms.untracked.int32(99)
        )
    ),
    overrideInputFileSplitLevels = cms.untracked.bool(True),
    splitLevel = cms.untracked.int32(0)
)

process.NANOEDMAODSIMoutput = cms.OutputModule("PoolOutputModule",
    compressionAlgorithm = cms.untracked.string('LZMA'),
    compressionLevel = cms.untracked.int32(9),
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('NANOAODSIM'),
        filterName = cms.untracked.string('')
    ),
    fileName = cms.untracked.string('step3_RAW2DIGI_L1Reco_RECO_RECOSIM_PAT_NANO_VALIDATION_DQM_inNANOEDMAODSIM.root'),
    outputCommands = process.NANOAODSIMEventContent.outputCommands
)

process.DQMoutput = cms.OutputModule("DQMRootOutputModule",
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('DQMIO'),
        filterName = cms.untracked.string('')
    ),
    fileName = cms.untracked.string('step3_RAW2DIGI_L1Reco_RECO_RECOSIM_PAT_NANO_VALIDATION_DQM_inDQM.root'),
    outputCommands = process.DQMEventContent.outputCommands,
    splitLevel = cms.untracked.int32(0)
)

# ROOT output (thread-safe via TFileService)
process.TFileService = cms.Service(
    "TFileService",
    fileName = cms.string(f'HAA_{args.jobId}.root'),
    compressionAlgorithm = cms.string("ZSTD"),
    compressionLevel = cms.int32(9)
)

# Output definition

process.MessageLogger.cerr.default = cms.untracked.PSet(
    limit = cms.untracked.int32(30)
)

# Add pileup
if args.pileup:
    process.mix.input.nbPileupEvents.averageNumber = cms.double(200.000000)
    process.mix.bunchspace = cms.int32(25)
    process.mix.minBunch = cms.int32(-3)
    process.mix.maxBunch = cms.int32(3)
    # process.mix.input.fileNames = cms.untracked.vstring([
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/040dcdc7-6ea6-4ef9-bf3e-bf6575a033d9.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/09a4d033-f954-48a8-8ef0-6e4f0ec12aa6.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/44fb52e6-65c4-4b51-82cf-6c32528d27f0.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/5ce8f333-4f0f-4ff9-a2be-6e8222d198d8.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/60d921f4-da81-4e14-9610-932e30a920a3.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/94d74ef9-616a-420b-9669-6eeda4c71d4a.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/b3424ef2-5c7b-43a1-b7d8-02270db82ad7.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/cfc6c8ad-7a13-4676-8111-1ef765f393b6.root', 
    #     '/store/relval/CMSSW_14_1_0/RelValMinBias_14TeV/GEN-SIM/141X_mcRun4_realistic_v1_STD_RegeneratedGS_2026D110_noPU-v1/2580000/e57f9a76-5eaf-4e5a-a0dc-d686e82076b2.root'
    # ])
    process.mix.input.fileNames = cms.untracked.vstring([
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/0207b889-4022-4139-bd59-077e9be69ca4.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/04a0e24a-1698-4186-9fc3-da7178ef5275.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/092059e6-1787-4ae4-8fd3-26731a7ffd3c.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/0bad8446-d125-46a8-a2a6-d1d384869877.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/0eaa4db2-77a7-4830-adc4-98640e028347.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/152b33de-b07e-4736-a203-7b37a05e7847.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/19ceddd9-e274-4a38-b064-932b8c261fcc.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/1cf3e2b5-7643-4538-af06-f859f4b6d927.root',
        'file:/feynman/home/dphp/ym280958/scratch/CMSSW_15_1_0_pre1/work/minbias_pu200/1cfddd24-fd40-4114-9c80-e836338bb297.root'
    ])

# Other statements
process.mix.playback = True
process.mix.digitizers = cms.PSet()
for a in process.aliases: delattr(process, a)
process.RandomNumberGeneratorService.restoreStateLabel=cms.untracked.string("randomEngineStateProducer")
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, '142X_mcRun3_2025_realistic_v7', '') # 150X_mcRun3_2025_realistic_v14

# Path and EndPath definitions
process.raw2digi_step = cms.Path(process.RawToDigi)
process.L1Reco_step = cms.Path(process.L1Reco)
process.reconstruction_step = cms.Path(process.reconstruction)
process.recosim_step = cms.Path(process.recosim)
process.Flag_BadChargedCandidateFilter = cms.Path(process.BadChargedCandidateFilter)
process.Flag_BadChargedCandidateSummer16Filter = cms.Path(process.BadChargedCandidateSummer16Filter)
process.Flag_BadPFMuonDzFilter = cms.Path(process.BadPFMuonDzFilter)
process.Flag_BadPFMuonFilter = cms.Path(process.BadPFMuonFilter)
process.Flag_BadPFMuonSummer16Filter = cms.Path(process.BadPFMuonSummer16Filter)
process.Flag_CSCTightHalo2015Filter = cms.Path(process.CSCTightHalo2015Filter)
process.Flag_CSCTightHaloFilter = cms.Path(process.CSCTightHaloFilter)
process.Flag_CSCTightHaloTrkMuUnvetoFilter = cms.Path(process.CSCTightHaloTrkMuUnvetoFilter)
process.Flag_EcalDeadCellBoundaryEnergyFilter = cms.Path(process.EcalDeadCellBoundaryEnergyFilter)
process.Flag_EcalDeadCellTriggerPrimitiveFilter = cms.Path(process.EcalDeadCellTriggerPrimitiveFilter)
process.Flag_HBHENoiseFilter = cms.Path(process.HBHENoiseFilterResultProducer+process.HBHENoiseFilter)
process.Flag_HBHENoiseIsoFilter = cms.Path(process.HBHENoiseFilterResultProducer+process.HBHENoiseIsoFilter)
process.Flag_HcalStripHaloFilter = cms.Path(process.HcalStripHaloFilter)
process.Flag_chargedHadronTrackResolutionFilter = cms.Path(process.chargedHadronTrackResolutionFilter)
process.Flag_ecalBadCalibFilter = cms.Path(process.ecalBadCalibFilter)
process.Flag_ecalLaserCorrFilter = cms.Path(process.ecalLaserCorrFilter)
process.Flag_eeBadScFilter = cms.Path(process.eeBadScFilter)
process.Flag_globalSuperTightHalo2016Filter = cms.Path(process.globalSuperTightHalo2016Filter)
process.Flag_globalTightHalo2016Filter = cms.Path(process.globalTightHalo2016Filter)
process.Flag_goodVertices = cms.Path(process.primaryVertexFilter)
process.Flag_hcalLaserEventFilter = cms.Path(process.hcalLaserEventFilter)
process.Flag_hfNoisyHitsFilter = cms.Path(process.hfNoisyHitsFilter)
process.Flag_muonBadTrackFilter = cms.Path(process.muonBadTrackFilter)
process.Flag_trackingFailureFilter = cms.Path(process.goodVertices+process.trackingFailureFilter)
process.Flag_trkPOGFilters = cms.Path(process.trkPOGFilters)
process.Flag_trkPOG_logErrorTooManyClusters = cms.Path(~process.logErrorTooManyClusters)
process.Flag_trkPOG_manystripclus53X = cms.Path(~process.manystripclus53X)
process.Flag_trkPOG_toomanystripclus53X = cms.Path(~process.toomanystripclus53X)
process.nanoAOD_step = cms.Path(process.nanoSequenceMC)
process.prevalidation_step = cms.Path(process.prevalidation)
process.prevalidation_step1 = cms.Path(process.prevalidationMiniAOD)
process.validation_step = cms.EndPath(process.validation)
process.validation_step1 = cms.EndPath(process.validationMiniAOD)
process.dqmoffline_step = cms.EndPath(process.DQMOffline)
process.dqmoffline_1_step = cms.EndPath(process.DQMOfflineExtraHLT)
process.dqmoffline_2_step = cms.EndPath(process.DQMOfflineMiniAOD)
process.dqmoffline_3_step = cms.EndPath(process.DQMOfflineNanoAOD)
process.dqmofflineOnPAT_step = cms.EndPath(process.PostDQMOffline)
process.dqmofflineOnPAT_1_step = cms.EndPath(process.PostDQMOffline)
process.dqmofflineOnPAT_2_step = cms.EndPath(process.PostDQMOfflineMiniAOD)
process.dqmofflineOnPAT_3_step = cms.EndPath(process.PostDQMOffline)
process.RECOSIMoutput_step = cms.EndPath(process.RECOSIMoutput)
process.MINIAODSIMoutput_step = cms.EndPath(process.MINIAODSIMoutput)
process.NANOEDMAODSIMoutput_step = cms.EndPath(process.NANOEDMAODSIMoutput)
process.DQMoutput_step = cms.EndPath(process.DQMoutput)

# Schedule definition
process.schedule = cms.Schedule(process.raw2digi_step,process.L1Reco_step,process.reconstruction_step,process.recosim_step,process.Flag_HBHENoiseFilter,process.Flag_HBHENoiseIsoFilter,process.Flag_CSCTightHaloFilter,process.Flag_CSCTightHaloTrkMuUnvetoFilter,process.Flag_CSCTightHalo2015Filter,process.Flag_globalTightHalo2016Filter,process.Flag_globalSuperTightHalo2016Filter,process.Flag_HcalStripHaloFilter,process.Flag_hcalLaserEventFilter,process.Flag_EcalDeadCellTriggerPrimitiveFilter,process.Flag_EcalDeadCellBoundaryEnergyFilter,process.Flag_ecalBadCalibFilter,process.Flag_goodVertices,process.Flag_eeBadScFilter,process.Flag_ecalLaserCorrFilter,process.Flag_trkPOGFilters,process.Flag_chargedHadronTrackResolutionFilter,process.Flag_muonBadTrackFilter,process.Flag_BadChargedCandidateFilter,process.Flag_BadPFMuonFilter,process.Flag_BadPFMuonDzFilter,process.Flag_hfNoisyHitsFilter,process.Flag_BadChargedCandidateSummer16Filter,process.Flag_BadPFMuonSummer16Filter,process.Flag_trkPOG_manystripclus53X,process.Flag_trkPOG_toomanystripclus53X,process.Flag_trkPOG_logErrorTooManyClusters,process.nanoAOD_step,process.prevalidation_step,process.prevalidation_step1,process.validation_step,process.validation_step1,process.dqmoffline_step,process.dqmoffline_1_step,process.dqmoffline_2_step,process.dqmoffline_3_step,process.dqmofflineOnPAT_step,process.dqmofflineOnPAT_1_step,process.dqmofflineOnPAT_2_step,process.dqmofflineOnPAT_3_step,process.RECOSIMoutput_step,process.MINIAODSIMoutput_step,process.NANOEDMAODSIMoutput_step,process.DQMoutput_step)
process.schedule.associate(process.patTask)
from PhysicsTools.PatAlgos.tools.helpers import associatePatAlgosToolsTask
associatePatAlgosToolsTask(process)

# customisation of the process.

# Automatic addition of the customisation function from SimGeneral.MixingModule.fullMixCustomize_cff
from SimGeneral.MixingModule.fullMixCustomize_cff import setCrossingFrameOn 

#call to customisation function setCrossingFrameOn imported from SimGeneral.MixingModule.fullMixCustomize_cff
process = setCrossingFrameOn(process)

# Automatic addition of the customisation function from PhysicsTools.NanoAOD.nano_cff
from PhysicsTools.NanoAOD.nano_cff import nanoAOD_customizeCommon

#call to customisation function nanoAOD_customizeCommon imported from PhysicsTools.NanoAOD.nano_cff
process = nanoAOD_customizeCommon(process)

# End of customisation functions

# customisation of the process.

# Automatic addition of the customisation function from PhysicsTools.PatAlgos.slimming.miniAOD_tools
from PhysicsTools.PatAlgos.slimming.miniAOD_tools import miniAOD_customizeAllMC 

#call to customisation function miniAOD_customizeAllMC imported from PhysicsTools.PatAlgos.slimming.miniAOD_tools
process = miniAOD_customizeAllMC(process)

# End of customisation functions

# Customisation from command line

#Have logErrorHarvester wait for the same EDProducers to finish as those providing data for the OutputModule
from FWCore.Modules.logErrorHarvester_cff import customiseLogErrorHarvesterUsingOutputCommands
process = customiseLogErrorHarvesterUsingOutputCommands(process)

# Add early deletion of temporary data products to reduce peak memory need
from Configuration.StandardSequences.earlyDeleteSettings_cff import customiseEarlyDelete
process = customiseEarlyDelete(process)
# End adding early deletion
