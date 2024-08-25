// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorHorizontalLayout.h"

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
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorHorizontalLayout"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorHorizontalLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel && InLayout, TEXT("Invalid control console editor model, can't create layout view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		EditorLayout = InLayout;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::Refresh);
		EditorModel->GetOnScrollFaderGroupControllerIntoView().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView);

		ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(HorizontalScrollBox, SScrollBox)
						.Orientation(Orient_Horizontal)

						+ SScrollBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoSize()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							.Padding(0.f, 8.f)
							[
								SAssignNew(FaderGroupControllersHorizontalBox, SHorizontalBox)
							]
						]
					]
				]
			];
	}

	bool SDMXControlConsoleEditorHorizontalLayout::CanRefresh() const
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

	void SDMXControlConsoleEditorHorizontalLayout::OnLayoutElementAdded()
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
				SNew(SDMXControlConsoleEditorFaderGroupControllerView, FaderGroupControllerModel, EditorModel.Get())
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupControllerViewVisibility, FaderGroupController));

			const int32 Index = AllFaderGroupControllers.IndexOfByKey(FaderGroupController);
			FaderGroupControllerViews.Insert(FaderGroupControllerWidget, Index);

			FaderGroupControllersHorizontalBox->InsertSlot(Index)
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(8.f, 0.f)
				[
					FaderGroupControllerWidget
				];
		}
	}

	void SDMXControlConsoleEditorHorizontalLayout::OnLayoutElementRemoved()
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
				FaderGroupControllersHorizontalBox->RemoveSlot(FaderGroupControllerView.Pin().ToSharedRef());
				FaderGroupControllerViewsToRemove.Add(FaderGroupControllerView);
			}
		}

		FaderGroupControllerViews.RemoveAll([&FaderGroupControllerViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerView)
			{
				return !FaderGroupControllerView.IsValid() || FaderGroupControllerViewsToRemove.Contains(FaderGroupControllerView);
			});
	}

	bool SDMXControlConsoleEditorHorizontalLayout::IsFaderGroupControllerContained(UDMXControlConsoleFaderGroupController* FaderGroupController)
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

	void SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!FaderGroupController || !HorizontalScrollBox.IsValid())
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
			HorizontalScrollBox->ScrollDescendantIntoView(FaderGroupControllerViewPtr->Pin(), true, EDescendantScrollDestination::TopOrLeft);
		}
	}

	EVisibility SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupControllerViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroupController> FaderGroupController) const
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