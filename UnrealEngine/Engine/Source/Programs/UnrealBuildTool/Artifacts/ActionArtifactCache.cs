// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Logging.Abstractions;
using UnrealBuildBase;

namespace UnrealBuildTool.Artifacts
{

	/// <summary>
	/// Helper class for directory mapping
	/// </summary>
	internal class ArtifactDirectoryMapping : IArtifactDirectoryMapping
	{

		/// <summary>
		/// Creating cache
		/// </summary>
		public IActionArtifactCache? Cache;

		/// <summary>
		/// Root of the project
		/// </summary>
		public DirectoryReference? ProjectRoot;

		/// <inheritdoc/>
		public string GetDirectory(ArtifactDirectoryTree tree)
		{
			switch (tree)
			{
				case ArtifactDirectoryTree.Absolute:
					return String.Empty;
				case ArtifactDirectoryTree.Engine:
					if (Cache == null || Cache.EngineRoot == null)
					{
						throw new ApplicationException("Attempt to get engine root when not set");
					}
					return Cache.EngineRoot.FullName;
				case ArtifactDirectoryTree.Project:
					if (ProjectRoot == null)
					{
						throw new ApplicationException("Attempt to get project root when not set");
					}
					return ProjectRoot.FullName;
				default:
					throw new NotImplementedException("Unexpected directory tree value");
			}
		}

		/// <summary>
		/// Given a file, return the artifact structure for it.  This routine tests to see if the 
		/// file is under any of the well known directories.
		/// </summary>
		/// <param name="action">Action requesting the artifact</param>
		/// <param name="file">File in question</param>
		/// <param name="hash">Hash value used to populate artifact</param>
		/// <returns>Created artifact</returns>
		public ArtifactFile GetArtifact(LinkedAction action, FileItem file, IoHash hash)
		{
			if (action.ArtifactMode.HasFlag(ArtifactMode.AbsolutePath))
			{
				return CreateArtifact(ArtifactDirectoryTree.Absolute, file.AbsolutePath, hash);
			}
			if (Cache != null && Cache.EngineRoot != null && file.Location.IsUnderDirectory(Cache.EngineRoot))
			{
				return CreateArtifact(ArtifactDirectoryTree.Engine, file.Location.MakeRelativeTo(Cache.EngineRoot), hash);
			}
			else if (ProjectRoot != null && file.Location.IsUnderDirectory(ProjectRoot))
			{
				return CreateArtifact(ArtifactDirectoryTree.Project, file.Location.MakeRelativeTo(ProjectRoot), hash);
			}
			else
			{
				return CreateArtifact(ArtifactDirectoryTree.Absolute, file.AbsolutePath, hash);
			}
		}

		/// <summary>
		/// Create an artifact with the given settings
		/// </summary>
		/// <param name="tree">Directory tree</param>
		/// <param name="path">Path to artifact</param>
		/// <param name="hash">Hash of the artifact</param>
		/// <returns>The artifact</returns>
		private ArtifactFile CreateArtifact(ArtifactDirectoryTree tree, string path, IoHash hash)
		{
			return new(tree, new Utf8String(path), hash);
		}
	}

	/// <summary>
	/// Generic handler for artifacts
	/// </summary>
	internal class ActionArtifactCache : IActionArtifactCache
	{

		/// <inheritdoc/>
		public bool EnableReads { get; set; } = true;

		/// <inheritdoc/>
		public bool EnableWrites { get; set; } = true;

		/// <inheritdoc/>
		public bool LogCacheMisses { get; set; } = false;

		/// <inheritdoc/>
		public IArtifactCache ArtifactCache { get; init; }

		/// <inheritdoc/>
		public DirectoryReference? EngineRoot { get; set; } = null;

		/// <inheritdoc/>
		public DirectoryReference[]? DirectoryRoots { get; set; } = null;

		/// <summary>
		/// Logging device
		/// </summary>
		private readonly ILogger _logger;

		/// <summary>
		/// Cache of dependency files.
		/// </summary>
		private readonly CppDependencyCache _cppDependencyCache;

