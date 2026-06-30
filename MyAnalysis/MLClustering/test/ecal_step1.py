# Auto generated configuration file
# using: 
# Revision: 1.19 
# Source: /local/reps/CMSSW/CMSSW/Configuration/Applications/python/ConfigBuilder.py,v 
# with command line options: SingleGammaFlatPt8To150_cfi -s GEN,SIM -n 1000 --conditions auto:phase2_realistic_T33_13TeV --beamspot DBrealisticHLLHC --datatier GEN-SIM --eventcontent FEVTDEBUG --geometry DD4hepExtendedRun4D110 --era Phase2C17I13M9 --procModifiers dd4hep --relval 9000,100 --fileout file:step1_2.root
import FWCore.ParameterSet.Config as cms
import argparse
import os
import random

parser = argparse.ArgumentParser(description="")
parser.add_argument('--n', type=int, help="Number of events", default=444)
parser.add_argument('--jobId', type=int, help="File number", default=444)
parser.add_argument('--threads', type=int, help="Number of threads", default=os.cpu_count() or 1)
parser.add_argument('--streams', type=int, help="Number of streams (0=auto)", default=0)
args = parser.parse_args()
print("Number of events: ", args.n)
print("Job ID: ", args.jobId)
print("Threads: ", args.threads)
print("Streams: ", args.streams)

from Configuration.Eras.Era_Run3_2025_cff import Run3_2025
process = cms.Process('SIM',Run3_2025)

random.seed(int.from_bytes(os.urandom(10), "big")) # ~10^14

# import of standard configurations
process.load('Configuration.StandardSequences.Services_cff')
process.load('SimGeneral.HepPDTESSource.pythiapdt_cfi')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.load('Configuration.EventContent.EventContent_cff')
process.load('SimGeneral.MixingModule.mixNoPU_cfi')
process.load('Configuration.StandardSequences.GeometryRecoDB_cff')
process.load('Configuration.StandardSequences.GeometrySimDB_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.Generator_cff')
process.load('IOMC.EventVertexGenerators.VtxSmearedRealistic_cfi')
process.load('GeneratorInterface.Core.genFilterSummary_cff')
process.load('Configuration.StandardSequences.SimIdeal_cff')
process.load('Configuration.StandardSequences.EndOfProcess_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')
process.load('Configuration.ProcessModifiers.fineCalo_cff')

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(args.n),
    output = cms.optional.untracked.allowed(cms.int32,cms.PSet)
)

# Input source
process.source = cms.Source("EmptySource")

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
    annotation = cms.untracked.string('SingleGammaFlatPt8To150_cfi nevts:'+str(args.n)),
    name = cms.untracked.string('Applications'),
    version = cms.untracked.string('$Revision: 1.19 $')
)

# Output definition

process.FEVTDEBUGoutput = cms.OutputModule("PoolOutputModule",
    outputCommands = cms.untracked.vstring(
        'keep *',
        # Make sure HepMCProduct is NOT dropped
        'keep *_generatorSmeared_*_*',
        'keep *_generator_*_*',
        'drop *_genParticles_*_*',
        'drop *_genParticlesForJets_*_*',
        'drop *_kt4GenJets_*_*',
        'drop *_kt6GenJets_*_*',
        'drop *_iterativeCone5GenJets_*_*',
        'drop *_ak4GenJets_*_*',
        'drop *_ak7GenJets_*_*',
        'drop *_ak8GenJets_*_*',
        'drop *_ak4GenJetsNoNu_*_*',
        'drop *_ak8GenJetsNoNu_*_*',
        'drop *_genCandidatesForMET_*_*',
        'drop *_genParticlesForMETAllVisible_*_*',
        'drop *_genMetCalo_*_*',
        'drop *_genMetCaloAndNonPrompt_*_*',
        'drop *_genMetTrue_*_*',
        'drop *_genMetIC5GenJs_*_*'
    ),
    SelectEvents = cms.untracked.PSet(
        SelectEvents = cms.vstring('generation_step')
    ),
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('GEN-SIM'),
        filterName = cms.untracked.string('')
    ),
    fileName = cms.untracked.string('file:step1_' + str(args.jobId) + '.root'),
    #outputCommands = process.FEVTDEBUGEventContent.outputCommands,
    splitLevel = cms.untracked.int32(0)
)

# Additional output definition

process.g4SimHits.DoFineCalo = cms.bool(True)
process.g4SimHits.SaveCaloBoundaryInformation = cms.bool(True)
process.g4SimHits.EminFineTrack = cms.double(0.0)   # MeV, min track KE to apply fine treatment
process.g4SimHits.FineCaloNames = cms.vstring('ECAL','HCal','HGCal','HFNoseVol','VCAL')
process.g4SimHits.UseFineCalo = cms.vint32(0) # which entries above are active

# Other statements
process.genstepfilter.triggerConditions=cms.vstring("generation_step")
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, '142X_mcRun3_2025_realistic_v7', '')

process.RandomNumberGeneratorService = cms.Service("RandomNumberGeneratorService",
    VtxSmeared = cms.PSet(
        initialSeed = cms.untracked.uint32(random.randint(0,999999))
    ),
    g4SimHits = cms.PSet(
        initialSeed = cms.untracked.uint32(random.randint(0,999999))
    ),
    generator = cms.PSet(
        initialSeed = cms.untracked.uint32(random.randint(0,999999))
    ),
    LHCTransport = cms.PSet(
        initialSeed = cms.untracked.uint32(random.randint(0,999999)),
        engineName = cms.untracked.string('TRandom3')
    )
    #saveFileName = cms.untracked.string('RandomEngineStates_' + str(args.jobId) + '.txt')
)

