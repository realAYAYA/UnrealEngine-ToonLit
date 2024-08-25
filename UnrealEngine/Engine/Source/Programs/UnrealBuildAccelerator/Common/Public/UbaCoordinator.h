// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaBase.h"

namespace uba
{
	struct CoordinatorCreateInfo
	{
		const tchar* workDir = nullptr;
		const tchar* binariesDir = nullptr;

		// TODO: This is very horde specific.. maybe all these parameters should be a string or something
		const tchar* uri = nullptr;
		const tchar* pool = nullptr;
		const tchar* oidc = nullptr;
		u32 maxCoreCount = 0;

		bool logging = false;
	};

	class Coordinator
	{
	public:
		using AddClientCallback = bool(void* userData, const tchar* ip, u16 port);
		virtual void SetAddClientCallback(AddClientCallback* callback, void* userData) = 0;
		virtual void SetTargetCoreCount(u32 count) = 0;
	};

	// This is how the function signatures creating/destroying the coordinator module needs to exported
	using UbaCreateCoordinatorFunc = Coordinator*(const CoordinatorCreateInfo& info);
	using UbaDestroyCoordinatorFunc = void(Coordinator* coordinator);
}