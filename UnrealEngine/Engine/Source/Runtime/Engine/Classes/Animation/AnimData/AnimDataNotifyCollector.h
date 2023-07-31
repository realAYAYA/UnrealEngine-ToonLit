// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimData/AnimDataNotifications.h"
#include "Containers/Set.h"
#include "Containers/Array.h"

enum class EAnimDataModelNotifyType : uint8;

namespace UE {
namespace Anim {

#if WITH_EDITOR

/** Helper structure for keeping track of which notifies of type EAnimDataModelNotifType are broadcasted
between top-level EAnimDataModelNotifType::BracketOpened and EAnimDataModelNotifType::BracketClosed notifies */
struct ENGINE_API FAnimationDataNotifyCollector
{
	FAnimationDataNotifyCollector() : BracketDepth(0) {}

	/** Handle a broadcasted notify, reset if we are opening a new top-level bracket*/
	void Handle(EAnimDataModelNotifType NotifyType)
	{
		NotifyTypes.Add(NotifyType);

		if (NotifyType == EAnimDataModelNotifType::BracketOpened)
		{
			if (BracketDepth == 0)
			{
				Reset();
			}

			++BracketDepth;
		}
		else if (NotifyType == EAnimDataModelNotifType::BracketClosed)
		{
			--BracketDepth;
		}
	}
	
	/** Returns whether or not the notify of the provided types was broadcasted */
	bool Contains(EAnimDataModelNotifType NotifyType) const
	{
		return NotifyTypes.Find(NotifyType) != nullptr;
	}

	/** Returns whether or not any of the provided notify types were broadcasted */
	bool Contains(const TArray<EAnimDataModelNotifType>& TestNotifyTypes) const
	{
		for (EAnimDataModelNotifType Notify : TestNotifyTypes)
		{
			if (NotifyTypes.Find(Notify) != nullptr)
			{
				return true;
			}
		}

		return false;
	}

	/** Returns whether or not a bracket is still open */
	bool IsWithinBracket() const { return BracketDepth > 0; }

	/** Returns whether or not all brackets have been closed */
	bool IsNotWithinBracket() const { return BracketDepth == 0; }
protected:
	void Reset()
	{
		NotifyTypes.Empty();
	}
protected:
	TSet<EAnimDataModelNotifType> NotifyTypes;
	int32 BracketDepth;
};

#endif // WITH_EDITOR

} // namespace Anim	

} // namespace UE