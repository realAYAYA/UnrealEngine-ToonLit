// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertySourceModel.h"
#include "Model/Item/SourceSelectionCategory.h"
#include "Templates/SharedPointer.h"

namespace UE::ConcertSharedSlate
{
	using FPropertySourceCategory = ConcertSharedSlate::TSourceSelectionCategory<FSelectablePropertyInfo>;
	
	/** Decides which properties can be added to a IReplicationStreamModel. */
	class IPropertySelectionSourceModel : public TSharedFromThis<IPropertySelectionSourceModel>
	{
	public:

		/** Gets the single source determining which properties can be selected. */
		virtual TSharedRef<IPropertySourceModel> GetPropertySource(const FSoftClassPath& Class) const = 0;
		
		virtual ~IPropertySelectionSourceModel() = default;
	};
}
