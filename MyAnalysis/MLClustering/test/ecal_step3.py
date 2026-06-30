import FWCore.ParameterSet.Config as cms

import argparse
import os
parser = argparse.ArgumentParser(description="")
parser.add_argument('--n', type=int, help="Number of events", default=444)
parser.add_argument('--jobId', type=int, help="File number", default=444)
parser.add_argument('--threads', type=int, help="Number of threads", default=os.cpu_count() or 1)
parser.add_argument('--streams', type=int, help="Number of streams (0=auto)", default=0)
parser.add_argument('--pileup', type=int, help="Enable pileup mode", default=0)
args = parser.parse_args()
if args.streams == 0:
    args.streams = args.threads
print("Number of events: ", args.n)
print("Job ID: ", args.jobId)
print("Threads: ", args.threads)
print("Streams: ", args.streams)
print("Pileup: ", args.pileup)

from Configuration.Eras.Era_Run3_2025_cff import Run3_2025

process = cms.Process('RECO',Run3_2025)

# import of standard configurations
process.load('Configuration.StandardSequences.Services_cff')
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
process.load('MyAnalysis.MLClustering.mlClustering_cfi')
from RecoParticleFlow.PFClusterProducer.MLPFClusterProducer_cfi import mlPFClusterProducer
process.load('Validation.EcalHits.transClustering_cfi')

# Make jet-flavour parton selection explicit for this Pythia8-based chain.
for module_name in (
    'patJetPartons',
    'selectedHadronsAndPartons',
    'selectedHadronsAndPartonsForGenJetsFlavourInfos',
):
    if hasattr(process, module_name):
        getattr(process, module_name).partonMode = cms.string('Pythia8')

# ADD:  Load the generatorSmeared producer to make HepMC available
# This creates a "generatorSmeared" product from the existing generator in the input file
from GeneratorInterface.Core.generatorSmeared_cfi import generatorSmeared
process.generatorSmeared = generatorSmeared.clone(
    currentTag = cms.untracked.InputTag("generator", "", "SIM"),  # From step1
    previousTag = cms.untracked. InputTag("generator", "", "SIM")
)

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

# ROOT output (thread-safe via TFileService)
process.TFileService = cms.Service(
    "TFileService",
    fileName = cms.string(f'ecal_{args.jobId}.root'),
    compressionAlgorithm = cms.string("ZSTD"),
    compressionLevel = cms.int32(9)
)

# Output definition

# Additional output definition
process.transClustering.jobId = f'{args.jobId}'
process.mlClustering.jobId = f'{args.jobId}'
process.transClustering.model_path = cms.string("/feynman/scratch/dphp/ym280958/CMSSW_15_0_0/src/MyAnalysis/MLClustering/data/clus_tex.onnx")
process.mlClustering.model_path = cms.string("/feynman/scratch/dphp/ym280958/CMSSW_15_0_0/src/MyAnalysis/MLClustering/data/clus_tex.onnx")

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
process.GlobalTag = GlobalTag(process.GlobalTag, '142X_mcRun3_2025_realistic_v7', '')

# Wire mlPFClusterProducer: takes EE clusters + EEtoPSAssoc from ForEE producer
process.mlPFClusterProducer = mlPFClusterProducer.clone(
    inputEE          = cms.InputTag("particleFlowClusterECALForEE"),
    inputEEtoPSAssoc = cms.InputTag("particleFlowClusterECALForEE"),
)
process.mlpf_step = cms.Path(process.mlPFClusterProducer)

# Path and EndPath definitions
process.raw2digi_step = cms.Path(process.RawToDigi)
process.reconstruction_step = cms.Path(process.reconstruction)
process.recosim_step = cms.Path(process.recosim)
process.prevalidation_step = cms.EndPath(process.prevalidation)
process.validation_step = cms.EndPath(process.validation)
process.validation.remove(process.transClustering)
process.transClustering_step = cms.EndPath(process.transClustering)
process.validation.remove(process.mlClustering)
process.mlClustering_step = cms.EndPath(process.mlClustering)

# Schedule definition
process.schedule = cms.Schedule(process.raw2digi_step,process.reconstruction_step,process.recosim_step,
process.prevalidation_step,
process.validation_step,
process.mlpf_step,
#process.transClustering_step,
process.mlClustering_step
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