		/// <summary>
		/// Cache for file hashes
		/// </summary>
		private readonly FileHasher _fileHasher;

		/// <summary>
		/// Directory mapper to be used for targets without projects
		/// </summary>
		private readonly ArtifactDirectoryMapping _projectlessMapper;

		/// <summary>
		/// Artifact mappers for all targets
		/// </summary>
		private readonly ConcurrentDictionary<DirectoryReference, ArtifactDirectoryMapping> _mappings = new();

		/// <summary>
		/// Construct a new artifact handler object
		/// </summary>
		/// <param name="artifactCache">Artifact cache instance</param>
		/// <param name="cppDependencyCache">Previously created dependency cache</param>
		/// <param name="logger">Logging device</param>
		private ActionArtifactCache(IArtifactCache artifactCache, CppDependencyCache cppDependencyCache, ILogger logger)
		{
			_logger = logger;
			_cppDependencyCache = cppDependencyCache;
			ArtifactCache = artifactCache;
			_fileHasher = new(NullLogger.Instance);
			_projectlessMapper = new() { Cache = this };
		}

		/// <summary>
		/// Create a new action artifact cache using horde file based storage
		/// </summary>
		/// <param name="directory">Directory for the cache</param>
		/// <param name="cppDependencyCache">Previously created dependency cache</param>
		/// <param name="logger">Logging device</param>
		/// <returns>Action artifact cache object</returns>
		public static IActionArtifactCache CreateHordeFileCache(DirectoryReference directory, CppDependencyCache cppDependencyCache, ILogger logger)
		{
			IArtifactCache artifactCache = HordeStorageArtifactCache.CreateFileCache(directory, /*logger*/ NullLogger.Instance, false);
			return new ActionArtifactCache(artifactCache, cppDependencyCache, logger);
		}

		/// <summary>
		/// Create a new action artifact cache using horde memory based storage
		/// </summary>
		/// <param name="cppDependencyCache">Previously created dependency cache</param>
		/// <param name="logger">Logging device</param>
		/// <returns>Action artifact cache object</returns>
		public static IActionArtifactCache CreateHordeMemoryCache(CppDependencyCache cppDependencyCache, ILogger logger)
		{
			IArtifactCache artifactCache = HordeStorageArtifactCache.CreateMemoryCache(logger);
			return new ActionArtifactCache(artifactCache, cppDependencyCache, logger);
		}

		/// <inheritdoc/>
		public async Task<bool> CompleteActionFromCacheAsync(LinkedAction action, CancellationToken cancellationToken)
		{
			if (!EnableReads)
			{
				return false;
			}

			if (!action.ArtifactMode.HasFlag(ArtifactMode.Enabled))
			{
				return false;
			}

			ArtifactDirectoryMapping directoryMapping = GetDirectoryMapping(action);
			(List<FileItem> inputs, _) = CollectInputs(action, false);
			IoHash key = await GetKeyAsync(directoryMapping, action, inputs);

			ArtifactAction[] artifactActions = await ArtifactCache.QueryArtifactActionsAsync(new IoHash[] { key }, cancellationToken);

			string actionDescription = String.Empty;
			if (LogCacheMisses)
			{
				actionDescription = $"{(action.CommandDescription ?? action.CommandPath.GetFileNameWithoutExtension())} {action.StatusDescription}".Trim();
			}

			if (artifactActions.Length == 0)
			{
				if (LogCacheMisses)
				{
					_logger.LogInformation("Artifact Cache Miss: No artifact actions found for {ActionDescription}", actionDescription);
				}
				return false;
			}

			foreach (ArtifactAction artifactAction in artifactActions)
			{
				bool match = true;

				foreach (ArtifactFile input in artifactAction.Inputs)
				{
					string name = input.GetFullPath(directoryMapping);
					FileItem item = FileItem.GetItemByPath(name);
					if (!item.Exists)
					{
						if (LogCacheMisses)
						{
							_logger.LogInformation("Artifact Cache Miss: Input file missing {ActionDescription}/{File}", actionDescription, item.FullName);
						}
						match = false;
						break;
					}
					if (input.ContentHash != await _fileHasher.GetDigestAsync(item, cancellationToken))
					{
						if (LogCacheMisses)
						{
							_logger.LogInformation("Artifact Cache Miss: Content hash different {actionDescription}/{File}", actionDescription, item.FullName);
						}
						match = false;
						break;
					}
				}

				if (match)
				{
					ArtifactAction artifactActionCopy = artifactAction;
					artifactActionCopy.DirectoryMapping = directoryMapping;
					bool[]? readResults = await ArtifactCache.QueryArtifactOutputsAsync(new[] { artifactActionCopy }, cancellationToken);

					if (readResults == null || readResults.Length == 0 || !readResults[0])
					{
						return false;
					}
					else
					{
						foreach (ArtifactFile output in artifactAction.Outputs)
						{
							string outputName = output.GetFullPath(directoryMapping);
							FileItem item = FileItem.GetItemByPath(outputName);
							item.ResetCachedInfo(); // newly created outputs need refreshing
							_fileHasher.SetDigest(item, output.ContentHash);
						}
						return true;
					}
				}
			}
			return false;
		}

