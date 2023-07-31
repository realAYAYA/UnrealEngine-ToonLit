// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FCbObject;
class FCbObjectView;


namespace UE::RemoteExecution
{
	class IRemoteMessage
	{
	public:
		/** Virtual destructor */
		virtual ~IRemoteMessage() {}

		virtual FCbObject Save() const = 0;
		virtual void Load(const FCbObjectView& CbObjectView) = 0;
	};
}
