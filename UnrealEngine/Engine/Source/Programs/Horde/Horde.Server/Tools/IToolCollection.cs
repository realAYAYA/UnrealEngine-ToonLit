// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Server;
using Horde.Server.Storage;

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
		/// <returns></returns>
		Task<ITool?> GetAsync(ToolId id, GlobalConfig globalConfig);

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="stream">Stream containing the tool data</param>
		/// <param name="globalConfig">The current configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		Task<ITool?> CreateDeploymentAsync(ITool tool, ToolDeploymentConfig options, Stream stream, GlobalConfig globalConfig, CancellationToken cancellationToken);

		/// <summary>
		/// Adds a new deployment to the given tool. The new deployment will replace the current active deployment.
		/// </summary>
		/// <param name="tool">The tool to update</param>
		/// <param name="options">Options for the new deployment</param>
		/// <param name="locator">Handle to the root node containing the tool data</param>
		/// <param name="globalConfig">The current configuration</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Updated tool document, or null if it does not exist</returns>
		Task<ITool?> CreateDeploymentAsync(ITool tool, ToolDeploymentConfig options, NodeLocator locator, GlobalConfig globalConfig, CancellationToken cancellationToken);

		/// <summary>
		/// Updates the state of the current deployment
		/// </summary>
		/// <param name="tool">Tool to be updated</param>
		/// <param name="deploymentId">Identifier for the deployment to modify</param>
		/// <param name="action">New state of the deployment</param>
		/// <returns></returns>
		Task<ITool?> UpdateDeploymentAsync(ITool tool, ToolDeploymentId deploymentId, ToolDeploymentState action);

		/// <summary>
		/// Gets the storage client for a particular tool
		/// </summary>
		/// <param name="tool">The tool to get a storage client for</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Instance of the storage client</returns>
		Task<IStorageClient> GetStorageClientAsync(ITool tool, CancellationToken cancellationToken);

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
