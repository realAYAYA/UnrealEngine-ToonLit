// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EBreakBehavior.h"
#include "Replication/Editor/Model/Property/IPropertySourceModel.h"

#include "HAL/Platform.h"
#include "UObject/WeakObjectPtr.h"

class UClass;

namespace UE::ConcertClientSharedSlate
{
	/** The allowed properties are those returned by UE::ConcertSyncCore::ForEachReplicatableProperty.*/
	class CONCERTCLIENTSHAREDSLATE_API FConcertSyncCoreReplicatedPropertySource : public ConcertSharedSlate::IPropertySourceModel
	{
	public:

		void SetClass(UClass* InClass);
		
		//~ Begin IPropertySourceModel Interface
		virtual ConcertSharedSlate::FSourceDisplayInfo GetDisplayInfo() const override;
		virtual uint32 GetNumSelectableItems() const override { return NumProperties; }
		virtual void EnumerateSelectableItems(TFunctionRef<EBreakBehavior(const ConcertSharedSlate::FSelectablePropertyInfo& SelectableOption)> Delegate) const override;
		//~ End IPropertySourceModel Interface

	private:

		/** Weak ptr because it could be a Blueprint class that can be destroyed at any time. */
		TWeakObjectPtr<UClass> Class;

		int32 NumProperties = 0;
	};
}
