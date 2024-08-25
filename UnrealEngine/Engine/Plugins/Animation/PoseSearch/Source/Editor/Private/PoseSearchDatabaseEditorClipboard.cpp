// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseEditorClipboard.h"

#include "Factories.h"
#include "InstancedStruct.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "PoseSearch/PoseSearchMultiSequence.h"
#include "PoseSearch/PoseSearchDatabase.h"

#include "UnrealExporter.h"
#include "Exporters/Exporter.h"
#include "HAL/PlatformApplicationMisc.h"

void UPoseSearchDatabaseEditorClipboardContent::CopyDatabaseItem(const FPoseSearchDatabaseAnimationAssetBase* InItem)
{
	check(InItem != nullptr)
	
	UPoseSearchDatabaseItemCopyObject* CopyObj = NewObject<UPoseSearchDatabaseItemCopyObject>((UObject*)GetTransientPackage(), UPoseSearchDatabaseItemCopyObject::StaticClass(), NAME_None, RF_Transient);
	
	CopyObj->ClassName = InItem->GetAnimationAssetStaticClass()->GetFName();
	
	if (UAnimSequence::StaticClass() == InItem->GetAnimationAssetStaticClass())
	{
		FPoseSearchDatabaseSequence::StaticStruct()->ExportText(CopyObj->Content, InItem, nullptr, nullptr, PPF_None, nullptr);
	}
	else if (UAnimComposite::StaticClass() == InItem->GetAnimationAssetStaticClass())
	{
		FPoseSearchDatabaseAnimComposite::StaticStruct()->ExportText(CopyObj->Content, InItem, nullptr, nullptr, PPF_None, nullptr);
	}
	else if (UAnimMontage::StaticClass() == InItem->GetAnimationAssetStaticClass())
	{
		FPoseSearchDatabaseAnimMontage::StaticStruct()->ExportText(CopyObj->Content, InItem, nullptr, nullptr, PPF_None, nullptr);
	}
	else if (UBlendSpace::StaticClass() == InItem->GetAnimationAssetStaticClass())
	{
		FPoseSearchDatabaseBlendSpace::StaticStruct()->ExportText(CopyObj->Content, InItem, nullptr, nullptr, PPF_None, nullptr);
	}
	else if (UPoseSearchMultiSequence::StaticClass() == InItem->GetAnimationAssetStaticClass())
	{
		FPoseSearchDatabaseMultiSequence::StaticStruct()->ExportText(CopyObj->Content, InItem, nullptr, nullptr, PPF_None, nullptr);
	}

	DatabaseItems.Add(CopyObj);
}

void UPoseSearchDatabaseEditorClipboardContent::CopyToClipboard()
{
	// Clear the mark state for saving.
	UnMarkAllObjects(EObjectMark(OBJECTMARK_TagExp | OBJECTMARK_TagImp));
	
	// Export the clipboard to text.
	FStringOutputDevice Archive;
	const FExportObjectInnerContext Context;
	UExporter::ExportToOutputDevice(&Context, this, nullptr, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, this->GetOuter());
	FPlatformApplicationMisc::ClipboardCopy(*Archive);
}

