// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVR/DMXMVRGeneralSceneDescription.h"

#include "DMXProtocolCommon.h"
#include "DMXRuntimeLog.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXEntityFixtureType.h"
#include "Library/DMXGDTFAssetImportData.h"
#include "Library/DMXImportGDTF.h"
#include "Library/DMXLibrary.h"
#include "MVR/DMXMVRAssetImportData.h"
#include "MVR/Types/DMXMVRChildListNode.h"
#include "MVR/Types/DMXMVRFixtureNode.h"
#include "MVR/Types/DMXMVRGroupObjectNode.h"
#include "MVR/Types/DMXMVRLayerNode.h"
#include "MVR/Types/DMXMVRLayersNode.h"
#include "MVR/Types/DMXMVRParametricObjectNodeBase.h"
#include "MVR/Types/DMXMVRRootNode.h"
#include "MVR/Types/DMXMVRSceneNode.h"

#include "XmlFile.h"
#include "Algo/RemoveIf.h"
#include "EditorFramework/AssetImportData.h"
#include "Misc/Paths.h"


#define LOCTEXT_NAMESPACE "DMXMVRGeneralSceneDescription"

UDMXMVRGeneralSceneDescription::UDMXMVRGeneralSceneDescription()
{
	RootNode = CreateDefaultSubobject<UDMXMVRRootNode>("MVRRootNode");

#if WITH_EDITORONLY_DATA
	MVRAssetImportData = CreateDefaultSubobject<UDMXMVRAssetImportData>(TEXT("MVRAssetImportData"));
#endif
}

void UDMXMVRGeneralSceneDescription::GetFixtureNodes(TArray<UDMXMVRFixtureNode*>& OutFixtureNodes) const
{
	checkf(RootNode, TEXT("Unexpected: MVR General Scene Description Root Node is invalid."));

	OutFixtureNodes.Reset();
	RootNode->GetFixtureNodes(OutFixtureNodes);
}

UDMXMVRFixtureNode* UDMXMVRGeneralSceneDescription::FindFixtureNode(const FGuid& FixtureUUID) const
{
	checkf(RootNode, TEXT("Unexpected: MVR General Scene Description Root Node is invalid."));

	if (TObjectPtr<UDMXMVRParametricObjectNodeBase>* ObjectNodePtr = RootNode->FindParametricObjectNodeByUUID(FixtureUUID))
	{
		return Cast<UDMXMVRFixtureNode>(*ObjectNodePtr);
	}
	return nullptr;
}

