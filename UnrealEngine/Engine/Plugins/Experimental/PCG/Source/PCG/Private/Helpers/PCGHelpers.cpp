// Copyright Epic Games, Inc. All Rights Reserved.

#include "Helpers/PCGHelpers.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGModule.h"
#include "PCGSubsystem.h"
#include "PCGWorldActor.h"
#include "Grid/PCGPartitionActor.h"

#include "Landscape.h"
#include "Algo/AnyOf.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Editor.h"
#else
#include "Engine/World.h"
#endif

namespace PCGHelpers
{
	int ComputeSeed(int A)
	{
		return (A * 196314165U) + 907633515U;
	}

	int ComputeSeed(int A, int B)
	{
		return ((A * 196314165U) + 907633515U) ^ ((B * 73148459U) + 453816763U);
	}

	int ComputeSeed(int A, int B, int C)
	{
		return ((A * 196314165U) + 907633515U) ^ ((B * 73148459U) + 453816763U) ^ ((C * 34731343U) + 453816743U);
	}

	bool IsInsideBounds(const FBox& InBox, const FVector& InPosition)
	{
		return (InPosition.X >= InBox.Min.X) && (InPosition.X < InBox.Max.X) &&
			(InPosition.Y >= InBox.Min.Y) && (InPosition.Y < InBox.Max.Y) &&
			(InPosition.Z >= InBox.Min.Z) && (InPosition.Z < InBox.Max.Z);
	}

	bool IsInsideBoundsXY(const FBox& InBox, const FVector& InPosition)
	{
		return (InPosition.X >= InBox.Min.X) && (InPosition.X < InBox.Max.X) &&
			(InPosition.Y >= InBox.Min.Y) && (InPosition.Y < InBox.Max.Y);
	}

	FBox OverlapBounds(const FBox& InA, const FBox& InB)
	{
		if (!InA.IsValid || !InB.IsValid)
		{
			return FBox(EForceInit::ForceInit);
		}
		else
		{
			return InA.Overlap(InB);
		}
	}

	FBox GetGridBounds(const AActor* Actor, const UPCGComponent* Component)
	{
		FBox Bounds(EForceInit::ForceInit);

		if (const APCGPartitionActor* PartitionActor = Cast<const APCGPartitionActor>(Actor))
		{
			// First, get the bounds from the partition actor
			Bounds = PartitionActor->GetFixedBounds();

			const UPCGComponent* OriginalComponent = Component ? PartitionActor->GetOriginalComponent(Component) : nullptr;
			if (OriginalComponent)
			{
				if (OriginalComponent->GetOwner() != PartitionActor)
				{
					Bounds = Bounds.Overlap(OriginalComponent->GetGridBounds());
				}
			}
		}
		// TODO: verify this works as expected in non-editor builds
		else if (const ALandscapeProxy* LandscapeActor = Cast<const ALandscape>(Actor))
		{
			Bounds = GetLandscapeBounds(LandscapeActor);
		}
		else if (Actor)
		{
			Bounds = GetActorBounds(Actor);
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Actor is invalid in GetGridBounds"));
		}

		return Bounds;
	}

	FBox GetActorBounds(const AActor* InActor, bool bIgnorePCGCreatedComponents)
	{
		// Specialized version of GetComponentsBoundingBox that skips over PCG generated components
		// This is to ensure stable bounds and no timing issues (cleared ISMs, etc.)
		FBox Box(EForceInit::ForceInit);

		const bool bNonColliding = true;
		const bool bIncludeFromChildActors = true;

		if (InActor)
		{
			InActor->ForEachComponent<UPrimitiveComponent>(bIncludeFromChildActors, [bNonColliding, bIgnorePCGCreatedComponents, &Box](const UPrimitiveComponent* InPrimComp)
			{
				// Note: we omit the IsRegistered check here (e.g. InPrimComp->IsRegistered() )
				// since this can be called in a scope where the components are temporarily unregistered
				if ((bNonColliding || InPrimComp->IsCollisionEnabled()) &&
					(!bIgnorePCGCreatedComponents || !InPrimComp->ComponentTags.Contains(DefaultPCGTag)))
				{
					Box += InPrimComp->Bounds.GetBox();
				}
			});
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Actor is invalid in GetActorBounds"));
		}

		return Box;
	}

