// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConcertPackageEvents.h"

namespace UE::ConcertSyncCore::ConcertPackageEvents
{
	FConcertBeginSendPackageDelegate& OnLocalBeginSendPackage()
	{
		static FConcertBeginSendPackageDelegate Instance;
		return Instance;
	}
	
	FConcertFinishSendPackageDelegate& OnLocalFinishSendPackage()
	{
		static FConcertFinishSendPackageDelegate Instance;
		return Instance;
	}

	FConcertBeginSendPackageDelegate& OnRemoteBeginSendPackage()
	{
		static FConcertBeginSendPackageDelegate Instance;
		return Instance;
	}

	FConcertFinishSendPackageDelegate& OnRemoteFinishSendPackage()
	{
		static FConcertFinishSendPackageDelegate Instance;
		return Instance;
	}

	FConcertRejectSendPackageDelegate& OnRejectRemoteSendPackage()
	{
		static FConcertRejectSendPackageDelegate Instance;
		return Instance;
	}
}