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
namespace UE { namespace MovieScene { struct FCustomPropertyIndex; } }
namespace UE { namespace MovieScene { struct FInitialValueIndex; } }
namespace UE { namespace MovieScene { struct FInstanceHandle; } }
namespace UE { namespace MovieScene { struct FInterrogationKey; } }
namespace UE { namespace MovieScene { struct FRootInstanceHandle; } }
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

struct FSourceFloatChannelFlags
{
	bool bNeedsEvaluate = true;
};

/**
 * The component data for evaluation a double channel
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

struct FSourceDoubleChannelFlags
{
	bool bNeedsEvaluate = true;
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


/**
 * Pre-defined built in component types
 */
struct MOVIESCENE_API FBuiltInComponentTypes
{
	~FBuiltInComponentTypes();

public:

	FPropertyRegistry PropertyRegistry;

public:

	/**
	 * Component mask where set bits denot component types that should trigger instantiation when present.
	 * After instantiation, these components will be removed from any entities to prevent instantiation being run constantly
	 */
	FComponentMask RequiresInstantiationMask;

	TComponentTypeID<FMovieSceneEntityID> ParentEntity;

	TComponentTypeID<UObject*>            BoundObject;

	TComponentTypeID<FInstanceHandle>     InstanceHandle;

	TComponentTypeID<FRootInstanceHandle> RootInstanceHandle;

	TComponentTypeID<FFrameTime>          EvalTime;

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
	TComponentTypeID<FSourceFloatChannelFlags> FloatChannelFlags[9];

	// An FMovieSceneDoubleChannel considered to be at index N within the source structure (ie 0 = Location.X, Vector.X; 1 = Location.Y, Vector.Y)
	TComponentTypeID<FSourceDoubleChannel> DoubleChannel[9];
	TComponentTypeID<FSourceDoubleChannelFlags> DoubleChannelFlags[9];

	// An FMovieSceneFloatChannel that represents an arbitrary weight
	TComponentTypeID<FSourceFloatChannel> WeightChannel;
	TComponentTypeID<FSourceFloatChannelFlags> WeightChannelFlags;

	// FMovieSceneObjectPathChannel that represents a changing object path over time
	TComponentTypeID<FSourceObjectPathChannel> ObjectPathChannel;

	// A double considered to be at index N within the source structure (ie 0 = Location.X, Vector.X; 1 = Location.Y, Vector.Y)
	TComponentTypeID<double> DoubleResult[9];

	// A double representing the base value for the double channel at index N, for the purposes of "additive from base" blending.
	TComponentTypeID<double> BaseDouble[9];

	// The time at which to evaluate a base value, such as BaseFloat[] or BaseDouble[].
	TComponentTypeID<FFrameTime> BaseValueEvalTime;

	// A float representing the evaluated output of a weight channel
	TComponentTypeID<double> WeightResult;

	// The result of an evaluated FMovieSceneObjectPathChannel
	TComponentTypeID<UObject*> ObjectResult;

public:

	// An FEasingComponentData for computing easing curves
	TComponentTypeID<FEasingComponentData> Easing;

	// An index associated to hierarchical easing for the owning sub-sequence
	TComponentTypeID<uint16> HierarchicalEasingChannel;

	// The sub-sequence ID that should receive ease in/out as a whole
	TComponentTypeID<FMovieSceneSequenceID> HierarchicalEasingProvider;

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

		FComponentTypeID AbsoluteBlend;
		FComponentTypeID RelativeBlend;
		FComponentTypeID AdditiveBlend;
		FComponentTypeID AdditiveFromBaseBlend;

		FComponentTypeID NeedsLink;
		FComponentTypeID NeedsUnlink;

		/** Tag that is added to imported entities with a GenericObjectBinding or SceneComponentBinding whose binding did not resolve */
		FComponentTypeID HasUnresolvedBinding;

		FComponentTypeID MigratedFromFastPath;

		FComponentTypeID ImportedEntity;
		FComponentTypeID Master;

		FComponentTypeID FixedTime;

		FComponentTypeID SectionPreRoll;
		FComponentTypeID PreRoll;

		FComponentTypeID Finished;

		FComponentTypeID Ignored;

	} Tags;

	struct
	{
		TComponentTypeID<FInterrogationKey> InputKey;
		TComponentTypeID<FInterrogationKey> OutputKey;
	} Interrogation;

	struct
	{
		FComponentTypeID CreatesEntities;
	} SymbolicTags;

	FComponentMask FinishedMask;

public:

	static void Destroy();

	static FBuiltInComponentTypes* Get();

	FORCEINLINE static bool IsBoundObjectGarbage(UObject* InObject)
	{
		return InObject == nullptr || !IsValidChecked(InObject) || InObject->IsUnreachable();
	}

	FComponentTypeID GetBaseValueComponentType(const FComponentTypeID& InResultComponentType);

private:

	FBuiltInComponentTypes();

	TMap<FComponentTypeID, FComponentTypeID> ResultToBase;
};


} // namespace MovieScene
} // namespace UE
