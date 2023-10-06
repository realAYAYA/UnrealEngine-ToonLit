// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "EntitySystem/MovieSceneBlenderSystemTypes.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/MovieSceneInitialValueCache.h"
#include "EntitySystem/MovieScenePropertyRegistry.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "Evaluation/Blending/MovieSceneBlendType.h"
#include "Evaluation/IMovieSceneEvaluationHook.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "UObject/GCObjectScopeGuard.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/ScriptInterface.h"

#include "BuiltInComponentTypes.generated.h"

class IMovieSceneEvaluationHook;

namespace UE::MovieScene
{
	struct FCustomPropertyIndex;
	struct FInitialValueIndex;
	struct FInstanceHandle;
	struct FInterrogationKey;
	struct FInterrogationInstance;
	struct FRootInstanceHandle;
	namespace Interpolation
	{
		struct FCachedInterpolation;
	}
}

struct FFrameTime;
struct FMovieSceneBlendChannelID;
struct FMovieSceneSequenceID;

enum class EMovieSceneBlendType : uint8;
class FTrackInstancePropertyBindings;
class UMovieSceneBlenderSystem;
class UMovieSceneSection;
class UMovieSceneTrackInstance;
struct FMovieSceneBoolChannel;
struct FMovieSceneByteChannel;
struct FMovieSceneDoubleChannel;
struct FMovieSceneFloatChannel;
struct FMovieSceneIntegerChannel;
struct FMovieSceneObjectPathChannel;
struct FMovieSceneStringChannel;
struct FMovieScenePropertyBinding;


/**
 * Easing component data.
 */
USTRUCT()
struct FEasingComponentData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMovieSceneSection> Section = nullptr;
};


/**
 * A component that defines a type for a track instance
 */
USTRUCT()
struct FMovieSceneTrackInstanceComponent
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMovieSceneSection> Owner = nullptr;

	UPROPERTY()
	TSubclassOf<UMovieSceneTrackInstance> TrackInstanceClass;
};


/**
 * A component that defines a hook for direct evaluation
 */
USTRUCT()
struct FMovieSceneEvaluationHookComponent
{
	GENERATED_BODY()

	UPROPERTY()
	TScriptInterface<IMovieSceneEvaluationHook> Interface;

	FGuid ObjectBindingID;
};


USTRUCT()
struct FTrackInstanceInputComponent
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMovieSceneSection> Section = nullptr;

	UPROPERTY()
	int32 OutputIndex = INDEX_NONE;
};


namespace UE
{
namespace MovieScene
{

/**
 * The component data for evaluating a bool channel
 */
struct FSourceBoolChannel
{
	FSourceBoolChannel()
		: Source(nullptr)
	{}

	FSourceBoolChannel(const FMovieSceneBoolChannel* InSource)
		: Source(InSource)
	{}

	const FMovieSceneBoolChannel* Source;
};

/**
 * The component data for evaluating a byte channel
 */
struct FSourceByteChannel
{
	FSourceByteChannel()
		: Source(nullptr)
	{}

	FSourceByteChannel(const FMovieSceneByteChannel* InSource)
		: Source(InSource)
	{}

	const FMovieSceneByteChannel* Source;
};

/**
 * The component data for evaluating an integer channel
 */
struct FSourceIntegerChannel
{
	FSourceIntegerChannel()
		: Source(nullptr)
	{}

	FSourceIntegerChannel(const FMovieSceneIntegerChannel* InSource)
		: Source(InSource)
	{}

	const FMovieSceneIntegerChannel* Source;
};

/**
 * The component data for evaluating a float channel
 */
struct FSourceFloatChannel
{
	FSourceFloatChannel()
		: Source(nullptr)
	{}

	FSourceFloatChannel(const FMovieSceneFloatChannel* InSource)
		: Source(InSource)
	{}

	const FMovieSceneFloatChannel* Source;
};

/**
 * The component data for evaluating a double channel
 */
struct FSourceDoubleChannel
{
	FSourceDoubleChannel()
		: Source(nullptr)
	{}

	FSourceDoubleChannel(const FMovieSceneDoubleChannel* InSource)
		: Source(InSource)
	{}

