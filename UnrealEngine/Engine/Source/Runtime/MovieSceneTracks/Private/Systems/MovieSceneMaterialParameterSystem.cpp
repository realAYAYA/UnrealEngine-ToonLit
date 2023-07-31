// Copyright Epic Games, Inc. All Rights Reserved.

#include "Systems/MovieSceneMaterialParameterSystem.h"

#include "Systems/FloatChannelEvaluatorSystem.h"
#include "Systems/DoubleChannelEvaluatorSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "Systems/MovieSceneHierarchicalBiasSystem.h"
#include "Systems/MovieSceneMaterialSystem.h"

#include "Materials/MaterialParameterCollectionInstance.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneMaterialParameterSystem)

namespace UE::MovieScene
{

bool GMaterialParameterBlending = true;
FAutoConsoleVariableRef CVarMaterialParameterBlending(
	TEXT("Sequencer.MaterialParameterBlending"),
	GMaterialParameterBlending,
	TEXT("(Default: true) Defines whether material parameter blending should be enabled or not.\n"),
	ECVF_Default
);

bool GMaterialParameterEntityLifetimeTracking = false;
FAutoConsoleVariableRef CVarMaterialParameterEntityLifetimeTracking(
	TEXT("Sequencer.MaterialParameterEntityLifetimeTracking"),
	GMaterialParameterEntityLifetimeTracking,
	TEXT("(Default: false) Ensure on destruction that all entities have been cleaned up. This can report false positives (when the linker and material system are both cleaned up together) so is not enabled by default.\n"),
	ECVF_Default
);


void CollectGarbageForOutput(FAnimatedMaterialParameterInfo* Output)
{
	// This should only happen during garbage collection
	if (Output->OutputEntityID.IsValid())
	{
		UMovieSceneEntitySystemLinker* Linker = Output->WeakLinker.Get();
		if (Linker)
		{
			Linker->EntityManager.AddComponent(Output->OutputEntityID, FBuiltInComponentTypes::Get()->Tags.NeedsUnlink);
		}

		Output->OutputEntityID = FMovieSceneEntityID();
	}

	if (Output->BlendChannelID.IsValid())
	{
		UMovieSceneBlenderSystem* BlenderSystem = Output->WeakBlenderSystem.Get();
		if (BlenderSystem)
		{
			BlenderSystem->ReleaseBlendChannel(Output->BlendChannelID);
		}
		Output->BlendChannelID = FMovieSceneBlendChannelID();
	}
}

FAnimatedMaterialParameterInfo::~FAnimatedMaterialParameterInfo()
{
	if (GMaterialParameterEntityLifetimeTracking)
	{
		ensureAlways(!OutputEntityID.IsValid() && !BlendChannelID.IsValid());
	}
}

/** Apply scalar material parameters */
struct FApplyScalarParameters
{
	static void ForEachEntity(UObject* BoundMaterial, FName ParameterName, double InScalarValue)
	{
		// WARNING: BoundMaterial may be nullptr here
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(BoundMaterial))
		{
			MID->SetScalarParameterValue(ParameterName, (float)InScalarValue);
		}
		else if (UMaterialParameterCollectionInstance* MPCI = Cast<UMaterialParameterCollectionInstance>(BoundMaterial))
		{
			MPCI->SetScalarParameterValue(ParameterName, (float)InScalarValue);
		}
	}
};

