// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"


class UReplicationSystem;

namespace UE::Net
{

DECLARE_MULTICAST_DELEGATE_OneParam(FIrisCriticalErrorDetected, UReplicationSystem*);

class FIrisDelegates
{
public:
	IRISCORE_API static FIrisCriticalErrorDetected& GetCriticalErrorDetectedDelegate();
};

	
	

} // end namespace UE::Net

