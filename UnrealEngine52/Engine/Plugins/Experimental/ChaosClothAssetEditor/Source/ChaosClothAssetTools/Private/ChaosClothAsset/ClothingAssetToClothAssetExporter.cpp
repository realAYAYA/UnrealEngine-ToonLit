// Copyright Epic Games, Inc. All Rights Reserved.

#include "ClothingAssetToClothAssetExporter.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAdapter.h"
#include "Animation/Skeleton.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Engine/SkeletalMesh.h"
#include "Materials/Material.h"
#include "Misc/MessageDialog.h"
#include "ClothingAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothingAssetToClothAssetExporter)

#define LOCTEXT_NAMESPACE "ClothingAssetToClothAssetExporter"

UClass* UClothingAssetToChaosClothAssetExporter::GetExportedType() const
{
	return UChaosClothAsset::StaticClass();
}

void UClothingAssetToChaosClothAssetExporter::Export(const UClothingAssetBase* ClothingAsset, UObject* ExportedAsset)
{
	using namespace UE::Chaos::ClothAsset;

	const UClothingAssetCommon* const ClothingAssetCommon = ExactCast<UClothingAssetCommon>(ClothingAsset);
	if (!ClothingAssetCommon)
	{
		const FText TitleMessage = LOCTEXT("ClothingAssetExporterTitle", "Error Exporting Clothing Asset");
		const FText ErrorMessage = LOCTEXT("ClothingAssetExporterError", "Can only export from known ClothingAssetCommon types.");
		FMessageDialog::Open(EAppMsgType::Ok, EAppReturnType::Ok, ErrorMessage, &TitleMessage);
		return;
	}

	UChaosClothAsset* const ClothAsset = CastChecked<UChaosClothAsset>(ExportedAsset);
	check(ClothAsset);

	for (const FClothLODDataCommon& ClothLODData : ClothingAssetCommon->LodData)
	{
		const FClothPhysicalMeshData& PhysicalMeshData = ClothLODData.PhysicalMeshData;

		// Unwrap the physical mesh data into the pattern and rest meshes
		FClothAdapter Cloth(ClothAsset->GetClothCollection());
		FClothLodAdapter ClothLod = Cloth.AddGetLod();
		ClothLod.Initialize(PhysicalMeshData.Vertices, PhysicalMeshData.Indices);
	}

	// Add a default material
	ClothAsset->GetMaterials().Reset(1);
	UMaterialInterface* const DefaultMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EditorMaterials/Cloth/CameraLitDoubleSided.CameraLitDoubleSided"), nullptr, LOAD_None, nullptr);
	const int32 MaterialId = ClothAsset->GetMaterials().Emplace(DefaultMaterial, true, false, DefaultMaterial->GetFName());

	// Assign the physics asset if any
	ClothAsset->SetPhysicsAsset(ClothingAssetCommon->PhysicsAsset);

	// Set the skeleton from the skeletal mesh, or create a default skeleton if it can't find one
	USkeletalMesh* const SkeletalMesh = Cast<USkeletalMesh>(ClothingAssetCommon->GetOuter());

	ClothAsset->SetReferenceSkeleton(SkeletalMesh ?
		SkeletalMesh->GetRefSkeleton() :
		LoadObject<USkeleton>(nullptr, TEXT("/Engine/EditorMeshes/SkeletalMesh/DefaultSkeletalMesh_Skeleton.DefaultSkeletalMesh_Skeleton"), nullptr, LOAD_None, nullptr)->GetReferenceSkeleton());

	// Set the render mesh to duplicate the sim mesh
	ClothAsset->CopySimMeshToRenderMesh(MaterialId);

	// Build the asset, since it is already loaded, it won't rebuild on load
	ClothAsset->Build();
}

#undef LOCTEXT_NAMESPACE

