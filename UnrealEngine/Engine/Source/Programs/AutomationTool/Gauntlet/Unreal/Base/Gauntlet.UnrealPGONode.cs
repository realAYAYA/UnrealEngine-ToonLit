// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using System.Diagnostics;
using System.Diagnostics.Eventing.Reader;
using Gauntlet.Utils;

namespace Gauntlet
{
	public class PGOConfig : UnrealTestConfiguration, IAutoParamNotifiable
	{
		/// <summary>
		/// Output directory to write the resulting profile data to.
		/// </summary>
		[AutoParam("")]
		public string ProfileOutputDirectory;

		/// <summary>
		/// Directory to save periodic screenshots to whilst the PGO run is in progress.
		/// </summary>
		[AutoParam("")]
		public string ScreenshotDirectory;

		[AutoParam("")]
		public string PGOAccountSandbox;

		[AutoParam("")]
		public string PgcFilenamePrefix;		

		public virtual void ParametersWereApplied(string[] Params)
		{
			if (String.IsNullOrEmpty(ProfileOutputDirectory))
			{
				throw new AutomationException("ProfileOutputDirectory option must be specified for profiling data");
			}
		}
	}

	public class PGONode<TConfigClass> : UnrealTestNode<TConfigClass> where TConfigClass : PGOConfig, new()
	{
		protected string LocalOutputDirectory;
		private IPGOPlatform PGOPlatform;
		bool ProcessPGODataFailed = false;

		public PGONode(UnrealTestContext InContext) : base(InContext)
		{
			
		}

		public override TConfigClass GetConfiguration()
		{
			if (CachedConfig != null)
			{
				return CachedConfig as TConfigClass;
			}

			var Config = CachedConfig = base.GetConfiguration();

			// Set max duration to 1 hour
			Config.MaxDuration = 60 * 60;

			// Get output filenames
			LocalOutputDirectory = Path.GetFullPath(Config.ProfileOutputDirectory);

			// Create the local profiling data directory if needed
			if (!Directory.Exists(LocalOutputDirectory))
			{
				Directory.CreateDirectory(LocalOutputDirectory);
			}

			ScreenshotDirectory = Config.ScreenshotDirectory;

			if (!String.IsNullOrEmpty(ScreenshotDirectory))
			{
				if (Directory.Exists(ScreenshotDirectory))
				{
					Directory.Delete(ScreenshotDirectory, true);
				}

				Directory.CreateDirectory(ScreenshotDirectory);
			}

			PGOPlatform = PGOPlatformManager.GetPGOPlatform(Context.GetRoleContext(UnrealTargetRole.Client).Platform);
			PGOPlatform.ApplyConfiguration(Config);

			return Config as TConfigClass;

		}

		private DateTime ScreenshotStartTime = DateTime.UtcNow;
		private DateTime ScreenshotTime = DateTime.MinValue;
		private TimeSpan ScreenshotInterval = TimeSpan.FromSeconds(30);
		private const float ScreenshotScale = 1.0f / 3.0f;
		private const int ScreenshotQuality = 30;
		protected string ScreenshotDirectory;
		protected bool ResultsGathered = false;

		protected override TestResult GetUnrealTestResult()
		{
			if (ProcessPGODataFailed)
			{
				return TestResult.Failed;
			}

			return base.GetUnrealTestResult();
		}

