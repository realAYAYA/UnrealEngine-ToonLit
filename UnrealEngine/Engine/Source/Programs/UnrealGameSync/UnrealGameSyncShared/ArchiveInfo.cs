// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Horde.Artifacts;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;

namespace UnrealGameSync
{
	public interface IArchive
	{
		public string Key { get; }

		Task<bool> DownloadAsync(IPerforceConnection perforce, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken);
	}

	public interface IArchiveChannel
	{
		public const string EditorArchiveType = "Editor";

		// Name to display in the UI
		string Name { get; }

		// Type key; only one item of each type may be enabled
		string Type { get; }

		// Tooltip when hovering over item in UI
		string ToolTip { get; }

		bool HasAny();
		IArchive? TryGetArchiveForChangeNumber(int changeNumber, int maxChangeNumber);
	}

	public abstract class BaseArchiveChannel : IArchiveChannel
	{
		public string Name { get; }
		public string Type { get; }

		public virtual string ToolTip { get; } = "";

		// TODO: executable/configuration?
		public SortedList<int, IArchive> ChangeNumberToArchive { get; } = new SortedList<int, IArchive>();

		protected BaseArchiveChannel(string name, string type)
		{
			Name = name;
			Type = type;
		}

		public bool HasAny()
		{
			return ChangeNumberToArchive.Count > 0;
		}

		public IArchive? TryGetArchiveForChangeNumber(int changeNumber, int maxChangeNumber)
		{
			int idx = ChangeNumberToArchive.Keys.AsReadOnlyList().BinarySearch(changeNumber);
			if (idx >= 0)
			{
				return ChangeNumberToArchive.Values[idx];
			}

			int nextIdx = ~idx;
			if (nextIdx < ChangeNumberToArchive.Count && ChangeNumberToArchive.Keys[nextIdx] <= maxChangeNumber)
			{
				return ChangeNumberToArchive.Values[nextIdx];
			}

			return null;
		}

		public override string ToString()
		{
			return Name;
		}
	}

	public class PerforceArchiveChannel : BaseArchiveChannel
	{
		public string DepotPath { get; set; }
		public string? Target { get; }

		public override string ToolTip
			=> HasAny() ? "" : $"No valid archives found at {DepotPath}";

		public PerforceArchiveChannel(string name, string type, string depotPath, string? target)
			: base(name, type)
		{
			Target = target;
			DepotPath = depotPath;
			Target = target;
		}

		public override bool Equals(object? other)
		{
			PerforceArchiveChannel? otherArchive = other as PerforceArchiveChannel;
			return otherArchive != null && Name == otherArchive.Name && Type == otherArchive.Type && DepotPath == otherArchive.DepotPath && Target == otherArchive.Target
				&& Enumerable.SequenceEqual(ChangeNumberToArchive.Select(x => (x.Key, x.Value)), otherArchive.ChangeNumberToArchive.Select(x => (x.Key, x.Value)));
		}

		public override int GetHashCode()
		{
			throw new NotSupportedException();
		}

		class PerforceArchive : IArchive
		{
			public string Key { get; }

			public PerforceArchive(string key)
				=> Key = key;

