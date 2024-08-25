// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Core/IrisDelegates.h"


namespace UE::Net
{

FIrisCriticalErrorDetected& FIrisDelegates::GetCriticalErrorDetectedDelegate()
{
	static FIrisCriticalErrorDetected CriticalErrorDelegate;
	return CriticalErrorDelegate;
}

} // end namespace UE::Net

