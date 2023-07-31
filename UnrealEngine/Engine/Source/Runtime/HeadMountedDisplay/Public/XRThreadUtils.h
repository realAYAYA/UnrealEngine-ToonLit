// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/RunnableThread.h"
#include "RHI.h"
#include "Templates/Function.h"

class FRHICommandListImmediate;

/**
* Utility function for easily submitting TFunction to be run on the render thread. Must be invoked from the game thread.
* If rendering does not use a separate thread, the TFunction will be executed immediately, otherwise it will be added to the render thread task queue.
* @param Function - the Function to be invoked on the render thread.
* @return true if the function was queued, false if rendering does not use a separate thread, in which case, the function has already been executed.
*/
extern HEADMOUNTEDDISPLAY_API void ExecuteOnRenderThread_DoNotWait(const TFunction<void()>& Function);

/**
* Utility function for easily submitting TFunction to be run on the render thread. Must be invoked from the game thread.
* If rendering does not use a separate thread, the TFunction will be executed immediately, otherwise it will be added to the render thread task queue.
* @param Function - the Function to be invoked on the render thread. When executed, it will get the current FRHICommandList instance passed in as its sole argument.
* @return true if the function was queued, false if rendering does not use a separate thread, in which case, the function has already been executed.
*/
extern HEADMOUNTEDDISPLAY_API void ExecuteOnRenderThread_DoNotWait(const TFunction<void(FRHICommandListImmediate&)>& Function);

/**
* Utility function for easily running a TFunctionRef on the render thread. Must be invoked from the game thread.
* If rendering does not use a separate thread, the TFunction will be executed immediately, otherwise it will be added to the render thread task queue.
* This method will flush rendering commands meaning that the function will be executed before ExecuteOnRenderThread returns.
* @param Function - the Function to be invoked on the render thread.
* @return true if the function was queued, false if rendering does not use a separate thread, in which case, the function has already been executed.
*/
extern HEADMOUNTEDDISPLAY_API void ExecuteOnRenderThread(const TFunctionRef<void()>& Function);

/**
* Utility function for easily running a TFunctionRef on the render thread. Must be invoked from the game thread.
* If rendering does not use a separate thread, the TFunction will be executed immediately, otherwise it will be added to the render thread task queue.
* This method will flush rendering commands meaning that the function will be executed before ExecuteOnRenderThread returns.
* @param Function - the Function to be invoked on the render thread. When executed, it will get the current FRHICommandList instance passed in as its sole argument.
* @return true if the function was queued, false if rendering does not use a separate thread, in which case, the function has already been executed.
*/
extern HEADMOUNTEDDISPLAY_API void ExecuteOnRenderThread(const TFunctionRef<void(FRHICommandListImmediate&)>& Function);

/**
* Utility function for easily submitting TFunction to be run on the RHI thread. Must be invoked from the render thread.
* If RHI does not run on a separate thread, the TFunction will be executed immediately, otherwise it will be added to the RHI thread command list.
* @param Function - the Function to be invoked on the RHI thread.
* @return true if the function was queued, false if RHI does not use a separate thread, or if it's bypassed, in which case, the function has already been executed.
*/
extern HEADMOUNTEDDISPLAY_API bool ExecuteOnRHIThread_DoNotWait(const TFunction<void()>& Function);

/**
* Utility function for easily submitting TFunction to be run on the RHI thread. Must be invoked from the render thread.
* If RHI does not run on a separate thread, the TFunction will be executed immediately, otherwise it will be added to the RHI thread command list.
* @param Function - the Function to be invoked on the RHI thread. When executed, it will get the current FRHICommandList instance passed in as its sole argument.
* @return true if the function was queued, false if RHI does not use a separate thread, or if it's bypassed, in which case, the function has already been executed.
*/
extern HEADMOUNTEDDISPLAY_API bool ExecuteOnRHIThread_DoNotWait(const TFunction<void(FRHICommandListImmediate&)>& Function);

/**
* Utility function for easily running a TFunctionRef on the RHI thread. Must be invoked from the render thread.
* If RHI does not run on a separate thread, the TFunction will be executed on the current thread.
* This method will flush the RHI command list meaning that the function will be executed before ExecuteOnRHIThread returns.
* @param Function - the Function to be invoked on the RHI thread.
*/
extern HEADMOUNTEDDISPLAY_API void ExecuteOnRHIThread(const TFunctionRef<void()>& Function);

/**
* Utility function for easily running a TFunctionRef on the RHI thread. Must be invoked from the render thread.
* If RHI does not run on a separate thread, the TFunction will be executed on the current thread.
* This method will flush the RHI command list meaning that the function will be executed before ExecuteOnRHIThread returns.
* @param Function - the Function to be invoked on the RHI thread. When executed, it will get the current FRHICommandList instance passed in as its sole argument.
*/
extern HEADMOUNTEDDISPLAY_API void ExecuteOnRHIThread(const TFunctionRef<void(FRHICommandListImmediate&)>& Function);
