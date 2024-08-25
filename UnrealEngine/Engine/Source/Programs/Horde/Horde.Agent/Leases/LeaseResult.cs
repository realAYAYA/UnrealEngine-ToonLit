// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Agents.Leases;
using Horde.Agent.Services;

namespace Horde.Agent.Leases
{
	/// <summary>
	/// Result from executing a lease
	/// </summary>
	internal class LeaseResult
	{
		/// <summary>
		/// Outcome of the lease (whether it completed/failed due to an internal error, etc...)
		/// </summary>
		public LeaseOutcome Outcome { get; }

		/// <summary>
		/// Output from executing the task
		/// </summary>
		public byte[]? Output { get; }

		/// <summary>
		/// If a lease wants to terminate the current session, this specifies the subsequent action to take.
		/// </summary>
		public SessionResult? SessionResult { get; }

		/// <summary>
		/// Static instance of a cancelled result
		/// </summary>
		public static LeaseResult Cancelled { get; } = new LeaseResult(LeaseOutcome.Cancelled);

		/// <summary>
		/// Static instance of a failed result
		/// </summary>
		public static LeaseResult Failed { get; } = new LeaseResult(LeaseOutcome.Failed);

		/// <summary>
		/// Static instance of a succesful result without a payload
		/// </summary>
		public static LeaseResult Success { get; } = new LeaseResult(LeaseOutcome.Success);

		/// <summary>
		/// Constructor for 
		/// </summary>
		/// <param name="outcome"></param>
		private LeaseResult(LeaseOutcome outcome)
		{
			Outcome = outcome;
		}

		/// <summary>
		/// Constructor for successful results
		/// </summary>
		/// <param name="output"></param>
		public LeaseResult(byte[]? output)
		{
			Outcome = LeaseOutcome.Success;
			Output = output;
		}

		/// <summary>
		/// Constructor for successful results that cause a session state change
		/// </summary>
		/// <param name="sessionResult">Session result</param>
		public LeaseResult(SessionResult sessionResult)
		{
			Outcome = LeaseOutcome.Success;
			SessionResult = sessionResult;
		}
	}
}
