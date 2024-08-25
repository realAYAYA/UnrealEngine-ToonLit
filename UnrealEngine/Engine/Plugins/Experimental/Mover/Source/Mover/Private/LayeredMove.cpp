// Copyright Epic Games, Inc. All Rights Reserved.

#include "LayeredMove.h"
#include "MoverLog.h"
#include "MoverComponent.h"
#include "MoverSimulationTypes.h"

const float LayeredMove_InvalidTime = -UE_BIG_NUMBER;

void FLayeredMoveFinishVelocitySettings::NetSerialize(FArchive& Ar)
{
	uint8 bHasFinishVelocitySettings = Ar.IsSaving() ? 0 : (FinishVelocityMode == ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity);
	Ar.SerializeBits(&bHasFinishVelocitySettings, 1);

	if (bHasFinishVelocitySettings)
	{
		uint8 FinishVelocityModeAsU8 = (uint8)(FinishVelocityMode);
		Ar << FinishVelocityModeAsU8;
		FinishVelocityMode = (ELayeredMoveFinishVelocityMode)FinishVelocityModeAsU8;

		if (FinishVelocityMode == ELayeredMoveFinishVelocityMode::SetVelocity)
		{
			Ar << SetVelocity;
		}
		else if (FinishVelocityMode == ELayeredMoveFinishVelocityMode::ClampVelocity)
		{
			Ar << ClampVelocity;
		}
	}
}

FLayeredMoveBase::FLayeredMoveBase() :
	MixMode(EMoveMixMode::AdditiveVelocity),
	Priority(0),
	DurationMs(-1.f),
	StartSimTimeMs(LayeredMove_InvalidTime)
{
}


void FLayeredMoveBase::StartMove(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, float CurrentSimTimeMs)
{
	StartSimTimeMs = CurrentSimTimeMs;
	OnStart(MoverComp, SimBlackboard);
}

bool FLayeredMoveBase::IsFinished(float CurrentSimTimeMs) const
{
	const bool bHasStarted = (StartSimTimeMs >= 0.f);
	const bool bTimeExpired = bHasStarted && (DurationMs > 0.f) && (StartSimTimeMs + DurationMs <= CurrentSimTimeMs);
	const bool bDidTickOnceAndExpire = bHasStarted && (DurationMs == 0.f);

	return bTimeExpired || bDidTickOnceAndExpire;
}

void FLayeredMoveBase::EndMove(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, float CurrentSimTimeMs)
{
	OnEnd(MoverComp, SimBlackboard, CurrentSimTimeMs);
}

FLayeredMoveBase* FLayeredMoveBase::Clone() const
{
	// If child classes don't override this, saved moves will not work
	checkf(false, TEXT("FLayeredMoveBase::Clone() being called erroneously. This should always be overridden in child classes!"));
	return nullptr;
}


void FLayeredMoveBase::NetSerialize(FArchive& Ar)
{
	uint8 MixModeAsU8 = (uint8)MixMode;
	Ar << MixModeAsU8;
	MixMode = (EMoveMixMode)MixModeAsU8;

	uint8 bHasDefaultPriority = Priority == 0;
	Ar.SerializeBits(&bHasDefaultPriority, 1);
	if (!bHasDefaultPriority)
	{
		Ar << Priority;
	}
	
	Ar << DurationMs;
	Ar << StartSimTimeMs;

	FinishVelocitySettings.NetSerialize(Ar);
}


UScriptStruct* FLayeredMoveBase::GetScriptStruct() const
{
	return FLayeredMoveBase::StaticStruct();
}


FString FLayeredMoveBase::ToSimpleString() const
{
	return GetScriptStruct()->GetName();
}


FLayeredMoveGroup::FLayeredMoveGroup()
	: bApplyResidualVelocity(false)
	, ResidualVelocity(FVector::Zero())
	, ResidualClamping(-1.f)
{
}


void FLayeredMoveGroup::QueueLayeredMove(TSharedPtr<FLayeredMoveBase> Move)
{
	if (ensure(Move.IsValid()))
	{
		QueuedLayeredMoves.Add(Move);
		UE_LOG(LogMover, VeryVerbose, TEXT("LayeredMove queued move (%s)"), *Move->ToSimpleString());
	}
}

