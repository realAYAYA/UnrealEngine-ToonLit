// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/BuiltInComponentTypes.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"
#include "EntitySystem/MovieScenePropertyBinding.h"
#include "EntitySystem/MovieSceneEntityFactoryTemplates.h"
#include "Channels/MovieSceneInterpolation.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BuiltInComponentTypes)

namespace UE
{
namespace MovieScene
{

bool FObjectComponent::IsStrongReference() const
{
	return ObjectKey == FObjectKey();
}

UObject* FObjectComponent::GetObject() const
{
	if (IsStrongReference())
	{
		return ObjectPtr;
	}
	return ObjectKey.ResolveObjectPtr();
}

void AddReferencedObjectForComponent(FReferenceCollector& ReferenceCollector, FObjectComponent* ComponentData)
{
	if (ComponentData->IsStrongReference())
	{
		ReferenceCollector.AddReferencedObject(ComponentData->ObjectPtr);
	}
}

static bool GMovieSceneBuiltInComponentTypesDestroyed = false;
static TUniquePtr<FBuiltInComponentTypes> GMovieSceneBuiltInComponentTypes;

struct FBoundObjectKeyInitializer : IMutualComponentInitializer
{
	void Run(const FEntityRange& Range, const FEntityAllocationWriteContext& WriteContext) override
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

		TComponentReader<UObject*>   Objects    = Range.Allocation->ReadComponents(BuiltInComponents->BoundObject);
		TComponentWriter<FObjectKey> ObjectKeys = Range.Allocation->WriteComponents(BuiltInComponents->BoundObjectKey, WriteContext);

