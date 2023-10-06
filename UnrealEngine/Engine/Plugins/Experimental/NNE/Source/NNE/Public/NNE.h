// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NNERuntime.h"
#include "UObject/WeakInterfacePtr.h"

NNE_API DECLARE_LOG_CATEGORY_EXTERN(LogNNE, Log, All);

namespace UE::NNE
{
	/**
	 * Register a runtime to make it accessible to NNE clients.
	 *
	 * The caller needs to keep a strong pointer to the runtime object to prevent it from being garbage collected.
	 *
	 * @param Runtime A weak interface pointer to the runtime to be registered.
	 * @return True if the runtime has been registered successfully, false otherwise.
	 */
	NNE_API bool RegisterRuntime(TWeakInterfacePtr<INNERuntime> Runtime);

	/**
	 * Unregister a registered runtime.
	 *
	 * @param Runtime A weak interface pointer to the runtime to be unregistered.
	 * @return True if the runtime has been unregistered successfully, false otherwise (e.g. if the runtime has not been registered).
	 */
	NNE_API bool UnregisterRuntime(TWeakInterfacePtr<INNERuntime> Runtime);
	
	/**
	 * List and return all registered runtimes.
	 *
	 * @return An array containing weak pointers to all registered runtimes.
	 */
	NNE_API TArrayView<TWeakInterfacePtr<INNERuntime>> GetAllRuntimes();

	/**
	 * Find and return a runtime by name and interface.
	 *
	 * This function tries to find a runtime by name and casts it to the interface passed as template argument (e.g. INNERuntime, INNERuntimeCPU or INNERuntimeRDG).
	 *
	 * @param Name The name of the runtime.
	 * @return A weak pointer to the runtime if it has been found and implements the interface in the template argument or an invalid pointer otherwise.
	 */
	template<class T> TWeakInterfacePtr<T> GetRuntime(const FString& Name)
	{
		for (TWeakInterfacePtr<INNERuntime> Runtime : GetAllRuntimes())
		{
			if (Runtime->GetRuntimeName() == Name)
			{
				T* RuntimePtr = Cast<T>(Runtime.Get());
				return TWeakInterfacePtr<T>(RuntimePtr);
			}
				
		}
		return TWeakInterfacePtr<T>(nullptr);
	}
	
}
