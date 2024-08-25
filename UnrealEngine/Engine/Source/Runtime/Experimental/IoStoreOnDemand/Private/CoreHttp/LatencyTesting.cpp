// Copyright Epic Games, Inc. All Rights Reserved.

#include "LatencyTesting.h"

#include "Client.h"
#include "Containers/StringConv.h"
#include "Containers/StringView.h"
#include "HAL/PlatformTime.h"
#include "IO/IoStoreOnDemand.h"
#include "Misc/StringBuilder.h"
#include "Templates/Function.h"

namespace UE::IO::IAS::HTTP
{

void LatencyTest(FStringView InUrl, FStringView InPath, uint32 InTimeOutMs, TArrayView<int32> OutResults)
{
	auto AnsiUrl = StringCast<ANSICHAR>(InUrl.GetData(), InUrl.Len());

	FConnectionPool::FParams PoolParams;
	PoolParams.SetHostFromUrl(AnsiUrl);
	PoolParams.ConnectionCount = 1;
	FConnectionPool Pool(PoolParams);

	TAnsiStringBuilder<256> AnsiPath;
	if (!InPath.StartsWith(TEXT('/')))
	{
		AnsiPath << '/';
	}
	AnsiPath << InPath;

	FEventLoop Loop;

	for (int32& Result : OutResults)
	{
		bool Ok = false;

		FRequest Request = Loop.Request("HEAD", AnsiPath, Pool);
		Loop.Send(MoveTemp(Request), [&](const FTicketStatus& Status)
			{
				if (Status.GetId() != FTicketStatus::EId::Response)
					return;

				const FResponse& Response = Status.GetResponse();
				Ok = (Response.GetStatus() == EStatusCodeClass::Successful);
			});

		uint64 Cycles = FPlatformTime::Cycles64();
		while (Loop.Tick(InTimeOutMs) != 0);
		Cycles = FPlatformTime::Cycles64() - Cycles;

		Result = Ok ? int32(Cycles) : -1;
	}

	int64 Freq = int64(1.0 / FPlatformTime::GetSecondsPerCycle());
	for (int32& Result : OutResults)
	{
		if (Result == -1)
			continue;

		Result = int32((int64(Result) * 1000) / Freq);
	}
	
	TAnsiStringBuilder<512> ConnectionDesc;
	Pool.Describe(ConnectionDesc);
	UE_LOG(LogIas, Log, TEXT("Testing endpoint %s"), ANSI_TO_TCHAR(ConnectionDesc.ToString()));
}

} // namespace UE::IO::IAS::HTTP