/** Apply vector material parameters */
struct FApplyVectorParameters
{
	static void ForEachAllocation(FEntityAllocationIteratorItem Item,
		TRead<UObject*> BoundMaterials,
		TReadOneOrMoreOf<FName, FName> VectorOrColorParameterNames,
		TReadOneOrMoreOf<double, double, double, double> VectorChannels)
	{
		const int32 Num = Item.GetAllocation()->Num();
		// Use either the vector parameter name, or the color parameter name
		const FName* ParameterNames = VectorOrColorParameterNames.Get<0>() ? VectorOrColorParameterNames.Get<0>() : VectorOrColorParameterNames.Get<1>();
		const double* RESTRICT R = VectorChannels.Get<0>();
		const double* RESTRICT G = VectorChannels.Get<1>();
		const double* RESTRICT B = VectorChannels.Get<2>();
		const double* RESTRICT A = VectorChannels.Get<3>();
		
		for (int32 Index = 0; Index < Num; ++Index)
		{
			UObject* BoundMaterial = BoundMaterials[Index];
			if (!BoundMaterial)
			{
				continue;
			}

			// Default to white for unanimated channels for backwards compatibility with the legacy track
			FLinearColor Color(
				R ? (float)R[Index] : 1.f,
				G ? (float)G[Index] : 1.f,
				B ? (float)B[Index] : 1.f,
				A ? (float)A[Index] : 1.f
			);
			if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(BoundMaterial))
			{
				MID->SetVectorParameterValue(ParameterNames[Index], Color);
			}
			else if (UMaterialParameterCollectionInstance* MPCI = Cast<UMaterialParameterCollectionInstance>(BoundMaterial))
			{
				MPCI->SetVectorParameterValue(ParameterNames[Index], Color);
			}
		}
	}
};

/** Handler that manages creation of blend outputs where there are multiple contributors for the same material parameter */
struct FOverlappingMaterialParameterHandler
{
	UMovieSceneEntitySystemLinker* Linker;
	UMovieSceneMaterialParameterSystem* System;
	FComponentMask DefaultComponentMask;

	FOverlappingMaterialParameterHandler(UMovieSceneMaterialParameterSystem* InSystem)
		: Linker(InSystem->GetLinker())
		, System(InSystem)
	{}

	void InitializeOutput(UObject* BoundMaterial, FName ParameterName, TArrayView<const FMovieSceneEntityID> Inputs, FAnimatedMaterialParameterInfo* Output, FEntityOutputAggregate Aggregate)
	{
		UpdateOutput(BoundMaterial, ParameterName, Inputs, Output, Aggregate);
	}

	void UpdateOutput(UObject* BoundMaterial, FName ParameterName, TArrayView<const FMovieSceneEntityID> Inputs, FAnimatedMaterialParameterInfo* Output, FEntityOutputAggregate Aggregate)
	{
		using namespace UE::MovieScene;

		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

		const int32 NumContributors = Inputs.Num();
		if (NumContributors > 1)
		{
			if (!Output->OutputEntityID)
			{
				if (!System->DoubleBlenderSystem)
				{
					System->DoubleBlenderSystem = Linker->LinkSystem<UMovieScenePiecewiseDoubleBlenderSystem>();
					Linker->SystemGraph.AddReference(System, System->DoubleBlenderSystem);
				}

				Output->WeakLinker = Linker;
				Output->WeakBlenderSystem = System->DoubleBlenderSystem;

				// Initialize the blend channel ID
				Output->BlendChannelID = System->DoubleBlenderSystem->AllocateBlendChannel();

				// Needs blending
				Output->OutputEntityID = FEntityBuilder()
				.Add(TracksComponents->BoundMaterial, BoundMaterial)
				.Add(BuiltInComponents->BlendChannelOutput, Output->BlendChannelID)
				.AddTag(BuiltInComponents->Tags.NeedsLink)
				.AddMutualComponents()
				.CreateEntity(&Linker->EntityManager, DefaultComponentMask);

				Linker->EntityManager.CopyComponents(Inputs[0], Output->OutputEntityID, Linker->EntityManager.GetComponents()->GetCopyAndMigrationMask());
			}

			for (FMovieSceneEntityID Input : Inputs)
			{
				if (!Linker->EntityManager.HasComponent(Input, BuiltInComponents->BlendChannelInput))
				{
					Linker->EntityManager.AddComponent(Input, BuiltInComponents->BlendChannelInput, Output->BlendChannelID);
				}
				else
				{
					// If the bound material changed, we might have been re-assigned a different blend channel so make sure it's up to date
					Linker->EntityManager.WriteComponentChecked(Input, BuiltInComponents->BlendChannelInput, Output->BlendChannelID);
				}
			}
		}
		else
		{
			// Previously blended, but is no more - remove the tag from the remaining input if necessary and delete the output entity
			if (Output->OutputEntityID)
			{
				Linker->EntityManager.AddComponent(Output->OutputEntityID, BuiltInComponents->Tags.NeedsUnlink);
				Output->OutputEntityID = FMovieSceneEntityID();

				if (ensure(System->DoubleBlenderSystem))
				{
					System->DoubleBlenderSystem->ReleaseBlendChannel(Output->BlendChannelID);
				}
			}

			for (FMovieSceneEntityID Input : Inputs)
			{
				Linker->EntityManager.RemoveComponent(Input, BuiltInComponents->BlendChannelInput);
			}
			Output->BlendChannelID = FMovieSceneBlendChannelID();
		}

		Output->NumContributors = NumContributors;
	}

