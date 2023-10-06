// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "SMLDeformerInputWidget.h"
#include "Framework/Commands/Commands.h"

class FMenuBuilder;

namespace UE::MLDeformer
{
	class FMLDeformerEditorModel;
};

namespace UE::NeuralMorphModel
{
	class SNeuralMorphBoneGroupsWidget;
	class SNeuralMorphCurveGroupsWidget;
	class FNeuralMorphBoneGroupsTreeElement;
	class FNeuralMorphCurveGroupsTreeElement;

	class NEURALMORPHMODELEDITOR_API FNeuralMorphInputWidgetCommands
		: public TCommands<FNeuralMorphInputWidgetCommands>
	{
	public:
		FNeuralMorphInputWidgetCommands();
		virtual void RegisterCommands() override;

	public:
		TSharedPtr<FUICommandInfo> ResetAllBoneMasks;
		TSharedPtr<FUICommandInfo> ResetSelectedBoneMasks;
		TSharedPtr<FUICommandInfo> ExpandSelectedBoneMasks;

		TSharedPtr<FUICommandInfo> ResetAllBoneGroupMasks;
		TSharedPtr<FUICommandInfo> ResetSelectedBoneGroupMasks;
		TSharedPtr<FUICommandInfo> ExpandSelectedBoneGroupMasks;
	};


	class NEURALMORPHMODELEDITOR_API SNeuralMorphInputWidget
		: public UE::MLDeformer::SMLDeformerInputWidget
	{
	public:
		SLATE_BEGIN_ARGS(SNeuralMorphInputWidget) {}
		SLATE_ARGUMENT(UE::MLDeformer::FMLDeformerEditorModel*, EditorModel)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		// SMLDeformerInputWidget overrides.
		virtual void Refresh() override;
		virtual void AddInputBonesMenuItems(FMenuBuilder& MenuBuilder) override;
		virtual void AddInputBonesPlusIconMenuItems(FMenuBuilder& MenuBuilder) override;
		virtual void OnAddInputBones(const TArray<FName>& Names) override;
		virtual void OnAddInputCurves(const TArray<FName>& Names) override;
		virtual void OnAddAnimatedBones() override;
		virtual void OnClearInputBones() override;
		virtual void OnDeleteInputBones(const TArray<FName>& Names) override;
		virtual void OnDeleteInputCurves(const TArray<FName>& Names) override;
		virtual void OnSelectInputBone(FName BoneName) override;
		virtual void OnSelectInputCurve(FName BoneName) override;
		virtual void OnAddAnimatedCurves() override;
		virtual void ClearSelectionForAllWidgetsExceptThis(TSharedPtr<SWidget> ExceptThisWidget) override;
		virtual TSharedPtr<SWidget> GetExtraBonePickerWidget() override;
		// ~END SMLDeformerInputWidget overrides.

		void AddInputBoneGroupsMenuItems(FMenuBuilder& MenuBuilder);
		void AddInputBoneGroupsPlusIconMenuItems(FMenuBuilder& MenuBuilder);

		void OnSelectInputBoneGroup(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> Element);
		void OnSelectInputCurveGroup(TSharedPtr<FNeuralMorphCurveGroupsTreeElement> Element);

		TSharedPtr<FUICommandList> GetBoneGroupsCommandList() const	{ return BoneGroupsCommandList; }
		TSharedPtr<FUICommandList> GetCurveGroupsCommandList() const { return CurveGroupsCommandList; }

		int32 GetHierarchyDepth() const { return HierarchyDepth; }

	protected:
		void CreateBoneGroupsSection();
		void CreateCurveGroupsSection();

		void ResetAllBoneMasks();
		void ResetSelectedBoneMasks();
		void ExpandBoneMasks();

		void ResetAllBoneGroupMasks();
		void ResetSelectedBoneGroupMasks();
		void ExpandBoneGroupMasks();

		void BindCommands();
		FReply ShowBoneGroupsManageContextMenu();
		FReply ShowCurveGroupsManageContextMenu();

	protected:
		TSharedPtr<SNeuralMorphBoneGroupsWidget> BoneGroupsWidget;
		TSharedPtr<SNeuralMorphCurveGroupsWidget> CurveGroupsWidget;
		TSharedPtr<FUICommandList> BoneGroupsCommandList;
		TSharedPtr<FUICommandList> CurveGroupsCommandList;
		int32 HierarchyDepth = 2;
	};
}	// namespace UE::NeuralMorphModel
