// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ICastable.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/CameraCutTrackModel.h"
#include "MVVM/ViewModels/CinematicShotTrackModel.h"
#include "MVVM/ViewModels/BindingLifetimeTrackModel.h"


namespace UE
{
namespace Sequencer
{

// Model types
UE_SEQUENCER_DEFINE_CASTABLE(FCameraCutTrackModel);
UE_SEQUENCER_DEFINE_CASTABLE(FCinematicShotTrackModel);
UE_SEQUENCER_DEFINE_CASTABLE(FBindingLifetimeTrackModel);

} // namespace Sequencer
} // namespace UE

