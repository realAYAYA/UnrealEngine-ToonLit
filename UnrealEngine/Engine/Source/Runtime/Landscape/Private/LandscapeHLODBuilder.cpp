// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeHLODBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LandscapeHLODBuilder)

#if WITH_EDITOR

#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeMeshProxyComponent.h"

#include "MeshDescription.h"
#include "TriangleTypes.h"
#include "StaticMeshAttributes.h"

#include "Materials/MaterialInstanceConstant.h"
#include "MaterialUtilities.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"

#include "Algo/ForEach.h"

#include "Serialization/ArchiveCrc32.h"
#include "Engine/HLODProxy.h"

#endif

ULandscapeHLODBuilder::ULandscapeHLODBuilder(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

#if WITH_EDITOR

uint32 ULandscapeHLODBuilder::ComputeHLODHash(const UActorComponent* InSourceComponent) const
{
	FArchiveCrc32 Ar;

	if (const ULandscapeComponent* LSComponent = Cast<ULandscapeComponent>(InSourceComponent))
	{
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("ULandscapeHLODBuilder::ComputeHLODHash %s"), *InSourceComponent->GetName());

		ALandscapeProxy* LSProxy = LSComponent->GetLandscapeProxy();

		// LS LOD setup
		TArray<float> LODScreenSize = LSProxy->GetLODScreenSizeArray();
		Ar << LODScreenSize;
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - LODScreenSize = %x"), Ar.GetCrc());
		
		// LS Transform
		uint32 TransformHash = UHLODProxy::GetCRC(LSComponent->GetComponentTransform());
		Ar << TransformHash;
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - TransformHash = %x"), Ar.GetCrc());

		// LS Content - Heightmap & Weightmaps
		uint32 LSContentHash = LSComponent->ComputeLayerHash(/*InReturnEditingHash=*/ false);
		Ar << LSContentHash;
		UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - LSContentHash = %x"), Ar.GetCrc());

		TSet<UTexture*> UsedTextures;

		// LS Materials
		{
			TArray<UMaterialInterface*> UsedMaterials;
			if (LSProxy->bUseDynamicMaterialInstance)
			{
				UsedMaterials.Append(LSComponent->MaterialInstancesDynamic);
			}
			else
			{
				UsedMaterials.Append(LSComponent->MaterialInstances);
			}

			UsedMaterials.Add(LSComponent->OverrideMaterial);
			UsedMaterials.Add(LSComponent->OverrideHoleMaterial);
			TArray<uint32> UsedMaterialsCRC;
			for (UMaterialInterface* MaterialInterface : UsedMaterials)
			{
				if (MaterialInterface)
				{
					uint32 MaterialCRC = UHLODProxy::GetCRC(MaterialInterface);
					UsedMaterialsCRC.Add(MaterialCRC);

					TArray<UTexture*> Textures;
					MaterialInterface->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, ERHIFeatureLevel::SM5, true);
					UsedTextures.Append(Textures);
				}
			}
			UsedMaterialsCRC.Sort();
			Ar << UsedMaterialsCRC;
			UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - UsedMaterialsCRC = %x"), Ar.GetCrc());
		}

		// LS Textures
		{
			TArray<uint32> UsedTexturesCRC;
			for (UTexture* Texture : UsedTextures)
			{
				uint32 TextureCRC = UHLODProxy::GetCRC(Texture);
				UsedTexturesCRC.Add(TextureCRC);
			}
			UsedTexturesCRC.Sort();
			Ar << UsedTexturesCRC;
			UE_LOG(LogHLODBuilder, VeryVerbose, TEXT("     - UsedTexturesCRC = %x"), Ar.GetCrc());
		}

		// Nanite enabled?
		bool bNaniteEnabled = LSProxy->IsNaniteEnabled();
		Ar << bNaniteEnabled;
	}

	return Ar.GetCrc();
}

