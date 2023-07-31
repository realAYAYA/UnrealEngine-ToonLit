// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Net;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;
using Grpc.Net.Client;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Utility
{
	/// <summary>
	/// Ref-counted reference to a HordeRpcClient. Must be disposed after use.
	/// </summary>
	interface IRpcClientRef : IDisposable
	{
		/// <summary>
		/// The Grpc channel instance
		/// </summary>
		GrpcChannel Channel { get; }

		/// <summary>
		/// The client instance
		/// </summary>
		HordeRpc.HordeRpcClient Client { get; }

		/// <summary>
		/// Task which completes when the client needs to be disposed of
		/// </summary>
		Task DisposingTask { get; }
	}

	/// <summary>
	/// Context for an RPC call. Used for debugging reference leaks.
	/// </summary>
	class RpcContext
	{
		/// <summary>
		/// Text to display
		/// </summary>
		public string Text { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="text">Text to display</param>
		public RpcContext(string text)
		{
			Text = text;
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="line">Line number of the file</param>
		/// <param name="file">Calling file</param>
		public RpcContext([CallerLineNumber] int line = 0, [CallerFilePath] string file = "")
		{
			Text = $"{file}({line})";
		}
	}

	internal interface IRpcConnection : IAsyncDisposable
	{
		/// <summary>
		/// Attempts to get a client reference, returning immediately if there's not one available
		/// </summary>
		/// <returns></returns>
		IRpcClientRef? TryGetClientRef(RpcContext context);

		/// <summary>
		/// Obtains a new client reference object
		/// </summary>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for this request</param>
		/// <returns>New client reference</returns>
		Task<IRpcClientRef> GetClientRef(RpcContext context, CancellationToken cancellationToken);

		/// <summary>
		/// Invokes an asynchronous command with a HordeRpcClient instance
		/// </summary>
		/// <param name="func">The function to execute</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		Task<T> InvokeOnceAsync<T>(Func<HordeRpc.HordeRpcClient, Task<T>> func, RpcContext context, CancellationToken cancellationToken);

		/// <summary>
		/// Obtains a client reference and executes a unary RPC
		/// </summary>
		/// <typeparam name="T">Return type from the call</typeparam>
		/// <param name="func">Method to execute with the RPC instance</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		Task<T> InvokeOnceAsync<T>(Func<HordeRpc.HordeRpcClient, AsyncUnaryCall<T>> func, RpcContext context, CancellationToken cancellationToken);

		/// <summary>
		/// Invokes an asynchronous command with a HordeRpcClient instance
		/// </summary>
		/// <param name="func">The function to execute</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		Task<T> InvokeAsync<T>(Func<HordeRpc.HordeRpcClient, Task<T>> func, RpcContext context, CancellationToken cancellationToken);

		/// <summary>
		/// Obtains a client reference and executes a unary RPC
		/// </summary>
		/// <typeparam name="T">Return type from the call</typeparam>
		/// <param name="func">Method to execute with the RPC instance</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		Task<T> InvokeAsync<T>(Func<HordeRpc.HordeRpcClient, AsyncUnaryCall<T>> func, RpcContext context, CancellationToken cancellationToken);
	}

	/// <summary>
	/// Represents a connection to a GRPC server, which responds to server shutting down events and attempts to reconnect to another instance.
	/// </summary>
	class RpcConnection : IRpcConnection
	{
		/// <summary>
		/// An instance of a connection to a particular server
		/// </summary>
		class RpcSubConnection : IAsyncDisposable
		{
			/// <summary>
			/// The connection id
			/// </summary>
			public int ConnectionId { get; }

			/// <summary>
			/// Name of the server
			/// </summary>
			public string Name
			{
				get;
			}

			/// <summary>
			/// The Grpc channel
			/// </summary>
			public GrpcChannel Channel { get; }

			/// <summary>
			/// The RPC interface
			/// </summary>
			public HordeRpc.HordeRpcClient Client
			{
				get;
			}

			/// <summary>
			/// Task which is set to indicate the connection is being disposed
			/// </summary>
			public Task DisposingTask => _disposingTaskSource.Task;

			/// <summary>
			/// Logger for messages about refcount changes
			/// </summary>
			private readonly ILogger _logger;

			/// <summary>
			/// The current refcount
			/// </summary>
			private int _refCount;

			/// <summary>
			/// Whether the connection is terminating
			/// </summary>
			private bool _disposing;

			/// <summary>
			/// Cancellation token set 
			/// </summary>
			private readonly TaskCompletionSource<bool> _disposingTaskSource = new TaskCompletionSource<bool>();

			/// <summary>
			/// Wraps a task allowing the disposer to wait for clients to finish using this connection
			/// </summary>
			private readonly TaskCompletionSource<bool> _disposedTaskSource = new TaskCompletionSource<bool>();

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="connectionId">The connection id</param>
			/// <param name="name">Name of the server</param>
			/// <param name="channel">The channel instance</param>
			/// <param name="client">The client instance</param>
			/// <param name="logger">Logger for debug messages</param>
			public RpcSubConnection(int connectionId, string name, GrpcChannel channel, HordeRpc.HordeRpcClient client, ILogger logger)
			{
				ConnectionId = connectionId;
				Name = name;
				Channel = channel;
				Client = client;
				_logger = logger;
				_refCount = 1;
			}

			/// <summary>
			/// Attempts to increment the reference count for this subconnection. Fails if it's already zero.
			/// </summary>
			/// <returns>True if a reference was added</returns>
			public bool TryAddRef()
			{
				for (; ; )
				{
					int initialRefCount = _refCount;
					if (initialRefCount == 0)
					{
						return false;
					}
					if (Interlocked.CompareExchange(ref _refCount, initialRefCount + 1, initialRefCount) == initialRefCount)
					{
						return true;
					}
				}
			}

			/// <summary>
			/// Release a reference to this connection
			/// </summary>
			public void Release()
			{
				int newRefCount = Interlocked.Decrement(ref _refCount);
				if (_disposing)
				{
					_logger.LogInformation("Connection {ConnectionId}: Decrementing refcount to rpc server {ServerName} ({RefCount} rpc(s) in flight)", ConnectionId, Name, newRefCount);
				}
				if (newRefCount == 0)
				{
					_disposedTaskSource.SetResult(true);
				}
			}

			/// <summary>
			/// Dispose of this connection, waiting for all references to be released first
			/// </summary>
			/// <returns>Async task</returns>
			public ValueTask DisposeAsync()
			{
				_logger.LogInformation("Connection {ConnectionId}: Disposing sub-connection", ConnectionId);
				_disposingTaskSource.TrySetResult(true);
				_disposing = true;
				Release();
				return new ValueTask(_disposedTaskSource.Task);
			}
		}

		/// <summary>
		/// Reference to a connection instance
		/// </summary>
		class RpcClientRef : IRpcClientRef
		{
			/// <summary>
			/// The subconnection containing the client
			/// </summary>
			public RpcSubConnection SubConnection { get; }

			/// <summary>
			/// Context for the reference
			/// </summary>
			public RpcContext Context { get; }

			/// <summary>
			/// The channel instance
			/// </summary>
			public GrpcChannel Channel => SubConnection.Channel;

			/// <summary>
			/// The client instance
			/// </summary>
			public HordeRpc.HordeRpcClient Client => SubConnection.Client;

			/// <summary>
			/// Task which completes when the connection should be disposed
			/// </summary>
			public Task DisposingTask => SubConnection.DisposingTask;

			/// <summary>
			/// Tracks all the active refs
			/// </summary>
			private static readonly List<RpcClientRef> s_currentRefs = new List<RpcClientRef>();

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="sharedClient">The subconnection containing the client</param>
			/// <param name="context">Description of the reference, for debugging</param>
			public RpcClientRef(RpcSubConnection sharedClient, RpcContext context)
			{
				SubConnection = sharedClient;
				Context = context;

				lock (s_currentRefs)
				{
					s_currentRefs.Add(this);
				}
			}

			/// <summary>
			/// Dispose of this reference
			/// </summary>
			public void Dispose()
			{
				SubConnection.Release();

				lock(s_currentRefs)
				{
					s_currentRefs.Remove(this);
				}
			}

			/// <summary>
			/// Gets a list of all the active refs
			/// </summary>
			/// <returns>List of ref descriptions</returns>
			public static List<string> GetActiveRefs()
			{
				lock(s_currentRefs)
				{
					return s_currentRefs.Select(x => x.Context.Text).ToList();
				}
			}
		}

		private static readonly TimeSpan[] s_retryTimes =
		{
			TimeSpan.FromSeconds(1.0),
			TimeSpan.FromSeconds(10.0),
			TimeSpan.FromSeconds(30.0),
		};

		private readonly Func<GrpcChannel> _createGrpcChannel;
		private readonly Func<GrpcChannel, HordeRpc.HordeRpcClient> _createHordeRpcClient;
		private readonly TaskCompletionSource<bool> _stoppingTaskSource = new TaskCompletionSource<bool>();
		private TaskCompletionSource<RpcSubConnection> _subConnectionTaskSource = new TaskCompletionSource<RpcSubConnection>();
		private Task? _backgroundTask;
		private readonly ILogger _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="createGrpcChannel">Factory method for creating new GRPC channels</param>
		/// <param name="createHordeRpcClient">Factory method for creating new Horde.Rpc clients</param>
		/// <param name="logger">Logger instance</param>
		public RpcConnection(Func<GrpcChannel> createGrpcChannel, Func<GrpcChannel, HordeRpc.HordeRpcClient> createHordeRpcClient, ILogger logger)
		{
			_createGrpcChannel = createGrpcChannel;
			_createHordeRpcClient = createHordeRpcClient;
			_logger = logger;

			_backgroundTask = Task.Run(() => ExecuteAsync());
		}

		/// <summary>
		/// Attempts to get a client reference, returning immediately if there's not one available
		/// </summary>
		/// <returns></returns>
		public IRpcClientRef? TryGetClientRef(RpcContext context)
		{
			Task<RpcSubConnection> subConnectionTask = _subConnectionTaskSource.Task;
			if (subConnectionTask.IsCompleted)
			{
				RpcSubConnection defaultClient = subConnectionTask.Result;
				if(defaultClient.TryAddRef())
				{
					return new RpcClientRef(defaultClient, context);
				}
			}
			return null;
		}

		/// <summary>
		/// Obtains a new client reference object
		/// </summary>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for this request</param>
		/// <returns>New client reference</returns>
		public async Task<IRpcClientRef> GetClientRef(RpcContext context, CancellationToken cancellationToken)
		{
			for (; ; )
			{
				// Print a message while we're waiting for a client to be available
				if (!_subConnectionTaskSource.Task.IsCompleted)
				{
					Stopwatch timer = Stopwatch.StartNew();
					while (!_subConnectionTaskSource.Task.IsCompleted)
					{
						Task delay = Task.Delay(TimeSpan.FromSeconds(30.0), cancellationToken);
						if (await Task.WhenAny(delay, _subConnectionTaskSource.Task, _stoppingTaskSource.Task) == delay)
						{
							await delay; // Allow the cancellation token to throw
							_logger.LogInformation("Thread stalled for {Time:0.0}s while waiting for IRpcClientRef. Source:\n{Context}\nActive references:\n{References}\nStack trace:\n{StackTrace}", timer.Elapsed.TotalSeconds, context.Text, String.Join("\n", RpcClientRef.GetActiveRefs()), Environment.StackTrace);
						}
						if (_stoppingTaskSource.Task.IsCompleted)
						{
							throw new OperationCanceledException("RpcConnection is being terminated");
						}
					}
				}

				// Get the new task and increase the refcount
				RpcSubConnection defaultClient = await _subConnectionTaskSource.Task;
				if (defaultClient.TryAddRef())
				{
					return new RpcClientRef(defaultClient, context);
				}
			}
		}

		/// <summary>
		/// Invokes an asynchronous command with a HordeRpcClient instance
		/// </summary>
		/// <param name="func">The function to execute</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public async Task<T> InvokeOnceAsync<T>(Func<HordeRpc.HordeRpcClient, Task<T>> func, RpcContext context, CancellationToken cancellationToken)
		{
			using (IRpcClientRef clientRef = await GetClientRef(context, cancellationToken))
			{
				return await func(clientRef.Client);
			}
		}

		/// <summary>
		/// Obtains a client reference and executes a unary RPC
		/// </summary>
		/// <typeparam name="T">Return type from the call</typeparam>
		/// <param name="func">Method to execute with the RPC instance</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public Task<T> InvokeOnceAsync<T>(Func<HordeRpc.HordeRpcClient, AsyncUnaryCall<T>> func, RpcContext context, CancellationToken cancellationToken)
		{
			return InvokeOnceAsync(async (x) => await func(x), context, cancellationToken);
		}

		/// <summary>
		/// Invokes an asynchronous command with a HordeRpcClient instance
		/// </summary>
		/// <param name="func">The function to execute</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public async Task<T> InvokeAsync<T>(Func<HordeRpc.HordeRpcClient, Task<T>> func, RpcContext context, CancellationToken cancellationToken)
		{
			for (int attempt = 0; ;attempt++)
			{
				// Attempt the RPC call
				using (IRpcClientRef clientRef = await GetClientRef(context, cancellationToken))
				{
					try
					{
						return await func(clientRef.Client);
					}
					catch (Exception ex)
					{
						// Otherwise check if we can try again
						if (attempt >= s_retryTimes.Length)
						{
							_logger.LogError("Max number of retry attempts reached");
							throw;
						}

						// Check if the user requested cancellation
						if (cancellationToken.IsCancellationRequested)
						{
							_logger.LogInformation("Cancellation of gRPC call requested");
							throw;
						}

						// Check we can retry it
						if (!CanRetry(ex))
						{
							_logger.LogError(ex, "Exception during RPC: {Message}", ex.Message);
							throw;
						}

						RpcException? rpcEx = ex as RpcException;
						if (rpcEx != null && rpcEx.StatusCode == StatusCode.Unavailable)
						{
							_logger.LogInformation(ex, "Failure #{Attempt} during gRPC call (service unavailable). Retrying...", attempt + 1);
						}
						else
						{
							_logger.LogError(ex, "Failure #{Attempt} during gRPC call.", attempt + 1);
						}
					}
				}

				// Wait before retrying
				await Task.Delay(s_retryTimes[attempt], cancellationToken);
			}
		}

		/// <summary>
		/// Determines if the given exception should be retried
		/// </summary>
		/// <param name="ex"></param>
		/// <returns></returns>
		public static bool CanRetry(Exception ex)
		{
			RpcException? rpcEx = ex as RpcException;
			if(rpcEx != null)
			{
				return rpcEx.StatusCode == StatusCode.Cancelled || rpcEx.StatusCode == StatusCode.Unavailable || rpcEx.StatusCode == StatusCode.DataLoss /* Interrupted streaming call */;
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Obtains a client reference and executes a unary RPC
		/// </summary>
		/// <typeparam name="T">Return type from the call</typeparam>
		/// <param name="func">Method to execute with the RPC instance</param>
		/// <param name="context">Context for the call, for debugging</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public Task<T> InvokeAsync<T>(Func<HordeRpc.HordeRpcClient, AsyncUnaryCall<T>> func, RpcContext context, CancellationToken cancellationToken)
		{
			return InvokeAsync(async (x) => await func(x), context, cancellationToken);
		}

		/// <summary>
		/// Background task to monitor the server state
		/// </summary>
		/// <returns>Async task</returns>
		async Task ExecuteAsync()
		{
			// Counter for the connection number. Used to track log messages through async threads.
			int connectionId = 0;

			// This task source is marked as completed once a RPC connection receives a notification from the server that it's shutting down.
			// Another connection will immediately be allocated for other connection requests to use, and the load balancer should route us to
			// the new server.
			TaskCompletionSource<bool>? reconnectTaskSource = null;

			// List of connection handling async tasks we're tracking
			List<Task> tasks = new List<Task>();
			while (tasks.Count > 0 || !_stoppingTaskSource.Task.IsCompleted)
			{
				// Get the task indicating we want a new connection (or create a new one)
				Task? reconnectTask = null;
				if (!_stoppingTaskSource.Task.IsCompleted)
				{
					if (reconnectTaskSource == null || reconnectTaskSource.Task.IsCompleted)
					{
						// Need to avoid lambda capture of the reconnect task source
						int newConnectionId = ++connectionId;
						TaskCompletionSource<bool> newReconnectTaskSource = new TaskCompletionSource<bool>();
						tasks.Add(Task.Run(() => HandleConnectionAsync(newConnectionId, newReconnectTaskSource)));
						reconnectTaskSource = newReconnectTaskSource;
					}
					reconnectTask = reconnectTaskSource.Task;
				}

				// Build a list of tasks to wait for
				List<Task> waitTasks = new List<Task>(tasks);
				if (reconnectTask != null)
				{
					waitTasks.Add(reconnectTask);
				}
				await Task.WhenAny(waitTasks);

				// Remove any complete tasks, logging any exceptions
				for (int idx = 0; idx < tasks.Count; idx++)
				{
					Task task = tasks[idx];
					if(task.IsCompleted)
					{
						await task;
						tasks.RemoveAt(idx--);
					}
				}
			}
		}

		/// <summary>
		/// Background task to monitor the server state
		/// </summary>
		/// <param name="connectionId">The connection id</param>
		/// <param name="reconnectTaskSource">Task source for reconnecting</param>
		/// <returns>Async task</returns>
		async Task HandleConnectionAsync(int connectionId, TaskCompletionSource<bool> reconnectTaskSource)
		{
			using IScope scope = GlobalTracer.Instance.BuildSpan("RpcConnection {ConnectionId}").StartActive();

			_logger.LogInformation("Connection {ConnectionId}: Creating connection", connectionId);
			try
			{
				await HandleConnectionInternalAsync(connectionId, reconnectTaskSource);
			}
			catch (RpcException ex) when (ex.StatusCode == StatusCode.Unavailable)
			{
				_logger.LogInformation("Connection {ConnectionId}: Rpc service is unavailable. Reconnecting.", connectionId);
			}
			catch (Exception ex)
			{
				_logger.LogError(ex, "Connection {ConnectionId}: Exception handling connection: {Message}", connectionId, ex.Message);
			}
			finally
			{
				TriggerReconnect(reconnectTaskSource);
			}
		}

		/// <summary>
		/// Background task to monitor the server state
		/// </summary>
		/// <returns>Async task</returns>
		async Task HandleConnectionInternalAsync(int connectionId, TaskCompletionSource<bool> reconnectTaskSource)
		{
			using (GrpcChannel channel = _createGrpcChannel())
			{
				HordeRpc.HordeRpcClient client = _createHordeRpcClient(channel);

				RpcSubConnection? subConnection = null;
				try
				{
					while (!reconnectTaskSource.Task.IsCompleted && !_stoppingTaskSource.Task.IsCompleted)
					{
						// Client HAS TO BE the one to end the call, by closing the stream. The server must keep receiving requests from us until it does.
						using (AsyncDuplexStreamingCall<QueryServerStateRequest, QueryServerStateResponse> call = client.QueryServerStateV2())
						{
							// Send the name of this agent
							QueryServerStateRequest request = new QueryServerStateRequest();
							request.Name = Dns.GetHostName();
							await call.RequestStream.WriteAsync(request);

							// Read the server info
							Task<bool> moveNextTask = call.ResponseStream.MoveNext();
							if (!await moveNextTask)
							{
								_logger.LogError("Connection {ConnectionId}: No response from server", connectionId);
								break;
							}

							// Read the server response
							QueryServerStateResponse response = call.ResponseStream.Current;
							if (!response.Stopping)
							{
								// The first time we connect, log the server name and create the subconnection
								if (subConnection == null)
								{
									_logger.LogInformation("Connection {ConnectionId}: Connected to rpc server {ServerName}", connectionId, response.Name);
									subConnection = new RpcSubConnection(connectionId, response.Name, channel, client, _logger);
									_subConnectionTaskSource.SetResult(subConnection);
								}

								// Wait for the StoppingTask token to be set, or the server to inform that it's shutting down
								moveNextTask = call.ResponseStream.MoveNext();
								await Task.WhenAny(_stoppingTaskSource.Task, Task.Delay(TimeSpan.FromSeconds(45.0)), moveNextTask);

								// Update the response
								if (moveNextTask.IsCompleted && await moveNextTask)
								{
									response = call.ResponseStream.Current;
								}
							}

							// If the server is stopping, start reconnecting
							if (response.Stopping)
							{
								_logger.LogInformation("Connection {ConnectionId}: Server is stopping. Triggering reconnect.", connectionId);
								TriggerReconnect(reconnectTaskSource);
							}

							// Close the request stream
							await call.RequestStream.CompleteAsync();

							// Wait for the server to finish posting responses
							while (await moveNextTask)
							{
								response = call.ResponseStream.Current;
								moveNextTask = call.ResponseStream.MoveNext();
							}
						}
					}
					_logger.LogInformation("Connection {ConnectionId}: Closing connection to rpc server", connectionId);
				}
				finally
				{
					if (subConnection != null)
					{
						await subConnection.DisposeAsync();
					}
				}

				// Wait a few seconds before finishing
				await Task.Delay(TimeSpan.FromSeconds(5.0));
			}
		}

		/// <summary>
		/// Clears the current subconnection and triggers a reconnect
		/// </summary>
		/// <param name="reconnectTaskSource"></param>
		void TriggerReconnect(TaskCompletionSource<bool> reconnectTaskSource)
		{
			if (!reconnectTaskSource.Task.IsCompleted)
			{
				// First reset the task source, so that nothing new starts to use the current connection
				if (_subConnectionTaskSource.Task.IsCompleted)
				{
					_subConnectionTaskSource = new TaskCompletionSource<RpcSubConnection>();
				}

				// Now trigger another connection
				reconnectTaskSource.SetResult(true);
			}
		}

		/// <summary>
		/// Dispose of this connection
		/// </summary>
		/// <returns>Async task</returns>
		public async ValueTask DisposeAsync()
		{
			if (_backgroundTask != null)
			{
				_stoppingTaskSource.TrySetResult(true);

				await _backgroundTask;
				_backgroundTask = null!;
			}
		}

		public static IRpcConnection Create(Func<GrpcChannel> createGrpcChannel, Func<GrpcChannel, HordeRpc.HordeRpcClient> createHordeRpcClient, ILogger logger)
		{
			return new RpcConnection(createGrpcChannel, createHordeRpcClient, logger);
		}
	}
}
