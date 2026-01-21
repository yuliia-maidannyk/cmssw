# Auto generated configuration file
# using: 
# Revision: 1.19 
# Source: /local/reps/CMSSW/CMSSW/Configuration/Applications/python/ConfigBuilder.py,v 
# with command line options: step3 -n 10 -s RAW2DIGI,RECO,RECOSIM,PAT,VALIDATION:@phase2Validation+@miniAODValidation --conditions auto:phase2_realistic_T33 --datatier GEN-SIM-RECO,MINIAODSIM,DQMIO --eventcontent FEVTDEBUGHLT,MINIAODSIM,DQM --geometry DD4hepExtendedRun4D110 --era Phase2C17I13M9 --procModifiers dd4hep --filein file:step1_3.root --fileout file:step2_444.root --no_exec --python_filename step3_444.py
import FWCore.ParameterSet.Config as cms

from Configuration.Eras.Era_Phase2C17I13M9_cff import Phase2C17I13M9
from Configuration.ProcessModifiers.dd4hep_cff import dd4hep

import argparse
parser = argparse.ArgumentParser(description="")
parser.add_argument('--n', type=int, help="Number of events", default=444)
parser.add_argument('--jobId', type=int, help="File number", default=444)
args = parser.parse_args()
print("Number of events: ", args.n)
print("Job ID: ", args.jobId)

process = cms.Process('RECO',Phase2C17I13M9,dd4hep)

# import of standard configurations
process.load('Configuration.StandardSequences.Services_cff')
process.load('SimGeneral.HepPDTESSource.pythiapdt_cfi') # REQUIRED for HepMC
process.load('FWCore.MessageService.MessageLogger_cfi')
process.load('Configuration.EventContent.EventContent_cff')
process.load('SimGeneral.MixingModule.mixNoPU_cfi')
process.load('Configuration.Geometry.GeometryDD4hepExtendedRun4D110Reco_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.RawToDigi_cff')
process.load('Configuration.StandardSequences.Reconstruction_cff')
process.load('Configuration.StandardSequences.RecoSim_cff')
process.load('PhysicsTools.PatAlgos.slimming.metFilterPaths_cff')
process.load('Configuration.StandardSequences.PATMC_cff')
process.load('Configuration.StandardSequences.Validation_cff')
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
process.transClustering.jobId = f'{args.jobId}'
# ADD THIS LINE to tell TransClustering where to find HepMC: 
process.transClustering.HepMCProductLabel = cms.InputTag('generatorSmeared')

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
    numberOfStreams = cms.untracked.uint32(0),
    numberOfThreads = cms.untracked.uint32(1),
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

# Output definition

# process.FEVTDEBUGHLToutput = cms.OutputModule("PoolOutputModule",
#     dataset = cms.untracked.PSet(
#         dataTier = cms.untracked.string('GEN-SIM-RECO'),
#         filterName = cms.untracked.string('')
#     ),
#     fileName = cms.untracked.string(f'file:step3_{args.jobId}.root'),
#     outputCommands = process.FEVTDEBUGHLTEventContent.outputCommands,
#     splitLevel = cms.untracked.int32(0)
# )

# process.MINIAODSIMoutput = cms.OutputModule("PoolOutputModule",
#     compressionAlgorithm = cms.untracked.string('LZMA'),
#     compressionLevel = cms.untracked.int32(4),
#     dataset = cms.untracked.PSet(
#         dataTier = cms.untracked.string('MINIAODSIM'),
#         filterName = cms.untracked.string('')
#     ),
#     dropMetaData = cms.untracked.string('ALL'),
#     eventAutoFlushCompressedSize = cms.untracked.int32(-900),
#     fastCloning = cms.untracked.bool(False),
#     fileName = cms.untracked.string('file:step3_3_inMINIAODSIM.root'),
#     outputCommands = process.MINIAODSIMEventContent.outputCommands,
#     overrideBranchesSplitLevel = cms.untracked.VPSet(
#         cms.untracked.PSet(
#             branch = cms.untracked.string('patPackedCandidates_packedPFCandidates__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('recoGenParticles_prunedGenParticles__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('patTriggerObjectStandAlones_slimmedPatTrigger__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('patPackedGenParticles_packedGenParticles__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('patJets_slimmedJets__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('recoVertexs_offlineSlimmedPrimaryVertices__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('recoVertexs_offlineSlimmedPrimaryVerticesWithBS__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('recoCaloClusters_reducedEgamma_reducedESClusters_*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('EcalRecHitsSorted_reducedEgamma_reducedEBRecHits_*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('EcalRecHitsSorted_reducedEgamma_reducedEERecHits_*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('recoGenJets_slimmedGenJets__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('patJets_slimmedJetsPuppi__*'),
#             splitLevel = cms.untracked.int32(99)
#         ),
#         cms.untracked.PSet(
#             branch = cms.untracked.string('EcalRecHitsSorted_reducedEgamma_reducedESRecHits_*'),
#             splitLevel = cms.untracked.int32(99)
#         )
#     ),
#     overrideInputFileSplitLevels = cms.untracked.bool(True),
#     splitLevel = cms.untracked.int32(0)
# )

