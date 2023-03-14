// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewer/EdGraphNode_Reference.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/PlatformFileManager.h"
#include "Styling/SlateIconFinder.h"
#include "SReferenceViewer.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "IAssetTypeActions.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EdGraphNode_Reference)

#define LOCTEXT_NAMESPACE "ReferenceViewer"

//////////////////////////////////////////////////////////////////////////
// UEdGraphNode_Reference

UEdGraphNode_Reference::UEdGraphNode_Reference(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DependencyPin = NULL;
	ReferencerPin = NULL;
	bIsCollapsed = false;
	bIsPackage = false;
	bIsPrimaryAsset = false;
	bUsesThumbnail = false;
	bAllowThumbnail = true;
	AssetTypeColor = FLinearColor(0.55f, 0.55f, 0.55f);
	bIsFiltered = false;
	bIsOverflow = false;
}

void UEdGraphNode_Reference::SetupReferenceNode(const FIntPoint& NodeLoc, const TArray<FAssetIdentifier>& NewIdentifiers, const FAssetData& InAssetData, bool bInAllowThumbnail, bool bInIsADuplicate)
{
	check(NewIdentifiers.Num() > 0);

	NodePosX = NodeLoc.X;
	NodePosY = NodeLoc.Y;

	Identifiers = NewIdentifiers;
	const FAssetIdentifier& First = NewIdentifiers[0];
	FString MainAssetName = InAssetData.AssetName.ToString();
	FString AssetTypeName = InAssetData.AssetClassPath.GetAssetName().ToString();

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));	
	if (UClass* AssetClass = InAssetData.GetClass())
	{
		TWeakPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(InAssetData.GetClass());
		if(AssetTypeActions.IsValid())
		{
			AssetTypeColor = AssetTypeActions.Pin()->GetTypeColor();
		}
	}
	AssetBrush = FSlateIcon("EditorStyle", FName( *("ClassIcon." + AssetTypeName)));

	bIsCollapsed = false;
	bIsPackage = true;
	bAllowThumbnail = bInAllowThumbnail;
	bIsADuplicate = bInIsADuplicate;

	FPrimaryAssetId PrimaryAssetID = NewIdentifiers[0].GetPrimaryAssetId();
	if (PrimaryAssetID.IsValid())  // Management References (PrimaryAssetIDs)
	{
		static FText ManagerText = LOCTEXT("ReferenceManager", "Manager");
		MainAssetName = PrimaryAssetID.PrimaryAssetType.ToString() + TEXT(":") + PrimaryAssetID.PrimaryAssetName.ToString();
		AssetTypeName = ManagerText.ToString();
		bIsPackage = false;
		bIsPrimaryAsset = true;
	}
	else if (First.IsValue()) // Searchable Names (GamePlay Tags, Data Table Row Handle)
	{
		MainAssetName = First.ValueName.ToString();
		AssetTypeName = First.ObjectName.ToString();
		static const FName NAME_DataTable(TEXT("DataTable"));
		static const FText InDataTableText = LOCTEXT("InDataTable", "In DataTable");
		if (InAssetData.AssetClassPath.GetAssetName() == NAME_DataTable)
		{
			AssetTypeName = InDataTableText.ToString() + TEXT(" ") + AssetTypeName;
		}

		bIsPackage = false;
	}
	else if (First.IsPackage() && !InAssetData.IsValid()) 
	{
		const FString PackageNameStr = Identifiers[0].PackageName.ToString();
		if ( PackageNameStr.StartsWith(TEXT("/Script")) )// C++ Packages (/Script Code)
		{
			MainAssetName = PackageNameStr.RightChop(8);
			AssetTypeName = TEXT("Script");
		}
	}

	if (NewIdentifiers.Num() == 1 )
	{
		static const FName NAME_ActorLabel(TEXT("ActorLabel"));
		InAssetData.GetTagValue(NAME_ActorLabel, MainAssetName); 

		// append the type so it shows up on the extra line
		NodeTitle = FText::FromString(FString::Printf(TEXT("%s\n%s"), *MainAssetName, *AssetTypeName));

		if (bIsPackage)
		{
			NodeComment = First.PackageName.ToString();
		}
	}
	else
	{
		NodeTitle = FText::Format(LOCTEXT("ReferenceNodeMultiplePackagesComment", "{0} and {1} others"), FText::FromString(MainAssetName), FText::AsNumber(NewIdentifiers.Num() - 1));
	}
	
	CacheAssetData(InAssetData);
	AllocateDefaultPins();
}

void UEdGraphNode_Reference::SetReferenceNodeCollapsed(const FIntPoint& NodeLoc, int32 InNumReferencesExceedingMax)
{
	NodePosX = NodeLoc.X;
	NodePosY = NodeLoc.Y;

	Identifiers.Empty();
	bIsCollapsed = true;
	bUsesThumbnail = false;
	bIsOverflow = true;
	AssetBrush = FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.WarningWithColor");

	NodeTitle = FText::Format( LOCTEXT("ReferenceNodeCollapsedTitle", "{0} Collapsed nodes"), FText::AsNumber(InNumReferencesExceedingMax));
	CacheAssetData(FAssetData());
	AllocateDefaultPins();
}

void UEdGraphNode_Reference::AddReferencer(UEdGraphNode_Reference* ReferencerNode)
{
	UEdGraphPin* ReferencerDependencyPin = ReferencerNode->GetDependencyPin();

	if ( ensure(ReferencerDependencyPin) )
	{
		ReferencerDependencyPin->bHidden = false;
		ReferencerPin->bHidden = false;
		ReferencerPin->MakeLinkTo(ReferencerDependencyPin);
	}
}

