// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using EpicGames.Serialization;
using EpicGames.ProjectStore;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Enumeration of different storage options for snapshots.
	/// </summary>
	public enum SnapshotStorageType
	{
		/// <summary>
		/// A reserved non-valid storage type for snapshots.
		/// </summary>
		Invalid,
		/// <summary>
		/// Snapshot stored in cloud repositories such as Unreal Cloud DDC.
		/// </summary>
		Cloud,
		/// <summary>
		/// Snapshot stored in a zenserver.
		/// </summary>
		Zen,
		/// <summary>
		/// Snapshot stored as a file on disk.
		/// </summary>
		File,
	}

	/// <summary>
	/// Parameters for a task that exports an snapshot from ZenServer
	/// </summary>
	public class ZenExportSnapshotTaskParameters
	{
		/// <summary>
		/// The project from which to export the snapshot
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference Project;

		/// <summary>
		/// The target platform(s) to export the snapshot for
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Platform;

		/// <summary>
		/// A file to read with information about the snapshot that should be used as a base when exporting this new snapshot
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference SnapshotBaseDescriptorFile;

		/// <summary>
		/// A file to create with information about the snapshot that was exported
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference SnapshotDescriptorFile;

		/// <summary>
		/// The type of destination to export the snapshot to (cloud, ...)
		/// </summary>
		[TaskParameter]
		public string DestinationStorageType;

		/// <summary>
		/// The identifier to use when exporting to a destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DestinationIdentifier;

		/// <summary>
		/// The host name to use when exporting to a cloud destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DestinationCloudHost;

		/// <summary>
		/// The host name to use when writing a snapshot descriptor for a cloud destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string SnapshotDescriptorCloudHost;

		/// <summary>
		/// The http version to use when exporting to a cloud destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DestinationCloudHttpVersion;

		/// <summary>
		/// The http version to use when writing a snapshot descriptor for a cloud destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string SnapshotDescriptorCloudHttpVersion;

		/// <summary>
		/// The namespace to use when exporting to a cloud destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DestinationCloudNamespace;

		/// <summary>
		/// A custom bucket name to use when exporting to a cloud destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DestinationCloudBucket;

		/// <summary>
		/// The directory to use when exporting to a file destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public DirectoryReference DestinationFileDir;

		/// <summary>
		/// The filename to use when exporting to a file destination
		/// </summary>
		[TaskParameter(Optional = true)]
		public string DestinationFileName;

		/// <summary>
		/// Optional. Where to look for the ue.projectstore
		/// The pattern {Platform} can be used for exporting multiple platforms at once.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string OverridePlatformCookedDir;

		/// <summary>
		/// Optional. Whether to force export of data even if the destination claims to have them.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Force = false;

	}

	/// <summary>
	/// Exports an snapshot from Zen to a specified destination.
	/// </summary>
	[TaskElement("ZenExportSnapshot", typeof(ZenExportSnapshotTaskParameters))]
	public class ZenExportSnapshotTask : BgTaskImpl
	{
		/// <summary>
		/// Metadata about a snapshot
		/// </summary>
		public class SnapshotDescriptor
		{
			/// <summary>
			/// Name of the snapshot
			/// </summary>
			public string Name { get; set; }

			/// <summary>
			/// Storage type used for the snapshot
			/// </summary>
			public SnapshotStorageType Type { get; set; }

			/// <summary>
			/// Target platform for this snapshot
			/// </summary>
			public string TargetPlatform { get; set; }
			
			/// <summary>
			/// For cloud snapshots, the host they are stored on.
			/// </summary>
			public string Host { get; set; }

			/// <summary>
			/// For cloud snapshots, the namespace they are stored in.
			/// </summary>
			public string Namespace { get; set; }

			/// <summary>
			/// For cloud snapshots, the bucket they are stored in.
			/// </summary>
			public string Bucket { get; set; }

			/// <summary>
			/// For cloud snapshots, the key they are stored in.
			/// </summary>
			public string Key { get; set; }
			
			/// <summary>
			/// For file snapshots, the directory it is stored in.
			/// </summary>
			public string Directory { get; set; }

			/// <summary>
			/// For file snapshots, the filename (not including path) that they are stored in.
			/// </summary>
			public string Filename { get; set; }
		}
		/// <summary>
		/// A collection of one or more snapshot descriptors
		/// </summary>
		public class SnapshotDescriptorCollection
		{
			/// <summary>
			/// The list of snapshots contained within this collection.
			/// </summary>
			public List<SnapshotDescriptor> Snapshots { get; set; }
		}
		private class ExportSourceData
		{
			public bool IsLocalHost;
			public string HostName;
			public int HostPort;
			public string ProjectId;
			public string OplogId;
			public string TargetPlatform;
			public SnapshotDescriptor SnapshotBaseDescriptor;
		}

		/// <summary>
		/// Parameters for the task
		/// </summary>
		ZenExportSnapshotTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public ZenExportSnapshotTask(ZenExportSnapshotTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Gets the assumed path to where Zen should exist
		/// </summary>
		/// <returns></returns>
		public static FileReference ZenExeFileReference()
		{
			return ResolveFile(String.Format("Engine/Binaries/{0}/zen{1}", HostPlatform.Current.HostEditorPlatform.ToString(), RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? ".exe" : ""));
		}


		/// <summary>
		/// Ensures that ZenServer is running on this current machine. This is needed before running any oplog commands
		/// This passes the sponsor'd process Id to launch zen.
		/// This ensures that zen does not live longer than the lifetime of a particular a process that needs Zen to be running
		/// </summary>
		/// <param name="ProjectFile"></param>
		public static void ZenLaunch(FileReference ProjectFile)
		{
			// Get the ZenLaunch executable path
			FileReference ZenLaunchExe = ResolveFile(String.Format("Engine/Binaries/{0}/ZenLaunch{1}", HostPlatform.Current.HostEditorPlatform.ToString(), RuntimeInformation.IsOSPlatform(OSPlatform.Windows) ? ".exe" : ""));

			StringBuilder ZenLaunchCommandline = new StringBuilder();
			ZenLaunchCommandline.AppendFormat("{0} -SponsorProcessID={1}", CommandUtils.MakePathSafeToUseWithCommandLine(ProjectFile.FullName), Environment.ProcessId);

			CommandUtils.RunAndLog(CommandUtils.CmdEnv, ZenLaunchExe.FullName, ZenLaunchCommandline.ToString(), Options: CommandUtils.ERunOptions.Default);
		}
		
		static JsonSerializerOptions GetDefaultJsonSerializerOptions()
		{
			JsonSerializerOptions options = new JsonSerializerOptions();
			options.AllowTrailingCommas = true;
			options.ReadCommentHandling = JsonCommentHandling.Skip;
			options.PropertyNameCaseInsensitive = true;
			options.Converters.Add(new JsonStringEnumConverter());
			return options;
		}

#nullable enable
		static bool TryLoadJson<T>(FileReference file, [NotNullWhen(true)] out T? obj) where T : class
		{
			if (!FileReference.Exists(file))
			{
				obj = null;
				return false;
			}

			try
			{
				obj = LoadJson<T>(file);
				return true;
			}
			catch(Exception)
			{
				obj = null;
				return false;
			}
		}

		static T? TryDeserializeJson<T>(byte[] data) where T : class
		{
			try
			{
				return JsonSerializer.Deserialize<T>(data, GetDefaultJsonSerializerOptions())!;
			}
			catch (Exception)
			{
				return null;
			}
		}

		static T LoadJson<T>(FileReference file)
		{
			byte[] data = FileReference.ReadAllBytes(file);
			return JsonSerializer.Deserialize<T>(data, GetDefaultJsonSerializerOptions())!;
		}
		private void WriteExportSource(JsonWriter Writer, SnapshotStorageType DestinationStorageType, ExportSourceData ExportSource, string Name)
		{
			Writer.WriteObjectStart();
			switch (DestinationStorageType)
			{
				case SnapshotStorageType.Cloud:
					string BucketName = Parameters.DestinationCloudBucket;
					string ProjectNameAsBucketName = Parameters.Project.GetFileNameWithoutAnyExtensions().ToLowerInvariant();
					if (string.IsNullOrEmpty(BucketName))
					{
						BucketName = ProjectNameAsBucketName;
					}

					string HostName = Parameters.SnapshotDescriptorCloudHost;
					if (string.IsNullOrEmpty(HostName))
					{
						HostName = Parameters.DestinationCloudHost;
					}

					string HttpVersion = Parameters.SnapshotDescriptorCloudHttpVersion;
					if (string.IsNullOrEmpty(HttpVersion))
					{
						HostName = Parameters.DestinationCloudHttpVersion;
					}

					IoHash DestinationKeyHash = IoHash.Compute(Encoding.UTF8.GetBytes(Name));
					Writer.WriteValue("name", Name);
					Writer.WriteValue("type", "cloud");
					Writer.WriteValue("targetplatform", ExportSource.TargetPlatform);
					Writer.WriteValue("host", HostName);
					if (!string.IsNullOrEmpty(HttpVersion) && !HttpVersion.Equals("None", StringComparison.InvariantCultureIgnoreCase))
					{
						Writer.WriteValue("httpversion", HttpVersion);
					}
					Writer.WriteValue("namespace", Parameters.DestinationCloudNamespace);
					Writer.WriteValue("bucket", BucketName);
					Writer.WriteValue("key", DestinationKeyHash.ToString().ToLowerInvariant());
					break;
				case SnapshotStorageType.File:
					Writer.WriteValue("name", Name);
					Writer.WriteValue("type", "file");
					Writer.WriteValue("targetplatform", ExportSource.TargetPlatform);
					Writer.WriteValue("directory", Parameters.DestinationFileDir.FullName);
					Writer.WriteValue("filename", Parameters.DestinationFileName);
					break;
			}
			Writer.WriteObjectEnd();
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			SnapshotStorageType DestinationStorageType = SnapshotStorageType.Invalid;
			if (!string.IsNullOrEmpty(Parameters.DestinationStorageType))
			{
				DestinationStorageType = (SnapshotStorageType)Enum.Parse(typeof(SnapshotStorageType), Parameters.DestinationStorageType);
			}

			FileReference ProjectFile = Parameters.Project;
			if(!FileReference.Exists(ProjectFile))
			{
				throw new AutomationException("Missing project file - {0}", ProjectFile.FullName);
			}

			ZenLaunch(ProjectFile);

			List<ExportSourceData> ExportSources = new List<ExportSourceData>();
			foreach (string Platform in Parameters.Platform.Split('+'))
			{
				DirectoryReference PlatformCookedDirectory;
				if (string.IsNullOrEmpty(Parameters.OverridePlatformCookedDir))
				{
					PlatformCookedDirectory = DirectoryReference.Combine(ProjectFile.Directory, "Saved", "Cooked", Platform);
				}
				else
				{
					PlatformCookedDirectory = new DirectoryReference(Parameters.OverridePlatformCookedDir.Replace("{Platform}", Platform, StringComparison.InvariantCultureIgnoreCase));
				}
				if (!DirectoryReference.Exists(PlatformCookedDirectory))
				{
					throw new AutomationException("Cook output directory not found ({0})", PlatformCookedDirectory.FullName);
				}

				FileReference ProjectStoreFile = FileReference.Combine(PlatformCookedDirectory, "ue.projectstore");
				ProjectStoreData? ParsedProjectStore = null;
				if (TryLoadJson(ProjectStoreFile, out ParsedProjectStore) && (ParsedProjectStore != null) && (ParsedProjectStore.ZenServer != null))
				{
					ExportSourceData NewExportSource = new ExportSourceData();
					NewExportSource.IsLocalHost = ParsedProjectStore.ZenServer.IsLocalHost;
					NewExportSource.HostName = ParsedProjectStore.ZenServer.HostName;
					NewExportSource.HostPort = ParsedProjectStore.ZenServer.HostPort;
					NewExportSource.ProjectId = ParsedProjectStore.ZenServer.ProjectId;
					NewExportSource.OplogId = ParsedProjectStore.ZenServer.OplogId;
					NewExportSource.TargetPlatform = Platform;
					NewExportSource.SnapshotBaseDescriptor = null;

					if (Parameters.SnapshotBaseDescriptorFile != null)
					{
						FileReference PlatformSnapshotBase = new FileReference(Parameters.SnapshotBaseDescriptorFile.FullName.Replace("{Platform}", Platform, StringComparison.InvariantCultureIgnoreCase));

						SnapshotDescriptorCollection? ParsedDescriptorCollection = null;
						if (TryLoadJson(PlatformSnapshotBase, out ParsedDescriptorCollection) && (ParsedDescriptorCollection != null) && (ParsedDescriptorCollection.Snapshots != null))
						{
							foreach (SnapshotDescriptor ParsedDescriptor in ParsedDescriptorCollection.Snapshots)
							{
								if (ParsedDescriptor.TargetPlatform == Platform)
								{
									NewExportSource.SnapshotBaseDescriptor = ParsedDescriptor;
									break;
								}
							}
						}
					}

					ExportSources.Add(NewExportSource);
				}
			}
			int ExportIndex = 0;
			string[] ExportNames = new string[ExportSources.Count];

			// Get the Zen executable path
			FileReference ZenExe = ZenExeFileReference();

			// Format the command line
			StringBuilder OplogExportCommandline = new StringBuilder();
			OplogExportCommandline.Append("oplog-export");
			if (Parameters.Force)
			{
				OplogExportCommandline.Append(" --force");
			}

			switch (DestinationStorageType)
			{
				case SnapshotStorageType.Cloud:
					if (string.IsNullOrEmpty(Parameters.DestinationCloudHost))
					{
						throw new AutomationException("Missing destination cloud host");
					}
					if (string.IsNullOrEmpty(Parameters.DestinationCloudNamespace))
					{
						throw new AutomationException("Missing destination cloud namespace");
					}
					if (string.IsNullOrEmpty(Parameters.DestinationIdentifier))
					{
						throw new AutomationException("Missing destination identifier when exporting to cloud");
					}

					string BucketName = Parameters.DestinationCloudBucket;
					string ProjectNameAsBucketName = ProjectFile.GetFileNameWithoutAnyExtensions().ToLowerInvariant();
					if (string.IsNullOrEmpty(BucketName))
					{
						BucketName = ProjectNameAsBucketName;
					}

					OplogExportCommandline.AppendFormat(" --cloud {0} --namespace {1} --bucket {2}", Parameters.DestinationCloudHost, Parameters.DestinationCloudNamespace, BucketName);

					if (!string.IsNullOrEmpty(Parameters.DestinationCloudHttpVersion))
					{
						if (Parameters.DestinationCloudHttpVersion.Equals("http2-only", StringComparison.InvariantCultureIgnoreCase))
						{
							OplogExportCommandline.Append(" --assume-http2");
						}
						else
						{
							throw new AutomationException("Unexpected destination cloud http version");
						}
					}

					ExportIndex = 0;
					foreach (ExportSourceData ExportSource in ExportSources)
					{
						string HostUrlArg = string.Format("--hosturl http://{0}:{1}", ExportSource.IsLocalHost ? "localhost" : ExportSource.HostName, ExportSource.HostPort);
						
						string BaseKeyArg = string.Empty;
						if ((ExportSource.SnapshotBaseDescriptor != null) && !string.IsNullOrEmpty(ExportSource.SnapshotBaseDescriptor.Key))
						{
							if (ExportSource.SnapshotBaseDescriptor.Type == SnapshotStorageType.Cloud)
							{
								BaseKeyArg = " --basekey " + ExportSource.SnapshotBaseDescriptor.Key;
							}
							else
							{
								Logger.LogWarning("Base snapshot descriptor was for a snapshot storage type {0}, but we're producing a snapshot of type cloud.  Skipping use of base snapshot.", ExportSource.SnapshotBaseDescriptor.Type);
							}
						}

						StringBuilder ExportSingleSourceCommandline = new StringBuilder(OplogExportCommandline.Length);
						ExportSingleSourceCommandline.Append(OplogExportCommandline);

						StringBuilder DestinationKeyBuilder = new StringBuilder();
						DestinationKeyBuilder.AppendFormat("{0}.{1}.{2}", ProjectNameAsBucketName, Parameters.DestinationIdentifier, ExportSource.OplogId);
						ExportNames[ExportIndex] = DestinationKeyBuilder.ToString().ToLowerInvariant();
						IoHash DestinationKeyHash = IoHash.Compute(Encoding.UTF8.GetBytes(ExportNames[ExportIndex]));

						ProcessResult.SpewFilterCallbackType SilentOutputFilter = new ProcessResult.SpewFilterCallbackType(Line =>
							{
								return null;
							});
						ExportSingleSourceCommandline.AppendFormat(" {0} --embedloosefiles --key {1} {2} {3} {4}", HostUrlArg, DestinationKeyHash.ToString().ToLowerInvariant(), BaseKeyArg, ExportSource.ProjectId, ExportSource.OplogId);
						CommandUtils.RunAndLog(CommandUtils.CmdEnv, ZenExe.FullName, ExportSingleSourceCommandline.ToString(), MaxSuccessCode: int.MaxValue, Options: CommandUtils.ERunOptions.Default, SpewFilterCallback: SilentOutputFilter);

						ExportIndex = ExportIndex + 1;
					}

					break;
				case SnapshotStorageType.File:
					string DefaultProjectId = ProjectUtils.GetProjectPathId(ProjectFile);
					ExportIndex = 0;
					foreach (ExportSourceData ExportSource in ExportSources)
					{
						StringBuilder ExportNameBuilder = new StringBuilder();
						ExportNameBuilder.AppendFormat("{0}.{1}.{2}", ProjectFile.GetFileNameWithoutAnyExtensions().ToLowerInvariant(), Parameters.DestinationIdentifier, ExportSource.OplogId);
						ExportNames[ExportIndex] = ExportNameBuilder.ToString().ToLowerInvariant();

						StringBuilder ExportSingleSourceCommandline = new StringBuilder(OplogExportCommandline.Length);
						ExportSingleSourceCommandline.Append(OplogExportCommandline);

						string DestinationFileName = ExportSource.OplogId;
						if (!string.IsNullOrEmpty(Parameters.DestinationFileName))
						{
							DestinationFileName = Parameters.DestinationFileName.Replace("{Platform}", ExportSource.TargetPlatform, StringComparison.InvariantCultureIgnoreCase);
						}

						string ProjectId = string.IsNullOrEmpty(ExportSource.ProjectId) ? DefaultProjectId : ExportSource.ProjectId;
						string BaseNameArg = string.Empty;
						DirectoryReference PlatformDestinationFileDir = new DirectoryReference(Parameters.DestinationFileDir.FullName.Replace("{Platform}", ExportSource.TargetPlatform, StringComparison.InvariantCultureIgnoreCase));
						if ((ExportSource.SnapshotBaseDescriptor != null) && !string.IsNullOrEmpty(ExportSource.SnapshotBaseDescriptor.Directory) && !string.IsNullOrEmpty(ExportSource.SnapshotBaseDescriptor.Filename))
						{
							if (ExportSource.SnapshotBaseDescriptor.Type == SnapshotStorageType.File)
							{
								FileReference BaseSnapshotFile = new FileReference(Path.Combine(ExportSource.SnapshotBaseDescriptor.Directory, ExportSource.SnapshotBaseDescriptor.Filename));
								if (FileReference.Exists(BaseSnapshotFile))
								{
									BaseNameArg = " --basename " + CommandUtils.MakePathSafeToUseWithCommandLine(BaseSnapshotFile.FullName);
								}
								else
								{
									Logger.LogWarning("Base snapshot descriptor missing.  Skipping use of base snapshot.");
								}
							}
							else
							{
								Logger.LogWarning("Base snapshot descriptor was for a snapshot storage type {0}, but we're producing a snapshot of type file.  Skipping use of base snapshot.", ExportSource.SnapshotBaseDescriptor.Type);
							}
						}
						ExportSingleSourceCommandline.AppendFormat(" --file {0} --name {1} {2} {3} {4}", CommandUtils.MakePathSafeToUseWithCommandLine(PlatformDestinationFileDir.FullName), DestinationFileName, BaseNameArg, ProjectId, ExportSource.OplogId);

						CommandUtils.RunAndLog(CommandUtils.CmdEnv, ZenExe.FullName, ExportSingleSourceCommandline.ToString(), Options: CommandUtils.ERunOptions.Default);

						ExportIndex = ExportIndex + 1;
					}
					break;
				default:
					throw new AutomationException("Unknown/invalid/unimplemented destination storage type - {0}", Parameters.DestinationStorageType);
			}
			

			if ((Parameters.SnapshotDescriptorFile != null) && ExportSources.Any())
			{
				if (Parameters.SnapshotDescriptorFile.FullName.Contains("{Platform}"))
				{
					// Separate descriptor file per platform
					ExportIndex = 0;
					foreach (ExportSourceData ExportSource in ExportSources)
					{
						FileReference PlatformSnapshotDescriptorFile = new FileReference(Parameters.SnapshotDescriptorFile.FullName.Replace("{Platform}", ExportSource.TargetPlatform, StringComparison.InvariantCultureIgnoreCase));
						DirectoryReference.CreateDirectory(PlatformSnapshotDescriptorFile.Directory);
						using (JsonWriter Writer = new JsonWriter(PlatformSnapshotDescriptorFile))
						{
							Writer.WriteObjectStart();
							Writer.WriteArrayStart("snapshots");
							WriteExportSource(Writer, DestinationStorageType, ExportSource, ExportNames[ExportIndex]);
							Writer.WriteArrayEnd();
							Writer.WriteObjectEnd();
						}
						ExportIndex = ExportIndex + 1;
					}
				}
				else
				{
					// Write out a single snapshot descriptor with info about all snapshots
					DirectoryReference.CreateDirectory(Parameters.SnapshotDescriptorFile.Directory);
					using (JsonWriter Writer = new JsonWriter(Parameters.SnapshotDescriptorFile))
					{
						Writer.WriteObjectStart();
						Writer.WriteArrayStart("snapshots");
								
						ExportIndex = 0;
						foreach (ExportSourceData ExportSource in ExportSources)
						{
							WriteExportSource(Writer, DestinationStorageType, ExportSource, ExportNames[ExportIndex]);
							ExportIndex = ExportIndex + 1;
						}

						Writer.WriteArrayEnd();
						Writer.WriteObjectEnd();
					}
				}
			}


			return Task.CompletedTask;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
