// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerGeomCacheModelDetails.h"
#include "MLDeformerGeomCacheEditorModel.h"
#include "MLDeformerGeomCacheModel.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Layout/SBox.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "MLDeformerGeomCacheModelDetails"

namespace UE::MLDeformer
{
	bool FMLDeformerGeomCacheModelDetails::UpdateMemberPointers(const TArray<TWeakObjectPtr<UObject>>& Objects)
	{
		if (!FMLDeformerModelDetails::UpdateMemberPointers(Objects))
		{
			return false;
		}

		GeomCacheModel = Cast<UMLDeformerGeomCacheModel>(Model);
		check(GeomCacheModel);
		GeomCacheEditorModel = static_cast<FMLDeformerGeomCacheEditorModel*>(EditorModel);

		return (GeomCacheModel != nullptr && GeomCacheEditorModel != nullptr);
	}

	void FMLDeformerGeomCacheModelDetails::AddAnimSequenceErrors()
	{
		const FText WarningText = GetGeomCacheAnimSequenceErrorText(GeomCacheModel->GetGeometryCache(), Model->GetAnimSequence());
		BaseMeshCategoryBuilder->AddCustomRow(FText::FromString("AnimSeqWarning"))
			.Visibility(!WarningText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(WarningText)
				]
			];
	}

	void FMLDeformerGeomCacheModelDetails::AddTargetMesh()
	{
		TargetMeshCategoryBuilder->AddProperty(UMLDeformerGeomCacheModel::GetGeometryCachePropertyName(), UMLDeformerGeomCacheModel::StaticClass());

		const FText TargetMeshErrorText = GetGeomCacheErrorText(GeomCacheModel->GetSkeletalMesh(), GeomCacheModel->GetGeometryCache());
		TargetMeshCategoryBuilder->AddCustomRow(FText::FromString("TargetMeshError"))
			.Visibility(!TargetMeshErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Error)
					.Message(TargetMeshErrorText)
				]
			];

		const FText ChangedErrorText = EditorModel->GetTargetAssetChangedErrorText();
		TargetMeshCategoryBuilder->AddCustomRow(FText::FromString("TargetMeshChangedError"))
			.Visibility(!ChangedErrorText.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(ChangedErrorText)
				]
			];

		AddGeomCacheMeshMappingWarnings(TargetMeshCategoryBuilder, Model->GetSkeletalMesh(), GeomCacheModel->GetGeometryCache());
	}

	void FMLDeformerGeomCacheModelDetails::AddGeomCacheMeshMappingWarnings(IDetailCategoryBuilder* InTargetMeshCategoryBuilder, USkeletalMesh* SkeletalMesh, UGeometryCache* GeometryCache)
	{
		const FText MeshMappingError = GetGeomCacheMeshMappingErrorText(SkeletalMesh, GeometryCache);
		InTargetMeshCategoryBuilder->AddCustomRow(FText::FromString("MeshMappingWarning"))
			.Visibility(!MeshMappingError.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed)
			.WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0.0f, 4.0f))
				[
					SNew(SWarningOrErrorBox)
					.MessageStyle(EMessageStyle::Warning)
					.Message(MeshMappingError)
				]
			];
	}
}	// namespace UE::MLDeformer

#undef LOCTEXT_NAMESPACE
