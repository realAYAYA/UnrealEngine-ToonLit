// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using IdentityModel.Client;
using EpicGames.Core;
using EpicGames.Jupiter;
using EpicGames.OIDC;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;

#nullable disable

namespace UnrealGameSync
{
	class JupiterMonitor: IArchiveInfoSource, IDisposable
	{
		private readonly object _archivesLock = new object();
		private IReadOnlyList<IArchiveInfo> _archives;

		private readonly OidcTokenManager _tokenManager;
		private readonly string _jupiterNamespace;
		private readonly string _providerIdentifier;
		private readonly string _expectedBranch;
		private readonly Uri _jupiterUrl;

		private readonly Timer _updateTimer;
		private readonly ILogger _logger;

		public IReadOnlyList<IArchiveInfo> AvailableArchives
		{
			get
			{
				lock (_archivesLock)
				{
					return _archives;
				}
			}
		}

		private JupiterMonitor(OidcTokenManager inTokenManager, ILogger inLogger, string inNamespace, string inUrl,
			string inProviderIdentifier, string inExpectedBranch)
		{
			_tokenManager = inTokenManager;
			_jupiterNamespace = inNamespace;
			_providerIdentifier = inProviderIdentifier;
			_expectedBranch = inExpectedBranch;
			_jupiterUrl = new Uri(inUrl);

			_logger = inLogger;
			_updateTimer = new Timer(DoUpdate, null, TimeSpan.Zero, TimeSpan.FromMinutes(5));
		}
		public void Dispose()
		{
			_updateTimer?.Dispose();
		}

		private async void DoUpdate(object state)
		{
			_logger.LogInformation("Starting poll of JupiterMonitor for namespace {JupiterNamespace}", _jupiterNamespace);
			try
			{
				IReadOnlyList<IArchiveInfo> newArchives = await GetAvailableArchives();
				lock (_archivesLock)
				{
					_archives = newArchives;
				}

			}
			catch (Exception exception)
			{
				_logger.LogError(exception, "Exception occured during poll!");
			}
		}

		public static JupiterMonitor CreateFromConfigFile(OidcTokenManager tokenManager, ILogger<JupiterMonitor> logger, ConfigFile configFile, string selectedProjectIdentifier)
		{
			ConfigSection jupiterConfigSection = configFile.FindSection("Jupiter");
			if (jupiterConfigSection == null)
				return null;

			string jupiterUrl = jupiterConfigSection.GetValue("JupiterUrl");
			string oidcProviderIdentifier = jupiterConfigSection.GetValue("OIDCProviderIdentifier");

			ConfigSection projectConfigSection = configFile.FindSection(selectedProjectIdentifier);
			if (projectConfigSection == null)
				return null;

			string jupiterNamespace = projectConfigSection.GetValue("JupiterNamespace");
			// Is no namespace has been specified we are unable to fetch builds
			if (jupiterNamespace == null)
				return null;

			string expectedBranch = projectConfigSection.GetValue("ExpectedBranch");
			// If we do not know which branch to fetch we can not list builds, as it would risk getting binaries from other branches
			if (expectedBranch == null)
				return null;

			oidcProviderIdentifier = projectConfigSection.GetValue("OIDCProviderIdentifier") ?? oidcProviderIdentifier;

			// with no oidc provider we are unable to login, thus it is required
			if (oidcProviderIdentifier == null)
				return null;

			// project specific overrides
			jupiterUrl = projectConfigSection.GetValue("JupiterUrl") ?? jupiterUrl;

			return new JupiterMonitor(tokenManager, logger, jupiterNamespace, jupiterUrl, oidcProviderIdentifier, expectedBranch);
		}

