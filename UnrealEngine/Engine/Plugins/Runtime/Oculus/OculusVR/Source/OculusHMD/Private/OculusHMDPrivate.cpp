// Copyright Epic Games, Inc. All Rights Reserved.

#include "OculusHMDPrivate.h"
#include "RHICommandList.h"
#include "RenderingThread.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// Utility functions
//-------------------------------------------------------------------------------------------------

bool InGameThread()
{
	if (GIsGameThreadIdInitialized)
	{
		return FPlatformTLS::GetCurrentThreadId() == GGameThreadId;
	}
	else
	{
		return true;
	}
}


bool InRenderThread()
{
	if (GIsThreadedRendering && !GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed))
	{
		return IsInActualRenderingThread();
	}
	else
	{
		return InGameThread();
	}
}


bool InRHIThread()
{
	if (GIsThreadedRendering && !GIsRenderingThreadSuspended.Load(EMemoryOrder::Relaxed))
	{
		if (IsRHIThreadRunning())
		{
			if (IsInRHIThread())
			{
				return true;
			}

			if (IsInActualRenderingThread())
			{
				return GetImmediateCommandList_ForRenderCommand().Bypass();
			}

			return false;
		}
		else
		{
			return IsInActualRenderingThread();
		}
	}
	else
	{
		return InGameThread();
	}
}

bool ConvertPose_Internal(const FPose& InPose, FPose& OutPose, const FQuat BaseOrientation, const FVector BaseOffset, float WorldToMetersScale)
{
	// apply base orientation correction
	OutPose.Orientation = BaseOrientation.Inverse() * InPose.Orientation;
	OutPose.Orientation.Normalize();

	// correct position according to BaseOrientation and BaseOffset.
	OutPose.Position = (InPose.Position - BaseOffset) * WorldToMetersScale;
	OutPose.Position = BaseOrientation.Inverse().RotateVector(OutPose.Position);

	return true;
}

bool ConvertPose_Internal(const ovrpPosef& InPose, FPose& OutPose, const FQuat BaseOrientation, const FVector BaseOffset, float WorldToMetersScale)
{
	return ConvertPose_Internal(FPose(ToFQuat(InPose.Orientation), ToFVector(InPose.Position)), OutPose, BaseOrientation, BaseOffset, WorldToMetersScale);
}

bool ConvertPose_Internal(const FPose& InPose, ovrpPosef& OutPose, const FQuat BaseOrientation, const FVector BaseOffset, float WorldToMetersScale)
{
	OutPose.Orientation = ToOvrpQuatf(BaseOrientation * InPose.Orientation);
	OutPose.Position = ToOvrpVector3f(BaseOrientation.RotateVector(InPose.Position) / WorldToMetersScale + BaseOffset);
	return true;
}

#if OCULUS_HMD_SUPPORTED_PLATFORMS
bool IsOculusServiceRunning()
{
#if PLATFORM_WINDOWS
	HANDLE hEvent = ::OpenEventW(SYNCHRONIZE, 0 /*FALSE*/, L"OculusHMDConnected");

	if (!hEvent)
	{
		return false;
	}

	::CloseHandle(hEvent);
#endif

	return true;
}


bool IsOculusHMDConnected()
{
#if PLATFORM_WINDOWS
	HANDLE hEvent = ::OpenEventW(SYNCHRONIZE, 0 /*FALSE*/, L"OculusHMDConnected");

	if (!hEvent)
	{
		return false;
	}

	uint32 dwWait = ::WaitForSingleObject(hEvent, 0);

	::CloseHandle(hEvent);

	if (WAIT_OBJECT_0 != dwWait)
	{
		return false;
	}
#endif

	return true;
}
#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

} // namespace OculusHMD
