// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundReceiveNode.h"

namespace Metasound
{
	namespace ReceiveNodeInfo
	{
		FNodeClassName GetClassNameForDataType(const FName& InDataTypeName)
		{
			return FNodeClassName { "Receive", InDataTypeName, FName() };
		}

		int32 GetCurrentMajorVersion() { return 1; }

		int32 GetCurrentMinorVersion() { return 0; }
	}
}
