// Copyright Epic Games, Inc. All Rights Reserved.

#include "QosModule.h"
#include "QosInterface.h"


IMPLEMENT_MODULE(FQosModule, Qos);

DEFINE_LOG_CATEGORY(LogQos);

void FQosModule::StartupModule()
{
}

void FQosModule::ShutdownModule()
{
	ensure(!QosInterface.IsValid() || QosInterface.IsUnique());
	QosInterface = nullptr;
}

TSharedRef<FQosInterface> FQosModule::GetQosInterface()
{
	if (!QosInterface.IsValid())
	{
		QosInterface = MakeShareable(new FQosInterface());
		QosInterface->Init();
	}
	return QosInterface.ToSharedRef();
}

bool FQosModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	bool bWasHandled = false;

	// Ignore any execs that don't start with Qos
	if (FParse::Command(&Cmd, TEXT("Qos")))
	{
		if (FParse::Command(&Cmd, TEXT("Ping")))
		{
			GetQosInterface()->BeginQosEvaluation(InWorld, nullptr, FSimpleDelegate::CreateLambda([this]()
			{
				UE_LOG(LogQos, Log, TEXT("ExecQosPingComplete!"));
				GetQosInterface()->DumpRegionStats();
			}));
			bWasHandled = true;
		}
		else if (FParse::Command(&Cmd, TEXT("DumpRegions")))
		{
			GetQosInterface()->DumpRegionStats();
			bWasHandled = true;
		}
	}

	return bWasHandled;
}