		private async Task<IReadOnlyList<IArchiveInfo>> GetAvailableArchives()
		{
			OidcTokenInfo token = await _tokenManager.GetAccessToken(_providerIdentifier);

			List<JupiterArchiveInfo> newArchives = new List<JupiterArchiveInfo>();
			using (HttpClient client = new HttpClient())
			{
				client.BaseAddress = _jupiterUrl;
				client.SetBearerToken(token.AccessToken);

				string responseBody = await client.GetStringAsync($"/api/v1/c/tree-root/{_jupiterNamespace}");
				TreeRootListResponse response = JsonSerializer.Deserialize<TreeRootListResponse>(responseBody, new JsonSerializerOptions {PropertyNameCaseInsensitive = true});

				foreach (string treeRoot in response.TreeRoots)
				{
					// fetch info on this tree root
					string resourceUrl = $"/api/v1/c/tree-root/{_jupiterNamespace}/{treeRoot}";
					string responseBodyTreeReference = await client.GetStringAsync(resourceUrl);
					TreeRootReferenceResponse responseTreeReference = JsonSerializer.Deserialize<TreeRootReferenceResponse>(responseBodyTreeReference, new JsonSerializerOptions { PropertyNameCaseInsensitive = true });

					Dictionary<string, string> metadata = responseTreeReference.Metadata.ToDictionary(pair => pair.Key, pair => pair.Value.ToString());

					string archiveType, project, branch, changelistString;

					if (!metadata.TryGetValue("ArchiveType", out archiveType) ||
					    !metadata.TryGetValue("Project", out project) ||
					    !metadata.TryGetValue("Branch", out branch) ||
						!metadata.TryGetValue("Changelist", out changelistString))
					{
						continue;
					}

					// skip if we do not have metadata as we need that to be able to determine if this root is of the type we want
					if (branch == null || project == null || archiveType == null || changelistString == null)
						continue;

					if (!string.Equals(_expectedBranch, branch, StringComparison.InvariantCultureIgnoreCase))
					{
						continue;
					}

					int changelist;
					if (!int.TryParse(changelistString, out changelist))
					{
						continue; // invalid changelist format
					}

					JupiterArchiveInfo existingArchive = newArchives.FirstOrDefault(info =>
						string.Equals(info.Name, project, StringComparison.InvariantCultureIgnoreCase) &&
						string.Equals(info.Type, archiveType, StringComparison.InvariantCultureIgnoreCase));
					if (existingArchive == null)
					{
						JupiterArchiveInfo archive = new JupiterArchiveInfo(project, archiveType, _jupiterUrl.ToString(), _jupiterNamespace);
						newArchives.Add(archive);
						existingArchive = archive;
					}

					existingArchive.AddArchiveVersion(changelist, responseTreeReference.TreeReferenceKey);
				}
			}

			return newArchives.AsReadOnly();
		}

		private class TreeRootListResponse
		{
			public List<string> TreeRoots { get; set; }
		}

		private class TreeRootReferenceResponse
		{
			public string TreeReferenceKey { get; set; }
			public string TreeHash { get; set; }

			public Dictionary<string, object> Metadata { get; set; }

			public string TreeRootState { get; set; }
		}

		private class JupiterArchiveInfo : IArchiveInfo
		{
			private readonly string _jupiterUrl;
			private readonly string _jupiterNamespace;
			private readonly Dictionary<int, string> _changeToKey = new Dictionary<int, string>();

			public JupiterArchiveInfo(string inName, string inType, string inJupiterUrl, string inJupiterNamespace)
			{
				Name = inName;
				Type = inType;

				_jupiterUrl = inJupiterUrl;
				_jupiterNamespace = inJupiterNamespace;
			}

			public string Name { get; }
			public string Type { get; }

			public string BasePath
			{
				get { return null; } 
			}

			public string Target => throw new NotSupportedException();

			public bool Exists()
			{
				return _changeToKey.Count > 0;
			}

			public bool TryGetArchiveKeyForChangeNumber(int changeNumber, out string archiveKey)
			{
				return _changeToKey.TryGetValue(changeNumber, out archiveKey);
			}

			public Task<bool> DownloadArchive(IPerforceConnection _, string archiveKey, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken)
			{
				try
				{
					Progress<Tuple<float, FileReference>> progressCallback = new Progress<Tuple<float, FileReference>>(progressUpdate =>
					{
						(float progressFraction, FileReference fileReference) = progressUpdate;
						progress.Set(progressFraction);
						logger.LogInformation("Writing {FileName}", fileReference.FullName);
					});

					// place the manifest for the Jupiter download next to the UGS manifest
					FileReference ugsManifestFileReference = manifestFileName;
					FileReference jupiterManifestFileReference = FileReference.Combine(ugsManifestFileReference.Directory, "Jupiter-Manifest.json");

					DirectoryReference rootDirectory = localRootPath;
					JupiterFileTree fileTree = new JupiterFileTree(rootDirectory, InDeferReadingFiles: true);
					Task<List<FileReference>> downloadTask = fileTree.DownloadFromJupiter(jupiterManifestFileReference, _jupiterUrl, _jupiterNamespace, archiveKey, progressCallback);
					downloadTask.Wait();

					List<FileReference> writtenFiles = downloadTask.Result;
					ArchiveManifest archiveManifest = new ArchiveManifest();
					foreach (FileReference file in writtenFiles)
					{
						archiveManifest.Files.Add(new ArchiveManifestFile(file.FullName, file.ToFileInfo().Length, DateTime.Now));
					}

					// Write it out to a temporary file, then move it into place
					FileReference tempManifestFileName = manifestFileName + ".tmp";
					using (FileStream outputStream = FileReference.Open(tempManifestFileName, FileMode.Create, FileAccess.Write))
					{
						archiveManifest.Write(outputStream);
					}
					FileReference.Move(tempManifestFileName, manifestFileName);

					return Task.FromResult(true);
				}
				catch (Exception exception)
				{
					logger.LogError(exception, "Exception occured when downloading build from Jupiter with key {Key}.", archiveKey);
					return Task.FromResult(false);
				}
			}

			public void AddArchiveVersion(int changelist, string key)
			{
				_changeToKey[changelist] = key;
			}
		}
	}
}