	const FMovieSceneDoubleChannel* Source;
};

/**
 * The component data for evaluating a string channel
 */
struct FSourceStringChannel
{
	FSourceStringChannel()
		: Source(nullptr)
	{}

	FSourceStringChannel(const FMovieSceneStringChannel* InSource)
		: Source(InSource)
	{}

	const FMovieSceneStringChannel* Source;
};

struct FEvaluationHookFlags
{
	bool bHasBegun = false;
};

struct FSourceObjectPathChannel
{
	FSourceObjectPathChannel()
		: Source(nullptr)
	{}

	FSourceObjectPathChannel(const FMovieSceneObjectPathChannel* InSource)
		: Source(InSource)
	{}

	const FMovieSceneObjectPathChannel* Source;
};

/** A component that represents a UObject* either as a strong or weak reference */
struct FObjectComponent
{
	FObjectComponent()
		: ObjectPtr(nullptr)
	{}

	/** Construct a new null component */
	static FObjectComponent Null()
	{
		return FObjectComponent{ nullptr, FObjectKey() };
	}

	/** Construct a new strongly-referenced component from an object ptr */
	static FObjectComponent Strong(UObject* InObject)
	{
		return FObjectComponent{ InObject, FObjectKey() };
	}

	/** Construct a new weakly-referenced component from an object ptr */
	static FObjectComponent Weak(UObject* InObject)
	{
		return FObjectComponent{ InObject, InObject };
	}

	/** Check whether this object component is valid */
	explicit operator bool() const
	{
		return ObjectPtr != nullptr;
	}

	/** Compare this component with another object ptr */
	bool operator==(UObject* InObject) const
	{
		return ObjectPtr == InObject;
	}

	/** Compare this component with another object ptr */
	bool operator!=(UObject* InObject) const
	{
		return ObjectPtr != InObject;
	}

	/** Equality operator */
	friend bool operator==(const FObjectComponent& A, const FObjectComponent& B)
	{
		if (A.ObjectKey != FObjectKey() || B.ObjectKey != FObjectKey())
		{
			return A.ObjectKey == B.ObjectKey;
		}

		return A.ObjectPtr == B.ObjectPtr;
	}

	/** Generate a type has from this component */
	friend uint32 GetTypeHash(const FObjectComponent& In)
	{
		if (In.ObjectKey != FObjectKey())
		{
			return GetTypeHash(In.ObjectKey);
		}

		return GetTypeHash(In.ObjectPtr);
	}

	UObject* operator->() const
	{
		UObject* Object = GetObject();
		checkSlow(Object);
		return Object;
	}

	MOVIESCENE_API UObject* GetObject() const;

	/** Conditionally add a reference for the specified component data based on whether it is strongly referenced or not */
	friend void AddReferencedObjectForComponent(FReferenceCollector& ReferenceCollector, FObjectComponent* ComponentData);

private:

	FObjectComponent(UObject* InObject, const FObjectKey& InObjectKey)
		: ObjectPtr(InObject)
		, ObjectKey(InObjectKey)
	{}

	bool IsStrongReference() const;

	/** Raw pointer to the object. This is explicitly assigned for both strong and weak ptrs, but is only added to the reference graph when FObjectKey() is default constructed.  */
	TObjectPtr<UObject> ObjectPtr;

	/** Default constructed for strong pointers. Assigned on construction for weak ptrs. */
	FObjectKey ObjectKey;
};

/**
 * Pre-defined built in component types
 */
struct FBuiltInComponentTypes
{
	MOVIESCENE_API ~FBuiltInComponentTypes();

public:

	FPropertyRegistry PropertyRegistry;

public:

	/**
	 * Component mask where set bits denot component types that should trigger instantiation when present.
	 * After instantiation, these components will be removed from any entities to prevent instantiation being run constantly
	 */
	FComponentMask RequiresInstantiationMask;

	TComponentTypeID<FMovieSceneEntityID> ParentEntity;

	/**
	 * A bound object ptr component that defines the object being animated. This ptr is explicitly hidden from the reference graph and cleaned up
	 * after a garbage collection pass if it becomes invalid by checking the BoundObjectKey component that must exist alongside it
	 */
	TComponentTypeID<UObject*>            BoundObject;
	TComponentTypeID<FObjectKey>          BoundObjectKey;