	FBox GetActorLocalBounds(const AActor* InActor, bool bIgnorePCGCreatedComponents)
	{
		// Specialized version of CalculateComponentsBoundingBoxInLocalScape that skips over PCG generated components
		// This is to ensure stable bounds and no timing issues (cleared ISMs, etc.)
		FBox Box(EForceInit::ForceInit);

		const bool bNonColliding = true;
		const bool bIncludeFromChildActors = true;

		if (InActor)
		{
			const FTransform& ActorToWorld = InActor->GetTransform();
			const FTransform WorldToActor = ActorToWorld.Inverse();

			InActor->ForEachComponent<UPrimitiveComponent>(bIncludeFromChildActors, [bNonColliding, bIgnorePCGCreatedComponents, &WorldToActor, &Box](const UPrimitiveComponent* InPrimComp)
			{
				if ((bNonColliding || InPrimComp->IsCollisionEnabled()) &&
					(!bIgnorePCGCreatedComponents || !InPrimComp->ComponentTags.Contains(DefaultPCGTag)))
				{
					const FTransform ComponentToActor = InPrimComp->GetComponentTransform() * WorldToActor;
					Box += InPrimComp->CalcBounds(ComponentToActor).GetBox();
				}
			});
		}
		else
		{
			UE_LOG(LogPCG, Error, TEXT("Actor is invalid in GetActorLocalBounds"));
		}

		return Box;
	}

	bool IsRuntimeOrPIE()
	{
#if WITH_EDITOR
		return (GEditor && GEditor->PlayWorld) || GIsPlayInEditorWorld || IsRunningGame();
#else
		return true;
#endif // WITH_EDITOR
	}

	FBox GetLandscapeBounds(const ALandscapeProxy* InLandscape)
	{
		check(InLandscape);

		if (const ALandscape* Landscape = Cast<const ALandscape>(InLandscape))
		{
			// If the landscape isn't done being loaded, we're very unlikely to want to interact with it
			if (Landscape->GetLandscapeInfo() == nullptr)
			{
				return FBox(EForceInit::ForceInit);
			}

#if WITH_EDITOR
			if (!IsRuntimeOrPIE())
			{
				return Landscape->GetCompleteBounds();
			}
			else
#endif
			{
				return Landscape->GetLoadedBounds();
			}
		}
		else
		{
			return GetActorBounds(InLandscape);
		}
	}

	TArray<TWeakObjectPtr<ALandscapeProxy>> GetAllLandscapeProxies(UWorld* InWorld)
	{
		TArray<TWeakObjectPtr<ALandscapeProxy>> LandscapeProxies;

		for (TObjectIterator<ALandscapeProxy> It; It; ++It)
		{
			if (It->GetWorld() == InWorld)
			{
				LandscapeProxies.Add(*It);
			}
		}

		return LandscapeProxies;
	}

	ALandscape* GetLandscape(UWorld* InWorld, const FBox& InBounds)
	{
		ALandscape* Landscape = nullptr;

		if (!InBounds.IsValid)
		{
			return Landscape;
		}

		for (TObjectIterator<ALandscape> It; It; ++It)
		{
			if (IsValid(*It) && It->GetWorld() == InWorld)
			{
				const FBox LandscapeBounds = GetLandscapeBounds(*It);
				if (LandscapeBounds.IsValid && LandscapeBounds.Intersect(InBounds))
				{
					Landscape = (*It);
					break;
				}
			}
		}

		return Landscape;
	}

	TArray<TWeakObjectPtr<ALandscapeProxy>> GetLandscapeProxies(UWorld* InWorld, const FBox& InBounds)
	{
		TArray<TWeakObjectPtr<ALandscapeProxy>> LandscapeProxies;

		if (!InBounds.IsValid)
		{
			return LandscapeProxies;
		}

		for (TObjectIterator<ALandscapeProxy> It; It; ++It)
		{
			if (IsValid(*It) && It->GetWorld() == InWorld)
			{
				const FBox LandscapeBounds = GetLandscapeBounds(*It);
				if (LandscapeBounds.IsValid && LandscapeBounds.Intersect(InBounds))
				{
					LandscapeProxies.Add(*It);
				}
			}
		}

		return LandscapeProxies;
	}

