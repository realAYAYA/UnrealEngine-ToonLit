// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/TaskGraphInterfaces.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "EntitySystem/EntityAllocationIterator.h"
#include "EntitySystem/MovieSceneComponentPtr.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "EntitySystem/MovieSceneSystemTaskDependencies.h"
#include "HAL/Platform.h"
#include "Misc/AssertionMacros.h"
#include "MovieSceneEntityIDs.h"
#include "Templates/Tuple.h"

#include <initializer_list>

namespace UE
{
namespace MovieScene
{
class FEntityManager;


struct FComponentAccess
{
	FComponentTypeID ComponentType;
};
struct FReadAccess : FComponentAccess
{
	FReadAccess(FComponentTypeID InComponentType) : FComponentAccess{ InComponentType } {}
};
struct FWriteAccess : FComponentAccess
{
	FWriteAccess(FComponentTypeID InComponentType) : FComponentAccess{ InComponentType } {}
};


struct FOptionalComponentAccess
{
	FComponentTypeID ComponentType;
};
struct FOptionalReadAccess : FOptionalComponentAccess
{
	FOptionalReadAccess(FComponentTypeID InComponentType) : FOptionalComponentAccess{ InComponentType } {}
};
struct FOptionalWriteAccess : FOptionalComponentAccess
{
	FOptionalWriteAccess(FComponentTypeID InComponentType) : FOptionalComponentAccess{ InComponentType } {}
};


struct FEntityIDAccess
{
	using AccessType = const FMovieSceneEntityID;

	FORCEINLINE TRead<FMovieSceneEntityID> LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return TRead<FMovieSceneEntityID>(Allocation->GetRawEntityIDs());
	}
};



template<typename T>
struct TReadAccess : FReadAccess
{
	using AccessType = const T;

	TReadAccess(FComponentTypeID InComponentTypeID)
		: FReadAccess{ InComponentTypeID }
	{}

	FORCEINLINE TComponentLock<TRead<T>> LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return Allocation->ReadComponents(ComponentType.ReinterpretCast<T>());
	}
};


struct FErasedReadAccess : FReadAccess
{
	FErasedReadAccess(FComponentTypeID InComponentTypeID)
		: FReadAccess{ InComponentTypeID }
	{}

	FORCEINLINE FComponentReader LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return Allocation->ReadComponentsErased(ComponentType);
	}
};


struct FErasedWriteAccess : FWriteAccess
{
	FErasedWriteAccess(FComponentTypeID InComponentTypeID)
		: FWriteAccess{ InComponentTypeID }
	{}

	FORCEINLINE FComponentWriter LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return Allocation->WriteComponentsErased(ComponentType, WriteContext);
	}
};




template<typename T>
struct TWriteAccess : FWriteAccess
{
	using AccessType = T;

	TWriteAccess(FComponentTypeID InComponentTypeID)
		: FWriteAccess{ InComponentTypeID }
	{}

	FORCEINLINE TComponentLock<TWrite<T>> LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return Allocation->WriteComponents(ComponentType.ReinterpretCast<T>(), WriteContext);
	}
};




template<typename T>
struct TOptionalReadAccess : FOptionalReadAccess
{
	using AccessType = const T;

	TOptionalReadAccess(FComponentTypeID InComponentTypeID)
		: FOptionalReadAccess{InComponentTypeID}
	{}

	FORCEINLINE TComponentLock<TReadOptional<T>> LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return Allocation->TryReadComponents(ComponentType.ReinterpretCast<T>());
	}
};




template<typename T>
struct TOptionalWriteAccess : FOptionalWriteAccess
{
	using AccessType = T;

	TOptionalWriteAccess(FComponentTypeID InComponentTypeID)
		: FOptionalWriteAccess{ InComponentTypeID }
	{}

	FORCEINLINE TComponentLock<TWriteOptional<T>> LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return Allocation->TryWriteComponents(ComponentType.ReinterpretCast<T>(), WriteContext);
	}
};




template<typename... T>
struct TReadOneOfAccessor
{
	using AccessType = TMultiComponentLock<TReadOptional<T>...>;

	TReadOneOfAccessor(TComponentTypeID<T>... InComponentTypeIDs)
		: ComponentTypes{ InComponentTypeIDs... }
	{}

	FORCEINLINE TMultiComponentLock<TReadOptional<T>...> LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return TransformTuple(ComponentTypes, [Allocation, WriteContext](auto In){ return In.LockComponentData(Allocation, WriteContext); });
	}

	TTuple< TOptionalReadAccess<T>... > ComponentTypes;
};




template<typename... T>
struct TReadOneOrMoreOfAccessor
{
	using AccessType = TMultiComponentLock<TReadOptional<T>...>;

