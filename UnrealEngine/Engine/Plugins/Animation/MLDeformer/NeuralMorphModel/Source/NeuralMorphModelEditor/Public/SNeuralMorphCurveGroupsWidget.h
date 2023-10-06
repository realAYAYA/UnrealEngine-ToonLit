// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/Commands/Commands.h"
#include "MLDeformerEditorStyle.h"

class SWidget;

namespace UE::MLDeformer
{
	class SMLDeformerInputWidget;
};

namespace UE::NeuralMorphModel
{
	class FNeuralMorphEditorModel;
	class SNeuralMorphCurveGroupsWidget;

	class FNeuralMorphCurveGroupsCommands
		: public TCommands<FNeuralMorphCurveGroupsCommands>
	{
	public:
		FNeuralMorphCurveGroupsCommands();
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> CreateGroup;
		TSharedPtr<FUICommandInfo> DeleteSelectedItems;
		TSharedPtr<FUICommandInfo> ClearGroups;
		TSharedPtr<FUICommandInfo> AddCurveToGroup;
	};


	class FNeuralMorphCurveGroupsTreeElement
		: public TSharedFromThis<FNeuralMorphCurveGroupsTreeElement>
	{
	public:
		TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FNeuralMorphCurveGroupsTreeElement> InTreeElement, TSharedPtr<SNeuralMorphCurveGroupsWidget> InTreeWidget);

		bool IsGroup() const { return GroupIndex != INDEX_NONE; }

	public:
		FName Name;
		TArray<TSharedPtr<FNeuralMorphCurveGroupsTreeElement>> Children;
		TWeakPtr<FNeuralMorphCurveGroupsTreeElement> ParentGroup;
		FSlateColor TextColor;
		int32 GroupIndex = INDEX_NONE;
		int32 GroupCurveIndex = INDEX_NONE;
	};


	class SNeuralMorphCurveGroupsTreeRowWidget 
		: public STableRow<TSharedPtr<FNeuralMorphCurveGroupsTreeElement>>
	{
	public:
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FNeuralMorphCurveGroupsTreeElement> InTreeElement, TSharedPtr<SNeuralMorphCurveGroupsWidget> InTreeView);

	private:
		TWeakPtr<FNeuralMorphCurveGroupsTreeElement> WeakTreeElement;
		FText GetName() const;

		friend class SNeuralMorphCurveGroupsWidget; 
	};


	class NEURALMORPHMODELEDITOR_API SNeuralMorphCurveGroupsWidget
		: public STreeView<TSharedPtr<FNeuralMorphCurveGroupsTreeElement>>
	{
	public:
		SLATE_BEGIN_ARGS(SNeuralMorphCurveGroupsWidget) {}
		SLATE_ARGUMENT(FNeuralMorphEditorModel*, EditorModel)
		SLATE_ARGUMENT(TSharedPtr<UE::MLDeformer::SMLDeformerInputWidget>, InputWidget)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);
		void BindCommands(TSharedPtr<FUICommandList> CommandList);
		void Refresh();

		FText GetSectionTitle() const;
		int32 GetNumSelectedGroups() const;
		TSharedPtr<UE::MLDeformer::SMLDeformerInputWidget> GetInputWidget() const;

	private:
		void AddElement(TSharedPtr<FNeuralMorphCurveGroupsTreeElement> Element, TSharedPtr<FNeuralMorphCurveGroupsTreeElement> ParentElement);	
		void HandleGetChildrenForTree(TSharedPtr<FNeuralMorphCurveGroupsTreeElement> InItem, TArray<TSharedPtr<FNeuralMorphCurveGroupsTreeElement>>& OutChildren);	
		void OnSelectionChanged(TSharedPtr<FNeuralMorphCurveGroupsTreeElement> Selection, ESelectInfo::Type SelectInfo);
		const TArray<TSharedPtr<FNeuralMorphCurveGroupsTreeElement>>& GetRootElements() const;
		FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
		TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FNeuralMorphCurveGroupsTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable);
		TSharedPtr<SWidget> CreateContextMenuWidget() const;

		void UpdateTreeElements();
		void RefreshTree(bool bBroadcastPropertyChanged);
		bool BroadcastModelPropertyChanged(const FName PropertyName);
		TSharedPtr<SWidget> CreateContextWidget() const;

		void OnCreateCurveGroup();
		void OnDeleteSelectedItems();
		void OnClearCurveGroups();
		void OnAddCurveToGroup();

	private:
		TArray<TSharedPtr<FNeuralMorphCurveGroupsTreeElement>> RootElements;
		TSharedPtr<UE::MLDeformer::SMLDeformerInputWidget> InputWidget;
		FNeuralMorphEditorModel* EditorModel = nullptr;
		FText SectionTitle;
	};

}	// namespace UE::NeuralMorphModel