TArray<TSharedPtr<FLayeredMoveBase>> FLayeredMoveGroup::GenerateActiveMoves(const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard)
{
	const float SimStartTimeMs		= TimeStep.BaseSimTimeMs;
	const float SimTimeAfterTickMs	= SimStartTimeMs + TimeStep.StepMs;

	FlushMoveArrays(MoverComp, SimBlackboard, SimStartTimeMs);

	return ActiveLayeredMoves;
}

void FLayeredMoveGroup::NetSerialize(FArchive& Ar, uint8 MaxNumMovesToSerialize/* = MAX_uint8*/)
{
	// TODO: Warn if some sources will be dropped
	const uint8 NumActiveMovesToSerialize = FMath::Min<int32>(ActiveLayeredMoves.Num(), MaxNumMovesToSerialize);
	const uint8 NumQueuedMovesToSerialize = NumActiveMovesToSerialize < MaxNumMovesToSerialize ? MaxNumMovesToSerialize - NumActiveMovesToSerialize : 0;
	NetSerializeLayeredMovesArray(Ar, ActiveLayeredMoves, NumActiveMovesToSerialize);
	NetSerializeLayeredMovesArray(Ar, QueuedLayeredMoves, NumQueuedMovesToSerialize);
}


FLayeredMoveGroup& FLayeredMoveGroup::operator=(const FLayeredMoveGroup& Other)
{
	// Perform deep copy of this Group
	if (this != &Other)
	{
		// Deep copy active moves
		ActiveLayeredMoves.Empty(Other.ActiveLayeredMoves.Num());
		for (int i = 0; i < Other.ActiveLayeredMoves.Num(); ++i)
		{
			if (Other.ActiveLayeredMoves[i].IsValid())
			{
				FLayeredMoveBase* CopyOfSourcePtr = Other.ActiveLayeredMoves[i]->Clone();
				ActiveLayeredMoves.Add(TSharedPtr<FLayeredMoveBase>(CopyOfSourcePtr));
			}
			else
			{
				UE_LOG(LogMover, Warning, TEXT("FLayeredMoveGroup::operator= trying to copy invalid Other Layered Move"));
			}
		}

		// Deep copy queued moves
		QueuedLayeredMoves.Empty(Other.QueuedLayeredMoves.Num());
		for (int i = 0; i < Other.QueuedLayeredMoves.Num(); ++i)
		{
			if (Other.QueuedLayeredMoves[i].IsValid())
			{
				FLayeredMoveBase* CopyOfSourcePtr = Other.QueuedLayeredMoves[i]->Clone();
				QueuedLayeredMoves.Add(TSharedPtr<FLayeredMoveBase>(CopyOfSourcePtr));
			}
			else
			{
				UE_LOG(LogMover, Warning, TEXT("FLayeredMoveGroup::operator= trying to copy invalid Other Layered Move"));
			}
		}
	}

	return *this;
}

bool FLayeredMoveGroup::operator==(const FLayeredMoveGroup& Other) const
{
	// Deep move-by-move comparison
	if (ActiveLayeredMoves.Num() != Other.ActiveLayeredMoves.Num())
	{
		return false;
	}
	if (QueuedLayeredMoves.Num() != Other.QueuedLayeredMoves.Num())
	{
		return false;
	}


	for (int32 i = 0; i < ActiveLayeredMoves.Num(); ++i)
	{
		if (ActiveLayeredMoves[i].IsValid() == Other.ActiveLayeredMoves[i].IsValid())
		{
			if (ActiveLayeredMoves[i].IsValid())
			{
				// TODO: Implement deep equality checks
				// 				if (!ActiveLayeredMoves[i]->MatchesAndHasSameState(Other.ActiveLayeredMoves[i].Get()))
				// 				{
				// 					return false; // They're valid and don't match/have same state
				// 				}
			}
		}
		else
		{
			return false; // Mismatch in validity
		}
	}
	for (int32 i = 0; i < QueuedLayeredMoves.Num(); ++i)
	{
		if (QueuedLayeredMoves[i].IsValid() == Other.QueuedLayeredMoves[i].IsValid())
		{
			if (QueuedLayeredMoves[i].IsValid())
			{
				// TODO: Implement deep equality checks
				// 				if (!QueuedLayeredMoves[i]->MatchesAndHasSameState(Other.QueuedLayeredMoves[i].Get()))
				// 				{
				// 					return false; // They're valid and don't match/have same state
				// 				}
			}
		}
		else
		{
			return false; // Mismatch in validity
		}
	}
	return true;
}

bool FLayeredMoveGroup::operator!=(const FLayeredMoveGroup& Other) const
{
	return !(FLayeredMoveGroup::operator==(Other));
}