		for (int32 Index = 0; Index < Range.Num; ++Index)
		{
			ObjectKeys[Range.ComponentStartOffset + Index] = Objects[Range.ComponentStartOffset + Index];
		}
	}
};

FBuiltInComponentTypes::FBuiltInComponentTypes()
{
	using namespace UE::MovieScene;

	FComponentRegistry* ComponentRegistry = UMovieSceneEntitySystemLinker::GetComponents();

	ComponentRegistry->NewComponentType(&ParentEntity,          TEXT("Parent Entity"));
	ComponentRegistry->NewComponentType(&SequenceID,            TEXT("Sequence ID"));
	ComponentRegistry->NewComponentType(&InstanceHandle,        TEXT("Instance Handle"));
	ComponentRegistry->NewComponentType(&RootInstanceHandle,    TEXT("Root Instance Handle"));

	ComponentRegistry->NewComponentType(&EvalTime,              TEXT("Eval Time"));
	ComponentRegistry->NewComponentType(&EvalSeconds,           TEXT("Eval Seconds"));

	ComponentRegistry->NewComponentType(&BoundObjectKey,        TEXT("Bound Object Key"));
	// Intentionally hidden from the reference graph because they are always accompanied by a BoundObjectKey which is used for garbage collection
	ComponentRegistry->NewComponentTypeNoAddReferencedObjects(&BoundObject, TEXT("Bound Object"));

	ComponentRegistry->NewComponentType(&PropertyBinding,         TEXT("Property Binding"), EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&GenericObjectBinding,    TEXT("Generic Object Binding ID"));
	ComponentRegistry->NewComponentType(&SceneComponentBinding,   TEXT("USceneComponent Binding ID"));
	ComponentRegistry->NewComponentType(&SpawnableBinding,        TEXT("Spawnable Binding"));
	ComponentRegistry->NewComponentType(&TrackInstance,           TEXT("Track Instance"));
	ComponentRegistry->NewComponentType(&BoolChannel,             TEXT("Bool Channel"));
	ComponentRegistry->NewComponentType(&ByteChannel,             TEXT("Byte Channel"));
	ComponentRegistry->NewComponentType(&IntegerChannel,          TEXT("Integer Channel"));
	ComponentRegistry->NewComponentType(&FloatChannel[0],         TEXT("Float Channel 0"));
	ComponentRegistry->NewComponentType(&FloatChannel[1],         TEXT("Float Channel 1"));
	ComponentRegistry->NewComponentType(&FloatChannel[2],         TEXT("Float Channel 2"));
	ComponentRegistry->NewComponentType(&FloatChannel[3],         TEXT("Float Channel 3"));
	ComponentRegistry->NewComponentType(&FloatChannel[4],         TEXT("Float Channel 4"));
	ComponentRegistry->NewComponentType(&FloatChannel[5],         TEXT("Float Channel 5"));
	ComponentRegistry->NewComponentType(&FloatChannel[6],         TEXT("Float Channel 6"));
	ComponentRegistry->NewComponentType(&FloatChannel[7],         TEXT("Float Channel 7"));
	ComponentRegistry->NewComponentType(&FloatChannel[8],         TEXT("Float Channel 8"));
	ComponentRegistry->NewComponentType(&DoubleChannel[0],        TEXT("Double Channel 0"));
	ComponentRegistry->NewComponentType(&DoubleChannel[1],        TEXT("Double Channel 1"));
	ComponentRegistry->NewComponentType(&DoubleChannel[2],        TEXT("Double Channel 2"));
	ComponentRegistry->NewComponentType(&DoubleChannel[3],        TEXT("Double Channel 3"));
	ComponentRegistry->NewComponentType(&DoubleChannel[4],        TEXT("Double Channel 4"));
	ComponentRegistry->NewComponentType(&DoubleChannel[5],        TEXT("Double Channel 5"));
	ComponentRegistry->NewComponentType(&DoubleChannel[6],        TEXT("Double Channel 6"));
	ComponentRegistry->NewComponentType(&DoubleChannel[7],        TEXT("Double Channel 7"));
	ComponentRegistry->NewComponentType(&DoubleChannel[8],        TEXT("Double Channel 8"));
	ComponentRegistry->NewComponentType(&WeightChannel,           TEXT("Weight Channel"));
	ComponentRegistry->NewComponentType(&StringChannel,           TEXT("String Channel"));
	ComponentRegistry->NewComponentType(&ObjectPathChannel,       TEXT("Object Path Channel"));

	ComponentRegistry->NewComponentType(&CachedInterpolation[0],   TEXT("Cached Interpolation [0]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[1],   TEXT("Cached Interpolation [1]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[2],   TEXT("Cached Interpolation [2]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[3],   TEXT("Cached Interpolation [3]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[4],   TEXT("Cached Interpolation [4]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[5],   TEXT("Cached Interpolation [5]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[6],   TEXT("Cached Interpolation [6]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[7],   TEXT("Cached Interpolation [7]"));
	ComponentRegistry->NewComponentType(&CachedInterpolation[8],   TEXT("Cached Interpolation [8]"));
	ComponentRegistry->NewComponentType(&CachedWeightChannelInterpolation, TEXT("Cached Weight Channel Interpolation"));

	ComponentRegistry->NewComponentType(&Easing,                  TEXT("Easing"));
	ComponentRegistry->NewComponentType(&EasingResult,            TEXT("Easing Result"));
	ComponentRegistry->NewComponentType(&HierarchicalEasingChannel,  TEXT("Hierarchical Easing Channel"));
	ComponentRegistry->NewComponentType(&HierarchicalBlendTarget,    TEXT("Hierarchical Blend Target"));
	ComponentRegistry->NewComponentType(&HierarchicalEasingProvider, TEXT("Hierarchical Easing Provider"));

	ComponentRegistry->NewComponentType(&BlenderType,           TEXT("Blender System Type"), EComponentTypeFlags::CopyToChildren);
	ComponentRegistry->NewComponentType(&BlendChannelInput,     TEXT("Blend Channel Input"));
	ComponentRegistry->NewComponentType(&HierarchicalBias,      TEXT("Hierarchical Bias"));
	ComponentRegistry->NewComponentType(&BlendChannelOutput,    TEXT("Blend Channel Output"));
	ComponentRegistry->NewComponentType(&InitialValueIndex,     TEXT("Initial Value Index"));

	ComponentRegistry->NewComponentType(&CustomPropertyIndex,   TEXT("Custom Property Index"));			// Not EComponentTypeFlags::Preserved because the system property manager will always ensure that the component is added to the correct entity
	ComponentRegistry->NewComponentType(&FastPropertyOffset,    TEXT("Fast Property Offset"));			// Not EComponentTypeFlags::Preserved because the system property manager will always ensure that the component is added to the correct entity
	ComponentRegistry->NewComponentType(&SlowProperty,          TEXT("Slow Property Binding"));			// Not EComponentTypeFlags::Preserved because the system property manager will always ensure that the component is added to the correct entity
	ComponentRegistry->NewComponentType(&BoolResult,            TEXT("Bool Result"));
	ComponentRegistry->NewComponentType(&ByteResult,            TEXT("Byte Result"));
	ComponentRegistry->NewComponentType(&IntegerResult,         TEXT("Integer Result"));
	ComponentRegistry->NewComponentType(&DoubleResult[0],       TEXT("Double Result 0"));
	ComponentRegistry->NewComponentType(&DoubleResult[1],       TEXT("Double Result 1"));
	ComponentRegistry->NewComponentType(&DoubleResult[2],       TEXT("Double Result 2"));
	ComponentRegistry->NewComponentType(&DoubleResult[3],       TEXT("Double Result 3"));
	ComponentRegistry->NewComponentType(&DoubleResult[4],       TEXT("Double Result 4"));
	ComponentRegistry->NewComponentType(&DoubleResult[5],       TEXT("Double Result 5"));
	ComponentRegistry->NewComponentType(&DoubleResult[6],       TEXT("Double Result 6"));
	ComponentRegistry->NewComponentType(&DoubleResult[7],       TEXT("Double Result 7"));
	ComponentRegistry->NewComponentType(&DoubleResult[8],       TEXT("Double Result 8"));
	ComponentRegistry->NewComponentType(&StringResult,          TEXT("String Result"));
	ComponentRegistry->NewComponentType(&ObjectResult,          TEXT("Object Result"));

	ComponentRegistry->NewComponentType(&BaseByte,			    TEXT("Base Byte"));
	ComponentRegistry->NewComponentType(&BaseInteger,			TEXT("Base Integer"));
	ComponentRegistry->NewComponentType(&BaseDouble[0],         TEXT("Base Double 0"));
	ComponentRegistry->NewComponentType(&BaseDouble[1],         TEXT("Base Double 1"));
	ComponentRegistry->NewComponentType(&BaseDouble[2],         TEXT("Base Double 2"));
	ComponentRegistry->NewComponentType(&BaseDouble[3],         TEXT("Base Double 3"));
	ComponentRegistry->NewComponentType(&BaseDouble[4],         TEXT("Base Double 4"));
	ComponentRegistry->NewComponentType(&BaseDouble[5],         TEXT("Base Double 5"));
	ComponentRegistry->NewComponentType(&BaseDouble[6],         TEXT("Base Double 6"));
	ComponentRegistry->NewComponentType(&BaseDouble[7],         TEXT("Base Double 7"));
	ComponentRegistry->NewComponentType(&BaseDouble[8],         TEXT("Base Double 8"));

	ComponentRegistry->NewComponentType(&BaseValueEvalTime,     TEXT("Base Value Eval Time"));
	ComponentRegistry->NewComponentType(&BaseValueEvalSeconds,  TEXT("Base Value Eval Seconds"));

	ComponentRegistry->NewComponentType(&WeightResult,          TEXT("Weight Result"));
	ComponentRegistry->NewComponentType(&WeightAndEasingResult, TEXT("Weight/Easing Result"));

	ComponentRegistry->NewComponentType(&TrackInstance,         TEXT("Track Instance"));
	ComponentRegistry->NewComponentType(&TrackInstanceInput,    TEXT("Track Instance Input"));

	ComponentRegistry->NewComponentType(&EvaluationHook,        TEXT("Evaluation Hook"));
	ComponentRegistry->NewComponentType(&EvaluationHookFlags,   TEXT("Evaluation Hook Flags"), EComponentTypeFlags::Preserved);

	ComponentRegistry->NewComponentType(&Interrogation.InputKey,  TEXT("Interrogation Input"));
	ComponentRegistry->NewComponentType(&Interrogation.Instance,  TEXT("Interrogation Instance"));
	ComponentRegistry->NewComponentType(&Interrogation.OutputKey, TEXT("Interrogation Output"));

	Tags.RestoreState            = ComponentRegistry->NewTag(TEXT("Is Restore State Entity"));
	Tags.IgnoreHierarchicalBias  = ComponentRegistry->NewTag(TEXT("Ignore Hierarchical Bias"));
	Tags.BlendHierarchicalBias   = ComponentRegistry->NewTag(TEXT("Blend Hierarchical Bias"));

	Tags.AbsoluteBlend           = ComponentRegistry->NewTag(TEXT("Is Absolute Blend"));
	Tags.RelativeBlend           = ComponentRegistry->NewTag(TEXT("Is Relative Blend"));
	Tags.AdditiveBlend           = ComponentRegistry->NewTag(TEXT("Is Additive Blend"));
	Tags.AdditiveFromBaseBlend   = ComponentRegistry->NewTag(TEXT("Is Additive From Base Blend"));

	Tags.NeedsLink               = ComponentRegistry->NewTag(TEXT("Needs Link"));
	Tags.NeedsUnlink             = ComponentRegistry->NewTag(TEXT("Needs Unlink"));
	Tags.HasUnresolvedBinding    = ComponentRegistry->NewTag(TEXT("Has Unresolved Binding"));
	Tags.HasAssignedInitialValue = ComponentRegistry->NewTag(TEXT("Has Assigned Initial Value"));
	Tags.Root                    = ComponentRegistry->NewTag(TEXT("Root"));
	Tags.SubInstance             = ComponentRegistry->NewTag(TEXT("Sub Instance"));
	Tags.ImportedEntity          = ComponentRegistry->NewTag(TEXT("Imported Entity"));
	Tags.Finished                = ComponentRegistry->NewTag(TEXT("Finished Evaluating"));
	Tags.Ignored                 = ComponentRegistry->NewTag(TEXT("Ignored"));
	Tags.DontOptimizeConstants   = ComponentRegistry->NewTag(TEXT("Don't Optimize Constants"));
	Tags.FixedTime               = ComponentRegistry->NewTag(TEXT("Fixed Time"));
	Tags.PreRoll                 = ComponentRegistry->NewTag(TEXT("Pre Roll"));
	Tags.SectionPreRoll          = ComponentRegistry->NewTag(TEXT("Section Pre Roll"));
	Tags.AlwaysCacheInitialValue = ComponentRegistry->NewTag(TEXT("Always Cache Initial Value"));

	SymbolicTags.CreatesEntities = ComponentRegistry->NewTag(TEXT("~~ SYMBOLIC ~~ Creates Entities"));

	FinishedMask.SetAll({ Tags.NeedsUnlink, Tags.Finished });

	{
		FMutuallyInclusiveComponentParams ObjectKeyParams;
		ObjectKeyParams.CustomInitializer = MakeUnique<FBoundObjectKeyInitializer>();
		ObjectKeyParams.Type = EMutuallyInclusiveComponentType::Mandatory;

		ComponentRegistry->Factories.DefineMutuallyInclusiveComponents(BoundObject, { BoundObjectKey }, MoveTemp(ObjectKeyParams));
	}

	// New children always need link
	ComponentRegistry->Factories.DefineChildComponent(Tags.NeedsLink);

	// Always copy these tags over to children
	ComponentRegistry->Factories.DefineChildComponent(Tags.RestoreState,  Tags.RestoreState);
	ComponentRegistry->Factories.DefineChildComponent(Tags.IgnoreHierarchicalBias,  Tags.IgnoreHierarchicalBias);
	ComponentRegistry->Factories.DefineChildComponent(Tags.BlendHierarchicalBias,  Tags.BlendHierarchicalBias);
	ComponentRegistry->Factories.DefineChildComponent(Tags.AbsoluteBlend, Tags.AbsoluteBlend);
	ComponentRegistry->Factories.DefineChildComponent(Tags.RelativeBlend, Tags.RelativeBlend);
	ComponentRegistry->Factories.DefineChildComponent(Tags.AdditiveBlend, Tags.AdditiveBlend);
	ComponentRegistry->Factories.DefineChildComponent(Tags.AdditiveFromBaseBlend, Tags.AdditiveFromBaseBlend);
	ComponentRegistry->Factories.DefineChildComponent(Tags.FixedTime,     Tags.FixedTime);
	ComponentRegistry->Factories.DefineChildComponent(Tags.PreRoll,       Tags.PreRoll);
	ComponentRegistry->Factories.DefineChildComponent(Tags.SectionPreRoll,Tags.SectionPreRoll);
	ComponentRegistry->Factories.DefineChildComponent(Tags.AlwaysCacheInitialValue,Tags.AlwaysCacheInitialValue);

	ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(Tags.SectionPreRoll,Tags.PreRoll);

	ComponentRegistry->Factories.DuplicateChildComponent(EvalTime);
	ComponentRegistry->Factories.DuplicateChildComponent(EvalSeconds);
	ComponentRegistry->Factories.DuplicateChildComponent(BaseValueEvalTime);
	ComponentRegistry->Factories.DuplicateChildComponent(BaseValueEvalSeconds);

	ComponentRegistry->Factories.DuplicateChildComponent(SequenceID);
	ComponentRegistry->Factories.DuplicateChildComponent(InstanceHandle);
	ComponentRegistry->Factories.DuplicateChildComponent(RootInstanceHandle);
	ComponentRegistry->Factories.DuplicateChildComponent(PropertyBinding);
	ComponentRegistry->Factories.DuplicateChildComponent(HierarchicalBias);

	// Children always need a Parent - these are initialized by the tasks that create them
	{
		ComponentRegistry->Factories.DefineChildComponent(FComponentTypeID::Invalid(), ParentEntity);
	}
	
	// Bool channel relationships
	{
		ComponentRegistry->Factories.DuplicateChildComponent(BoolChannel);
		ComponentRegistry->Factories.DuplicateChildComponent(BoolResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(BoolChannel, BoolResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(BoolResult, EvalTime);
	}

	// Byte channel relationships
	{
		ComponentRegistry->Factories.DuplicateChildComponent(ByteChannel);
		ComponentRegistry->Factories.DuplicateChildComponent(ByteResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(ByteChannel, ByteResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(ByteChannel, EvalTime);

		ComponentRegistry->Factories.DefineComplexInclusiveComponents(
				FComplexInclusivityFilter::All({ ByteResult, BaseValueEvalTime, Tags.AdditiveFromBaseBlend }),
				BaseByte);
	}
	
	// Integer channel relationships
	{
		ComponentRegistry->Factories.DuplicateChildComponent(IntegerChannel);
		ComponentRegistry->Factories.DuplicateChildComponent(IntegerResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(IntegerChannel, IntegerResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(IntegerChannel, EvalTime);

		ComponentRegistry->Factories.DefineComplexInclusiveComponents(
				FComplexInclusivityFilter::All({ IntegerResult, BaseValueEvalTime, Tags.AdditiveFromBaseBlend }),
				BaseInteger);
	}

	// Float channel relationships
	{
		static_assert(
				UE_ARRAY_COUNT(FloatChannel) == UE_ARRAY_COUNT(BaseDouble), 
				"Base floats and float results should have the same size.");
		static_assert(
				UE_ARRAY_COUNT(FloatChannel) == UE_ARRAY_COUNT(CachedInterpolation),
				"Float channel flags and flor channels should have the same size.");

		// Duplicate float channels
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(FloatChannel); ++Index)
		{
			ComponentRegistry->Factories.DuplicateChildComponent(FloatChannel[Index]);
			ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(FloatChannel[Index], DoubleResult[Index]);
			ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(FloatChannel[Index], EvalTime);
			ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(FloatChannel[Index], CachedInterpolation[Index]);
		}
	}

	// Double channel relationships
	{
		static_assert(
				UE_ARRAY_COUNT(DoubleChannel) == UE_ARRAY_COUNT(BaseDouble), 
				"Base doubles and double results should have the same size.");
		static_assert(
				UE_ARRAY_COUNT(DoubleChannel) == UE_ARRAY_COUNT(CachedInterpolation),
				"Double channel flags and flor channels should have the same size.");

		// Duplicate double channels
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DoubleChannel); ++Index)
		{
			ComponentRegistry->Factories.DuplicateChildComponent(DoubleChannel[Index]);
			ComponentRegistry->Factories.DuplicateChildComponent(DoubleResult[Index]);
			ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(DoubleChannel[Index], DoubleResult[Index]);
			ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(DoubleChannel[Index], EvalTime);
			ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(DoubleChannel[Index], CachedInterpolation[Index]);
		}
	}

	{
		// Associate result and base values.
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(DoubleResult); ++Index)
		{
			ResultToBase.Add(DoubleResult[Index], BaseDouble[Index]);
		}

		// Create base double components for any channels that are meant to be additive from base.
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BaseDouble); ++Index)
		{
			ComponentRegistry->Factories.DefineComplexInclusiveComponents(
					FComplexInclusivityFilter::All({ DoubleResult[Index], BaseValueEvalTime, Tags.AdditiveFromBaseBlend }),
					BaseDouble[Index]);
		}

		// If normal evaluation requires the time in seconds, the base value evaluation probably also requires
		// a base eval time in seconds.
		ComponentRegistry->Factories.DefineComplexInclusiveComponents(
				FComplexInclusivityFilter::All({ EvalSeconds, BaseValueEvalTime }),
				BaseValueEvalSeconds);
	}

	// Easing component relationships
	{
		// Easing components should be duplicated to children
		ComponentRegistry->Factories.DuplicateChildComponent(Easing);
		ComponentRegistry->Factories.DuplicateChildComponent(HierarchicalEasingChannel);

		// Easing needs a time to evaluate
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(Easing, EvalTime);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(Easing, EasingResult);
	}

	// Weight channel relationships
	{
		// Weight channel components should be duplicated to children
		ComponentRegistry->Factories.DuplicateChildComponent(WeightChannel);
		ComponentRegistry->Factories.DuplicateChildComponent(WeightResult);

		// Weight channel components need a time and result to evaluate
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(WeightChannel, EvalTime);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(WeightChannel, WeightResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(WeightChannel, CachedWeightChannelInterpolation);
	}

	// Weight and easing result component relationship
	{
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(HierarchicalEasingChannel, WeightAndEasingResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(EasingResult, WeightAndEasingResult);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(WeightResult, WeightAndEasingResult);
	}
	
	// String channel relationships
	{
		ComponentRegistry->Factories.DuplicateChildComponent(StringChannel);
		ComponentRegistry->Factories.DuplicateChildComponent(StringResult);

		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(StringChannel, EvalTime);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(StringChannel, StringResult);
	}

	// Object path channel relationships
	{
		// Object channel components should be duplicated to children
		ComponentRegistry->Factories.DuplicateChildComponent(ObjectPathChannel);
		ComponentRegistry->Factories.DuplicateChildComponent(ObjectResult);

		// Object channel components need a time and result to evaluate
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(ObjectPathChannel, EvalTime);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(ObjectPathChannel, ObjectResult);
	}

	// Track instances always produce inputs
	{
		auto InitInput = [](const FMovieSceneTrackInstanceComponent& InInstance, FTrackInstanceInputComponent& OutInput)
		{
			OutInput.Section = InInstance.Owner;
		};
		ComponentRegistry->Factories.DefineChildComponent(TrackInstance, TrackInstanceInput, InitInput);
	}

	{
		ComponentRegistry->Factories.DefineChildComponent(EvaluationHook, EvaluationHook);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(EvaluationHook, EvalTime);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(EvaluationHook, EvaluationHookFlags);
	}

	RequiresInstantiationMask.Set(Tags.NeedsLink);
}

FBuiltInComponentTypes::~FBuiltInComponentTypes()
{
}

void FBuiltInComponentTypes::Destroy()
{
	GMovieSceneBuiltInComponentTypes.Reset();
	GMovieSceneBuiltInComponentTypesDestroyed = true;
}

FBuiltInComponentTypes* FBuiltInComponentTypes::Get()
{
	if (!GMovieSceneBuiltInComponentTypes.IsValid())
	{
		check(!GMovieSceneBuiltInComponentTypesDestroyed);
		GMovieSceneBuiltInComponentTypes.Reset(new FBuiltInComponentTypes);
	}
	return GMovieSceneBuiltInComponentTypes.Get();
}

FComponentTypeID FBuiltInComponentTypes::GetBaseValueComponentType(const FComponentTypeID& InResultComponentType)
{
	if (FComponentTypeID* BaseComponentType = ResultToBase.Find(InResultComponentType))
	{
		return *BaseComponentType;
	}
	return FComponentTypeID::Invalid();
}

} // namespace MovieScene
} // namespace UE

