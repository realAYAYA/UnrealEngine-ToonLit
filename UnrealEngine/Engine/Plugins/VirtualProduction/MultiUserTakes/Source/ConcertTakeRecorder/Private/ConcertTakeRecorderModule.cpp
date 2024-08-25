// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertTakeRecorderModule.h"

#include "ConcertTakeRecorderManager.h"
#include "ConcertTakeRecorderStyle.h"

LLM_DEFINE_TAG(Concert_ConcertTakeRecorder);

namespace UE::ConcertTakeRecorder
{
	void FConcertTakeRecorderModule::StartupModule()
	{
		LLM_SCOPE_BYTAG(Concert_ConcertTakeRecorder);
		
		FConcertTakeRecorderStyle::Initialize();
		ConcertManager = MakeUnique<FConcertTakeRecorderManager>();
	}
	
	void FConcertTakeRecorderModule::ShutdownModule()
	{
		ConcertManager.Reset();
		FConcertTakeRecorderStyle::Shutdown();
	}
}

IMPLEMENT_MODULE(UE::ConcertTakeRecorder::FConcertTakeRecorderModule, ConcertTakeRecorder);
