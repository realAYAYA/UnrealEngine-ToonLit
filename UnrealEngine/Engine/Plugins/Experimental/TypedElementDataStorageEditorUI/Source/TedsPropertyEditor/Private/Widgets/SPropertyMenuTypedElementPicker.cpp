// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SPropertyMenuTypedElementPicker.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "TypedElementPickingMode.h"
#include "TypedElementOutlinerItem.h"

#define LOCTEXT_NAMESPACE "TedsPropertyEditor"
void SPropertyMenuTypedElementPicker::Construct(const FArguments& InArgs)
{
	bAllowClear = InArgs._AllowClear;
	TypedElementQueryFilter = InArgs._TypedElementQueryFilter;
	ElementFilter = InArgs._ElementFilter;
	OnSet = InArgs._OnSet;

	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("CurrentTypedElementOperationsHeader", "Current Element"));
	{
		if (bAllowClear)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("ClearElement", "Clear"),
				LOCTEXT("ClearElement_Tooltip", "Clears the item set on this field"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SPropertyMenuTypedElementPicker::OnClear))
			);
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("BrowseHeader", "Browse"));
	{
		TSharedPtr<SWidget> MenuContent;
		{
			// TEDS-Outliner TODO: Taken from private implementation of PropertyEditorAssetConstants.
			//                     Should be centralized when TEDS is moved to core
			static const FVector2D ContentBrowserWindowSize(300.0f, 300.0f);
			static const FVector2D SceneOutlinerWindowSize(350.0f, 300.0f);

			UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();
			checkf(Registry, TEXT("Unable to initialize the Typed Elements Outliner before TEDS is initialized."));

			if (!Registry->AreDataStorageInterfacesSet())
			{
				MenuContent = SNew(STextBlock)
					.Text(LOCTEXT("TEDSPluginNotEnabledText", "Typed Element Data Storage plugin required to use this property picker."));
			}
			else
			{
				auto OnItemPicked = FOnSceneOutlinerItemPicked::CreateLambda([&](TSharedRef<ISceneOutlinerTreeItem> Item)
					{
						if (FTypedElementOutlinerTreeItem* ElementItem = Item->CastTo<FTypedElementOutlinerTreeItem>())
						{
							if (ElementItem->IsValid())
							{
								OnSet.ExecuteIfBound(ElementItem->GetRowHandle());
							}
						}
					});

				FSceneOutlinerInitializationOptions InitOptions;
				InitOptions.bShowHeaderRow = true;
				InitOptions.bShowTransient = true;
				InitOptions.bShowSearchBox = false; // Search not currently supported in TEDS Outliner

				InitOptions.ModeFactory = FCreateSceneOutlinerMode::CreateLambda([&](SSceneOutliner* Outliner)
					{
						FTypedElementOutlinerModeParams Params(Outliner);
						Params.QueryDescription = TypedElementQueryFilter;
						return new FTypedElementPickingMode(Params, OnItemPicked);
					});

				TSharedPtr<SSceneOutliner> Outliner = SNew(SSceneOutliner, InitOptions);

				Outliner->AddFilter(
					MakeShared<TSceneOutlinerPredicateFilter<FTypedElementOutlinerTreeItem>>(
						FTypedElementOutlinerTreeItem::FFilterPredicate::CreateLambda([this](const TypedElementDataStorage::RowHandle RowHandle) -> bool
					{
						if (ElementFilter.IsBound())
						{
							return ElementFilter.Execute(RowHandle);
						}
						return true;
					}), FSceneOutlinerFilter::EDefaultBehaviour::Pass));

				MenuContent =
					SNew(SBox)
					.WidthOverride(static_cast<float>(SceneOutlinerWindowSize.X))
					.HeightOverride(static_cast<float>(SceneOutlinerWindowSize.Y))
					[
						Outliner.ToSharedRef()
					];
			}
		}

		MenuBuilder.AddWidget(MenuContent.ToSharedRef(), FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	ChildSlot
	[
		MenuBuilder.MakeWidget()
	];
}

void SPropertyMenuTypedElementPicker::OnClear()
{
	SetValue(TypedElementInvalidRowHandle);
	OnClose.ExecuteIfBound();
}

void SPropertyMenuTypedElementPicker::OnElementSelected(TypedElementDataStorage::RowHandle RowHandle)
{
	SetValue(RowHandle);
	OnClose.ExecuteIfBound();
}

void SPropertyMenuTypedElementPicker::SetValue(TypedElementDataStorage::RowHandle RowHandle)
{
	OnSet.ExecuteIfBound(RowHandle);
}

#undef LOCTEXT_NAMESPACE // "TedsPropertyEditor"
