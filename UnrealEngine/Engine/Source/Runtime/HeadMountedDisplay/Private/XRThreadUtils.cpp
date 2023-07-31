// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRThreadUtils.h"
#include "RenderingThread.h"
#include "RHICommandList.h"

void ExecuteOnRenderThread_DoNotWait(const TFunction<void()>& Function)
{
	check(IsInGameThread());

	ENQUEUE_RENDER_COMMAND(ExecuteOnRenderThread)([Function](FRHICommandListImmediate& /*RHICmdList*/)
	{
		Function();
	});
}

void ExecuteOnRenderThread_DoNotWait(const TFunction<void(FRHICommandListImmediate&)>& Function)
{
	check(IsInGameThread());

	ENQUEUE_RENDER_COMMAND(ExecuteOnRenderThread)([Function](FRHICommandListImmediate& RHICmdList)
	{
		Function(RHICmdList);
	});
}

void ExecuteOnRenderThread(const TFunctionRef<void()>& Function)
{
	check(IsInGameThread());

	ENQUEUE_RENDER_COMMAND(ExecuteOnRenderThread)([Function](FRHICommandListImmediate& /*RHICmdList*/)
	{
		Function();
	});
	FlushRenderingCommands();
}

void ExecuteOnRenderThread(const TFunctionRef<void(FRHICommandListImmediate&)>& Function)
{
	check(IsInGameThread());

	ENQUEUE_RENDER_COMMAND(ExecuteOnRenderThread)([Function](FRHICommandListImmediate& RHICmdList)
	{
		Function(RHICmdList);
	});
	FlushRenderingCommands();
}

namespace
{

	FORCEINLINE void Invoke_Impl(const TFunction<void()>& Function, FRHICommandListImmediate& RHICmdList)
	{
		Function();
	}
	FORCEINLINE void Invoke_Impl(const TFunctionRef<void()>& Function, FRHICommandListImmediate& RHICmdList)
	{
		Function();
	}
	FORCEINLINE void Invoke_Impl(const TFunction<void(FRHICommandListImmediate&)>& Function, FRHICommandListImmediate& RHICmdList)
	{
		Function(RHICmdList);
	}
	FORCEINLINE void Invoke_Impl(const TFunctionRef<void(FRHICommandListImmediate&)>& Function, FRHICommandListImmediate& RHICmdList)
	{
		Function(RHICmdList);
	}

	template <typename T>
	struct FXRFunctionWrapperRHICommand final : public FRHICommand<FXRFunctionWrapperRHICommand<T>>
	{
		T Function;

		FXRFunctionWrapperRHICommand(const T& InFunction)
			: Function(InFunction)
		{
		}

		void Execute(FRHICommandListBase& RHICmdList)
		{
			Invoke_Impl(Function, *static_cast<FRHICommandListImmediate*>(&RHICmdList));
		}
	};


	template<typename T>
	FORCEINLINE bool ExecuteOnRHIThread_Impl(const T& Function, bool bFlush)
	{
		check(IsInRenderingThread() || IsInRHIThread());

		FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();

		if (IsRHIThreadRunning() && !IsInRHIThread() && !RHICmdList.Bypass())
		{
			ALLOC_COMMAND_CL(RHICmdList, FXRFunctionWrapperRHICommand<T>)(Function);
			if (bFlush)
			{
				RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
			}
			return true;
		}
		else
		{
			Invoke_Impl(Function, RHICmdList);
			return false;
		}
	}
}

bool ExecuteOnRHIThread_DoNotWait(const TFunction<void()>& Function)
{
	return ExecuteOnRHIThread_Impl(Function, false);
}

bool ExecuteOnRHIThread_DoNotWait(const TFunction<void(FRHICommandListImmediate&)>& Function)
{
	return ExecuteOnRHIThread_Impl(Function, false);
}

void ExecuteOnRHIThread(const TFunctionRef<void()>& Function)
{
	ExecuteOnRHIThread_Impl(Function, true);
}

void ExecuteOnRHIThread(const TFunctionRef<void(FRHICommandListImmediate&)>& Function)
{
	ExecuteOnRHIThread_Impl(Function, true);
}