		public override void TickTest()
		{
			base.TickTest();

			if (GetTestStatus() == TestStatus.Complete || ResultsGathered)
			{
				if (!ResultsGathered)
				{
					ResultsGathered = true;

					try
					{
						// Gather results and merge PGO data
						Log.Info("Gathering profiling results to {0}", TestInstance.ClientApps[0].ArtifactPath);
						PGOPlatform.GatherResults(TestInstance.ClientApps[0].ArtifactPath);
					}
					catch (Exception Ex)
					{
						ProcessPGODataFailed = true;
						Log.Error("Error getting PGO results: {0}", Ex);
					}

				}

				return;
			}

			// Handle device screenshot update
			TimeSpan Delta = DateTime.Now - ScreenshotTime;
			ITargetDevice Device = TestInstance.ClientApps[0].Device;

			string ImageFilename;
			if (!String.IsNullOrEmpty(ScreenshotDirectory) && Delta >= ScreenshotInterval && Device != null && PGOPlatform.TakeScreenshot(Device, ScreenshotDirectory, out ImageFilename))
			{
				ScreenshotTime = DateTime.Now;

				if (!File.Exists(Path.Combine(ScreenshotDirectory, ImageFilename)))
				{
					Log.Info("PGOPlatform.TakeScreenshot returned true, but output image {0} does not exist! skipping", ImageFilename);
				}
				else if(new FileInfo(Path.Combine(ScreenshotDirectory, ImageFilename)).Length <= 0)
				{
					Log.Info("PGOPlatform.TakeScreenshot returned true, but output image {0} is size 0! skipping", ImageFilename);
				}
				else
				{
					try
					{
						TimeSpan ImageTimestamp = DateTime.UtcNow - ScreenshotStartTime;
						string ImageOutputPath = Path.Combine(ScreenshotDirectory, ImageTimestamp.ToString().Replace(':', '-') + ".jpg");
						ImageUtils.ResaveImageAsJpgWithScaleAndQuality(Path.Combine(ScreenshotDirectory, ImageFilename), ImageOutputPath, ScreenshotScale, ScreenshotQuality);

						// Delete the temporary image file
						try { File.Delete(Path.Combine(ScreenshotDirectory, ImageFilename)); }
						catch (Exception e)
						{
							Log.Warning("Got Exception Deleting temp iamge: {0}", e.ToString());
						}
					}
					catch (Exception e)
					{
						Log.Info("Got Exception Renaming PGO image {0}: {1}", ImageFilename, e.ToString());

						TimeSpan ImageTimestamp = DateTime.UtcNow - ScreenshotStartTime;
						string CopyFileName = Path.Combine(ScreenshotDirectory, ImageTimestamp.ToString().Replace(':', '-') + ".bmp");
						Log.Info("Copying unconverted image {0} to {1}", ImageFilename, CopyFileName);
						try
						{
							File.Copy(Path.Combine(ScreenshotDirectory, ImageFilename), CopyFileName);
						}
						catch (Exception e2)
						{
							Log.Warning("Got Exception copying un-converted screenshot image: {0}", e2.ToString());
						}
					}
				}
			}

		}
	}

	internal interface IPGOPlatform
	{
		void ApplyConfiguration(PGOConfig Config);

		void GatherResults(string ArtifactPath);

		bool TakeScreenshot(ITargetDevice Device, string ScreenshotDirectory, out string ImageFilename);

		UnrealTargetPlatform GetPlatform();
	}

	/// <summary>
	/// PGO platform manager
	/// </summary>
	internal abstract class PGOPlatformManager
	{
		public static IPGOPlatform GetPGOPlatform(UnrealTargetPlatform Platform)
		{
			Type PGOPlatformType;
			if (!PGOPlatforms.TryGetValue(Platform, out PGOPlatformType))
			{
				throw new AutomationException("Invalid PGO Platform: {0}", Platform);
			}

			return Activator.CreateInstance(PGOPlatformType) as IPGOPlatform;
		}

		protected static void RegisterPGOPlatform(UnrealTargetPlatform Platform, Type PGOPlatformType)
		{
			PGOPlatforms[Platform] = PGOPlatformType;
		}

		static Dictionary<UnrealTargetPlatform, Type> PGOPlatforms = new Dictionary<UnrealTargetPlatform, Type>();

		static PGOPlatformManager()
		{
			IEnumerable<IPGOPlatform> DiscoveredPGOPlatforms = Utils.InterfaceHelpers.FindImplementations<IPGOPlatform>();

			foreach (IPGOPlatform PGOPlatform in DiscoveredPGOPlatforms)
			{
				PGOPlatforms[PGOPlatform.GetPlatform()] = PGOPlatform.GetType();
			}

		}

	}

}
