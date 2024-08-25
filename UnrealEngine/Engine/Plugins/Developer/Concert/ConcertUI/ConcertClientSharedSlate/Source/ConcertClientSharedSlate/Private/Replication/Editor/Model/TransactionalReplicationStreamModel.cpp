// Copyright Epic Games, Inc. All Rights Reserved.

#include "TransactionalReplicationStreamModel.h"

#include "ScopedTransaction.h"
#include "Misc/TransactionObjectEvent.h"

#define LOCTEXT_NAMESPACE "FTransactionalReplicationStreamModel"

namespace UE::ConcertSharedSlate
{
	FTransactionalReplicationStreamModel::FTransactionalReplicationStreamModel(
		TSharedRef<IEditableReplicationStreamModel> WrappedModel,
		UObject& OwningObject
		)
		: WrappedModel(MoveTemp(WrappedModel))
		, OwningObject(&OwningObject)
	{}

	void FTransactionalReplicationStreamModel::AddObjects(TConstArrayView<UObject*> Objects)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddObjects", "Add replicated objects"));
		OwningObject->Modify();
		WrappedModel->AddObjects(Objects);
	}

	void FTransactionalReplicationStreamModel::RemoveObjects(TConstArrayView<FSoftObjectPath> Objects)
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveObjects", "Remove replicated objects"));
		OwningObject->Modify();
		WrappedModel->RemoveObjects(Objects);
	}

	void FTransactionalReplicationStreamModel::AddProperties(const FSoftObjectPath& SoftObjectPath, TConstArrayView<FConcertPropertyChain> Properties)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddProperties", "Add replicated properties"));
		OwningObject->Modify();
		WrappedModel->AddProperties(SoftObjectPath, Properties);
	}

	void FTransactionalReplicationStreamModel::RemoveProperties(const FSoftObjectPath& SoftObjectPath, TConstArrayView<FConcertPropertyChain> Properties)
	{
		const FScopedTransaction Transaction(LOCTEXT("RemoveProperties", "Remove replicated properties"));
		OwningObject->Modify();
		WrappedModel->RemoveProperties(SoftObjectPath, Properties);
	}

	bool FTransactionalReplicationStreamModel::MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const
	{
		if (!OwningObject.IsValid())
		{
			return false;
		}
		
		for (const TPair<UObject*, FTransactionObjectEvent>& TransactionObjectPair : TransactionObjectContexts)
		{
			UObject* Object = TransactionObjectPair.Key;
			while (Object != nullptr)
			{
				if (Object == OwningObject)
				{
					return true;
				}
				Object = Object->GetOuter();
			}
		}

		return false;
	}

	void FTransactionalReplicationStreamModel::PostUndo(bool bSuccess)
	{
		OnObjectsChanged().Broadcast({}, {}, EReplicatedObjectChangeReason::ExternalChange);
		OnPropertiesChanged().Broadcast();
	}

	void FTransactionalReplicationStreamModel::PostRedo(bool bSuccess)
	{
		OnObjectsChanged().Broadcast({}, {}, EReplicatedObjectChangeReason::ExternalChange);
		OnPropertiesChanged().Broadcast();
	}

	void FTransactionalReplicationStreamModel::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(OwningObject);
	}
}

#undef LOCTEXT_NAMESPACE