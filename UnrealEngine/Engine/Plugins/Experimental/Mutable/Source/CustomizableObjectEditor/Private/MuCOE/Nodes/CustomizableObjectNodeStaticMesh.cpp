// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"

#include "AssetThumbnail.h"
#include "Containers/EnumAsByte.h"
#include "EdGraph/EdGraph.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/PlatformCrt.h"
#include "ISinglePropertyView.h"
#include "Internationalization/Internationalization.h"
#include "Layout/Margin.h"
#include "MaterialTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "Modules/ModuleManager.h"
#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/CustomizableObjectLayout.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/MutableUtils.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPins.h"
#include "MuCOE/RemapPins/CustomizableObjectNodeRemapPinsByNameDefaultPin.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyEditorModule.h"
#include "Serialization/Archive.h"
#include "SlotBase.h"
#include "StaticMeshResources.h"
#include "Styling/AppStyle.h"
#include "Styling/ISlateStyle.h"
#include "Templates/Casts.h"
#include "Types/SlateEnums.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealType.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

class UCustomizableObjectNodeRemapPinsByName;
class UObject;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


/** Default node pin configuration pin name (node does not have an static mesh). */
static const TCHAR* STATIC_MESH_PIN_NAME = TEXT("Static Mesh");


void UCustomizableObjectNodeStaticMesh::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged && PropertyThatChanged->GetName() == TEXT("StaticMesh"))
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeStaticMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::PostLoadToCustomVersion)
	{
		for (FCustomizableObjectNodeStaticMeshLOD& LOD : LODs)
		{
			for (FCustomizableObjectNodeStaticMeshMaterial& Material : LOD.Materials)
			{
				if (Material.MeshPin && !Material.MeshPinRef.Get())
				{
					UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(Material.MeshPin);
					Material.MeshPinRef.SetPin(AuxPin);
				}

				if (Material.LayoutPin && !Material.LayoutPinRef.Get())
				{
					UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(Material.LayoutPin);
					Material.LayoutPinRef.SetPin(AuxPin);
				}

				if (Material.ImagePins.Num() && !Material.ImagePinsRef.Num())
				{
					for (UEdGraphPin_Deprecated* DeprecatedPin : Material.ImagePins)
					{
						UEdGraphPin* AuxPin = UEdGraphPin::FindPinCreatedFromDeprecatedPin(DeprecatedPin);
						FEdGraphPinReference AuxEdGraphPinReference(AuxPin);
						Material.ImagePinsRef.Add(AuxEdGraphPinReference);
					}
				}
			}
		}
	}
}


