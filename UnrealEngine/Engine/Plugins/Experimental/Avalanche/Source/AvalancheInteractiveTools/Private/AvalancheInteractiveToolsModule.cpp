// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheInteractiveToolsModule.h"
#include "AvaInteractiveToolsCommands.h"
#include "AvaInteractiveToolsDelegates.h"
#include "AvaInteractiveToolsSettings.h"
#include "Builders/AvaInteractiveToolsActorToolBuilder.h"
#include "Interfaces/IPluginManager.h"
#include "IPlacementModeModule.h"
#include "Styling/SlateIconFinder.h"
#include "Tools/AvaInteractiveToolsActorToolNull.h"
#include "Tools/AvaInteractiveToolsActorToolSpline.h"

#define LOCTEXT_NAMESPACE "AvalancheInteractiveTools"

DEFINE_LOG_CATEGORY(LogAvaInteractiveTools);

namespace UE::AvaInteractiveTools::Private
{
	bool bInitialRegistration = false;
}

void FAvalancheInteractiveToolsModule::StartupModule()
{
	FAvaInteractiveToolsCommands::Register();
	bHasActiveTool = false;

	if (IPluginManager::Get().GetLastCompletedLoadingPhase() >= ELoadingPhase::PostEngineInit)
	{
		OnPostEngineInit();
	}
	else
	{
		FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAvalancheInteractiveToolsModule::OnPostEngineInit);
	}
}

