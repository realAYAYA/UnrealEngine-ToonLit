// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Base interface for Perforce clients
	/// </summary>
	public interface IPerforceConnection : IDisposable
	{
		/// <summary>
		/// Connection settings
		/// </summary>
		IPerforceSettings Settings { get; }

		/// <summary>
		/// Logger for this connection
		/// </summary>
		ILogger Logger { get; }

		/// <summary>
		/// Runs a Perforce command
		/// </summary>
		/// <param name="command">The command name</param>
		/// <param name="arguments">Arguments for the command</param>
		/// <param name="fileArguments">File arguments (may be put into a response file)</param>
		/// <param name="inputData">Input data to be passed to the command</param>
		/// <param name="interceptIo">Whether to intercept file I/O and return it in the reponse stream. Only supported by the native client.</param>
		/// <returns>Response object</returns>
		Task<IPerforceOutput> CommandAsync(string command, IReadOnlyList<string> arguments, IReadOnlyList<string>? fileArguments, byte[]? inputData, bool interceptIo);

		/// <summary>
		/// Execute the 'login' command
		/// </summary>
		/// <param name="password">Password to use to login</param>
		/// <param name="cancellationToken">Token used to cancel the operation</param>
		/// <returns>Response from the server</returns>
		Task<IPerforceOutput> LoginCommandAsync(string password, CancellationToken cancellationToken = default);

		/// <summary>
		/// Creates a record from a set of input fields
		/// </summary>
		/// <param name="fields">Fields for the record</param>
		/// <returns>Serialized record data</returns>
		PerforceRecord CreateRecord(List<KeyValuePair<string, object>> fields);
	}
}