		/// <inheritdoc/>
		public async Task ActionCompleteAsync(LinkedAction action, CancellationToken cancellationToken)
		{
			if (!EnableWrites)
			{
				return;
			}

			if (!action.ArtifactMode.HasFlag(ArtifactMode.Enabled))
			{
				return;
			}

			ArtifactAction artifactAction = await CreateArtifactActionAsync(action, cancellationToken);
			if (artifactAction.Key != IoHash.Zero)
			{
				await ArtifactCache.SaveArtifactActionsAsync(new ArtifactAction[] { (ArtifactAction)artifactAction }, cancellationToken);
			}
			return;
		}

		/// <inheritdoc/>
		public async Task FlushChangesAsync(CancellationToken cancellationToken)
		{
			await ArtifactCache.FlushChangesAsync(cancellationToken);
		}

		/// <summary>
		/// Create a new artifact action that represents the input and output of the action
		/// </summary>
		/// <param name="action">Source action</param>
		/// <param name="cancellationToken">Cancellation token</param>
		/// <returns>Artifact action</returns>
		private async Task<ArtifactAction> CreateArtifactActionAsync(LinkedAction action, CancellationToken cancellationToken)
		{
			ArtifactDirectoryMapping directoryMapping = GetDirectoryMapping(action);
			(List<FileItem> inputs, List<FileItem>? dependencies) = CollectInputs(action, false);
			(IoHash key, IoHash actionKey) = await GetKeyAndActionKeyAsync(directoryMapping, action, inputs, dependencies);

			// We gather the output files first to make sure that all the generated files (including the dependency file) gets
			// their cached FileInfo reset.
			List<ArtifactFile> outputs = new();
			foreach (FileItem output in action.ProducedItems)
			{
				// Outputs can not be a directory.
				if (output.Attributes.HasFlag(System.IO.FileAttributes.Directory))
				{
					return new(IoHash.Zero, IoHash.Zero, Array.Empty<ArtifactFile>(), Array.Empty<ArtifactFile>());
				}
				IoHash hash = await _fileHasher.GetDigestAsync(output, cancellationToken);
				ArtifactFile artifact = directoryMapping.GetArtifact(action, output, hash);
				outputs.Add(artifact);
			}

			List<ArtifactFile> inputArtifacts = new();
			foreach (FileItem input in inputs)
			{
				IoHash hash = await _fileHasher.GetDigestAsync(input, cancellationToken);
				ArtifactFile artifact = directoryMapping.GetArtifact(action, input, hash);
				inputArtifacts.Add(artifact);
			}

			if (dependencies != null)
			{
				foreach (FileItem dependency in dependencies)
				{
					IoHash hash = await _fileHasher.GetDigestAsync(dependency, cancellationToken);
					ArtifactFile artifact = directoryMapping.GetArtifact(action, dependency, hash);
					inputArtifacts.Add(artifact);
				}
			}

			return new(key, actionKey, inputArtifacts.ToArray(), outputs.ToArray())
			{
				DirectoryMapping = directoryMapping
			};
		}

