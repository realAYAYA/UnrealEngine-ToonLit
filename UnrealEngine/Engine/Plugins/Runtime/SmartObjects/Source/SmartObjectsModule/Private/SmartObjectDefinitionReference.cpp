// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinitionReference.h"
#include "SmartObjectDefinition.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectDefinitionReference)

void FSmartObjectDefinitionReference::SyncParameters()
{
	if (SmartObjectDefinition == nullptr)
	{
		Parameters.Reset();
	}
	else
	{
		// In editor builds, sync with overrides.
		Parameters.MigrateToNewBagInstanceWithOverrides(SmartObjectDefinition->GetDefaultParameters(), PropertyOverrides);
		
		// Remove overrides that do not exists anymore
		if (!PropertyOverrides.IsEmpty())
		{
			if (const UPropertyBag* Bag = Parameters.GetPropertyBagStruct())
			{
				for (TArray<FGuid>::TIterator It = PropertyOverrides.CreateIterator(); It; ++It)
				{
					if (!Bag->FindPropertyDescByID(*It))
					{
						It.RemoveCurrentSwap();
					}
				}
			}
		}
	}
}

bool FSmartObjectDefinitionReference::RequiresParametersSync() const
{
	bool bShouldSync = false;
	
	if (SmartObjectDefinition)
	{
		const FInstancedPropertyBag& DefaultParameters = SmartObjectDefinition->GetDefaultParameters();
		const UPropertyBag* DefaultParametersBag = DefaultParameters.GetPropertyBagStruct();
		const UPropertyBag* ParametersBag = Parameters.GetPropertyBagStruct();
		
		// Mismatching property bags, needs sync.
		if (DefaultParametersBag != ParametersBag)
		{
			bShouldSync = true;
		}
		else if (ParametersBag && DefaultParametersBag)
		{
			// Check if non-overridden parameters are not identical, needs sync.
			const uint8* SourceAddress = DefaultParameters.GetValue().GetMemory();
			const uint8* TargetAddress = Parameters.GetValue().GetMemory();
			check(SourceAddress);
			check(TargetAddress);

			for (const FPropertyBagPropertyDesc& Desc : ParametersBag->GetPropertyDescs())
			{
				// Skip overridden
				if (PropertyOverrides.Contains(Desc.ID))
				{
					continue;
				}

				const uint8* SourceValueAddress = SourceAddress + Desc.CachedProperty->GetOffset_ForInternal();
				const uint8* TargetValueAddress = TargetAddress + Desc.CachedProperty->GetOffset_ForInternal();
				if (!Desc.CachedProperty->Identical(SourceValueAddress, TargetValueAddress))
				{
					// Mismatching values, should sync.
					bShouldSync = true;
					break;
				}
			}
		}
	}
	else
	{
		// Empty state tree should not have parameters
		bShouldSync = Parameters.IsValid();
	}
	
	return bShouldSync;
}

void FSmartObjectDefinitionReference::ConditionallySyncParameters() const
{
	if (RequiresParametersSync())
	{
		FSmartObjectDefinitionReference* NonConstThis = const_cast<FSmartObjectDefinitionReference*>(this);
		NonConstThis->SyncParameters();
		UE_LOG(LogSmartObject, Warning, TEXT("Parameters for '%s' stored in SmartObjectDefinitionReference were auto-fixed to be usable at runtime."), *GetNameSafe(SmartObjectDefinition));	
	}
}

void FSmartObjectDefinitionReference::SetPropertyOverridden(const FGuid PropertyID, const bool bIsOverridden)
{
	if (bIsOverridden)
	{
		PropertyOverrides.AddUnique(PropertyID);
	}
	else
	{
		PropertyOverrides.Remove(PropertyID);
		ConditionallySyncParameters();
	}
}

bool FSmartObjectDefinitionReference::SerializeFromMismatchedTag(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot)
{
	// Serialize from an object pointer.
	if (Tag.Type == NAME_ObjectProperty)
	{
		Slot << SmartObjectDefinition;
		return true;
	}
	
	return false;
}
