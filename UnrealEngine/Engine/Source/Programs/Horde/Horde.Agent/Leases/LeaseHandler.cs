// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using Google.Protobuf;
using Google.Protobuf.Reflection;
using Google.Protobuf.WellKnownTypes;
using Horde.Agent.Services;

namespace Horde.Agent.Leases
{
	/// <summary>
	/// Handles execution of a specific lease type
	/// </summary>
	abstract class LeaseHandler
	{
		/// <summary>
		/// Returns protobuf type urls for the handled message types
		/// </summary>
		public abstract string LeaseType { get; }

		/// <summary>
		/// Executes a lease
		/// </summary>
		/// <returns>Result for the lease</returns>
		public abstract Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, Any message, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Implementation of <see cref="LeaseHandler"/> for a specific lease type
	/// </summary>
	/// <typeparam name="T">Type of the lease message</typeparam>
	abstract class LeaseHandler<T> : LeaseHandler where T : IMessage<T>, new()
	{
		/// <summary>
		/// Static for the message type descriptor
		/// </summary>
		public static MessageDescriptor Descriptor { get; } = new T().Descriptor;

		/// <inheritdoc/>
		public override string LeaseType { get; } = $"type.googleapis.com/{Descriptor.Name}";

		/// <inheritdoc/>
		public override Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, Any message, CancellationToken cancellationToken)
		{
			return ExecuteAsync(session, leaseId, message.Unpack<T>(), cancellationToken); 
		}

		/// <inheritdoc/>
		public abstract Task<LeaseResult> ExecuteAsync(ISession session, string leaseId, T message, CancellationToken cancellationToken);
	}
}
