// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;

namespace Horde.Agent.Services
{
	/// <summary>
	/// Outcome from a session
	/// </summary>
	enum SessionOutcome
	{
		/// <summary>
		/// Continue the session with a backoff
		/// </summary>
		BackOff,

		/// <summary>
		/// Session completed normally, terminate the application.
		/// </summary>
		Terminate,

		/// <summary>
		/// Runs the given callback then attempts to reconnect
		/// </summary>
		RunCallback,
	}

	/// <summary>
	/// Result from executing a session
	/// </summary>
	class SessionResult
	{
		/// <summary>
		/// The outcome code
		/// </summary>
		public SessionOutcome Outcome { get; }

		/// <summary>
		/// Callback for running upgrades
		/// </summary>
		public Func<ILogger, CancellationToken, Task>? CallbackAsync { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public SessionResult(SessionOutcome outcome)
		{
			Outcome = outcome;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		public SessionResult(Func<ILogger, CancellationToken, Task> callbackAsync)
		{
			Outcome = SessionOutcome.RunCallback;
			CallbackAsync = callbackAsync;
		}
	}
}
