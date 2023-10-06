// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "UObject/UnrealType.h"
#include "UObject/ObjectKey.h"
#include "Curves/KeyHandle.h"
#include "Misc/FrameNumber.h"
#include "UObject/WeakFieldPtr.h"

class AActor;
class UCameraComponent;
class UMovieScene;
class UMovieSceneSection;
class UMovieSceneSubSection;
class UMovieSceneSequence;
class USceneComponent;
class USoundBase;
struct FRichCurve;
enum class EMovieSceneKeyInterpolation : uint8;

class MovieSceneHelpers
{
public:

	/*
	* Helper struct to cache the package dirty state and then to restore it
	* after this leaves scope. This is for a few minor areas where calling
	* functions on actors dirties them, but Sequencer doesn't actually want
	* the package to be dirty as it causes Sequencer to unnecessairly dirty
	* actors.
	*/
	struct FMovieSceneScopedPackageDirtyGuard
	{
		MOVIESCENE_API FMovieSceneScopedPackageDirtyGuard(class USceneComponent* InComponent);
		MOVIESCENE_API virtual ~FMovieSceneScopedPackageDirtyGuard();

	private:
		class USceneComponent* Component;
		bool bPackageWasDirty;
	};

	/** 
	 * @return Whether the section is keyable (active, on a track that is not muted, etc 
	 */
	static MOVIESCENE_API bool IsSectionKeyable(const UMovieSceneSection*);

	/**
	 * Finds a section that exists at a given time
	 *
	 * @param Time	The time to find a section at
	 * @param RowIndex  Limit the search to a given row index
	 * @return The found section or null
	 */
	static MOVIESCENE_API UMovieSceneSection* FindSectionAtTime( TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time, int32 RowIndex = INDEX_NONE );

	/**
	 * Finds the nearest section to the given time
	 *
	 * @param Time	The time to find a section at
	 * @param RowIndex  Limit the search to a given row index
	 * @return The found section or null
	 */
	static MOVIESCENE_API UMovieSceneSection* FindNearestSectionAtTime( TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time, int32 RowIndex = INDEX_NONE );

	/** Find the next section that doesn't overlap - the section that has the next closest start time to the requested start time */
	static MOVIESCENE_API UMovieSceneSection* FindNextSection(TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time);

	/** Find the previous section that doesn't overlap - the section that has the previous closest start time to the requested start time */
	static MOVIESCENE_API UMovieSceneSection* FindPreviousSection(TArrayView<UMovieSceneSection* const> Sections, FFrameNumber Time);

