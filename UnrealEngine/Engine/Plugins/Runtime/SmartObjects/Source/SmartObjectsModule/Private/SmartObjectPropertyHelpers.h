// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "Misc/EnumerateRange.h"

/**
 * Helper class to deal with relative property paths in PostEditChangeChainProperty().
 * @todo: Make this more widely available, this is copy from State Tree. 
 */
struct FSmartObjectEditPropertyPath
{
private:
	struct FStateTreeEditPropertySegment
	{
		FStateTreeEditPropertySegment() = default;
		FStateTreeEditPropertySegment(const FProperty* InProperty, const FName InPropertyName, const int32 InArrayIndex = INDEX_NONE)
			: Property(InProperty)
			, PropertyName(InPropertyName)
			, ArrayIndex(InArrayIndex)
		{
		}
	
		const FProperty* Property = nullptr;
		FName PropertyName = FName();
		int32 ArrayIndex = INDEX_NONE;
	};
	
public:
	FSmartObjectEditPropertyPath() = default;

	/** Makes property path relative to BaseStruct. Checks if the path is not part of the type. */
	explicit FSmartObjectEditPropertyPath(const UStruct* BaseStruct, const FString& InPath)
	{
		TArray<FString> PathSegments;
		InPath.ParseIntoArray(PathSegments, TEXT("."));

		const UStruct* CurrBase = BaseStruct;
		for (const FString& Segment : PathSegments)
		{
			check(CurrBase);
			const FName PropertyName(Segment);
			if (const FProperty* Property = CurrBase->FindPropertyByName(PropertyName))
			{
				Path.Emplace(Property, PropertyName);

				if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					Property = ArrayProperty->Inner;
				}

				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					CurrBase = StructProperty->Struct;
				}
				else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					CurrBase = ObjectProperty->PropertyClass;
				}
			}
			else
			{
				checkf(false, TEXT("Path %s id not part of type %s."), *InPath, *GetNameSafe(BaseStruct));
				Path.Reset();
				break;
			}
		}
	}

	/** Makes property path from property change event. */
	explicit FSmartObjectEditPropertyPath(const FPropertyChangedChainEvent& PropertyChangedEvent)
	{
		FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
		while (PropertyNode != nullptr)
		{
			if (FProperty* Property = PropertyNode->GetValue())
			{
				const FName PropertyName = Property->GetFName(); 
				const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(PropertyName.ToString());
				Path.Emplace(Property, PropertyName, ArrayIndex);
			}
			PropertyNode = PropertyNode->GetNextNode();
		}
	}

	/** @return true if the property path contains specified path. */
	bool ContainsPath(const FSmartObjectEditPropertyPath& InPath) const
	{
		if (InPath.Path.Num() > Path.Num())
    	{
    		return false;
    	}

    	for (TConstEnumerateRef<FStateTreeEditPropertySegment> Segment : EnumerateRange(InPath.Path))
    	{
    		if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
    		{
    			return false;
    		}
    	}
    	return true;
	}

	/** @return true if the property path is exactly the specified path. */
	bool IsPathExact(const FSmartObjectEditPropertyPath& InPath) const
	{
		if (InPath.Path.Num() != Path.Num())
		{
			return false;
		}

		for (TConstEnumerateRef<FStateTreeEditPropertySegment> Segment : EnumerateRange(InPath.Path))
		{
			if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
			{
				return false;
			}
		}
		return true;
	}

	/** @return array index at specified property, or INDEX_NONE, if the property is not array or property not found.  */
	int32 GetPropertyArrayIndex(const FSmartObjectEditPropertyPath& InPath) const
	{
		return ContainsPath(InPath) ? Path[InPath.Path.Num() - 1].ArrayIndex : INDEX_NONE;
	}

private:
	TArray<FStateTreeEditPropertySegment> Path;
};
