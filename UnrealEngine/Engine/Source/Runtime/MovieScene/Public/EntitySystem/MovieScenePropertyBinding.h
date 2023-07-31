// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "EntitySystem/MovieSceneComponentDebug.h"

#include "MovieScenePropertyBinding.generated.h"


/**
 * Source property binding information for an entity on a moviescene timeline
 * Comprises a leaf property name and a path and a cached boolean signifying whether the binding is allowed to perform a fast class-wise property lookup
 */
USTRUCT()
struct FMovieScenePropertyBinding
{
	GENERATED_BODY()

	FMovieScenePropertyBinding()
		: bCanUseClassLookup(false)
	{}

	FMovieScenePropertyBinding(FName InPropertyName, const FString& InPropertyPath)
		: PropertyName(InPropertyName), PropertyPath(*InPropertyPath)
	{
		bCanUseClassLookup = !(InPropertyPath.Contains(TEXT(".")) || InPropertyPath.Contains(TEXT("/")) || InPropertyPath.Contains(TEXT("\\")) || InPropertyPath.Contains(TEXT("[")));
	}

	static FMovieScenePropertyBinding FromPath(const FString& InPropertyPath)
	{
		FName PropertyName;

		int32 NamePos = INDEX_NONE;
		if (InPropertyPath.FindLastChar('.', NamePos) || InPropertyPath.FindLastChar('/', NamePos) || InPropertyPath.FindLastChar('\\', NamePos))
		{
			PropertyName = FName(FStringView(*InPropertyPath + NamePos, InPropertyPath.Len() - NamePos));
		}
		else
		{
			PropertyName = *InPropertyPath;
		}
		return FMovieScenePropertyBinding(PropertyName, InPropertyPath);
	}

	friend bool operator==(FMovieScenePropertyBinding A, FMovieScenePropertyBinding B)
	{
		return A.PropertyName == B.PropertyName && A.PropertyPath == B.PropertyPath;
	}

	bool CanUseClassLookup() const
	{
		return bCanUseClassLookup;
	}

	/** Leaf name of the property to animate */
	UPROPERTY()
	FName PropertyName;

	/** Full path to the property from the object including struct and array indirection */
	UPROPERTY()
	FName PropertyPath;

	/** True if this property can be considered for fast property offset resolution(ie the property address is _always_ a constant offset from the obejct ptr), false otherwise */
	UPROPERTY()
	bool bCanUseClassLookup;
};



#if UE_MOVIESCENE_ENTITY_DEBUG

namespace UE
{
namespace MovieScene
{

	template<> struct TComponentDebugType<FMovieScenePropertyBinding>
	{
		static const EComponentDebugType Type = EComponentDebugType::Property;
	};

} // namespace MovieScene
} // namespace UE

#endif // UE_MOVIESCENE_ENTITY_DEBUG