// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMPreviewSourcePanel.h"

#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

#include "Preview/PreviewMode.h"
#include "WidgetBlueprintEditor.h"

#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/PropertyViewer/SFieldIcon.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SMVVMDebugSourcePanel"


namespace UE::MVVM::Private
{

class SPreviewSourceView : public SListView<TSharedPtr<SPreviewSourceEntry>>
{
};

class SPreviewSourceEntry
{
public:
	SPreviewSourceEntry(UObject* InInstance, FName InName)
		: WeakInstance(InInstance)
		, Name(InName)
	{}
	
	UClass* GetClass() const
	{
		UObject* Instance = WeakInstance.Get();
		return Instance ? Instance->GetClass() : nullptr;
	}

	FText GetDisplayName() const
	{
		return FText::FromName(Name);
	}

	UObject* GetInstance() const
	{
		return WeakInstance.Get();
	}

private:
	TWeakObjectPtr<UObject> WeakInstance;
	FName Name;
};

} //namespace UE::MVVM::Private

namespace UE::MVVM
{

void SPreviewSourcePanel::Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> Editor)
{
	WeakEditor = Editor;
	check(Editor);

	if (TSharedPtr<UE::UMG::Editor::FPreviewMode> Context = Editor->GetPreviewMode())
	{
		HandlePreviewWidgetChanged();
		Context->OnPreviewWidgetChanged().AddSP(this, &SPreviewSourcePanel::HandlePreviewWidgetChanged);
		Context->OnSelectedObjectChanged().AddSP(this, &SPreviewSourcePanel::HandleSelectedObjectChanged);
	}

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(4.f))
		[
			SAssignNew(SourceListView, Private::SPreviewSourceView)
			.ListItemsSource(&SourceList)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SPreviewSourcePanel::GenerateWidget)
			.OnSelectionChanged(this, &SPreviewSourcePanel::HandleSourceSelectionChanged)
		]
	];
}


void SPreviewSourcePanel::HandlePreviewWidgetChanged()
{
	SourceList.Reset();

	if (TSharedPtr<FWidgetBlueprintEditor> Editor = WeakEditor.Pin())
	{
		if (UUserWidget* NewWidget = Editor->GetPreviewMode() ? Editor->GetPreviewMode()->GetPreviewWidget() : nullptr)
		{
			if (UMVVMView* View = UMVVMSubsystem::GetViewFromUserWidget(NewWidget))
			{
				for (const FMVVMViewSource& Source : View->GetSources())
				{
					SourceList.Emplace(MakeShared<Private::SPreviewSourceEntry>(Source.Source, Source.SourceName));
				}
			}
		}
	}

	if (SourceListView)
	{
		SourceListView->RequestListRefresh();
	}
}


void SPreviewSourcePanel::HandleSelectedObjectChanged()
{
	if (bInternalSelection)
	{
		return;
	}

	TGuardValue<bool> TmpInternalSelection(bInternalSelection, true);

	if (TSharedPtr<FWidgetBlueprintEditor> Editor = WeakEditor.Pin())
	{
		TArray<UObject*> NewSelections = Editor->GetPreviewMode() ? Editor->GetPreviewMode()->GetSelectedObjectList() : TArray<UObject*>();
		if (NewSelections.Num() == 1)
		{
			TSharedPtr<Private::SPreviewSourceEntry>* Found = SourceList.FindByPredicate([ToFind = NewSelections[0]](const TSharedPtr<Private::SPreviewSourceEntry>& Other){ return Other->GetInstance() == ToFind; });
			if (Found)
			{
				SourceListView->SetSelection(*Found);
			}
			else
			{
				SourceListView->SetSelection(TSharedPtr<Private::SPreviewSourceEntry>());
			}
		}
		else
		{
			SourceListView->SetSelection(TSharedPtr<Private::SPreviewSourceEntry>());
		}
	}
}


void SPreviewSourcePanel::HandleSourceSelectionChanged(TSharedPtr<Private::SPreviewSourceEntry> Entry, ESelectInfo::Type SelectionType) const
{
	if (bInternalSelection)
	{
		return;
	}

	TGuardValue<bool> TmpInternalSelection(bInternalSelection, true);

	if (TSharedPtr<FWidgetBlueprintEditor> Editor = WeakEditor.Pin())
	{
		if (Editor->GetPreviewMode())
		{
			TArray<TWeakObjectPtr<UObject>> Selection;
			if (Entry && Entry->GetInstance())
			{
				Selection.Add(Entry->GetInstance());
			}

			Editor->GetPreviewMode()->SetSelectedObject(Selection);
		}
	}
}


TSharedRef<ITableRow> SPreviewSourcePanel::GenerateWidget(TSharedPtr<Private::SPreviewSourceEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable) const
{
	typedef STableRow<TSharedPtr<Private::SPreviewSourceEntry>> RowType;

	TSharedRef<RowType> NewRow = SNew(RowType, OwnerTable);
	NewRow->SetContent(SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew(UE::PropertyViewer::SFieldIcon, Entry->GetClass())
		]
		+ SHorizontalBox::Slot()
		.Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(Entry->GetDisplayName())
		]);

	return NewRow;
}

} // namespace

#undef LOCTEXT_NAMESPACE
