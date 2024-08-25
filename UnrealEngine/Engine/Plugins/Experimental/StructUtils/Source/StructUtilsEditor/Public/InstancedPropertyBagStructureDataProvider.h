// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IStructureDataProvider.h"
#include "PropertyBag.h"

//	FInstancePropertyBagStructureDataProvider
//	Allows structure views to use FInstancedPropertyBag even if the bag layout changes.
//	The caller needs to make sure that property bag outlives the property view widget.
class FInstancePropertyBagStructureDataProvider : public IStructureDataProvider
{
public:
	FInstancePropertyBagStructureDataProvider(FInstancedPropertyBag& InPropertyBag)
		: PropertyBag(InPropertyBag)
	{
	}

	virtual bool IsValid() const override
	{
		return PropertyBag.IsValid();
	};
	
	virtual const UStruct* GetBaseStructure() const override
	{
		return PropertyBag.IsValid() ? PropertyBag.GetPropertyBagStruct() : nullptr;
	}
	
	virtual void GetInstances(TArray<TSharedPtr<FStructOnScope>>& OutInstances, const UStruct* ExpectedBaseStructure) const override
	{
		if (PropertyBag.IsValid())
		{
			const UStruct* Struct = PropertyBag.GetPropertyBagStruct();
			if (ExpectedBaseStructure && Struct && Struct == ExpectedBaseStructure)
			{
				OutInstances.Add(MakeShared<FStructOnScope>(Struct, PropertyBag.GetMutableValue().GetMemory()));
			}
		}
	}

protected:
	FInstancedPropertyBag& PropertyBag;
};