	TReadOneOrMoreOfAccessor(TComponentTypeID<T>... InComponentTypeIDs)
		: ComponentTypes{ InComponentTypeIDs... }
	{}

	FORCEINLINE TMultiComponentLock<TReadOptional<T>...> LockComponentData(const FEntityAllocation* Allocation, FEntityAllocationWriteContext WriteContext) const
	{
		return TransformTuple(ComponentTypes, [Allocation, WriteContext](auto In){ return In.LockComponentData(Allocation, WriteContext); });
	}

	TTuple< TOptionalReadAccess<T>... > ComponentTypes;
};


inline void AddAccessorToFilter(const FEntityIDAccess*, FEntityComponentFilter* OutFilter)
{
}
inline void AddAccessorToFilter(const FComponentAccess* In, FEntityComponentFilter* OutFilter)
{
	check(In->ComponentType);
	OutFilter->All({ In->ComponentType });
}
inline void AddAccessorToFilter(const FOptionalComponentAccess* In, FEntityComponentFilter* OutFilter)
{
}
template<typename... T>
void AddAccessorToFilter(const TReadOneOfAccessor<T...>* In, FEntityComponentFilter* OutFilter)
{
	FComponentMask Mask;
	VisitTupleElements([&Mask](FOptionalReadAccess Composite){ if (Composite.ComponentType) { Mask.Set(Composite.ComponentType); } }, In->ComponentTypes);

	check(Mask.NumComponents() != 0);
	OutFilter->Complex(Mask, EComplexFilterMode::OneOf);
}
template<typename... T>
void AddAccessorToFilter(const TReadOneOrMoreOfAccessor<T...>* In, FEntityComponentFilter* OutFilter)
{
	FComponentMask Mask;
	VisitTupleElements([&Mask](FOptionalReadAccess Composite) { if (Composite.ComponentType) { Mask.Set(Composite.ComponentType); } }, In->ComponentTypes);

	check(Mask.NumComponents() != 0);
	OutFilter->Complex(Mask, EComplexFilterMode::OneOrMoreOf);
}


inline void PopulatePrerequisites(const FEntityIDAccess*, const FSystemTaskPrerequisites& InPrerequisites, FGraphEventArray* OutGatheredPrereqs)
{
}
inline void PopulatePrerequisites(const FComponentAccess* In, const FSystemTaskPrerequisites& InPrerequisites, FGraphEventArray* OutGatheredPrereqs)
{
	check(In->ComponentType);
	InPrerequisites.FilterByComponent(*OutGatheredPrereqs, In->ComponentType);
}
inline void PopulatePrerequisites(const FOptionalComponentAccess* In, const FSystemTaskPrerequisites& InPrerequisites, FGraphEventArray* OutGatheredPrereqs)
{
	if (In->ComponentType)
	{
		check(In->ComponentType);
		InPrerequisites.FilterByComponent(*OutGatheredPrereqs, In->ComponentType);
	}
}
template<typename... T>
void PopulatePrerequisites(const TReadOneOfAccessor<T...>* In, const FSystemTaskPrerequisites& InPrerequisites, FGraphEventArray* OutGatheredPrereqs)
{
	VisitTupleElements(
		[&InPrerequisites, OutGatheredPrereqs](FOptionalReadAccess Composite)
		{
			PopulatePrerequisites(&Composite, InPrerequisites, OutGatheredPrereqs);
		}
	, In->ComponentTypes);
}
template<typename... T>
void PopulatePrerequisites(const TReadOneOrMoreOfAccessor<T...>* In, const FSystemTaskPrerequisites& InPrerequisites, FGraphEventArray* OutGatheredPrereqs)
{
	VisitTupleElements(
		[&InPrerequisites, OutGatheredPrereqs](FOptionalReadAccess Composite)
		{
			PopulatePrerequisites(&Composite, InPrerequisites, OutGatheredPrereqs);
		}
	, In->ComponentTypes);
}



inline void PopulateSubsequents(const FWriteAccess* In, const FGraphEventRef& InEvent, FSystemSubsequentTasks& OutSubsequents)
{
	check(In->ComponentType);
	OutSubsequents.AddComponentTask(In->ComponentType, InEvent);
}
inline void PopulateSubsequents(const FOptionalWriteAccess* In, const FGraphEventRef& InEvent, FSystemSubsequentTasks& OutSubsequents)
{
	if (In->ComponentType)
	{
		OutSubsequents.AddComponentTask(In->ComponentType, InEvent);
	}
}
inline void PopulateSubsequents(const void* In, const FGraphEventRef& InEvent, FSystemSubsequentTasks& OutSubsequents)
{
}

