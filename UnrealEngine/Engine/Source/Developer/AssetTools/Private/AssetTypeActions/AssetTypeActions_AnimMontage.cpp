// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_AnimMontage.h"
#include "Factories/AnimMontageFactory.h"
#include "AnimationEditorUtils.h"
#include "ToolMenus.h"
#include "AssetTools.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"


void FAssetTypeActions_AnimMontage::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto Montages = GetTypedWeakObjectPtrs<UAnimMontage>(InObjects);

	// only show child montage if inobjects are not child montage already
	bool bContainsChildMontage = false;
	for (UObject* Object : InObjects)
	{
		bContainsChildMontage |= CastChecked<UAnimationAsset>(Object)->HasParentAsset();
		if (bContainsChildMontage)
		{
			break;
		}
	}

	// if no child montage is found
	if (!bContainsChildMontage)
	{
		// create mew child anim montage
		Section.AddMenuEntry(
			"AnimMontage_CreateChildMontage",
			LOCTEXT("AnimMontage_CreateChildMontage", "Create Child Montage"),
			LOCTEXT("AnimMontage_CreateChildMontageTooltip", "Create Child Animation Montage and remap to another animation assets."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.AnimMontage"),
			FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_AnimMontage::CreateChildAnimMontage, Montages))
		);
	}

	FAssetTypeActions_AnimationAsset::GetActions(InObjects, Section);
}

void FAssetTypeActions_AnimMontage::CreateChildAnimMontage(TArray<TWeakObjectPtr<UAnimMontage>> AnimMontages) 
{
	if (AnimMontages.Num() > 0)
	{
		const FString DefaultSuffix = TEXT("_Montage");
		UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();

		TArray<UObject*> ObjectsToSync;
		// need to know source and target
		for (int32 MontageIndex = 0; MontageIndex < AnimMontages.Num(); ++MontageIndex)
		{
			UAnimMontage* ParentMontage = AnimMontages[MontageIndex].Get();
			check(ParentMontage);

			UAnimMontage* NewAsset = AnimationEditorUtils::CreateAnimationAsset<UAnimMontage>(ParentMontage->GetSkeleton(), ParentMontage->GetOutermost()->GetName(), TEXT("_Child"));
			if (NewAsset)
			{
				NewAsset->SetParentAsset(ParentMontage);
				ObjectsToSync.Add(NewAsset);
			}
		}

		FAssetTools::Get().SyncBrowserToAssets(ObjectsToSync);
	}
}

#undef LOCTEXT_NAMESPACE
