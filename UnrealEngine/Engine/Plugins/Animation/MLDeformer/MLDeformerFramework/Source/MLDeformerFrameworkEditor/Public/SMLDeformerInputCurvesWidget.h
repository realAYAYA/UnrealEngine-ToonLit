// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Framework/Commands/Commands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/NameTypes.h"
#include "Styling/SlateColor.h"

class USkeleton;

namespace UE::MLDeformer
{
	class SMLDeformerInputCurveListWidget;
	class SMLDeformerInputCurvesWidget;
	class SMLDeformerInputWidget;
	class FMLDeformerEditorModel;

	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerInputCurvesWidgetCommands
		: public TCommands<FMLDeformerInputCurvesWidgetCommands>
	{
	public:
		FMLDeformerInputCurvesWidgetCommands();
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> AddInputCurves;
		TSharedPtr<FUICommandInfo> DeleteInputCurves;
		TSharedPtr<FUICommandInfo> ClearInputCurves;
		TSharedPtr<FUICommandInfo> AddAnimatedCurves;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerInputCurveListElement
		: public TSharedFromThis<FMLDeformerInputCurveListElement>
	{
	public:
		TSharedRef<ITableRow> MakeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FMLDeformerInputCurveListElement> InTreeElement, TSharedPtr<SMLDeformerInputCurveListWidget> InTreeWidget);

	public:
		FName Name;
		FSlateColor TextColor;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerInputCurveListRowWidget 
		: public STableRow<TSharedPtr<FMLDeformerInputCurveListElement>>
	{
	public:
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FMLDeformerInputCurveListElement> InElement, TSharedPtr<SMLDeformerInputCurveListWidget> InListView);

	private:
		TWeakPtr<FMLDeformerInputCurveListElement> WeakElement;
		FText GetName() const;

		friend class SMLDeformerInputCurveListWidget; 
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerInputCurveListWidget
		: public SListView<TSharedPtr<FMLDeformerInputCurveListElement>>
	{
		SLATE_BEGIN_ARGS(SMLDeformerInputCurveListWidget) {}
		SLATE_ARGUMENT(TSharedPtr<SMLDeformerInputCurvesWidget>, InputCurvesWidget)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs);
		const TArray<TSharedPtr<FMLDeformerInputCurveListElement>>& GetElements() const	{ return Elements; }
		void AddElement(TSharedPtr<FMLDeformerInputCurveListElement> Element);
		void RefreshElements(const TArray<FName>& CurveNames, const USkeleton* Skeleton);

	private:
		TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FMLDeformerInputCurveListElement> InItem, const TSharedRef<STableViewBase>& OwnerTable);
		FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
		TSharedPtr<SWidget> OnContextMenuOpening() const;
		void SortElements();
		void OnSelectionChanged(TSharedPtr<FMLDeformerInputCurveListElement> Selection, ESelectInfo::Type SelectInfo);

	private:
		TArray<TSharedPtr<FMLDeformerInputCurveListElement>> Elements;
		TSharedPtr<SMLDeformerInputCurvesWidget> InputCurvesWidget;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerInputCurvesWidget
		: public SCompoundWidget
	{
		friend class SMLDeformerInputCurveListWidget;

		SLATE_BEGIN_ARGS(SMLDeformerInputCurvesWidget) {}
		SLATE_ARGUMENT(FMLDeformerEditorModel*, EditorModel)
		SLATE_ARGUMENT(TSharedPtr<SMLDeformerInputWidget>, InputWidget)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs);
		void Refresh();
		void BindCommands(TSharedPtr<FUICommandList> CommandList);
		FText GetSectionTitle() const;
		TSharedPtr<SMLDeformerInputCurveListWidget> GetListWidget() const;
		TSharedPtr<SMLDeformerInputWidget> GetInputWidget() const;

	private:
		void OnFilterTextChanged(const FText& InFilterText);
		void RefreshList(bool bBroadCastPropertyChanged=true);
		bool BroadcastModelPropertyChanged(const FName PropertyName);

		void OnAddInputCurves();
		void OnDeleteInputCurves();
		void OnClearInputCurves();
		void OnAddAnimatedCurves();

	private:
		TSharedPtr<SMLDeformerInputWidget> InputWidget;
		TSharedPtr<SMLDeformerInputCurveListWidget> ListWidget;
		FMLDeformerEditorModel* EditorModel = nullptr;
		FString FilterText;
		FText SectionTitle;
	};

}	// namespace UE::MLDeformer

