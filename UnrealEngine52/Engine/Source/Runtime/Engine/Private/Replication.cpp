// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	Replication.cpp: Engine actor replication implementation
=============================================================================*/

#include "Camera/PlayerCameraManager.h"

//
// Static variables for networking.
//

/** @hack: saves and restores fade state for a PC when it goes out of scope
 * used for fade track hack below */
struct FSavedFadeState
{
public:
	FSavedFadeState(APlayerCameraManager* InCamera)
		: Camera(InCamera), bEnableFading(InCamera->bEnableFading), FadeAmount(InCamera->FadeAmount), FadeTimeRemaining(InCamera->FadeTimeRemaining)
	{}
	~FSavedFadeState()
	{
		Camera->bEnableFading = bEnableFading;
		Camera->FadeAmount = FadeAmount;
		Camera->FadeTimeRemaining = FadeTimeRemaining;
	}
private:
	APlayerCameraManager* Camera;
	bool bEnableFading;
	float FadeAmount;
	float FadeTimeRemaining;
};

