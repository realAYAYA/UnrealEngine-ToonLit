// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayoutView.h"

#include "Algo/AnyOf.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Layouts/Widgets/SDMXControlConsoleEditorGridLayout.h"
#include "Layouts/Widgets/SDMXControlConsoleEditorHorizontalLayout.h"
#include "Layouts/Widgets/SDMXControlConsoleEditorVerticalLayout.h"
#include "Misc/TransactionObjectEvent.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "TimerManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorView"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorLayoutView::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		checkf(InEditorModel, TEXT("Invalid control console editor model, can't constuct layout view correctly."));
		EditorModel = InEditorModel;

		if (UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts())
		{
			ControlConsoleLayouts->GetOnActiveLayoutChanged().AddSP(this, &SDMXControlConsoleEditorLayoutView::OnActiveLayoutChanged);
			ControlConsoleLayouts->GetOnLayoutModeChanged().AddSP(this, &SDMXControlConsoleEditorLayoutView::UpdateLayout);
		}

		ChildSlot
			[
				SNew(SBorder)
				.BorderImage(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.DefaultBrush"))
				.Padding(10.f)
				[
					SAssignNew(LayoutBox, SHorizontalBox)
				]
			];

		UpdateLayout();

		const TSharedRef<SWidget> SharedThis = AsShared();
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda([SharedThis]()
			{
				FSlateApplication::Get().SetKeyboardFocus(SharedThis);
			}));
	}

	void SDMXControlConsoleEditorLayoutView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		if (!ensureMsgf(Layout.IsValid(), TEXT("Invalid layout, can't update control console state correctly.")))
		{
			return;
		}

		Layout->RequestRefresh();
	}

	FReply SDMXControlConsoleEditorLayoutView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (EditorModel.IsValid())
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			SelectionHandler->ClearSelection();
		}

		return FReply::Handled();
	}

	bool SDMXControlConsoleEditorLayoutView::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		const TArray<UClass*> MatchingContextClasses =
		{
			UDMXControlConsoleEditorModel::StaticClass(),
			UDMXControlConsoleData::StaticClass(),
			UDMXControlConsoleEditorLayouts::StaticClass(),
			UDMXControlConsoleEditorGlobalLayoutBase::StaticClass(),
			UDMXControlConsoleEditorGlobalLayoutRow::StaticClass()
		};

		const bool bMatchesContext = Algo::AnyOf(TransactionObjectContexts, 
			[this, MatchingContextClasses](const TPair<UObject*, FTransactionObjectEvent>& Pair)
			{
				bool bMatchesClasses = false;
				const UObject* Object = Pair.Key;
				if (IsValid(Object))
				{
					const UClass* ObjectClass = Object->GetClass();
					bMatchesClasses = Algo::AnyOf(MatchingContextClasses, [ObjectClass](UClass* InClass)
						{
							return IsValid(ObjectClass) && ObjectClass->IsChildOf(InClass);
						});
				}

				return bMatchesClasses;
			});

		return bMatchesContext;
	}

	void SDMXControlConsoleEditorLayoutView::PostUndo(bool bSuccess)
	{
		UpdateLayout();
		if (EditorModel.IsValid())
		{
			EditorModel->RequestUpdateEditorModel();
		}
	}

	void SDMXControlConsoleEditorLayoutView::PostRedo(bool bSuccess)
	{
		UpdateLayout();
		if (EditorModel.IsValid())
		{
			EditorModel->RequestUpdateEditorModel();
		}
	}

	void SDMXControlConsoleEditorLayoutView::UpdateLayout()
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, can't update layout view correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!EditorConsoleLayouts || !LayoutBox.IsValid())
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const EDMXControlConsoleLayoutMode LayoutMode = ActiveLayout->GetLayoutMode();
		TSharedPtr<SDMXControlConsoleEditorLayout> NewLayoutWidget;
		switch (LayoutMode)
		{
		case EDMXControlConsoleLayoutMode::Horizontal:
		{
			NewLayoutWidget = SNew(SDMXControlConsoleEditorHorizontalLayout, ActiveLayout, EditorModel.Get());
			break;
		}
		case EDMXControlConsoleLayoutMode::Vertical:
		{
			NewLayoutWidget = SNew(SDMXControlConsoleEditorVerticalLayout, ActiveLayout, EditorModel.Get());
			break;
		}
		case EDMXControlConsoleLayoutMode::Grid:
		{
			NewLayoutWidget = SNew(SDMXControlConsoleEditorGridLayout, ActiveLayout, EditorModel.Get());
			break;
		}
		default:
			NewLayoutWidget = SNew(SDMXControlConsoleEditorGridLayout, ActiveLayout, EditorModel.Get());
			break;
		}

		if (!ensureMsgf(NewLayoutWidget.IsValid(), TEXT("Invalid layout widget, cannot update layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = Layout.IsValid() ? Layout->GetEditorLayout() : nullptr;
		if (ActiveLayout != CurrentLayout || !IsCurrentLayoutWidgetType(NewLayoutWidget->GetType()))
		{
			Layout = NewLayoutWidget;
			LayoutBox->ClearChildren();
			LayoutBox->AddSlot()
				[
					Layout.ToSharedRef()
				];
		}
	}

	bool SDMXControlConsoleEditorLayoutView::IsCurrentLayoutWidgetType(const FName& InWidgetTypeName) const
	{
		if (Layout.IsValid())
		{
			const FName& WidgetTypeName = Layout->GetType();
			return WidgetTypeName == InWidgetTypeName;
		}

		return false;
	}

	void SDMXControlConsoleEditorLayoutView::OnActiveLayoutChanged(const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout)
	{
		if (ActiveLayout)
		{
			UpdateLayout();
		}
	}
}

#undef LOCTEXT_NAMESPACE
