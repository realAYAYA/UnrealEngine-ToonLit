// Copyright Epic Games, Inc. All Rights Reserved.

#include "RequiredProgramMainCPPInclude.h"

//typical defined by TargetRules but LowLevelTestRunner is not setup correctly
//should revist this in the future
#ifndef IMPLEMENT_ENCRYPTION_KEY_REGISTRATION
	#define IMPLEMENT_ENCRYPTION_KEY_REGISTRATION()
#endif
#ifndef IMPLEMENT_SIGNING_KEY_REGISTRATION
	#define IMPLEMENT_SIGNING_KEY_REGISTRATION()
#endif

IMPLEMENT_APPLICATION(LowLevelTests, "LowLevelTests");
