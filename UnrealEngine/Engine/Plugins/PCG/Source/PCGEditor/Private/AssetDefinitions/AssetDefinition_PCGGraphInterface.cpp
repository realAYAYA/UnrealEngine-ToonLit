// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_PCGGraphInterface.h"

#include "PCGGraph.h"

#include "PCGEditorStyle.h"
#include "PCGGraphFactory.h"

#include "ContentBrowserMenuContexts.h"
#include "IAssetTools.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"
#include "Misc/AssetCategoryPath.h"
#include "Misc/DelayedAutoRegister.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_PCGGraphInterface"

FText UAssetDefinition_PCGGraphInterface::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "PCG Graph Interface");
}

FLinearColor UAssetDefinition_PCGGraphInterface::GetAssetColor() const
{
	return FColor::Turquoise;
}

TSoftClassPtr<UObject> UAssetDefinition_PCGGraphInterface::GetAssetClass() const
{
	return UPCGGraphInterface::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_PCGGraphInterface::GetAssetCategories() const
{
	static const FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("PCGCategory", "PCG")) };
	return Categories;
}

// Menu Extensions
//--------------------------------------------------------------------

namespace MenuExtension_PCGGraphInterface
{
	static void ExecuteNewPCGGraphInstance(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);

		IAssetTools::Get().CreateAssetsFrom<UPCGGraphInterface>(
			CBContext->LoadSelectedObjects<UPCGGraphInterface>(), UPCGGraphInstance::StaticClass(), TEXT("_Inst"), [](UPCGGraphInterface* SourceObject)
			{
				UPCGGraphInstanceFactory* Factory = NewObject<UPCGGraphInstanceFactory>();
				Factory->ParentGraph = SourceObject;
				return Factory;
			}
		);
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UPCGGraphInterface::StaticClass());

			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				{
					const TAttribute<FText> Label = LOCTEXT("PCGGraph_NewInstance", "Create PCG Graph Instance");
					const TAttribute<FText> ToolTip = LOCTEXT("PCGGraph_NewInstanceToolTip", "Creates a parameterized PCG graph using this graph as a base.");
					const FToolMenuExecuteAction UIAction = FToolMenuExecuteAction::CreateStatic(&ExecuteNewPCGGraphInstance);

					InSection.AddMenuEntry("PCGGraph_NewInstance", Label, ToolTip, FSlateIcon(FPCGEditorStyle::Get().GetStyleSetName(), "ClassIcon.PCGGraphInstance"), UIAction);
				}
			}));
		}));
	});
}

#undef LOCTEXT_NAMESPACE