	/*
	 * Fix up consecutive sections so that there are no gaps
	 * 
	 * @param Sections All the sections
	 * @param Section The section that was modified 
	 * @param bDelete Was this a deletion?
	 * @param bCleanUp Should we cleanup any invalid sections?
	 * @return Whether the list of sections was modified as part of the clean-up
	 */
	static MOVIESCENE_API bool FixupConsecutiveSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete, bool bCleanUp = false);

	/**
	 * Fix up consecutive sections so that there are no gaps, but there can be overlaps, in which case the sections
	 * blend together.
	 *
	 * @param Sections All the sections
	 * @param Section The section that was modified 
	 * @param bDelete Was this a deletion?
	 * @param bCleanUp Should we cleanup any invalid sections?
	 * @return Whether the list of sections was modified as part of the clean-up
	 */
	static MOVIESCENE_API bool FixupConsecutiveBlendingSections(TArray<UMovieSceneSection*>& Sections, UMovieSceneSection& Section, bool bDelete, bool bCleanUp = false);

	/*
 	 * Sort consecutive sections so that they are in order based on start time
 	 */
	static MOVIESCENE_API void SortConsecutiveSections(TArray<UMovieSceneSection*>& Sections);

	/*
	 * Gather up descendant movie scenes from the incoming sequence
	 */
	static MOVIESCENE_API void GetDescendantMovieScenes(UMovieSceneSequence* InSequence, TArray<UMovieScene*> & InMovieScenes);

	/*
	 * Gather up descendant movie scene sub-sections from the incoming movie scene
	 */
	static MOVIESCENE_API void GetDescendantSubSections(const UMovieScene* InMovieScene, TArray<UMovieSceneSubSection*>& InSubSections);
	
	/**
	 * Get the scene component from the runtime object
	 *
	 * @param Object The object to get the scene component for
	 * @return The found scene component
	 */	
	static MOVIESCENE_API USceneComponent* SceneComponentFromRuntimeObject(UObject* Object);

	/**
	 * Get the active camera component from the actor 
	 *
	 * @param InActor The actor to look for the camera component on
	 * @return The active camera component
	 */
	static MOVIESCENE_API UCameraComponent* CameraComponentFromActor(const AActor* InActor);

	/**
	 * Find and return camera component from the runtime object
	 *
	 * @param Object The object to get the camera component for
	 * @return The found camera component
	 */	
	static MOVIESCENE_API UCameraComponent* CameraComponentFromRuntimeObject(UObject* RuntimeObject);

	/**
	 * Set the runtime object movable
	 *
	 * @param Object The object to set the mobility for
	 * @param Mobility The mobility of the runtime object
	 */
	static MOVIESCENE_API void SetRuntimeObjectMobility(UObject* Object, EComponentMobility::Type ComponentMobility = EComponentMobility::Movable);

	/*
	 * Get the duration for the given sound

	 * @param Sound The sound to get the duration for
	 * @return The duration in seconds
	 */
	static MOVIESCENE_API float GetSoundDuration(USoundBase* Sound);

	/**
	 * Sort predicate that sorts lower bounds of a range
	 */
	static bool SortLowerBounds(TRangeBound<FFrameNumber> A, TRangeBound<FFrameNumber> B)
	{
		return TRangeBound<FFrameNumber>::MinLower(A, B) == A && A != B;
	}

	/**
	 * Sort predicate that sorts upper bounds of a range
	 */
	static bool SortUpperBounds(TRangeBound<FFrameNumber> A, TRangeBound<FFrameNumber> B)
	{
		return TRangeBound<FFrameNumber>::MinUpper(A, B) == A && A != B;
	}

	/**
	 * Sort predicate that sorts overlapping sections by row primarily, then by overlap priority
	 */
	static MOVIESCENE_API bool SortOverlappingSections(const UMovieSceneSection* A, const UMovieSceneSection* B);

	/*
	* Get weight needed to modify the global difference in order to correctly key this section due to it possibly being blended by other sections.
	* @param Section The Section who's weight we are calculating.
	* @param  Time we are at.
	* @return Returns the weight that needs to be applied to the global difference to correctly key this section.
	*/
	static MOVIESCENE_API float CalculateWeightForBlending(UMovieSceneSection* SectionToKey, FFrameNumber Time);

	/*
	 * Return a name unique to the spawnable names in the given movie scene
	 * @param InMovieScene The movie scene to look for existing spawnables.
	 * @param InName The requested name to make unique.
	 * @return The unique name
	 */
	static MOVIESCENE_API FString MakeUniqueSpawnableName(UMovieScene* InMovieScene, const FString& InName);

	/**
	 * Return a copy of the source object, suitable for use as a spawnable template.
	 * @param InSourceObject The source object to convert into a spawnable template
	 * @param InMovieScene The movie scene the spawnable template will be associated with
	 * @param InName The name to use for the spawnable template
	 * @return The spawnable template
	 */
	static MOVIESCENE_API UObject* MakeSpawnableTemplateFromInstance(UObject& InSourceObject, UMovieScene* InMovieScene, FName InName);
};

/**
 * Manages bindings to keyed properties for a track instance. 
 * Calls UFunctions to set the value on runtime objects
 */
class FTrackInstancePropertyBindings
{
public:
	MOVIESCENE_API FTrackInstancePropertyBindings( FName InPropertyName, const FString& InPropertyPath);

	/**
	 * Calls the setter function for a specific runtime object or if the setter function does not exist, the property is set directly
	 *
	 * @param InRuntimeObject The runtime object whose function to call
	 * @param PropertyValue The new value to assign to the property
	 */
	template <typename ValueType>
	void CallFunction( UObject& InRuntimeObject, typename TCallTraits<ValueType>::ParamType PropertyValue )
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(InRuntimeObject);

		FProperty* Property = GetProperty(InRuntimeObject);
		if (Property && Property->HasSetter())
		{
			Property->CallSetter(&InRuntimeObject, &PropertyValue);
		}
		else if (UFunction* SetterFunction = PropAndFunction.SetterFunction.Get())
		{
			InvokeSetterFunction(&InRuntimeObject, SetterFunction, PropertyValue);
		}
		else if (ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>())
		{
			*Val = MoveTempIfPossible(PropertyValue);
		}

