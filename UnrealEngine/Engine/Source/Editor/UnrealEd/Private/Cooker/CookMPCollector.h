// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Cooker/CookTypes.h"
#include "HAL/Platform.h"
#include "Misc/Guid.h"
#include "Templates/RefCounting.h"

class FCbObject;
class FCbObjectView;
class FCbWriter;
class ITargetPlatform;

namespace UE::Cook { class FCookWorkerServer; }

namespace UE::Cook
{

/**
 * Interface used during cooking to send data collected from save/load on a remote CookWorker
 * to the Director for aggregation into files saves at the end of the cook. 
 */
class IMPCollector : public FRefCountBase
{
public:
	class FClientContext
	{
	public:
		TConstArrayView<const ITargetPlatform*> GetPlatforms() const { return Platforms; }
		bool IsFlush()  const { return bFlush; }

		void AddMessage(FCbObject Object);

		uint8 PlatformToInt(const ITargetPlatform* Platform) const;
		const ITargetPlatform* IntToPlatform(uint8 PlatformAsInt) const;

	private:
		TConstArrayView<const ITargetPlatform*> Platforms;
		TArray<FCbObject> Messages;
		bool bFlush = false;

		friend class FCookWorkerClient;
	};

	class FServerContext
	{
	public:
		TConstArrayView<const ITargetPlatform*> GetPlatforms() { return Platforms; }

		uint8 PlatformToInt(const ITargetPlatform* Platform) const;
		const ITargetPlatform* IntToPlatform(uint8 PlatformAsInt) const;
		FWorkerId GetWorkerId() const { return WorkerId; }

	private:
		TConstArrayView<const ITargetPlatform*> Platforms;
		FWorkerId WorkerId;

		friend class FCookWorkerServer;
	};

	virtual ~IMPCollector() {}

	virtual FGuid GetMessageType() const = 0;
	virtual const TCHAR* GetDebugName() const = 0;

	virtual void ClientTick(FClientContext& Context) {}
	virtual void ReceiveMessage(FServerContext& Context, FCbObjectView Message);
};

}