// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SCustomizableObjectEditorTextureAnalyzer.h"

#include "Containers/EnumAsByte.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/PlatformCrt.h"
#include "IPropertyTable.h"
#include "IPropertyTableCell.h"
#include "IPropertyTableColumn.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Layout/Visibility.h"
#include "MaterialTypes.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Attribute.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectInstanceEditor.h"
#include "PropertyEditorModule.h"
#include "SlotBase.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Templates/Casts.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

class SWidget;
struct FGeometry;

#define LOCTEXT_NAMESPACE "SCustomizableObjecEditorTextureAnalyzer"



void SCustomizableObjecEditorTextureAnalyzer::Construct(const FArguments& InArgs)
{
	CustomizableObjectEditor = InArgs._CustomizableObjectEditor;
	CustomizableObjectInstanceEditor = InArgs._CustomizableObjectInstanceEditor;

	TotalSizeTextures = SNew(STextBlock);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.AutoHeight()
		.Padding(1.0f, 5.0f, 0.0f, 10.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshTable", "Refresh Table"))
				.OnClicked(this, &SCustomizableObjecEditorTextureAnalyzer::RefreshTextureAnalyzerTable)
				.Visibility(CustomizableObjectEditor || CustomizableObjectInstanceEditor ? EVisibility::Visible : EVisibility::Collapsed)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 10.0f, 0.0f, 0.0f)
			[
				TotalSizeTextures.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot()
		[
			BuildTextureAnalyzerTable()
		]
	];
}


void SCustomizableObjecEditorTextureAnalyzer::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (UObject* i : TabTextures)
	{
		Collector.AddReferencedObject(i);
	}
}

void SCustomizableObjecEditorTextureAnalyzer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (TextureAnalyzerTable.IsValid())
	{
		if (TextureAnalyzerTable->GetLastClickedCell().IsValid() && TextureAnalyzerTable->GetLastClickedCell()->InEditMode())
		{
			OnTextureTableSelectionChanged(TextureAnalyzerTable->GetLastClickedCell());
		}
	}

	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}


TSharedRef<SWidget> SCustomizableObjecEditorTextureAnalyzer::BuildTextureAnalyzerTable()
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TextureAnalyzerTable = PropertyEditorModule.CreatePropertyTable();
	TextureAnalyzerTable->SetIsUserAllowedToChangeRoot(false);
	TextureAnalyzerTable->SetSelectionMode(ESelectionMode::Single);
	TextureAnalyzerTable->SetSelectionUnit(EPropertyTableSelectionUnit::Cell);
	TextureAnalyzerTable->SetShowObjectName(false);

	if (TextureAnalyzerTable.IsValid())
	{
		FillTextureAnalyzerTable();
	}

	return PropertyEditorModule.CreatePropertyTableWidget(TextureAnalyzerTable.ToSharedRef());
}


