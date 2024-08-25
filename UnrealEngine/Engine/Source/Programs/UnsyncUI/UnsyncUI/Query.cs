// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using System.Threading;
using System;
using System.IO;
using System.Diagnostics;
using System.Text.Json;
using System.Linq;
using System.Collections;

namespace UnsyncUI
{
	public class UnsyncQueryConfig
	{
		public String unsyncPath;
		public String proxyAddress;
	}

	class SearchQueryResultEntry
	{
		public String path { get; set; }

		public bool is_directory { get; set; }
		public UInt64 size { get; set; }
		public UInt64 mtime { get; set; }
	}

	class SearchQueryResult
	{
		public String root { get; set; }
		public List<SearchQueryResultEntry> entries { get; set; }
	}

	// Subset of decoded JWT claims
	public class LoginQueryResult
	{
		public String aud { get; set; }
		public String sub { get; set; }
		public String iss { get; set; }
		public List<String> scp { get; set; }
		public UInt64 iat { get; set; }
		public UInt64 exp { get; set; }
		public List<String> groups { get; set; }
	}

	public class UnsyncQueryUtil
	{
		UnsyncQueryConfig Config;
		public UnsyncQueryUtil(UnsyncQueryConfig InConfig)
		{
			Config = InConfig;
		}

		public UnsyncQueryUtil(string unsyncPath, string proxyAddress)
		{
			Config = new UnsyncQueryConfig();
			Config.unsyncPath = unsyncPath;
			Config.proxyAddress = proxyAddress;
		}

		public LoginQueryResult Login()
		{
			String argsStr = $"login --decode --proxy {Config.proxyAddress}";
			AsyncProcess proc = new AsyncProcess(Config.unsyncPath, argsStr);
			CancellationToken cancellationToken = new CancellationToken();

			var responseJson = "";

			var LoginTask = Task.Run(async () => {
				// TODO: read stderr stream and somehow report status/errors
				await foreach (var str in proc.RunAsync(cancellationToken, false /*ReadStdErr*/))
				{
					responseJson += str;
				}
			});
			LoginTask.Wait();

			LoginQueryResult queryResult = JsonSerializer.Deserialize<LoginQueryResult>(responseJson);

			return queryResult;
		}
	}

	

	public class UnsyncDirectoryEnumerator : IDirectoryEnumerator
	{
		private UnsyncQueryConfig Config;

		private Config.Project ProjectSchema;

		private String DirectoryRoot;
		private Config.Directory DirectorySchema;

		private bool Initialized = false;

		private class Entry
		{
			public String FullPath;
			public int Depth;
			public UInt64 Size;
			public UInt64 MTime;
			public bool IsDirectory;
		}

		List<Entry> Entries;

		public UnsyncDirectoryEnumerator(Config.Project ProjectSchema, UnsyncQueryConfig Config)
		{
			this.Config = Config;
			this.ProjectSchema = ProjectSchema;
		}
		public UnsyncDirectoryEnumerator(String Path, Config.Directory DirectorySchema, UnsyncQueryConfig Config)
		{
			this.Config = Config;
			this.DirectoryRoot = Path;
			this.DirectorySchema = DirectorySchema;
		}

		private async Task LazyInit(CancellationToken cancellationToken)
		{
			if (!Initialized)
			{
				if (ProjectSchema != null)
				{
					await InitProject(cancellationToken);
				}
				else if (DirectorySchema != null)
				{
					await InitDirectory(cancellationToken);
				}
				else
				{
					throw new Exception("Unexpected UnsyncDirectoryEnumerator configuration. Either project or directory must be specified.");
				}
			}
		}
		class BuildTraversalState
		{
			public bool FoundCL = false;
			public bool FoundStream = false;
		}

		private void ProcessQueryResult(SearchQueryResult queryResult)
		{
			if (Entries == null)
			{
				Entries = new List<Entry>();
			}

			foreach (var queryEntry in queryResult.entries)
			{
				Entry entry = new Entry();
				entry.FullPath = Path.Combine(queryResult.root, queryEntry.path);
				entry.Depth = GetDirectoryDepth(entry.FullPath);
				entry.IsDirectory = queryEntry.is_directory;
				entry.MTime = queryEntry.mtime;
				entry.Size = queryEntry.size;
				Entries.Add(entry);
			}
		}

		private async Task RunQueries(List<String> queryStrings, CancellationToken cancellationToken)
		{
			foreach (String query in queryStrings)
			{
				String argsStr = query + $" --proxy {Config.proxyAddress}";

				var proc = new AsyncProcess(Config.unsyncPath, argsStr);
				var responseJson = "";
				// TODO: read stderr stream and somehow report status/errors
				await foreach (var str in proc.RunAsync(cancellationToken, false /*ReadStdErr*/))
				{
					responseJson += str;
				}

				if (proc.ExitCode == 0)
				{
					try
					{
						SearchQueryResult queryResult = JsonSerializer.Deserialize<SearchQueryResult>(responseJson);
						ProcessQueryResult(queryResult);
					}
					catch (Exception ex)
					{
						App.Current.LogError("Exception while parsing unsync query JSON: " + ex.Message);
					}
				}
			}
		}