process.generator = cms.EDProducer("ManyParticleFlatRandomEGunProducer",
    PGunParameters = cms.PSet(
        PartID = cms.vint32(22,22),
        MinEta = cms.vdouble(-1.479,0.0),
        MaxEta = cms.vdouble(0.0,1.479),
        MinPhi = cms.vdouble(-3.14159265359,-3.14159265359),
        MaxPhi = cms.vdouble(3.14159265359,3.14159265359),
        MinE = cms.vdouble(1.0,1.0),
        MaxE = cms.vdouble(100.0,100.0),
        Mass = cms.vdouble(0.0,0.0)
    ),
    BackToBack = cms.bool(True),
    AddAntiParticle = cms.bool(False),
    Verbosity = cms.untracked.int32(1),
    firstRun = cms.untracked.uint32(0),
    psethack = cms.string('2 gammas energy 1 to 100')
)

# process.generator = cms.EDProducer("ManyParticleFlatRandomEGunProducer",
#     PGunParameters = cms.PSet(
#         PartID = cms.vint32(22),
#         MinEta = cms.vdouble(0.605),
#         MaxEta = cms.vdouble(0.605),
#         MinPhi = cms.vdouble(2.620),
#         MaxPhi = cms.vdouble(2.620),
#         MinE = cms.vdouble(50.0),
#         MaxE = cms.vdouble(50.0),
#     ),
#     AddAntiParticle = cms.bool(False),
#     Verbosity = cms.untracked.int32(100),
#     firstRun = cms.untracked.uint32(0),
#     psethack = cms.string('1 gamma energy 50')
# )

# process.generator = cms.EDProducer("MultiParticleInConeGunProducer",
#     PGunParameters = cms.PSet(
#         PartID = cms.vint32(22),
#         InConeID = cms.vint32(22),
#         MinDeltaR = cms.double(0.012), # N * sqrt((0.0174)^2 + (pi/180)^2) with N=1,2,3...
#         MaxDeltaR = cms.double(0.074), # N * sqrt((0.0174)^2 + (pi/180)^2) with N=1,2,3...
#         MinMomRatio = cms.double(0.1),
#         MaxMomRatio = cms.double(1.0),
#         InConeMinEta = cms.double(-1.45),
#         InConeMaxEta = cms.double(1.45),
#         InConeMinPhi = cms.double(-3.1),
#         InConeMaxPhi = cms.double(3.1),
#         InConeMaxTry = cms.uint32(2),
#         MinEta = cms.double(-1.45),
#         MaxEta = cms.double(1.45),
#         MinPhi = cms.double(-3.1),
#         MaxPhi = cms.double(3.1),
#         MinE = cms.double(1.0),
#         MaxE = cms.double(100.0),
#     ),
#     AddAntiParticle = cms.bool(False),
#     Verbosity = cms.untracked.int32(100),
#     firstRun = cms.untracked.uint32(0),
#     psethack = cms.string('2 gamma in cone energy 1 to 100')
# )

process.g4SimHits.SteppingVerbosity = 0

# process.MessageLogger.debugModules = cms.untracked.vstring("*")
process.MessageLogger.cerr.threshold = cms.untracked.string('WARNING')
process.MessageLogger.cerr.DoFineCalo = cms.untracked.PSet(
    limit = cms.untracked.int32(-1)   # -1 = no limit
)
process.MessageLogger.cerr.CaloSim = cms.untracked.PSet(
    limit = cms.untracked.int32(-1)
)
# process.MessageLogger.cerr.DEBUG = cms.untracked.PSet(
#     limit = cms.untracked.int32(0)
# )
# process.MessageLogger.cerr.INFO = cms.untracked.PSet(
#     limit = cms.untracked.int32(0)
# )
# process.MessageLogger.cerr.G4cout = cms.untracked.PSet(
#     limit = cms.untracked.int32(-1)
# )
# process.MessageLogger.cerr.G4cerr = cms.untracked.PSet(
#     limit = cms.untracked.int32(-1)
# )

# Path and EndPath definitions
process.generation_step = cms.Path(process.pgen)
process.simulation_step = cms.Path(process.psim)
process.genfiltersummary_step = cms.EndPath(process.genFilterSummary)
process.endjob_step = cms.EndPath(process.endOfProcess)
process.FEVTDEBUGoutput_step = cms.EndPath(process.FEVTDEBUGoutput)

# Schedule definition
process.schedule = cms.Schedule(process.generation_step,process.genfiltersummary_step,process.simulation_step,process.endjob_step,process.FEVTDEBUGoutput_step)
from PhysicsTools.PatAlgos.tools.helpers import associatePatAlgosToolsTask
associatePatAlgosToolsTask(process)

#Setup FWK for multithreaded
process.options.numberOfConcurrentLuminosityBlocks = 1
process.options.eventSetup.numberOfConcurrentIOVs = 1
# filter all path with the production filter sequence
for path in process.paths:
	getattr(process,path).insert(0, process.generator)

# Customisation from command line

# Add early deletion of temporary data products to reduce peak memory need
from Configuration.StandardSequences.earlyDeleteSettings_cff import customiseEarlyDelete
process = customiseEarlyDelete(process)
# End adding early deletion
