// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "UObject/GCObject.h"

struct FConcertObjectReplicationMap;

namespace UE::ConcertSharedSlate
{
	class IStreamExtender;

	/** Special case of FGenericPropertySelectionModel where the edited FConcertObjectReplicationMap lives in an UObject that is RF_Transactional. */
	class FTransactionalReplicationStreamModel
		: public IEditableReplicationStreamModel
		, public FSelfRegisteringEditorUndoClient
		, public FGCObject
	{
	public:

		FTransactionalReplicationStreamModel(
			TSharedRef<IEditableReplicationStreamModel> WrappedModel,
			UObject& OwningObject
			);

		//~ Begin IReplicationStreamModel Interface
		virtual FSoftClassPath GetObjectClass(const FSoftObjectPath& Object) const override { return WrappedModel->GetObjectClass(Object); }
		virtual bool ContainsObjects(const TSet<FSoftObjectPath>& Objects) const override { return WrappedModel->ContainsObjects(Objects); }
		virtual bool ContainsProperties(const FSoftObjectPath& Object, const TSet<FConcertPropertyChain>& Properties) const override { return WrappedModel->ContainsProperties(Object, Properties); }
		virtual bool ForEachReplicatedObject(TFunctionRef<EBreakBehavior(const FSoftObjectPath& Object)> Delegate) const override { return WrappedModel->ForEachReplicatedObject(Delegate); }
		virtual bool ForEachProperty(const FSoftObjectPath& Object, TFunctionRef<EBreakBehavior(const FConcertPropertyChain& Property)> Delegate) const override { return WrappedModel->ForEachProperty(Object, Delegate); }
		virtual FOnObjectsChanged& OnObjectsChanged() override { return WrappedModel->OnObjectsChanged(); }
		virtual FOnPropertiesChanged& OnPropertiesChanged() override { return WrappedModel->OnPropertiesChanged(); }
		//~ End IReplicationStreamModel Interface
		
		//~ Begin IEditableReplicationStreamModel Interface
		virtual void AddObjects(TConstArrayView<UObject*> Objects) override;
		virtual void RemoveObjects(TConstArrayView<FSoftObjectPath> Objects) override;
		virtual void AddProperties(const FSoftObjectPath&, TConstArrayView<FConcertPropertyChain> Properties) override;
		virtual void RemoveProperties(const FSoftObjectPath&, TConstArrayView<FConcertPropertyChain> Properties) override;
		//~ End IEditableReplicationStreamModel Interface

		//~ Begin FEditorUndoClient Interface
		virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FEditorUndoClient Interface

		//~ Begin FGCObject Interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FTransactionalReplicationStreamModel"); }
		//~ End FGCObject Interface

	private:

		/** The model to be transacted */
		TSharedRef<IEditableReplicationStreamModel> WrappedModel;
		
		/** User of FTransactionalPropertySelectionModel is responsible for keeping OwningObject alive, e.g. via an asset editor. */
		TWeakObjectPtr<UObject> OwningObject;
	};
}


