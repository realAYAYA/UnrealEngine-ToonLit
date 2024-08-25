// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDocumentSummoner.h"

#include "AnimNextWorkspaceEditor.h"
#include "AssetDefinitionRegistry.h"
#include "ClassIconFinder.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SSpacer.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDocumentSummoner"

namespace UE::AnimNext::Editor
{

FAssetDocumentSummoner::FAssetDocumentSummoner(FName InIdentifier, TSharedPtr<FWorkspaceEditor> InHostingApp)
	: FDocumentTabFactoryForObjects<UObject>(InIdentifier, InHostingApp)
	, HostingAppPtr(InHostingApp)
{
}

void FAssetDocumentSummoner::SetAllowedAssetClassPaths(TConstArrayView<FTopLevelAssetPath> InAllowedAssetClassPaths)
{
	AllowedAssetClassPaths = InAllowedAssetClassPaths;
}

void FAssetDocumentSummoner::OnTabActivated(TSharedPtr<SDockTab> Tab) const
{
}

void FAssetDocumentSummoner::OnTabBackgrounded(TSharedPtr<SDockTab> Tab) const
{
}

void FAssetDocumentSummoner::OnTabRefreshed(TSharedPtr<SDockTab> Tab) const
{
}

void FAssetDocumentSummoner::SaveState(TSharedPtr<SDockTab> Tab, TSharedPtr<FTabPayload> Payload) const
{
	UObject* Object = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Payload) : nullptr;
	OnSaveDocumentStateDelegate.ExecuteIfBound(Object);
}

TAttribute<FText> FAssetDocumentSummoner::ConstructTabNameForObject(UObject* DocumentID) const
{
	if (DocumentID)
	{
		return MakeAttributeLambda([WeakObject = TWeakObjectPtr<UObject>(DocumentID)]()
		{
			if(UObject* Object = WeakObject.Get())
			{
				return FText::FromName(Object->GetFName());
			}

			return FText::GetEmpty();
		});
	}

	return LOCTEXT("UnknownGraphName", "Unknown");
}

bool FAssetDocumentSummoner::IsPayloadSupported(TSharedRef<FTabPayload> Payload) const
{
	UObject* Object = Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Payload) : nullptr;
	if(Object && Object->IsAsset())
	{
		if(FWorkspaceEditor::FAssetDocumentWidgetFactoryFunc* FactoryFunc = FWorkspaceEditor::AssetDocumentWidgetFactories.Find(Object->GetClass()->GetFName()))
		{
			return AllowedAssetClassPaths.IsEmpty() || AllowedAssetClassPaths.Contains(Object->GetClass()->GetClassPathName());
		}
	}

	return false;
}

TAttribute<FText> FAssetDocumentSummoner::ConstructTabLabelSuffix(const FWorkflowTabSpawnInfo& Info) const
{
	UObject* Object = Info.Payload->IsValid() ? FTabPayload_UObject::CastChecked<UObject>(Info.Payload) : nullptr;
	if(Object)
	{
		return MakeAttributeLambda([WeakObject = TWeakObjectPtr<UObject>(Object)]()
		{
			if(UObject* Object = WeakObject.Get())
			{
				if(Object->GetPackage()->IsDirty())
				{
					return LOCTEXT("TabSuffixAsterisk", "*");
				}
			}

			return FText::GetEmpty();
		});
	}

	return FText::GetEmpty();
}

TSharedRef<SWidget> FAssetDocumentSummoner::CreateTabBodyForObject(const FWorkflowTabSpawnInfo& Info, UObject* DocumentID) const
{
	if(TSharedPtr<FWorkspaceEditor> WorkspaceEditor = HostingAppPtr.Pin())
	{
		if(FWorkspaceEditor::FAssetDocumentWidgetFactoryFunc* FactoryFunc = FWorkspaceEditor::AssetDocumentWidgetFactories.Find(DocumentID->GetClass()->GetFName()))
		{
			return (*FactoryFunc)(WorkspaceEditor.ToSharedRef(), DocumentID);
		}
	}

	return SNew(SSpacer);
}

const FSlateBrush* FAssetDocumentSummoner::GetTabIconForObject(const FWorkflowTabSpawnInfo& Info, UObject* DocumentID) const
{
	if (UAssetDefinitionRegistry* AssetDefinitionRegistry = UAssetDefinitionRegistry::Get())
	{
		const FAssetData AssetData(DocumentID);
		if(const UAssetDefinition* AssetDefinition = AssetDefinitionRegistry->GetAssetDefinitionForAsset(AssetData))
		{
			const FSlateBrush* ThumbnailBrush = AssetDefinition->GetThumbnailBrush(AssetData, AssetData.AssetClassPath.GetAssetName());
			if(ThumbnailBrush == nullptr)
			{
				return FClassIconFinder::FindThumbnailForClass(DocumentID->GetClass(), NAME_None);
			}
			return ThumbnailBrush;
		}
	}
	return nullptr;
}

}

#undef LOCTEXT_NAMESPACE