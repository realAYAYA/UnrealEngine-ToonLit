// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableInstanceLODManagement.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Containers/Array.h"
#include "Containers/EnumAsByte.h"
#include "Containers/Map.h"
#include "Engine/EngineTypes.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformCrt.h"
#include "Kismet/GameplayStatics.h"
#include "Math/BoxSphereBounds.h"
#include "Math/Matrix.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "PerPlatformProperties.h"
#include "SceneManagement.h"
#include "Serialization/StructuredArchiveAdapters.h"
#include "Templates/Casts.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "LevelEditorViewport.h"
#endif

static TAutoConsoleVariable<int32> CVarNumGeneratedInstancesLimit(
	TEXT("b.NumGeneratedInstancesLimit"),
	0,
	TEXT("If different than 0, limit the number of mutable instances with full LODs to have at a given time."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarNumGeneratedInstancesLimitLOD1(
	TEXT("b.NumGeneratedInstancesLimitLOD1"),
	0,
	TEXT("If different than 0, limit the number of mutable instances with LOD 1 to have at a given time."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarNumGeneratedInstancesLimitLOD2(
	TEXT("b.NumGeneratedInstancesLimitLOD2"),
	0,
	TEXT("If different than 0, limit the number of mutable instances with LOD 2 to have at a given time."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarDistanceForFixedLOD2(
	TEXT("b.DistanceForFixedLOD2"),
	50000,
	TEXT("If NumGeneratedInstancesLimit is different than 0, sets the distance at which the system will fix the LOD of an instance to the lowest res one (LOD2) to prevent unnecessary LOD changes and memory consumption"),
	ECVF_Scalability);


UCustomizableInstanceLODManagement::UCustomizableInstanceLODManagement() : UCustomizableInstanceLODManagementBase()
{
	bOnlyUpdateCloseCustomizableObjects = false;
	CloseCustomizableObjectsDist = 2000.f;
}


UCustomizableInstanceLODManagement::~UCustomizableInstanceLODManagement()
{

}

// Used to manually update distances used in "OnlyUpdateCloseCustomizableObjects" system. If OnlyForInstance is null, all instances have their distance updated
// ViewCenter is the origin where the distances will be measured from.
void UpdatePawnToInstancesDistances(const class UCustomizableObjectInstance* OnlyForInstance, const TWeakObjectPtr<const AActor> ViewCenter)
{
	for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
	{
		if (CustomizableSkeletalComponent->IsValidLowLevel() && (OnlyForInstance == nullptr || CustomizableSkeletalComponent->CustomizableObjectInstance == OnlyForInstance))
		{
			CustomizableSkeletalComponent->UpdateDistFromComponentToPlayer(ViewCenter.IsValid() ? ViewCenter.Get() : nullptr, OnlyForInstance != nullptr);
		}
	}
}

#if WITH_EDITOR
// Used to manually update instances in the level editor
void UpdateCameraToInstancesDistance(const FVector CameraPosition)
{
	for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
	{
		if (CustomizableSkeletalComponent->IsValidLowLevel() && !CustomizableSkeletalComponent->IsTemplate())
		{
			CustomizableSkeletalComponent->UpdateDistFromComponentToLevelEditorCamera(CameraPosition);
		}
	}
}
#endif


void UCustomizableInstanceLODManagement::UpdateInstanceDistsAndLODs()
{
	UCustomizableObjectSystem* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance();

	if (ViewCenters.Num() == 0) // Just use the first pawn
	{
		UCustomizableSkeletalComponent* FirstCustomizableSkeletalComponent = nullptr;

#if WITH_EDITOR
		bool bLevelEditorInstancesUpdated = false;
#endif

		for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
		{
			if (CustomizableSkeletalComponent && !CustomizableSkeletalComponent->IsTemplate())
			{
				UWorld* LocalWorld = CustomizableSkeletalComponent->GetWorld();
				APlayerController* Controller = LocalWorld ? LocalWorld->GetFirstPlayerController() : nullptr;
				TWeakObjectPtr<const AActor> ViewCenter = Controller ? TWeakObjectPtr<const AActor>(Controller->GetPawn()) : nullptr;

				if (ViewCenter.IsValid())
				{
					UpdatePawnToInstancesDistances(nullptr, ViewCenter);
					break;
				}

#if WITH_EDITOR
				else if (CustomizableSkeletalComponent->GetWorld())
				{
					EWorldType::Type worldType = CustomizableSkeletalComponent->GetWorld()->WorldType;
					
					// Level Editor Instances (non PIE)
					if (!bLevelEditorInstancesUpdated && worldType == EWorldType::Editor)
					{
						for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
						{
							if (LevelVC && LevelVC->IsPerspective())
							{
								UpdateCameraToInstancesDistance(LevelVC->GetViewLocation());
								bLevelEditorInstancesUpdated = true;
								break;
							}
						}
					}

					// Blueprint instances
					else if (worldType == EWorldType::EditorPreview)
					{
						CustomizableSkeletalComponent->EditorUpdateComponent();
					}
				}
#endif // WITH_EDITOR

			}
		}
	}
	else
	{
		for (const TWeakObjectPtr<const AActor> ViewCenter : ViewCenters)
		{
			if (ViewCenter.IsValid())
			{
				UpdatePawnToInstancesDistances(nullptr, ViewCenter);
			}
		}
	}

	int32 NumGeneratedInstancesLimitLODs = GetNumGeneratedInstancesLimitFullLODs();

	if (NumGeneratedInstancesLimitLODs > 0)
	{
		int32 NumGeneratedInstancesLimitLOD1 = GetNumGeneratedInstancesLimitLOD1();
		int32 NumGeneratedInstancesLimitLOD2 = GetNumGeneratedInstancesLimitLOD2();

		TArray<UCustomizableObjectInstance*> SortedInstances;

		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValidChecked(*CustomizableObjectInstance) &&
				CustomizableObjectInstance->GetPrivate() &&
				CustomizableObjectInstance->GetIsBeingUsedByComponentInPlay())
			{
				if (const UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
					CustomizableObject &&
					!CustomizableObject->IsLocked())
				{
					UCustomizableObjectInstance* Ptr = *CustomizableObjectInstance;
					Ptr->SetIsDiscardedBecauseOfTooManyInstances(false);
					SortedInstances.Add(Ptr);
				}
			}
		}

		for (int32 i = 0; i < GetNumberOfPriorityUpdateInstances() && i < SortedInstances.Num(); ++i)
		{
			SortedInstances[i]->SetIsPlayerOrNearIt(true);
		}

		if (SortedInstances.Num() > NumGeneratedInstancesLimitLODs)
		{
			SortedInstances.Sort([](UCustomizableObjectInstance& A, UCustomizableObjectInstance& B)
			{
				return A.GetMinSquareDistToPlayer() < B.GetMinSquareDistToPlayer();
			});

			bool bAlreadyReachedFixedLOD = false;
			const float DistanceForFixedLOD = float(CVarDistanceForFixedLOD2.GetValueOnGameThread());
			const float DistanceForFixedLODSquared = FMath::Square(DistanceForFixedLOD);

			for (int32 i = 0; i < NumGeneratedInstancesLimitLODs && i < SortedInstances.Num(); ++i)
			{
				if (!bAlreadyReachedFixedLOD && SortedInstances[i]->GetMinSquareDistToPlayer() < DistanceForFixedLODSquared)
				{
					SortedInstances[i]->SetMinMaxLODToLoad(0, INT32_MAX);
				}
				else
				{
					SortedInstances[i]->SetMinMaxLODToLoad(2, 2);
					bAlreadyReachedFixedLOD = true;
				}
			}

			for (int32 i = NumGeneratedInstancesLimitLODs; i < NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1 && i < SortedInstances.Num(); ++i)
			{
				if (!bAlreadyReachedFixedLOD && SortedInstances[i]->GetMinSquareDistToPlayer() < DistanceForFixedLODSquared)
				{
					SortedInstances[i]->SetMinMaxLODToLoad(1, 2);
				}
				else
				{
					SortedInstances[i]->SetMinMaxLODToLoad(2, 2);
					bAlreadyReachedFixedLOD = true;
				}
			}

			for (int32 i = NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1; i < NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1 + NumGeneratedInstancesLimitLOD2 && i < SortedInstances.Num(); ++i)
			{
				SortedInstances[i]->SetMinMaxLODToLoad(2, 2);
			}

			for (int32 i = NumGeneratedInstancesLimitLODs + NumGeneratedInstancesLimitLOD1 + NumGeneratedInstancesLimitLOD2; i < SortedInstances.Num(); ++i)
			{
				SortedInstances[i]->SetIsDiscardedBecauseOfTooManyInstances(true);
			}
		}
		else
		{
			// No limit surpassed, set all instances to have all LODs, there will be an UpdateSkeletalMesh only if there's a change in LOD state
			for (int32 i = 0; i < SortedInstances.Num(); ++i)
			{
				SortedInstances[i]->SetMinMaxLODToLoad(0, INT32_MAX);
			}
		}
	}
	else if (bOnlyUpdateCloseCustomizableObjects)
	{
		TArray<FVector> Locations;
		TArray<FMatrix> ProjectionMatrices;
		TMap <const AActor*, int32> ViewCentersToIndexMap;

		UCameraComponent* PlayerCamera = nullptr;

		if (ViewCenters.Num() == 0)
		{
			for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
			{
				UWorld* LocalWorld = CustomizableSkeletalComponent->GetWorld();
				APlayerController* Controller = LocalWorld ? LocalWorld->GetFirstPlayerController() : nullptr;

				if (Controller && Controller->GetViewTarget())
				{
					PlayerCamera = Cast<UCameraComponent>(Controller->GetViewTarget()->GetComponentByClass(UCameraComponent::StaticClass()));
					ViewCentersToIndexMap.Add(Controller, 0);
					break;
				}
			}

			if (PlayerCamera && PlayerCamera->IsActive())
			{
				FMinimalViewInfo MinimalViewInfo;
				PlayerCamera->GetCameraView(16.6f, MinimalViewInfo); // The Delta Time parameter is irrelevant

				FMatrix ViewMatrix;
				FMatrix ProjectionMatrix;
				FMatrix ViewProjectionMatrix;
				UGameplayStatics::GetViewProjectionMatrix(MinimalViewInfo, ViewMatrix, ProjectionMatrix, ViewProjectionMatrix);

				Locations.Add(MinimalViewInfo.Location);
				ProjectionMatrices.Add(ProjectionMatrix);
			}
		}
		else
		{
			int32 index = 0;
			for (const TWeakObjectPtr<const AActor> ViewCenter : ViewCenters)
			{
				if (!ViewCenter.IsValid())
				{
					continue;
				}

				UCameraComponent* Camera = nullptr;
				if (const ACameraActor* CameraActor = Cast<ACameraActor>(ViewCenter.Get()))
				{
					Camera = CameraActor->GetCameraComponent();
				}
				else
				{
					Camera = Cast<UCameraComponent>(ViewCenter->GetComponentByClass(UCameraComponent::StaticClass()));
				}
				
				if (Camera && Camera->IsActive())
				{
					FMinimalViewInfo MinimalViewInfo;
					Camera->GetCameraView(16.6f, MinimalViewInfo); // The Delta Time parameter is irrelevant

					FMatrix ViewMatrix;
					FMatrix ProjectionMatrix;
					FMatrix ViewProjectionMatrix;
					UGameplayStatics::GetViewProjectionMatrix(MinimalViewInfo, ViewMatrix, ProjectionMatrix, ViewProjectionMatrix);

					Locations.Add(MinimalViewInfo.Location);
					ProjectionMatrices.Add(ProjectionMatrix);
					ViewCentersToIndexMap.Add(ViewCenter.Get(), index);
					index++;
				}
			}
		}

		if (Locations.Num())
		{
			float SquaredDist = FMath::Square(CloseCustomizableObjectsDist);
			float ScreenSizeLOD1 = 0.04f;
			float ScreenSizeLOD2 = 0.02f;

			for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
			{
				if ( IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate() )
				{
					UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject(); 
					if (!CustomizableObject)
					{
						continue;
					}
														
					if (CustomizableObjectInstance->GetMinSquareDistToPlayer() > SquaredDist || !CustomizableObjectInstance->NearestToActor.IsValid())
					{
						continue;
					}

					USkeletalMesh* RefSkeletalMesh = CustomizableObject->GetRefSkeletalMesh();
					if (!RefSkeletalMesh)
					{
						continue;
					}
					
					if (RefSkeletalMesh->GetLODInfoArray().Num() > 1)
					{
						ScreenSizeLOD1 = FMath::Square(RefSkeletalMesh->GetLODInfo(1)->ScreenSize.Default);
					}
					
					if (RefSkeletalMesh->GetLODInfoArray().Num() > 2)
					{
						ScreenSizeLOD2 = FMath::Square(RefSkeletalMesh->GetLODInfo(2)->ScreenSize.Default);
					}
					
					float BoundSize = RefSkeletalMesh->GetBounds().SphereRadius;
					FVector BoundOrigin;

					AActor* ParentActor = CustomizableObjectInstance->NearestToActor->GetAttachmentRootActor();
					if (ParentActor)
					{
						BoundSize *= ParentActor->GetActorScale().Size() * 1.55f;
						BoundOrigin = ParentActor->GetActorLocation();
					}
					else
					{
						BoundOrigin = CustomizableObjectInstance->NearestToActor->GetComponentLocation();
					}


					int32* ViewIndex = ViewCentersToIndexMap.Find(CustomizableObjectInstance->NearestToViewCenter.Get());
					float InstanceScreenSize = ComputeBoundsScreenRadiusSquared(BoundOrigin, BoundSize, Locations[ViewIndex ? *ViewIndex : 0 ], ProjectionMatrices[ ViewIndex ? *ViewIndex : 0]);

					int32 InstanceMaxLOD = FMath::Min(RefSkeletalMesh->GetLODNum(), CustomizableObjectInstance->GetNumLODsAvailable()) - 1;

					bool bIsPawn = CustomizableObjectInstance->GetMinSquareDistToPlayer() <= 0;

					if (InstanceMaxLOD == 0 || InstanceScreenSize > ScreenSizeLOD1 || bIsPawn)
					{
						CustomizableObjectInstance->SetMinMaxLODToLoad(0, InstanceMaxLOD, false);
					}
					else if (InstanceMaxLOD == 1 || InstanceScreenSize > ScreenSizeLOD2 )
					{
						CustomizableObjectInstance->SetMinMaxLODToLoad(1, InstanceMaxLOD, false);
					}
					else
					{
						CustomizableObjectInstance->SetMinMaxLODToLoad(2, 2);
					}
				}
			}
		}
	}
}


int32 UCustomizableInstanceLODManagement::GetNumGeneratedInstancesLimitFullLODs()
{
	return CVarNumGeneratedInstancesLimit.GetValueOnGameThread();
}


int32 UCustomizableInstanceLODManagement::GetNumGeneratedInstancesLimitLOD1()
{
	return CVarNumGeneratedInstancesLimitLOD1.GetValueOnGameThread();
}


int32 UCustomizableInstanceLODManagement::GetNumGeneratedInstancesLimitLOD2()
{
	return CVarNumGeneratedInstancesLimitLOD2.GetValueOnGameThread();
}


void UCustomizableInstanceLODManagement::SetNumberOfPriorityUpdateInstances(int32 InNumPriorityUpdateInstances)
{
	NumPriorityUpdateInstances = InNumPriorityUpdateInstances;
}


int32 UCustomizableInstanceLODManagement::GetNumberOfPriorityUpdateInstances() const
{
	return NumPriorityUpdateInstances;
}


bool UCustomizableInstanceLODManagement::IsOnlyUpdateCloseCustomizableObjectsEnabled() const
{
	return bOnlyUpdateCloseCustomizableObjects;
}


void UCustomizableInstanceLODManagement::EnableOnlyUpdateCloseCustomizableObjects(float CloseDist)
{
	bOnlyUpdateCloseCustomizableObjects = true;
	CloseCustomizableObjectsDist = CloseDist;
}


void UCustomizableInstanceLODManagement::DisableOnlyUpdateCloseCustomizableObjects()
{
	bOnlyUpdateCloseCustomizableObjects = false;
}


float UCustomizableInstanceLODManagement::GetOnlyUpdateCloseCustomizableObjectsDist() const
{
	return CloseCustomizableObjectsDist;
}
