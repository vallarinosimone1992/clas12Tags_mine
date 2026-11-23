#include "../hitprocess/mine/mine_geant4_tracking.h"
#include <cstdlib>
/// \file ActionInitialization.cc
/// \brief Implementation of the ActionInitialization class

// gemc
#include "ActionInitialization.h"

ActionInitialization::ActionInitialization(goptions* go, map<string, double> *gPars) : G4VUserActionInitialization()
{
	genAction = new MPrimaryGeneratorAction(go);
	evtAction = new MEventAction(*go, *gPars);
    if (gPars->count("mine_light_yield")) {
        mineRunAction = new MineRunAction();
        evtAction->SetMineRunAction(mineRunAction);
    }
	stpAction = new MSteppingAction(*go);
    const char* mineMode=std::getenv("MINE_RESPONSE_MODE");
    mineOpticalDisplay=go->optMap["USE_GUI"].arg && mineMode && std::string(mineMode)=="geant4";
}


ActionInitialization::~ActionInitialization()
{}


void ActionInitialization::BuildForMaster() const
{
	// SetUserAction(new RunAction);
}


void ActionInitialization::Build() const
{
	SetUserAction(genAction);
	SetUserAction(evtAction);
    if (mineRunAction) SetUserAction(mineRunAction);
	SetUserAction(stpAction);
    if(mineOpticalDisplay)
        SetUserAction(mine::MakeOpticalVisualizationTracking());
}