void SCustomizableObjecEditorTextureAnalyzer::FillTextureAnalyzerTable(UCustomizableObjectInstance* PreviewInstance)
{
	TabTextures.Empty();

	uint32 TotalSize = 0;

	if(!PreviewInstance)
	{
		if (CustomizableObjectEditor)
		{
			PreviewInstance = CustomizableObjectEditor->GetPreviewInstance();
		}

		if (CustomizableObjectInstanceEditor)
		{
			PreviewInstance = CustomizableObjectInstanceEditor->GetPreviewInstance();
		}
	}

	if (PreviewInstance && PreviewInstance->HasAnySkeletalMesh())
	{
		for (int32 ComponentIndex = 0; ComponentIndex < PreviewInstance->SkeletalMeshes.Num(); ++ComponentIndex)
		{
			if (PreviewInstance->GetSkeletalMesh(ComponentIndex))
			{
				for (int i = 0; i < PreviewInstance->GetSkeletalMesh(ComponentIndex)->GetMaterials().Num(); ++i)
				{
					UMaterialInterface* MaterialInterface = PreviewInstance->GetSkeletalMesh(ComponentIndex)->GetMaterials()[i].MaterialInterface;
					UMaterialInstance* Material = Cast<UMaterialInstance>(MaterialInterface);
					FString MaterialPath = MaterialInterface->GetPathName();

					// We just want the Transient materials
					if (Material && MaterialPath.Contains("Transient"))
					{
						for (int32 j = 0; j < Material->TextureParameterValues.Num(); ++j)
						{
							UTexture* Texture = Material->TextureParameterValues[j].ParameterValue;
							if (!Texture) continue;

							FString TextPath = Texture->GetPathName();

							// We just want the Transient textures
							if (TextPath.Contains("Transient"))
							{
								//New Object for the table
								UCustomizableObjectEditorTextureStats* Entry = NewObject<UCustomizableObjectEditorTextureStats>();
								TabTextures.Add(Entry);

								//Texture Info
								Entry->Texture = Texture;
								Entry->TextureName = Texture->GetName();
								Entry->TextureParameterName = Material->TextureParameterValues[j].ParameterInfo.Name.ToString();
								Entry->ResolutionX = Cast<UTexture2D>(Texture)->GetSizeX();
								Entry->ResolutionY = Cast<UTexture2D>(Texture)->GetSizeY();
								Entry->LODBias = Texture->GetCachedLODBias();
								uint32 textureSize = Texture->CalcTextureMemorySizeEnum(TMC_AllMips);
								Entry->Size = FString::Printf(TEXT("%.2f"), textureSize / 1024.0);
								Entry->Format = Cast<UTexture2D>(Texture)->GetPixelFormat();
								Entry->IsStreamed = Texture->IsCurrentlyVirtualTextured() ? "Virtual Streamed" : (!Texture->IsStreamable() ? "Not Streamed" : "Streamed");
								Entry->LODGroup = Texture->LODGroup;

								//Material Info
								Entry->Material = MaterialInterface;
								Entry->MaterialName = MaterialInterface->GetName();
								Entry->ParentMaterial = Material->Parent;
								Entry->MaterialParameterName = Material->Parent ? Material->Parent->GetName() : FString();
								Entry->Component = ComponentIndex;

								//Total size
								TotalSize += textureSize;
							}
						}
					}
				}
			}
		}

		TextureAnalyzerTable->SetObjects(TabTextures);
	}

	// Updating Texture Size Text
	if (TotalSizeTextures.IsValid())
	{
		TotalSizeTextures->SetText(FText::FromString(FString::Printf(TEXT("Total Size (All LODs, only Mutable): %.2f Mb "), TotalSize / (1024.0 * 1024.0))));
	}
}


void SCustomizableObjecEditorTextureAnalyzer::OnTextureTableSelectionChanged(TSharedPtr<class IPropertyTableCell> Cell)
{
	FString SelectedColumn = Cell->GetColumn()->GetDisplayName().ToString();

	if (UCustomizableObjectEditorTextureStats* Current = Cast<UCustomizableObjectEditorTextureStats>(Cell->GetObject().Get()))
	{
		//Open Texture Editor Tab
		if ((SelectedColumn == "Texture" || SelectedColumn == "Name") && Current->Texture)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Current->Texture);
		}

		//Open Material Editor Tab
		if (SelectedColumn == "Material" && Current->Material)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Current->Material);
		}

		if (SelectedColumn == "Parent" && Current->ParentMaterial)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Current->ParentMaterial);
		}
	}

	//Avoid edit mode (not needed)
	Cell->ExitEditMode();
}


FReply SCustomizableObjecEditorTextureAnalyzer::RefreshTextureAnalyzerTable()
{
	return RefreshTextureAnalyzerTable(nullptr);
}

FReply SCustomizableObjecEditorTextureAnalyzer::RefreshTextureAnalyzerTable(UCustomizableObjectInstance* PreviewInstance)
{
	FillTextureAnalyzerTable(PreviewInstance);

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE 