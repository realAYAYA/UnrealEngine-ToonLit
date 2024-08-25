// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMeshesDetailCustomization.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "DetailView/Widgets/SAvaDynamicMaterialWidget.h"
#include "Components/DynamicMeshComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "DynamicMeshToMeshDescription.h"
#include "Engine/StaticMesh.h"
#include "IAssetTools.h"
#include "IDetailGroup.h"
#include "Material/DynamicMaterialInstance.h"
#include "StaticMeshAttributes.h"
#include "AvaShapeActor.h"
#include "DynamicMeshes/AvaShapeDynMeshBase.h"
#include "UObject/Object.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "MeshesDetailsCustomization"

void FAvaMeshesDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedPtr<IPropertyHandle> MeshDatasHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UAvaShapeDynamicMeshBase, MeshDatas),
		UAvaShapeDynamicMeshBase::StaticClass()
	);

	if (!MeshDatasHandle.IsValid())
	{
		return;
	}

	SelectedDynamicMeshes.Empty();
	TArray<FName> MeshNames;
	UAvaShapeDynamicMeshBase* DynMesh = nullptr;

	for (const TWeakObjectPtr<UObject>& Object : DetailBuilder.GetSelectedObjects())
	{
		if (!Object.IsValid())
		{
			continue;
		}

		DynMesh = Cast<UAvaShapeDynamicMeshBase>(Object.Get());

		if (!DynMesh)
		{
			AAvaShapeActor* ShapeActor = Cast<AAvaShapeActor>(Object.Get());

			if (ShapeActor)
			{
				if (ShapeActor->GetDynamicMesh())
				{
					DynMesh = Cast<UAvaShapeDynamicMeshBase>(ShapeActor->GetDynamicMesh());
				}
			}
		}

		if (DynMesh)
		{
			MeshNames.Append(DynMesh->GetMeshDataNames());
			SelectedDynamicMeshes.Add(DynMesh);
		}
	}

	IDetailPropertyRow* MeshDatasRow = DetailBuilder.EditDefaultProperty(MeshDatasHandle);
	DetailBuilder.HideProperty(MeshDatasHandle);

	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(FName("Material"));

	// use primary material everywhere
	TSharedPtr<IPropertyHandle> UsePrimaryMaterialEverywhereHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UAvaShapeDynamicMeshBase, bUsePrimaryMaterialEverywhere),
		UAvaShapeDynamicMeshBase::StaticClass()
	);

	// fix bug where toggle would appear in other group with same name
	if (UsePrimaryMaterialEverywhereHandle.IsValid())
	{
		DetailBuilder.HideProperty(UsePrimaryMaterialEverywhereHandle);
	}

	if (MeshDatasRow)
	{
		MeshDatasRow->Visibility(EVisibility::Hidden);

		TSharedPtr<IPropertyHandleMap> MapHandle = MeshDatasHandle->AsMap();
		uint32 Count = 0;
		MapHandle->GetNumElements(Count);

		uint32 CountNames = MeshNames.Num();

		if (Count < CountNames)
		{
			return;
		}

		for (uint32 Index = 0; Index < Count; ++Index)
		{
			FString MeshName = TEXT("");

			if (Index > 0 || !DynMesh->GetUsePrimaryMaterialEverywhere())
			{
				MeshName = MeshNames[Index].ToString() + TEXT(" ");
			}

			TSharedPtr<IPropertyHandle> MeshPropertyHandle = MeshDatasHandle->GetChildHandle(Index);
			check(MeshPropertyHandle.IsValid());

			// Material Type
			TSharedPtr<IPropertyHandle> MaterialTypeHandle = MeshPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaShapeMeshData, MaterialType));
			check(MaterialTypeHandle.IsValid());

			FString MaterialTypeName = MeshName + TEXT("Material Type");
			FDetailWidgetRow& MaterialTypeRow = CategoryBuilder.AddCustomRow(FText::FromString(MaterialTypeName));

			MaterialTypeRow.NameContent()[MaterialTypeHandle->CreatePropertyNameWidget(FText::FromString(MaterialTypeName))];
			MaterialTypeRow.ValueContent()[MaterialTypeHandle->CreatePropertyValueWidget()];
			MaterialTypeRow.Visibility(MakeAttributeLambda([=]() {
				return MaterialTypeHandle->IsEditable() ? EVisibility::Visible : EVisibility::Hidden;
			}));

			// Material Asset
			TSharedPtr<IPropertyHandle> MaterialHandle = MeshPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaShapeMeshData, Material));
			check(MaterialHandle.IsValid());

			FString MaterialName = MeshName + TEXT("Material Asset");
			FDetailWidgetRow& MaterialRow = CategoryBuilder.AddCustomRow(FText::FromString(MaterialName));

			MaterialRow.NameContent()
			[
				MaterialHandle->CreatePropertyNameWidget(FText::FromString(MaterialName))
			];

			MaterialRow.ValueContent()
			[
				SNew(SAvaDynamicMaterialWidget, MaterialHandle.ToSharedRef())
			];

			MaterialRow.Visibility(MakeAttributeLambda([MaterialHandle]()
			{
				return MaterialHandle->IsEditable() ? EVisibility::Visible : EVisibility::Hidden;
			}));

			// Parametric Material Color
			TSharedPtr<IPropertyHandle> ParametricMaterialHandle = MeshPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaShapeMeshData, ParametricMaterial));
			check(ParametricMaterialHandle.IsValid());

			uint32 NumChildren = 0;
			ParametricMaterialHandle->GetNumChildren(NumChildren);

			for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ChildIdx++)
			{
				TSharedPtr<IPropertyHandle> ParametricChildHandle = ParametricMaterialHandle->GetChildHandle(ChildIdx);
				check(ParametricChildHandle.IsValid());

				IDetailPropertyRow& NewParametricRow = CategoryBuilder.AddProperty(ParametricChildHandle.ToSharedRef());
				FString ParametricRowName = MeshName + ParametricChildHandle->GetPropertyDisplayName().ToString();
				NewParametricRow.DisplayName(FText::FromString(ParametricRowName));
				NewParametricRow.Visibility(MakeAttributeLambda([=]() {
					return ParametricMaterialHandle->IsEditable() ?
						(ParametricChildHandle->IsEditable() ? EVisibility::Visible : EVisibility::Hidden) :
						EVisibility::Hidden;
				}));
			}

			// Use primary uv params
			TSharedPtr<IPropertyHandle> UsePrimaryUVParamsHandle = MeshPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaShapeMeshData, bOverridePrimaryUVParams));
			check(UsePrimaryUVParamsHandle.IsValid());

			FString UsePrimaryParamsName = MeshName + TEXT("Override UV");
			FDetailWidgetRow& UsePrimaryUVParamsRow = CategoryBuilder.AddCustomRow(FText::FromString(UsePrimaryParamsName));

			UsePrimaryUVParamsRow.NameContent()[UsePrimaryUVParamsHandle->CreatePropertyNameWidget(FText::FromString(UsePrimaryParamsName))];
			UsePrimaryUVParamsRow.ValueContent()[UsePrimaryUVParamsHandle->CreatePropertyValueWidget()];
			UsePrimaryUVParamsRow.Visibility(MakeAttributeLambda([=]() {
				return UsePrimaryUVParamsHandle->IsEditable() ? EVisibility::Visible : EVisibility::Hidden;
			}));

			// Only Add it the first time at this specific point
			if (UsePrimaryMaterialEverywhereHandle.IsValid() && Index == 0 && MeshNames.Num() > 1)
			{
				CategoryBuilder.AddProperty(UsePrimaryMaterialEverywhereHandle);
			}

			// uv params
			TSharedPtr<IPropertyHandle> MaterialUVHandle = MeshPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAvaShapeMeshData, MaterialUVParams));
			check(MaterialUVHandle.IsValid());

			IDetailPropertyRow& MaterialUVRow = CategoryBuilder.AddProperty(MaterialUVHandle.ToSharedRef());
			FString MaterialUVName = MeshName + TEXT("Material UV");
			MaterialUVRow.DisplayName(FText::FromString(MaterialUVName));
			MaterialUVRow.Visibility(MakeAttributeLambda([=]() {
				return MaterialUVHandle->IsEditable() ? EVisibility::Visible : EVisibility::Hidden;
			}));

			// Separator row
			FDetailWidgetRow& SeparatorRow = CategoryBuilder.AddCustomRow(FText::GetEmpty());
			SeparatorRow.WholeRowContent()[
				SNullWidget::NullWidget
			];

			// visibility for the separator row
			SeparatorRow.Visibility(MakeAttributeLambda([=]() {
				return UsePrimaryUVParamsHandle->IsEditable() || MaterialUVHandle->IsEditable() ? EVisibility::Visible : EVisibility::Hidden;
			}));
		}
	}

	// StaticMesh Export button

	static const FText ConvertToStaticMeshText = LOCTEXT("ConvertToStaticMesh", "Create StaticMesh");

	IDetailGroup& GroupBuilderExport = CategoryBuilder.AddGroup(FName("Export"), FText::FromString(TEXT("Export")), false, true);
	GroupBuilderExport.ToggleExpansion(true);

	GroupBuilderExport.AddWidgetRow()
	.NameContent()
	[
		SNullWidget::NullWidget
	]
	.ValueContent()
	.VAlign(VAlign_Center)
	.MaxDesiredWidth(250)
	[
		SNew(SButton)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT("ConvertToStaticMeshTooltip", "Create a new StaticMesh asset using current geometry from this DynamicMeshComponent. Does not modify instance."))
		.OnClicked(this, &FAvaMeshesDetailCustomization::OnConvertToStaticMeshClicked)
		.IsEnabled(this, &FAvaMeshesDetailCustomization::CanConvertToStaticMesh)
		.Content()
		[
			SNew(STextBlock)
			.Text(ConvertToStaticMeshText)
		]
	];
}