void UCustomizableObjectNodeStaticMesh::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	// Pass information to the remap pins action context
	if (UCustomizableObjectNodeRemapPinsByNameDefaultPin* RemapPinsCustom = Cast<UCustomizableObjectNodeRemapPinsByNameDefaultPin>(RemapPins))
	{
		RemapPinsCustom->DefaultPin = DefaultPin.Get();
	}
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (StaticMesh)
	{
		const int NumLODs = StaticMesh->GetRenderData()->LODResources.Num();

		LODs.Reset(NumLODs);
		LODs.SetNum(NumLODs);
		for (int32 i = 0; i < NumLODs; ++i)
		{
			FString LODName = FString::Printf(TEXT("LOD %d, "), i);

			const int NumMaterials = StaticMesh->GetRenderData()->LODResources[i].Sections.Num();

			LODs[i].Materials.Reset(NumMaterials);
			LODs[i].Materials.SetNum(NumMaterials);
			for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex)
			{
				LODs[i].Materials.Reset(NumMaterials);
				LODs[i].Materials.SetNum(NumMaterials);
				
				UMaterialInterface* MaterialInterface = 0;
				int StaticMeshMaterialIndex = StaticMesh->GetRenderData()->LODResources[i].Sections[MaterialIndex].MaterialIndex;
				if (StaticMeshMaterialIndex >= 0 && StaticMeshMaterialIndex < StaticMesh->GetStaticMaterials().Num())
				{
					MaterialInterface = StaticMesh->GetStaticMaterials()[StaticMeshMaterialIndex].MaterialInterface;
				}

				FString MaterialName = TEXT("Unnamed Material");
				if (MaterialInterface)
				{
					MaterialName = MaterialInterface->GetName();
				}
				LODs[i].Materials[MaterialIndex].Name = MaterialName;

				// Mesh
				const FString MeshName = MaterialName + TEXT(" Mesh");
				UEdGraphPin* Pin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(*MeshName));
				Pin->bDefaultValueIsIgnored = true;
				LODs[i].Materials[MaterialIndex].MeshPinRef.SetPin(Pin);

				// Layout
				// TODO: Multiple layout ui
				const FString LayoutName = MaterialName + TEXT(" Layout");
				Pin = CustomCreatePin(EGPD_Input, Schema->PC_Layout, FName(*LayoutName));
				Pin->bDefaultValueIsIgnored = true;
				LODs[i].Materials[MaterialIndex].LayoutPinRef.SetPin(Pin);

				// Images
				if (MaterialInterface)
				{
					UMaterial* Material = MaterialInterface->GetMaterial();

					Helper_ForEachTextureParameterName(Material, [&](const FString& ImageName)
						{
							Pin = CustomCreatePin(EGPD_Output, Schema->PC_Image, *ImageName);
							Pin->bDefaultValueIsIgnored = true;
							LODs[i].Materials[MaterialIndex].ImagePinsRef.Add(Pin);
						});
				}
			}
		}

		DefaultPin = FEdGraphPinReference();
	}
	else
	{
		UEdGraphPin* Pin = CustomCreatePin(EGPD_Output, Schema->PC_Mesh, FName(STATIC_MESH_PIN_NAME));
		Pin->bDefaultValueIsIgnored = true;
		Pin->PinFriendlyName = FText::FromString(STATIC_MESH_PIN_NAME);

		DefaultPin = FEdGraphPinReference(Pin);
	}
}


FText UCustomizableObjectNodeStaticMesh::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (StaticMesh)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("MeshName"), FText::FromString(StaticMesh->GetName()));

		return FText::Format(LOCTEXT("StaticMesh_Title", "{MeshName}\nStatic Mesh"), Args);
	}
	else
	{
		return LOCTEXT("Static_Mesh", "Static Mesh");
	}
}


FLinearColor UCustomizableObjectNodeStaticMesh::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(Schema->PC_Mesh);
}


UTexture2D* UCustomizableObjectNodeStaticMesh::FindTextureForPin(const UEdGraphPin* Pin) const
{
	if (!StaticMesh)
	{
		return 0;
	}

	for (int LODIndex = 0; LODIndex < LODs.Num(); ++LODIndex)
	{
		for (int MaterialIndex = 0; MaterialIndex<LODs[LODIndex].Materials.Num(); ++MaterialIndex)
		{
			UMaterialInterface* MaterialInterface = 0;
			int StaticMeshMaterialIndex = -1;
			if (StaticMesh->GetRenderData()->LODResources.Num() > LODIndex && StaticMesh->GetRenderData()->LODResources[LODIndex].Sections.Num()>MaterialIndex)
			{
				StaticMeshMaterialIndex = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections[MaterialIndex].MaterialIndex;
			}

			if (StaticMeshMaterialIndex >= 0 && StaticMeshMaterialIndex < StaticMesh->GetStaticMaterials().Num() )
			{
				MaterialInterface = StaticMesh->GetStaticMaterials()[ StaticMeshMaterialIndex ].MaterialInterface;
			}

			if (MaterialInterface)
			{
				for (int ImageIndex = 0; ImageIndex < LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef.Num(); ++ImageIndex)
				{
					if (LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef[ImageIndex].Get() == Pin)
					{
						UTexture* Texture=nullptr;
						MaterialInterface->GetTextureParameterValue(Helper_GetPinFName(Pin), Texture);
						return Cast<UTexture2D>(Texture);
					}
				}
			}
		}
	}

	return 0;
}


