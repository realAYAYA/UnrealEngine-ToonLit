// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"


namespace UE::RemoteExecution
{
	class IRemoteExecutor;

	/**
	 * Module used to remotely execute computation
	 */
	class IRemoteExecutionModule : public IModuleInterface
	{
	public:
		/**
		 * Check to see if remote execution can be used.
		 * @return true if remote execution can be use.
		 */
		virtual bool CanRemoteExecute() const = 0;
		/**
		 * Get the accessor to allow us to remotely execute computation
		 * @return the executor
		 */
		virtual IRemoteExecutor& GetRemoteExecutor() const = 0;

		/**
		 * Set the remote executor we want to use to do computation
		 * @param	InName	The name of the executor we want to use
		 */
		virtual void SetRemoteExecutor(const FName& InName) = 0;
	};
}
