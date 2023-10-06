// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.Core;
using Gauntlet;
using ICVFXTest.Switchboard;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using UnrealBuildBase;
using UnrealBuildTool;
using Log = EpicGames.Core.Log;

namespace ICVFXTest
{
	namespace NDisplay
	{
		public class NDisplayInnerObject
		{
			[JsonPropertyName("assetPath")]
			public string AssetPath { get; set; }
		}

		public class NDisplayConfig
		{
			[JsonPropertyName("nDisplay")]
			public NDisplayInnerObject InnerConfig { get; set; }
		}
	}

	namespace Switchboard
	{
		public class NDisplayDeviceSetting
		{
			[JsonPropertyName("ndisplay_cfg_file")]
			public string DisplayConfigFile { get; set; }

			[JsonPropertyName("primary_device_name")]
			public string PrimaryDeviceName { get; set; }
		}

		public class NdisplaySettings
		{
			[JsonPropertyName("settings")]
			public NDisplayDeviceSetting Settings { get; set; }
		}

		public class NDisplayConfig
		{
			[JsonPropertyName("nDisplay")]
			public NdisplaySettings DeviceSettings { get; set; }
		}

		/// <summary>
		/// Represents a config output by switchboard.
		/// </summary>
		public class SwitchboardConfig
		{
			public string ConfigName { get; set; }

			[JsonPropertyName("project_name")]
			public string ProjectName { get; set; }

			[JsonPropertyName("uproject")]
			public string ProjectPath { get; set; }

			[JsonPropertyName("devices")]
			public NDisplayConfig Devices { get; set; }
		}


		public class UserSettings
		{
			[JsonPropertyName("config")]
			public string LastUsedConfig { get; set; }

			[JsonPropertyName("last_browsed_path")]
			public string LastBrowsedPath { get; set; }
		}
	}

	/// <summary>
	/// CI testing
	/// </summary>
	public class PerformanceReport : AutoTest
	{
		private static ILogger Logger => Log.Logger;
		
		public PerformanceReport(UnrealTestContext InContext)
			: base(InContext)
		{
			FindSwitchboardConfigs();
		}

		public override ICVFXTestConfig GetConfiguration()
		{
			ICVFXTestConfig Config = base.GetConfiguration();
			UnrealTestRole ClientRole = Config.RequireRole(UnrealTargetRole.Client);
			ClientRole.CommandLineParams.Add("csvGpuStats");
			ClientRole.CommandLineParams.Add("csvMetadata", $"\"testname={Config.TestName}\"");
			ClientRole.CommandLineParams.AddOrAppendParamValue("execcmds", "t.FPSChart.DoCSVProfile 1");
			ClientRole.CommandLineParams.AddOrAppendParamValue("execcmds", "t.FPSChart.OpenFolderOnDump 0");
			ClientRole.CommandLineParams.Add("ICVFXTest.FPSChart");

			return Config;
		}

		protected override void InitHandledErrors()
        {
			base.InitHandledErrors();
		}

		public override string GetDisplayClusterUAssetPath(in string NDisplayJsonFile)
		{
			var NDisplayJsonFullPath = Path.Combine(Unreal.RootDirectory.FullName, NDisplayJsonFile);
			string Text = File.ReadAllText(NDisplayJsonFullPath);
			NDisplay.NDisplayConfig DisplayConfig = JsonSerializer.Deserialize<NDisplay.NDisplayConfig>(Text);
			return DisplayConfig.InnerConfig.AssetPath;
		}

