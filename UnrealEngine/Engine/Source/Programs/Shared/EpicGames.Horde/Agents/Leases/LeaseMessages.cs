// Copyright Epic Games, Inc. All Rights Reserved.

#pragma warning disable CA1027 // Mark enums with FlagsAttribute

namespace EpicGames.Horde.Agents.Leases
{
	/// <summary>
	/// Outcome from a lease. Values must match lease_outcome.proto.
	/// </summary>
	public enum LeaseOutcome
	{
		/// <summary>
		/// Default value.
		/// </summary>
		Unspecified = 0,

		/// <summary>
		/// The lease was executed successfully
		/// </summary>
		Success = 1,

		/// <summary>
		/// The lease was not executed succesfully, but cannot be run again.
		/// </summary>
		Failed = 2,

		/// <summary>
		/// The lease was cancelled by request
		/// </summary>
		Cancelled = 4
	}

	/// <summary>
	/// State of a lease. Values must match lease_state.proto.
	/// </summary>
	public enum LeaseState
	{
		/// <summary>
		/// Default value.
		/// </summary>
		Unspecified = 0,

		/// <summary>
		/// Set by the server when waiting for an agent to accept the lease. Once processed, the agent should transition the lease state to active.
		/// </summary>
		Pending = 1,

		/// <summary>
		/// The agent is actively working on this lease.
		/// </summary>
		Active = 2,

		/// <summary>
		/// The agent has finished working on this lease.
		/// </summary>
		Completed = 3,

		/// <summary>
		/// Set by the server to indicate that the lease should be cancelled.
		/// </summary>
		Cancelled = 4
	}

	/// <summary>
	/// Updates an existing lease
	/// </summary>
	public class UpdateLeaseRequest
	{
		/// <summary>
		/// Mark this lease as aborted
		/// </summary>
		public bool? Aborted { get; set; }
	}
}
