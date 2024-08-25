// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Tools;
using Horde.Server.Server;

namespace Horde.Server.Tools
{
	/// <summary>
	/// Collection of tool documents
	/// </summary>
	public interface IToolCollection
	{
		/// <summary>
		/// Gets a tool with the given identifier
		/// </summary>
		/// <param name="id">The tool identifier</param>
		/// <param name="globalConfig">The current global configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<ITool?> GetAsync(ToolId id, GlobalConfig globalConfig, CancellationToken cancellationToken = default);

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="stream">Stream containing the tool data</param>
		/// <param name="globalConfig">The current configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		Task<ITool?> CreateDeploymentAsync(ITool tool, ToolDeploymentConfig options, Stream stream, GlobalConfig globalConfig, CancellationToken cancellationToken = default);

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="target">Path to the root node containing the tool data</param>
		/// <param name="globalConfig">The current configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		Task<ITool?> CreateDeploymentAsync(ITool tool, ToolDeploymentConfig options, BlobRefValue target, GlobalConfig globalConfig, CancellationToken cancellationToken = default);

		/// <summary>
		/// Updates the state of the current deployment
		/// </summary>
		/// <param name="tool">Tool to be updated</param>
		/// <param name="deploymentId">Identifier for the deployment to modify</param>
		/// <param name="action">New state of the deployment</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<ITool?> UpdateDeploymentAsync(ITool tool, ToolDeploymentId deploymentId, ToolDeploymentState action, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets the storage backend for a particular tool
		/// </summary>
		/// <param name="tool">The tool to get a storage backend for</param>
		/// <returns>Instance of the backend client</returns>
		IStorageBackend CreateStorageBackend(ITool tool);

		/// <summary>
		/// Gets the storage backend for a particular tool
		/// </summary>
		/// <param name="tool">The tool to get a storage backend for</param>
		/// <returns>Instance of the backend client</returns>
		IStorageClient CreateStorageClient(ITool tool);

		/// <summary>
		/// Opens a stream to the data for a particular deployment
		/// </summary>
		/// <param name="tool">Identifier for the tool</param>
		/// <param name="deployment">The deployment</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the data</returns>
		Task<Stream> GetDeploymentZipAsync(ITool tool, IToolDeployment deployment, CancellationToken cancellationToken);
	}
}
