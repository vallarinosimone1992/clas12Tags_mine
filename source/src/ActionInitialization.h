#ifndef ActionInitialization_h
#define ActionInitialization_h 1

// gemc
#include "MPrimaryGeneratorAction.h"
#include "MEventAction.h"
#include "MineRunAction.h"
#include "MSteppingAction.h"
#include "gemcOptions.h"


// geant4
#include "G4VUserActionInitialization.hh"


/// Action initialization class.
class ActionInitialization : public G4VUserActionInitialization
{
public:
	ActionInitialization(goptions *go, map<string, double> *gPars);
	virtual ~ActionInitialization();
	
	virtual void BuildForMaster() const;
	virtual void Build() const;
	
	
	bool mineOpticalDisplay=false;
	MPrimaryGeneratorAction *genAction;
	MEventAction            *evtAction;
    MineRunAction          *mineRunAction = nullptr;
	MSteppingAction         *stpAction;
};


#endif


