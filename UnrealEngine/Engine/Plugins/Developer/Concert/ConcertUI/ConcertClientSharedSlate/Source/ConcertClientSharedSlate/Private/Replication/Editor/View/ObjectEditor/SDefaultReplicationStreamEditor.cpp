// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDefaultReplicationStreamEditor.h"

#include "Replication/ReplicationWidgetFactories.h"

namespace UE::ConcertClientSharedSlate
{
	void SDefaultReplicationStreamEditor::Construct(const FArguments& InArgs, ConcertSharedSlate::FCreateEditorParams EditorParams, ConcertSharedSlate::FCreateViewerParams ViewerParams)
	{
		PropertiesModel = EditorParams.DataModel;

		WrappedEditor = CreateBaseStreamEditor(MoveTemp(EditorParams), MoveTemp(ViewerParams));
		ChildSlot
		[
			WrappedEditor.ToSharedRef()
		];
	}

	void SDefaultReplicationStreamEditor::Refresh()
	{
		return WrappedEditor->Refresh();
	}

	void SDefaultReplicationStreamEditor::RequestObjectColumnResort(const FName& ColumnId)
	{
		WrappedEditor->RequestObjectColumnResort(ColumnId);
	}

	void SDefaultReplicationStreamEditor::RequestPropertyColumnResort(const FName& ColumnId)
	{
		WrappedEditor->RequestPropertyColumnResort(ColumnId);
	}

	TArray<FSoftObjectPath> SDefaultReplicationStreamEditor::GetObjectsBeingPropertyEdited() const
	{
		return WrappedEditor->GetObjectsBeingPropertyEdited();
	}
}
