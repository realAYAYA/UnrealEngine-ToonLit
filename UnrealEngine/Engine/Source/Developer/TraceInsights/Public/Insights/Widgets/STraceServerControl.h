// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <atomic>
#include "CoreFwd.h"
#include "HAL/CriticalSection.h"
#include "Templates/UniquePtr.h"
#include "Trace/StoreClient.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

class FMenuBuilder;

class TRACEINSIGHTS_API STraceServerControl
{
public:
	STraceServerControl(const TCHAR* Host, uint32 Port = 0, FName StyleSet = NAME_None);
	~STraceServerControl();

	void MakeMenu(FMenuBuilder& Builder);

private:
	enum class EState : uint8
	{
		NotConnected,
		Connecting,
		Connected,
		CheckStatus,
		Command
	};

	bool ChangeState(EState Expected, EState ChangeTo, uint32 Attempts = 1);

	void TriggerStatusUpdate();
	void UpdateStatus();
	void ResetStatus();

	bool CanServerBeStarted() const { return !bIsCancelRequested && bIsLocalHost && State.load(std::memory_order_relaxed) == EState::NotConnected; }
	bool CanServerBeStopped() const { return !bIsCancelRequested && bIsLocalHost && State.load(std::memory_order_relaxed) == EState::Connected; }
	bool AreControlsEnabled() const { return !bIsCancelRequested && bIsLocalHost && State.load(std::memory_order_relaxed) == EState::Connected; }
	bool IsSponsored() const { return bSponsored.load(std::memory_order_relaxed); }

	void OnStart_Clicked();
	void OnStop_Clicked();
	void OnSponsored_Changed();

	std::atomic<EState> State = EState::NotConnected;

	std::atomic<bool> bCanServerBeStarted = false;
	std::atomic<bool> bCanServerBeStopped = false;
	std::atomic<bool> bSponsored = false;
	std::atomic<bool> bIsCancelRequested = false;

	FCriticalSection AsyncTaskLock;
	FCriticalSection StringsLock;
	FString StatusString;

	FString Host;
	uint32 Port = 0;
	FName StyleSet;
	bool bIsLocalHost = false;
	TUniquePtr<UE::Trace::FStoreClient> Client;

	friend const TCHAR* LexState(EState);
};