		private async Task InitProject(CancellationToken cancellationToken)
		{
			String baseQuery = $"query search \"{ProjectSchema.Root}\"";

			BuildTraversalState traversalState = new BuildTraversalState();
			List<String> queryStrings = new List<String>();

			foreach (Config.Directory childDir in ProjectSchema.Children)
			{
				GenerateQueries(ref traversalState, childDir, baseQuery, ref queryStrings);
			}

			await RunQueries(queryStrings, cancellationToken);

			Initialized = true;
		}

		private void GenerateQueries(ref BuildTraversalState traversalState, Config.Directory dir, String query, ref List<String> output)
		{
			if (traversalState != null)
			{
				if (dir.CL != null)
				{
					traversalState.FoundCL = true;
				}

				if (dir.Stream != null)
				{
					traversalState.FoundStream = true;
				}

				if (traversalState.FoundCL && traversalState.FoundStream)
				{
					// Stop visiting directories early when rules for matching CL and Stream are found in the project config.
					// Typically this just includes the project root directory, but if the builds are organized
					// by Stream and then by CL, we want to traverse a deeper hierarchy.

					output.Add(query);

					return;
				}
			}

			// Adding a regex rule for the directory will instruct unsync to list all of its children
			query += $" \"{dir.Regex}\"";

			if (dir.SubDirectories.Any())
			{
				foreach (Config.Directory childDir in dir.SubDirectories)
				{
					GenerateQueries(ref traversalState, childDir, query, ref output);
				}
			}
			else
			{
				output.Add(query);
			}
		}

		private async Task InitDirectory(CancellationToken cancellationToken)
		{
			String baseQuery = $"query search \"{DirectoryRoot}\"";

			BuildTraversalState traversalState = null;
			List<String> queryStrings = new List<String>();

			foreach (Config.Directory childDir in DirectorySchema.SubDirectories)
			{
				GenerateQueries(ref traversalState, childDir, baseQuery, ref queryStrings);
			}

			await RunQueries(queryStrings, cancellationToken);

			Initialized = true;
		}

		private int GetDirectoryDepth(string path)
		{
			int depth = 0;
			foreach (var c in path)
			{
				if (c == Path.DirectorySeparatorChar)
				{
					depth += 1;
				}
			}
			return depth;
		}

		private struct EntryFilter
		{
			public bool IncludeFiles;
			public bool IncludeDirectories;

			public bool ShouldInclude(Entry entry)
			{
				if (entry.IsDirectory && !IncludeDirectories)
				{
					return false;
				}

				if (!entry.IsDirectory && !IncludeFiles)
				{
					return false;
				}

				return true;
			}
		}

		private Task<IEnumerable<string>> EnumerateEntries(string path, CancellationToken token, EntryFilter filter)
		{
			var tcs = new TaskCompletionSource<IEnumerable<string>>();
			Task.Run(async () =>
			{
				using var cancel = token.Register(() => tcs.TrySetCanceled());
				try
				{
					await LazyInit(token);
					var dirs = new List<string>();

					int pathDepth = GetDirectoryDepth(path);

					string requiredPrefix = path + Path.DirectorySeparatorChar;

					// Doesn't run very often and typically matches most entries anyway,
					// so can just do a naive search without building any hierarchies.
					foreach (var entry in Entries)
					{
						if (!filter.ShouldInclude(entry))
						{
							continue;
						}

						// We want only directories that are immediate children of the given path
						if (entry.Depth == 1 + pathDepth
							&& entry.FullPath.StartsWith(requiredPrefix))
						{
							//String relativePath = Path.GetRelativePath(path, entry.FullPath);
							dirs.Add(entry.FullPath);
						}
					}

					tcs.TrySetResult(dirs);
				}
				catch (OperationCanceledException)
				{
					tcs.TrySetCanceled();
				}
			});
			return tcs.Task;
		}

		public Task<IEnumerable<string>> EnumerateDirectories(string path, CancellationToken token)
		{
			EntryFilter filter;
			filter.IncludeDirectories = true;
			filter.IncludeFiles = false;
			return EnumerateEntries(path, token, filter);
		}
		public Task<IEnumerable<string>> EnumerateFiles(string path, CancellationToken token)
		{
			EntryFilter filter;
			filter.IncludeDirectories = false;
			filter.IncludeFiles = true;
			return EnumerateEntries(path, token, filter);
		}
	}

}
