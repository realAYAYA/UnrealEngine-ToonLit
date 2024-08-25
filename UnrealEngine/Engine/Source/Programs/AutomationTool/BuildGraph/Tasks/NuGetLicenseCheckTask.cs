// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System;
using System.CodeDom;
using System.Collections.Generic;
using System.Data;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web;
using System.Xml;
using UnrealBuildBase;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a NuGetLicenseCheck task
	/// </summary>
	public class NuGetLicenseCheckTaskParameters
	{
		/// <summary>
		/// Base directory for running the command
		/// </summary>
		[TaskParameter]
		public string BaseDir;

		/// <summary>
		/// Specifies a list of packages to ignore for version checks, separated by semicolons. Optional version number may be specified with 'name@version' syntax.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string IgnorePackages;

		/// <summary>
		/// Directory containing allowed licenses
		/// </summary>
		[TaskParameter(Optional = true)]
		public DirectoryReference LicenseDir;

		/// <summary>
		/// Path to a csv file to write with list of packages and their licenses
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference CsvFile;

		/// <summary>
		/// Override path to dotnet executable
		/// </summary>
		[TaskParameter(Optional = true)]
		public FileReference DotNetPath;
	}

	/// <summary>
	/// Verifies which licenses are in use by nuget dependencies
	/// </summary>
	[TaskElement("NuGet-LicenseCheck", typeof(NuGetLicenseCheckTaskParameters))]
	public class NuGetLicenseCheckTask : SpawnTaskBase
	{
		class LicenseConfig
		{
			public List<string> Urls { get; set; } = new List<string>();
		}

		/// <summary>
		/// Parameters for this task
		/// </summary>
		NuGetLicenseCheckTaskParameters Parameters;

		/// <summary>
		/// Construct a NuGetLicenseCheckTask task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public NuGetLicenseCheckTask(NuGetLicenseCheckTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		enum PackageState
		{
			None,
			IgnoredViaArgs,
			MissingPackageFolder,
			MissingPackageDescriptor,
			Valid,
		}

		class PackageInfo
		{
			public string Name;
			public string Version;
			public string ProjectUrl;
			public LicenseInfo License;
			public string LicenseSource;
			public PackageState State;
			public FileReference Descriptor;
		}

		class LicenseInfo
		{
			public IoHash Hash;
			public string Text;
			public string NormalizedText;
			public string Extension;
			public bool Approved;
			public FileReference File;
		}

		LicenseInfo FindOrAddLicense(Dictionary<IoHash, LicenseInfo> Licenses, string Text, string Extension)
		{
			string NormalizedText = Text;
			NormalizedText = Regex.Replace(NormalizedText, @"^\s+", "", RegexOptions.Multiline);
			NormalizedText = Regex.Replace(NormalizedText, @"\s+$", "", RegexOptions.Multiline);
			NormalizedText = Regex.Replace(NormalizedText, "^(?:MIT License|The MIT License \\(MIT\\))\n", "", RegexOptions.Multiline);
			NormalizedText = Regex.Replace(NormalizedText, "^Copyright \\(c\\)[^\n]*\\s*(?:All rights reserved\\.?\\s*)?", "", RegexOptions.Multiline);
			NormalizedText = Regex.Replace(NormalizedText, @"\s+", " ");
			NormalizedText = NormalizedText.Trim();

			byte[] Data = Encoding.UTF8.GetBytes(NormalizedText);
			IoHash Hash = IoHash.Compute(Data);

			LicenseInfo LicenseInfo;
			if (!Licenses.TryGetValue(Hash, out LicenseInfo))
			{
				LicenseInfo = new LicenseInfo();
				LicenseInfo.Hash = Hash;
				LicenseInfo.Text = Text;
				LicenseInfo.NormalizedText = NormalizedText;
				LicenseInfo.Extension = Extension;
				Licenses.Add(Hash, LicenseInfo);
			}
			return LicenseInfo;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			FileReference DotNetPath = Parameters.DotNetPath ?? Unreal.DotnetPath;

			IProcessResult NuGetOutput = await ExecuteAsync(DotNetPath.FullName, $"nuget locals global-packages --list", LogOutput: false);
			if (NuGetOutput.ExitCode != 0)
			{
				throw new AutomationException("DotNet terminated with an exit code indicating an error ({0})", NuGetOutput.ExitCode);
			}

			List<DirectoryReference> NuGetPackageDirs = new List<DirectoryReference>();
			foreach (string Line in NuGetOutput.Output.Split('\n'))
			{
				int ColonIdx = Line.IndexOf(':');
				if (ColonIdx != -1)
				{
					DirectoryReference NuGetPackageDir = new DirectoryReference(Line.Substring(ColonIdx + 1).Trim());
					Logger.LogInformation("Using NuGet package directory: {Path}", NuGetPackageDir);
					NuGetPackageDirs.Add(NuGetPackageDir);
				}
			}

			const string UnknownPrefix = "Unknown-";

			IProcessResult PackageListOutput = await ExecuteAsync(DotNetPath.FullName, "list package --include-transitive", WorkingDir: Parameters.BaseDir, LogOutput: false);
			if (PackageListOutput.ExitCode != 0)
			{
				throw new AutomationException("DotNet terminated with an exit code indicating an error ({0})", PackageListOutput.ExitCode);
			}

			Dictionary<string, PackageInfo> Packages = new Dictionary<string, PackageInfo>();
			foreach (string Line in PackageListOutput.Output.Split('\n'))
			{
				Match Match = Regex.Match(Line, @"^\s*>\s*([^\s]+)\s+(?:[^\s]+\s+)?([^\s]+)\s*$");
				if (Match.Success)
				{
					PackageInfo Info = new PackageInfo();
					Info.Name = Match.Groups[1].Value;
					Info.Version = Match.Groups[2].Value;
					Packages.TryAdd($"{Info.Name}@{Info.Version}", Info);
				}
			}

			DirectoryReference PackageRootDir = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile), ".nuget", "packages");
			if (!DirectoryReference.Exists(PackageRootDir))
			{
				throw new AutomationException("Missing NuGet package cache at {0}", PackageRootDir);
			}

			HashSet<string> LicenseUrls = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

			Dictionary<IoHash, LicenseInfo> Licenses = new Dictionary<IoHash, LicenseInfo>();
			if (Parameters.LicenseDir != null)
			{
				Logger.LogInformation("Reading allowed licenses from {LicenseDir}", Parameters.LicenseDir);
				foreach (FileReference File in DirectoryReference.EnumerateFiles(Parameters.LicenseDir))
				{
					if (!File.GetFileName().StartsWith(UnknownPrefix, StringComparison.OrdinalIgnoreCase))
					{
						try
						{
							if (File.HasExtension(".json"))
							{
								byte[] Data = await FileReference.ReadAllBytesAsync(File);
								LicenseConfig Config = JsonSerializer.Deserialize<LicenseConfig>(Data, new JsonSerializerOptions { PropertyNameCaseInsensitive = true, AllowTrailingCommas = true, ReadCommentHandling = JsonCommentHandling.Skip });
								LicenseUrls.UnionWith(Config.Urls);
							}
							else if (File.HasExtension(".txt") || File.HasExtension(".html"))
							{
								string Text = await FileReference.ReadAllTextAsync(File);
								LicenseInfo License = FindOrAddLicense(Licenses, Text, File.GetFileNameWithoutExtension());
								License.File = File;
								License.Approved = true;
							}
						}
						catch (Exception ex)
						{
							throw new AutomationException(ex, $"Error parsing {File}: {ex.Message}");
						}
					}
				}
			}

			HashSet<string> IgnorePackages = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			if (Parameters.IgnorePackages != null)
			{
				IgnorePackages.UnionWith(Parameters.IgnorePackages.Split(';'));
			}

			Dictionary<string, LicenseInfo> LicenseUrlToInfo = new Dictionary<string, LicenseInfo>(StringComparer.OrdinalIgnoreCase);
			foreach (PackageInfo Info in Packages.Values)
			{
				if (IgnorePackages.Contains(Info.Name) || IgnorePackages.Contains($"{Info.Name}@{Info.Version}"))
				{
					Info.State = PackageState.IgnoredViaArgs;
					continue;
				}

				DirectoryReference PackageDir = NuGetPackageDirs.Select(x => DirectoryReference.Combine(x, Info.Name.ToLowerInvariant(), Info.Version.ToLowerInvariant())).FirstOrDefault(x => DirectoryReference.Exists(x));
				if (PackageDir == null)
				{
					Info.State = PackageState.MissingPackageFolder;
					continue;
				}

				Info.Descriptor = FileReference.Combine(PackageDir, $"{Info.Name.ToLowerInvariant()}.nuspec");
				if (!FileReference.Exists(Info.Descriptor))
				{
					Info.State = PackageState.MissingPackageDescriptor;
					continue;
				}

				using (Stream Stream = FileReference.Open(Info.Descriptor, FileMode.Open, FileAccess.Read, FileShare.Read))
				{
					XmlTextReader XmlReader = new XmlTextReader(Stream);
					XmlReader.Namespaces = false;

					XmlDocument XmlDocument = new XmlDocument();
					XmlDocument.Load(XmlReader);

					XmlNode ProjectUrlNode = XmlDocument.SelectSingleNode("/package/metadata/projectUrl");
					Info.ProjectUrl = ProjectUrlNode?.InnerText;

					if (Info.License == null)
					{
						XmlNode LicenseNode = XmlDocument.SelectSingleNode("/package/metadata/license");
						if (LicenseNode?.Attributes["type"]?.InnerText?.Equals("file", StringComparison.Ordinal) ?? false)
						{
							FileReference LicenseFile = FileReference.Combine(PackageDir, LicenseNode.InnerText);
							if (FileReference.Exists(LicenseFile))
							{
								string Text = await FileReference.ReadAllTextAsync(LicenseFile);
								Info.License = FindOrAddLicense(Licenses, Text, LicenseFile.GetExtension());
								Info.LicenseSource = LicenseFile.FullName;
							}
						}
					}

					if (Info.License == null)
					{
						XmlNode LicenseUrlNode = XmlDocument.SelectSingleNode("/package/metadata/licenseUrl");

						string LicenseUrl = LicenseUrlNode?.InnerText;
						if (LicenseUrl != null)
						{
							LicenseUrl = Regex.Replace(LicenseUrl, @"^https://github.com/(.*)/blob/(.*)$", @"https://raw.githubusercontent.com/$1/$2");
							Info.LicenseSource = LicenseUrl;

							if (!LicenseUrlToInfo.TryGetValue(LicenseUrl, out Info.License))
							{
								using (HttpClient Client = new HttpClient())
								{
									using HttpResponseMessage Response = await Client.GetAsync(LicenseUrl);
									if (!Response.IsSuccessStatusCode)
									{
										Logger.LogError("Unable to fetch license from {LicenseUrl}", LicenseUrl);
									}
									else
									{
										string Text = await Response.Content.ReadAsStringAsync();
										string Type = (Response.Content.Headers.ContentType?.MediaType == "text/html") ? ".html" : ".txt";
										Info.License = FindOrAddLicense(Licenses, Text, Type);
										if (!Info.License.Approved)
										{
											Info.License.Approved = LicenseUrls.Contains(LicenseUrl);
										}
										LicenseUrlToInfo.Add(LicenseUrl, Info.License);
									}
								}
							}
						}
					}
				}

				Info.State = PackageState.Valid;
			}

			Logger.LogInformation("Referenced Packages:");
			Logger.LogInformation("");
			foreach (PackageInfo Info in Packages.Values.OrderBy(x => x.Name).ThenBy(x => x.Version))
			{
				switch (Info.State)
				{
					case PackageState.IgnoredViaArgs:
						Logger.LogInformation("  {Name,-60} {Version,-10} Explicitly ignored via task arguments", Info.Name, Info.Version);
						break;
					case PackageState.MissingPackageFolder:
						Logger.LogInformation("  {Name,-60} {Version,-10} NuGet package not found", Info.Name, Info.Version);
						break;
					case PackageState.MissingPackageDescriptor:
						Logger.LogWarning("  {Name,-60} {Version,-10} Missing package descriptor: {NuSpecFile}", Info.Name, Info.Version, Info.Descriptor);
						break;
					case PackageState.Valid:
						if (Info.License == null)
						{
							Logger.LogError("  {Name,-60} {Version,-10} No license metadata found", Info.Name, Info.Version);
						}
						else if (!Info.License.Approved)
						{
							Logger.LogWarning("  {Name,-60} {Version,-10} {Hash}", Info.Name, Info.Version, Info.License.Hash);
						}
						else
						{
							Logger.LogInformation("  {Name,-60} {Version,-10} {Hash}", Info.Name, Info.Version, Info.License.Hash);
						}
						break;
					default:
						Logger.LogError("  {Name,-60} {Version,-10} Unhandled state: {State}", Info.Name, Info.Version, Info.State);
						break;
				}
			}

			Dictionary<LicenseInfo, List<PackageInfo>> MissingLicenses = new Dictionary<LicenseInfo, List<PackageInfo>>();
			foreach (PackageInfo PackageInfo in Packages.Values)
			{
				if (PackageInfo.License != null && !PackageInfo.License.Approved)
				{
					List<PackageInfo> LicensePackages;
					if (!MissingLicenses.TryGetValue(PackageInfo.License, out LicensePackages))
					{
						LicensePackages = new List<PackageInfo>();
						MissingLicenses.Add(PackageInfo.License, LicensePackages);
					}
					LicensePackages.Add(PackageInfo);
				}
			}

			if (MissingLicenses.Count > 0)
			{
				DirectoryReference LicenseDir = Parameters.LicenseDir ?? DirectoryReference.Combine(Unreal.RootDirectory, "Engine", "Saved", "Licenses");
				DirectoryReference.CreateDirectory(LicenseDir);

				Logger.LogInformation("");
				Logger.LogInformation("Missing licenses:");
				foreach ((LicenseInfo MissingLicense, List<PackageInfo> MissingLicensePackages) in MissingLicenses.OrderBy(x => x.Key.Hash))
				{
					FileReference OutputFile = FileReference.Combine(LicenseDir, $"{UnknownPrefix}{MissingLicense.Hash}{MissingLicense.Extension}");
					await FileReference.WriteAllTextAsync(OutputFile, MissingLicense.Text);

					Logger.LogInformation("");
					Logger.LogInformation("  {LicenseFile}", OutputFile);
					foreach (PackageInfo LicensePackage in MissingLicensePackages)
					{
						Logger.LogInformation("  -> {Name} {Version} ({Source})", LicensePackage.Name, LicensePackage.Version, LicensePackage.LicenseSource);
					}
				}
			}

			if (Parameters.CsvFile != null)
			{
				Logger.LogInformation("Writing {File}", Parameters.CsvFile);
				DirectoryReference.CreateDirectory(Parameters.CsvFile.Directory);
				using (StreamWriter writer = new StreamWriter(Parameters.CsvFile.FullName))
				{
					await writer.WriteLineAsync($"Package,Version,Project Url,License Url,License Hash,License File");
					foreach (PackageInfo PackageInfo in Packages.Values)
					{
						string RelativeLicensePath = "";
						if (PackageInfo.License?.File != null)
						{
							RelativeLicensePath = PackageInfo.License.File.MakeRelativeTo(Parameters.CsvFile.Directory);
						}

						string LicenseUrl = "";
						if (PackageInfo.LicenseSource != null && PackageInfo.LicenseSource.StartsWith("http", StringComparison.OrdinalIgnoreCase))
						{
							LicenseUrl = PackageInfo.LicenseSource;
						}

						await writer.WriteLineAsync($"\"{PackageInfo.Name}\",\"{PackageInfo.Version}\",{PackageInfo.ProjectUrl},{LicenseUrl},{PackageInfo.License?.Hash},{RelativeLicensePath}");
					}
				}
			}
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