			public async Task<bool> DownloadAsync(IPerforceConnection perforce, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken)
			{
				DirectoryReference configDir = UserSettings.GetConfigDir(localRootPath);
				UserSettings.CreateConfigDir(configDir);

				FileReference tempZipFileName = FileReference.Combine(configDir, "archive.zip");
				try
				{
					PrintRecord record = await perforce.PrintAsync(tempZipFileName.FullName, Key, cancellationToken);

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
		}

		public static bool TryParseConfigEntryAsync(string text, [NotNullWhen(true)] out PerforceArchiveChannel? channel)
		{
			ConfigObject obj = new ConfigObject(text);

			string? name = obj.GetValue("Name", null);
			if (name == null)
			{
				channel = null;
				return false;
			}

			// Where to find archives, you'll have either Perforce (DepotPath) or Horde (ArchiveType)
			string? depotPath = obj.GetValue("DepotPath", null);
			if (depotPath == null)
			{
				channel = null;
				return false;
			}

			string? target = obj.GetValue("Target", null);

			string type = obj.GetValue("Type", null) ?? name;

			// Build a new list of zipped binaries
			channel = new PerforceArchiveChannel(name, type, depotPath, target);
			return true;
		}

		public async Task FindArtifactsAsync(IPerforceConnection perforce, CancellationToken cancellationToken)
		{
			PerforceResponseList<FileLogRecord> response = await perforce.TryFileLogAsync(128, FileLogOptions.FullDescriptions, DepotPath, cancellationToken);
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
								if (Int32.TryParse(tokens[1].Substring(0, tokens[1].Length - 1), out originalChangeNumber) && !ChangeNumberToArchive.ContainsKey(originalChangeNumber))
								{
									PerforceArchive archive = new PerforceArchive($"{DepotPath}#{revision.RevisionNumber}");
									ChangeNumberToArchive[originalChangeNumber] = archive;
								}
							}
						}
					}
				}
			}
		}

		public static async Task<List<PerforceArchiveChannel>> GetChannelsAsync(IPerforceConnection perforce, ConfigFile latestProjectConfigFile, string projectIdentifier, CancellationToken cancellationToken)
		{
			List<PerforceArchiveChannel> channels = new List<PerforceArchiveChannel>();

			// Find all the zipped binaries under this stream
			ConfigSection? projectConfigSection = latestProjectConfigFile.FindSection(projectIdentifier);
			if (projectConfigSection != null)
			{
				// Legacy
				string? legacyEditorArchivePath = projectConfigSection.GetValue("ZippedBinariesPath", null);
				if (legacyEditorArchivePath != null)
				{
					// Only Perforce uses the legacy method
					PerforceArchiveChannel legacyChannel = new PerforceArchiveChannel("Editor", "Editor", legacyEditorArchivePath, null);
					await legacyChannel.FindArtifactsAsync(perforce, cancellationToken);
					channels.Add(legacyChannel);
				}

				// New style
				foreach (string archiveValue in projectConfigSection.GetValues("Archives", Array.Empty<string>()))
				{
					PerforceArchiveChannel? channel;
					if (PerforceArchiveChannel.TryParseConfigEntryAsync(archiveValue, out channel))
					{
						await channel.FindArtifactsAsync(perforce, cancellationToken);
						channels.Add(channel!);
					}
				}
			}

			return channels;
		}
	}

	public class HordeArchiveChannel : BaseArchiveChannel
	{
		public HordeArchiveChannel(string name, string type)
			: base(name, type)
		{
		}

		public override bool Equals(object? other)
		{
			HordeArchiveChannel? otherArchive = other as HordeArchiveChannel;
			return otherArchive != null && Name == otherArchive.Name && Type == otherArchive.Type
				&& Enumerable.SequenceEqual(ChangeNumberToArchive.Select(x => (x.Key, x.Value)), otherArchive.ChangeNumberToArchive.Select(x => (x.Key, x.Value)));
		}

		public override int GetHashCode()
		{
			throw new NotSupportedException();
		}

		class HordeArchive : IArchive
		{
			readonly IHordeClient _hordeClient;
			readonly ArtifactId _artifactId;

			public string Key { get; }

			public HordeArchive(IHordeClient hordeClient, ArtifactId artifactId)
			{
				_hordeClient = hordeClient;
				_artifactId = artifactId;
				Key = artifactId.ToString();
			}

			public async Task<bool> DownloadAsync(IPerforceConnection perforce, DirectoryReference localRootPath, FileReference manifestFileName, ILogger logger, ProgressValue progress, CancellationToken cancellationToken)
			{
				DirectoryReference configDir = UserSettings.GetConfigDir(localRootPath);
				UserSettings.CreateConfigDir(configDir);

				FileReference tempZipFileName = FileReference.Combine(configDir, "archive.zip");
				try
				{
					HordeHttpClient hordeHttpClient = _hordeClient.CreateHttpClient();

					using (FileStream stream = FileReference.Open(tempZipFileName, FileMode.Create, FileAccess.Write, FileShare.None))
					{
						await using Stream sourceStream = await hordeHttpClient.GetArtifactZipAsync(_artifactId, cancellationToken);
						await sourceStream.CopyToAsync(stream, cancellationToken);
					}

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
		}

		public static async Task<List<HordeArchiveChannel>> GetChannelsAsync(IHordeClient hordeClient, string projectIdentifier, CancellationToken cancellationToken)
		{
			List<HordeArchiveChannel> channels = new List<HordeArchiveChannel>();
			try
			{
				HordeHttpClient hordeHttpClient = hordeClient.CreateHttpClient();

				ArtifactType artifactType = new ArtifactType("ugs-pcb");
				string[] artifactKeys = new[] { $"ugs-project={projectIdentifier}" };

				List<GetArtifactResponse> artifactResponses = await hordeHttpClient.FindArtifactsByTypeAsync(artifactType, keys: artifactKeys, cancellationToken: cancellationToken);
				foreach (IGrouping<ArtifactName, GetArtifactResponse> group in artifactResponses.GroupBy(x => x.Name))
				{
					GetArtifactResponse? first = group.FirstOrDefault();
					if (first != null)
					{
						string name = first.Description ?? first.Name.ToString();

						const string ArchiveTypePrefix = "ArchiveType=";
						string? archiveType = first.Metadata?.FirstOrDefault(x => x.StartsWith(ArchiveTypePrefix, StringComparison.OrdinalIgnoreCase));

						string type = IArchiveChannel.EditorArchiveType;
						if (archiveType != null)
						{
							type = archiveType.Substring(ArchiveTypePrefix.Length);
						}

						HordeArchiveChannel channel = new HordeArchiveChannel(name, type);
						foreach (GetArtifactResponse response in group)
						{
							HordeArchive archive = new HordeArchive(hordeClient, response.Id);
							channel.ChangeNumberToArchive[response.Change] = archive;
						}
						channels.Add(channel);
					}
				}
			}
			catch (Exception)
			{
			}
			return channels;
		}
	}

	public static class BaseArchive
	{
		public static async Task<List<BaseArchiveChannel>> EnumerateChannelsAsync(IPerforceConnection perforce, IHordeClient? hordeClient, ConfigFile latestProjectConfigFile, string projectIdentifier, CancellationToken cancellationToken)
		{
			List<BaseArchiveChannel> newArchives = new List<BaseArchiveChannel>();
			newArchives.AddRange(await PerforceArchiveChannel.GetChannelsAsync(perforce, latestProjectConfigFile, projectIdentifier, cancellationToken));
			if (hordeClient != null)
			{
				newArchives.AddRange(await HordeArchiveChannel.GetChannelsAsync(hordeClient, projectIdentifier, cancellationToken));
			}
			return newArchives;
		}
	}
}
