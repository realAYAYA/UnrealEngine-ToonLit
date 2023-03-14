// Copyright Epic Games, Inc. All Rights Reserved.

#include "Net/Core/PropertyConditions/RepChangedPropertyTracker.h"
#include "Net/Core/PushModel/PushModel.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FRepChangedPropertyTracker::FRepChangedPropertyTracker(FCustomPropertyConditionState&& InActiveState)
	: ActiveState(MoveTemp(InActiveState))
	, ExternalDataNumBits(0)
{
}

FRepChangedPropertyTracker::FRepChangedPropertyTracker(const bool InbIsReplay, const bool InbIsClientReplayRecording)
	: FRepChangedPropertyTracker(0)
{}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

namespace UE::Net::Private
{
#if UE_WITH_IRIS
static FIrisSetPropertyCustomCondition IrisSetPropertyCustomConditionDelegate;

void SetIrisSetPropertyCustomConditionDelegate(const FIrisSetPropertyCustomCondition& Delegate)
{
	IrisSetPropertyCustomConditionDelegate = Delegate;
}
#endif // UE_WITH_IRIS
}

void FRepChangedPropertyTracker::SetCustomIsActiveOverride(UObject* OwningObject, const uint16 RepIndex, const bool bIsActive)
{
	const bool bOldActive = ActiveState.GetActiveState(RepIndex);
	ActiveState.SetActiveState(RepIndex, bIsActive);	// check for client replay recording moved to FReplicationFlags

#if WITH_PUSH_MODEL
	if (!bOldActive && bIsActive)
	{
		MARK_PROPERTY_DIRTY_UNSAFE(OwningObject, RepIndex);
	}
#endif

#if UE_WITH_IRIS
	if (bOldActive != bIsActive && UE::Net::Private::IrisSetPropertyCustomConditionDelegate.IsBound())
	{
		UE::Net::Private::IrisSetPropertyCustomConditionDelegate.Execute(OwningObject, RepIndex, bIsActive);
	}
#endif // UE_WITH_IRIS
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
void FRepChangedPropertyTracker::SetExternalData(const uint8* Src, const int32 NumBits)
{
	ExternalDataNumBits = NumBits;
	const int32 NumBytes = (NumBits + 7) >> 3;
	ExternalData.Reset(NumBytes);
	ExternalData.AddUninitialized(NumBytes);
	FMemory::Memcpy(ExternalData.GetData(), Src, NumBytes);
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FRepChangedPropertyTracker::CountBytes(FArchive& Ar) const
{
	// Include our size here, because the caller won't know.
	Ar.CountBytes(sizeof(FRepChangedPropertyTracker), sizeof(FRepChangedPropertyTracker));
	ActiveState.CountBytes(Ar);

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ExternalData.CountBytes(Ar);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
