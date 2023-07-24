//
// Copyright (C) Valve Corporation. All rights reserved.
//

#pragma once

#include "Delegates/Delegate.h"
#include "CoreMinimal.h"

class UPhononSourceComponent;

namespace SteamAudio
{
	// True if a baking process is currently running.
	extern TAtomic<bool> GIsBaking;

	DECLARE_DELEGATE_OneParam(FBakedSourceUpdated, FName);
	
	void Bake(const TArray<UPhononSourceComponent*> PhononSourceComponents, const bool BakeReverb, SteamAudio::FBakedSourceUpdated BakedSourceUpdated);
}
