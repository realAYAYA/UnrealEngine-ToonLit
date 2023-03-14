// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewer/ReferenceViewerSchema.h"
#include "Textures/SlateIcon.h"
#include "Misc/Attribute.h"
#include "SReferenceViewer.h"
#include "ToolMenus.h"
#include "EdGraph/EdGraph.h"
#include "Styling/AppStyle.h"
#include "CollectionManagerTypes.h"
#include "AssetManagerEditorCommands.h"
#include "AssetManagerEditorModule.h"
#include "Toolkits/GlobalEditorCommonCommands.h"
#include "ConnectionDrawingPolicy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ReferenceViewerSchema)

namespace UE
{
namespace DependencyPinCategory
{
	FName NamePassive(TEXT("Passive"));
	FName NameHardUsedInGame(TEXT("Hard"));
	FName NameHardEditorOnly(TEXT("HardEditorOnly"));
	FName NameSoftUsedInGame(TEXT("Soft"));
	FName NameSoftEditorOnly(TEXT("SoftEditorOnly"));
	const FLinearColor ColorPassive = FLinearColor(128, 128, 128);
	const FLinearColor ColorHardUsedInGame = FLinearColor(FColor(236, 252, 227)); // RiceFlower
	const FLinearColor ColorHardEditorOnly = FLinearColor(FColor(118, 126, 114));
	const FLinearColor ColorSoftUsedInGame = FLinearColor(FColor(145, 66, 117)); // CannonPink
	const FLinearColor ColorSoftEditorOnly = FLinearColor(FColor(73, 33, 58));

}
}

EDependencyPinCategory ParseDependencyPinCategory(FName PinCategory)
{
	if (PinCategory == UE::DependencyPinCategory::NameHardUsedInGame)
	{
		return EDependencyPinCategory::LinkEndActive | EDependencyPinCategory::LinkTypeHard | EDependencyPinCategory::LinkTypeUsedInGame;
	}
	else if (PinCategory == UE::DependencyPinCategory::NameHardEditorOnly)
	{
		return EDependencyPinCategory::LinkEndActive | EDependencyPinCategory::LinkTypeHard;
	}
	else if (PinCategory == UE::DependencyPinCategory::NameSoftUsedInGame)
	{
		return EDependencyPinCategory::LinkEndActive | EDependencyPinCategory::LinkTypeUsedInGame;
	}
	else if (PinCategory == UE::DependencyPinCategory::NameSoftEditorOnly)
	{
		return EDependencyPinCategory::LinkEndActive;
	}
	else
	{
		return EDependencyPinCategory::LinkEndPassive;
	}
}

FName GetName(EDependencyPinCategory Category)
{
	if ((Category & EDependencyPinCategory::LinkEndMask) == EDependencyPinCategory::LinkEndPassive)
	{
		return UE::DependencyPinCategory::NamePassive;
	}
	else
	{
		switch (Category & EDependencyPinCategory::LinkTypeMask)
		{
		case EDependencyPinCategory::LinkTypeHard | EDependencyPinCategory::LinkTypeUsedInGame:
			return UE::DependencyPinCategory::NameHardUsedInGame;
		case EDependencyPinCategory::LinkTypeHard:
			return UE::DependencyPinCategory::NameHardEditorOnly;
		case EDependencyPinCategory::LinkTypeUsedInGame:
			return UE::DependencyPinCategory::NameSoftUsedInGame;
		default:
			return UE::DependencyPinCategory::NameSoftEditorOnly;
		}
	}
}


FLinearColor GetColor(EDependencyPinCategory Category)
{
	if ((Category & EDependencyPinCategory::LinkEndMask) == EDependencyPinCategory::LinkEndPassive)
	{
		return UE::DependencyPinCategory::ColorPassive;
	}
	else
	{
		switch (Category & EDependencyPinCategory::LinkTypeMask)
		{
		case EDependencyPinCategory::LinkTypeHard | EDependencyPinCategory::LinkTypeUsedInGame:
			return UE::DependencyPinCategory::ColorHardUsedInGame;
		case EDependencyPinCategory::LinkTypeHard:
			return UE::DependencyPinCategory::ColorHardEditorOnly;
		case EDependencyPinCategory::LinkTypeUsedInGame:
			return UE::DependencyPinCategory::ColorSoftUsedInGame;
		default:
			return UE::DependencyPinCategory::ColorSoftEditorOnly;
		}
	}
}

// Overridden connection drawing policy to use less curvy lines between nodes
class FReferenceViewerConnectionDrawingPolicy : public FConnectionDrawingPolicy
{
public:
	FReferenceViewerConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, FSlateWindowElementList& InDrawElements)
		: FConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements)
	{
	}

	virtual FVector2D ComputeSplineTangent(const FVector2D& Start, const FVector2D& End) const override
	{
		const int32 Tension = FMath::Abs<int32>(Start.X - End.X);
		return Tension * FVector2D(1.0f, 0);
	}

	virtual void DetermineWiringStyle(UEdGraphPin* OutputPin, UEdGraphPin* InputPin, /*inout*/ FConnectionParams& Params) override
	{
		EDependencyPinCategory OutputCategory = ParseDependencyPinCategory(OutputPin->PinType.PinCategory);
		EDependencyPinCategory InputCategory = ParseDependencyPinCategory(InputPin->PinType.PinCategory);

		EDependencyPinCategory Category = !!(OutputCategory & EDependencyPinCategory::LinkEndActive) ? OutputCategory : InputCategory;
		Params.WireColor = GetColor(Category);
	}
};

//////////////////////////////////////////////////////////////////////////
// UReferenceViewerSchema

