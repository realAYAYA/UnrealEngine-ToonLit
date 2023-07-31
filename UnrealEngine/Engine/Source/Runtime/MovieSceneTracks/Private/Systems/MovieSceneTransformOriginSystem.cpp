// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneTransformOriginSystem.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "Tracks/IMovieSceneTransformOrigin.h"
#include "MovieSceneTracksComponentTypes.h"

#include "Systems/DoubleChannelEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "Systems/MovieSceneComponentTransformSystem.h"

#include "IMovieScenePlayer.h"
#include "IMovieScenePlaybackClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneTransformOriginSystem)

namespace UE
{
namespace MovieScene
{

struct FAssignTransformOrigin
{
	const TSparseArray<FTransform>* TransformOriginsByInstanceID;

	void ForEachAllocation(const FEntityAllocation* Allocation, TRead<FInstanceHandle> Instances, TRead<UObject*> BoundObjects,
		TWriteOptional<double> LocationX, TWriteOptional<double> LocationY, TWriteOptional<double> LocationZ,
		TWriteOptional<double> RotationX, TWriteOptional<double> RotationY, TWriteOptional<double> RotationZ,
		TWriteOptional<FSourceDoubleChannelFlags> FlagsLocationX, TWriteOptional<FSourceDoubleChannelFlags> FlagsLocationY, TWriteOptional<FSourceDoubleChannelFlags> FlagsLocationZ,
		TWriteOptional<FSourceDoubleChannelFlags> FlagsRotationX, TWriteOptional<FSourceDoubleChannelFlags> FlagsRotationY, TWriteOptional<FSourceDoubleChannelFlags> FlagsRotationZ)
	{
		TransformLocation(Instances, BoundObjects, LocationX, LocationY, LocationZ, RotationX, RotationY, RotationZ, FlagsLocationX, FlagsLocationY, FlagsLocationZ, FlagsRotationX, FlagsRotationY, FlagsRotationZ, Allocation->Num());
	}

	void TransformLocation(const FInstanceHandle* Instances, const UObject* const * BoundObjects,
		double* OutLocationX, double* OutLocationY, double* OutLocationZ,
		double* OutRotationX, double* OutRotationY, double* OutRotationZ,
		FSourceDoubleChannelFlags* OutFlagsLocationX, FSourceDoubleChannelFlags* OutFlagsLocationY, FSourceDoubleChannelFlags* OutFlagsLocationZ,
		FSourceDoubleChannelFlags* OutFlagsRotationX, FSourceDoubleChannelFlags* OutFlagsRotationY, FSourceDoubleChannelFlags* OutFlagsRotationZ,
		int32 Num)
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			FInstanceHandle InstanceHandle = Instances[Index];
			if (!TransformOriginsByInstanceID->IsValidIndex(InstanceHandle.InstanceID))
			{
				continue;
			}

			// Do not apply transform origins to attached objects
			const USceneComponent* SceneComponent = CastChecked<const USceneComponent>(BoundObjects[Index]);
			if (SceneComponent->GetAttachParent() != nullptr)
			{
				continue;
			}

			FTransform Origin = (*TransformOriginsByInstanceID)[InstanceHandle.InstanceID];

			FVector  CurrentTranslation(OutLocationX ? OutLocationX[Index] : 0.f, OutLocationY ? OutLocationY[Index] : 0.f, OutLocationZ ? OutLocationZ[Index] : 0.f);
			FRotator CurrentRotation(OutRotationY ? OutRotationY[Index] : 0.f, OutRotationZ ? OutRotationZ[Index] : 0.f, OutRotationX ? OutRotationX[Index] : 0.f);

			FTransform NewTransform = FTransform(CurrentRotation, CurrentTranslation)*Origin;

			FVector  NewTranslation = NewTransform.GetTranslation();
			FRotator NewRotation    = NewTransform.GetRotation().Rotator();

			if (OutLocationX) { OutLocationX[Index] = NewTranslation.X; }
			if (OutLocationY) { OutLocationY[Index] = NewTranslation.Y; }
			if (OutLocationZ) { OutLocationZ[Index] = NewTranslation.Z; }

			if (OutRotationX) { OutRotationX[Index] = NewRotation.Roll; }
			if (OutRotationY) { OutRotationY[Index] = NewRotation.Pitch; }
			if (OutRotationZ) { OutRotationZ[Index] = NewRotation.Yaw; }

			if (OutFlagsLocationX) { OutFlagsLocationX[Index].bNeedsEvaluate = true; }
			if (OutFlagsLocationY) { OutFlagsLocationY[Index].bNeedsEvaluate = true; }
			if (OutFlagsLocationZ) { OutFlagsLocationZ[Index].bNeedsEvaluate = true; }

			if (OutFlagsRotationX) { OutFlagsRotationX[Index].bNeedsEvaluate = true; }
			if (OutFlagsRotationY) { OutFlagsRotationY[Index].bNeedsEvaluate = true; }
			if (OutFlagsRotationZ) { OutFlagsRotationZ[Index].bNeedsEvaluate = true; }
		}
	}
};

} // namespace MovieScene
} // namespace UE