#if WITH_EDITOR
UDMXMVRGeneralSceneDescription* UDMXMVRGeneralSceneDescription::CreateFromXmlFile(TSharedRef<FXmlFile> GeneralSceneDescriptionXml, UObject* Outer, FName Name, EObjectFlags Flags)
{
	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = NewObject<UDMXMVRGeneralSceneDescription>(Outer, Name, Flags);
	GeneralSceneDescription->ParseGeneralSceneDescriptionXml(GeneralSceneDescriptionXml);

	return GeneralSceneDescription;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
UDMXMVRGeneralSceneDescription* UDMXMVRGeneralSceneDescription::CreateFromDMXLibrary(const UDMXLibrary& DMXLibrary, UObject* Outer, FName Name, EObjectFlags Flags)
{
	UDMXMVRGeneralSceneDescription* GeneralSceneDescription = NewObject<UDMXMVRGeneralSceneDescription>(Outer, Name, Flags);
	GeneralSceneDescription->WriteDMXLibraryToGeneralSceneDescription(DMXLibrary);

	return GeneralSceneDescription;
}
#endif // WITH_EDITOR


// Hotfix UE5.1.1: To set MVR Fixture UUIDs faster, hold newly added nodes in the array here
namespace UE::DMX::DMXMVRGeneralSceneDescription::HotFix::Private
{
	TArray<UDMXMVRFixtureNode*> NewNodes;
}

#if WITH_EDITOR
void UDMXMVRGeneralSceneDescription::WriteDMXLibraryToGeneralSceneDescription(const UDMXLibrary& DMXLibrary)
{
	TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary.GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	FixturePatches.Sort([](const UDMXEntityFixturePatch& FixturePatchA, const UDMXEntityFixturePatch& FixturePatchB)
		{
			const bool bUniverseIsSmaller = FixturePatchA.GetUniverseID() < FixturePatchB.GetUniverseID();
			const bool bUniverseIsEqual = FixturePatchA.GetUniverseID() == FixturePatchB.GetUniverseID();
			const bool bAddressIsSmaller = FixturePatchA.GetStartingChannel() <= FixturePatchB.GetStartingChannel();

			return bUniverseIsSmaller || (bUniverseIsEqual && bAddressIsSmaller);
		});

	// Remove Fixture Nodes no longer defined in the DMX Library
	TArray<FGuid> MVRFixtureUUIDsInUse;
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
	{
		MVRFixtureUUIDsInUse.Add(FixturePatch->GetMVRFixtureUUID());
	}
	TArray<UDMXMVRFixtureNode*> FixtureNodes;
	RootNode->GetFixtureNodes(FixtureNodes);
	for (UDMXMVRFixtureNode* FixtureNode : FixtureNodes)
	{
		if (!FixtureNode || !MVRFixtureUUIDsInUse.Contains(FixtureNode->UUID))
		{
			RootNode->RemoveParametricObjectNode(FixtureNode);
		}
	}

	// Create or update MVR Fixtures for each Fixture Patch's MVR Fixture UUIDs
	UE::DMX::DMXMVRGeneralSceneDescription::HotFix::Private::NewNodes.Reset();
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
	{
		if (FixturePatch)
		{
			WriteFixturePatchToGeneralSceneDescription(*FixturePatch);
		}
	}

	// Generate a Fixture ID for all new nodes. This is a 5.1.1 optimization to avoid the O1 lookup for each patch individually.
	const TArray<int32> FixtureIDsInUse = GetNumericalFixtureIDsInUse(DMXLibrary);
	int32 NextFixtureID = FixtureIDsInUse.Num() > 0 ? FixtureIDsInUse.Last() + 1 : 1;
	
	for (UDMXMVRFixtureNode* FixtureNode : UE::DMX::DMXMVRGeneralSceneDescription::HotFix::Private::NewNodes)
	{
		FixtureNode->FixtureID = FString::FromInt(NextFixtureID);
		NextFixtureID++;
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
bool UDMXMVRGeneralSceneDescription::CanCreateXmlFile(FText& OutReason) const
{
	TArray<UDMXMVRFixtureNode*> FixtureNodes;
	GetFixtureNodes(FixtureNodes);
	if (FixtureNodes.IsEmpty())
	{
		OutReason = LOCTEXT("CannotCreateXmlFileBecauseNoFixtures", "DMX Library does not define valid MVR fixtures.");
		return false;
	}
	return true;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
TSharedPtr<FXmlFile> UDMXMVRGeneralSceneDescription::CreateXmlFile() const
{
	return RootNode ? RootNode->CreateXmlFile() : nullptr;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXMVRGeneralSceneDescription::WriteFixturePatchToGeneralSceneDescription(const UDMXEntityFixturePatch& FixturePatch)
{
	checkf(RootNode, TEXT("Unexpected: MVR General Scene Description Root Node is invalid."));

	const UDMXLibrary* DMXLibrary = FixturePatch.GetParentLibrary();
	if (!DMXLibrary)
	{
		return;
	}

	const FGuid& MVRFixtureUUID = FixturePatch.GetMVRFixtureUUID();
	TObjectPtr<UDMXMVRParametricObjectNodeBase>* ParametricObjectNodePtr = RootNode->FindParametricObjectNodeByUUID(MVRFixtureUUID);

	UDMXMVRFixtureNode* MVRFixtureNode = nullptr;
	if (ParametricObjectNodePtr)
	{
		MVRFixtureNode = Cast<UDMXMVRFixtureNode>(*ParametricObjectNodePtr);
	}

	if (!MVRFixtureNode)
	{
		UDMXMVRChildListNode& AnyChildList = RootNode->GetOrCreateFirstChildListNode();
		MVRFixtureNode = AnyChildList.CreateParametricObject<UDMXMVRFixtureNode>();

		MVRFixtureNode->Name = FixturePatch.Name;
		MVRFixtureNode->UUID = MVRFixtureUUID;

		UE::DMX::DMXMVRGeneralSceneDescription::HotFix::Private::NewNodes.Add(MVRFixtureNode);
	}
	check(MVRFixtureNode);

	MVRFixtureNode->SetUniverseID(FixturePatch.GetUniverseID());
	MVRFixtureNode->SetStartingChannel(FixturePatch.GetStartingChannel());

	UDMXEntityFixtureType* FixtureType = FixturePatch.GetFixtureType();
	const int32 ModeIndex = FixturePatch.GetActiveModeIndex();
	bool bSetGDTFSpec = false;
	if (FixtureType &&
		FixtureType->Modes.IsValidIndex(ModeIndex))
	{
		MVRFixtureNode->GDTFMode = FixtureType->Modes[ModeIndex].ModeName;

		if (UDMXImportGDTF* GDTF = Cast<UDMXImportGDTF>(FixtureType->DMXImport))
		{
			const FString SourceFilename = [GDTF]()
			{
				if (GDTF && GDTF->GetGDTFAssetImportData())
				{
					return GDTF->GetGDTFAssetImportData()->GetFilePathAndName();
				}
				return FString();
			}();
			MVRFixtureNode->GDTFSpec = FPaths::GetCleanFilename(SourceFilename);

			bSetGDTFSpec = true;
		}
	}

	if (!bSetGDTFSpec)
	{
		// Don't set a mode when there's no GDTF
		MVRFixtureNode->GDTFMode = TEXT("");
		MVRFixtureNode->GDTFSpec = TEXT("");
	}
}
#endif // WITH_EDITOR

#if WITH_EDITOR
void UDMXMVRGeneralSceneDescription::ParseGeneralSceneDescriptionXml(const TSharedRef<FXmlFile>& GeneralSceneDescriptionXml)
{
	checkf(RootNode, TEXT("Unexpected: MVR General Scene Description Root Node is invalid."));

	RootNode->InitializeFromGeneralSceneDescriptionXml(GeneralSceneDescriptionXml);
}
#endif // WITH_EDITOR

TArray<int32> UDMXMVRGeneralSceneDescription::GetNumericalFixtureIDsInUse(const UDMXLibrary& DMXLibrary)
{
	checkf(RootNode, TEXT("Unexpected: MVR General Scene Description Root Node is invalid."));

	TArray<UDMXEntityFixturePatch*> FixturePatches = DMXLibrary.GetEntitiesTypeCast<UDMXEntityFixturePatch>();

	TArray<FGuid> MVRFixtureUUIDsInUse;
	for (UDMXEntityFixturePatch* FixturePatch : FixturePatches)
	{
		MVRFixtureUUIDsInUse.Add(FixturePatch->GetMVRFixtureUUID());
	}

	TArray<int32> FixtureIDsInUse;
	for (const FGuid& MVRFixtureUUID : MVRFixtureUUIDsInUse)
	{
		if (TObjectPtr<UDMXMVRParametricObjectNodeBase>* ObjectNodePtr = RootNode->FindParametricObjectNodeByUUID(MVRFixtureUUID))
		{
			if (UDMXMVRFixtureNode* FixtureNode = Cast<UDMXMVRFixtureNode>(*ObjectNodePtr))
			{
				int32 FixtureIDNumerical;
				if (LexTryParseString(FixtureIDNumerical, *FixtureNode->FixtureID))
				{
					FixtureIDsInUse.Add(FixtureIDNumerical);
				}
			}
		}
	}

	FixtureIDsInUse.Sort([](int32 FixtureIDA, int32 FixtureIDB)
		{
			return FixtureIDA < FixtureIDB;
		});

	return FixtureIDsInUse;
}

#undef LOCTEXT_NAMESPACE