void FAvalancheInteractiveToolsModule::ShutdownModule()
{
	FAvaInteractiveToolsCommands::Unregister();
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

void FAvalancheInteractiveToolsModule::RegisterCategory(FName InCategoryName, TSharedPtr<FUICommandInfo> InCategoryCommand,
	int32 InPlacementModeSortPriority)
{
	if (!InCategoryCommand.IsValid())
	{
		return;
	}

	if (Categories.Contains(InCategoryName))
	{
		return;
	}

	Categories.Add(InCategoryName, InCategoryCommand);
	Tools.Add(InCategoryName, {});

	if (InPlacementModeSortPriority != NoPlacementCategory)
	{
		IPlacementModeModule& PlacementMode = IPlacementModeModule::Get();

		if (!PlacementMode.GetRegisteredPlacementCategory(InCategoryName))
		{
			static const FText LabelFormat = LOCTEXT("LabelFormat", "Motion Design {0}");

			const FPlacementCategoryInfo PlacementCategory = FPlacementCategoryInfo(
				FText::Format(LabelFormat, InCategoryCommand->GetLabel()),
				InCategoryCommand->GetIcon(),
				InCategoryName,
				InCategoryCommand->GetCommandName().ToString(),
				InPlacementModeSortPriority
			);

			PlacementMode.RegisterPlacementCategory(PlacementCategory);
		}
	}
}

void FAvalancheInteractiveToolsModule::RegisterTool(FName InCategory, FAvaInteractiveToolsToolParameters&& InToolParams)
{
	if (!Categories.Contains(InCategory))
	{
		return;
	}

	if (!Tools.Contains(InCategory))
	{
		Tools.Add(InCategory, {});
	}

	const bool bToolAdded = Tools[InCategory].ContainsByPredicate(
		[&InToolParams](const FAvaInteractiveToolsToolParameters& InTool)
		{
			return InTool.ToolIdentifier.Equals(InToolParams.ToolIdentifier);
		}
	);

	if (bToolAdded)
	{
		return;
	}

	if (InToolParams.Factory)
	{
		// Hotfix-version of the GC fix. Not permanent.
		InToolParams.Factory->AddToRoot();
	}

	Tools[InCategory].Add(MoveTemp(InToolParams));

	using namespace UE::AvaInteractiveTools::Private;

	if (!bInitialRegistration)
	{
		Tools[InCategory].StableSort([](const FAvaInteractiveToolsToolParameters& InA, const FAvaInteractiveToolsToolParameters& InB)
			{
				return InA.Priority < InB.Priority;
			});

		IPlacementModeModule::Get().RegenerateItemsForCategory(InCategory);
	}
}

const TMap<FName, TSharedPtr<FUICommandInfo>>& FAvalancheInteractiveToolsModule::GetCategories()
{
	return Categories;
}

const TArray<FAvaInteractiveToolsToolParameters>* FAvalancheInteractiveToolsModule::GetTools(FName InCategory)
{
	return Tools.Find(InCategory);
}

bool FAvalancheInteractiveToolsModule::HasActiveTool() const
{
	return bHasActiveTool;
}

void FAvalancheInteractiveToolsModule::OnToolActivated()
{
	bHasActiveTool = true;
}

void FAvalancheInteractiveToolsModule::OnToolDeactivated()
{
	bHasActiveTool = false;
}

void FAvalancheInteractiveToolsModule::OnPostEngineInit()
{
	using namespace UE::AvaInteractiveTools::Private;

	IPlacementModeModule& PlacementMode = IPlacementModeModule::Get();

	bInitialRegistration = true;
	BroadcastRegisterCategories();
	BroadcastRegisterTools();
	bInitialRegistration = false;

	for (TPair<FName, TArray<FAvaInteractiveToolsToolParameters>>& ToolPair : Tools)
	{
		ToolPair.Value.StableSort([](const FAvaInteractiveToolsToolParameters& InA, const FAvaInteractiveToolsToolParameters& InB)
			{
				return InA.Priority < InB.Priority;
			});
	}

	PlacementMode.OnPlacementModeCategoryRefreshed().AddRaw(
		this,
		&FAvalancheInteractiveToolsModule::OnPlacementCategoryRefreshed
	);

	for (const TPair<FName, TArray<FAvaInteractiveToolsToolParameters>>& Pair : Tools)
	{
		PlacementMode.RegenerateItemsForCategory(Pair.Key);
	}
}

void FAvalancheInteractiveToolsModule::BroadcastRegisterCategories()
{
	// Ensure that ours are first
	RegisterDefaultCategories();
	FAvaInteractiveToolsDelegates::GetRegisterCategoriesDelegate().Broadcast(this);
}

void FAvalancheInteractiveToolsModule::RegisterDefaultCategories()
{
	RegisterCategory(CategoryName2D, FAvaInteractiveToolsCommands::Get().Category_2D, 41);
	RegisterCategory(CategoryName3D, FAvaInteractiveToolsCommands::Get().Category_3D, 42);
	RegisterCategory(CategoryNameActor, FAvaInteractiveToolsCommands::Get().Category_Actor, 43);
}

void FAvalancheInteractiveToolsModule::BroadcastRegisterTools()
{
	RegisterDefaultTools();
	FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate().Broadcast(this);
}

void FAvalancheInteractiveToolsModule::RegisterDefaultTools()
{
	RegisterTool(CategoryNameActor, GetDefault<UAvaInteractiveToolsActorToolNull>()->GetToolParameters());
	RegisterTool(CategoryNameActor, GetDefault<UAvaInteractiveToolsActorToolSpline>()->GetToolParameters());
}

void FAvalancheInteractiveToolsModule::OnPlacementCategoryRefreshed(FName InCategory)
{
	if (!Categories.Contains(InCategory))
	{
		return;
	}

	if (!Tools.Contains(InCategory))
	{
		return;
	}

	IPlacementModeModule& PlacementMode = IPlacementModeModule::Get();

	TArray<TSharedPtr<FPlaceableItem>> Items;
	PlacementMode.GetItemsForCategory(InCategory, Items);

	for (const FAvaInteractiveToolsToolParameters& Tool : Tools[InCategory])
	{
		if (!(Tool.Factory && Tool.Factory->NewActorClass) && !Tool.FactoryClass)
		{
			continue;
		}

		const bool bAlreadyRegistered = Items.ContainsByPredicate(
			[&Tool](const TSharedPtr<FPlaceableItem>& InItem)
			{
				return InItem.IsValid() && InItem->NativeName.Equals(Tool.ToolIdentifier);
			}
		);

		if (bAlreadyRegistered)
		{
			continue;
		}

		TSharedPtr<FPlaceableItem> PlaceableItem;

		if (Tool.Factory)
		{
			PlaceableItem = MakeShared<FPlaceableItem>(
				Tool.Factory,
				FAssetData(Tool.Factory->NewActorClass->GetDefaultObject()),
				Tool.Priority
			);
		}
		else
		{
			PlaceableItem = MakeShared<FPlaceableItem>(
				*Tool.FactoryClass.Get(),
				FAssetData(Tool.FactoryClass.Get()),
				NAME_None,
				NAME_None,
				TOptional<FLinearColor>(),
				Tool.Priority
			);
		}

		PlaceableItem->DisplayName = Tool.UICommand->GetLabel();
		PlaceableItem->NativeName = Tool.ToolIdentifier;

		if (FSlateIconFinder::FindIcon(Tool.UICommand->GetCommandName()).IsSet())
		{
			PlaceableItem->ClassThumbnailBrushOverride = Tool.UICommand->GetCommandName();
			PlaceableItem->bAlwaysUseGenericThumbnail = false;
		}

		PlacementMode.RegisterPlaceableItem(InCategory, PlaceableItem.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAvalancheInteractiveToolsModule, AvalancheInteractiveTools)