static int32 GetMeshTextureSizeFromTargetTexelDensity(const FMeshDescription& InMesh, float InTargetTexelDensity)
{
	FStaticMeshConstAttributes Attributes(InMesh);

	TVertexAttributesConstRef<FVector3f> VertexPositions = Attributes.GetVertexPositions();
	
	double Mesh3DArea = 0;
	for (const FTriangleID TriangleID : InMesh.Triangles().GetElementIDs())
	{
		TArrayView<const FVertexID> TriVertices = InMesh.GetTriangleVertices(TriangleID);

		// World space area
		Mesh3DArea += UE::Geometry::VectorUtil::Area(VertexPositions[TriVertices[0]],
													 VertexPositions[TriVertices[1]],
													 VertexPositions[TriVertices[2]]);
	}
	double TexelRatio = FMath::Sqrt(1.0 / Mesh3DArea) * 100;

	// Compute the perfect texture size that would get us to our texture density
	// Also compute the nearest power of two sizes (below and above our target)
	const int32 SizePerfect = FMath::CeilToInt(InTargetTexelDensity / TexelRatio);
	const int32 SizeHi = FMath::RoundUpToPowerOfTwo(SizePerfect);
	const int32 SizeLo = SizeHi >> 1;

	// Compute the texel density we achieve with these two texture sizes
	const double TexelDensityLo = SizeLo * TexelRatio;
	const double TexelDensityHi = SizeHi * TexelRatio;

	// Select best match between low & high res textures.
	const double TexelDensityLoDiff = InTargetTexelDensity - TexelDensityLo;
	const double TexelDensityHiDiff = TexelDensityHi - InTargetTexelDensity;
	const int32 BestTextureSize = TexelDensityLoDiff < TexelDensityHiDiff ? SizeLo : SizeHi;

	return BestTextureSize;
}

static int32 ComputeRequiredLandscapeLOD(const ALandscapeProxy* InLandscapeProxy, const float InViewDistance)
{
	check(InLandscapeProxy && !InLandscapeProxy->LandscapeComponents.IsEmpty());
	
	// These constants are showing up a lot in the screen size computation for Level HLODs. This should be configurable per project.
	const float HalfFOV = PI * 0.25f;
	const float ScreenWidth = 1920.0f;
	const float ScreenHeight = 1080.0f;
	const FPerspectiveMatrix ProjMatrix(HalfFOV, ScreenWidth, ScreenHeight, 1.0f);

	TArray<float> LODScreenSizes = InLandscapeProxy->GetLODScreenSizeArray();

	ULandscapeComponent* LSComponent = InLandscapeProxy->LandscapeComponents[0];
	float ComponentRadiusScaled = LSComponent->GetLocalBounds().SphereRadius * LSComponent->GetComponentTransform().GetScale3D().GetAbsMax();
	float ExpectedScreenSize = ComputeBoundsScreenSize(FVector::ZeroVector, ComponentRadiusScaled, FVector(0.0f, 0.0f, InViewDistance), ProjMatrix);

	int32 RequiredLOD;
	for (RequiredLOD = 0; RequiredLOD < LODScreenSizes.Num(); ++RequiredLOD)
	{
		if (ExpectedScreenSize > LODScreenSizes[RequiredLOD])
		{
			break;
		}
	}

	return RequiredLOD;
}

static UMaterialInterface* BakeLandscapeMaterial(const FHLODBuildContext& InHLODBuildContext, const FMeshDescription& InMeshDescription, const ALandscapeProxy* InLandscapeProxy, const int32 InLandscapeLOD, int32 InTextureSize)
{
	// Build landscape material
	FFlattenMaterial LandscapeFlattenMaterial;
	LandscapeFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Diffuse, InTextureSize);
	LandscapeFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Normal, InTextureSize);
	LandscapeFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Metallic, InTextureSize);
	LandscapeFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Roughness, InTextureSize);
	LandscapeFlattenMaterial.SetPropertySize(EFlattenMaterialProperties::Specular, InTextureSize);
	FMaterialUtilities::ExportLandscapeMaterial(InLandscapeProxy, LandscapeFlattenMaterial);

	// Optimize flattened material
	FMaterialUtilities::OptimizeFlattenMaterial(LandscapeFlattenMaterial);

	UMaterial* LandscapeMaterial = GEngine->DefaultLandscapeFlattenMaterial;

	// Validate that the flatten material expects world space normals
	if (LandscapeMaterial->bTangentSpaceNormal)
	{
		UE_LOG(LogHLODBuilder, Error, TEXT("Landscape flatten material %s should use world space normals rather than tangent space normals."), *LandscapeMaterial->GetName());
	}

	FMaterialProxySettings MaterialProxySettings;
	MaterialProxySettings.TextureSizingType = ETextureSizingType::TextureSizingType_UseSingleTextureSize;
	MaterialProxySettings.TextureSize = InTextureSize;
	MaterialProxySettings.bNormalMap = true;
	MaterialProxySettings.bMetallicMap = true;
	MaterialProxySettings.bRoughnessMap = true;
	MaterialProxySettings.bSpecularMap = true;

	// Create a new proxy material instance
	TArray<UObject*> GeneratedAssets;
	UMaterialInstanceConstant* LandscapeMaterialInstance = FMaterialUtilities::CreateFlattenMaterialInstance(InHLODBuildContext.AssetsOuter->GetPackage(), MaterialProxySettings, LandscapeMaterial, LandscapeFlattenMaterial, InHLODBuildContext.AssetsBaseName, InLandscapeProxy->GetName(), GeneratedAssets);

	Algo::ForEach(GeneratedAssets, [](UObject* Asset) 
	{
		// We don't want any of the generate HLOD assets to be public
		Asset->ClearFlags(RF_Public | RF_Standalone);

		// Use clamp texture addressing to avoid artifacts between tiles
		if (UTexture2D* Texture = Cast<UTexture2D>(Asset))
		{
			Texture->AddressX = TA_Clamp;
			Texture->AddressY = TA_Clamp;
		}
	});

	return LandscapeMaterialInstance;
}

