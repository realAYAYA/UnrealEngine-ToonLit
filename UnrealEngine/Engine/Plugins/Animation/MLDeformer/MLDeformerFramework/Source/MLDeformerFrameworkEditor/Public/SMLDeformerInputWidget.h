// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Framework/SlateDelegates.h"
#include "Framework/Commands/UICommandList.h"

class SWidget;
class FMenuBuilder;
class SVerticalBox;

namespace UE::MLDeformer
{
	class SMLDeformerInputBonesWidget;
	class SMLDeformerInputCurvesWidget;
	class FMLDeformerEditorModel;

	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerInputWidget
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMLDeformerInputWidget) {}
		SLATE_ARGUMENT(FMLDeformerEditorModel*, EditorModel)
		SLATE_END_ARGS()

		struct MLDEFORMERFRAMEWORKEDITOR_API FSectionInfo
		{
			TAttribute<FText> PlusButtonTooltip;
			TAttribute<FText> SectionTitle;
			FOnClicked PlusButtonPressed;
		};

		void Construct(const FArguments& InArgs);
		void AddSection(TSharedPtr<SWidget> Widget, const FSectionInfo& SectionInfo);

		TSharedPtr<FUICommandList> GetBonesCommandList() const;
		TSharedPtr<FUICommandList> GetCurvesCommandList() const;

		/**
		 * Refresh all sub-widget contents.
		 */
		virtual void Refresh();

		virtual void AddInputBonesMenuItems(FMenuBuilder& MenuBuilder) {}
		virtual void AddInputBonesPlusIconMenuItems(FMenuBuilder& MenuBuilder) {}
		virtual void AddInputCurvesMenuItems(FMenuBuilder& MenuBuilder) {}
		virtual void AddInputCurvesPlusIconMenuItems(FMenuBuilder& MenuBuilder) {}

		virtual void OnClearInputBones() {}
		virtual void OnAddInputBones(const TArray<FName>& Names) {}
		virtual void OnDeleteInputBones(const TArray<FName>& Names) {}
		virtual void OnAddAnimatedBones() {}

		virtual void OnClearInputCurves() {}
		virtual void OnAddInputCurves(const TArray<FName>& Names) {}
		virtual void OnDeleteInputCurves(const TArray<FName>& Names) {}
		virtual void OnAddAnimatedCurves() {}

		virtual void ClearSelectionForAllWidgetsExceptThis(TSharedPtr<SWidget> ExceptThisWidget);
		virtual void OnSelectInputBone(FName BoneName) {}
		virtual void OnSelectInputCurve(FName BoneName) {}

		virtual TSharedPtr<SWidget> GetExtraBonePickerWidget();

		// Registers the commands that are being executed by the different widgets.
		static void RegisterCommands();

	protected:
		void AddSectionSeparator();
		void CreateBonesWidget();
		void CreateCurvesWidget();

		FReply ShowCurvesPlusIconContextMenu();
		FReply ShowBonesPlusIconContextMenu();

	protected:
		TSharedPtr<SMLDeformerInputBonesWidget> InputBonesWidget;
		TSharedPtr<SMLDeformerInputCurvesWidget> InputCurvesWidget;
		TSharedPtr<FUICommandList> BonesCommandList;
		TSharedPtr<FUICommandList> CurvesCommandList;
		TSharedPtr<SVerticalBox> SectionVerticalBox;
		FMLDeformerEditorModel* EditorModel = nullptr;
	};
}