FReply FAvaMeshesDetailCustomization::OnConvertToStaticMeshClicked()
{
	if (!CanConvertToStaticMesh())
	{
		return FReply::Handled();
	}

	TWeakObjectPtr<UAvaShapeDynamicMeshBase> DynMeshBase = SelectedDynamicMeshes[0];

	if (!DynMeshBase.IsValid())
	{
		return FReply::Handled();
	}

	// generate name for asset
	FString NewNameSuggestion = TEXT("SM_MotionDesign") + DynMeshBase->GetMeshName();
	FString PackageName = FString(TEXT("/Game/Meshes/")) + NewNameSuggestion;
	FString AssetName;

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, AssetName);

	TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
		SNew(SDlgPickAssetPath)
		.Title(LOCTEXT("ConvertToStaticMeshPickName", "Choose New StaticMesh Location"))
		.DefaultAssetPath(FText::FromString(PackageName));

	if (PickAssetPathWidget->ShowModal() != EAppReturnType::Ok)
	{
		return FReply::Handled();
	}

	// get input name provided by user
	FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
	FName MeshName(*FPackageName::GetLongPackageAssetName(UserPackageName));

	// is input name valid ?
	if (MeshName == NAME_None)
	{
		// Use default if invalid
		UserPackageName = PackageName;
		MeshName = *AssetName;
	}

	UE::Geometry::FDynamicMesh3* MeshIn = DynMeshBase->GetShapeMeshComponent()->GetMesh();

	// empty mesh do not export
	if (!MeshIn || MeshIn->TriangleCount() == 0)
	{
		return FReply::Handled();
	}

	// find/create package
	UPackage* Package = CreatePackage(*UserPackageName);
	check(Package);

	// Create StaticMesh object
	UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, MeshName, RF_Public | RF_Standalone);

	if (DynMeshBase->ExportToStaticMesh(StaticMesh))
	{
		// Notify asset registry of new asset
		FAssetRegistryModule::AssetCreated(StaticMesh);
	}

	return FReply::Handled();
}

bool FAvaMeshesDetailCustomization::CanConvertToStaticMesh() const
{
	return SelectedDynamicMeshes.Num() == 1;
}

#undef LOCTEXT_NAMESPACE