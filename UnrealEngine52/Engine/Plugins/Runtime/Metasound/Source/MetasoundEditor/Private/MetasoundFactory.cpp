// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundFactory.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Metasound.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorSettings.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetasoundFactory)


namespace Metasound
{
	namespace FactoryPrivate
	{
		template <typename T>
		T* CreateNewMetaSoundObject(UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InReferencedMetaSoundObject)
		{
			using namespace Editor;
			using namespace Frontend;

			T* MetaSoundObject = NewObject<T>(InParent, InName, InFlags);
			check(MetaSoundObject);

			FString Author = UKismetSystemLibrary::GetPlatformUserName();
			if (const UMetasoundEditorSettings* EditorSettings = GetDefault<UMetasoundEditorSettings>())
			{
				if (!EditorSettings->DefaultAuthor.IsEmpty())
				{
					Author = EditorSettings->DefaultAuthor;
				}
			}

			FGraphBuilder::InitMetaSound(*MetaSoundObject, Author);

			if (InReferencedMetaSoundObject)
			{
				FGraphBuilder::InitMetaSoundPreset(*InReferencedMetaSoundObject, *MetaSoundObject);
			}

			FMetasoundAssetBase* MetaSoundAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(MetaSoundObject);
			check(MetaSoundAsset);

			// Initial graph generation is not something to be managed by the transaction
			// stack, so don't track dirty state until after initial setup if necessary.
			UMetasoundEditorGraph* Graph = Cast<UMetasoundEditorGraph>(MetaSoundAsset->GetGraph());
			if (!Graph)
			{
				Graph = NewObject<UMetasoundEditorGraph>(MetaSoundObject, FName(), RF_Transactional);
				Graph->Schema = UMetasoundEditorGraphSchema::StaticClass();
				MetaSoundAsset->SetGraph(Graph);

				// Has to be done inline to have valid graph initially when opening editor for the first time
				FGraphBuilder::SynchronizeGraph(*MetaSoundObject);
			}

			return MetaSoundObject;
		}
	}
}

UMetaSoundBaseFactory::UMetaSoundBaseFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNew = true;
	bEditorImport = false;
	bEditAfterNew = true;
}

UMetaSoundFactory::UMetaSoundFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMetaSoundPatch::StaticClass();
}

UObject* UMetaSoundFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InFeedbackContext)
{
	using namespace Metasound::FactoryPrivate;
	return CreateNewMetaSoundObject<UMetaSoundPatch>(InParent, InName, InFlags, ReferencedMetaSoundObject);
}

UMetaSoundSourceFactory::UMetaSoundSourceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMetaSoundSource::StaticClass();
}

UObject* UMetaSoundSourceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InFeedbackContext)
{
	using namespace Metasound::FactoryPrivate;
	UMetaSoundSource* NewSource = CreateNewMetaSoundObject<UMetaSoundSource>(InParent, InName, InFlags, ReferencedMetaSoundObject);

	// Copy over referenced fields that are specific to sources
	if (UMetaSoundSource* ReferencedMetaSound = Cast<UMetaSoundSource>(ReferencedMetaSoundObject))
	{
		NewSource->OutputFormat = ReferencedMetaSound->OutputFormat;
	}

	return NewSource;
}

