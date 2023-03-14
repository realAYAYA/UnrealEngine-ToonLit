// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/PresenceCommon.h"

#include "Online/OnlineServicesCommon.h"

namespace UE::Online {

FPresenceCommon::FPresenceCommon(FOnlineServicesCommon& InServices)
	: TOnlineComponent(TEXT("Presence"), InServices)
	, Services(InServices)
{
}

TOnlineAsyncOpHandle<FQueryPresence> FPresenceCommon::QueryPresence(FQueryPresence::Params&& Params)
{
	TOnlineAsyncOpRef<FQueryPresence> Operation = GetOp<FQueryPresence>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}
/*
      The common implementation of BatchQueryPresence sends out a bunch of asynchronous QueryPresence calls and combines them. 
*/ 
struct FBatchQueryPresenceHelperResult
{
public:
	FBatchQueryPresence::Result Result;
	FOnlineError ErrorState;
};

class FBatchQueryPresenceHelper
{
public:
	FBatchQueryPresenceHelper(TOnlineAsyncOpRef<FBatchQueryPresence> InOp, TSharedPtr<IPresence> InPresence)
		: Op(InOp)
		, Presence(InPresence)
		, ErrorState(Errors::Success())
	{
		NumPresencesRemaining = Op->GetParams().TargetAccountIds.Num();
	}

	TFuture<FBatchQueryPresenceHelperResult> GetPromise()
	{
		for(FAccountId TargetAccountId : Op->GetParams().TargetAccountIds)
		{
			FQueryPresence::Params Params;
			Params.LocalAccountId = Op->GetParams().LocalAccountId;
			Params.TargetAccountId = TargetAccountId;
			Params.bListenToChanges = Op->GetParams().bListenToChanges;

			Presence->QueryPresence(MoveTemp(Params)).OnComplete([this](const TOnlineResult<FQueryPresence> NewOp) mutable
			{
				if (NewOp.IsOk())
				{
					// push results
					Result.Presences.Add(NewOp.GetOkValue().Presence);
				}
				else
				{
					// error
					// we're only reporting the most recent one, more verbose logging on each individual error can be found in the logs
					ErrorState = NewOp.GetErrorValue();
				}


				// final calculations
				if (--NumPresencesRemaining <= 0)
				{
					Promise.SetValue(FBatchQueryPresenceHelperResult{ MoveTemp(Result), MoveTemp(ErrorState) });
				}
			});
		}

		return Promise.GetFuture();
	}

	FBatchQueryPresence::Result Result;
	TOnlineAsyncOpRef<FBatchQueryPresence> Op;
	TSharedPtr<IPresence> Presence;
	FOnlineError ErrorState;
	int NumPresencesRemaining = 0;
	TPromise<FBatchQueryPresenceHelperResult> Promise;
};

// Base implementation for the batch query function makes multiple async calls to query and returns them as a single output
TOnlineAsyncOpHandle<FBatchQueryPresence> FPresenceCommon::BatchQueryPresence(FBatchQueryPresence::Params&& Params)
{
	TOnlineAsyncOpRef<FBatchQueryPresence> Op = GetOp<FBatchQueryPresence>(MoveTemp(Params));
	TSharedRef<FBatchQueryPresenceHelper> Helper = MakeShared<FBatchQueryPresenceHelper>(Op, this->AsShared());

	Op->Then([this, Helper](TOnlineAsyncOp<FBatchQueryPresence>& InAsyncOp) mutable
	{
		return Helper->GetPromise();
	})
	.Then([this, Helper](TOnlineAsyncOp<FBatchQueryPresence>& InAsyncOp, FBatchQueryPresenceHelperResult Result) mutable
	{
		if (Result.ErrorState == Errors::Success())
		{
			InAsyncOp.SetResult(MoveTemp(Result.Result));
		}
		else 
		{
			InAsyncOp.SetError(MoveTemp(Result.ErrorState));
		}
	})
	.Enqueue(Services.GetParallelQueue());

	return Op->GetHandle();
}

TOnlineResult<FGetCachedPresence> FPresenceCommon::GetCachedPresence(FGetCachedPresence::Params&& Params)
{
	return TOnlineResult<FGetCachedPresence>(Errors::NotImplemented());
}

TOnlineAsyncOpHandle<FUpdatePresence> FPresenceCommon::UpdatePresence(FUpdatePresence::Params&& Params)
{
	TOnlineAsyncOpRef<FUpdatePresence> Operation = GetOp<FUpdatePresence>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineAsyncOpHandle<FPartialUpdatePresence> FPresenceCommon::PartialUpdatePresence(FPartialUpdatePresence::Params&& Params)
{
	TOnlineResult<FGetCachedPresence> CachedPresence = GetCachedPresence({Params.LocalAccountId, Params.LocalAccountId});
	if (CachedPresence.IsOk())
	{
		TSharedRef<FUserPresence> NewPresence = MakeShared<FUserPresence>();
		TSharedRef<const FUserPresence> ExistingPresence = CachedPresence.GetOkValue().Presence;
		*NewPresence = *ExistingPresence;

		if (Params.Mutations.Status.IsSet())
		{
			NewPresence->Status = Params.Mutations.Status.GetValue();
		}

		if (Params.Mutations.GameStatus.IsSet())
		{
			NewPresence->GameStatus = Params.Mutations.GameStatus.GetValue();
		}

		if (Params.Mutations.Joinability.IsSet())
		{
			NewPresence->Joinability = Params.Mutations.Joinability.GetValue();
		}

		for (const TPair<FString, FPresenceVariant>& UpdatedProperty : Params.Mutations.UpdatedProperties)
		{
			NewPresence->Properties.Add(UpdatedProperty.Key, UpdatedProperty.Value);
		}

		for (const FString& RemovedProperty : Params.Mutations.RemovedProperties)
		{
			NewPresence->Properties.Remove(RemovedProperty);
		}

		TOnlineAsyncOpRef<FPartialUpdatePresence> Op = GetOp<FPartialUpdatePresence>(MoveTemp(Params));
		TOnlineAsyncOpHandle<FUpdatePresence> UpdatePresenceResult = UpdatePresence({Op->GetParams().LocalAccountId, NewPresence});
		UpdatePresenceResult.OnComplete([this, Op](const TOnlineResult<FUpdatePresence> Result)
		{
			if (Result.IsOk())
			{
				Op->SetResult(FPartialUpdatePresence::Result());
			}
			else
			{
				FOnlineError Error = Result.GetErrorValue();
				Op->SetError(MoveTemp(Error));
			}
		});

		return Op->GetHandle();
	}

	TOnlineAsyncOpRef<FPartialUpdatePresence> Operation = GetOp<FPartialUpdatePresence>(MoveTemp(Params));
	Operation->SetError(Errors::NotImplemented());
	return Operation->GetHandle();
}

TOnlineEvent<void(const FPresenceUpdated&)> FPresenceCommon::OnPresenceUpdated()
{
	return OnPresenceUpdatedEvent;
}

/* UE::Online */ }
