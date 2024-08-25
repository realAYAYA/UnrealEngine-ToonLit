// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.Net;
using Grpc.Core;
using Grpc.Net.Client;
using HordeCommon.Rpc;
using Microsoft.Extensions.Logging;
using OpenTracing;
using OpenTracing.Util;

namespace Horde.Agent.Utility
{
	interface IRpcClientRef<T> : IDisposable where T : ClientBase<T>
	{
		/// <summary>
		/// The client instance
		/// </summary>
		T Client { get; }

		/// <summary>
		/// Task which completes when the client needs to be disposed of
		/// </summary>
		Task DisposingTask { get; }
	}

	interface IRpcConnection : IAsyncDisposable
	{
		/// <summary>
		/// Reports whether the server connection is healthy
		/// </summary>
		bool Healthy { get; }

		/// <summary>
		/// Logger for this connection
		/// </summary>
		ILogger Logger { get; }

		/// <summary>
		/// Attempts to get a client reference, returning immediately if there's not one available
		/// </summary>
		/// <returns></returns>
		IRpcClientRef<TClient>? TryGetClientRef<TClient>() where TClient : ClientBase<TClient>;

		/// <summary>
		/// Obtains a new client reference object
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for this request</param>
		/// <returns>New client reference</returns>
		Task<IRpcClientRef<TClient>> GetClientRefAsync<TClient>(CancellationToken cancellationToken) where TClient : ClientBase<TClient>;
	}

	static class RpcConnectionExtensions
	{
		private static readonly TimeSpan[] s_retryTimes =
		{
			TimeSpan.FromSeconds(1.0),
			TimeSpan.FromSeconds(10.0),
			TimeSpan.FromSeconds(30.0),
		};

		/// <summary>
		/// Invokes an asynchronous command with a HordeRpcClient instance
		/// </summary>
		/// <typeparam name="TClient">The type of client to call</typeparam>
		/// <typeparam name="TResult">Return type from the call</typeparam>
		/// <param name="connection">The connection object</param>
		/// <param name="func">The function to execute</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public static async Task<TResult> InvokeOnceAsync<TClient, TResult>(this IRpcConnection connection, Func<TClient, Task<TResult>> func, CancellationToken cancellationToken) where TClient : ClientBase<TClient>
		{
			using (IRpcClientRef<TClient> clientRef = await connection.GetClientRefAsync<TClient>(cancellationToken))
			{
				return await func(clientRef.Client);
			}
		}

		/// <summary>
		/// Obtains a client reference and executes a unary RPC
		/// </summary>
		/// <typeparam name="TClient">The type of client to call</typeparam>
		/// <typeparam name="TResult">Return type from the call</typeparam>
		/// <param name="connection">The connection object</param>
		/// <param name="func">Method to execute with the RPC instance</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public static Task<TResult> InvokeOnceAsync<TClient, TResult>(this IRpcConnection connection, Func<TClient, AsyncUnaryCall<TResult>> func, CancellationToken cancellationToken) where TClient : ClientBase<TClient>
		{
			return InvokeOnceAsync<TClient, TResult>(connection, async (x) => await func(x), cancellationToken);
		}

		/// <summary>
		/// Invokes an asynchronous command with a HordeRpcClient instance
		/// </summary>
		/// <typeparam name="TClient">The type of client to call</typeparam>
		/// <typeparam name="TResult">Return type from the call</typeparam>
		/// <param name="connection">The connection object</param>
		/// <param name="func">The function to execute</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public static async Task<TResult> InvokeAsync<TClient, TResult>(this IRpcConnection connection, Func<TClient, Task<TResult>> func, CancellationToken cancellationToken) where TClient : ClientBase<TClient>
		{
			for (int attempt = 0; ; attempt++)
			{
				// Attempt the RPC call
				using (IRpcClientRef<TClient> clientRef = await connection.GetClientRefAsync<TClient>(cancellationToken))
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
							connection.Logger.LogError("Max number of retry attempts reached");
							throw;
						}

						// Check if the user requested cancellation
						if (cancellationToken.IsCancellationRequested)
						{
							connection.Logger.LogInformation("Cancellation of gRPC call requested");
							throw;
						}

						// Check we can retry it
						if (!CanRetry(ex))
						{
							connection.Logger.LogError(ex, "Exception during RPC: {Message}", ex.Message);
							throw;
						}

						RpcException? rpcEx = ex as RpcException;
						if (rpcEx != null && rpcEx.StatusCode == StatusCode.Unavailable)
						{
							connection.Logger.LogInformation(ex, "Failure #{Attempt} during gRPC call (service unavailable). Retrying...", attempt + 1);
						}
						else
						{
							connection.Logger.LogError(ex, "Failure #{Attempt} during gRPC call.", attempt + 1);
						}
					}
				}