void UCustomizableObjectNodeStaticMesh::GetUVChannelForPin(const UEdGraphPin* Pin, TArray<FVector2f>& OutSegments, int32 UVIndex) const
{
	check(Pin);
	
	if (!StaticMesh)
	{
		return;
	}

	int32 LODIndex;
	int32 SectionIndex;
	int32 LayoutIndex;
	GetPinSection(*Pin, LODIndex, SectionIndex, LayoutIndex);
	
	OutSegments = GetUV(*StaticMesh, LODIndex, SectionIndex, UVIndex);
}


TArray<class UCustomizableObjectLayout*> UCustomizableObjectNodeStaticMesh::GetLayouts(const UEdGraphPin* OutPin) const
{
	TArray<class UCustomizableObjectLayout*> Result;

	for (int LODIndex = 0; LODIndex<LODs.Num(); ++LODIndex)
	{
		for (int MaterialIndex = 0; MaterialIndex<LODs[LODIndex].Materials.Num(); ++MaterialIndex)
		{
			if (LODs[LODIndex].Materials[MaterialIndex].MeshPinRef.Get() == OutPin)
			{
				if (UEdGraphPin* LayoutInPin = LODs[LODIndex].Materials[MaterialIndex].LayoutPinRef.Get())
				{
					if (const UEdGraphPin* ConnectedPin = FollowInputPin(*LayoutInPin))
					{
						const UCustomizableObjectNodeLayoutBlocks* LayoutNode = Cast<UCustomizableObjectNodeLayoutBlocks>(ConnectedPin->GetOwningNode());
						if (LayoutNode && LayoutNode->Layout)
						{
							Result.Add(LayoutNode->Layout);
						}
					}
				}
			}
		}
	}

	return Result;
}


UObject* UCustomizableObjectNodeStaticMesh::GetMesh() const
{
	return StaticMesh;
}


UEdGraphPin* UCustomizableObjectNodeStaticMesh::GetMeshPin(const int32 LODIndex, const int MaterialIndex) const
{
	if (LODIndex < LODs.Num())
	{
		if (const FCustomizableObjectNodeStaticMeshLOD& LOD = LODs[LODIndex];
			MaterialIndex < LOD.Materials.Num())
		{
			return LOD.Materials[MaterialIndex].MeshPinRef.Get();
		}
	}

	return nullptr;
}


bool UCustomizableObjectNodeStaticMesh::IsPinRelevant(const UEdGraphPin* Pin) const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	if (Pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{
		return Pin->PinType.PinCategory == Schema->PC_Layout;
	}

	if (Pin->Direction == EEdGraphPinDirection::EGPD_Input)
	{	
		return Pin->PinType.PinCategory == Schema->PC_Mesh;
	}

	return false;
}


UCustomizableObjectNodeRemapPinsByName* UCustomizableObjectNodeStaticMesh::CreateRemapPinsByName() const
{
	return NewObject<UCustomizableObjectNodeRemapPinsByNameDefaultPin>();
}


