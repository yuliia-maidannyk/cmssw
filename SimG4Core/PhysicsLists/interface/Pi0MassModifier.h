#ifndef Pi0MassModifier_h
#define Pi0MassModifier_h

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "G4VPhysicsConstructor.hh"

class Pi0MassModifier : public G4VPhysicsConstructor {
public:
    explicit Pi0MassModifier(const edm::ParameterSet& p);
    ~Pi0MassModifier() override = default;

    void ConstructParticle() override;
    void ConstructProcess() override;
private:
    double mass_;
};

#endif