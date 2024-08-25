// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"
#include "Containers/UnrealString.h"

class UWorld;
class AActor;

namespace UE::MLDeformer
{
	class FMLDeformerEditorToolkit;
}

namespace UE::MLDeformer
{
	struct FMLDeformerDebugActor
	{
		/** The actor inside the PIE viewport, or nullptr when debugging is disabled. */
		TObjectPtr<AActor> Actor;

		/**
		 * Is this actor selected in the editor / engine?
		 * This is different from the Debug Actor we picked, we call that Active debug actor. 
		 */
		bool bSelectedInEngine = false;
	};

	/**
	 * 
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API SMLDeformerDebugSelectionWidget
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SMLDeformerDebugSelectionWidget) {}
		SLATE_ARGUMENT(FMLDeformerEditorToolkit*, MLDeformerEditor)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

		/** Refresh the list of debuggable actors in the combobox. */
		void Refresh();

		/** Get the current actor we're debugging, or a nullptr when debugging is disabled. */
		AActor* GetDebugActor() const;

		const TArray<TSharedPtr<FMLDeformerDebugActor>>& GetActors() const { return Actors; }

	private:
		TSharedRef<SWidget> OnGenerateActorComboBoxItemWidget(TSharedPtr<FMLDeformerDebugActor> Item);
		TArray<TSharedPtr<FMLDeformerDebugActor>> GetDebugActorsForWorld(TObjectPtr<UWorld> World) const;
		FText GetComboBoxText() const;
		void OnActorSelectionChanged(TSharedPtr<FMLDeformerDebugActor> Item, ESelectInfo::Type SelectInfo);
		void RefreshActorList();
		void UpdateDebugActorSelectedFlags();
		bool IsDebuggingDisabled() const;

	private:
		/** The combobox that contains the actor names. */
		TSharedPtr<SComboBox<TSharedPtr<FMLDeformerDebugActor>>> ActorComboBox;

		/** The actors we can debug. */
		TArray<TSharedPtr<FMLDeformerDebugActor>> Actors;

		/** The selected actor. */
		TSharedPtr<FMLDeformerDebugActor> ActiveDebugActor;

		/** The name of the actor we are debugging. */
		FText DebugActorName;

		/** A pointer to our asset editor. */
		FMLDeformerEditorToolkit* MLDeformerEditor = nullptr;
	};

}	// namespace UE::MLDeformer