		/// <summary>
		/// Get the key has for the action
		/// </summary>
		/// <param name="directoryMapping">Directory mapping object</param>
		/// <param name="action">Source action</param>
		/// <param name="inputs">Inputs used to construct the key</param>
		/// <returns>Task returning the key</returns>
		private async Task<IoHash> GetKeyAsync(ArtifactDirectoryMapping directoryMapping, LinkedAction action, List<FileItem> inputs)
		{
			StringBuilder builder = new();
			await AppendKeyAsync(builder, directoryMapping, action, inputs);
			IoHash key = IoHash.Compute(new Utf8String(builder.ToString()));
			return key;
		}

		/// <summary>
		/// Generate the key and action key hashes for the action
		/// </summary>
		/// <param name="directoryMapping">Directory mapping object</param>
		/// <param name="action">Source action</param>
		/// <param name="inputs">Inputs used to construct the key</param>
		/// <param name="dependencies">Dependencies used to construct the key</param>
		/// <returns>Task object with the key and action key</returns>
		private async Task<(IoHash, IoHash)> GetKeyAndActionKeyAsync(ArtifactDirectoryMapping directoryMapping, LinkedAction action, List<FileItem> inputs, List<FileItem>? dependencies)
		{
			StringBuilder builder = new();
			await AppendKeyAsync(builder, directoryMapping, action, inputs);
			IoHash key = IoHash.Compute(new Utf8String(builder.ToString()));
			await AppendActionKeyAsync(builder, directoryMapping, action, dependencies);
			IoHash actionKey = IoHash.Compute(new Utf8String(builder.ToString()));
			return (key, actionKey);
		}

		/// <summary>
		/// Generate the lookup key.  This key is generated from the action's inputs.
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="directoryMapping">Directory mapping object</param>
		/// <param name="action">Source action</param>
		/// <param name="inputs">Inputs used to construct the key</param>
		/// <returns>Task object</returns>
		private async Task AppendKeyAsync(StringBuilder builder, ArtifactDirectoryMapping directoryMapping, LinkedAction action, List<FileItem> inputs)
		{
			builder.AppendLine(action.CommandVersion);
			builder.AppendLine(action.CommandArguments);
			await AppendFiles(builder, directoryMapping, action, inputs);
		}

		/// <summary>
		/// Generate the full action key.  This contains the hashes for the action's inputs and the dependent files.
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="directoryMapping">Directory mapping object</param>
		/// <param name="action">Source action</param>
		/// <param name="dependencies">Dependencies used to construct the key</param>
		/// <returns>Task object</returns>
		private async Task AppendActionKeyAsync(StringBuilder builder, ArtifactDirectoryMapping directoryMapping, LinkedAction action, List<FileItem>? dependencies)
		{
			await AppendFiles(builder, directoryMapping, action, dependencies);
		}

		/// <summary>
		/// Append the file information for a given list of files
		/// </summary>
		/// <param name="builder">Destination builder</param>
		/// <param name="directoryMapping">Directory mapping object</param>
		/// <param name="action">Source action</param>
		/// <param name="files">Collection of files</param>
		/// <returns>Task object</returns>
		private async Task AppendFiles(StringBuilder builder, ArtifactDirectoryMapping directoryMapping, LinkedAction action, List<FileItem>? files)
		{
			if (files != null)
			{
				Task<IoHash>[] waits = new Task<IoHash>[files.Count];
				for (int index = 0; index < files.Count; index++)
				{
					waits[index] = _fileHasher.GetDigestAsync(files[index]);
				}
				await Task.WhenAll(waits);
				string[] lines = new string[files.Count];
				for (int index = 0; index < files.Count; index++)
				{
					ArtifactFile artifact = directoryMapping.GetArtifact(action, files[index], waits[index].Result);
					lines[index] = $"{GetArtifactTreeName(artifact)} {artifact.Tree} {artifact.Name} {artifact.ContentHash}";
				}
				Array.Sort(lines, StringComparer.Ordinal);
				foreach (string line in lines)
				{
					builder.AppendLine(line);
				}
			}
		}