				// Wait before retrying
				await Task.Delay(s_retryTimes[attempt], cancellationToken);
			}
		}

		/// <summary>
		/// Obtains a client reference and executes a unary RPC
		/// </summary>
		/// <typeparam name="TClient">The type of client to call</typeparam>
		/// <typeparam name="TResult">Return type from the call</typeparam>
		/// <param name="connection">The connection object</param>
		/// <param name="func">Method to execute with the RPC instance</param>
		/// <param name="cancellationToken">Cancellation token for the call</param>
		/// <returns>Response from the call</returns>
		public static Task<TResult> InvokeAsync<TClient, TResult>(this IRpcConnection connection, Func<TClient, AsyncUnaryCall<TResult>> func, CancellationToken cancellationToken) where TClient : ClientBase<TClient>
		{
			return InvokeAsync<TClient, TResult>(connection, async (x) => await func(x), cancellationToken);
		}

		/// <summary>
		/// Determines if the given exception should be retried
		/// </summary>
		/// <param name="ex"></param>
		/// <returns></returns>
		static bool CanRetry(Exception ex)
		{
			RpcException? rpcEx = ex as RpcException;
			if (rpcEx != null)
			{
				return rpcEx.StatusCode == StatusCode.Cancelled || rpcEx.StatusCode == StatusCode.Unavailable || rpcEx.StatusCode == StatusCode.DataLoss /* Interrupted streaming call */;
			}
			else
			{
				return false;
			}
		}
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
			/// Task which is set to indicate the connection is being disposed
			/// </summary>
			public Task DisposingTask => _disposingTaskSource.Task;

			/// <summary>
			/// Logger for messages about refcount changes
			/// </summary>
			private readonly ILogger _logger;

			/// <summary>
			/// Cache of created clients
			/// </summary>
			private readonly Dictionary<Type, ClientBase> _clients = new Dictionary<Type, ClientBase>();

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
			private readonly TaskCompletionSource<bool> _disposingTaskSource = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);

			/// <summary>
			/// Wraps a task allowing the disposer to wait for clients to finish using this connection
			/// </summary>
			private readonly TaskCompletionSource<bool> _disposedTaskSource = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="connectionId">The connection id</param>
			/// <param name="name">Name of the server</param>
			/// <param name="channel">The channel instance</param>
			/// <param name="logger">Logger for debug messages</param>
			public RpcSubConnection(int connectionId, string name, GrpcChannel channel, ILogger logger)
			{
				ConnectionId = connectionId;
				Name = name;
				Channel = channel;
				_logger = logger;
				_refCount = 1;
			}

			/// <summary>
			/// Creates a client of the given type using this connection
			/// </summary>
			/// <typeparam name="T">The client type</typeparam>
			/// <returns>Client of the given type</returns>
			public T CreateClient<T>() where T : ClientBase => (T)CreateClient(typeof(T));

			/// <summary>
			/// Creates a client of an arbitrary type using the current channel
			/// </summary>
			/// <param name="clientType">The client type</param>
			/// <returns></returns>
			public ClientBase CreateClient(Type clientType)
			{
				lock (_clients)
				{
					ClientBase? client;
					if (!_clients.TryGetValue(clientType, out client))
					{
						client = (ClientBase)Activator.CreateInstance(clientType, Channel)!;
					}
					return client;
				}
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
		class RpcClientRef<TClient> : IRpcClientRef<TClient> where TClient : ClientBase<TClient>
		{
			RpcSubConnection _subConnection;

			public RpcClientRef(RpcSubConnection subConnection)
			{
				_subConnection = subConnection;
				Client = _subConnection.CreateClient<TClient>();
			}

			public void Dispose()
			{
				if (_subConnection != null)
				{
					_subConnection.Release();
					_subConnection = null!;
				}
			}

			public GrpcChannel Channel => _subConnection.Channel;

			public TClient Client { get; }

			public Task DisposingTask => _subConnection.DisposingTask;
		}

		private readonly Func<CancellationToken, Task<GrpcChannel>> _createGrpcChannelAsync;
		private readonly TaskCompletionSource<bool> _stoppingTaskSource = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
		private TaskCompletionSource<RpcSubConnection> _subConnectionTaskSource = new TaskCompletionSource<RpcSubConnection>(TaskCreationOptions.RunContinuationsAsynchronously);
		private Task? _backgroundTask;
		private bool _healthy;
		private readonly ILogger _logger;

		public bool Healthy => _healthy;
		public ILogger Logger => _logger;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="createGrpcChannelAsync">Factory method for creating new GRPC channels</param>
		/// <param name="logger">Logger instance</param>
		public RpcConnection(Func<CancellationToken, Task<GrpcChannel>> createGrpcChannelAsync, ILogger logger)
		{
			_createGrpcChannelAsync = createGrpcChannelAsync;
			_logger = logger;

			_backgroundTask = Task.Run(() => ExecuteAsync());
		}

#pragma warning disable VSTHRD002 // Synchronously waiting on tasks or awaiters may cause deadlocks.
		/// <summary>
		/// Attempts to get a client reference, returning immediately if there's not one available
		/// </summary>
		/// <returns></returns>
		public IRpcClientRef<TClient>? TryGetClientRef<TClient>() where TClient : ClientBase<TClient>
		{
			Task<RpcSubConnection> subConnectionTask = _subConnectionTaskSource.Task;
			if (subConnectionTask.IsCompleted)
			{
				RpcSubConnection defaultClient = subConnectionTask.Result;
				if (defaultClient.TryAddRef())
				{
					return new RpcClientRef<TClient>(defaultClient);
				}
			}
			return null;
		}
#pragma warning restore VSTHRD002

		/// <summary>
		/// Obtains a new client reference object
		/// </summary>
		/// <typeparam name="TClient">Type of the client</typeparam>
		/// <param name="cancellationToken">Cancellation token for this request</param>
		/// <returns>New client reference</returns>
		public async Task<IRpcClientRef<TClient>> GetClientRefAsync<TClient>(CancellationToken cancellationToken) where TClient : ClientBase<TClient>
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
							_logger.LogInformation("Thread stalled for {Time:0.0}s while waiting for IRpcClientRef.\nStack trace:\n{StackTrace}", timer.Elapsed.TotalSeconds, Environment.StackTrace);
						}
						if (_stoppingTaskSource.Task.IsCompleted)
						{
							throw new OperationCanceledException("RpcConnection is being terminated");
						}
					}
				}

				// Get the new task and increase the refcount
				RpcSubConnection subConnection = await _subConnectionTaskSource.Task;
				if (subConnection.TryAddRef())
				{
					return new RpcClientRef<TClient>(subConnection);
				}
			}
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
						TaskCompletionSource<bool> newReconnectTaskSource = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
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
					if (task.IsCompleted)
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
			using (GrpcChannel channel = await _createGrpcChannelAsync(CancellationToken.None))
			{
				HordeRpc.HordeRpcClient client = new HordeRpc.HordeRpcClient(channel);

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
									subConnection = new RpcSubConnection(connectionId, response.Name, channel, _logger);
									_subConnectionTaskSource.SetResult(subConnection);
								}

								// Set the healthy flag for reporting status to the tray app
								_healthy = true;

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
					_healthy = false;

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
					_subConnectionTaskSource = new TaskCompletionSource<RpcSubConnection>(TaskCreationOptions.RunContinuationsAsynchronously);
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
	}
}
