// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnsyncCmdQuery.h"
#include "UnsyncHttp.h"
#include "UnsyncThread.h"
#include "UnsyncUtil.h"

#include <algorithm>
#include <json11.hpp>
#include <float.h>

namespace unsync {

struct FMirrorInfo
{
	std::string Name;
	std::string Address;
	uint16		Port = UNSYNC_DEFAULT_PORT;
	double		Ping = 0;
};
using FMirrorInfoResult = TResult<std::vector<FMirrorInfo>>;

// Runs a basic HTTP request against the remote server and returns the time it took to get the response, -1 if connection could not be
// established.
static double
RunHttpPing(std::string_view Address, uint16 Port)
{
	FTimePoint		TimeBegin = TimePointNow();
	FHttpConnection Connection(Address, Port);
	FHttpRequest	Request;
	Request.Url				   = "/api/v1/hello";
	Request.Method			   = EHttpMethod::GET;
	FHttpResponse PingResponse = HttpRequest(Connection, Request);
	FTimePoint	  TimeEnd	   = TimePointNow();

	if (PingResponse.Success())
	{
		return DurationSec(TimeBegin, TimeEnd);
	}
	else
	{
		return -1;
	}
}

FMirrorInfoResult
RunQueryMirrors(const FRemoteDesc& RemoteDesc)
{
	const char* Url = "/api/v1/mirrors";
	FHttpResponse Response = HttpRequest(RemoteDesc, EHttpMethod::GET, Url);

	if (!Response.Success())
	{
		return HttpError(Url, Response.Code);
	}

	using namespace json11;
	std::string JsonString = std::string(Response.AsStringView());

	std::string JsonErrorString;
	Json		JsonObject = Json::parse(JsonString, JsonErrorString);

	if (!JsonErrorString.empty())
	{
		return AppError(std::string("JSON parse error while getting server mirrors: ") + JsonErrorString);
	}

	std::vector<FMirrorInfo> Result;

	for (const auto& Elem : JsonObject.array_items())
	{
		FMirrorInfo Info;
		for (const auto& Field : Elem.object_items())
		{
			if (Field.first == "name")
			{
				Info.Name = Field.second.string_value();
			}
			else if (Field.first == "address")
			{
				Info.Address = Field.second.string_value();
			}
			else if (Field.first == "port")
			{
				int PortValue = Field.second.int_value();
				if (PortValue > 0 && PortValue < 65536)
				{
					Info.Port = uint16(PortValue);
				}
				else
				{
					UNSYNC_WARNING(L"Unexpected port value: %d", PortValue);
					Info.Port = 0;
				}
			}
		}
		Result.push_back(Info);
	}

	return ResultOk(Result);
}

int32
CmdQuery(const FCmdQueryOptions& Options)
{
	if (Options.Query == "mirrors")
	{
		FMirrorInfoResult MirrorsResult = RunQueryMirrors(Options.Remote);
		if (MirrorsResult.IsError())
		{
			LogError(MirrorsResult.GetError());
			return 1;
		}

		std::vector<FMirrorInfo> Mirrors = MirrorsResult.GetData();

		ParallelForEach(Mirrors.begin(), Mirrors.end(), [](FMirrorInfo& Mirror) {
			Mirror.Ping = RunHttpPing(Mirror.Address, Mirror.Port);
		});

		std::sort(Mirrors.begin(), Mirrors.end(), [](const FMirrorInfo& InA, const FMirrorInfo& InB) {
			double A = InA.Ping > 0 ? InA.Ping : FLT_MAX;
			double B = InB.Ping > 0 ? InB.Ping : FLT_MAX;
			return A < B;
		});

		LogPrintf(ELogLevel::Info, L"[\n");

		for (size_t I = 0; I < Mirrors.size(); ++I)
		{
			const FMirrorInfo& Mirror = Mirrors[I];

			LogPrintf(ELogLevel::Info,
					  L"  {\"address\":\"%hs\", \"port\":%d, \"ok\":%hs, \"ping\":%d, \"name\":\"%hs\"}%hs\n",
					  Mirror.Address.c_str(),
					  Mirror.Port,
					  Mirror.Ping > 0 ? "true" : "false",
					  int32(Mirror.Ping * 1000.0),
					  Mirror.Name.c_str(),
					  I + 1 == Mirrors.size() ? "" : ",");
		}

		LogPrintf(ELogLevel::Info, L"]\n");

		return 0;
	}
	else
	{
		UNSYNC_ERROR(L"Unknown query command");
		return 1;
	}
}

}  // namespace unsync