		void FindSwitchboardConfigs()
		{
			var SwitchboardConfigsFolder = Path.Combine(Unreal.RootDirectory.FullName, "Engine", "Plugins", "VirtualProduction", "Switchboard", "Source", "Switchboard", "configs");
			if (Directory.Exists(SwitchboardConfigsFolder))
			{
				string[] FileEntries = Directory.GetFiles(SwitchboardConfigsFolder);

				List<SwitchboardConfig> ConfigsForProject = new List<SwitchboardConfig>();

				string LastConfigName = "";

				foreach (string ConfigFile in FileEntries)
				{
					string Text = File.ReadAllText(ConfigFile);

					if (ConfigFile.EndsWith("user_settings.json"))
					{
						var UserConfig = JsonSerializer.Deserialize<Switchboard.UserSettings>(Text);
						LastConfigName = UserConfig.LastUsedConfig;
						Logger.LogInformation($"Last config used: {LastConfigName}");
						continue;
					}

					var Config = JsonSerializer.Deserialize<Switchboard.SwitchboardConfig>(Text);
					Config.ConfigName = Path.GetFileName(ConfigFile);

					if (Config.ProjectName == Context.Options.Project)
					{
						ConfigsForProject.Add(Config);
					}
				}
				
				Logger.LogInformation($"Detected {ConfigsForProject.Count} switchboard configs for this project.");

				foreach (SwitchboardConfig Config in ConfigsForProject)
				{
					Logger.LogInformation($"\t{Config.ConfigName}");

					if (!String.IsNullOrEmpty(LastConfigName) && Config.ConfigName == LastConfigName)
					{
						Logger.LogInformation($"Last NDisplay Config used: {Config.Devices.DeviceSettings.Settings.DisplayConfigFile}");
						Logger.LogInformation($"Found DisplayConfigFile {Config.Devices.DeviceSettings.Settings.DisplayConfigFile}");
						OverrideDisplayConfigPath = Config.Devices.DeviceSettings.Settings.DisplayConfigFile;

						Logger.LogInformation($"Using autodetected DisplayConfig Node: {Config.Devices.DeviceSettings.Settings.PrimaryDeviceName}");
						OverrideDisplayClusterNode = Config.Devices.DeviceSettings.Settings.PrimaryDeviceName;
						break;
					}
				}

				if ((string.IsNullOrEmpty(OverrideDisplayConfigPath) || string.IsNullOrEmpty(OverrideDisplayClusterNode)) && ConfigsForProject.Count() != 0)
				{
					// Fallback to first config that we found from switchboard.
					SwitchboardConfig FirstConfig = ConfigsForProject.First();

					if (string.IsNullOrEmpty(OverrideDisplayConfigPath))
					{
						Logger.LogInformation($"Using autodetected DisplayConfigFile {FirstConfig.Devices.DeviceSettings.Settings.DisplayConfigFile}");
						OverrideDisplayConfigPath = FirstConfig.Devices.DeviceSettings.Settings.DisplayConfigFile;
					}

					if (string.IsNullOrEmpty(OverrideDisplayClusterNode))
					{
						Logger.LogInformation($"Using autodetected DisplayConfig Node: {FirstConfig.Devices.DeviceSettings.Settings.PrimaryDeviceName}");
						OverrideDisplayClusterNode = FirstConfig.Devices.DeviceSettings.Settings.PrimaryDeviceName;
					}
				}
			}

			if (string.IsNullOrEmpty(OverrideDisplayClusterNode))
			{
				// Fallback to node_0 if we don't find anything.
				OverrideDisplayClusterNode = "node_0";
			}
		}

		public override string GetDisplayConfigPath()
		{
			return OverrideDisplayConfigPath;
		}

		public override string GetDisplayClusterNode()
		{
			return OverrideDisplayClusterNode;
		}

		/// <summary>
		/// Produces a detailed csv report using PerfReportTool.
		/// Also, stores perf data in the perf cache, and generates a historic report using the data the cache contains.
		/// </summary>
		private void GeneratePerfReport(UnrealTargetPlatform Platform, string ArtifactPath, string TempDir)
		{
			var ReportCacheDir = GetConfiguration().PerfCacheFolder;

			if (GetTestSuffix().Length != 0)
			{
				ReportCacheDir += "_" + GetTestSuffix(); // We don't want to mix test results
			}

			var ToolPath = FileReference.Combine(Unreal.EngineDirectory, "Binaries", "DotNET", "CsvTools", "PerfreportTool.exe");
			if (!FileReference.Exists(ToolPath))
			{
				Logger.LogError($"Failed to find perf report utility at this path: \"{ToolPath}\".");
				return;
			}
			var ReportConfigDir = Path.Combine(Unreal.RootDirectory.FullName, "Engine", "Plugins", "VirtualProduction", "ICVFXTesting", "Build", "Scripts", "PerfReport");
			var ReportPath = Path.Combine(ArtifactPath, "Reports", "Performance");

		var CsvsPaths = new[]
			{
				Path.Combine(ArtifactPath, "EditorGame", "Profiling", "FPSChartStats"),
				Path.Combine(ArtifactPath, "EditorGame", "Settings", $"{Context.Options.Project}", "Saved", "Profiling", "FPSChartStats"),
				Path.Combine(TempDir, "DeviceCache", Platform.ToString(), TestInstance.ClientApps[0].Device.ToString(), "UserDir")
			};


		var DiscoveredCsvs = new List<string>();
			foreach (var CsvsPath in CsvsPaths)
			{
				if (Directory.Exists(CsvsPath))
				{
					DiscoveredCsvs.AddRange(
						from CsvFile in Directory.GetFiles(CsvsPath, "*.csv", SearchOption.AllDirectories)
						where CsvFile.Contains("csvprofile", StringComparison.InvariantCultureIgnoreCase)
						select CsvFile);
				}
			}

			if (DiscoveredCsvs.Count == 0)
			{
				Logger.LogError($"Test completed successfully but no csv profiling results were found. Searched paths were:\r\n  {string.Join("\r\n  ", CsvsPaths.Select(s => $"\"{s}\""))}");
				return;
			}

