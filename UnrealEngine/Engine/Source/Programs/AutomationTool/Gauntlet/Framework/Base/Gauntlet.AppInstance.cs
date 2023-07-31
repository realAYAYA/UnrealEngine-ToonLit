// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;


namespace Gauntlet
{
	/// <summary>
	/// Interface that represents an instance of an app running on a device
	/// </summary>
	public interface IAppInstance
 	{
		/// <summary>
		/// Returns true/false if the process has exited for any reason
		/// </summary>
		bool HasExited { get; }

		/// <summary>
		/// Current StdOut of the process
		/// </summary>
		string StdOut { get; }

		/// <summary>
		/// Exit code of the process.
		/// </summary>
		int ExitCode { get; }

		/// <summary>
		/// Returns true if the process exited due to Kill() being called
		/// </summary>
		bool WasKilled { get; }

		/// <summary>
		/// Path to commandline used to start the process
		/// </summary>
		string CommandLine { get; }

		/// <summary>
		/// Path to artifacts from the process
		/// </summary>
		string ArtifactPath { get; }

		/// <summary>
		/// Device that the app was run on
		/// </summary>
		ITargetDevice Device { get; }

		/// <summary>
		/// Kills the process if its running (no need to call WaitForExit)
		/// </summary>
		void Kill();

		/// <summary>
		/// Waits for the process to exit normally
		/// </summary>
		/// <returns></returns>
		int WaitForExit();

	}

	public interface IWithUnfilteredStdOut
	{
		string UnfilteredStdOut { get; }
	}



	/// <summary>
	/// Interface used by IAppInstance if they support Suspend/Resume
	/// </summary>
	public interface IWithPLMSuspend
	{
		/// <summary>
		/// Attempt to suspend the running application. Correlates to FCoreDelegates::ApplicationWillEnterBackgroundDelegate
		/// </summary>
		bool Suspend();

		/// <summary>
		/// Attempts to resume a suspended application. Correlates to FCoreDelegates::ApplicationHasEnteredForegroundDelegate
		/// </summary>
		bool Resume();
	}

	/// <summary>
	/// Interface used by IAppInstance if they support Constrain/Unconstrain
	/// </summary>
	public interface IWithPLMConstrain
	{
		/// <summary>
		/// Attempts to contrain the running application. Correlates to FCoreDelegates::ApplicationWillDeactivateDelegate
		/// </summary>
		bool Constrain();

		/// <summary>
		/// Attempts to unconstained a constrained application. Correlates to FCoreDelegates::ApplicationHasReactivatedDelegate
		/// </summary>
		bool Unconstrain();
	}
}