UReferenceViewerSchema::UReferenceViewerSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UReferenceViewerSchema::GetContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	{
		FToolMenuSection& Section = Menu->AddSection(TEXT("Asset"), NSLOCTEXT("ReferenceViewerSchema", "AssetSectionLabel", "Asset"));
		Section.AddMenuEntry(FGlobalEditorCommonCommands::Get().FindInContentBrowser);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().OpenSelectedInAssetEditor);
	}

	{
		FToolMenuSection& Section = Menu->AddSection(TEXT("Misc"), NSLOCTEXT("ReferenceViewerSchema", "MiscSectionLabel", "Misc"));
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().ZoomToFit);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().ReCenterGraph);
		Section.AddSubMenu(
			"MakeCollectionWith",
			NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithTitle", "Make Collection with"),
			NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithTooltip", "Makes a collection with either the referencers or dependencies of the selected nodes."),
			FNewToolMenuDelegate::CreateUObject(const_cast<UReferenceViewerSchema*>(this), &UReferenceViewerSchema::GetMakeCollectionWithSubMenu)
		);
	}

	{
		FToolMenuSection& Section = Menu->AddSection(TEXT("References"), NSLOCTEXT("ReferenceViewerSchema", "ReferencesSectionLabel", "References"));
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().CopyReferencedObjects);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().CopyReferencingObjects);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().ShowReferencedObjects);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().ShowReferencingObjects);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().ShowReferenceTree);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().ViewSizeMap);

		FToolMenuEntry ViewAssetAuditEntry = FToolMenuEntry::InitMenuEntry(FAssetManagerEditorCommands::Get().ViewAssetAudit);
		ViewAssetAuditEntry.Name = TEXT("ContextMenu");
		Section.AddEntry(ViewAssetAuditEntry);
	}
}

FLinearColor UReferenceViewerSchema::GetPinTypeColor(const FEdGraphPinType& PinType) const
{
	return GetColor(ParseDependencyPinCategory(PinType.PinCategory));
}

void UReferenceViewerSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const
{
	// Don't allow breaking any links
}

void UReferenceViewerSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) const
{
	// Don't allow breaking any links
}

FPinConnectionResponse UReferenceViewerSchema::MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsIntermediateMove, bool bNotifyLinkedNodes) const
{
	// Don't allow moving any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, FString());
}

FPinConnectionResponse UReferenceViewerSchema::CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsIntermediateCopy) const
{
	// Don't allow copying any links
	return FPinConnectionResponse(CONNECT_RESPONSE_DISALLOW, FString());
}

FConnectionDrawingPolicy* UReferenceViewerSchema::CreateConnectionDrawingPolicy(int32 InBackLayerID, int32 InFrontLayerID, float InZoomFactor, const FSlateRect& InClippingRect, class FSlateWindowElementList& InDrawElements, class UEdGraph* InGraphObj) const
{
	return new FReferenceViewerConnectionDrawingPolicy(InBackLayerID, InFrontLayerID, InZoomFactor, InClippingRect, InDrawElements);
}

void UReferenceViewerSchema::DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const
{
	TArray<FAssetIdentifier> AssetIdentifiers;

	IAssetManagerEditorModule::ExtractAssetIdentifiersFromAssetDataList(Assets, AssetIdentifiers);
	IAssetManagerEditorModule::Get().OpenReferenceViewerUI(AssetIdentifiers);
}

void UReferenceViewerSchema::GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const
{
	OutOkIcon = true;
}

void UReferenceViewerSchema::GetMakeCollectionWithSubMenu(UToolMenu* Menu)
{
	FToolMenuSection& Section = Menu->AddSection("Section");

	Section.AddSubMenu(
		"MakeCollectionWithReferencers",
		NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithReferencersTitle", "Referencers <-"),
		NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithReferencersTooltip", "Makes a collection with assets one connection to the left of selected nodes."),
		FNewToolMenuDelegate::CreateUObject(this, &UReferenceViewerSchema::GetMakeCollectionWithReferencersOrDependenciesSubMenu, true)
		);

	Section.AddSubMenu(
		"MakeCollectionWithDependencies",
		NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithDependenciesTitle", "Dependencies ->"),
		NSLOCTEXT("ReferenceViewerSchema", "MakeCollectionWithDependenciesTooltip", "Makes a collection with assets one connection to the right of selected nodes."),
		FNewToolMenuDelegate::CreateUObject(this, &UReferenceViewerSchema::GetMakeCollectionWithReferencersOrDependenciesSubMenu, false)
		);
}

void UReferenceViewerSchema::GetMakeCollectionWithReferencersOrDependenciesSubMenu(UToolMenu* Menu, bool bReferencers)
{
	FToolMenuSection& Section = Menu->AddSection("Section");

	if (bReferencers)
	{
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().MakeLocalCollectionWithReferencers, 
			TAttribute<FText>(),
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Local), 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Local))
			);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().MakePrivateCollectionWithReferencers,
			TAttribute<FText>(),
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Private), 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Private))
			);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().MakeSharedCollectionWithReferencers,
			TAttribute<FText>(),
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Shared), 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Shared))
			);
	}
	else
	{
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().MakeLocalCollectionWithDependencies, 
			TAttribute<FText>(),
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Local), 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Local))
			);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().MakePrivateCollectionWithDependencies,
			TAttribute<FText>(),
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Private), 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Private))
			);
		Section.AddMenuEntry(FAssetManagerEditorCommands::Get().MakeSharedCollectionWithDependencies,
			TAttribute<FText>(),
			ECollectionShareType::GetDescription(ECollectionShareType::CST_Shared), 
			FSlateIcon(FAppStyle::GetAppStyleSetName(), ECollectionShareType::GetIconStyleName(ECollectionShareType::CST_Shared))
			);
	}
}