			// Find the newest csv file and get its directory
			// (PerfReportTool will only output cached data in -csvdir mode)
			var NewestFile =
				(from CsvFile in DiscoveredCsvs
				 let Timestamp = File.GetCreationTimeUtc(CsvFile)
				 orderby Timestamp descending
				 select CsvFile).First();
			var NewestDir = Path.GetDirectoryName(NewestFile);

			Logger.LogInformation($"Using perf report cache directory \"{ReportCacheDir}\".");
			Logger.LogInformation($"Using perf report output directory \"{ReportPath}\".");
			Logger.LogInformation($"Using csv results directory \"{NewestDir}\". Generating historic perf report data...");

			// Make sure the cache and output directories exist
			if (!Directory.Exists(ReportCacheDir))
			{
				try { Directory.CreateDirectory(ReportCacheDir); }
				catch (Exception Ex)
				{
					Logger.LogError($"Failed to create perf report cache directory \"{ReportCacheDir}\". {Ex}");
					return;
				}
			}
			if (!Directory.Exists(ReportPath))
			{
				try { Directory.CreateDirectory(ReportPath); }
				catch (Exception Ex)
				{
					Logger.LogError($"Failed to create perf report output directory \"{ReportPath}\". {Ex}");
					return;
				}
			}

			// Win64 is actually called "Windows" in csv profiles
			var PlatformNameFilter = Platform == UnrealTargetPlatform.Win64 ? "Windows" : $"{Platform}";

			// Produce the detailed report, and update the perf cache
			CommandUtils.RunAndLog(ToolPath.FullName, $"-csvdir \"{NewestDir}\" -o \"{ReportPath}\" -reportxmlbasedir \"{ReportConfigDir}\" -summaryTableCache \"{ReportCacheDir}\" -searchpattern csvprofile* -metadatafilter platform=\"{PlatformNameFilter}\"", out int ErrorCode);
			if (ErrorCode != 0)
			{
				Logger.LogError($"PerfReportTool returned error code \"{ErrorCode}\" while generating detailed report.");
			}

			// Now generate the all-time historic summary report
			HistoricReport("HistoricReport_AllTime", new[]
			{
				$"platform={PlatformNameFilter}"
			});

			// 14 days historic report
			HistoricReport($"HistoricReport_14Days", new[]
			{
				$"platform={PlatformNameFilter}",
				$"starttimestamp>={DateTimeOffset.Now.ToUnixTimeSeconds() - (14 * 60L * 60L * 24L)}"
			});

			// 7 days historic report
			HistoricReport($"HistoricReport_7Days", new[]
			{
				$"platform={PlatformNameFilter}",
				$"starttimestamp>={DateTimeOffset.Now.ToUnixTimeSeconds() - (7 * 60L * 60L * 24L)}"
			});

			void HistoricReport_Alt(string Name, IEnumerable<string> Filter)
			{
				var Args = new[]
				{
					$"-summarytablecachein \"{ReportCacheDir}\"",
					$"-summaryTableFilename \"{Name  + GetTestSuffix()}.html\"",
					$"-reportxmlbasedir \"{ReportConfigDir}\"",
					$"-o \"{base.GetConfiguration().SummaryReportPath}\"",
					$"-metadatafilter \"{string.Join(" and ", Filter)}\"",
					"-summaryTable autoPerfReportStandard",
					"-condensedSummaryTable autoPerfReportStandard",
					"-emailtable",
					"-recurse"
				};

				var ArgStr = string.Join(" ", Args);

				CommandUtils.RunAndLog(ToolPath.FullName, ArgStr, out ErrorCode);
				if (ErrorCode != 0)
				{
					Logger.LogError($"PerfReportTool returned error code \"{ErrorCode}\" while generating historic report.");
				}
			}

			// 14 days historic report
			HistoricReport_Alt($"HistoricReport_14Days_Summary", new[]
			{
				$"platform={PlatformNameFilter}",
				$"starttimestamp>={DateTimeOffset.Now.ToUnixTimeSeconds() - (14 * 60L * 60L * 24L)}"
			});

