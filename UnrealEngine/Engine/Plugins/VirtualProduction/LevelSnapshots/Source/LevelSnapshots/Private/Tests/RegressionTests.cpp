// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include "Selection/PropertySelectionMap.h"
#include "Util/SnapshotTestRunner.h"
#include "Types/SnapshotTestActor.h"
#include "Types/ActorWithReferencesInCDO.h"

#include "Engine/World.h"
#include "Misc/AutomationTest.h"

// Bug fixes should generally be tested. Put tests for bug fixes here.

namespace UE::LevelSnapshots::Private::Tests
{
	/**
	* FTakeClassDefaultObjectSnapshotArchive used to crash when a class CDO contained a collection of object references. Make sure it does not crash and restores.
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContainersWithObjectReferencesInCDO, "VirtualProduction.LevelSnapshots.Snapshot.Regression.ContainersWithObjectReferencesInCDO", (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter));
	bool FContainersWithObjectReferencesInCDO::RunTest(const FString& Parameters)
	{
		AActorWithReferencesInCDO* Actor = nullptr;

		FSnapshotTestRunner()
			.ModifyWorld([&](UWorld* World)
			{
				Actor = World->SpawnActor<AActorWithReferencesInCDO>();
			})
			.TakeSnapshot()
			.ModifyWorld([&](UWorld* World)
			{
				Actor->SetAllPropertiesTo(Actor->CylinderMesh);
			})
			.ApplySnapshot()
			.RunTest([&]()
			{
				TestTrue(TEXT("Object properties restored correctly"), Actor->DoAllPropertiesPointTo(Actor->CubeMesh));
			});
	
		return true;
	}

	namespace RestoreCollision
	{
		static bool AreCollisionResponsesEqual(ASnapshotTestActor* Instance, ECollisionResponse Response, int32 NumChannels)
		{
			const FCollisionResponse& Responses = Instance->StaticMeshComponent->BodyInstance.GetCollisionResponse();
			for (int32 i = 0; i < NumChannels; ++i)
			{
				if (Responses.GetResponse(static_cast<ECollisionChannel>(i)) != Response)
				{
					return false;
				}
			}
			return true;
		}
		static bool WereAllCollisionResponsesRestored(ASnapshotTestActor* Instance, ECollisionResponse Response)
		{
			return AreCollisionResponsesEqual(Instance, Response, 32);
		}

		static bool WereCollisionProfileResponsesRestored(ASnapshotTestActor* Instance, ECollisionResponse Response)
		{
			return AreCollisionResponsesEqual(Instance, Response, static_cast<int32>(ECollisionChannel::ECC_EngineTraceChannel1));
		}
	}

	/**
	* Multiple bugs:
	*	- FBodyInstance::ObjectType, FBodyInstance::CollisionEnabled and FBodyInstance::CollisionResponses should not be diffed when UStaticMeshComponent::bUseDefaultCollision == true
	*	- Other FBodyInstance properties should still diff and restore correctly
	*	- After restoration, transient property FCollisionResponse::ResponseToChannels should contain the correct values
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRestoreCollision, "VirtualProduction.LevelSnapshots.Snapshot.Regression.RestoreCollision", (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter));
	bool FRestoreCollision::RunTest(const FString& Parameters)
	{
		ASnapshotTestActor* CustomBlockAllToOverlapAll = nullptr;
		ASnapshotTestActor* DefaultWithChangedIgnoredProperties = nullptr;
		ASnapshotTestActor* DefaultToCustom = nullptr;
		ASnapshotTestActor* CustomToDefault = nullptr;
		ASnapshotTestActor* ChangeCollisionProfile = nullptr;

		FSnapshotTestRunner()
			.ModifyWorld([&](UWorld* World)
			{
				CustomBlockAllToOverlapAll = ASnapshotTestActor::Spawn(World, "CustomBlockAllToOverlapAll");
				DefaultWithChangedIgnoredProperties = ASnapshotTestActor::Spawn(World, "DefaultWithChangedIgnoredProperties");
				DefaultToCustom = ASnapshotTestActor::Spawn(World, "DefaultToCustom");
				CustomToDefault = ASnapshotTestActor::Spawn(World, "CustomToDefault");
				ChangeCollisionProfile = ASnapshotTestActor::Spawn(World, "ChangeCollisionProfile");

				CustomBlockAllToOverlapAll->StaticMeshComponent->bUseDefaultCollision = false;
				CustomBlockAllToOverlapAll->StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Block);
			
				DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetCollisionEnabled(ECollisionEnabled::NoCollision);
				DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetObjectType(ECC_WorldStatic);
				DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetResponseToAllChannels(ECR_Ignore);
				DefaultWithChangedIgnoredProperties->StaticMeshComponent->bUseDefaultCollision = true;
			
				DefaultToCustom->StaticMeshComponent->bUseDefaultCollision = true;
			
				CustomToDefault->StaticMeshComponent->bUseDefaultCollision = false;
				CustomToDefault->StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);

				ChangeCollisionProfile->StaticMeshComponent->SetCollisionProfileName(FName("OverlapAll"));
			})
			.TakeSnapshot()
			.ModifyWorld([&](UWorld* World)
			{
				CustomBlockAllToOverlapAll->StaticMeshComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

				DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
				DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetObjectType(ECC_WorldDynamic);
				DefaultWithChangedIgnoredProperties->StaticMeshComponent->BodyInstance.SetResponseToAllChannels(ECR_Overlap);

				DefaultToCustom->StaticMeshComponent->SetCollisionProfileName(FName("OverlapAll"));

				CustomToDefault->StaticMeshComponent->bUseDefaultCollision = true;

				ChangeCollisionProfile->StaticMeshComponent->SetCollisionProfileName(FName("BlockAll"));
			})

			.FilterProperties(DefaultWithChangedIgnoredProperties, [&](const FPropertySelectionMap& SelectionMap)
			{
				TestTrue(TEXT("Collision properties ignored when default collision is used"), !SelectionMap.HasChanges(DefaultWithChangedIgnoredProperties));
			})
			.ApplySnapshot()
			.RunTest([&]()
			{
				TestTrue(TEXT("CustomBlockAllToOverlapAll: SetResponseToAllChannels restored"), RestoreCollision::WereAllCollisionResponsesRestored(CustomBlockAllToOverlapAll, ECR_Block));
			
				TestTrue(TEXT("Default collision restored"), DefaultToCustom->StaticMeshComponent->bUseDefaultCollision);
			
				TestTrue(TEXT("CustomToDefault: SetResponseToAllChannels restored"), RestoreCollision::WereAllCollisionResponsesRestored(CustomToDefault, ECR_Ignore));
			
				TestEqual(TEXT("Profile name restored"), ChangeCollisionProfile->StaticMeshComponent->GetCollisionProfileName(), FName("OverlapAll"));
				TestTrue(TEXT("Collision profile responses restored"), RestoreCollision::WereCollisionProfileResponsesRestored(ChangeCollisionProfile, ECR_Overlap));
			});
	
		return true;
	}

	/**
	* Suppose snapshot contains Root > Child and now the hierarchy is Child > Root. This used to cause a crash.
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUpdateAttachChildrenInfiniteLoop, "VirtualProduction.LevelSnapshots.Snapshot.Regression.UpdateAttachChildrenInfiniteLoop", (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter));
	bool FUpdateAttachChildrenInfiniteLoop::RunTest(const FString& Parameters)
	{
		ASnapshotTestActor* Root = nullptr;
		ASnapshotTestActor* Child = nullptr;
	
		FSnapshotTestRunner()
			.ModifyWorld([&](UWorld* World)
			{
				Root = ASnapshotTestActor::Spawn(World, "Root");
				Child = ASnapshotTestActor::Spawn(World, "Child");

				Child->AttachToActor(Root, FAttachmentTransformRules::KeepRelativeTransform);
			})
			.TakeSnapshot()
			.ModifyWorld([&](UWorld* World)
			{
				Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				Root->AttachToActor(Child, FAttachmentTransformRules::KeepRelativeTransform);
			})
			// If the issue remains, ApplySnapshot will now cause a stack overflow crash 
			.ApplySnapshot()
			.RunTest([&]()
			{
				TestTrue(TEXT("Root: attach component"), Root->GetRootComponent()->GetAttachParent() == nullptr);
				TestTrue(TEXT("Root: attach actor"), Root->GetAttachParentActor() == nullptr);
			
				TestTrue(TEXT("Child: attach component"), Child->GetRootComponent()->GetAttachParent() == Root->GetRootComponent());
				TestTrue(TEXT("Child: attach actor"), Child->GetAttachParentActor() == Root);
			});

		return true;
	}

	/**
	* Spawn naked AActor and add instanced components. RootComponent needs to be set.
	*/
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRestoreRootComponent, "VirtualProduction.LevelSnapshots.Snapshot.Regression.RestoreRootComponent", (EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter));
	bool FRestoreRootComponent::RunTest(const FString& Parameters)
	{
		AActor* Actor = nullptr;
		USceneComponent* InstancedComponent = nullptr;
	
		FSnapshotTestRunner()
			.ModifyWorld([&](UWorld* World)
			{
				Actor = World->SpawnActor<AActor>();

				InstancedComponent = NewObject<USceneComponent>(Actor, USceneComponent::StaticClass(), "InstancedComponent");
				Actor->AddInstanceComponent(InstancedComponent);
				Actor->SetRootComponent(InstancedComponent);
			})
			.TakeSnapshot()
			.ModifyWorld([&](UWorld* World)
			{
				InstancedComponent->DestroyComponent();
			})
			.ApplySnapshot()
			.RunTest([&]()
			{
				TestTrue(TEXT("RootComponent"), Actor->GetRootComponent() != nullptr && Actor->GetRootComponent()->GetFName().IsEqual("InstancedComponent"));
			});

		return true;
	}
}