	void DestroyOutput(UObject* BoundMaterial, FName ParameterName, FAnimatedMaterialParameterInfo* Output, FEntityOutputAggregate Aggregate)
	{
		if (Output->OutputEntityID)
		{
			FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

			Linker->EntityManager.AddComponent(Output->OutputEntityID, BuiltInComponents->Tags.NeedsUnlink);
			Output->OutputEntityID = FMovieSceneEntityID();

			if (System->DoubleBlenderSystem)
			{
				System->DoubleBlenderSystem->ReleaseBlendChannel(Output->BlendChannelID);
			}
			Output->BlendChannelID = FMovieSceneBlendChannelID();
		}
	}

};

} // namespace UE::MovieScene

UMovieSceneMaterialParameterSystem::UMovieSceneMaterialParameterSystem(const FObjectInitializer& ObjInit)
	: Super(ObjInit)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

	RelevantComponent = TracksComponents->BoundMaterial;
	Phase = ESystemPhase::Instantiation | ESystemPhase::Evaluation;

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		DefineComponentConsumer(GetClass(), TracksComponents->BoundMaterial);

		DefineImplicitPrerequisite(UFloatChannelEvaluatorSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(UDoubleChannelEvaluatorSystem::StaticClass(), GetClass());

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(BuiltInComponents->DoubleResult); ++Index)
		{
			DefineComponentConsumer(GetClass(), BuiltInComponents->DoubleResult[Index]);
		}

		DefineImplicitPrerequisite(UMovieScenePiecewiseDoubleBlenderSystem::StaticClass(), GetClass());
		DefineImplicitPrerequisite(GetClass(), UMovieSceneHierarchicalBiasSystem::StaticClass());
	}
}

void UMovieSceneMaterialParameterSystem::OnLink()
{
	using namespace UE::MovieScene;

	ScalarParameterTracker.Initialize(this);
	VectorParameterTracker.Initialize(this);
}

void UMovieSceneMaterialParameterSystem::OnUnlink()
{
	using namespace UE::MovieScene;

	// Always reset the float blender system on link to ensure that recycled systems are correctly initialized.
	DoubleBlenderSystem = nullptr;

	FOverlappingMaterialParameterHandler Handler(this);

	ScalarParameterTracker.Destroy(Handler);
	VectorParameterTracker.Destroy(Handler);
}

void UMovieSceneMaterialParameterSystem::OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FMovieSceneEntitySystemRunner* ActiveRunner = Linker->GetActiveRunner();
	if (!ensure(ActiveRunner))
	{
		return;
	}

	ESystemPhase CurrentPhase = ActiveRunner->GetCurrentPhase();
	if (CurrentPhase == ESystemPhase::Instantiation)
	{
		OnInstantiation();
	}
	else if (CurrentPhase == ESystemPhase::Evaluation)
	{
		OnEvaluation(InPrerequisites, Subsequents);
	}
}


