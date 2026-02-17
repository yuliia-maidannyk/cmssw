# Auto generated configuration file
# using: 
# Revision: 1.19 
# Source: /local/reps/CMSSW/CMSSW/Configuration/Applications/python/ConfigBuilder.py,v 
# with command line options: step2 -n 10 -s DIGI:pdigi_valid,L1TrackTrigger,L1,L1P2GT,DIGI2RAW,HLT:@relvalRun4 --conditions auto:phase2_realistic_T33 --datatier GEN-SIM-DIGI-RAW --eventcontent FEVTDEBUGHLT --geometry DD4hepExtendedRun4D110 --era Phase2C17I13M9 --procModifiers dd4hep --filein file:step1_3.root --fileout file:step2_444.root --no_exec --python_filename step2_444.py
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

process = cms.Process('HLT',Phase2C17I13M9,dd4hep)

# import of standard configurations
process.load('Configuration.StandardSequences.Services_cff')
process.load('SimGeneral.HepPDTESSource.pythiapdt_cfi')
process.load('FWCore.MessageService.MessageLogger_cfi')
process.load('Configuration.EventContent.EventContent_cff')
if args.pileup == 0:
    process.load('SimGeneral.MixingModule.mixNoPU_cfi') # no pileup
else:
    process.load('SimGeneral.MixingModule.mix_POISSON_average_cfi') # with pileup
process.load('Configuration.Geometry.GeometryDD4hepExtendedRun4D110Reco_cff')
process.load('Configuration.StandardSequences.MagneticField_cff')
process.load('Configuration.StandardSequences.Digi_cff')
process.load('Configuration.StandardSequences.L1TrackTrigger_cff')
process.load('Configuration.StandardSequences.SimL1Emulator_cff')
process.load('Configuration.StandardSequences.SimPhase2L1GlobalTriggerEmulator_cff')
process.load('L1Trigger.Configuration.Phase2GTMenus.SeedDefinitions.step1_2024.l1tGTMenu_cff')
process.load('Configuration.StandardSequences.DigiToRaw_cff')
process.load('HLTrigger.Configuration.HLT_75e33_cff')
process.load('Configuration.StandardSequences.EndOfProcess_cff')
process.load('Configuration.StandardSequences.FrontierConditions_GlobalTag_cff')

process.maxEvents = cms.untracked.PSet(
    input = cms.untracked.int32(args.n),
    output = cms.optional.untracked.allowed(cms.int32,cms.PSet)
)

