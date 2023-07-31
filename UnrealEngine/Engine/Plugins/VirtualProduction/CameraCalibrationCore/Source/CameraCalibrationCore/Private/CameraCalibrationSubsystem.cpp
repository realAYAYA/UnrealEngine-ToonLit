// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraCalibrationSubsystem.h"

#include "CameraCalibrationCoreLog.h"
#include "CameraCalibrationSettings.h"
#include "CineCameraComponent.h"
#include "Engine/TimecodeProvider.h"
#include "LensComponent.h"
#include "Misc/CoreDelegates.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Editor.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "CameraCalibrationSubsystem"



ULensFile* UCameraCalibrationSubsystem::GetDefaultLensFile() const
{
	return DefaultLensFile;
}

void UCameraCalibrationSubsystem::SetDefaultLensFile(ULensFile* NewDefaultLensFile)
{
	//Todo : Add callbacks when default lens file changes
	DefaultLensFile = NewDefaultLensFile;
}

ULensFile* UCameraCalibrationSubsystem::GetLensFile(const FLensFilePicker& Picker) const
{
	if (Picker.bUseDefaultLensFile)
	{
		return GetDefaultLensFile();
	}
	else
	{
		return Picker.LensFile;
	}
}

TArray<ULensDistortionModelHandlerBase*> UCameraCalibrationSubsystem::GetDistortionModelHandlers(UCineCameraComponent* Component)
{
	// This function has been deprecated. The implementation has been changed to provide some backwards compatibility, but code should be updated to not call this function.
	TArray<ULensDistortionModelHandlerBase*> Handlers;

	if (Component)
	{
		AActor* OwningActor = Component->GetOwner();

		TInlineComponentArray<ULensComponent*> LensComponents;
		OwningActor->GetComponents(LensComponents);

		for (ULensComponent* LensComponent : LensComponents)
		{
			Handlers.Add(LensComponent->GetLensDistortionHandler());
		}
	}

	return Handlers;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
ULensDistortionModelHandlerBase* UCameraCalibrationSubsystem::FindDistortionModelHandler(FDistortionHandlerPicker& DistortionHandlerPicker, bool bUpdatePicker) const
{
	// This function has been deprecated. The implementation has been changed to provide some backwards compatibility, but code should be updated to not call this function.
	TArray<ULensDistortionModelHandlerBase*> Handlers;

	if (!DistortionHandlerPicker.TargetCameraComponent)
	{
		return nullptr;
	}

	AActor* OwningActor = DistortionHandlerPicker.TargetCameraComponent->GetOwner();

	TInlineComponentArray<ULensComponent*> LensComponents;
	OwningActor->GetComponents(LensComponents);

	if (LensComponents.Num() > 0)
	{
		return LensComponents[0]->GetLensDistortionHandler();
	}

	return nullptr;
}

ULensDistortionModelHandlerBase* UCameraCalibrationSubsystem::FindOrCreateDistortionModelHandler(FDistortionHandlerPicker& DistortionHandlerPicker, const TSubclassOf<ULensModel> LensModelClass)
{
	// This function has been deprecated. The implementation has been changed to provide some backwards compatibility, but code should be updated to not call this function.
	TArray<ULensDistortionModelHandlerBase*> Handlers = GetDistortionModelHandlers(DistortionHandlerPicker.TargetCameraComponent);

	for (ULensDistortionModelHandlerBase* Handler : Handlers)
	{
		if (Handler->GetLensModelClass() == LensModelClass)
		{
			return Handler;
		}
	}

	return nullptr;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UCameraCalibrationSubsystem::UnregisterDistortionModelHandler(UCineCameraComponent* Component, ULensDistortionModelHandlerBase* Handler)
{
	// This function has been deprecated.
}

void UCameraCalibrationSubsystem::RegisterDistortionModel(TSubclassOf<ULensModel> LensModel)
{
	LensModelMap.Add(LensModel->GetDefaultObject<ULensModel>()->GetModelName(), LensModel);
}

void UCameraCalibrationSubsystem::UnregisterDistortionModel(TSubclassOf<ULensModel> LensModel)
{
	LensModelMap.Remove(LensModel->GetDefaultObject<ULensModel>()->GetModelName());
}

void UCameraCalibrationSubsystem::RegisterOverlayMaterial(const FName& MaterialName, const FSoftObjectPath& MaterialPath)
{
	RegisteredOverlayMaterials.Add(MaterialName, TSoftObjectPtr<UMaterialInterface>(MaterialPath));
}

void UCameraCalibrationSubsystem::UnregisterOverlayMaterial(const FName& MaterialName)
{
	RegisteredOverlayMaterials.Remove(MaterialName);
}

void UCameraCalibrationSubsystem::UpdateOriginalFocalLength(UCineCameraComponent* Component, float InFocalLength)
{
	// This function has been deprecated.
}

void UCameraCalibrationSubsystem::UpdateOverscanFocalLength(UCineCameraComponent* Component, float InFocalLength)
{
	// This function has been deprecated.
}

bool UCameraCalibrationSubsystem::GetOriginalFocalLength(UCineCameraComponent* Component, float& OutFocalLength)
{
	// This function has been deprecated.
	return false;
}

TSubclassOf<ULensModel> UCameraCalibrationSubsystem::GetRegisteredLensModel(FName ModelName) const
{
	if (LensModelMap.Contains(ModelName))
	{
		return LensModelMap[ModelName];
	}
	return nullptr;
}

TSubclassOf<UCameraNodalOffsetAlgo> UCameraCalibrationSubsystem::GetCameraNodalOffsetAlgo(FName Name) const
{
	if (CameraNodalOffsetAlgosMap.Contains(Name))
	{
		return CameraNodalOffsetAlgosMap[Name];
	}
	return nullptr;
}

TArray<FName> UCameraCalibrationSubsystem::GetCameraNodalOffsetAlgos() const
{
	TArray<FName> OutKeys;
	CameraNodalOffsetAlgosMap.GetKeys(OutKeys);
	return OutKeys;
}

TSubclassOf<UCameraImageCenterAlgo> UCameraCalibrationSubsystem::GetCameraImageCenterAlgo(FName Name) const
{
	if (CameraImageCenterAlgosMap.Contains(Name))
	{
		return CameraImageCenterAlgosMap[Name];
	}
	return nullptr;
}

TArray<FName> UCameraCalibrationSubsystem::GetCameraImageCenterAlgos() const
{
	TArray<FName> OutKeys;
	CameraImageCenterAlgosMap.GetKeys(OutKeys);
	return OutKeys;
}

UMaterialInterface* UCameraCalibrationSubsystem::GetOverlayMaterial(const FName& OverlayName) const
{
#if WITH_EDITOR
	if (UMaterialInterface* DefaultMaterial = GetDefault<UCameraCalibrationSettings>()->GetCalibrationOverlayMaterialOverride(OverlayName))
	{
		return DefaultMaterial;
	}
#endif

	const TSoftObjectPtr<UMaterialInterface> OverlayMaterial = RegisteredOverlayMaterials.FindRef(OverlayName);

	return OverlayMaterial.LoadSynchronous();
}

TArray<FName> UCameraCalibrationSubsystem::GetOverlayMaterialNames() const
{
	// Use a set to avoid duplicates when combining the registered overlays with the set of overrides
	TSet<FName> OverlayNames;

	TArray<FName> OutKeys;
	RegisteredOverlayMaterials.GetKeys(OutKeys);

	OverlayNames.Append(OutKeys);

#if WITH_EDITOR
	OverlayNames.Append(GetDefault<UCameraCalibrationSettings>()->GetCalibrationOverlayMaterialOverrideNames());
#endif

	return OverlayNames.Array();
}

TSubclassOf<UCameraCalibrationStep> UCameraCalibrationSubsystem::GetCameraCalibrationStep(FName Name) const
{
	if (CameraCalibrationStepsMap.Contains(Name))
	{
		return CameraCalibrationStepsMap[Name];
	}
	return nullptr;
}

TArray<FName> UCameraCalibrationSubsystem::GetCameraCalibrationSteps() const
{
	TArray<FName> OutKeys;
	CameraCalibrationStepsMap.GetKeys(OutKeys);
	return OutKeys;
}

void UCameraCalibrationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddLambda([&]()
	{
		// Find Image Center Algos
		{
			TArray<TSubclassOf<UCameraImageCenterAlgo>> Algos;

			for (TObjectIterator<UClass> AlgoIt; AlgoIt; ++AlgoIt)
			{
				if (AlgoIt->IsChildOf(UCameraImageCenterAlgo::StaticClass()) && !AlgoIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
				{
					const UCameraImageCenterAlgo* Algo = CastChecked<UCameraImageCenterAlgo>(AlgoIt->GetDefaultObject());
					CameraImageCenterAlgosMap.Add(Algo->FriendlyName(), TSubclassOf<UCameraImageCenterAlgo>(*AlgoIt));
				}
			}
		}

		// Find Nodal Offset Algos
		{
			TArray<TSubclassOf<UCameraNodalOffsetAlgo>> Algos;

			for (TObjectIterator<UClass> AlgoIt; AlgoIt; ++AlgoIt)
			{
				if (AlgoIt->IsChildOf(UCameraNodalOffsetAlgo::StaticClass()) && !AlgoIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
				{
					const UCameraNodalOffsetAlgo* Algo = CastChecked<UCameraNodalOffsetAlgo>(AlgoIt->GetDefaultObject());
					CameraNodalOffsetAlgosMap.Add(Algo->FriendlyName(), TSubclassOf<UCameraNodalOffsetAlgo>(*AlgoIt));
				}
			}
		}

		// Find Calibration Steps
		{
			TArray<TSubclassOf<UCameraCalibrationStep>> Steps;

			for (TObjectIterator<UClass> StepIt; StepIt; ++StepIt)
			{
				if (StepIt->IsChildOf(UCameraCalibrationStep::StaticClass()) && !StepIt->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
				{
					CameraCalibrationStepsMap.Add(StepIt->GetFName(), TSubclassOf<UCameraCalibrationStep>(*StepIt));
				}
			}
		}
	});
}

void UCameraCalibrationSubsystem::Deinitialize()
{
	LensModelMap.Empty(0);
	CameraImageCenterAlgosMap.Empty(0);
	CameraNodalOffsetAlgosMap.Empty(0);
	CameraCalibrationStepsMap.Empty(0);

	if (PostEngineInitHandle.IsValid())
	{
		FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
	}

	Super::Deinitialize();
}

#undef LOCTEXT_NAMESPACE