		/// <summary>
		/// Given an action, collect the list of inputs and dependencies.  This includes any processing required
		/// for disabled actions that propagate their inputs to dependent actions.
		/// </summary>
		/// <param name="action">Action in question</param>
		/// <param name="collectDependencies">If true, collect the dependencies too</param>
		/// <returns>Collection of inputs and dependencies</returns>
		private (List<FileItem>, List<FileItem>?) CollectInputs(LinkedAction action, bool collectDependencies)
		{
			// Search for any prerequisite action that is set to propagate inputs (i.e. PCH).  Collect those
			// inputs and we will be inserting those inputs in our list
			Dictionary<FileItem, List<FileItem>> substitutions = new();
			foreach (LinkedAction prereq in action.PrerequisiteActions)
			{
				if (prereq.ArtifactMode.HasFlag(ArtifactMode.PropagateInputs))
				{
					(List<FileItem> prereqInputs, List<FileItem>? prereqDependencies) = CollectInputs(prereq, collectDependencies);
					if (prereqDependencies != null)
					{
						prereqInputs.AddRange(prereqDependencies);
					}
					foreach (FileItem output in prereq.ProducedItems)
					{
						substitutions.TryAdd(output, prereqInputs);
					}
				}
			}

			HashSet<FileItem> uniques = new();
			List<FileItem> inputs = new();
			AddFileItems(uniques, substitutions, inputs, action.PrerequisiteItems);

			List<FileItem>? dependencies = null;
			if (collectDependencies && action.DependencyListFile != null)
			{
				if (_cppDependencyCache.TryGetDependencies(action.DependencyListFile, _logger, out List<FileItem>? cppDeps))
				{
					dependencies = new();
					AddFileItems(uniques, substitutions, dependencies, cppDeps);
				}
			}

			return (inputs, dependencies);
		}

		/// <summary>
		/// Add a list of file items to the collection
		/// </summary>
		/// <param name="uniques">Hash set used to detect already included file items</param>
		/// <param name="substitutions">Substitutions when a given input is found</param>
		/// <param name="outputs">Destination list</param>
		/// <param name="inputs">Source inputs</param>
		private void AddFileItems(HashSet<FileItem> uniques, Dictionary<FileItem, List<FileItem>>? substitutions, List<FileItem> outputs, IEnumerable<FileItem> inputs)
		{
			if (substitutions != null)
			{
				foreach (FileItem input in inputs)
				{
					if (substitutions.TryGetValue(input, out List<FileItem>? substituteFileItems))
					{
						AddFileItems(uniques, null, outputs, substituteFileItems);
					}
					else if (uniques.Add(input))
					{
						outputs.Add(input);
					}
				}
			}
			else
			{
				foreach (FileItem input in inputs)
				{
					if (uniques.Add(input))
					{
						outputs.Add(input);
					}
				}
			}
		}

		/// <summary>
		/// Return the tree name of the artifact
		/// </summary>
		/// <param name="artifact">Artifact in question</param>
		/// <returns>Tree name</returns>
		private static string GetArtifactTreeName(ArtifactFile artifact)
		{
			switch (artifact.Tree)
			{
				case ArtifactDirectoryTree.Absolute:
					return "Absolute";
				case ArtifactDirectoryTree.Engine:
					return "Engine";
				case ArtifactDirectoryTree.Project:
					return "Project";
				default:
					throw new NotImplementedException("Unexpected artifact directory tree type");
			}
		}

		/// <summary>
		/// Return an artifact mapper for the given action
		/// </summary>
		/// <param name="action">Action in question</param>
		/// <returns>Artifact mapper specific to the target's project directory</returns>
		private ArtifactDirectoryMapping GetDirectoryMapping(LinkedAction action)
		{
			DirectoryReference? projectDirectory = action.Target != null && action.Target.ProjectFile != null ? action.Target.ProjectFile.Directory : null;
			if (projectDirectory == null)
			{
				return _projectlessMapper;
			}
			return _mappings.GetOrAdd(projectDirectory, x =>
			{
				return new()
				{
					Cache = this,
					ProjectRoot = x,
				};
			});
		}
	}
}
