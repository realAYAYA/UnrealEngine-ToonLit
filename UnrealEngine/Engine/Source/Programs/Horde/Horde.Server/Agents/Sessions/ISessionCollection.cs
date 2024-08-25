// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Agents;
using EpicGames.Horde.Agents.Sessions;

namespace Horde.Server.Agents.Sessions
{
	/// <summary>
	/// Interface for a collection of session documents
	/// </summary>
	public interface ISessionCollection
	{
		/// <summary>
		/// Adds a new session to the collection
		/// </summary>
		/// <param name="id">The session id</param>
		/// <param name="agentId">The agent this session is for</param>
		/// <param name="startTime">Start time of this session</param>
		/// <param name="properties">Properties of this agent at the time the session started</param>
		/// <param name="resources">Resources which the agent has</param>
		/// <param name="version">Version of the agent software</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task<ISession> AddAsync(SessionId id, AgentId agentId, DateTime startTime, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, string? version, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets information about a particular session
		/// </summary>
		/// <param name="sessionId">The unique session id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The session information</returns>
		Task<ISession?> GetAsync(SessionId sessionId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Find sessions for the given agent
		/// </summary>
		/// <param name="agentId">The unique agent id</param>
		/// <param name="startTime">Start time to include in the search</param>
		/// <param name="finishTime">Finish time to include in the search</param>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of sessions matching the given criteria</returns>
		Task<List<ISession>> FindAsync(AgentId agentId, DateTime? startTime, DateTime? finishTime, int index, int count, CancellationToken cancellationToken = default);

		/// <summary>
		/// Finds any active sessions
		/// </summary>
		/// <param name="index">Index of the first result to return</param>
		/// <param name="count">Number of results to return</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of sessions</returns>
		Task<List<ISession>> FindActiveSessionsAsync(int? index = null, int? count = null, CancellationToken cancellationToken = default);

		/// <summary>
		/// Update a session from the collection
		/// </summary>
		/// <param name="sessionId">The session to update</param>
		/// <param name="finishTime">Time at which the session finished</param>
		/// <param name="properties">The agent properties</param>
		/// <param name="resources">Resources which the agent has</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task UpdateAsync(SessionId sessionId, DateTime? finishTime, IReadOnlyList<string>? properties, IReadOnlyDictionary<string, int>? resources, CancellationToken cancellationToken = default);

		/// <summary>
		/// Delete a session from the collection
		/// </summary>
		/// <param name="sessionId">The session id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(SessionId sessionId, CancellationToken cancellationToken = default);
	}
}