		if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
		{
			InRuntimeObject.ProcessEvent(NotifyFunction, nullptr);
		}
	}

	/**
	 * Calls the setter function for a specific runtime object or if the setter function does not exist, the property is set directly
	 *
	 * @param InRuntimeObject The runtime object whose function to call
	 * @param PropertyValue The new value to assign to the property
	 */
	MOVIESCENE_API void CallFunctionForEnum( UObject& InRuntimeObject, int64 PropertyValue );

	/**
	 * Rebuilds the property and function mappings for a single runtime object, and adds them to the cache
	 *
	 * @param InRuntimeObject	The object to cache mappings for
	 */
	MOVIESCENE_API void CacheBinding( const UObject& InRuntimeObject );

	/**
	 * Gets the FProperty that is bound to the track instance
	 *
	 * @param Object	The Object that owns the property
	 * @return			The property on the object if it exists
	 */
	MOVIESCENE_API FProperty* GetProperty(const UObject& Object) const;

	/**
	 * Gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return ValueType	The current value
	 */
	template <typename ValueType>
	ValueType GetCurrentValue(const UObject& Object)
	{
		ValueType Value{};

		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);
		ResolvePropertyValue<ValueType>(PropAndFunction.PropertyAddress, Value);

		return Value;
	}

	/**
	 * Optionally gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return (Optional) The current value of the property on the object
	 */
	template <typename ValueType>
	TOptional<ValueType> GetOptionalValue(const UObject& Object)
	{
		ValueType Value{};

		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);
		if (ResolvePropertyValue<ValueType>(PropAndFunction.PropertyAddress, Value))
		{
			return Value;
		}

		return TOptional<ValueType>();
	}

	/**
	 * Static function for accessing a property value on an object without caching its address
	 *
	 * @param Object			The object to get the property from
	 * @param InPropertyPath	The path to the property to retrieve
	 * @return (Optional) The current value of the property on the object
	 */
	template <typename ValueType>
	static TOptional<ValueType> StaticValue(const UObject* Object, const FString& InPropertyPath)
	{
		checkf(Object, TEXT("No object specified"));

		FPropertyAddress Address = FindPropertyAddress(*Object, InPropertyPath);

		ValueType Value;
		if (ResolvePropertyValue<ValueType>(Address, Value))
		{
			return Value;
		}

		return TOptional<ValueType>();
	}

	/**
	 * Gets the current value of a property on an object
	 *
	 * @param Object	The object to get the property from
	 * @return ValueType	The current value
	 */
	MOVIESCENE_API int64 GetCurrentValueForEnum(const UObject& Object);

	/**
	 * Sets the current value of a property on an object
	 *
	 * @param Object	The object to set the property on
	 * @param InValue   The value to set
	 */
	template <typename ValueType>
	void SetCurrentValue(UObject& Object, typename TCallTraits<ValueType>::ParamType InValue)
	{
		FPropertyAndFunction PropAndFunction = FindOrAdd(Object);

		if(ValueType* Val = PropAndFunction.GetPropertyAddress<ValueType>())
		{
			*Val = InValue;

			if (UFunction* NotifyFunction = PropAndFunction.NotifyFunction.Get())
			{
				Object.ProcessEvent(NotifyFunction, nullptr);
			}
		}
	}

	/** @return the property path that this binding was initialized from */
	const FString& GetPropertyPath() const
	{
		return PropertyPath;
	}

	/** @return the property name that this binding was initialized from */
	const FName& GetPropertyName() const
	{
		return PropertyName;
	}

	static MOVIESCENE_API FProperty* FindProperty(const UObject* Object, const FString& InPropertyPath);

private:

	/**
	 * Wrapper for UObject::ProcessEvent that attempts to pass the new property value directly to the function as a parameter,
	 * but handles cases where multiple parameters or a return value exists. The setter parameter must be the first in the list,
	 * any other parameters will be default constructed.
	 */
	template<typename T>
	static void InvokeSetterFunction(UObject* InRuntimeObject, UFunction* Setter, T&& InPropertyValue);

	struct FPropertyAddress
	{
		TWeakFieldPtr<FProperty> Property;
		void* Address;

		FProperty* GetProperty() const
		{
			FProperty* PropertyPtr = Property.Get();
			if (PropertyPtr && Address && !PropertyPtr->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed))
			{
				return PropertyPtr;
			}
			return nullptr;
		}

		template<typename ValueType>
		ValueType* GetPropertyAddress() const
		{
			FProperty* PropertyPtr = GetProperty();
			return PropertyPtr ? PropertyPtr->ContainerPtrToValuePtr<ValueType>(Address) : nullptr;
		}

		FPropertyAddress()
			: Property(nullptr)
			, Address(nullptr)
		{}
	};

	struct FPropertyAndFunction
	{
		FPropertyAddress PropertyAddress;
		TWeakObjectPtr<UFunction> SetterFunction;
		TWeakObjectPtr<UFunction> NotifyFunction;

		template<typename ValueType>
		ValueType* GetPropertyAddress() const
		{
			return PropertyAddress.GetPropertyAddress<ValueType>();
		}

		FPropertyAndFunction()
			: PropertyAddress()
			, SetterFunction( nullptr )
			, NotifyFunction( nullptr )
		{}
	};

	template <typename ValueType>
	static bool ResolvePropertyValue(const FPropertyAddress& Address, ValueType& OutValue)
	{
		if (const ValueType* Value = Address.GetPropertyAddress<ValueType>())
		{
			OutValue = *Value;
			return true;
		}
		return false;
	}

	static MOVIESCENE_API FPropertyAddress FindPropertyAddressRecursive(void* BasePointer, UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index);
	static MOVIESCENE_API FPropertyAddress FindPropertyAddress(const UObject& Object, const FString& InPropertyPath);

	static MOVIESCENE_API FProperty* FindPropertyRecursive(UStruct* InStruct, TArray<FString>& InPropertyNames, uint32 Index);

	/** Find or add the FPropertyAndFunction for the specified object */
	FPropertyAndFunction FindOrAdd(const UObject& InObject)
	{
		FObjectKey ObjectKey(&InObject);

		const FPropertyAndFunction* PropAndFunction = RuntimeObjectToFunctionMap.Find(ObjectKey);
		if (PropAndFunction && (PropAndFunction->SetterFunction.IsValid() || PropAndFunction->PropertyAddress.Property.Get()))
		{
			return *PropAndFunction;
		}

		CacheBinding(InObject);
		return RuntimeObjectToFunctionMap.FindRef(ObjectKey);
	}