# process.DQMoutput = cms.OutputModule("DQMRootOutputModule",
#     dataset = cms.untracked.PSet(
#         dataTier = cms.untracked.string('DQMIO'),
#         filterName = cms.untracked.string('')
#     ),
#     fileName = cms.untracked.string(f'file:step3_{args.jobId}_inDQM.root'),
#     outputCommands = process.DQMEventContent.outputCommands,
#     splitLevel = cms.untracked.int32(0)
# )

# Additional output definition
#process.btlClustering.jobId = f'{args.jobId}'
process.transClustering.jobId = f'{args.jobId}'

# Other statements
process.mix.playback = True
process.mix.digitizers = cms.PSet()
for a in process.aliases: delattr(process, a)
process.RandomNumberGeneratorService.restoreStateLabel=cms.untracked.string("randomEngineStateProducer")
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, 'auto:phase2_realistic_T33', '')

# Path and EndPath definitions
process.raw2digi_step = cms.Path(process.RawToDigi)
process.reconstruction_step = cms.Path(process.reconstruction)
process.recosim_step = cms.Path(process.recosim)
#process.prevalidation_step = cms.Path(process.baseCommonPreValidation)
# Modify the validation step to include generatorSmeared BEFORE validation
process.prevalidation_step = cms.Path(
    process.generatorSmeared *     # ADD THIS - create HepMC product first
    process.baseCommonPreValidation
)
process.validation_step = cms.EndPath(process.baseCommonValidation)
#process.validation_step10 = cms.EndPath(process.globalValidationMTD)
process.validation_step11 = cms.EndPath(process.validationECALPhase2)


# Schedule definition
process.schedule = cms.Schedule(process.raw2digi_step,process.reconstruction_step,process.recosim_step,process.Flag_HBHENoiseFilter,process.Flag_HBHENoiseIsoFilter,process.Flag_CSCTightHaloFilter,process.Flag_CSCTightHaloTrkMuUnvetoFilter,process.Flag_CSCTightHalo2015Filter,process.Flag_globalTightHalo2016Filter,process.Flag_globalSuperTightHalo2016Filter,process.Flag_HcalStripHaloFilter,process.Flag_hcalLaserEventFilter,process.Flag_EcalDeadCellTriggerPrimitiveFilter,process.Flag_EcalDeadCellBoundaryEnergyFilter,process.Flag_ecalBadCalibFilter,process.Flag_goodVertices,process.Flag_eeBadScFilter,process.Flag_ecalLaserCorrFilter,process.Flag_trkPOGFilters,process.Flag_chargedHadronTrackResolutionFilter,process.Flag_muonBadTrackFilter,process.Flag_BadChargedCandidateFilter,process.Flag_BadPFMuonFilter,process.Flag_BadPFMuonDzFilter,process.Flag_hfNoisyHitsFilter,process.Flag_BadChargedCandidateSummer16Filter,process.Flag_BadPFMuonSummer16Filter,process.Flag_trkPOG_manystripclus53X,process.Flag_trkPOG_toomanystripclus53X,process.Flag_trkPOG_logErrorTooManyClusters,
process.prevalidation_step,
process.validation_step,
#process.validation_step10,
process.validation_step11
)
process.schedule.associate(process.patTask)
from PhysicsTools.PatAlgos.tools.helpers import associatePatAlgosToolsTask
associatePatAlgosToolsTask(process)

# customisation of the process.

# Automatic addition of the customisation function from SimGeneral.MixingModule.fullMixCustomize_cff
from SimGeneral.MixingModule.fullMixCustomize_cff import setCrossingFrameOn 

#call to customisation function setCrossingFrameOn imported from SimGeneral.MixingModule.fullMixCustomize_cff
process = setCrossingFrameOn(process)

# End of customisation functions

# customisation of the process.

# Automatic addition of the customisation function from PhysicsTools.PatAlgos.slimming.miniAOD_tools
# from PhysicsTools.PatAlgos.slimming.miniAOD_tools import miniAOD_customizeAllMC 

#call to customisation function miniAOD_customizeAllMC imported from PhysicsTools.PatAlgos.slimming.miniAOD_tools
# process = miniAOD_customizeAllMC(process)

# End of customisation functions

# Customisation from command line

#Have logErrorHarvester wait for the same EDProducers to finish as those providing data for the OutputModule
from FWCore.Modules.logErrorHarvester_cff import customiseLogErrorHarvesterUsingOutputCommands
process = customiseLogErrorHarvesterUsingOutputCommands(process)

# Add early deletion of temporary data products to reduce peak memory need
from Configuration.StandardSequences.earlyDeleteSettings_cff import customiseEarlyDelete
process = customiseEarlyDelete(process)
# End adding early deletion