UMovieSceneTransformOriginSystem::UMovieSceneTransformOriginSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// This system relies upon anything that creates entities
		DefineImplicitPrerequisite(GetClass(), UMovieScenePiecewiseDoubleBlenderSystem::StaticClass());
		DefineImplicitPrerequisite(GetClass(), UMovieSceneComponentTransformSystem::StaticClass());

		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleResult[0]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleResult[1]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleResult[2]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleResult[3]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleResult[4]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleResult[5]);

		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleChannelFlags[0]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleChannelFlags[1]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleChannelFlags[2]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleChannelFlags[3]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleChannelFlags[4]);
		DefineComponentConsumer(GetClass(), FBuiltInComponentTypes::Get()->DoubleChannelFlags[5]);
	}
}

bool UMovieSceneTransformOriginSystem::IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const
{
	using namespace UE::MovieScene;

	for (const FSequenceInstance& Instance : InLinker->GetInstanceRegistry()->GetSparseInstances())
	{
		const IMovieScenePlaybackClient*  Client       = Instance.GetPlayer()->GetPlaybackClient();
		const UObject*                    InstanceData = Client ? Client->GetInstanceData() : nullptr;
		const IMovieSceneTransformOrigin* RawInterface = Cast<const IMovieSceneTransformOrigin>(InstanceData);

		if (RawInterface || (InstanceData && InstanceData->GetClass()->ImplementsInterface(UMovieSceneTransformOrigin::StaticClass())))
		{
			return true;
		}
	}

	return false;
}

void UMovieSceneTransformOriginSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	TransformOriginsByInstanceID.Empty(InstanceRegistry->GetSparseInstances().Num());

	const TSparseArray<FSequenceInstance>& SparseInstances = InstanceRegistry->GetSparseInstances();
	for (int32 Index = 0; Index < SparseInstances.GetMaxIndex(); ++Index)
	{
		if (!SparseInstances.IsValidIndex(Index))
		{
			continue;
		}

		const FSequenceInstance& Instance = SparseInstances[Index];

		const IMovieScenePlaybackClient*  Client       = Instance.GetPlayer()->GetPlaybackClient();
		const UObject*                    InstanceData = Client ? Client->GetInstanceData() : nullptr;
		const IMovieSceneTransformOrigin* RawInterface = Cast<const IMovieSceneTransformOrigin>(InstanceData);

		const bool bHasInterface = RawInterface || (InstanceData && InstanceData->GetClass()->ImplementsInterface(UMovieSceneTransformOrigin::StaticClass()));
		if (bHasInterface)
		{
			// Retrieve the current origin
			FTransform TransformOrigin = RawInterface ? RawInterface->GetTransformOrigin() : IMovieSceneTransformOrigin::Execute_BP_GetTransformOrigin(InstanceData);

			TransformOriginsByInstanceID.Insert(Index, TransformOrigin);
		}
	}

	if (TransformOriginsByInstanceID.Num() != 0)
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

		FEntityComponentFilter Filter;
		Filter.All({ TracksComponents->ComponentTransform.PropertyTag, BuiltInComponents->Tags.AbsoluteBlend });
		Filter.None({ BuiltInComponents->BlendChannelOutput });

		FEntityTaskBuilder()
		.Read(BuiltInComponents->InstanceHandle)
		.Read(BuiltInComponents->BoundObject)
		.WriteOptional(BuiltInComponents->DoubleResult[0])
		.WriteOptional(BuiltInComponents->DoubleResult[1])
		.WriteOptional(BuiltInComponents->DoubleResult[2])
		.WriteOptional(BuiltInComponents->DoubleResult[3])
		.WriteOptional(BuiltInComponents->DoubleResult[4])
		.WriteOptional(BuiltInComponents->DoubleResult[5])
		.WriteOptional(BuiltInComponents->DoubleChannelFlags[0])
		.WriteOptional(BuiltInComponents->DoubleChannelFlags[1])
		.WriteOptional(BuiltInComponents->DoubleChannelFlags[2])
		.WriteOptional(BuiltInComponents->DoubleChannelFlags[3])
		.WriteOptional(BuiltInComponents->DoubleChannelFlags[4])
		.WriteOptional(BuiltInComponents->DoubleChannelFlags[5])
		.CombineFilter(Filter)
		// Must contain at least one double result
		.FilterAny({ BuiltInComponents->DoubleResult[0], BuiltInComponents->DoubleResult[1], BuiltInComponents->DoubleResult[2],
			BuiltInComponents->DoubleResult[3], BuiltInComponents->DoubleResult[4], BuiltInComponents->DoubleResult[5] })
		.Dispatch_PerAllocation<FAssignTransformOrigin>(&Linker->EntityManager, InPrerequisites, &Subsequents, &TransformOriginsByInstanceID);
	}
}


