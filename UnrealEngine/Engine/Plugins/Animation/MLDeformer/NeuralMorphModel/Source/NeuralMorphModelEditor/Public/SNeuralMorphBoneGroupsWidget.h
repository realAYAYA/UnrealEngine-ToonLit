// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/Commands/Commands.h"

class SWidget;

namespace UE::MLDeformer
{
	class SMLDeformerInputWidget;
}

namespace UE::NeuralMorphModel
{
	class FNeuralMorphEditorModel;
	class SNeuralMorphBoneGroupsWidget;
	class SNeuralMorphInputWidget;
	class FNeuralMorphBoneGroupsCommands
		: public TCommands<FNeuralMorphBoneGroupsCommands>
	{
	public:
		FNeuralMorphBoneGroupsCommands();
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> CreateGroup;
		TSharedPtr<FUICommandInfo> DeleteSelectedItems;
		TSharedPtr<FUICommandInfo> ClearGroups;
		TSharedPtr<FUICommandInfo> AddBoneToGroup;
	};


	class FNeuralMorphBoneGroupsTreeElement
		: public TSharedFromThis<FNeuralMorphBoneGroupsTreeElement>
	{
	public:
		TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FNeuralMorphBoneGroupsTreeElement> InTreeElement, TSharedPtr<SNeuralMorphBoneGroupsWidget> InTreeWidget);

		bool IsGroup() const { return GroupIndex != INDEX_NONE; }

	public:
		FName Name;
		TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>> Children;
		TWeakPtr<FNeuralMorphBoneGroupsTreeElement> ParentGroup;
		FSlateColor TextColor;
		int32 GroupIndex = INDEX_NONE;
		int32 GroupBoneIndex = INDEX_NONE;
	};


	class SNeuralMorphBoneGroupsTreeRowWidget 
		: public STableRow<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>
	{
	public:
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FNeuralMorphBoneGroupsTreeElement> InTreeElement, TSharedPtr<SNeuralMorphBoneGroupsWidget> InTreeView);

	private:
		TWeakPtr<FNeuralMorphBoneGroupsTreeElement> WeakTreeElement;
		FText GetName() const;

		friend class SNeuralMorphBoneGroupsWidget; 
	};


	class NEURALMORPHMODELEDITOR_API SNeuralMorphBoneGroupsWidget
		: public STreeView<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>
	{
	public:
		SLATE_BEGIN_ARGS(SNeuralMorphBoneGroupsWidget) {}
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
		void AddElement(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> Element, TSharedPtr<FNeuralMorphBoneGroupsTreeElement> ParentElement);	
		void HandleGetChildrenForTree(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> InItem, TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>& OutChildren);	
		void OnSelectionChanged(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> Selection, ESelectInfo::Type SelectInfo);
		const TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>& GetRootElements() const;
		FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
		TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable);
		TSharedPtr<SWidget> CreateContextMenuWidget() const;

		void UpdateTreeElements();
		void RefreshTree(bool bBroadcastPropertyChanged);
		bool BroadcastModelPropertyChanged(const FName PropertyName);
		TSharedPtr<SWidget> CreateContextWidget() const;

		void OnCreateBoneGroup();
		void OnDeleteSelectedItems();
		void OnClearBoneGroups();
		void OnAddBoneToGroup();

	private:
		TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>> RootElements;
		TSharedPtr<UE::MLDeformer::SMLDeformerInputWidget> InputWidget;
		FNeuralMorphEditorModel* EditorModel = nullptr;
		FText SectionTitle;
	};

}	// namespace UE::NeuralMorphModel