bool UCustomizableObjectNodeStaticMesh::IsNodeOutDatedAndNeedsRefresh()
{
	bool Result = false;

	if (StaticMesh)
	{
		int32 numLods = StaticMesh->GetRenderData()->LODResources.Num();
		if (LODs.Num() != numLods)
		{
			Result = true;
		}

		for (int LODIndex = 0; !Result && LODIndex < LODs.Num(); ++LODIndex)
		{
			int32 NumMaterials = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections.Num();
			if (LODs[LODIndex].Materials.Num() != NumMaterials)
			{
				Result = true;
			}

			for (int MaterialIndex = 0; !Result && MaterialIndex < NumMaterials; ++MaterialIndex)
			{
				UMaterialInterface* MaterialInterface = nullptr;
				int SkeletalMeshMaterialIndex = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections[MaterialIndex].MaterialIndex;
				if (SkeletalMeshMaterialIndex >= 0 && SkeletalMeshMaterialIndex < StaticMesh->GetStaticMaterials().Num())
				{
					MaterialInterface = StaticMesh->GetStaticMaterials()[SkeletalMeshMaterialIndex].MaterialInterface;
				}

				if (MaterialInterface)
				{
					/*if (LODs[LODIndex].Materials[MaterialIndex].Name != MaterialInterface->GetName())
					{
						Result = true;
						break;
					}*/
					UMaterial* Material = MaterialInterface->GetMaterial();

					TArray<FMaterialParameterInfo> ImageInfos;
					TArray<FGuid> ImageIds;
					Material->GetAllTextureParameterInfo(ImageInfos, ImageIds);

					if (LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef.Num() != ImageInfos.Num())
					{
						Result = true;
						break;
					}

					for (int ImageIndex = 0; ImageIndex < ImageInfos.Num(); ++ImageIndex)
					{
						const FString ImagePinName = LODs[LODIndex].Materials[MaterialIndex].ImagePinsRef[ImageIndex].Get()->GetName();
						if (!ImagePinName.Contains(ImageInfos[ImageIndex].Name.ToString()))
						{
							Result = true;
							break;
						}
					}
				}
			}
		}
	}

	// Remove previous compilation warnings
	if (!Result && bHasCompilerMessage)
	{
		RemoveWarnings();
		GetGraph()->NotifyGraphChanged();
	}

	return Result;
}

FString UCustomizableObjectNodeStaticMesh::GetRefreshMessage() const
{
	return "Node data outdated. Please refresh node.";
}


FText UCustomizableObjectNodeStaticMesh::GetTooltipText() const
{
	return LOCTEXT("Static_Mesh_Tooltip", "Static Mesh");
}


void UCustomizableObjectNodeStaticMesh::GetPinSection(const UEdGraphPin& Pin, int32& OutLODIndex, int32& OutSectionIndex, int32& OutLayoutIndex) const
{
	OutLayoutIndex = 0; // Currently layouts different from 0 not supported for static mesh.
	
	for (OutLODIndex = 0; OutLODIndex < LODs.Num(); ++OutLODIndex)
	{
		const FCustomizableObjectNodeStaticMeshLOD& LOD = LODs[OutLODIndex];

		for (OutSectionIndex = 0; OutSectionIndex < LOD.Materials.Num(); ++OutSectionIndex)
		{
			const FCustomizableObjectNodeStaticMeshMaterial& Material = LOD.Materials[OutSectionIndex];

			// Is a Layout pin?
			if (Material.LayoutPinRef.Get() == &Pin)
			{
				return;
			}

			// Is a Mesh pin?
			if (Material.MeshPinRef.Get() == &Pin)
			{
				return;
			}
			
			// Is an Image pin?
			for (int ImageIndex = 0; ImageIndex < Material.ImagePinsRef.Num(); ++ImageIndex)
			{
				if (Material.ImagePinsRef[ImageIndex].Get() == &Pin)
				{
					return;
				}
			}
		}
	}
	
	check(false); // Pin not found. Probably a new pin has been added which is not contemplated in this function.
}


UMaterialInterface* UCustomizableObjectNodeStaticMesh::GetMaterialFor(const UEdGraphPin* Pin) const
{
	if (StaticMesh)
	{
		check(Pin);
		int32 LODIndex;
		int32 SectionIndex;
		int32 LayoutIndex;
		GetPinSection(*Pin, LODIndex, SectionIndex, LayoutIndex);

		const int32 MaterialIndex = StaticMesh->GetRenderData()->LODResources[LODIndex].Sections[SectionIndex].MaterialIndex;

		if (const TArray<FStaticMaterial>& StaticMaterials = StaticMesh->GetStaticMaterials();
			StaticMaterials.IsValidIndex(MaterialIndex))
		{
			return StaticMaterials[MaterialIndex].MaterialInterface;
		}
	}
	
	return nullptr;
}