# Input source
process.source = cms.Source("PoolSource",
    dropDescendantsOfDroppedBranches = cms.untracked.bool(False),
    fileNames = cms.untracked.vstring(f'file:step1_{args.jobId}.root'),
    inputCommands = cms.untracked.vstring(
        'keep *',
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
    secondaryFileNames = cms.untracked.vstring()
)

process.options = cms.untracked.PSet(
    IgnoreCompletely = cms.untracked.vstring(),
    Rethrow = cms.untracked.vstring(),
    TryToContinue = cms.untracked.vstring('ProductNotFound'),
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
    annotation = cms.untracked.string(f'step2 nevts:{args.n}'),
    name = cms.untracked.string('Applications'),
    version = cms.untracked.string('$Revision: 1.19 $')
)

# Output definition

process.FEVTDEBUGHLToutput = cms.OutputModule("PoolOutputModule",
    dataset = cms.untracked.PSet(
        dataTier = cms.untracked.string('GEN-SIM-DIGI-RAW'),
        filterName = cms.untracked.string('')
    ),
    fileName = cms.untracked.string(f'file:step2_{args.jobId}.root'),
    outputCommands = process.FEVTDEBUGHLTEventContent.outputCommands,
    splitLevel = cms.untracked.int32(0)
)

# ADD THESE LINES to keep HepMC
process.FEVTDEBUGHLToutput.outputCommands.append('keep *_generatorSmeared_*_*')
process.FEVTDEBUGHLToutput.outputCommands.append('keep *_generator_*_*')
process.FEVTDEBUGHLToutput.outputCommands.append('keep *_VtxSmeared_*_*')

# Additional output definition

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
process.mix.digitizers = cms.PSet(process.theDigitizersValid)
from Configuration.AlCa.GlobalTag import GlobalTag
process.GlobalTag = GlobalTag(process.GlobalTag, '141X_mcRun4_realistic_v3', '')

# Path and EndPath definitions
process.digitisation_step = cms.Path(process.pdigi_valid)
process.L1TrackTrigger_step = cms.Path(process.L1TrackTrigger)
process.L1simulation_step = cms.Path(process.SimL1Emulator)
process.Phase2L1GTProducer = cms.Path(process.l1tGTProducerSequence)
process.Phase2L1GTAlgoBlockProducer = cms.Path(process.l1tGTAlgoBlockProducerSequence)
process.pDoubleEGEle37_24 = cms.Path(process.DoubleEGEle3724)
process.pDoubleIsoTkPho22_12 = cms.Path(process.DoubleIsoTkPho2212)
process.pDoublePuppiJet112_112 = cms.Path(process.DoublePuppiJet112112)
process.pDoublePuppiJet160_35_mass620 = cms.Path(process.DoublePuppiJet16035Mass620)
process.pDoublePuppiTau52_52 = cms.Path(process.DoublePuppiTau5252)
process.pDoubleTkEle25_12 = cms.Path(process.DoubleTkEle2512)
process.pDoubleTkElePuppiHT_8_8_390 = cms.Path(process.DoubleTkElePuppiHT)
process.pDoubleTkMuPuppiHT_3_3_300 = cms.Path(process.DoubleTkMuPuppiHT)
process.pDoubleTkMuPuppiJetPuppiMet_3_3_60_130 = cms.Path(process.DoubleTkMuPuppiJetPuppiMet)
process.pDoubleTkMuon15_7 = cms.Path(process.DoubleTkMuon157)
process.pDoubleTkMuonTkEle5_5_9 = cms.Path(process.DoubleTkMuonTkEle559)
process.pDoubleTkMuon_4_4_OS_Dr1p2 = cms.Path(process.DoubleTkMuon44OSDr1p2)
process.pDoubleTkMuon_4p5_4p5_OS_Er2_Mass7to18 = cms.Path(process.DoubleTkMuon4p5OSEr2Mass7to18)
process.pDoubleTkMuon_OS_Er1p5_Dr1p4 = cms.Path(process.DoubleTkMuonOSEr1p5Dr1p4)
process.pIsoTkEleEGEle22_12 = cms.Path(process.IsoTkEleEGEle2212)
process.pNNPuppiTauPuppiMet_55_190 = cms.Path(process.NNPuppiTauPuppiMet)
process.pPuppiHT400 = cms.Path(process.PuppiHT400)
process.pPuppiHT450 = cms.Path(process.PuppiHT450)
process.pPuppiMET200 = cms.Path(process.PuppiMET200)
process.pPuppiMHT140 = cms.Path(process.PuppiMHT140)
process.pPuppiTauTkIsoEle45_22 = cms.Path(process.PuppiTauTkIsoEle4522)
process.pPuppiTauTkMuon42_18 = cms.Path(process.PuppiTauTkMuon4218)
process.pQuadJet70_55_40_40 = cms.Path(process.QuadJet70554040)
process.pSingleEGEle51 = cms.Path(process.SingleEGEle51)
process.pSingleIsoTkEle28 = cms.Path(process.SingleIsoTkEle28)
process.pSingleIsoTkPho36 = cms.Path(process.SingleIsoTkPho36)
process.pSinglePuppiJet230 = cms.Path(process.SinglePuppiJet230)
process.pSingleTkEle36 = cms.Path(process.SingleTkEle36)
process.pSingleTkMuon22 = cms.Path(process.SingleTkMuon22)
process.pTkEleIsoPuppiHT_26_190 = cms.Path(process.TkEleIsoPuppiHT)
process.pTkElePuppiJet_28_40_MinDR = cms.Path(process.TkElePuppiJetMinDR)
process.pTkEleTkMuon10_20 = cms.Path(process.TkEleTkMuon1020)
process.pTkMuPuppiJetPuppiMet_3_110_120 = cms.Path(process.TkMuPuppiJetPuppiMet)
process.pTkMuTriPuppiJet_12_40_dRMax_DoubleJet_dEtaMax = cms.Path(process.TkMuTriPuppiJetdRMaxDoubleJetdEtaMax)
process.pTkMuonDoubleTkEle6_17_17 = cms.Path(process.TkMuonDoubleTkEle61717)
process.pTkMuonPuppiHT6_320 = cms.Path(process.TkMuonPuppiHT6320)
process.pTkMuonTkEle7_23 = cms.Path(process.TkMuonTkEle723)
process.pTkMuonTkIsoEle7_20 = cms.Path(process.TkMuonTkIsoEle720)
process.pTripleTkMuon5_3_3 = cms.Path(process.TripleTkMuon533)
process.pTripleTkMuon_5_3_0_DoubleTkMuon_5_3_OS_MassTo9 = cms.Path(process.TripleTkMuon530OSMassMax9)
process.pTripleTkMuon_5_3p5_2p5_OS_Mass5to17 = cms.Path(process.TripleTkMuon53p52p5OSMass5to17)
process.digi2raw_step = cms.Path(process.DigiToRaw)
process.endjob_step = cms.EndPath(process.endOfProcess)
process.FEVTDEBUGHLToutput_step = cms.EndPath(process.FEVTDEBUGHLToutput)

# Schedule definition
# process.schedule imported from cff in HLTrigger.Configuration
process.schedule.insert(0, process.digitisation_step)
process.schedule.insert(1, process.L1TrackTrigger_step)
process.schedule.insert(2, process.L1simulation_step)
process.schedule.insert(3, process.Phase2L1GTProducer)
process.schedule.insert(4, process.Phase2L1GTAlgoBlockProducer)
process.schedule.insert(5, process.pDoubleEGEle37_24)
process.schedule.insert(6, process.pDoubleIsoTkPho22_12)
process.schedule.insert(7, process.pDoublePuppiJet112_112)
process.schedule.insert(8, process.pDoublePuppiJet160_35_mass620)
process.schedule.insert(9, process.pDoublePuppiTau52_52)
process.schedule.insert(10, process.pDoubleTkEle25_12)
process.schedule.insert(11, process.pDoubleTkElePuppiHT_8_8_390)
process.schedule.insert(12, process.pDoubleTkMuPuppiHT_3_3_300)
process.schedule.insert(13, process.pDoubleTkMuPuppiJetPuppiMet_3_3_60_130)
process.schedule.insert(14, process.pDoubleTkMuon15_7)
process.schedule.insert(15, process.pDoubleTkMuonTkEle5_5_9)
process.schedule.insert(16, process.pDoubleTkMuon_4_4_OS_Dr1p2)
process.schedule.insert(17, process.pDoubleTkMuon_4p5_4p5_OS_Er2_Mass7to18)
process.schedule.insert(18, process.pDoubleTkMuon_OS_Er1p5_Dr1p4)
process.schedule.insert(19, process.pIsoTkEleEGEle22_12)
process.schedule.insert(20, process.pNNPuppiTauPuppiMet_55_190)
process.schedule.insert(21, process.pPuppiHT400)
process.schedule.insert(22, process.pPuppiHT450)
process.schedule.insert(23, process.pPuppiMET200)
process.schedule.insert(24, process.pPuppiMHT140)
process.schedule.insert(25, process.pPuppiTauTkIsoEle45_22)
process.schedule.insert(26, process.pPuppiTauTkMuon42_18)
process.schedule.insert(27, process.pQuadJet70_55_40_40)
process.schedule.insert(28, process.pSingleEGEle51)
process.schedule.insert(29, process.pSingleIsoTkEle28)
process.schedule.insert(30, process.pSingleIsoTkPho36)
process.schedule.insert(31, process.pSinglePuppiJet230)
process.schedule.insert(32, process.pSingleTkEle36)
process.schedule.insert(33, process.pSingleTkMuon22)
process.schedule.insert(34, process.pTkEleIsoPuppiHT_26_190)
process.schedule.insert(35, process.pTkElePuppiJet_28_40_MinDR)
process.schedule.insert(36, process.pTkEleTkMuon10_20)
process.schedule.insert(37, process.pTkMuPuppiJetPuppiMet_3_110_120)
process.schedule.insert(38, process.pTkMuTriPuppiJet_12_40_dRMax_DoubleJet_dEtaMax)
process.schedule.insert(39, process.pTkMuonDoubleTkEle6_17_17)
process.schedule.insert(40, process.pTkMuonPuppiHT6_320)
process.schedule.insert(41, process.pTkMuonTkEle7_23)
process.schedule.insert(42, process.pTkMuonTkIsoEle7_20)
process.schedule.insert(43, process.pTripleTkMuon5_3_3)
process.schedule.insert(44, process.pTripleTkMuon_5_3_0_DoubleTkMuon_5_3_OS_MassTo9)
process.schedule.insert(45, process.pTripleTkMuon_5_3p5_2p5_OS_Mass5to17)
process.schedule.insert(46, process.digi2raw_step)
process.schedule.extend([process.endjob_step,process.FEVTDEBUGHLToutput_step])
from PhysicsTools.PatAlgos.tools.helpers import associatePatAlgosToolsTask
associatePatAlgosToolsTask(process)

# customisation of the process.

# Automatic addition of the customisation function from HLTrigger.Configuration.customizeHLTforMC
from HLTrigger.Configuration.customizeHLTforMC import customizeHLTforMC 

#call to customisation function customizeHLTforMC imported from HLTrigger.Configuration.customizeHLTforMC
process = customizeHLTforMC(process)

# End of customisation functions


# Customisation from command line

# Add early deletion of temporary data products to reduce peak memory need
from Configuration.StandardSequences.earlyDeleteSettings_cff import customiseEarlyDelete
process = customiseEarlyDelete(process)
# End adding early deletion
