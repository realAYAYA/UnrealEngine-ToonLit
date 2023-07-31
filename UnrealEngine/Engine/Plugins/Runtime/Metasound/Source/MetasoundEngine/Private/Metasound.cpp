// Copyright Epic Games, Inc. All Rights Reserved.
#include "Metasound.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Internationalization/Text.h"
#include "Logging/TokenizedMessage.h"
#include "MetasoundAssetBase.h"
#include "MetasoundAudioFormats.h"
#include "MetasoundEngineArchetypes.h"
#include "MetasoundEngineAsset.h"
#include "MetasoundEngineEnvironment.h"
#include "MetasoundEnvironment.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendQuery.h"
#include "MetasoundFrontendQuerySteps.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundGenerator.h"
#include "MetasoundLog.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundPrimitives.h"
#include "MetasoundReceiveNode.h"
#include "MetasoundTrigger.h"
#include "MetasoundUObjectRegistry.h"
#include "UObject/ObjectSaveContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Metasound)

#if WITH_EDITORONLY_DATA
#include "EdGraph/EdGraph.h"
#endif // WITH_EDITORONLY_DATA

#define LOCTEXT_NAMESPACE "MetaSound"


int32 UMetasoundEditorGraphBase::GetHighestMessageSeverity() const
{
	int32 HighestMessageSeverity = EMessageSeverity::Info;

	for (const UEdGraphNode* Node : Nodes)
	{
		// Lower integer value is "higher severity"
		if (Node->ErrorType < HighestMessageSeverity)
		{
			HighestMessageSeverity = Node->ErrorType;
		}
	}

	return HighestMessageSeverity;
}


UMetaSoundPatch::UMetaSoundPatch(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, FMetasoundAssetBase()
{
}

#if WITH_EDITOR
void UMetaSoundPatch::PostDuplicate(EDuplicateMode::Type InDuplicateMode)
{
	Super::PostDuplicate(InDuplicateMode);

	// Guid is reset as asset may share implementation from
	// asset duplicated from but should not be registered as such.
	if (InDuplicateMode == EDuplicateMode::Normal)
	{
		AssetClassID = FGuid::NewGuid();
		Metasound::Frontend::FRenameRootGraphClass::Generate(GetDocumentHandle(), AssetClassID);
	}
}

void UMetaSoundPatch::PostEditUndo()
{
	Super::PostEditUndo();
	Metasound::FMetaSoundEngineAssetHelper::PostEditUndo(*this);
}
#endif // WITHEDITOR

void UMetaSoundPatch::BeginDestroy()
{
	UnregisterGraphWithFrontend();
	Super::BeginDestroy();
}

void UMetaSoundPatch::PreSave(FObjectPreSaveContext InSaveContext)
{
	Super::PreSave(InSaveContext);
	Metasound::FMetaSoundEngineAssetHelper::PreSaveAsset(*this, InSaveContext);
}

void UMetaSoundPatch::Serialize(FArchive& InArchive)
{
	Super::Serialize(InArchive);
	Metasound::FMetaSoundEngineAssetHelper::SerializeToArchive(*this, InArchive);
}

#if WITH_EDITORONLY_DATA
UEdGraph* UMetaSoundPatch::GetGraph()
{
	return Graph;
}

const UEdGraph* UMetaSoundPatch::GetGraph() const
{
	return Graph;
}

UEdGraph& UMetaSoundPatch::GetGraphChecked()
{
	check(Graph);
	return *Graph;
}

const UEdGraph& UMetaSoundPatch::GetGraphChecked() const
{
	check(Graph);
	return *Graph;
}

FText UMetaSoundPatch::GetDisplayName() const
{
	FString TypeName = UMetaSoundPatch::StaticClass()->GetName();
	return FMetasoundAssetBase::GetDisplayName(MoveTemp(TypeName));
}


void UMetaSoundPatch::SetRegistryAssetClassInfo(const Metasound::Frontend::FNodeClassInfo& InNodeInfo)
{
	Metasound::FMetaSoundEngineAssetHelper::SetMetaSoundRegistryAssetClassInfo(*this, InNodeInfo);
}
#endif // WITH_EDITORONLY_DATA

void UMetaSoundPatch::PostLoad() 
{
	Super::PostLoad();
	Metasound::FMetaSoundEngineAssetHelper::PostLoad(*this);
}

Metasound::Frontend::FNodeClassInfo UMetaSoundPatch::GetAssetClassInfo() const
{
	return { GetDocumentChecked().RootGraph, FSoftObjectPath(this) };
}

#if WITH_EDITOR
void UMetaSoundPatch::SetReferencedAssetClasses(TSet<Metasound::Frontend::IMetaSoundAssetManager::FAssetInfo>&& InAssetClasses)
{
	Metasound::FMetaSoundEngineAssetHelper::SetReferencedAssetClasses(*this, MoveTemp(InAssetClasses));
}
#endif

TArray<FMetasoundAssetBase*> UMetaSoundPatch::GetReferencedAssets()
{
	return Metasound::FMetaSoundEngineAssetHelper::GetReferencedAssets(*this);
}

const TSet<FSoftObjectPath>& UMetaSoundPatch::GetAsyncReferencedAssetClassPaths() const 
{
	return ReferenceAssetClassCache;
}

void UMetaSoundPatch::OnAsyncReferencedAssetsLoaded(const TArray<FMetasoundAssetBase*>& InAsyncReferences)
{
	Metasound::FMetaSoundEngineAssetHelper::OnAsyncReferencedAssetsLoaded(*this, InAsyncReferences);
}

#undef LOCTEXT_NAMESPACE // MetaSound

