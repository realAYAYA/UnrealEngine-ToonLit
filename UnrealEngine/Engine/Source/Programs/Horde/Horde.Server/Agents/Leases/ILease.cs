// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Leases;
using EpicGames.Horde.Agents.Pools;
using EpicGames.Horde.Agents.Sessions;
using EpicGames.Horde.Logs;
using EpicGames.Horde.Streams;
using Google.Protobuf;
using Google.Protobuf.WellKnownTypes;

namespace Horde.Server.Agents.Leases
{
	/// <summary>
	/// Document describing a lease. This exists to permanently record a lease; the agent object tracks internal state of any active leases through AgentLease objects.
	/// </summary>
	public interface ILease
	{
		/// <summary>
		/// The unique id of this lease
		/// </summary>
		public LeaseId Id { get; }

		/// <summary>
		/// Identifier for the parent lease. Used to terminate hierarchies of leases.
		/// </summary>
		public LeaseId? ParentId { get; }

		/// <summary>
		/// Name of this lease
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Unique id of the agent 
		/// </summary>
		public AgentId AgentId { get; }

		/// <summary>
		/// Unique id of the agent session
		/// </summary>
		public SessionId SessionId { get; }

		/// <summary>
		/// The stream this lease belongs to
		/// </summary>
		public StreamId? StreamId { get; }

		/// <summary>
		/// Pool for the work being executed
		/// </summary>
		public PoolId? PoolId { get; }

		/// <summary>
		/// The log for this lease, if applicable
		/// </summary>
		public LogId? LogId { get; }

		/// <summary>
		/// Time at which this lease started
		/// </summary>
		public DateTime StartTime { get; }

		/// <summary>
		/// Time at which this lease completed
		/// </summary>
		public DateTime? FinishTime { get; }

		/// <summary>
		/// Payload for this lease. A packed Google.Protobuf.Any object.
		/// </summary>
		public ReadOnlyMemory<byte> Payload { get; }

		/// <summary>
		/// Outcome of the lease
		/// </summary>
		public LeaseOutcome Outcome { get; }

		/// <summary>
		/// Output from executing the lease
		/// </summary>
		public ReadOnlyMemory<byte> Output { get; }
	}

	/// <summary>
	/// Extension methods for leases
	/// </summary>
	public static class LeaseExtensions
	{
		/// <summary>
		/// Gets the task from a lease, encoded as an Any protobuf object
		/// </summary>
		/// <param name="lease">The lease to query</param>
		/// <returns>The task definition encoded as a protobuf Any object</returns>
		public static Any GetTask(this ILease lease)
		{
			return Any.Parser.ParseFrom(lease.Payload.ToArray());
		}

		/// <summary>
		/// Gets a typed task object from a lease
		/// </summary>
		/// <typeparam name="T">Type of the protobuf message to return</typeparam>
		/// <param name="lease">The lease to query</param>
		/// <returns>The task definition</returns>
		public static T GetTask<T>(this ILease lease) where T : IMessage<T>, new()
		{
			return GetTask(lease).Unpack<T>();
		}
	}
}
