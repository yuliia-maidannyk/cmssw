#include "SimG4Core/PhysicsLists/interface/Pi0MassModifier.h"
#include "G4ParticleTable.hh"
#include "G4ParticleDefinition.hh"
#include "G4DecayTable.hh"
#include "G4PhaseSpaceDecayChannel.hh"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "G4Decay.hh"
#include "G4ProcessManager.hh"
#include "G4ProcessVector.hh"
#include "G4VProcess.hh"

Pi0MassModifier::Pi0MassModifier(const edm::ParameterSet& p)
  : G4VPhysicsConstructor("Pi0MassModifier") 
{
    mass_ = p.getUntrackedParameter<double>("massGeV", 2.0);
}

void Pi0MassModifier::ConstructParticle() {
    auto particleTable = G4ParticleTable::GetParticleTable();

    // Check if particle already exists
    G4ParticleDefinition* existing = particleTable->FindParticle("X");
    if (existing) return;

    // Get standard pi0 properties to copy
    G4ParticleDefinition* pi0 = particleTable->FindParticle("pi0");

    if (!pi0) {
        edm::LogWarning("SimG4Core") << "pi0 not found!";
        return;
    }

    // Create the particle first, then attach decay channels.
    auto* x = new G4ParticleDefinition(
        "X",             // name
        mass_*CLHEP::GeV,      // mass
        pi0->GetPDGWidth(),    // width
        pi0->GetPDGCharge(),   // charge
        pi0->GetPDGSpin(),     // spin
        pi0->GetPDGiParity(),   // parity
        pi0->GetPDGiConjugation(), // C-conjugation
        pi0->GetPDGIsospin(),     // isospin
        pi0->GetPDGIsospin3(),    // isospin3
        0,     // G-parity
        pi0->GetParticleType(),// type
        pi0->GetLeptonNumber(),// lepton number
        pi0->GetBaryonNumber(),// baryon number
        9000001,                // PDG code
        false,                 // stable
        pi0->GetPDGLifeTime(), // lifetime
        nullptr               // decay table
        // false,                 // short-lived
        // pi0->GetParticleSubType(),     // sub-type
        // 0,                     // anti-encoding
        // 0                      // magnetic moment (0 = default)
    );

    G4DecayTable* decayTable = new G4DecayTable();
    decayTable->Insert(new G4PhaseSpaceDecayChannel("X", 1.0, 2, "gamma", "gamma"));
    x->SetDecayTable(decayTable);

    edm::LogInfo("SimG4Core") << "Created X with mass " << mass_ << " GeV";
}

void Pi0MassModifier::ConstructProcess() {
    auto particleTable = G4ParticleTable::GetParticleTable();
    auto p = particleTable->FindParticle("X");

    if (!p) return;

    auto pm = p->GetProcessManager();
    if (!pm) return;

    // G4DecayPhysics may already have attached decay to X.
    auto* plist = pm->GetProcessList();
    if (plist) {
        for (size_t i = 0; i < static_cast<size_t>(pm->GetProcessListLength()); ++i) {
            G4VProcess* proc = (*plist)[i];
            if (proc && proc->GetProcessName() == "Decay") {
                return;
            }
        }
    }

    G4Decay* decay = new G4Decay();
    pm->AddProcess(decay);
    pm->SetProcessOrdering(decay, idxPostStep);
    pm->SetProcessOrdering(decay, idxAtRest);
}