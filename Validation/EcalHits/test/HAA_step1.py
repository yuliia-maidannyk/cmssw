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
    annotation = cms.untracked.string('Custom_X_pair_2GeV_GEN-SIM nevts:'+str(args.n)),
    name = cms.untracked.string('Applications'),
    version = cms.untracked.string('$Revision: 1.19 $')
)

# Output definition

process.FEVTDEBUGoutput = cms.OutputModule("PoolOutputModule",
    outputCommands = cms.untracked.vstring(
        'keep *',
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
    splitLevel = cms.untracked.int32(0)
)

# Additional output definition

# Other statements
if hasattr(process, "XMLFromDBSource"): process.XMLFromDBSource.label="Extended"
if hasattr(process, "DDDetectorESProducerFromDB"): process.DDDetectorESProducerFromDB.label="Extended"
process.genstepfilter.triggerConditions=cms.vstring("generation_step")
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, '142X_mcRun3_2025_realistic_v7', '') # 150X_mcRun3_2025_realistic_v14

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

process.load("SimGeneral.HepPDTESSource.pythiapdt_cfi")
 
process.customParticles = cms.ESSource(
    "HepPDTESSource",
    pdtFileName = cms.FileInPath("Validation/EcalHits/data/Pi0Table.txt")
)
 
process.es_prefer_custom = cms.ESPrefer("HepPDTESSource", "customParticles")
 
# process.generator = cms.EDFilter("Pythia8EGun",
#     PGunParameters = cms.PSet(
#         ParticleID = cms.vint32(9000001),
#         MinEta = cms.double(-1.479),
#         MaxEta = cms.double(1.479),
#         MinPhi = cms.double(-3.14159265359),
#         MaxPhi = cms.double(3.14159265359),
#         MinE = cms.double(1.0),
#         MaxE = cms.double(100.0),
#         AddAntiParticle = cms.bool(False)
#     ),
#     PythiaParameters = cms.PSet(
#         parameterSets = cms.vstring("pythiaUESettings", "ProcessParameters"),
#         pythiaUESettings = cms.vstring(
#         ),
#         ProcessParameters = cms.vstring(
#             '9000001:new = X X 1 0 0 2.0 0.0 0.0 0.0 0.0',
#             '9000001:isResonance = false',
#             '9000001:mayDecay = false',
#             '9000001:addChannel = 1 1.0 0 22 22'
#         )
#     ),
#     Verbosity = cms.untracked.int32(0),
#     firstRun = cms.untracked.uint32(0),
# )

process.generator = cms.EDProducer("ManyParticleFlatRandomEGunProducer",
    PGunParameters = cms.PSet(
        PartID = cms.vint32(9000001,9000001),
        MinEta = cms.vdouble(-1.479,0),
        MaxEta = cms.vdouble(0,1.479),
        MinPhi = cms.vdouble(-3.14159265359,-3.14159265359),
        MaxPhi = cms.vdouble(3.14159265359,3.14159265359),
        # For X with m=2 GeV, require E >= m to avoid invalid kinematics.
        MinE = cms.vdouble(2.1,2.1),
        MaxE = cms.vdouble(100.0,100.0),
        Mass = cms.vdouble(2.0, 2.0)
    ),
    AddAntiParticle = cms.bool(False),
    Verbosity = cms.untracked.int32(100),
    firstRun = cms.untracked.uint32(0),
    BackToBack = cms.bool(True),
    psethack = cms.string('2 Xs energy 1 to 100')
)

process.g4SimHits.SteppingVerbosity = 0

# Ensure Geant4 uses the local physics list plugin set and add the custom constructor.
process.g4SimHits.Physics.type = cms.string('SimG4Core/Physics/FTFP_BERT_EMM')
process.g4SimHits.Physics.Pi0MassModifier = cms.PSet(
    Type = cms.string('Pi0MassModifier'),
    massGeV = cms.untracked.double(2.0)
)

# process.MessageLogger.debugModules = cms.untracked.vstring("*")
process.MessageLogger.cerr.threshold = cms.untracked.string('WARNING')
# process.MessageLogger.cerr.INFO = cms.untracked.PSet(
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