inline bool HasBeenWrittenToSince(const FEntityIDAccess* In, FEntityAllocation* Allocation, uint64 InSystemSerial)
{
	return Allocation->HasStructureChangedSince(InSystemSerial);
}
inline bool HasBeenWrittenToSince(const FComponentAccess* In, FEntityAllocation* Allocation, uint64 InSystemSerial)
{
	return Allocation->GetComponentHeaderChecked(In->ComponentType).HasBeenWrittenToSince(InSystemSerial);
}
inline bool HasBeenWrittenToSince(const FOptionalComponentAccess* In, FEntityAllocation* Allocation, uint64 InSystemSerial)
{
	if (FComponentHeader* Header = Allocation->FindComponentHeader(In->ComponentType))
	{
		return Header->HasBeenWrittenToSince(InSystemSerial);
	}
	return false;
}
template<typename... T>
bool HasBeenWrittenToSince(const TReadOneOfAccessor<T...>* In, FEntityAllocation* Allocation, uint64 InSystemSerial)
{
	bool bAnyWrittenTo = false;
	VisitTupleElements([Allocation, &bAnyWrittenTo, InSystemSerial](FOptionalReadAccess It){ bAnyWrittenTo |= HasBeenWrittenToSince(&It, Allocation, InSystemSerial); }, In->ComponentTypes);
	return bAnyWrittenTo;
}
template<typename... T>
bool HasBeenWrittenToSince(const TReadOneOrMoreOfAccessor<T...>* In, FEntityAllocation* Allocation, uint64 InSystemSerial)
{
	bool bAnyWrittenTo = false;
	VisitTupleElements([Allocation, &bAnyWrittenTo, InSystemSerial](FOptionalReadAccess It) { bAnyWrittenTo |= HasBeenWrittenToSince(&It, Allocation, InSystemSerial); }, In->ComponentTypes);
	return bAnyWrittenTo;
}



inline bool IsAccessorValid(const FEntityIDAccess*)
{
	return true;
}
inline bool IsAccessorValid(const FComponentAccess* In)
{
	return In->ComponentType != FComponentTypeID::Invalid();
}
inline bool IsAccessorValid(const FOptionalComponentAccess* In)
{
	return true;
}
template<typename... T>
inline bool IsAccessorValid(const TReadOneOfAccessor<T...>* In)
{
	bool bValid = false;
	VisitTupleElements([&bValid](FOptionalReadAccess It){ bValid |= IsAccessorValid(&It); }, In->ComponentTypes);
	return bValid;
}
template<typename... T>
inline bool IsAccessorValid(const TReadOneOrMoreOfAccessor<T...>* In)
{
	bool bValid = false;
	VisitTupleElements([&bValid](FOptionalReadAccess It) { bValid |= IsAccessorValid(&It); }, In->ComponentTypes);
	return bValid;
}

#if UE_MOVIESCENE_ENTITY_DEBUG

	MOVIESCENE_API void AccessorToString(const FReadAccess* In, FEntityManager* EntityManager, FString& OutString);
	MOVIESCENE_API void AccessorToString(const FWriteAccess* In, FEntityManager* EntityManager, FString& OutString);
	MOVIESCENE_API void AccessorToString(const FOptionalReadAccess* In, FEntityManager* EntityManager, FString& OutString);
	MOVIESCENE_API void AccessorToString(const FOptionalWriteAccess* In, FEntityManager* EntityManager, FString& OutString);
	MOVIESCENE_API void AccessorToString(const FEntityIDAccess*, FEntityManager* EntityManager, FString& OutString);
	MOVIESCENE_API void OneOfAccessorToString(const FOptionalReadAccess* In, FEntityManager* EntityManager, FString& OutString);

	template<typename... T>
	void AccessorToString(const TReadOneOfAccessor<T...>* In, FEntityManager* EntityManager, FString& OutString)
	{
		TArray<FString> Strings;
		VisitTupleElements([&Strings, EntityManager](FOptionalReadAccess ReadOptional)
		{
			OneOfAccessorToString(&ReadOptional, EntityManager, Strings.Emplace_GetRef());
		}, In->ComponentTypes);

		OutString += FString::Printf(TEXT("\n\tRead One Of: [ %s ]"), *FString::Join(Strings, TEXT(",")));
	}

	template<typename... T>
	void AccessorToString(const TReadOneOrMoreOfAccessor<T...>* In, FEntityManager* EntityManager, FString& OutString)
	{
		TArray<FString> Strings;
		VisitTupleElements([&Strings, EntityManager](FOptionalReadAccess ReadOptional)
			{
				OneOfAccessorToString(&ReadOptional, EntityManager, Strings.Emplace_GetRef());
			}, In->ComponentTypes);

		OutString += FString::Printf(TEXT("\n\tRead One Or More Of: [ %s ]"), *FString::Join(Strings, TEXT(",")));
	}

#endif // UE_MOVIESCENE_ENTITY_DEBUG


} // namespace MovieScene
} // namespace UE