void FLayeredMoveGroup::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const TSharedPtr<FLayeredMoveBase>& LayeredMove : ActiveLayeredMoves)
	{
		if (LayeredMove.IsValid())
		{
			LayeredMove->AddReferencedObjects(Collector);
		}
	}

	for (const TSharedPtr<FLayeredMoveBase>& LayeredMove : QueuedLayeredMoves)
	{
		if (LayeredMove.IsValid())
		{
			LayeredMove->AddReferencedObjects(Collector);
		}
	}
}

FString FLayeredMoveGroup::ToSimpleString() const
{
	return FString::Printf(TEXT("FLayeredMoveGroup. Active: %i Queued: %i"), ActiveLayeredMoves.Num(), QueuedLayeredMoves.Num());
}

TArray<TSharedPtr<FLayeredMoveBase>>::TConstIterator FLayeredMoveGroup::GetActiveMovesIterator() const
{
	return ActiveLayeredMoves.CreateConstIterator();
}

TArray<TSharedPtr<FLayeredMoveBase>>::TConstIterator FLayeredMoveGroup::GetQueuedMovesIterator() const
{
	return QueuedLayeredMoves.CreateConstIterator();
}

void FLayeredMoveGroup::FlushMoveArrays(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, float CurrentSimTimeMs)
{
	bool bResidualVelocityOverridden = false;
	bool bClampVelocityOverridden = false;
	// Remove any finished moves
	ActiveLayeredMoves.RemoveAll([MoverComp, SimBlackboard, CurrentSimTimeMs, &bResidualVelocityOverridden, &bClampVelocityOverridden, this]
		(const TSharedPtr<FLayeredMoveBase>& Move)
		{
			if (Move.IsValid())
			{
				if (Move->IsFinished(CurrentSimTimeMs))
				{
					GatherResidualVelocitySettings(Move, bResidualVelocityOverridden, bClampVelocityOverridden);
					Move->EndMove(MoverComp, SimBlackboard, CurrentSimTimeMs);
					return true;
				}
			}
			else
			{
				return true;	
			}

			return false;
		});

	// Make any queued moves active
	for (TSharedPtr<FLayeredMoveBase>& QueuedMove : QueuedLayeredMoves)
	{
		ActiveLayeredMoves.Add(QueuedMove);
		QueuedMove->StartMove(MoverComp, SimBlackboard, CurrentSimTimeMs);
	}

	QueuedLayeredMoves.Empty();
}

