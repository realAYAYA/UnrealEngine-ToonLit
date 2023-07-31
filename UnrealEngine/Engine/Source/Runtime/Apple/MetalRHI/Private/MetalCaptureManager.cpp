// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalCaptureManager.h"
#include "MetalCommandQueue.h"
#include "capture_manager.hpp"

bool GMetalSupportsCaptureManager = false;

FMetalCaptureManager::FMetalCaptureManager(mtlpp::Device InDevice, FMetalCommandQueue& InQueue)
: Device(InDevice)
, Queue(InQueue)
{
	MTLPP_IF_AVAILABLE(10.13, 11.0, 11.0)
	{
		GMetalSupportsCaptureManager = true;
		
		mtlpp::CaptureManager Manager = mtlpp::CaptureManager::SharedCaptureManager();
		Manager.SetDefaultCaptureScope(Manager.NewCaptureScopeWithDevice(Device));
		Manager.GetDefaultCaptureScope().SetLabel(@"1 Frame");
		
		FMetalCaptureScope DefaultScope;
		DefaultScope.MTLScope = Manager.GetDefaultCaptureScope();
		DefaultScope.Type = EMetalCaptureTypePresent;
		DefaultScope.StepCount = 1;
		DefaultScope.LastTrigger = 0;
		ActiveScopes.Add(DefaultScope);
		DefaultScope.MTLScope.BeginScope();
		
		uint32 PresentStepCounts[] = {2, 5, 10, 15, 30, 60, 90, 120};
		for (uint32 i = 0; i < (sizeof(PresentStepCounts) / sizeof(uint32)); i++)
		{
			FMetalCaptureScope Scope;
			Scope.MTLScope = Manager.NewCaptureScopeWithDevice(Device);
			Scope.MTLScope.SetLabel(FString::Printf(TEXT("%u Frames"), PresentStepCounts[i]).GetNSString());
			Scope.Type = EMetalCaptureTypePresent;
			Scope.StepCount = PresentStepCounts[i];
			Scope.LastTrigger = 0;
			ActiveScopes.Add(Scope);
			Scope.MTLScope.BeginScope();
		}
	}
}

FMetalCaptureManager::~FMetalCaptureManager()
{
}

void FMetalCaptureManager::PresentFrame(uint32 FrameNumber)
{
	if (GMetalSupportsCaptureManager)
	{
		for (FMetalCaptureScope& Scope : ActiveScopes)
		{
			uint32 Diff = 0;
			if (FrameNumber > Scope.LastTrigger)
			{
				Diff = FrameNumber - Scope.LastTrigger;
			}
			else
			{
				Diff = (UINT32_MAX - Scope.LastTrigger) + FrameNumber;
			}
			
			if (Diff >= Scope.StepCount)
			{
				Scope.MTLScope.EndScope();
				Scope.MTLScope.BeginScope();
				Scope.LastTrigger = FrameNumber;
			}
		}
	}
	else
	{
		Queue.InsertDebugCaptureBoundary();
	}
}

void FMetalCaptureManager::BeginCapture(void)
{
	if (GMetalSupportsCaptureManager)
	{
		mtlpp::CaptureManager::SharedCaptureManager().StartCaptureWithDevice(Device);
	}
}

void FMetalCaptureManager::EndCapture(void)
{
	if (GMetalSupportsCaptureManager)
	{
		mtlpp::CaptureManager::SharedCaptureManager().StopCapture();
	}
}

