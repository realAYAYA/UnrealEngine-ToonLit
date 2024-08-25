// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Replication/Editor/View/IReplicationStreamEditor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SHorizontalBox;
struct FConcertPropertyChain;
struct FConcertStreamObjectAutoBindingRules;

namespace UE::ConcertSharedSlate
{
	struct FCreateEditorParams;
	struct FCreateViewerParams;
	class IEditableReplicationStreamModel;
}

namespace UE::ConcertClientSharedSlate
{
	/**
	 * The default replication stream editor adds a checkbox in front of every property row.
	 * 
	 * The checkbox allows adding / removing the properties to / from the selected objects.
	 * The properties are sorted based on the check state: checked come before unchecked.
	 */
	class SDefaultReplicationStreamEditor : public ConcertSharedSlate::IReplicationStreamEditor
    {
    public:
		
		SLATE_BEGIN_ARGS(SDefaultReplicationStreamEditor) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, ConcertSharedSlate::FCreateEditorParams EditorParams, ConcertSharedSlate::FCreateViewerParams ViewerParams);

		//~ Begin IReplicationStreamEditor Interface
		virtual void Refresh() override;
		virtual void RequestObjectColumnResort(const FName& ColumnId) override;
		virtual void RequestPropertyColumnResort(const FName& ColumnId) override;
		virtual TArray<FSoftObjectPath> GetObjectsBeingPropertyEdited() const override;
		//~ End IReplicationStreamEditor Interface

	private:

		/** Core editor implementing generic editing UI & workflows. */
		TSharedPtr<IReplicationStreamEditor> WrappedEditor;
		
		/** Model needed for sorting properties based on selection. */
		TSharedPtr<ConcertSharedSlate::IEditableReplicationStreamModel> PropertiesModel;
    };
}