	APCGWorldActor* GetPCGWorldActor(UWorld* InWorld)
	{
		return (InWorld && InWorld->GetSubsystem<UPCGSubsystem>()) ? InWorld->GetSubsystem<UPCGSubsystem>()->GetPCGWorldActor() : nullptr;
	}

	TArray<FString> GetStringArrayFromCommaSeparatedString(const FString& InCommaSeparatedString)
	{
		TArray<FString> Result;
		InCommaSeparatedString.ParseIntoArrayWS(Result, TEXT(","));
		return Result;
	}

#if WITH_EDITOR
	bool CanBeExpanded(UClass* ObjectClass)
	{
		// There shouldn't be any need to dig through Niagara assets + there are some issues (most likely related to loading) with parsing all their dependencies
		if (!ObjectClass ||
			ObjectClass->GetFName() == TEXT("NiagaraSystem") ||
			ObjectClass->GetFName() == TEXT("NiagaraComponent"))
		{
			return false;
		}

		return true;
	}

	void GatherDependencies(UObject* Object, TSet<TObjectPtr<UObject>>& OutDependencies, int32 MaxDepth, const TArray<UClass*>& InExcludedClasses)
	{
		UClass* ObjectClass = Object ? Object->GetClass() : nullptr;

		if(!CanBeExpanded(ObjectClass))
		{
			return;
		}
		checkSlow(ObjectClass); // CanBeExpanded checks this but static analyzer doesn't note that

		for (FProperty* Property = ObjectClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
			// Skip any kind of internal property and the ones that are susceptible to be instable
			if (!Property || Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_Deprecated))
			{
				continue;
			}

			// For Object properties, if the class is in the excluded classes, continue
			if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
			{
				if (ObjectProperty->PropertyClass && Algo::AnyOf(InExcludedClasses, [InClass = ObjectProperty->PropertyClass](const UClass* ExcludedClass){ return InClass->IsChildOf(ExcludedClass); }))
				{
					continue;
				}
			}

			GatherDependencies(Property, Object, OutDependencies, MaxDepth);
		}
	}

	// Inspired by IteratePropertiesRecursive in ObjectPropertyTrace.cpp
	void GatherDependencies(FProperty* Property, const void* InContainer, TSet<TObjectPtr<UObject>>& OutDependencies, int32 MaxDepth)
	{
		auto AddToDependenciesAndGatherRecursively = [&OutDependencies, MaxDepth](UObject* Object) {
			if (Object && !OutDependencies.Contains(Object))
			{
				OutDependencies.Add(Object);
				if (MaxDepth != 0)
				{
					GatherDependencies(Object, OutDependencies, MaxDepth - 1);
				}
			}
		};

		if (!InContainer || !Property)
		{
			return;
		}

		if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			UObject* Object = ObjectProperty->GetPropertyValue_InContainer(InContainer);
			AddToDependenciesAndGatherRecursively(Object);
		}
		else if (FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
		{
			FWeakObjectPtr WeakObject = WeakObjectProperty->GetPropertyValue_InContainer(InContainer);
			AddToDependenciesAndGatherRecursively(WeakObject.Get());
		}
		else if (FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
		{
			FSoftObjectPtr SoftObject = SoftObjectProperty->GetPropertyValue_InContainer(InContainer);
			AddToDependenciesAndGatherRecursively(SoftObject.Get());
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			const void* StructContainer = StructProperty->ContainerPtrToValuePtr<const void>(InContainer);
			for (TFieldIterator<FProperty> It(StructProperty->Struct); It; ++It)
			{
				GatherDependencies(*It, StructContainer, OutDependencies, MaxDepth);
			}
		}
		else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper_InContainer Helper(ArrayProperty, InContainer);
			for (int32 DynamicIndex = 0; DynamicIndex < Helper.Num(); ++DynamicIndex)
			{
				const void* ValuePtr = Helper.GetRawPtr(DynamicIndex);
				GatherDependencies(ArrayProperty->Inner, ValuePtr, OutDependencies, MaxDepth);
			}
		}
		else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper_InContainer Helper(MapProperty, InContainer);
			int32 Num = Helper.Num();
			for (int32 DynamicIndex = 0; Num; ++DynamicIndex)
			{
				if (Helper.IsValidIndex(DynamicIndex))
				{
					// Key and Value are stored next to each other in memory.
					// ValueProp has an offset, so we should use the same starting address for both.
					const void* PairKeyValuePtr = Helper.GetKeyPtr(DynamicIndex);
					GatherDependencies(MapProperty->KeyProp, PairKeyValuePtr, OutDependencies, MaxDepth);
					GatherDependencies(MapProperty->ValueProp, PairKeyValuePtr, OutDependencies, MaxDepth);

					--Num;
				}
			}
		}
		else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper_InContainer Helper(SetProperty, InContainer);
			int32 Num = Helper.Num();
			for (int32 DynamicIndex = 0; Num; ++DynamicIndex)
			{
				if (Helper.IsValidIndex(DynamicIndex))
				{
					const void* ValuePtr = Helper.GetElementPtr(DynamicIndex);
					GatherDependencies(SetProperty->ElementProp, ValuePtr, OutDependencies, MaxDepth);

					--Num;
				}
			}
		}
	}