	TComponentTypeID<FInstanceHandle>     InstanceHandle;

	TComponentTypeID<FRootInstanceHandle> RootInstanceHandle;

	TComponentTypeID<FMovieSceneSequenceID> SequenceID;

	TComponentTypeID<FFrameTime>          EvalTime;

	TComponentTypeID<double>              EvalSeconds;

public:

	TComponentTypeID<FMovieSceneBlendChannelID> BlendChannelInput;

	TComponentTypeID<FMovieSceneBlendChannelID> BlendChannelOutput;

	TComponentTypeID<int16>               HierarchicalBias;

	TComponentTypeID<FInitialValueIndex>  InitialValueIndex;
public:

	// An FMovieScenePropertyBinding structure
	TComponentTypeID<FMovieScenePropertyBinding> PropertyBinding;

	// An FGuid relating to a direct object binding in a sequence
	TComponentTypeID<FGuid> GenericObjectBinding;

	// An FGuid that is always resolved as a USceneComponent either directly or through the AActor that the GUID relates to
	TComponentTypeID<FGuid> SceneComponentBinding;

	// An FGuid relating to a spawnable binding in a sequence
	TComponentTypeID<FGuid> SpawnableBinding;

public:

	// A boolean repesenting the output of a bool property track or channel
	TComponentTypeID<bool> BoolResult;

	// An FMovieSceneByteChannel
	TComponentTypeID<FSourceByteChannel> ByteChannel;

	// A byte representing the output of a byte or enum track or channel
	TComponentTypeID<uint8> ByteResult;

	// A byte representing the base value for the byte channel for the purposes of "additive from base" blending.
	TComponentTypeID<uint8> BaseByte;

	// An FMovieSceneIntegerChannel
	TComponentTypeID<FSourceIntegerChannel> IntegerChannel;

	// An integer representing the output of an integer track or channel
	TComponentTypeID<int32> IntegerResult;

	// An integer representing the base value for the integer channel for the purposes of "additive from base" blending.
	TComponentTypeID<int32> BaseInteger;

	// An FMovieSceneBoolChannel
	TComponentTypeID<FSourceBoolChannel> BoolChannel;

	// An FMovieSceneFloatChannel considered to be at index N within the source structure (ie 0 = Location.X, Vector.X, Color.R; 1 = Location.Y, Vector.Y, Color.G)
	TComponentTypeID<FSourceFloatChannel> FloatChannel[9];

	// An FMovieSceneDoubleChannel considered to be at index N within the source structure (ie 0 = Location.X, Vector.X; 1 = Location.Y, Vector.Y)
	TComponentTypeID<FSourceDoubleChannel> DoubleChannel[9];

	// An FMovieSceneStringChannel
	TComponentTypeID<FSourceStringChannel> StringChannel;

	// A cached interpolation structure relating to either float channels or double channels
	TComponentTypeID<Interpolation::FCachedInterpolation> CachedInterpolation[9];

	// An FMovieSceneFloatChannel that represents an arbitrary weight
	TComponentTypeID<FSourceFloatChannel> WeightChannel;
	TComponentTypeID<Interpolation::FCachedInterpolation> CachedWeightChannelInterpolation;

	// FMovieSceneObjectPathChannel that represents a changing object path over time
	TComponentTypeID<FSourceObjectPathChannel> ObjectPathChannel;

	// A double considered to be at index N within the source structure (ie 0 = Location.X, Vector.X; 1 = Location.Y, Vector.Y)
	TComponentTypeID<double> DoubleResult[9];

	// A double representing the base value for the double channel at index N, for the purposes of "additive from base" blending.
	TComponentTypeID<double> BaseDouble[9];

	// The time (in frames or in seconds) at which to evaluate a base value, such as BaseFloat[] or BaseDouble[].
	TComponentTypeID<FFrameTime> BaseValueEvalTime;
	TComponentTypeID<double> BaseValueEvalSeconds;

	// A float representing the evaluated output of a weight channel
	TComponentTypeID<double> WeightResult;
	TComponentTypeID<double> EasingResult;

	// The result of an evaluated FMovieSceneStringChannel
	TComponentTypeID<FString> StringResult;

	// The result of an evaluated FMovieSceneObjectPathChannel
	TComponentTypeID<FObjectComponent> ObjectResult;

public:

	// An FEasingComponentData for computing easing curves
	TComponentTypeID<FEasingComponentData> Easing;

	// An index associated to hierarchical easing for the owning sub-sequence
	TComponentTypeID<uint16> HierarchicalEasingChannel;

	// The sub-sequence ID that should receive ease in/out as a whole
	TComponentTypeID<FMovieSceneSequenceID> HierarchicalEasingProvider;

	// Defines an HBias level that is the highest blend target for a given set of components that need to blend together
	TComponentTypeID<int16>       HierarchicalBlendTarget;

	// A float representing the evaluated easing weight
	TComponentTypeID<double> WeightAndEasingResult;

	/** A blender type that should be used for blending this entity */
	TComponentTypeID<TSubclassOf<UMovieSceneBlenderSystem>> BlenderType;

	// An FMovieSceneTrackInstanceComponent that defines the track instance to use
	TComponentTypeID<FMovieSceneTrackInstanceComponent> TrackInstance;

	// An FTrackInstanceInputComponent that defines an input for a track instance
	TComponentTypeID<FTrackInstanceInputComponent> TrackInstanceInput;

	// An FMovieSceneEvaluationHookComponent that defines a stateless hook interface that doesn't need any overlap handling (track instances should be preferred there)
	TComponentTypeID<FMovieSceneEvaluationHookComponent> EvaluationHook;

	TComponentTypeID<FEvaluationHookFlags> EvaluationHookFlags;

public:

	// 
	TComponentTypeID<FCustomPropertyIndex> CustomPropertyIndex;

	// A property offset from a UObject* that points to the memory for a given property - care should be taken to ensure that this is only ever accessed in conjunction with a property tag
	TComponentTypeID<uint16> FastPropertyOffset;

	// A property binding that supports setters and notifications
	TComponentTypeID<TSharedPtr<FTrackInstancePropertyBindings>> SlowProperty;

	struct
	{
		// A tag specifying that an entity wants to restore state on completioon
		FComponentTypeID RestoreState;

		// A tag that specifies this entity should always contribute to the output, regardless of hbias
		FComponentTypeID IgnoreHierarchicalBias;
		FComponentTypeID BlendHierarchicalBias;

		FComponentTypeID AbsoluteBlend;
		FComponentTypeID RelativeBlend;
		FComponentTypeID AdditiveBlend;
		FComponentTypeID AdditiveFromBaseBlend;

		FComponentTypeID NeedsLink;
		FComponentTypeID NeedsUnlink;

		/** Tag that is added to imported entities with a GenericObjectBinding or SceneComponentBinding whose binding did not resolve */
		FComponentTypeID HasUnresolvedBinding;

		FComponentTypeID HasAssignedInitialValue;

		FComponentTypeID ImportedEntity;

		FComponentTypeID SubInstance;

		FComponentTypeID Root;

		FComponentTypeID FixedTime;

		FComponentTypeID SectionPreRoll;
		FComponentTypeID PreRoll;

		FComponentTypeID Finished;

		FComponentTypeID Ignored;

		FComponentTypeID AlwaysCacheInitialValue;

		FComponentTypeID DontOptimizeConstants;

	} Tags;

	struct
	{
		TComponentTypeID<FInterrogationKey> InputKey;
		TComponentTypeID<FInterrogationInstance> Instance;
		TComponentTypeID<FInterrogationKey> OutputKey;
	} Interrogation;

	struct
	{
		FComponentTypeID CreatesEntities;
	} SymbolicTags;

	FComponentMask FinishedMask;

public:

	static MOVIESCENE_API void Destroy();

	static MOVIESCENE_API FBuiltInComponentTypes* Get();

	FORCEINLINE static bool IsBoundObjectGarbage(UObject* InObject)
	{
		return InObject == nullptr || !IsValidChecked(InObject) || InObject->IsUnreachable();
	}

	MOVIESCENE_API FComponentTypeID GetBaseValueComponentType(const FComponentTypeID& InResultComponentType);

private:

	MOVIESCENE_API FBuiltInComponentTypes();

	TMap<FComponentTypeID, FComponentTypeID> ResultToBase;
};


} // namespace MovieScene
} // namespace UE
