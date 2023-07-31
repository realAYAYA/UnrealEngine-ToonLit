// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ChildActorComponent.h"
#include "InheritedSubobjectData.h"
#include "Internationalization/Text.h"

class UObject;
struct FSubobjectDataHandle;

/**
 * Container that represents the subobejct data of a child actor component.
 */
struct SUBOBJECTDATAINTERFACE_API FChildActorSubobjectData final : public FInheritedSubobjectData
{
	explicit FChildActorSubobjectData(UObject* ContextObject, const FSubobjectDataHandle& ParentHandle, const bool InbIsInheritedSCS);

	// FSubobjectData interface
	virtual FText GetDisplayName() const override;
	virtual FText GetActorDisplayText() const override;
	virtual bool IsChildActor() const override;
	// End FSubobjectData

	inline const UChildActorComponent* GetChildActorComponent(bool bEvenIfPendingKill = false) const
	{
		return GetObject<UChildActorComponent>(bEvenIfPendingKill);
	}
};