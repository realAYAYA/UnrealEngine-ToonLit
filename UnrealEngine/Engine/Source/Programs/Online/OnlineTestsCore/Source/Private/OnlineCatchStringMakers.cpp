// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnlineCatchStringMakers.h"

#include "Containers/StringConv.h"
#include "Online/OnlineError.h"

namespace Catch
{
	std::string StringMaker<UE::Online::FOnlineError>::convert(UE::Online::FOnlineError const& Error)
	{
		auto ErrorRepresentation = StringCast<UTF8CHAR>(*Error.GetLogString());
		return std::string((const std::string::value_type*)ErrorRepresentation.Get());
	}
}