void UMovieSceneMaterialParameterSystem::OnInstantiation()
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes*          BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents  = FMovieSceneTracksComponentTypes::Get();

	if (GMaterialParameterBlending)
	{
		auto HandleUnlinkedAllocation = [this](const FEntityAllocation* Allocation)
		{
			this->ScalarParameterTracker.VisitUnlinkedAllocation(Allocation);
			this->VectorParameterTracker.VisitUnlinkedAllocation(Allocation);
		};

		auto HandleUpdatedAllocation = [this](const FEntityAllocation* Allocation, TRead<UObject*> InObjects, TReadOneOf<FName, FName, FName> ScalarVectorOrColorParameterNames)
		{
			if (TComponentPtr<const FName> ScalarParameterNames = TComponentPtr<const FName>(ScalarVectorOrColorParameterNames.Get<0>()))
			{
				this->ScalarParameterTracker.VisitActiveAllocation(Allocation, InObjects, ScalarParameterNames);
			}
			else if (TComponentPtr<const FName> VectorParameterNames = TComponentPtr<const FName>(ScalarVectorOrColorParameterNames.Get<1>()))
			{
				this->VectorParameterTracker.VisitActiveAllocation(Allocation, InObjects, VectorParameterNames);
			}
			else if (TComponentPtr<const FName> ColorParameterNames = TComponentPtr<const FName>(ScalarVectorOrColorParameterNames.Get<2>()))
			{
				this->VectorParameterTracker.VisitActiveAllocation(Allocation, InObjects, ColorParameterNames);
			}
		};

		// First step handle any new or updated bound materials
		FEntityTaskBuilder()
		.Read(TracksComponents->BoundMaterial)
		.ReadOneOf(TracksComponents->ScalarParameterName, TracksComponents->VectorParameterName, TracksComponents->ColorParameterName)
		.FilterAny({ BuiltInComponents->Tags.NeedsLink, TracksComponents->Tags.BoundMaterialChanged })
		.FilterNone({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerAllocation(&Linker->EntityManager, HandleUpdatedAllocation);

		// Next handle any entities that are going away
		FEntityTaskBuilder()
		.FilterAll({ BuiltInComponents->Tags.NeedsUnlink })
		.Iterate_PerAllocation(&Linker->EntityManager, HandleUnlinkedAllocation);

		// Process all blended scalar parameters
		FOverlappingMaterialParameterHandler ScalarHandler(this);
		ScalarHandler.DefaultComponentMask.Set(BuiltInComponents->DoubleResult[0]);
		ScalarParameterTracker.ProcessInvalidatedOutputs(Linker, ScalarHandler);

		// Process all blended vector parameters
		FOverlappingMaterialParameterHandler VectorHandler(this);
		VectorHandler.DefaultComponentMask.SetAll({ BuiltInComponents->DoubleResult[0], BuiltInComponents->DoubleResult[1], BuiltInComponents->DoubleResult[2], BuiltInComponents->DoubleResult[3] });
		VectorParameterTracker.ProcessInvalidatedOutputs(Linker, VectorHandler);
	}
}

void UMovieSceneMaterialParameterSystem::OnEvaluation(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents)
{
	using namespace UE::MovieScene;

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
	FMovieSceneTracksComponentTypes* TracksComponents = FMovieSceneTracksComponentTypes::Get();

	if (Linker->EntityManager.ContainsComponent(TracksComponents->ScalarParameterName))
	{
		FEntityTaskBuilder()
		.Read(TracksComponents->BoundMaterial)
		.Read(TracksComponents->ScalarParameterName)
		.Read(BuiltInComponents->DoubleResult[0])
		.FilterNone({ BuiltInComponents->BlendChannelInput })
		.SetDesiredThread(Linker->EntityManager.GetDispatchThread())
		.Dispatch_PerEntity<FApplyScalarParameters>(&Linker->EntityManager, InPrerequisites, &Subsequents);
	}

	// Vectors and colors use the same API
	if (Linker->EntityManager.ContainsComponent(TracksComponents->VectorParameterName) || Linker->EntityManager.ContainsComponent(TracksComponents->ColorParameterName))
	{
		FEntityTaskBuilder()
		.Read(TracksComponents->BoundMaterial)
		.ReadOneOrMoreOf(TracksComponents->VectorParameterName, TracksComponents->ColorParameterName)
		.ReadOneOrMoreOf(BuiltInComponents->DoubleResult[0], BuiltInComponents->DoubleResult[1], BuiltInComponents->DoubleResult[2], BuiltInComponents->DoubleResult[3])
		.FilterNone({ BuiltInComponents->BlendChannelInput })
		.SetDesiredThread(Linker->EntityManager.GetDispatchThread())
		.Dispatch_PerAllocation<FApplyVectorParameters>(&Linker->EntityManager, InPrerequisites, &Subsequents);
	}
}