#endif

	bool IsNewObjectAndNotDefault(const UObject* InObject, bool bCheckHierarchy)
	{
		const UObject* CurrentInspectedObject = InObject;
		while (bCheckHierarchy && CurrentInspectedObject && CurrentInspectedObject->HasAnyFlags(RF_DefaultSubObject))
		{
			CurrentInspectedObject = CurrentInspectedObject->GetOuter();
		}

		// We detect new objects if they are not a default object/archetype and/or they do not need load.
		// In some cases, where the component is a default sub object (like APCGVolume), it has no loading flags
		// even if it is loading, so we use the outer found above.
		return CurrentInspectedObject && !CurrentInspectedObject->HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad | RF_NeedPostLoad);
	}

	bool GetGenerationGridSizes(const UPCGGraph* InGraph, const APCGWorldActor* InWorldActor, PCGHiGenGrid::FSizeArray& OutGridSizes, bool& bOutHasUnbounded)
	{
		bOutHasUnbounded = false;

		if (InGraph && InGraph->IsHierarchicalGenerationEnabled())
		{
			InGraph->GetGridSizes(OutGridSizes, bOutHasUnbounded);
			return true;
		}
		else if (InWorldActor)
		{
			OutGridSizes.Add(InWorldActor->PartitionGridSize);
			return true;
		}
		else
		{
			OutGridSizes.Add(PCGHiGenGrid::UnboundedGridSize());
			return true;
		}
	}

#if WITH_EDITOR
	void GetGeneratedActorsFolderPath(const AActor* InTargetActor, FString& OutFolderPath)
	{
		if (!InTargetActor)
		{
			return;
		}

		// Reserves reasonable max string length on stack, overflows to heap if exceeded.
		TStringBuilderWithBuffer<TCHAR, 1024> GeneratedActorsFolder;

		FName TargetActorFolder = InTargetActor->GetFolderPath();
		if (TargetActorFolder != NAME_None)
		{
			GeneratedActorsFolder << TargetActorFolder.ToString() << "/";
		}

		GeneratedActorsFolder << InTargetActor->GetActorLabel() << "_Generated";
		OutFolderPath = GeneratedActorsFolder;
	}
#endif

	void AttachToParent(AActor* InActorToAttach, AActor* InParent, EPCGAttachOptions AttachOptions, const FString& InGeneratedPath)
	{
		if (!InParent)
		{
			return;
		}

		if (AttachOptions == EPCGAttachOptions::Attached)
		{
			InActorToAttach->AttachToActor(InParent, FAttachmentTransformRules::KeepWorldTransform);
		}
#if WITH_EDITOR
		else if(AttachOptions == EPCGAttachOptions::InFolder)
		{
			FString DefaultFolderPath;

			if (InGeneratedPath.IsEmpty())
			{
				GetGeneratedActorsFolderPath(InParent, DefaultFolderPath);
			}

			const FString& FolderPath = (InGeneratedPath.IsEmpty() ? DefaultFolderPath : InGeneratedPath);
			InActorToAttach->SetFolderPath(*FolderPath);
		}
#endif
	}
}
