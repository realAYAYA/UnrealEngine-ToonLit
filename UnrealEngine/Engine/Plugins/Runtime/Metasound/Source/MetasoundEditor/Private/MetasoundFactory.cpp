// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundFactory.h"

#include "Metasound.h"
#include "MetasoundEditorGraph.h"
#include "MetasoundEditorGraphBuilder.h"
#include "MetasoundEditorGraphSchema.h"
#include "MetasoundEditorSubsystem.h"
#include "MetasoundFrontendDocumentBuilder.h"
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
			T* MetaSoundObject = NewObject<T>(InParent, InName, InFlags);
			check(MetaSoundObject);

			UMetaSoundEditorSubsystem::GetChecked().InitAsset(*MetaSoundObject, InReferencedMetaSoundObject);
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

void UMetaSoundBaseFactory::InitAsset(UObject& InNewMetaSound, UObject* InReferencedMetaSound)
{
	using namespace Metasound;
	using namespace Metasound::Editor;
	using namespace Metasound::Frontend;

	TScriptInterface<IMetaSoundDocumentInterface> DocInterface = &InNewMetaSound;
	FMetaSoundFrontendDocumentBuilder Builder(DocInterface);
	Builder.InitDocument();
#if WITH_EDITORONLY_DATA
	Builder.InitNodeLocations();
#endif // WITH_EDITORONLY_DATA

	const FString& Author = UMetaSoundEditorSubsystem::GetChecked().GetDefaultAuthor();
	Builder.SetAuthor(Author);

	// Initialize asset as a preset
	if (InReferencedMetaSound)
	{
		// Ensure the referenced MetaSound is registered already
		UMetaSoundEditorSubsystem::GetChecked().RegisterGraphWithFrontend(*InReferencedMetaSound);
		
		// Initialize preset with referenced Metasound 
		TScriptInterface<IMetaSoundDocumentInterface> ReferencedDocInterface = InReferencedMetaSound;
		Builder.ConvertToPreset(ReferencedDocInterface->GetConstDocument());
		
		// Update asset object data from interfaces 
		FMetasoundAssetBase* PresetAsset = IMetasoundUObjectRegistry::Get().GetObjectAsAssetBase(&InNewMetaSound);
		check(PresetAsset);
		PresetAsset->ConformObjectDataToInterfaces();
	}

	// Initial graph generation is not something to be managed by the transaction
	// stack, so don't track dirty state until after initial setup if necessary.
	UMetaSoundEditorSubsystem::GetChecked().InitEdGraph(InNewMetaSound);
}

UMetaSoundFactory::UMetaSoundFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMetaSoundPatch::StaticClass();
}

UObject* UMetaSoundFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InFeedbackContext)
{
	using namespace Metasound::Editor;
	using namespace Metasound::FactoryPrivate;

	if (UMetaSoundPatch* NewPatch = CreateNewMetaSoundObject<UMetaSoundPatch>(InParent, InName, InFlags, ReferencedMetaSoundObject))
	{
		FGraphBuilder::RegisterGraphWithFrontend(*NewPatch);
		return NewPatch;
	}

	return nullptr;
}

UMetaSoundSourceFactory::UMetaSoundSourceFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UMetaSoundSource::StaticClass();
}

UObject* UMetaSoundSourceFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InFeedbackContext)
{
	using namespace Metasound::Editor;
	using namespace Metasound::FactoryPrivate;

	if (UMetaSoundSource* NewSource = CreateNewMetaSoundObject<UMetaSoundSource>(InParent, InName, InFlags, ReferencedMetaSoundObject))
	{
		// Copy over referenced fields that are specific to sources
		if (UMetaSoundSource* ReferencedMetaSound = Cast<UMetaSoundSource>(ReferencedMetaSoundObject))
		{
			NewSource->OutputFormat = ReferencedMetaSound->OutputFormat;
		}

		FGraphBuilder::RegisterGraphWithFrontend(*NewSource);
		return NewSource;
	}

	return nullptr;
}