void FLayeredMoveGroup::GatherResidualVelocitySettings(const TSharedPtr<FLayeredMoveBase>& Move, bool& bResidualVelocityOverridden, bool& bClampVelocityOverridden)
{
	if (Move->FinishVelocitySettings.FinishVelocityMode == ELayeredMoveFinishVelocityMode::SetVelocity)
	{
		if (Move->MixMode == EMoveMixMode::OverrideVelocity)
		{
			if (bResidualVelocityOverridden)
			{
				UE_LOG(LogMover, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bResidualVelocityOverridden = true;
			ResidualVelocity = Move->FinishVelocitySettings.SetVelocity;
		}
		else if (Move->MixMode == EMoveMixMode::AdditiveVelocity && !bResidualVelocityOverridden)
		{
			ResidualVelocity += Move->FinishVelocitySettings.SetVelocity;
		}
		else if (Move->MixMode == EMoveMixMode::OverrideAll)
		{
			if (bResidualVelocityOverridden)
			{
				UE_LOG(LogMover, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bResidualVelocityOverridden = true;
			ResidualVelocity = Move->FinishVelocitySettings.SetVelocity;
		}
		else
		{
			check(0);	// unhandled case
		}
		bApplyResidualVelocity = true;
	}
	else if (Move->FinishVelocitySettings.FinishVelocityMode == ELayeredMoveFinishVelocityMode::ClampVelocity)
	{
		if (Move->MixMode == EMoveMixMode::OverrideVelocity)
		{
			if (bClampVelocityOverridden)
			{
				UE_LOG(LogMover, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bClampVelocityOverridden = true;
			ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
		}
		else if (Move->MixMode == EMoveMixMode::AdditiveVelocity && !bClampVelocityOverridden)
		{
			if (ResidualClamping < 0)
			{
				ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
			}
			// No way to really add clamping so we instead apply it if it was smaller
			else if (ResidualClamping > Move->FinishVelocitySettings.ClampVelocity)
			{
				ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
			}
		}
		else if (Move->MixMode == EMoveMixMode::OverrideAll)
		{
			if (bClampVelocityOverridden)
			{
				UE_LOG(LogMover, Log, TEXT("Multiple LayeredMove residual settings have a MixMode that overrides. Only one will take effect."));
			}

			bClampVelocityOverridden = true;
			ResidualClamping = Move->FinishVelocitySettings.ClampVelocity;
		}
		else
		{
			check(0);	// unhandled case
		}
	}
}

struct FLayeredMoveDeleter
{
	FORCEINLINE void operator()(FLayeredMoveBase* Object) const
	{
		check(Object);
		UScriptStruct* ScriptStruct = Object->GetScriptStruct();
		check(ScriptStruct);
		ScriptStruct->DestroyStruct(Object);
		FMemory::Free(Object);
	}
};


/* static */ void FLayeredMoveGroup::NetSerializeLayeredMovesArray(FArchive& Ar, TArray< TSharedPtr<FLayeredMoveBase> >& LayeredMovesArray, uint8 MaxNumLayeredMovesToSerialize /*=MAX_uint8*/)
{
	uint8 NumMovesToSerialize;
	if (Ar.IsSaving())
	{
		UE_CLOG(LayeredMovesArray.Num() > MaxNumLayeredMovesToSerialize, LogMover, Warning, TEXT("Too many Layered Moves (%d!) to net serialize. Clamping to %d"),
			LayeredMovesArray.Num(), MaxNumLayeredMovesToSerialize);

		NumMovesToSerialize = FMath::Min<int32>(LayeredMovesArray.Num(), MaxNumLayeredMovesToSerialize);
	}

	Ar << NumMovesToSerialize;

	if (Ar.IsLoading())
	{
		LayeredMovesArray.SetNumZeroed(NumMovesToSerialize);
	}

	for (int32 i = 0; i < NumMovesToSerialize && !Ar.IsError(); ++i)
	{
		TCheckedObjPtr<UScriptStruct> ScriptStruct = LayeredMovesArray[i].IsValid() ? LayeredMovesArray[i]->GetScriptStruct() : nullptr;
		UScriptStruct* ScriptStructLocal = ScriptStruct.Get();
		Ar << ScriptStruct;

		if (ScriptStruct.IsValid())
		{
			// Restrict replication to derived classes of FLayeredMoveBase for security reasons:
			// If FLayeredMoveGroup is replicated through a Server RPC, we need to prevent clients from sending us
			// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
			// for invalid structs. All provided sources are direct children of FLayeredMoveBase and we never expect to have deep hierarchies
			// so this should not be too costly
			bool bIsDerivedFromBase = false;
			UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
			while (CurrentSuperStruct)
			{
				if (CurrentSuperStruct == FLayeredMoveBase::StaticStruct())
				{
					bIsDerivedFromBase = true;
					break;
				}
				CurrentSuperStruct = CurrentSuperStruct->GetSuperStruct();
			}

			if (bIsDerivedFromBase)
			{
				if (Ar.IsLoading())
				{
					if (LayeredMovesArray[i].IsValid() && ScriptStructLocal == ScriptStruct.Get())
					{
						// What we have locally is the same type as we're being serialized into, so we don't need to
						// reallocate - just use existing structure
					}
					else
					{
						// For now, just reset/reallocate the data when loading.
						// Longer term if we want to generalize this and use it for property replication, we should support
						// only reallocating when necessary
						FLayeredMoveBase* NewMove = (FLayeredMoveBase*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
						ScriptStruct->InitializeStruct(NewMove);

						LayeredMovesArray[i] = TSharedPtr<FLayeredMoveBase>(NewMove, FLayeredMoveDeleter());
					}
				}

				LayeredMovesArray[i]->NetSerialize(Ar);
			}
			else
			{
				UE_LOG(LogMover, Error, TEXT("FLayeredMoveGroup::NetSerialize: ScriptStruct not derived from FLayeredMoveBase attempted to serialize."));
				Ar.SetError();
				break;
			}
		}
		else if (ScriptStruct.IsError())
		{
			UE_LOG(LogMover, Error, TEXT("FLayeredMoveGroup::NetSerialize: Invalid ScriptStruct serialized."));
			Ar.SetError();
			break;
		}
	}
}

void FLayeredMoveGroup::ResetResidualVelocity()
{
	bApplyResidualVelocity = false;
	ResidualVelocity = FVector::ZeroVector;
	ResidualClamping = -1.f;
}

