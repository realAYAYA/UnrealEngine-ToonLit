// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGHelpers.h"

#include "PCGSubsystem.h"
#include "PCGWorldActor.h"

#include "GameFramework/Actor.h"
#include "Landscape.h"
#include "UObject/UObjectIterator.h"
#include "WorldPartition/WorldPartition.h"

#if WITH_EDITOR
#include "Editor.h"
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

	FBox GetActorBounds(AActor* InActor, bool bIgnorePCGCreatedComponents)
	{
		// Specialized version of GetComponentsBoundingBox that skips over PCG generated components
		// This is to ensure stable bounds and no timing issues (cleared ISMs, etc.)
		FBox Box(EForceInit::ForceInit);

		const bool bNonColliding = true;
		const bool bIncludeFromChildActors = true;

		if (InActor)
		{
			InActor->ForEachComponent<UPrimitiveComponent>(bIncludeFromChildActors, [&](const UPrimitiveComponent* InPrimComp)
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

	FBox GetActorLocalBounds(AActor* InActor, bool bIgnorePCGCreatedComponents)
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

			InActor->ForEachComponent<UPrimitiveComponent>(bIncludeFromChildActors, [&](const UPrimitiveComponent* InPrimComp)
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
		return (GEditor && GEditor->PlayWorld) || GIsPlayInEditorWorld;
#else
		return true;
#endif // WITH_EDITOR
	}

	FBox GetLandscapeBounds(ALandscapeProxy* InLandscape)
	{
		check(InLandscape);

		if (ALandscape* Landscape = Cast<ALandscape>(InLandscape))
		{
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
			if (It->GetWorld() == InWorld)
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
			if (It->GetWorld() == InWorld)
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

	void GatherDependencies(UObject* Object, TSet<TObjectPtr<UObject>>& OutDependencies, int32 MaxDepth)
	{
		UClass* ObjectClass = Object ? Object->GetClass() : nullptr;

		if(!CanBeExpanded(ObjectClass))
		{
			return;
		}
		checkSlow(ObjectClass); // CanBeExpanded checks this but static analyzer doesn't note that

		for (FProperty* Property = ObjectClass->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
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
					const void* KeyPtr = Helper.GetKeyPtr(DynamicIndex);
					GatherDependencies(MapProperty->KeyProp, KeyPtr, OutDependencies, MaxDepth);

					const void* ValuePtr = Helper.GetValuePtr(DynamicIndex);
					GatherDependencies(MapProperty->ValueProp, ValuePtr, OutDependencies, MaxDepth);

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
}