			void HistoricReport(string Name, IEnumerable<string> Filter)
			{
				var Args = new[]
				{
					$"-summarytablecachein \"{ReportCacheDir}\"",
					$"-summaryTableFilename \"{Name  + GetTestSuffix()}.html\"",
					$"-reportxmlbasedir \"{ReportConfigDir}\"",
					$"-o \"{ReportPath}\"",
					$"-metadatafilter \"{string.Join(" and ", Filter)}\"",
					"-summaryTable autoPerfReportStandard",
					"-condensedSummaryTable autoPerfReportStandard",
					"-emailtable",
					"-recurse"
				};

				var ArgStr = string.Join(" ", Args);

				CommandUtils.RunAndLog(ToolPath.FullName, ArgStr, out ErrorCode);
				if (ErrorCode != 0)
				{
					Logger.LogError($"PerfReportTool returned error code \"{ErrorCode}\" while generating historic report.");
				}
				else if (!CommandUtils.IsBuildMachine)
				{
					/*
					if (Directory.Exists(ReportPath))
					{
						ProcessStartInfo startInfo = new ProcessStartInfo
						{
							Arguments = ReportPath,
							FileName = "explorer.exe"
						};

						Process.Start(startInfo);

						if (File.Exists(ReportPath + "/index.html"))
						{
							ProcessStartInfo chromeInfo = new ProcessStartInfo
							{
								Arguments = ReportPath + "/index.html",
								FileName = "chrome.exe"
							};

							Process.Start(chromeInfo);
						}
					}*/
				}
			}
		}

		public override ITestReport CreateReport(TestResult Result, UnrealTestContext Context, UnrealBuildSource Build, IEnumerable<UnrealRoleResult> Artifacts, string ArtifactPath)
		{
			if (Result == TestResult.Passed)
			{
				Logger.LogInformation($"Generating performance reports using PerfReportTool.");
				GeneratePerfReport(Context.GetRoleContext(UnrealTargetRole.Client).Platform, ArtifactPath, Context.Options.TempDir);
			}
			else
			{
				Logger.LogWarning($"Skipping performance report generation because the perf report test failed.");
			}

			return base.CreateReport(Result, Context, Build, Artifacts, ArtifactPath);
		}

		private string OverrideDisplayConfigPath;
		private string OverrideDisplayClusterNode;
	}
  
	//
	// Horrible hack to repeat the perf tests 3 times...
	// There is no way to pass "-repeat=N" to Gauntlet via the standard engine build scripts, nor is
	// it possible to override the number of iterations per-test via the GetConfiguration() function.
	//
	// In theory we can pass the "ICVFXTest.PerformanceReport" test name to Gauntlet 3 times via Horde scripts,
	// but the standard build scripts will attempt to define 3 nodes all with the same name, which won't work.
	//
	// These three classes allow us to run 3 copies of the PerformanceReport test, but ensures they all have 
	// different names to fit into the build script / Gauntlet structure.
	//

	public class PerformanceReport_1 : PerformanceReport
	{
		public PerformanceReport_1(UnrealTestContext InContext) : base(InContext) { }
	}

	public class PerformanceReport_2 : PerformanceReport
	{
		public PerformanceReport_2(UnrealTestContext InContext) : base(InContext) { }
	}

	public class PerformanceReport_3 : PerformanceReport
	{
		public PerformanceReport_3(UnrealTestContext InContext) : base(InContext) { }
	}

	public class PerformanceReport_MGPU : PerformanceReport
	{
		public PerformanceReport_MGPU(UnrealTestContext InContext) : base(InContext)
		{
	
		}
		public override int GetMaxGPUCount() 
		{
			return 2;
		}

		public override string GetTestSuffix()
		{
			return "MGPU";
		}
	}
	public class PerformanceReport_NaniteLumen : PerformanceReport
	{
		public PerformanceReport_NaniteLumen(UnrealTestContext InContext) : base(InContext)
		{

		}
		public override bool IsLumenEnabled()
		{
			return true;
		}
		public override bool UseNanite()
		{
			return true;
		}

		public override string GetTestSuffix()
		{
			return "NaniteLumen";
		}
	}

	public class PerformanceReport_Vulkan : PerformanceReport
	{
		public PerformanceReport_Vulkan(UnrealTestContext InContext) : base(InContext)
		{

		}

		public override string GetTestSuffix()
		{
			return "Vulkan";
		}

		public override bool UseVulkan()
		{
			return true;
		}
	}

	public class PerformanceReport_Nanite : PerformanceReport
	{
		public PerformanceReport_Nanite(UnrealTestContext InContext) : base(InContext)
		{

		}

		public override string GetTestSuffix()
		{
			return "Nanite";
		}

		public override bool UseNanite()
		{
			return true;
		}
	}
}
