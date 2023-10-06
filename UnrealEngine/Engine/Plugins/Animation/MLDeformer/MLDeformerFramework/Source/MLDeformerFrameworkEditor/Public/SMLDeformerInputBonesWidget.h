// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/Commands/Commands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "UObject/NameTypes.h"
#include "Styling/SlateColor.h"

struct FReferenceSkeleton;

namespace UE::MLDeformer
{
	class SMLDeformerInputBoneTreeWidget;
	class SMLDeformerInputBonesWidget;
	class SMLDeformerInputWidget;
	class FMLDeformerEditorModel;

	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerInputBonesWidgetCommands
		: public TCommands<FMLDeformerInputBonesWidgetCommands>
	{
	public:
		FMLDeformerInputBonesWidgetCommands();
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> AddInputBones;
		TSharedPtr<FUICommandInfo> DeleteInputBones;
		TSharedPtr<FUICommandInfo> ClearInputBones;
		TSharedPtr<FUICommandInfo> AddAnimatedBones;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerInputBoneTreeElement
		: public TSharedFromThis<FMLDeformerInputBoneTreeElement>
	{
	public:
		TSharedRef<ITableRow> MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FMLDeformerInputBoneTreeElement> InTreeElement, TSharedPtr<SMLDeformerInputBoneTreeWidget> InTreeWidget);

	public:
		FName Name;
		TArray<TSharedPtr<FMLDeformerInputBoneTreeElement>> Children;
		FSlateColor TextColor;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerInputBoneTreeRowWidget 
		: public STableRow<TSharedPtr<FMLDeformerInputBoneTreeElement>>
	{
	public:
		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FMLDeformerInputBoneTreeElement> InTreeElement, TSharedPtr<SMLDeformerInputBoneTreeWidget> InTreeView);

	private:
		TWeakPtr<FMLDeformerInputBoneTreeElement> WeakTreeElement;
		FText GetName() const;

		friend class SMLDeformerInputBoneTreeWidget; 
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerInputBoneTreeWidget
		: public STreeView<TSharedPtr<FMLDeformerInputBoneTreeElement>>
	{
		SLATE_BEGIN_ARGS(SMLDeformerInputBoneTreeWidget) {}
		SLATE_ARGUMENT(TSharedPtr<SMLDeformerInputBonesWidget>, InputBonesWidget)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs);
		const TArray<TSharedPtr<FMLDeformerInputBoneTreeElement>>& GetRootElements() const	{ return RootElements; }
		void AddElement(TSharedPtr<FMLDeformerInputBoneTreeElement> Element, TSharedPtr<FMLDeformerInputBoneTreeElement> ParentElement);
		void RefreshElements(const TArray<FName>& BoneNames, const FReferenceSkeleton* RefSkeleton);
		TArray<FName> ExtractAllElementNames() const;

	private:
		TSharedRef<ITableRow> MakeTableRowWidget(TSharedPtr<FMLDeformerInputBoneTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable);
		void HandleGetChildrenForTree(TSharedPtr<FMLDeformerInputBoneTreeElement> InItem, TArray<TSharedPtr<FMLDeformerInputBoneTreeElement>>& OutChildren);
		FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
		TSharedPtr<SWidget> OnContextMenuOpening() const;
		void RecursiveSortElements(TSharedPtr<FMLDeformerInputBoneTreeElement> Element);
		void RecursiveAddNames(const FMLDeformerInputBoneTreeElement& Element, TArray<FName>& OutNames) const;
		void OnSelectionChanged(TSharedPtr<FMLDeformerInputBoneTreeElement> Selection, ESelectInfo::Type SelectInfo);

	private:
		TArray<TSharedPtr<FMLDeformerInputBoneTreeElement>> RootElements;
		TSharedPtr<SMLDeformerInputBonesWidget> InputBonesWidget;
	};


	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerInputBonesWidget
		: public SCompoundWidget
	{
		friend class SMLDeformerInputBoneTreeWidget;

		SLATE_BEGIN_ARGS(SMLDeformerInputBonesWidget) {}
		SLATE_ARGUMENT(FMLDeformerEditorModel*, EditorModel)
		SLATE_ARGUMENT(TSharedPtr<SMLDeformerInputWidget>, InputWidget)
		SLATE_END_ARGS()

	public:
		void Construct(const FArguments& InArgs);
		void Refresh();
		void BindCommands(TSharedPtr<FUICommandList> CommandList);
		TSharedPtr<SMLDeformerInputBoneTreeWidget> GetTreeWidget() const;
		TSharedPtr<SMLDeformerInputWidget> GetInputWidget() const;
		FText GetSectionTitle() const;

	private:
		void OnFilterTextChanged(const FText& InFilterText);
		void RefreshTree(bool bBroadCastPropertyChanged=true);
		bool BroadcastModelPropertyChanged(const FName PropertyName);

		void OnAddInputBones();
		void OnDeleteInputBones();
		void OnClearInputBones();
		void OnAddAnimatedBones();

	private:
		TSharedPtr<SMLDeformerInputBoneTreeWidget> TreeWidget;
		TSharedPtr<SMLDeformerInputWidget> InputWidget;
		FMLDeformerEditorModel* EditorModel = nullptr;
		FString FilterText;
		FText SectionTitle;
	};

}	// namespace UE::MLDeformer