void UPoseSearchDatabaseEditorClipboardContent::PasteToDatabase(UPoseSearchDatabase* InTargetDatabase) const
{
	check(InTargetDatabase != nullptr)
	
	InTargetDatabase->Modify();
	
	for (TObjectPtr<UPoseSearchDatabaseItemCopyObject> Item : DatabaseItems)
	{
		if (Item->ClassName == UAnimSequence::StaticClass()->GetFName())
		{
			FPoseSearchDatabaseSequence NewAsset;
			FPoseSearchDatabaseSequence::StaticStruct()->ImportText(*Item->Content, &NewAsset, nullptr, PPF_None, GLog, FPoseSearchDatabaseSequence::StaticStruct()->GetName());

			InTargetDatabase->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
		}
		else if (Item->ClassName == UAnimComposite::StaticClass()->GetFName())
		{
			FPoseSearchDatabaseAnimComposite NewAsset;
			FPoseSearchDatabaseAnimComposite::StaticStruct()->ImportText(*Item->Content, &NewAsset, nullptr, PPF_None, GLog, FPoseSearchDatabaseAnimComposite::StaticStruct()->GetName());

			InTargetDatabase->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
		}
		else if (Item->ClassName == UBlendSpace::StaticClass()->GetFName())
		{
			FPoseSearchDatabaseBlendSpace NewAsset;
			FPoseSearchDatabaseBlendSpace::StaticStruct()->ImportText(*Item->Content, &NewAsset, nullptr, PPF_None, GLog, FPoseSearchDatabaseBlendSpace::StaticStruct()->GetName());

			InTargetDatabase->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
		}
		else if (Item->ClassName == UAnimMontage::StaticClass()->GetFName())
		{
			FPoseSearchDatabaseAnimMontage NewAsset;
			FPoseSearchDatabaseAnimMontage::StaticStruct()->ImportText(*Item->Content, &NewAsset, nullptr, PPF_None, GLog, FPoseSearchDatabaseAnimMontage::StaticStruct()->GetName());

			InTargetDatabase->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
		}
		else if (Item->ClassName == UPoseSearchMultiSequence::StaticClass()->GetFName())
		{
			FPoseSearchDatabaseMultiSequence NewAsset;
			FPoseSearchDatabaseMultiSequence::StaticStruct()->ImportText(*Item->Content, &NewAsset, nullptr, PPF_None, GLog, FPoseSearchDatabaseMultiSequence::StaticStruct()->GetName());
				
			InTargetDatabase->AddAnimationAsset(FInstancedStruct::Make(NewAsset));
		}
	}
}

class FPoseSearchDatabaseEditorClipboardContentTextFactory : public FCustomizableTextObjectFactory
{
public:
	
	FPoseSearchDatabaseEditorClipboardContentTextFactory()
		: FCustomizableTextObjectFactory(GWarn)
		, ClipboardContent(nullptr) 
	{
	}

	UPoseSearchDatabaseEditorClipboardContent* ClipboardContent;

protected:

	virtual bool CanCreateClass(UClass* InObjectClass, bool& bOmitSubObjs) const override
	{
		if (InObjectClass->IsChildOf(UPoseSearchDatabaseEditorClipboardContent::StaticClass()))
		{
			return true;
		}
		return false;
	}
	
	virtual void ProcessConstructedObject(UObject* CreatedObject) override
	{
		if (CreatedObject->IsA<UPoseSearchDatabaseEditorClipboardContent>())
		{
			ClipboardContent = CastChecked<UPoseSearchDatabaseEditorClipboardContent>(CreatedObject);
		}
	}
};

bool UPoseSearchDatabaseEditorClipboardContent::CanPasteContentFromClipboard(const FString& InTextToImport)
{
	const FPoseSearchDatabaseEditorClipboardContentTextFactory ClipboardContentFactory;
	return ClipboardContentFactory.CanCreateObjectsFromText(InTextToImport);
}

UPoseSearchDatabaseEditorClipboardContent* UPoseSearchDatabaseEditorClipboardContent::CreateFromClipboard()
{
	// Get the text from the clipboard.
	FString ClipboardText;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardText);

	// Try to create clipboard content from text.
	if (CanPasteContentFromClipboard(ClipboardText))
	{
		FPoseSearchDatabaseEditorClipboardContentTextFactory ClipboardContentFactory;
		ClipboardContentFactory.ProcessBuffer((UObject*)GetTransientPackage(), RF_Transactional, ClipboardText);
		return ClipboardContentFactory.ClipboardContent;
	}

	return nullptr;
}

UPoseSearchDatabaseEditorClipboardContent* UPoseSearchDatabaseEditorClipboardContent::Create()
{
	return NewObject<UPoseSearchDatabaseEditorClipboardContent>((UObject*)GetTransientPackage());
}
