// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayoutRowView.h"

#include "Algo/Find.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "Views/SDMXControlConsoleEditorFaderGroupControllerView.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorLayoutRowView"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorLayoutRowView::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutRow* InLayoutRow, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, cannot create layout row view correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InLayoutRow, TEXT("Invalid layout row, cannot create layout row view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		LayoutRow = InLayoutRow;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorLayoutRowView::Refresh);

		ChildSlot
			[
				SAssignNew(FaderGroupControllersHorizontalBox, SHorizontalBox)
			];
	}

	TSharedPtr<SDMXControlConsoleEditorFaderGroupControllerView> SDMXControlConsoleEditorLayoutRowView::FindFaderGroupControllerView(const UDMXControlConsoleFaderGroupController* FaderGroupController) const
	{
		if (FaderGroupController)
		{
			const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>* FaderGroupControllerViewPtr = Algo::FindByPredicate(FaderGroupControllerViews, [FaderGroupController](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& WeakFaderGroupControllerView)
				{
					return WeakFaderGroupControllerView.IsValid() && WeakFaderGroupControllerView.Pin()->GetFaderGroupController() == FaderGroupController;
				});

			if (FaderGroupControllerViewPtr)
			{
				return FaderGroupControllerViewPtr->Pin();
			}
		}

		return nullptr;
	}

	void SDMXControlConsoleEditorLayoutRowView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (!ensureMsgf(LayoutRow.IsValid(), TEXT("Invalid layout row, cannot update fader group row view state correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = LayoutRow->GetFaderGroupControllers();
		if (FaderGroupControllers.Num() == FaderGroupControllerViews.Num())
		{
			return;
		}

		if (FaderGroupControllers.Num() > FaderGroupControllerViews.Num())
		{
			OnFaderGroupControllerAdded();
		}
		else
		{
			OnFaderGroupControllerRemoved();
		}
	}

	void SDMXControlConsoleEditorLayoutRowView::Refresh()
	{
		if (FaderGroupControllersHorizontalBox.IsValid())
		{
			FaderGroupControllersHorizontalBox->ClearChildren();
			FaderGroupControllerViews.Reset(FaderGroupControllerViews.Num());
			OnFaderGroupControllerAdded();
			OnFaderGroupControllerRemoved();
		}
	}

	void SDMXControlConsoleEditorLayoutRowView::OnFaderGroupControllerAdded()
	{
		if (!LayoutRow.IsValid())
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = LayoutRow->GetFaderGroupControllers();
		for (UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
		{
			if (FaderGroupController && !ContainsFaderGroupController(FaderGroupController))
			{
				AddFaderGroupController(FaderGroupController);
			}
		}
	}

	void SDMXControlConsoleEditorLayoutRowView::AddFaderGroupController(UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, cannot add new fader group controller view correctly.")))
		{
			return;
		}

		if (!ensureMsgf(FaderGroupController, TEXT("Invalid fader group controller, cannot add new fader group controller view correctly.")))
		{
			return;
		}

		if (!LayoutRow.IsValid() || !FaderGroupControllersHorizontalBox.IsValid())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = MakeShared<FDMXControlConsoleFaderGroupControllerModel>(FaderGroupController, EditorModel);
		const TSharedRef<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerWidget =
			SNew(SDMXControlConsoleEditorFaderGroupControllerView, FaderGroupControllerModel, EditorModel.Get())
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorLayoutRowView::GetFaderGroupControllerViewVisibility, FaderGroupControllerModel));

		const int32 Index = LayoutRow->GetIndex(FaderGroupController);
		if (FaderGroupControllerViews.IsValidIndex(Index))
		{
			FaderGroupControllerViews.Insert(FaderGroupControllerWidget, Index);

			FaderGroupControllersHorizontalBox->InsertSlot(Index)
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(8.f, 0.f)
				[
					FaderGroupControllerWidget
				];
		}
		else
		{
			FaderGroupControllerViews.Add(FaderGroupControllerWidget);

			FaderGroupControllersHorizontalBox->AddSlot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(8.f, 0.f)
				[
					FaderGroupControllerWidget
				];
		}
	}

	void SDMXControlConsoleEditorLayoutRowView::OnFaderGroupControllerRemoved()
	{
		if (!LayoutRow.IsValid())
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = LayoutRow->GetFaderGroupControllers();

		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>> FaderGroupControllerViewsToRemove;
		for (const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView>& FaderGroupControllerView : FaderGroupControllerViews)
		{
			if (!FaderGroupControllerView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerView.Pin()->GetFaderGroupController();
			if (!FaderGroupController || !FaderGroupControllers.Contains(FaderGroupController))
			{
				FaderGroupControllersHorizontalBox->RemoveSlot(FaderGroupControllerView.Pin().ToSharedRef());
				FaderGroupControllerViewsToRemove.Add(FaderGroupControllerView);
			}
		}

		FaderGroupControllerViews.RemoveAll([&FaderGroupControllerViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView)
			{
				return !FaderGroupControllerView.IsValid() || FaderGroupControllerViewsToRemove.Contains(FaderGroupControllerView);
			});
	}

	bool SDMXControlConsoleEditorLayoutRowView::ContainsFaderGroupController(const UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!FaderGroupController)
		{
			return false;
		}

		const bool bContainsFaderGroupController = Algo::FindByPredicate(FaderGroupControllerViews,
			[FaderGroupController](const TWeakPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView)
			{
				if (FaderGroupControllerView.IsValid())
				{
					const UDMXControlConsoleFaderGroupController* OtherFaderGroupController = FaderGroupControllerView.Pin()->GetFaderGroupController();
					return OtherFaderGroupController == FaderGroupController;
				}

				return false;
			}) != nullptr;

		return bContainsFaderGroupController;
	}

	EVisibility SDMXControlConsoleEditorLayoutRowView::GetFaderGroupControllerViewVisibility(TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel) const
	{
		const UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
		const bool bIsVisible = 
			FaderGroupController && 
			FaderGroupController->IsActive() && 
			FaderGroupController->IsMatchingFilter();

		return  bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
