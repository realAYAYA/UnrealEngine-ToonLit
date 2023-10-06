// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	public interface IArchiveInfo
	{
		public const string EditorArchiveType = "Editor";

		string Name { get; }
		string Type { get; }
		string BasePath { get; }
		string? Target { get; }

		bool Exists();
		bool TryGetArchiveKeyForChangeNumber(int changeNumber, int maxChangeNumber, [NotNullWhen(true)] out string? archiveKey);
		Task<bool> DownloadArchive(IPerforceConnection perforce, string archiveKey, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken);
	}

	public class PerforceArchiveInfo : IArchiveInfo
	{
		public string Name { get; }
		public string Type { get; }
		public string DepotPath { get; }
		public string? Target { get; }

		public string BasePath => DepotPath;

		// TODO: executable/configuration?
		public SortedList<int, string> ChangeNumberToFileRevision { get; } = new SortedList<int, string>();

		public PerforceArchiveInfo(string name, string type, string depotPath, string? target)
		{
			Name = name;
			Type = type;
			DepotPath = depotPath;
			Target = target;
		}

		public override bool Equals(object? other)
		{
			PerforceArchiveInfo? otherArchive = other as PerforceArchiveInfo;
			return otherArchive != null && Name == otherArchive.Name && Type == otherArchive.Type && DepotPath == otherArchive.DepotPath && Target == otherArchive.Target && Enumerable.SequenceEqual(ChangeNumberToFileRevision, otherArchive.ChangeNumberToFileRevision);
		}

		public override int GetHashCode()
		{
			throw new NotSupportedException();
		}

		public bool Exists()
		{
			return ChangeNumberToFileRevision.Count > 0;
		}

		public static bool TryParseConfigEntry(string text, [NotNullWhen(true)] out PerforceArchiveInfo? info)
		{
			ConfigObject obj = new ConfigObject(text);

			string? name = obj.GetValue("Name", null);
			if (name == null)
			{
				info = null;
				return false;
			}

			string? depotPath = obj.GetValue("DepotPath", null);
			if (depotPath == null)
			{
				info = null;
				return false;
			}

			string? target = obj.GetValue("Target", null);

			string type = obj.GetValue("Type", null) ?? name;

			info = new PerforceArchiveInfo(name, type, depotPath, target);
			return true;
		}

		public bool TryGetArchiveKeyForChangeNumber(int changeNumber, int maxChangeNumber, [NotNullWhen(true)] out string? archiveKey)
		{
			int idx = ChangeNumberToFileRevision.Keys.AsReadOnly().BinarySearch(changeNumber);
			if (idx >= 0)
			{
				archiveKey = ChangeNumberToFileRevision.Values[idx];
				return true;
			}

			int nextIdx = ~idx;
			if (nextIdx < ChangeNumberToFileRevision.Count && ChangeNumberToFileRevision.Keys[nextIdx] <= maxChangeNumber)
			{
				archiveKey = ChangeNumberToFileRevision.Values[nextIdx];
				return true;
			}

			archiveKey = null;
			return false;
		}

		public async Task<bool> DownloadArchive(IPerforceConnection perforce, string archiveKey, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken)
		{
			DirectoryReference configDir = UserSettings.GetConfigDir(localRootPath);
			UserSettings.CreateConfigDir(configDir);

			FileReference tempZipFileName = FileReference.Combine(configDir, "archive.zip");
			try
			{
				PrintRecord record = await perforce.PrintAsync(tempZipFileName.FullName, archiveKey, cancellationToken);
				if (tempZipFileName.ToFileInfo().Length == 0)
				{
					return false;
				}
				ArchiveUtils.ExtractFiles(tempZipFileName, localRootPath, manifestFileName, progress, logger);
			}
			finally
			{
				FileReference.SetAttributes(tempZipFileName, FileAttributes.Normal);
				FileReference.Delete(tempZipFileName);
			}

			return true;
		}

		public override string ToString()
		{
			return Name;
		}
	}

	public static class PerforceArchive
	{
		public static async Task<List<PerforceArchiveInfo>> EnumerateAsync(IPerforceConnection perforce, ConfigFile latestProjectConfigFile, string projectIdentifier, CancellationToken cancellationToken)
		{
			List<PerforceArchiveInfo> newArchives = new List<PerforceArchiveInfo>();

			// Find all the zipped binaries under this stream
			ConfigSection? projectConfigSection = latestProjectConfigFile.FindSection(projectIdentifier);
			if (projectConfigSection != null)
			{
				// Legacy
				string? legacyEditorArchivePath = projectConfigSection.GetValue("ZippedBinariesPath", null);
				if (legacyEditorArchivePath != null)
				{
					newArchives.Add(new PerforceArchiveInfo("Editor", "Editor", legacyEditorArchivePath, null));
				}

				// New style
				foreach (string archiveValue in projectConfigSection.GetValues("Archives", Array.Empty<string>()))
				{
					PerforceArchiveInfo? archive;
					if (PerforceArchiveInfo.TryParseConfigEntry(archiveValue, out archive))
					{
						newArchives.Add(archive);
					}
				}

				// Make sure the zipped binaries path exists
				foreach (PerforceArchiveInfo newArchive in newArchives)
				{
					PerforceResponseList<FileLogRecord> response = await perforce.TryFileLogAsync(30, FileLogOptions.FullDescriptions, newArchive.DepotPath, cancellationToken);
					if (response.Succeeded)
					{
						// Build a new list of zipped binaries
						foreach (FileLogRecord file in response.Data)
						{
							foreach (RevisionRecord revision in file.Revisions)
							{
								if (revision.Action != FileAction.Purge)
								{
									string[] tokens = revision.Description.Split(' ');
									if (tokens[0].StartsWith("[CL", StringComparison.Ordinal) && tokens[1].EndsWith("]", StringComparison.Ordinal))
									{
										int originalChangeNumber;
										if (Int32.TryParse(tokens[1].Substring(0, tokens[1].Length - 1), out originalChangeNumber) && !newArchive.ChangeNumberToFileRevision.ContainsKey(originalChangeNumber))
										{
											newArchive.ChangeNumberToFileRevision[originalChangeNumber] = $"{newArchive.DepotPath}#{revision.RevisionNumber}";
										}
									}
								}
							}
						}
					}
				}
			}

			return newArchives;
		}
	}
}