private:
	/** Mapping of objects to bound functions that will be called to update data on the track */
	TMap< FObjectKey, FPropertyAndFunction > RuntimeObjectToFunctionMap;

	/** Path to the property we are bound to */
	FString PropertyPath;

	/** Name of the function to call to set values */
	FName FunctionName;

	/** Name of a function to call when a value has been set */
	FName NotifyFunctionName;

	/** Actual name of the property we are bound to */
	FName PropertyName;

};

/** Explicit specializations for bools */
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::CallFunction<bool>(UObject& InRuntimeObject, TCallTraits<bool>::ParamType PropertyValue);
template<> MOVIESCENE_API bool FTrackInstancePropertyBindings::ResolvePropertyValue<bool>(const FPropertyAddress& Address, bool& OutValue);
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::SetCurrentValue<bool>(UObject& Object, TCallTraits<bool>::ParamType InValue);

template<> MOVIESCENE_API void FTrackInstancePropertyBindings::CallFunction<UObject*>(UObject& InRuntimeObject, UObject* PropertyValue);
template<> MOVIESCENE_API bool FTrackInstancePropertyBindings::ResolvePropertyValue<UObject*>(const FPropertyAddress& Address, UObject*& OutValue);
template<> MOVIESCENE_API void FTrackInstancePropertyBindings::SetCurrentValue<UObject*>(UObject& InRuntimeObject, UObject* InValue);


template<typename T>
void FTrackInstancePropertyBindings::InvokeSetterFunction(UObject* InRuntimeObject, UFunction* Setter, T&& InPropertyValue)
{
	// CacheBinding already guarantees that the function has >= 1 parameters
	const int32 ParmsSize = Setter->ParmsSize;

	// This should all be const really, but ProcessEvent only takes a non-const void*
	void* InputParameter = const_cast<typename TDecay<T>::Type*>(&InPropertyValue);

	// By default we try and use the existing stack value
	uint8* Params = reinterpret_cast<uint8*>(InputParameter);

	check(InRuntimeObject && Setter);
	if (Setter->ReturnValueOffset != MAX_uint16 || Setter->NumParms > 1)
	{
		// Function has a return value or multiple parameters, we need to initialize memory for the entire parameter pack
		// We use alloca here (as in UObject::ProcessEvent) to avoid a heap allocation. Alloca memory survives the current function's stack frame.
		Params = reinterpret_cast<uint8*>(FMemory_Alloca(ParmsSize));

		bool bFirstProperty = true;
		for (FProperty* Property = Setter->PropertyLink; Property; Property = Property->PropertyLinkNext)
		{
			// Initialize the parameter pack with any param properties that reside in the container
			if (Property->IsInContainer(ParmsSize))
			{
				Property->InitializeValue_InContainer(Params);

				// The first encountered property is assumed to be the input value so initialize this with the user-specified value from InPropertyValue
				if (Property->HasAnyPropertyFlags(CPF_Parm) && !Property->HasAnyPropertyFlags(CPF_ReturnParm) && bFirstProperty)
				{
					const bool bIsValid = ensureMsgf(sizeof(T) == Property->ElementSize, TEXT("Property type does not match for Sequencer setter function %s::%s (%ibytes != %ibytes"), *InRuntimeObject->GetName(), *Setter->GetName(), sizeof(T), Property->ElementSize);
					if (bIsValid)
					{
						Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(Params), &InPropertyValue);
					}
					else
					{
						return;
					}
				}
				bFirstProperty = false;
			}
		}
	}

	// Now we have the parameters set up correctly, call the function
	InRuntimeObject->ProcessEvent(Setter, Params);
}