// This is an initial implementation for UE 5.0
// Multiple improvements could be done
// * Currently, for each referenced landscape proxy, we generate individual HLOD meshes & textures. This should output a single mesh for all proxies
// * Generated mesh could be Nanite-enabled
TArray<UActorComponent*> ULandscapeHLODBuilder::Build(const FHLODBuildContext& InHLODBuildContext, const TArray<UActorComponent*>& InSourceComponents) const
{
	TArray<ULandscapeComponent*> SourceLandscapeComponents = FilterComponents<ULandscapeComponent>(InSourceComponents);
	
	TSet<ALandscapeProxy*> LandscapeProxies;
	Algo::Transform(SourceLandscapeComponents, LandscapeProxies, [](ULandscapeComponent* SourceComponent) { return SourceComponent->GetLandscapeProxy(); });

	// This code assume all components of a proxy are included in the same build... validate this
	checkCode
	(
		for (ALandscapeProxy* LandscapeProxy : LandscapeProxies)
		{
			for (ULandscapeComponent* LandscapeComponent : LandscapeProxy->LandscapeComponents)
			{
				check(SourceLandscapeComponents.Contains(LandscapeComponent));
			}
		}
	);

	TArray<UActorComponent*> HLODComponents;
	TArray<UStaticMesh*> StaticMeshes;

	for (ALandscapeProxy* LandscapeProxy : LandscapeProxies)
	{
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(InHLODBuildContext.AssetsOuter);
		FMeshDescription* MeshDescription = nullptr;

		// Compute source landscape LOD
		int32 LandscapeLOD = ComputeRequiredLandscapeLOD(LandscapeProxy, InHLODBuildContext.MinVisibleDistance);

		// Mesh
		{
			FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
			// Don't allow the engine to recalculate normals
			SrcModel.BuildSettings.bRecomputeNormals = false;
			SrcModel.BuildSettings.bRecomputeTangents = false;
			SrcModel.BuildSettings.bRemoveDegenerates = false;
			SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
			SrcModel.BuildSettings.bUseFullPrecisionUVs = false;

			MeshDescription = StaticMesh->CreateMeshDescription(0);
	
			LandscapeProxy->ExportToRawMesh(LandscapeLOD, *MeshDescription);

			StaticMesh->CommitMeshDescription(0);

			// Nanite settings
			StaticMesh->NaniteSettings.bEnabled = LandscapeProxy->IsNaniteEnabled();

			StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
		}

		// Material
		{
			float TargetTexelDensityPerMeter = FMaterialUtilities::ComputeRequiredTexelDensityFromDrawDistance(InHLODBuildContext.MinVisibleDistance, MeshDescription->GetBounds().SphereRadius);
			int32 RequiredTextureSize = GetMeshTextureSizeFromTargetTexelDensity(*MeshDescription, TargetTexelDensityPerMeter);

			UMaterialInterface* LandscapeMaterial = BakeLandscapeMaterial(InHLODBuildContext, *MeshDescription, LandscapeProxy, LandscapeLOD, RequiredTextureSize);

			//Assign the proxy material to the static mesh
			StaticMesh->GetStaticMaterials().Add(FStaticMaterial(LandscapeMaterial));
		}

		StaticMeshes.Add(StaticMesh);

		ULandscapeMeshProxyComponent* LandcapeMeshProxyComponent = NewObject<ULandscapeMeshProxyComponent>();
		LandcapeMeshProxyComponent->InitializeForLandscape(LandscapeProxy, LandscapeLOD);
		LandcapeMeshProxyComponent->SetStaticMesh(StaticMesh);
		HLODComponents.Add(LandcapeMeshProxyComponent);
	}

	UStaticMesh::BatchBuild(StaticMeshes);

	return HLODComponents;
}

#endif