TSharedPtr<SGraphNode> UCustomizableObjectNodeStaticMesh::CreateVisualWidget()
{
	TSharedPtr<SGraphNodeStaticMesh> GraphNode;
	SAssignNew(GraphNode, SGraphNodeStaticMesh, this);

	GraphNodeStaticMesh = GraphNode;

	return GraphNode;
}


void SGraphNodeStaticMesh::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	GraphNode = InGraphNode;
	NodeStaticMesh = Cast< UCustomizableObjectNodeStaticMesh >(GraphNode);

	WidgetSize = 128.0f;
	ThumbnailSize = 128;

	TSharedPtr<FCustomizableObjectEditor> Editor = StaticCastSharedPtr< FCustomizableObjectEditor >(NodeStaticMesh->GetGraphEditor());

	// Thumbnail
	AssetThumbnailPool = MakeShareable(new FAssetThumbnailPool(32));
	AssetThumbnail = MakeShareable(new FAssetThumbnail(NodeStaticMesh->StaticMesh, ThumbnailSize, ThumbnailSize, AssetThumbnailPool));

	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FSinglePropertyParams SingleDetails;
	SingleDetails.NamePlacement = EPropertyNamePlacement::Hidden;
	SingleDetails.NotifyHook = Editor.Get();
	SingleDetails.bHideAssetThumbnail = true;

	StaticMeshSelector = PropPlugin.CreateSingleProperty(NodeStaticMesh, "StaticMesh", SingleDetails);	

	UpdateGraphNode();
}


void SGraphNodeStaticMesh::UpdateGraphNode()
{
	SGraphNode::UpdateGraphNode();
}


void SGraphNodeStaticMesh::SetDefaultTitleAreaWidget(TSharedRef<SOverlay> DefaultTitleAreaWidget)
{
	DefaultTitleAreaWidget->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(FMargin(5))
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &SGraphNodeStaticMesh::OnExpressionPreviewChanged)
		.IsChecked(IsExpressionPreviewChecked())
		.Cursor(EMouseCursor::Default)
		.Style(FAppStyle::Get(), "Graph.Node.AdvancedView")
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SImage)
			.Image(GetExpressionPreviewArrow())
		]
		]
		];
}


void SGraphNodeStaticMesh::CreateBelowPinControls(TSharedPtr<SVerticalBox> MainBox)
{
	LeftNodeBox->AddSlot()
		.AutoHeight()
		.MaxHeight(WidgetSize)
		.Padding(10.0f, 10.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility(ExpressionPreviewVisibility())

		+ SHorizontalBox::Slot()
		.MaxWidth(WidgetSize)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			AssetThumbnail->MakeThumbnailWidget()
		]
		];

	if (StaticMeshSelector.IsValid())
	{
		LeftNodeBox->AddSlot()
			.AutoHeight()
			.Padding(10.0f, 5.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				.Visibility(ExpressionPreviewVisibility())

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(1.0f, 0.0f, 5.0f, 5.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				StaticMeshSelector.ToSharedRef()
			]
			];
	}
}


void SGraphNodeStaticMesh::OnExpressionPreviewChanged(const ECheckBoxState NewCheckedState)
{
	NodeStaticMesh->bCollapsed = (NewCheckedState != ECheckBoxState::Checked);
	UpdateGraphNode();
}


ECheckBoxState SGraphNodeStaticMesh::IsExpressionPreviewChecked() const
{
	return NodeStaticMesh->bCollapsed ? ECheckBoxState::Unchecked : ECheckBoxState::Checked;
}


const FSlateBrush* SGraphNodeStaticMesh::GetExpressionPreviewArrow() const
{
	return FCustomizableObjectEditorStyle::Get().GetBrush(NodeStaticMesh->bCollapsed ? TEXT("Nodes.ArrowDown") : TEXT("Nodes.ArrowUp"));
}


EVisibility SGraphNodeStaticMesh::ExpressionPreviewVisibility() const
{
	return NodeStaticMesh->bCollapsed ? EVisibility::Collapsed : EVisibility::Visible;
}


#undef LOCTEXT_NAMESPACE