FAssetIdentifier UEdGraphNode_Reference::GetIdentifier() const
{
	if (Identifiers.Num() > 0)
	{
		return Identifiers[0];
	}

	return FAssetIdentifier();
}

void UEdGraphNode_Reference::GetAllIdentifiers(TArray<FAssetIdentifier>& OutIdentifiers) const
{
	OutIdentifiers.Append(Identifiers);
}

void UEdGraphNode_Reference::GetAllPackageNames(TArray<FName>& OutPackageNames) const
{
	for (const FAssetIdentifier& AssetId : Identifiers)
	{
		if (AssetId.IsPackage())
		{
			OutPackageNames.AddUnique(AssetId.PackageName);
		}
	}
}

UEdGraph_ReferenceViewer* UEdGraphNode_Reference::GetReferenceViewerGraph() const
{
	return Cast<UEdGraph_ReferenceViewer>( GetGraph() );
}

FText UEdGraphNode_Reference::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NodeTitle;
}

FLinearColor UEdGraphNode_Reference::GetNodeTitleColor() const
{
	if (bIsPrimaryAsset)
	{
		return FLinearColor(0.2f, 0.8f, 0.2f);
	}
	else if (bIsPackage)
	{
		return AssetTypeColor;
	}
	else if (bIsCollapsed)
	{
		return FLinearColor(0.55f, 0.55f, 0.55f);
	}
	else 
	{
		return FLinearColor(0.0f, 0.55f, 0.62f);
	}
}

FText UEdGraphNode_Reference::GetTooltipText() const
{
	FString TooltipString;
	for (const FAssetIdentifier& AssetId : Identifiers)
	{
		if (!TooltipString.IsEmpty())
		{
			TooltipString.Append(TEXT("\n"));
		}
		TooltipString.Append(AssetId.ToString());
	}
	return FText::FromString(TooltipString);
}

FSlateIcon UEdGraphNode_Reference::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = bIsOverflow ? FLinearColor::White : AssetTypeColor;
	return AssetBrush;
}

void UEdGraphNode_Reference::AllocateDefaultPins()
{
	ReferencerPin = CreatePin( EEdGraphPinDirection::EGPD_Input, NAME_None, NAME_None);
	DependencyPin = CreatePin( EEdGraphPinDirection::EGPD_Output, NAME_None, NAME_None);

	ReferencerPin->bHidden = true;
	FName PassiveName = ::GetName(EDependencyPinCategory::LinkEndPassive);
	ReferencerPin->PinType.PinCategory = PassiveName;
	DependencyPin->bHidden = true;
	DependencyPin->PinType.PinCategory = PassiveName;
}

UObject* UEdGraphNode_Reference::GetJumpTargetForDoubleClick() const
{
	if (Identifiers.Num() > 0 )
	{
		GetReferenceViewerGraph()->SetGraphRoot(Identifiers, FIntPoint(NodePosX, NodePosY));
		GetReferenceViewerGraph()->RebuildGraph();
	}
	return NULL;
}

UEdGraphPin* UEdGraphNode_Reference::GetDependencyPin()
{
	return DependencyPin;
}

UEdGraphPin* UEdGraphNode_Reference::GetReferencerPin()
{
	return ReferencerPin;
}

void UEdGraphNode_Reference::CacheAssetData(const FAssetData& AssetData)
{
	if ( AssetData.IsValid() && IsPackage() )
	{
		bUsesThumbnail = true;
		CachedAssetData = AssetData;
	}
	else
	{
		CachedAssetData = FAssetData();
		bUsesThumbnail = false;

		if (Identifiers.Num() == 1 )
		{
			const FString PackageNameStr = Identifiers[0].PackageName.ToString();
			if ( FPackageName::IsValidLongPackageName(PackageNameStr, true) )
			{
				if ( PackageNameStr.StartsWith(TEXT("/Script")) )
				{
					// Used Only in the UI for the Thumbnail
					CachedAssetData.AssetClassPath = FTopLevelAssetPath(TEXT("/EdGraphNode_Reference"), TEXT("Code"));
				}
				else
				{
					const FString PotentiallyMapFilename = FPackageName::LongPackageNameToFilename(PackageNameStr, FPackageName::GetMapPackageExtension());
					const bool bIsMapPackage = FPlatformFileManager::Get().GetPlatformFile().FileExists(*PotentiallyMapFilename);
					if ( bIsMapPackage )
					{
						// Used Only in the UI for the Thumbnail
						CachedAssetData.AssetClassPath = TEXT("/Script/Engine.World");
					}
				}
			}
		}
		else
		{
			CachedAssetData.AssetClassPath = FTopLevelAssetPath(TEXT("/EdGraphNode_Reference"), TEXT("Multiple Nodes"));
		}
	}

}

FAssetData UEdGraphNode_Reference::GetAssetData() const
{
	return CachedAssetData;
}

bool UEdGraphNode_Reference::AllowsThumbnail() const
{
	return bAllowThumbnail;
}

bool UEdGraphNode_Reference::UsesThumbnail() const
{
	return bUsesThumbnail;
}

bool UEdGraphNode_Reference::IsPackage() const
{
	return bIsPackage;
}

bool UEdGraphNode_Reference::IsCollapsed() const
{
	return bIsCollapsed;
}

void UEdGraphNode_Reference::SetIsFiltered(bool bInFiltered)
{
	bIsFiltered = bInFiltered;	
}

bool UEdGraphNode_Reference::GetIsFiltered() const
{
	return bIsFiltered;
}

#undef LOCTEXT_NAMESPACE

