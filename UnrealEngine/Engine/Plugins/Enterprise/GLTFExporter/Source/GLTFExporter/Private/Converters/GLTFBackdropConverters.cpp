// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFBackdropConverters.h"
#include "Utilities/GLTFCoreUtilities.h"
#include "Converters/GLTFMaterialUtility.h"
#include "Converters/GLTFBlueprintUtility.h"
#include "Builders/GLTFContainerBuilder.h"

FGLTFJsonBackdrop* FGLTFBackdropConverter::Convert(const AActor* BackdropActor)
{
	if (!FGLTFBlueprintUtility::IsHDRIBackdrop(FGLTFBlueprintUtility::GetClassPath(BackdropActor)))
	{
		return nullptr;
	}

	FGLTFJsonBackdrop* JsonBackdrop = Builder.AddBackdrop();
	BackdropActor->GetName(JsonBackdrop->Name);

	const USceneComponent* SceneComponent = BackdropActor->GetRootComponent();
	const FRotator Rotation = SceneComponent->GetComponentRotation();

	// NOTE: when calculating map rotation in the HDRI_Attributes material function using RotateAboutAxis,
	// an angle in radians is used as input. But RotateAboutAxis expects a value of 1 per full revolution, not 2*PI.
	// The end result is that the map rotation is always 2*PI more than the actual yaw of the scene component.

	// TODO: remove the 2*PI multiplier if HDRI_Attributes is updated to match map rotation with yaw
	JsonBackdrop->Angle = FRotator::ClampAxis(Rotation.Yaw * 2.0f * PI);

	const UStaticMesh* Mesh;
	if (FGLTFBlueprintUtility::TryGetPropertyValue(BackdropActor, TEXT("Mesh"), Mesh))
	{
		JsonBackdrop->Mesh = Builder.AddUniqueMesh(Mesh, { FGLTFMaterialUtility::GetDefaultMaterial() });
	}
	else
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export Mesh for HDRIBackdrop %s"), *BackdropActor->GetName()));
	}

	const UTextureCube* Cubemap;
	if (FGLTFBlueprintUtility::TryGetPropertyValue(BackdropActor, TEXT("Cubemap"), Cubemap))
	{
		// TODO: add a proper custom gltf extension with its own converters for cubemaps

		for (int32 CubeFaceIndex = 0; CubeFaceIndex < CubeFace_MAX; ++CubeFaceIndex)
		{
			const ECubeFace CubeFace = static_cast<ECubeFace>(CubeFaceIndex);
			const EGLTFJsonCubeFace JsonCubeFace = FGLTFCoreUtilities::ConvertCubeFace(CubeFace);
			JsonBackdrop->Cubemap[static_cast<int32>(JsonCubeFace)] = Builder.AddUniqueTexture(Cubemap, CubeFace);
		}
	}
	else
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export Cubemap for HDRIBackdrop %s"), *BackdropActor->GetName()));
	}

	float Intensity;
	if (FGLTFBlueprintUtility::TryGetPropertyValue(BackdropActor, TEXT("Intensity"), Intensity))
	{
		JsonBackdrop->Intensity = Intensity;
	}
	else
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export Intensity for HDRIBackdrop %s"), *BackdropActor->GetName()));
	}

	float Size;
	if (FGLTFBlueprintUtility::TryGetPropertyValue(BackdropActor, TEXT("Size"), Size))
	{
		JsonBackdrop->Size = Size;
	}
	else
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export Size for HDRIBackdrop %s"), *BackdropActor->GetName()));
	}

	FVector ProjectionCenter;
	if (FGLTFBlueprintUtility::TryGetPropertyValue(BackdropActor, TEXT("ProjectionCenter"), ProjectionCenter))
	{
		JsonBackdrop->ProjectionCenter = FGLTFCoreUtilities::ConvertPosition(FVector3f(ProjectionCenter), Builder.ExportOptions->ExportUniformScale);
	}
	else
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export ProjectionCenter for HDRIBackdrop %s"), *BackdropActor->GetName()));
	}

	float LightingDistanceFactor;
	if (FGLTFBlueprintUtility::TryGetPropertyValue(BackdropActor, TEXT("LightingDistanceFactor"), LightingDistanceFactor))
	{
		JsonBackdrop->LightingDistanceFactor = LightingDistanceFactor;
	}
	else
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export LightingDistanceFactor for HDRIBackdrop %s"), *BackdropActor->GetName()));
	}

	bool UseCameraProjection;
	if (FGLTFBlueprintUtility::TryGetPropertyValue(BackdropActor, TEXT("UseCameraProjection"), UseCameraProjection))
	{
		JsonBackdrop->UseCameraProjection = UseCameraProjection;
	}
	else
	{
		Builder.LogWarning(FString::Printf(TEXT("Failed to export UseCameraProjection for HDRIBackdrop %s"), *BackdropActor->GetName()));
	}

	return JsonBackdrop;
}
