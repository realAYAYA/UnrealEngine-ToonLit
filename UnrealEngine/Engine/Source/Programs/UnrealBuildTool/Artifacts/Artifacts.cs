// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Serialization;

namespace UnrealBuildTool.Artifacts
{

	/// <summary>
	/// Artifacts can exist in different directory roots.
	/// </summary>
	public enum ArtifactDirectoryTree : byte
	{

		/// <summary>
		/// Absolute path
		/// </summary>
		Absolute,

		/// <summary>
		/// Input/Output exists in the engine directory tree
		/// </summary>
		Engine,

		/// <summary>
		/// Input/Outputs exists in the project directory tree
		/// </summary>
		Project,
	}

	/// <summary>
	/// Represents a single artifact file
	/// </summary>
	/// <param name="Tree">Directory tree containing the artifact</param>
	/// <param name="Name">Name of the artifact</param>
	/// <param name="ContentHash">Hash of the artifact contents</param>
	public record struct ArtifactFile(ArtifactDirectoryTree Tree, Utf8String Name, IoHash ContentHash)
	{
		/// <summary>
		/// The full path of the artifact
		/// </summary>
		/// <param name="mapping">Mapping object to use instead of embedded object</param>
		/// <returns>Full path of the artifact</returns>
		public string GetFullPath(IArtifactDirectoryMapping? mapping)
		{
			if (Tree == ArtifactDirectoryTree.Absolute)
			{
				return Name.ToString();
			}
			if (mapping == null)
			{
				throw new ApplicationException("Attempt to get the full path without mapping interface set");
			}
			string directory = mapping.GetDirectory(Tree);
			return Path.Combine(directory, Name.ToString());
		}
	}

	/// <summary>
	/// Collection of inputs and outputs for an action
	/// </summary>
	/// <param name="Key">The hash of the primary input and the environment</param>
	/// <param name="ActionKey">The unique hash for all inputs and the environment</param>
	/// <param name="Inputs">Information about all inputs</param>
	/// <param name="Outputs">Information about all outputs</param>
	public record struct ArtifactAction(IoHash Key, IoHash ActionKey, ArtifactFile[] Inputs, ArtifactFile[] Outputs)
	{
		/// <summary>
		/// Directory mapping object
		/// </summary>
		public IArtifactDirectoryMapping? DirectoryMapping { get; set; } = null;

		/// <inheritdoc/>
		public override int GetHashCode()
		{
			return HashCode.Combine(Key.GetHashCode(), ActionKey.GetHashCode(), Inputs.GetHashCode(), Outputs.GetHashCode());
		}
	}

	/// <summary>
	/// Collection of helper extension methods for working with IMemoryReader/Writer
	/// </summary>
	static class ArtifactSerializationExtensions
	{

		/// <summary>
		/// Read an artifact structure
		/// </summary>
		/// <param name="reader">Source reader</param>
		/// <returns>Read artifact structure</returns>
		public static ArtifactFile ReadArtifact(this IMemoryReader reader)
		{
			ArtifactDirectoryTree tree = (ArtifactDirectoryTree)reader.ReadUInt8();
			Utf8String name = reader.ReadUtf8String();
			IoHash contentHash = reader.ReadIoHash();
			return new ArtifactFile(tree, name, contentHash);
		}

		/// <summary>
		/// Write an artifact structure
		/// </summary>
		/// <param name="writer">Destination writer</param>
		/// <param name="artifact">Artifact to be written</param>
		public static void WriteArtifact(this IMemoryWriter writer, ArtifactFile artifact)
		{
			writer.WriteUInt8((byte)artifact.Tree);
			writer.WriteUtf8String(artifact.Name);
			writer.WriteIoHash(artifact.ContentHash);
		}

		/// <summary>
		/// Read an artifact action structure
		/// </summary>
		/// <param name="reader">Source reader</param>
		/// <returns>Read artifact action structure</returns>
		public static ArtifactAction ReadArtifactAction(this IMemoryReader reader)
		{
			IoHash key = reader.ReadIoHash();
			IoHash actionKey = reader.ReadIoHash();
			ArtifactFile[] inputs = reader.ReadVariableLengthArray(() => reader.ReadArtifact());
			ArtifactFile[] outputs = reader.ReadVariableLengthArray(() => reader.ReadArtifact());
			return new ArtifactAction(key, actionKey, inputs, outputs);
		}

		/// <summary>
		/// Write an artifact action structure
		/// </summary>
		/// <param name="writer">Destination writer</param>
		/// <param name="artifactAction">Artifact action to be written</param>
		public static void WriteArtifactAction(this IMemoryWriter writer, ArtifactAction artifactAction)
		{
			writer.WriteIoHash(artifactAction.Key);
			writer.WriteIoHash(artifactAction.ActionKey);
			writer.WriteVariableLengthArray(artifactAction.Inputs, x => writer.WriteArtifact(x));
			writer.WriteVariableLengthArray(artifactAction.Outputs, x => writer.WriteArtifact(x));
		}
	}

	/// <summary>
	/// Represents the state of the cache.  It is expect that after construction, the cache
	/// can be in pending state, but this is optional. From the pending state, the cache 
	/// becomes available or unavailable.
	/// </summary>
	public enum ArtifactCacheState
	{

		/// <summary>
		/// The cache is still initializing
		/// </summary>
		Pending,

		/// <summary>
		/// There has been some form of cache failure and it is unavailable
		/// </summary>
		Unavailable,

		/// <summary>
		/// The cache is functional and ready to process requests
		/// </summary>
		Available,
	}

	/// <summary>
	/// Interface used to map a directory tree to an actual directory
	/// </summary>
	public interface IArtifactDirectoryMapping
	{

		/// <summary>
		/// Return the directory root for the given tree
		/// </summary>
		/// <param name="tree">Tree in question</param>
		/// <returns>Directory root</returns>
		public string GetDirectory(ArtifactDirectoryTree tree);
	}

	/// <summary>
	/// Interface for querying and adding artifacts
	/// </summary>
	public interface IArtifactCache
	{

		/// <summary>
		/// Return true if the cache is ready to process requests
		/// </summary>
		/// <returns></returns>
		public ArtifactCacheState State { get; }

		/// <summary>
		/// Return task that waits for the cache to be ready
		/// </summary>
		/// <returns>State of the artifact cache</returns>
		public Task<ArtifactCacheState> WaitForReadyAsync();

		/// <summary>
		/// Given a collection of partial keys return all matching artifacts
		/// </summary>
		/// <param name="partialKeys">Source file key</param>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>Collection of all known artifacts</returns>
		public Task<ArtifactAction[]> QueryArtifactActionsAsync(IoHash[] partialKeys, CancellationToken cancellationToken);

		/// <summary>
		/// Query the actual contents of a group of artifacts
		/// </summary>
		/// <param name="artifactActions">Actions to be read</param>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>Dictionary of the artifacts</returns>
		public Task<bool[]?> QueryArtifactOutputsAsync(ArtifactAction[] artifactActions, CancellationToken cancellationToken);

		/// <summary>
		/// Save new artifact to the cache
		/// </summary>
		/// <param name="artifactActions">Collection of artifacts to be saved</param>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>Asynchronous task objects</returns>
		public Task SaveArtifactActionsAsync(ArtifactAction[] artifactActions, CancellationToken cancellationToken);

		/// <summary>
		/// Flush all updates in the cache asynchronously.
		/// </summary>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>Asynchronous task objects</returns>
		public Task FlushChangesAsync(CancellationToken cancellationToken);
	}

	/// <summary>
	/// Interface for action specific support of artifacts.
	/// </summary>
	interface IActionArtifactCache
	{

		/// <summary>
		/// Underlying artifact cache
		/// </summary>
		public IArtifactCache ArtifactCache { get; }

		/// <summary>
		/// If true, reads will be serviced
		/// </summary>
		public bool EnableReads { get; set; }

		/// <summary>
		/// If true, writes will be propagated to storage
		/// </summary>
		public bool EnableWrites { get; set; }

		/// <summary>
		/// If true, log all cache misses.  Defaults to false.
		/// </summary>
		public bool LogCacheMisses { get; set; }

		/// <summary>
		/// Root location of the engine
		/// </summary>
		public DirectoryReference? EngineRoot { get; set; }

		/// <summary>
		/// Array of extra directory roots when handling relative directories.
		/// The array MUST be consistent between runs since only the index is saved.
		/// </summary>
		public DirectoryReference[]? DirectoryRoots { get; set; }

		/// <summary>
		/// Complete an action from existing cached data
		/// </summary>
		/// <param name="action">Action to be completed</param>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>True if it has been completed, false if not</returns>
		public Task<bool> CompleteActionFromCacheAsync(LinkedAction action, CancellationToken cancellationToken);

		/// <summary>
		/// Save the output for a completed action
		/// </summary>
		/// <param name="action">Completed action</param>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>Asynchronous task object</returns>
		public Task ActionCompleteAsync(LinkedAction action, CancellationToken cancellationToken);

		/// <summary>
		/// Flush all updates in the cache asynchronously.
		/// </summary>
		/// <param name="cancellationToken">Token to be used to cancel operations</param>
		/// <returns>Asynchronous task object</returns>
		public Task FlushChangesAsync(CancellationToken cancellationToken);
	}
}
