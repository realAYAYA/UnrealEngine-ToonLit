// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorVerticalLayout.h"

#include "Algo/Find.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Editor.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "Views/SDMXControlConsoleEditorFaderGroupControllerView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorVerticalLayout"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorVerticalLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel && InLayout, TEXT("Invalid control console editor model, can't create layout view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		EditorLayout = InLayout;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorVerticalLayout::Refresh);
		EditorModel->GetOnScrollFaderGroupControllerIntoView().AddSP(this, &SDMXControlConsoleEditorVerticalLayout::OnScrollIntoView);

		const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Vertical);

		const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Horizontal);

		ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SNew(SScrollBox)
						.ExternalScrollbar(HorizontalScrollBar)
						.Orientation(Orient_Horizontal)

						+ SScrollBox::Slot()
						[
							SAssignNew(VerticalScrollBox, SScrollBox)
							.ExternalScrollbar(VerticalScrollBar)
							.Orientation(Orient_Vertical)

							+ SScrollBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Top)
							.AutoSize()
							[
								SAssignNew(FaderGroupControllersVerticalBox, SVerticalBox)
							]
						]
					]

					// Horizontal ScrollBar slot
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						VerticalScrollBar
					]
				]

				// Vertical Scrollbar slot
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					HorizontalScrollBar
				]
			];
	}

	bool SDMXControlConsoleEditorVerticalLayout::CanRefresh() const
	{
		if (!EditorModel.IsValid() || !EditorLayout.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return false;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		if (AllFaderGroupControllers.Num() == FaderGroupControllerViews.Num())
		{
			return false;
		}

		return true;
	}

	void SDMXControlConsoleEditorVerticalLayout::OnLayoutElementAdded()
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ensureMsgf(ActiveLayout, TEXT("Invalid active layout, can't add new element to layout correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (UDMXControlConsoleFaderGroupController* FaderGroupController : AllFaderGroupControllers)
		{
			if (!FaderGroupController)
			{
				continue;
			}

			if (IsFaderGroupControllerContained(FaderGroupController))
			{
				continue;
			}

			const TSharedRef<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = MakeShared<FDMXControlConsoleFaderGroupControllerModel>(FaderGroupController, EditorModel);
			const TSharedRef<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerWidget =
				SNew(SDMXControlConsoleEditorFaderGroupControllerView, FaderGroupControllerModel, EditorModel.Get());

			const int32 Index = AllFaderGroupControllers.IndexOfByKey(FaderGroupController);
			FaderGroupControllerViews.Insert(FaderGroupControllerWidget, Index);

			FaderGroupControllersVerticalBox->InsertSlot(Index)
				.AutoHeight()
				.VAlign(VAlign_Top)
				.Padding(0.f, 8.f)
				[
					SNew(SHorizontalBox)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorVerticalLayout::GetFaderGroupControllerViewVisibility, FaderGroupController))
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(8.f, 0.f)
					[
						FaderGroupControllerWidget
					]
				];
		}
	}

	void SDMXControlConsoleEditorVerticalLayout::OnLayoutElementRemoved()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ensureMsgf(ActiveLayout, TEXT("Invalid active layout, can't remove element from the layout correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> AllFaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();

		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>>FaderGroupControllerViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerView : FaderGroupControllerViews)
		{
			if (!FaderGroupControllerView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerView.Pin()->GetFaderGroupController();
			if (!FaderGroupController || !AllFaderGroupControllers.Contains(FaderGroupController))
			{
				const TSharedPtr<SWidget> ParentWidget = FaderGroupControllerView.Pin()->GetParentWidget();
				if (ParentWidget.IsValid())
				{
					FaderGroupControllersVerticalBox->RemoveSlot(ParentWidget.ToSharedRef());
					FaderGroupControllerViewsToRemove.Add(FaderGroupControllerView);
				}
			}
		}

		FaderGroupControllerViews.RemoveAll([&FaderGroupControllerViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerView)
			{
				return !FaderGroupControllerView.IsValid() || FaderGroupControllerViewsToRemove.Contains(FaderGroupControllerView);
			});
	}

	bool SDMXControlConsoleEditorVerticalLayout::IsFaderGroupControllerContained(UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!FaderGroupController)
		{
			return false;
		}

		const bool bContainsFaderGroupController = Algo::FindByPredicate(FaderGroupControllerViews,
			[FaderGroupController](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerViewWeakPtr)
			{
				if (const TSharedPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView = FaderGroupControllerViewWeakPtr.Pin())
				{
					const UDMXControlConsoleFaderGroupController* OtherFaderGroupController = FaderGroupControllerView->GetFaderGroupController();
					return FaderGroupController == OtherFaderGroupController;
				}

				return false;
			}) != nullptr;

		return bContainsFaderGroupController;
	}

	void SDMXControlConsoleEditorVerticalLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!FaderGroupController || !VerticalScrollBox.IsValid())
		{
			return;
		}

		const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>* FaderGroupControllerViewPtr =
			Algo::FindByPredicate(FaderGroupControllerViews, 
				[FaderGroupController](TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerViewWeakPtr)
				{
					const TSharedPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView = FaderGroupControllerViewWeakPtr.Pin();
					return FaderGroupControllerView.IsValid() && FaderGroupControllerView->GetFaderGroupController() == FaderGroupController;
				});

		if (FaderGroupControllerViewPtr && FaderGroupControllerViewPtr->IsValid())
		{
			VerticalScrollBox->ScrollDescendantIntoView(FaderGroupControllerViewPtr->Pin(), true, EDescendantScrollDestination::Center);
		}
	}

	EVisibility SDMXControlConsoleEditorVerticalLayout::GetFaderGroupControllerViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroupController> FaderGroupController) const
	{
		if (!FaderGroupController.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const bool bIsVisible = FaderGroupController->IsActive() && FaderGroupController->IsMatchingFilter();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE