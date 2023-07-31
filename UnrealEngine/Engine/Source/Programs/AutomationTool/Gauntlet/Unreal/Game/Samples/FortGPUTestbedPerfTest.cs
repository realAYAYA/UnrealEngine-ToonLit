// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.IO.Compression;
using Gauntlet;

namespace UnrealGame
{

	/// <summary>
	/// Runs automated tests on a platform
	/// </summary>
	public class FortGPUTestbedPerfTest : DefaultTest
	{
		public FortGPUTestbedPerfTest(Gauntlet.UnrealTestContext InContext)
			: base(InContext)
		{
		}

		public override UnrealTestConfig GetConfiguration()
		{
			// just need a single client
			UnrealTestConfig Config = base.GetConfiguration();

			UnrealTestRole ClientRole = Config.RequireRole(UnrealTargetRole.Client);
			ClientRole.Controllers.Add("FortGPUtestbedPerfTest");
			Config.MaxDuration = 5 * 60;        // 5min should be plenty
			return Config;
		}

		public override void TickTest()
		{
			if (TestInstance.ClientApps.Length > 0)
			{
				IAppInstance App = TestInstance.ClientApps.First();

				UnrealLogParser Log = new UnrealLogParser(App.StdOut);

				// Look for message that test is completed
				if (Log.GetAllMatchingLines("FortGPUTestbedPerfTest Finished").Length > 0)
				{
					MarkTestComplete();
					SetUnrealTestResult(TestResult.Passed);
				}
			}

			base.TickTest();
		}

		public override void SaveArtifacts_DEPRECATED(string OutputPath)
		{
			string UploadFolder = Globals.Params.ParseValue("uploadfolder", "");
			if (UploadFolder.Count() > 0 && Directory.CreateDirectory(UploadFolder).Exists)
			{
				string PlatformString = TestInstance.ClientApps[0].Device.Platform.ToString();
				string ArtifactDir = TestInstance.ClientApps[0].ArtifactPath;
				string ProfilingDir = Path.Combine(ArtifactDir, "Profiling");
				string FPSChartsDir = Path.Combine(ProfilingDir, "FPSChartStats").ToLower();
				string FpsChartsZipPath = Path.Combine(TestInstance.ClientApps[0].ArtifactPath, "FPSCharts.zip").ToLower();
				if (Directory.Exists(FPSChartsDir))
				{
					ZipFile.CreateFromDirectory(FPSChartsDir, FpsChartsZipPath);
					string DestFileName = "FortGPUTestbedPerfTest-" + PlatformString + ".zip";
					string DestZipFile = Path.Combine(UploadFolder, DestFileName);
					File.Copy(FpsChartsZipPath, DestZipFile);
				}
			}
			else
			{
				Log.Info("Not uploading CSV Result UploadFolder: '" + UploadFolder + "'");
			}

		}
	}
}
