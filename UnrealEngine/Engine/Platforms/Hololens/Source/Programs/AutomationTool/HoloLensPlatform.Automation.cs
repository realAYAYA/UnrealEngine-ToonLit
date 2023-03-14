// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Threading.Tasks;
using AutomationTool;
using UnrealBuildTool;
using System.Diagnostics;
using EpicGames.Core;
using System.Text.RegularExpressions;

using Windows.Management.Deployment;
using Windows.Foundation;
using System.Threading;
using System.Security.Cryptography.X509Certificates;
using Microsoft.Tools.WindowsDevicePortal;
using UnrealBuildBase;
using AutomationScripts;
using System.Runtime.Versioning;

namespace HoloLens.Automation
{
	class HoloLensLauncherCreatedProcess : IProcessResult
	{
		Process ExternallyLaunchedProcess;
		System.Threading.Thread LogTailThread;
		bool AllowSpew;
		LogEventType SpewVerbosity;

		public HoloLensLauncherCreatedProcess(Process InExternallyLaunchedProcess, string LogFile, bool InAllowSpew, LogEventType InSpewVerbosity)
		{
			ExternallyLaunchedProcess = InExternallyLaunchedProcess;
			AllowSpew = InAllowSpew;
			SpewVerbosity = InSpewVerbosity;

			// Can't redirect stdout, so tail the log file
			if (AllowSpew)
			{
				LogTailThread = new System.Threading.Thread(() => TailLogFile(LogFile));
				LogTailThread.Start();
			}
			else
			{
				LogTailThread = null;
			}
		}

		~HoloLensLauncherCreatedProcess()
		{
			if (ExternallyLaunchedProcess != null)
			{
				ExternallyLaunchedProcess.Dispose();
			}
		}

		public int ExitCode
		{
			get
			{
				// Can't access the exit code of a process we didn't start
				return 0;
			}

			set
			{
				throw new NotImplementedException();
			}
		}

		public bool HasExited
		{
			get
			{
				// Avoid potential access of the exit code.
				return ExternallyLaunchedProcess != null && ExternallyLaunchedProcess.HasExited;
			}
		}

		public string Output
		{
			get
			{
				return string.Empty;
			}
		}

		public Process ProcessObject
		{
			get
			{
				return ExternallyLaunchedProcess;
			}
		}

		public void DisposeProcess()
		{
			ExternallyLaunchedProcess.Dispose();
			ExternallyLaunchedProcess = null;
		}

		public string GetProcessName()
		{
			return ExternallyLaunchedProcess.ProcessName;
		}

		public void OnProcessExited()
		{
			ProcessManager.RemoveProcess(this);
			if (LogTailThread != null)
			{
				LogTailThread.Join();
				LogTailThread = null;
			}
		}

		public void StdErr(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StdOut(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StopProcess(bool KillDescendants = true)
		{
			string ProcessNameForLogging = GetProcessName();
			try
			{
				Process ProcessToKill = ExternallyLaunchedProcess;
				ExternallyLaunchedProcess = null;
				if (!ProcessToKill.CloseMainWindow())
				{
					CommandUtils.LogWarning("{0} did not respond to close request.  Killing...", ProcessNameForLogging);
					ProcessToKill.Kill();
					ProcessToKill.WaitForExit(60000);
				}
				if (!ProcessToKill.HasExited)
				{
					CommandUtils.LogLog("Process {0} failed to exit.", ProcessNameForLogging);
				}
				else
				{
					CommandUtils.LogLog("Process {0} successfully exited.", ProcessNameForLogging);
					OnProcessExited();
				}
				ProcessToKill.Close();
			}
			catch (Exception Ex)
			{
				CommandUtils.LogWarning("Exception while trying to kill process {0}:", ProcessNameForLogging);
				CommandUtils.LogWarning(LogUtils.FormatException(Ex));
			}
		}

		public void WaitForExit()
		{
			ExternallyLaunchedProcess.WaitForExit();
			if (LogTailThread != null)
			{
				LogTailThread.Join();
				LogTailThread = null;
			}
		}

		public FileReference WriteOutputToFile(string FileName)
		{
			throw new NotImplementedException();
		}

		private void TailLogFile(string LogFilePath)
		{
			string LogName = GetProcessName();
			while (!HasExited && !File.Exists(LogFilePath))
			{
				System.Threading.Thread.Sleep(1000);
			}

			if (File.Exists(LogFilePath))
			{
				using (StreamReader LogReader = new StreamReader(new FileStream(LogFilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)))
				{
					while (!HasExited)
					{
						System.Threading.Thread.Sleep(5000);
						while (!LogReader.EndOfStream)
						{
							string LogLine = LogReader.ReadLine();
							Log.WriteLine(SpewVerbosity, "{0} : {1}", LogName, LogLine);
						}
					}
				}
			}
		}
	}

	class HoloLensDevicePortalCreatedProcess : IProcessResult
	{
		object StateLock;
		Microsoft.Tools.WindowsDevicePortal.DevicePortal Portal;
		uint ProcId;
		string PackageName;
		string FriendlyName;

		bool ProcessHasExited;

		public HoloLensDevicePortalCreatedProcess(Microsoft.Tools.WindowsDevicePortal.DevicePortal InPortal, string InPackageName, string InFriendlyName)
		{
			Portal = InPortal;
			PackageName = InPackageName;
			FriendlyName = InFriendlyName;
			StateLock = new object();
			//MonitorThread = new System.Threading.Thread(()=>(MonitorProcess))

			// No ETW on Xbox One, so can't collect trace that way
			if (Portal.Platform != Microsoft.Tools.WindowsDevicePortal.DevicePortal.DevicePortalPlatforms.XboxOne)
			{
				Guid MicrosoftWindowsDiagnoticsLoggingChannelId = new Guid(0x4bd2826e, 0x54a1, 0x4ba9, 0xbf, 0x63, 0x92, 0xb7, 0x3e, 0xa1, 0xac, 0x4a);
				Portal.ToggleEtwProviderAsync(MicrosoftWindowsDiagnoticsLoggingChannelId).Wait();
				Portal.RealtimeEventsMessageReceived += Portal_RealtimeEventsMessageReceived;
				Portal.StartListeningForEtwEventsAsync().Wait();
			}
		}

		public void SetProcId(uint InProcId)
		{
			ProcId = InProcId;
			Portal.RunningProcessesMessageReceived += Portal_RunningProcessesMessageReceived;
			Portal.StartListeningForRunningProcessesAsync().Wait();
		}

		private void Portal_RealtimeEventsMessageReceived(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.WebSocketMessageReceivedEventArgs<Microsoft.Tools.WindowsDevicePortal.DevicePortal.EtwEvents> args)
		{
			foreach (var EtwEvent in args.Message.Events)
			{
				if (EtwEvent.ContainsKey("ProviderName") && EtwEvent.ContainsKey("StringMessage"))
				{
					if (EtwEvent["ProviderName"] == FriendlyName)
					{
						Log.WriteLine(GetLogVerbosityFromEventLevel(EtwEvent.Level), "{0} : {1}", FriendlyName, EtwEvent["StringMessage"].Trim('\"'));
					}
				}
			}
		}

		private LogEventType GetLogVerbosityFromEventLevel(uint EtwLevel)
		{
			switch (EtwLevel)
			{
				case 4:
					return LogEventType.Console;
				case 3:
					return LogEventType.Warning;
				case 2:
					return LogEventType.Error;
				case 1:
					return LogEventType.Fatal;
				default:
					return LogEventType.Verbose;
			}
		}

		private void Portal_RunningProcessesMessageReceived(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.WebSocketMessageReceivedEventArgs<Microsoft.Tools.WindowsDevicePortal.DevicePortal.RunningProcesses> args)
		{
			foreach (var P in args.Message.Processes)
			{
				if (P.ProcessId == ProcId)
				{
					return;
				}
			}
			ProcessHasExited = true;
		}

		public int ExitCode
		{
			get
			{
				return 0;
			}

			set
			{
				throw new NotImplementedException();
			}
		}

		public bool HasExited
		{
			get
			{
				return ProcessHasExited;
			}
		}

		public string Output
		{
			get
			{
				return string.Empty;
			}
		}

		public Process ProcessObject
		{
			get
			{
				return null;
			}
		}

		public void DisposeProcess()
		{
		}

		public string GetProcessName()
		{
			return FriendlyName;
		}

		public void OnProcessExited()
		{
			Portal.StopListeningForRunningProcessesAsync().Wait();
			if (Portal.Platform != Microsoft.Tools.WindowsDevicePortal.DevicePortal.DevicePortalPlatforms.XboxOne)
			{
				Portal.StopListeningForEtwEventsAsync().Wait();
			}
		}

		public void StdErr(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StdOut(object sender, DataReceivedEventArgs e)
		{
			throw new NotImplementedException();
		}

		public void StopProcess(bool KillDescendants = true)
		{
			Portal.TerminateApplicationAsync(PackageName).Wait();
		}

		public void WaitForExit()
		{
			while (!ProcessHasExited)
			{
				System.Threading.Thread.Sleep(1000);
			}
		}

		public FileReference WriteOutputToFile(string FileName)
		{
			throw new NotImplementedException();
		}
	}

	[SupportedOSPlatform("windows10.0.10240.0")]
	public class HoloLensPlatform : Platform
    {
		private WindowsArchitecture[] ActualArchitectures = { };

		private FileReference MakeAppXPath;
		private FileReference PDBCopyPath;
		private FileReference SignToolPath;
		private FileReference MakeCertPath;
		private FileReference Pvk2PfxPath;
		private string Windows10SDKVersion;
		private string Extension = ".appx";


		public HoloLensPlatform()
			: base(UnrealTargetPlatform.HoloLens)
		{

		}

		public override void PreBuildAgenda(UnrealBuild Build, UnrealBuild.BuildAgenda Agenda, ProjectParams Params)
		{
			if(ActualArchitectures.Length == 0)
			{
				throw new AutomationException(ExitCode.Error_Arguments, "No target architecture selected on \'Platforms/HoloLens/OS Info\' page. Please select at last one.");
			}

			foreach (var BuildConfig in Params.ClientConfigsToBuild)
			{
				foreach(var target in Params.ClientCookedTargets)
				{
					var MultiReceiptFileName = TargetReceipt.GetDefaultPath(Params.RawProjectPath.Directory, target, UnrealTargetPlatform.HoloLens, BuildConfig, "Multi");
					if (File.Exists(MultiReceiptFileName.FullName))
					{
						FileReference.Delete(MultiReceiptFileName);
					}

					var x64ReceiptFileName = TargetReceipt.GetDefaultPath(Params.RawProjectPath.Directory, target, UnrealTargetPlatform.HoloLens, BuildConfig, "x64");
					if (File.Exists(x64ReceiptFileName.FullName))
					{
						FileReference.Delete(x64ReceiptFileName);
					}

					var arm64ReceiptFileName = TargetReceipt.GetDefaultPath(Params.RawProjectPath.Directory, target, UnrealTargetPlatform.HoloLens, BuildConfig, "arm64");
					if (File.Exists(arm64ReceiptFileName.FullName))
					{
						FileReference.Delete(arm64ReceiptFileName);
					}

					foreach (var Arch in ActualArchitectures)
					{
						Agenda.Targets.Add(new UnrealBuild.BuildTarget()
						{
							TargetName = target,
							Platform = UnrealTargetPlatform.HoloLens,
							Config = BuildConfig,
							UprojectPath = Params.CodeBasedUprojectPath,
							UBTArgs = " -remoteini=\"" + Params.RawProjectPath.Directory.FullName + "\" -Architecture=" + WindowsExports.GetArchitectureSubpath(Arch),
						});
					}
				}
			}
		}

		public override bool CanBeCompiled() //block auto compilation
		{
			return false;
		}


		public override void PlatformSetupParams(ref ProjectParams ProjParams)
		{
			base.PlatformSetupParams(ref ProjParams);

			if (ProjParams.Deploy && !ProjParams.Package)
			{
				foreach (string DeviceAddress in ProjParams.DeviceNames)
				{
					if (!IsLocalDevice(DeviceAddress))
					{
						LogWarning("Project will be packaged to support deployment to remote HoloLens device {0}.", DeviceAddress);
						ProjParams.Package = true;
						break;
					}
				}
			}

			ConfigHierarchy PlatformEngineConfig = null;
			if (!ProjParams.EngineConfigs.TryGetValue(PlatformType, out PlatformEngineConfig))
			{
				throw new AutomationException(ExitCode.Error_Arguments, "No configuration on \'Platforms/HoloLens/OS Info\' page. Please create one.");
			}

			PlatformEngineConfig.GetString("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "Windows10SDKVersion", out Windows10SDKVersion);

			List<string> ThumbprintsFromConfig = new List<string>();
			if (PlatformEngineConfig.GetArray("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "AcceptThumbprints", out ThumbprintsFromConfig))
			{
				AcceptThumbprints.AddRange(ThumbprintsFromConfig);
			}


			if (ProjParams.Run)
			{
				var ArchList = new List<WindowsArchitecture>();
				foreach (string DeviceAddress in ProjParams.DeviceNames)
				{
					//We have to choose architecture of the device to run
					WindowsArchitecture Arch = WindowsArchitecture.x64;
					if (!IsLocalDevice(DeviceAddress))
					{
						Arch = RemoteDeviceArchitecture(DeviceAddress, ProjParams);
					}

					ArchList.Add(Arch);

					LogInformation(String.Format("Project will be compiled for the architecture {0} of the HoloLens device {1}.", WindowsExports.GetArchitectureSubpath(Arch), DeviceAddress));
				}

				ActualArchitectures = ArchList.Distinct().ToArray();
			}
			else
			{
				var ArchList = new List<WindowsArchitecture>();
				bool bBuildForEmulation = false;
				bool bBuildForDevice = false;
                
				if (PlatformEngineConfig.GetBool("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bBuildForEmulation", out bBuildForEmulation))
				{
					if (bBuildForEmulation)
					{
						ArchList.Add(WindowsArchitecture.x64);
					}
				}

				if (PlatformEngineConfig.GetBool("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bBuildForDevice", out bBuildForDevice))
				{
					// If both are unchecked, then we build for device
					if (bBuildForDevice || (!bBuildForEmulation && !bBuildForDevice))
					{
						ArchList.Add(WindowsArchitecture.ARM64);
					}
				}

				ActualArchitectures = ArchList.ToArray();
			}

			string ArchString = ActualArchitectures.Length == 1 && ProjParams.Run && !ProjParams.Package ? WindowsExports.GetArchitectureSubpath(ActualArchitectures[0]) : "Multi";
			ProjParams.SpecifiedArchitecture = ArchString;
			ProjParams.ConfigOverrideParams.Add(String.Format("Architecture={0}", ArchString));

			FindInstruments();
			GenerateSigningCertificate(ProjParams, PlatformEngineConfig);
		}

		private void FindInstruments()
		{
			if(string.IsNullOrEmpty(Windows10SDKVersion))
			{
				Windows10SDKVersion = "Latest";
			}

			if(!HoloLensExports.InitWindowsSdkToolPath(Windows10SDKVersion, Log.Logger))
			{
				throw new AutomationException(ExitCode.Error_Arguments, "Wrong WinSDK toolchain selected on \'Platforms/HoloLens/Toolchain\' page. Please check.");
			}

			// VS puts MSBuild stuff (where PDBCopy lives) under the Visual Studio Installation directory
			DirectoryReference MSBuildInstallDir = new DirectoryReference(Path.Combine(WindowsExports.GetMSBuildToolPath(), "..", "..", ".."));

			DirectoryReference SDKFolder;
			Version SDKVersion;

			if(WindowsExports.TryGetWindowsSdkDir(Windows10SDKVersion, out SDKVersion, out SDKFolder))
			{
				DirectoryReference WindowsSdkBinDir = DirectoryReference.Combine(SDKFolder, "bin", SDKVersion.ToString(), Environment.Is64BitProcess ? "x64" : "x86");
				if (!DirectoryReference.Exists(WindowsSdkBinDir))
				{
					throw new AutomationException(ExitCode.Error_Arguments, "WinSDK toolchain selected on \'Platforms/HoloLens/Toolchain\' page isn't exist anymore. Please select new one.");
				}
				if (PDBCopyPath == null || !FileReference.Exists(PDBCopyPath))
				{
					PDBCopyPath = FileReference.Combine(SDKFolder, "Debuggers", Environment.Is64BitProcess ? "x64" : "x86", "PDBCopy.exe");
				}
			}


			MakeAppXPath = HoloLensExports.GetWindowsSdkToolPath("makeappx.exe");
			SignToolPath = HoloLensExports.GetWindowsSdkToolPath("signtool.exe");
			MakeCertPath = HoloLensExports.GetWindowsSdkToolPath("makecert.exe");
			Pvk2PfxPath = HoloLensExports.GetWindowsSdkToolPath("pvk2pfx.exe");

			IEnumerable<DirectoryReference> VSInstallDirs;
			if (PDBCopyPath == null || !FileReference.Exists(PDBCopyPath))
			{
				if (null != (VSInstallDirs = WindowsExports.TryGetVSInstallDirs(WindowsCompiler.VisualStudio2019)))
				{
					PDBCopyPath = FileReference.Combine(VSInstallDirs.First(), "MSBuild", "Microsoft", "VisualStudio", "v16.0", "AppxPackage", "PDBCopy.exe");
				}
				else if (null != (VSInstallDirs = WindowsExports.TryGetVSInstallDirs(WindowsCompiler.VisualStudio2022)))
				{
					PDBCopyPath = FileReference.Combine(VSInstallDirs.First(), "MSBuild", "Microsoft", "VisualStudio", "v17.0", "AppxPackage", "PDBCopy.exe");
				}
			}

			// Earlier versions use a separate MSBuild install location
			if (PDBCopyPath == null || !FileReference.Exists(PDBCopyPath))
			{
				PDBCopyPath = FileReference.Combine(MSBuildInstallDir, "Microsoft", "VisualStudio", "v14.0", "AppxPackage", "PDBCopy.exe");
			}

			if (PDBCopyPath == null || !FileReference.Exists(PDBCopyPath))
			{
				PDBCopyPath = FileReference.Combine(MSBuildInstallDir, "Microsoft", "VisualStudio", "v12.0", "AppxPackage", "PDBCopy.exe");
			}

			if (!FileReference.Exists(PDBCopyPath))
			{
				PDBCopyPath = null;
			}

		}

		public override void Deploy(ProjectParams Params, DeploymentContext SC)
		{
			foreach (string DeviceAddress in Params.DeviceNames)
			{
				if (IsLocalDevice(DeviceAddress))
				{
					// Special case - we can use PackageManager to allow loose apps, plus we need to
					// apply the loopback exemption in case of cook-on-the-fly
					DeployToLocalDevice(Params, SC);
				}
				else
				{
					DeployToRemoteDevice(DeviceAddress, Params, SC);
				}
			}
		}

		public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
		{
			if(Params.Run && !Params.Package)
			{
				// Stage all the build products
				DirectoryReference ProjectBinariesFolder = Params.GetProjectBinariesPathForPlatform(PlatformType);
				HoloLensExports DeployExports = new HoloLensExports(Log.Logger);
				foreach (StageTarget Target in SC.StageTargets)
				{
					string ArchString = Target.Receipt.Architecture;
					SC.StageBuildProductsFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.bTreatNonShippingBinariesAsDebugFiles);
					DeployExports.AddWinMDReferencesFromReceipt(Target.Receipt, Params.RawProjectPath.Directory, SC.LocalRoot.FullName, Windows10SDKVersion);

					// Stage HoloLens-specific assets (tile, splash, etc.)
					DirectoryReference assetsPath = new DirectoryReference(Path.Combine(ProjectBinariesFolder.FullName, ArchString, "Resources"));
					StagedDirectoryReference stagedAssetPath = new StagedDirectoryReference("Resources");
					SC.StageFiles(StagedFileType.NonUFS, assetsPath, "*.png", StageFilesSearch.AllDirectories, stagedAssetPath);


					SC.StageFile(StagedFileType.NonUFS, new FileReference(Path.Combine(ProjectBinariesFolder.FullName, String.Format("AppxManifest_{0}.xml", ArchString))),
						new StagedFileReference("AppxManifest.xml"));
					SC.StageFile(StagedFileType.NonUFS, new FileReference(Path.Combine(ProjectBinariesFolder.FullName, String.Format("resources_{0}.pri", ArchString))),
						new StagedFileReference("resources.pri"));

					FileReference SourceNetworkManifestPath = new FileReference(Path.Combine(ProjectBinariesFolder.FullName, String.Format("NetworkManifest_{0}.xml", ArchString)));
					if (FileReference.Exists(SourceNetworkManifestPath))
					{
						SC.StageFile(StagedFileType.NonUFS, SourceNetworkManifestPath, new StagedFileReference("NetworkManifest.xml"));
					}

					TargetRules Rules = Params.ProjectTargets.Find(x => x.Rules.Type == TargetType.Game).Rules;
					bool UseDebugCrt = Params.ClientConfigsToBuild.Contains(UnrealTargetConfiguration.Debug) && Rules.bDebugBuildsActuallyUseDebugCRT;
					foreach (var RT in GetPathToVCLibsPackages(UseDebugCrt, Rules.WindowsPlatform.Compiler))
					{
						SC.StageFile(StagedFileType.NonUFS, new FileReference(RT), new StagedFileReference(Path.GetFileName(RT)));
					}
				}
			}
		}

		public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
		{
		    string Return = PlatformType.ToString();
            return bIsClientOnly ? Return + "Client" : Return;
		}

		static void FillMapfile(DirectoryReference DirectoryName, string SearchPath, string Mask, StringBuilder AppXRecipeBuiltFiles)
		{
			string StageDir = DirectoryName.FullName;
			if (!StageDir.EndsWith("\\") && !StageDir.EndsWith("/"))
			{
				StageDir += Path.DirectorySeparatorChar;
			}
			Uri StageDirUri = new Uri(StageDir);
			foreach (var pf in Directory.GetFiles(SearchPath, Mask, SearchOption.AllDirectories))
			{
				var FileUri = new Uri(pf);
				var relativeUri = StageDirUri.MakeRelativeUri(FileUri);
				var relativePath = Uri.UnescapeDataString(relativeUri.ToString());
				var newPath = relativePath.Replace('/', Path.DirectorySeparatorChar);

				AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", pf, newPath));
			}
		}

		static void FillMapfile(DirectoryReference DirectoryName, FileReference ManifestFile, StringBuilder AppXRecipeBuiltFiles)
		{
			if (FileReference.Exists(ManifestFile))
			{
				string[] Lines = FileReference.ReadAllLines(ManifestFile);
				foreach (string Line in Lines)
				{
					string[] Pair = Line.Split('\t');
					if (Pair.Length > 1)
					{
						AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", FileReference.Combine(DirectoryName, Pair[0]), Pair[0]));
					}
				}
			}
		}

		static void FillMapfile(DirectoryReference DirectoryName, string SearchPath, string Mask, Dictionary<string, FileReference> Dict)
		{
			string StageDir = DirectoryName.FullName;
			if (!StageDir.EndsWith("\\") && !StageDir.EndsWith("/"))
			{
				StageDir += Path.DirectorySeparatorChar;
			}
			Uri StageDirUri = new Uri(StageDir);
			foreach (var pf in Directory.GetFiles(SearchPath, Mask, SearchOption.AllDirectories))
			{
				var FileUri = new Uri(pf);
				var relativeUri = StageDirUri.MakeRelativeUri(FileUri);
				var relativePath = Uri.UnescapeDataString(relativeUri.ToString());
				var newPath = relativePath.Replace('/', Path.DirectorySeparatorChar);

				if (!Dict.ContainsKey(newPath))
				{
					Dict[newPath] = new FileReference(pf);
				}
			}
		}

		static void FillMapfile(DirectoryReference DirectoryName, FileReference ManifestFile, Dictionary<string, FileReference> Dict)
		{
			if (FileReference.Exists(ManifestFile))
			{
				string[] Lines = FileReference.ReadAllLines(ManifestFile);
				foreach (string Line in Lines)
				{
					string[] Pair = Line.Split('\t');
					if (Pair.Length > 1)
					{
						if(!Dict.ContainsKey(Pair[0]))
						{
							Dict[Pair[0]] = FileReference.Combine(DirectoryName, Pair[0]);
						}
					}
				}
			}
		}

		private List<string> GetContainerFileExtensions()
		{
			return new List<string> { "*.pak", "*.ucas", "*.utoc" };
		}

		private bool IsUsingContainers(ProjectParams Params)
		{
			return (Params.UsePak(this) || (Params.IoStore && !Params.SkipIoStore));
		}

		private void PackagePakFiles(ProjectParams Params, DeploymentContext SC, string OutputNameBase)
		{
			string IntermediateDirectory = Path.Combine(SC.ProjectRoot.FullName, "Intermediate", "Deploy", "neutral");
			var ListResources = new HoloLensManifestGenerator(Log.Logger).CreateAssetsManifest(SC.StageTargetPlatform.PlatformType, SC.StageDirectory.FullName, IntermediateDirectory, SC.RawProjectPath, SC.ProjectRoot.FullName);

			string OutputName = OutputNameBase + "_pak";

			var AppXRecipeBuiltFiles = new StringBuilder();

			string MapFilename = Path.Combine(SC.StageDirectory.FullName, OutputName + ".pkgmap");
			AppXRecipeBuiltFiles.AppendLine("[ResourceMetadata]");
			AppXRecipeBuiltFiles.AppendLine("\"ResourceId\"\t\"UnrealAssets\"");
			AppXRecipeBuiltFiles.AppendLine("");
			AppXRecipeBuiltFiles.AppendLine(@"[Files]");

			string OutputAppX = Path.Combine(SC.StageDirectory.FullName, OutputName + Extension);

			if(IsUsingContainers(Params))
			{
				foreach (string ContainerExtension in GetContainerFileExtensions())
				{
					FillMapfile(SC.StageDirectory, SC.StageDirectory.FullName, ContainerExtension, AppXRecipeBuiltFiles);
				}
			}
			else
			{
				FillMapfile(SC.StageDirectory, FileReference.Combine(SC.StageDirectory, SC.GetUFSDeployedManifestFileName(null)), AppXRecipeBuiltFiles);
			}

			FillMapfile(SC.StageDirectory, FileReference.Combine(SC.StageDirectory, SC.GetNonUFSDeployedManifestFileName(null)), AppXRecipeBuiltFiles);

			{
				DirectoryReference ResourceFolder = SC.StageDirectory;
				foreach(var ResourcePath in ListResources)
				{
					var ResourceFile = new FileReference(ResourcePath);
					if(ResourceFile.IsUnderDirectory(ResourceFolder))
					{
						AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", ResourceFile.FullName, ResourceFile.MakeRelativeTo(ResourceFolder)));
					}
				}
			}

			//AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", Path.Combine(SC.StageDirectory.FullName, "AppxManifest_assets.xml"), "AppxManifest.xml"));

			File.WriteAllText(MapFilename, AppXRecipeBuiltFiles.ToString(), Encoding.UTF8);

			string MakeAppXCommandLine = String.Format(@"pack /r /o /f ""{0}"" /p ""{1}"" /m ""{2}""", MapFilename, OutputAppX, Path.Combine(SC.StageDirectory.FullName, "AppxManifest_assets.xml"));
			RunAndLog(CmdEnv, MakeAppXPath.FullName, MakeAppXCommandLine, null, 0, null, ERunOptions.None);
			SignPackage(Params, SC, OutputAppX);
		}

		private void UpdateCodePackagesWithData(ProjectParams Params, DeploymentContext SC, string OutputNameBase)
		{
			foreach (StageTarget Target in SC.StageTargets)
			{
				foreach (BuildProduct Product in Target.Receipt.BuildProducts)
				{
					if (Product.Type != BuildProductType.MapFile)
					{
						continue;
					}

					string MapFilename = Product.Path.FullName; 

					var AppXRecipeBuiltFiles = new StringBuilder();

					string OutputAppX = Path.Combine(SC.StageDirectory.FullName, Product.Path.GetFileNameWithoutExtension() + Extension);

					if(IsUsingContainers(Params))
					{
						foreach (string ContainerExtension in GetContainerFileExtensions())
						{
							FillMapfile(SC.StageDirectory, SC.StageDirectory.FullName, ContainerExtension, AppXRecipeBuiltFiles);
						}
					}
					else
					{
						FillMapfile(SC.StageDirectory, FileReference.Combine(SC.StageDirectory, SC.GetUFSDeployedManifestFileName(null)), AppXRecipeBuiltFiles);
					}

					FillMapfile(SC.StageDirectory, FileReference.Combine(SC.StageDirectory, SC.GetNonUFSDeployedManifestFileName(null)), AppXRecipeBuiltFiles);

					File.AppendAllText(MapFilename, AppXRecipeBuiltFiles.ToString(), Encoding.UTF8);

					string MakeAppXCommandLine = String.Format(@"pack /o /f ""{0}"" /p ""{1}""", MapFilename, OutputAppX);
					RunAndLog(CmdEnv, MakeAppXPath.FullName, MakeAppXCommandLine, null, 0, null, ERunOptions.None);

					SignPackage(Params, SC, OutputAppX);
				}
			}
		}

		private void MakeSingleApp(ProjectParams Params, DeploymentContext SC, string OutputNameBase)
		{
			foreach (StageTarget Target in SC.StageTargets)
			{
				string IntermediateDirectory = Path.Combine(SC.ProjectRoot.FullName, "Intermediate", "Deploy", Params.SpecifiedArchitecture);
				var Receipt = Target.Receipt;
				string OutputName = String.Format("{0}_{1}_{2}_{3}", Receipt.TargetName, Receipt.Platform, Receipt.Configuration, Receipt.Architecture);

				string MapFilename = Path.Combine(IntermediateDirectory, OutputName + "_full.pkgmap");

				Dictionary<string, FileReference> Dict = new Dictionary<string, FileReference>();


				{
					//parse old mapfile
					string OldMapFilename = Path.Combine(IntermediateDirectory, OutputName + ".pkgmap");
					string[] lines = File.ReadAllLines(OldMapFilename, Encoding.UTF8);
					foreach (var line in lines)
					{
						if (line.Length == 0 || line[0] == '[')
						{
							continue;
						}

						string[] files = line.Split('\t');

						if(files.Length != 2)
						{
							continue;
						}

						files[0] = files[0].Trim('\"');
						files[1] = files[1].Trim('\"').Replace('\\', '/');

						if (Dict.ContainsKey(files[1]))
						{
							continue;
						}

						Dict[files[1]] = new FileReference(files[0]);
					}
				}

				string OutputAppX = Path.Combine(SC.StageDirectory.FullName, OutputName + Extension);

				if (IsUsingContainers(Params) && !Params.Run)
				{
					foreach (string ContainerExtension in GetContainerFileExtensions())
					{
						FillMapfile(SC.StageDirectory, SC.StageDirectory.FullName, ContainerExtension, Dict);
					}
				}
				else
				{
					FillMapfile(SC.StageDirectory, FileReference.Combine(SC.StageDirectory, SC.GetUFSDeployedManifestFileName(null)), Dict);
				}

				FillMapfile(SC.StageDirectory, FileReference.Combine(SC.StageDirectory, SC.GetNonUFSDeployedManifestFileName(null)), Dict);


				var AppXRecipeBuiltFiles = new StringBuilder();

				AppXRecipeBuiltFiles.AppendLine("[Files]");
				foreach (var P in Dict)
				{
					AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", P.Value, P.Key));
				}

				File.WriteAllText(MapFilename, AppXRecipeBuiltFiles.ToString(), Encoding.UTF8);

				string MakeAppXCommandLine = String.Format("pack /o /f \"{0}\" /p \"{1}\"", MapFilename, OutputAppX);
				RunAndLog(CmdEnv, MakeAppXPath.FullName, MakeAppXCommandLine, null, 0, null, ERunOptions.None);

				SignPackage(Params, SC, OutputAppX);
			}
		}
		
		void MakeAppInstaller(ProjectParams Params, DeploymentContext SC, string OutputNameBase, string autoUpdateURL, int hoursBetweenUpdates)
		{
			// Validate url
			string url = autoUpdateURL.Trim();
			{
				if (!url.EndsWith("/"))
				{
					url += "/";
				}

				if (!url.ToLower().StartsWith("http:") && !url.ToLower().StartsWith("https:") && !url.ToLower().StartsWith("www."))
				{
					if (!url.StartsWith("file:"))
					{
						url = "file:" + url;
					}
				}

				url = url.Replace('\\', '/');
			}

			string version;
			string packageName; 
			string appxBundleName = OutputNameBase + ".appxbundle";
			string publisherName;

			ConfigHierarchy PlatformGameConfig = null;
			Params.GameConfigs.TryGetValue(PlatformType, out PlatformGameConfig);
			PlatformGameConfig.GetString("/Script/EngineSettings.GeneralProjectSettings", "ProjectName", out packageName);
			PlatformGameConfig.GetString("/Script/EngineSettings.GeneralProjectSettings", "CompanyDistinguishedName", out publisherName);
			PlatformGameConfig.GetString("/Script/EngineSettings.GeneralProjectSettings", "ProjectVersion", out version);

			// Package name from project settings may be unique words, but installer and manifest need to strip whitespace to validate.
			packageName = packageName.Replace(" ", String.Empty);

			string VCLibsName = "Microsoft.VCLibs.140.00";
			string VCLibsPackage = "Microsoft.VCLibs.arm64.14.00.appx";
			string VCLibsInfo = "Publisher=\"CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US\" Version=\"14.0.0.0\" ProcessorArchitecture=\"arm64\"";
			TargetRules Rules = Params.ProjectTargets.Find(x => x.Rules.Type == TargetType.Game).Rules;
			bool UseDebugCrt = Params.ClientConfigsToBuild.Contains(UnrealTargetConfiguration.Debug) && Rules.bDebugBuildsActuallyUseDebugCRT;
			if (UseDebugCrt)
			{
				VCLibsName += ".Debug";
				VCLibsPackage = "Microsoft.VCLibs.arm64.Debug.14.00.appx";
			}

			// Ideally we would use an XmlWriter, but the installer uses an xmlns attribute which the XmlWriter fails to validate.
			var builder = new StringBuilder();

			string AppInstallerFilename = Path.Combine(SC.StageDirectory.FullName, OutputNameBase + ".appinstaller");
			builder.AppendLine("<?xml version=\"1.0\" encoding=\"utf-8\"?>");
			builder.AppendLine("<AppInstaller Uri=" + "\"" + url + OutputNameBase + ".appinstaller" + "\"" + " Version=" + "\"" + version + "\"" + " xmlns=\"http://schemas.microsoft.com/appx/appinstaller/2017/2\">");
			builder.AppendLine("  <MainBundle Name=" + "\"" + packageName + "\"" + " Version=" + "\"" + version + "\"" + " Publisher=" + "\"" + publisherName + "\"" + " Uri=" + "\"" + url + appxBundleName + "\"" + " />");
			builder.AppendLine("  <Dependencies>");
			builder.AppendLine("    <Package Name=" + "\"" + VCLibsName + "\" " + VCLibsInfo + " Uri=" + "\"" + url + VCLibsPackage + "\"" + " />");
			builder.AppendLine("  </Dependencies>");
			builder.AppendLine("  <UpdateSettings>");
			builder.AppendLine("    <OnLaunch HoursBetweenUpdateChecks=" + "\"" + hoursBetweenUpdates + "\"" + " />");
			builder.AppendLine("  </UpdateSettings>");
			builder.AppendLine("</AppInstaller>");

			File.WriteAllText(AppInstallerFilename, builder.ToString(), Encoding.UTF8);
		}

		void MakeBundle(ProjectParams Params, DeploymentContext SC, string OutputNameBase, bool SeparateAssetPackaging)
		{
			string OutputName = OutputNameBase;

			var AppXRecipeBuiltFiles = new StringBuilder();

			string MapFilename = Path.Combine(SC.StageDirectory.FullName, OutputName + "_bundle.pkgmap");
			AppXRecipeBuiltFiles.AppendLine(@"[Files]");

			string OutputAppX = Path.Combine(SC.StageDirectory.FullName, OutputName + Extension + "bundle");
			if(SeparateAssetPackaging)
			{
				foreach (StageTarget Target in SC.StageTargets)
				{
					foreach (BuildProduct Product in Target.Receipt.BuildProducts)
					{
						if (Product.Type == BuildProductType.Package)
						{
							AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", Product.Path.FullName, Path.GetFileName(Product.Path.FullName)));
						}
					}
				}

				AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", Path.Combine(SC.StageDirectory.FullName, OutputName + "_pak" + Extension), OutputName + "_pak" + Extension));//assets from pak file
			}
			else
			{
				FillMapfile(SC.StageDirectory, SC.StageDirectory.FullName, "*" + Extension, AppXRecipeBuiltFiles);
			}

			File.WriteAllText(MapFilename, AppXRecipeBuiltFiles.ToString(), Encoding.UTF8);

			ConfigHierarchy PlatformGameConfig = null;
			string projectVersion = "0.0.0.0";
			if (Params.GameConfigs.TryGetValue(PlatformType, out PlatformGameConfig))
			{
				PlatformGameConfig.GetString("/Script/EngineSettings.GeneralProjectSettings", "ProjectVersion", out projectVersion);
			}

			string MakeAppXCommandLine = string.Format(@"bundle /o /f ""{0}"" /p ""{1}"" /bv {2}", MapFilename, OutputAppX, projectVersion);
			RunAndLog(CmdEnv, MakeAppXPath.FullName, MakeAppXCommandLine, null, 0, null, ERunOptions.None);
			SignPackage(Params, SC, OutputName + Extension + "bundle", true);
		}

		private void CopyVCLibs(ProjectParams Params, DeploymentContext SC)
		{
			TargetRules Rules = Params.ProjectTargets.Find(x => x.Rules.Type == TargetType.Game)?.Rules;

			bool UseDebugCrt = false;
			WindowsCompiler compiler = WindowsCompiler.VisualStudio2019;

			//TODO: Why is this null?
			if (Rules != null)
			{
				UseDebugCrt = Params.ClientConfigsToBuild.Contains(UnrealTargetConfiguration.Debug) && Rules.bDebugBuildsActuallyUseDebugCRT;
				compiler = Rules.WindowsPlatform.Compiler;
			}

			foreach (string vcLib in GetPathToVCLibsPackages(UseDebugCrt, compiler))
			{
				CopyFile(vcLib, Path.Combine(SC.StageDirectory.FullName, Path.GetFileName(vcLib)));
			}
		}

		private void GenerateSigningCertificate(ProjectParams Params, ConfigHierarchy PlatformEngineConfig)
		{
			string SigningCertificate = @"Build\HoloLens\SigningCertificate.pfx";
			string SigningCertificatePath = Path.Combine(Params.RawProjectPath.Directory.FullName, SigningCertificate);
			if (!File.Exists(SigningCertificatePath))
			{
				if (!IsBuildMachine && !Params.Unattended)
				{
					LogError("Certificate is required.  Please go to Project Settings > HoloLens > Create Signing Certificate");
				}
			}
		}

		private void SignPackage(ProjectParams Params, DeploymentContext SC, string OutputName, bool GenerateCer = false)
		{
			string OutputNameBase = Path.GetFileNameWithoutExtension(OutputName);
			string OutputAppX = Path.Combine(SC.StageDirectory.FullName, OutputName);
			string SigningCertificate = @"Build\HoloLens\SigningCertificate.pfx";

			if (GenerateCer)
			{
				// Emit a .cer file adjacent to the appx so it can be installed to enable packaged deployment
				System.Security.Cryptography.X509Certificates.X509Certificate2 ActualCert = new System.Security.Cryptography.X509Certificates.X509Certificate2(Path.Combine(SC.ProjectRoot.FullName, SigningCertificate));
				File.WriteAllText(Path.Combine(SC.StageDirectory.FullName, OutputNameBase + ".cer"), Convert.ToBase64String(ActualCert.Export(System.Security.Cryptography.X509Certificates.X509ContentType.Cert)));
			}

			string CertFile = Path.Combine(SC.ProjectRoot.FullName, SigningCertificate);
			if(File.Exists(CertFile))
			{
				string SignToolCommandLine = string.Format(@"sign /a /f ""{0}"" /fd SHA256 ""{1}""", CertFile, OutputAppX);
				RunAndLog(CmdEnv, SignToolPath.FullName, SignToolCommandLine, null, 0, null, ERunOptions.None);
			}
		}

		public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
		{
			//GenerateDLCManifestIfNecessary(Params, SC);
			string OutputNameBase = Params.HasDLCName ? Params.DLCFile.GetFileNameWithoutExtension() : Params.ShortProjectName;
			string OutputAppX = Path.Combine(SC.StageDirectory.FullName, OutputNameBase + Extension);
			{
				OutputAppX += "bundle";
				bool SeparateAssetPackaging = true;
				bool bStartInVR = false;

				//auto update
				bool bShouldCreateAppInstaller = false;
				string autoUpdateURL = string.Empty;
				int hoursBetweenUpdates = 0;
				
				ConfigHierarchy PlatformEngineConfig = null;
				if (Params.EngineConfigs.TryGetValue(PlatformType, out PlatformEngineConfig))
				{
					//PlatformEngineConfig.GetBool("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bUseAssetPackage", out SeparateAssetPackaging);
					
					// Get auto update vars
					PlatformEngineConfig.GetBool("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bShouldCreateAppInstaller", out bShouldCreateAppInstaller);
					PlatformEngineConfig.GetString("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "AppInstallerInstallationURL", out autoUpdateURL);
					PlatformEngineConfig.GetInt32("/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "HoursBetweenUpdateChecks", out hoursBetweenUpdates);
				}

				ConfigHierarchy PlatformGameConfig = null;
				if (Params.GameConfigs.TryGetValue(PlatformType, out PlatformGameConfig))
				{
					PlatformGameConfig.GetBool("/Script/EngineSettings.GeneralProjectSettings", "bStartInVR", out bStartInVR);
					Log.TraceInformation("bStartInVR = {0}", bStartInVR.ToString());
				}

				if (bStartInVR)
				{
					bool updateCommandLine = false;
					if (string.IsNullOrEmpty(Params.StageCommandline))
					{
						Params.StageCommandline = "-vr";
						updateCommandLine = true;
					}
					else if (!Params.StageCommandline.Contains("-vr"))
					{
						Params.StageCommandline += " -vr";
						updateCommandLine = true;
					}

					if (updateCommandLine)
					{
						// Update the uecommandline.txt
						FileReference IntermediateCmdLineFile = FileReference.Combine(SC.StageDirectory, "UECommandLine.txt");
						Log.TraceInformation("Writing cmd line to: " + IntermediateCmdLineFile.FullName);
						Project.WriteStageCommandline(IntermediateCmdLineFile, Params, SC);
					}
				}

				if (SeparateAssetPackaging)
				{
					PackagePakFiles(Params, SC, OutputNameBase);
				}
				else
				{
					UpdateCodePackagesWithData(Params, SC, OutputNameBase);
				}

				MakeBundle(Params, SC, OutputNameBase, SeparateAssetPackaging);
				
				if (bShouldCreateAppInstaller)
				{
					MakeAppInstaller(Params, SC, OutputNameBase, autoUpdateURL, hoursBetweenUpdates);
				}
			}


			CopyVCLibs(Params, SC);

			// If the user indicated that they will distribute this build, then let's also generate an
			// appxupload file suitable for submission to the Windows Store.  This file zips together
			// the appx package and the public symbols (themselves zipped) for the binaries.
			if (Params.Distribution)
			{
				List<FileReference> SymbolFilesToZip = new List<FileReference>();
				DirectoryReference StageDirRef = new DirectoryReference(SC.StageDirectory.FullName);
				DirectoryReference PublicSymbols = DirectoryReference.Combine(StageDirRef, "PublicSymbols");
				DirectoryReference.CreateDirectory(PublicSymbols);
				foreach (StageTarget Target in SC.StageTargets)
				{
					foreach (BuildProduct Product in Target.Receipt.BuildProducts)
					{
						if (Product.Type == BuildProductType.SymbolFile)
						{
							FileReference FullSymbolFile = new FileReference(Product.Path.FullName);
							FileReference TempStrippedSymbols = FileReference.Combine(PublicSymbols, FullSymbolFile.GetFileName());
							StripSymbols(FullSymbolFile, TempStrippedSymbols);
							SymbolFilesToZip.Add(TempStrippedSymbols);
						}
					}
				}
				FileReference AppxSymFile = FileReference.Combine(StageDirRef, OutputNameBase + Extension + "sym");
				ZipFiles(AppxSymFile, PublicSymbols, SymbolFilesToZip);

				FileReference AppxUploadFile = FileReference.Combine(StageDirRef, OutputNameBase + Extension + "upload");
				ZipFiles(AppxUploadFile, StageDirRef,
					new FileReference[]
					{
							new FileReference(OutputAppX),
							AppxSymFile
					});
			}
		}

		public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		{
			IProcessResult ProcResult = null;
			foreach (string DeviceAddress in Params.DeviceNames)
			{
				// Prefer launcher tool for local device since it avoids having to deal with certificate issues on the
				// device portal connection.
				if (IsLocalDevice(DeviceAddress))
				{
					ProcResult = RunUsingLauncherTool(DeviceAddress, ClientRunFlags, ClientApp, ClientCmdLine, Params);
				}
				else
				{
					ProcResult = RunUsingDevicePortal(DeviceAddress, ClientRunFlags, ClientApp, ClientCmdLine, Params);
				}
			}
			return ProcResult;
		}

        public override List<FileReference> GetExecutableNames(DeploymentContext SC)
		{
			// If we're calling this for the purpose of running the app then the string we really
			// need is the AUMID.  We can't form a full AUMID here without making assumptions about 
			// how the PFN is built, which (while straightforward) does not appear to be officially
			// documented.  So we'll save off the path to the manifest, which the launch process can
			// parse later for information that, in conjunction with the target device, will allow
			// for looking up the true AUMID.
			List<FileReference> Exes = new List<FileReference>();
			Exes.Add(new FileReference(Path.Combine(SC.StageDirectory.FullName, "AppxManifest.xml")));
			return Exes;
		}

		public override bool IsSupported { get { return true; } }
		public override bool UseAbsLog { get { return false; } }
		public override bool LaunchViaUFE { get { return false; } }
		public override string ICUDataVersion { get { return "icudt53l"; } }


		public override List<string> GetDebugFileExtensions()
		{
			return new List<string> { ".pdb", ".map" };
		}

		protected override string GetPlatformExeExtension()
		{
			return ".exe";
		}


		public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
		{
			bool bStripInPlace = false;

			if (SourceFile == TargetFile)
			{
				// PDBCopy only supports creation of a brand new stripped file so we have to create a temporary filename
				TargetFile = new FileReference(Path.Combine(TargetFile.Directory.FullName, Guid.NewGuid().ToString() + TargetFile.GetExtension()));
				bStripInPlace = true;
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();

			if (PDBCopyPath == null || !FileReference.Exists(PDBCopyPath))
			{
				throw new AutomationException(ExitCode.Error_SDKNotFound, "Debugging Tools for Windows aren't installed. Please follow https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-download-tools");
			}

			StartInfo.FileName = PDBCopyPath.FullName;
			StartInfo.Arguments = String.Format("\"{0}\" \"{1}\" -p", SourceFile.FullName, TargetFile.FullName);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Log.Logger);

			if (bStripInPlace)
			{
				// Copy stripped file to original location and delete the temporary file
				File.Copy(TargetFile.FullName, SourceFile.FullName, true);
				FileReference.Delete(TargetFile);
			}
		}

		private void GenerateSigningCertificate(string InCertificatePath, string InPublisher)
		{
			// Ensure the output directory exists otherwise makecert will fail.
			InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(InCertificatePath));

			// MakeCert.exe -r -h 0 -n "CN=No Publisher, O=No Publisher" -eku 1.3.6.1.5.5.7.3.3 -pe -sv "Signing Certificate.pvk" "Signing Certificate.cer"
			// pvk2pfx -pvk "Signing Certificate.pvk" -spc "Signing Certificate.cer" -pfx "Signing Certificate.pfx"
			string CerFile = Path.ChangeExtension(InCertificatePath, ".cer");
			string PvkFile = Path.ChangeExtension(InCertificatePath, ".pvk");

			string MakeCertCommandLine = string.Format(@"-r -h 0 -n ""{0}"" -eku 1.3.6.1.5.5.7.3.3 -pe -sv ""{1}"" ""{2}""", InPublisher, PvkFile, CerFile);
			RunAndLog(CmdEnv, MakeCertPath.FullName, MakeCertCommandLine, null, 0, null, ERunOptions.None);

			string Pvk2PfxCommandLine = string.Format(@"-pvk ""{0}"" -spc ""{1}"" -pfx ""{2}""", PvkFile, CerFile, InCertificatePath);
			RunAndLog(CmdEnv, Pvk2PfxPath.FullName, Pvk2PfxCommandLine, null, 0, null, ERunOptions.None);
		}

		private bool IsLocalDevice(string DeviceAddress)
		{
			Uri uriResult;
			bool result = Uri.TryCreate(DeviceAddress, UriKind.Absolute, out uriResult)
				&& (uriResult.Scheme == Uri.UriSchemeHttp || uriResult.Scheme == Uri.UriSchemeHttps);
			return !result;
		}

		private void WaitFor(IAsyncOperationWithProgress<DeploymentResult, DeploymentProgress> deploymentOperation)
		{
			// This event is signaled when the operation completes
			ManualResetEvent opCompletedEvent = new ManualResetEvent(false);

			// Define the delegate using a statement lambda
			deploymentOperation.Completed = (depProgress, status) => { opCompletedEvent.Set(); };

			// Wait until the operation completes
			opCompletedEvent.WaitOne();

			// Check the status of the operation
			if (deploymentOperation.Status == AsyncStatus.Error)
			{
				DeploymentResult deploymentResult = deploymentOperation.GetResults();
				LogInformation("Error code: {0}", deploymentOperation.ErrorCode);
				LogInformation("Error text: {0}", deploymentResult.ErrorText);
				throw new AutomationException(ExitCode.Error_AppInstallFailed, deploymentResult.ErrorText, "");
			}
			else if (deploymentOperation.Status == AsyncStatus.Canceled)
			{
				//LogInformation("Operation canceled");
				throw new AutomationException(ExitCode.Error_AppInstallFailed, "Operation canceled", "");
			}
			else if (deploymentOperation.Status == AsyncStatus.Completed)
			{
				//LogInformation("Operation succeeded");
			}
			else
			{
				//LogInformation("Operation status unknown");
			}
		}

		private void DeployToLocalDevice(ProjectParams Params, DeploymentContext SC)
		{
            if (!RuntimePlatform.IsWindows)
            {
                return;
            }

            bool bRequiresPackage = Params.Package || SC.StageTargetPlatform.RequiresPackageToDeploy;
			string Name;
			string Publisher;
			GetPackageInfo(Params, out Name, out Publisher);
			PackageManager PackMgr = new PackageManager();
			try
			{
				var ExistingPackage = PackMgr.FindPackagesForUser("", Name, Publisher).FirstOrDefault();

				// Only remove an existing package if it's in development mode; otherwise the removal might silently delete stuff
				// that the user wanted.
				if (ExistingPackage != null)
				{
					if (ExistingPackage.IsDevelopmentMode)
					{
						WaitFor(PackMgr.RemovePackageAsync(ExistingPackage.Id.FullName, RemovalOptions.PreserveApplicationData));
					}
					else if (!Params.Package)
					{
						throw new AutomationException(ExitCode.Error_AppInstallFailed, "A packaged version of the application already exists.  It must be uninstalled manually - note this will remove user data.");
					}
				}

				{
					string PackagePath = Path.Combine(SC.StageDirectory.FullName, "AppxManifest.xml");


					LogInformation(String.Format("Registring {0}", PackagePath));
					WaitFor(PackMgr.RegisterPackageAsync(new Uri(PackagePath), null, DeploymentOptions.DevelopmentMode));
				}
			}
			catch (AggregateException agg)
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, agg.InnerException, "");
			}

			// Package should now be installed.  Locate it and make sure it's permitted to connect over loopback.
			try
			{
				var InstalledPackage = PackMgr.FindPackagesForUser("", Name, Publisher).FirstOrDefault();
				string LoopbackExemptCmdLine = string.Format("loopbackexempt -a -n={0}", InstalledPackage.Id.FamilyName);
				RunAndLog(CmdEnv, "checknetisolation.exe", LoopbackExemptCmdLine, null, 0, null, ERunOptions.None);
			}
			catch
			{
				LogWarning("Failed to apply a loopback exemption to the deployed app.  Connection to a local cook server will fail.");
			}
		}

		private WindowsArchitecture RemoteDeviceArchitecture(string DeviceAddress, ProjectParams Params)
		{
			string OsVersionString = "";
			try
			{
				DefaultDevicePortalConnection conn = new DefaultDevicePortalConnection(DeviceAddress, Params.DeviceUsername, Params.DevicePassword);
				DevicePortal portal = new DevicePortal(conn);
				portal.ConnectionStatus += Portal_ConnectionStatus;
				//portal.ConnectionStatus += (sender, args) => { Console.WriteLine(String.Format("Connected to portal with {0}", args.Status)); }; 

				portal.ConnectAsync().Wait(); //think about adding a cert here!
				var osInfo = portal.GetOperatingSystemInformationAsync().Result;
				OsVersionString = osInfo.OsVersionString;
			}
			catch (AggregateException e)
			{
				if (e.InnerException is AutomationException)
				{
					throw e.InnerException;
				}
				else
				{
					throw new AutomationException(ExitCode.Error_AppInstallFailed, e.InnerException, e.InnerException.Message);
				}
			}
			catch (Exception e)
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, e, e.Message);
			}


			string[] osBlocks = OsVersionString.Split('.');
			if (osBlocks.Length < 3)
			{
				LogError(String.Format("Wrong OS version string {0} from {1} device", OsVersionString, DeviceAddress));
				throw new AutomationException(ExitCode.Error_AppInstallFailed, "Wrong OS version string");
			}

			string osArchBlock = osBlocks[2].ToLower();

			if (osArchBlock.StartsWith("amd64"))
			{
				return WindowsArchitecture.x64;
			}
			else if (osArchBlock.StartsWith("arm64"))
			{
				return WindowsArchitecture.ARM64;
			}
			else
			{
				LogError(String.Format("Unsupported OS architecture {0} from {1} device", osArchBlock, DeviceAddress));
				throw new AutomationException(ExitCode.Error_AppInstallFailed, "Unsupported OS architecture");
			}
		}

		private void DeployToRemoteDevice(string DeviceAddress, ProjectParams Params, DeploymentContext SC)
		{
			if (!RuntimePlatform.IsWindows)
            {
                return;
            }

			string Name;
			string Publisher;
			GetPackageInfo(Params, out Name, out Publisher);

			if (Params.Package || SC.StageTargetPlatform.RequiresPackageToDeploy)
			{
				DefaultDevicePortalConnection conn = new DefaultDevicePortalConnection(DeviceAddress, Params.DeviceUsername, Params.DevicePassword);
				DevicePortal portal = new DevicePortal(conn);
				portal.UnvalidatedCert += (sender, certificate, chain, sslPolicyErrors) =>
				{
					return ShouldAcceptCertificate(new X509Certificate2(certificate), Params.Unattended);
				};
				portal.ConnectionStatus += Portal_ConnectionStatus;
				try
				{
					portal.ConnectAsync().Wait();
					string PackageName = Params.ShortProjectName;
					string PackagePath = Path.Combine(SC.StageDirectory.FullName, PackageName + Extension + "bundle");
					string CertPath = Path.Combine(SC.StageDirectory.FullName, PackageName + ".cer");

					{
						var list = portal.GetInstalledAppPackagesAsync().Result;
						//LogInformation(String.Format("Current Name = {0}, Publisher = {1}", Name, Publisher)); //code for installation debugging 
						//foreach(var p in list.Packages)
						//{
						//	LogInformation(String.Format("Package FamilyName = {0}, FullName = {1}, Publisher = {2}", p.FamilyName, p.FullName, p.Publisher));
						//}
						foreach (var pack in list.Packages.FindAll(package => package.FamilyName.StartsWith(Name) && package.Publisher == Publisher))
						{
							portal.UninstallApplicationAsync(pack.FullName).Wait();
						}
					}

					List<string> Dependencies = new List<string>();
					bool UseDebugCrt = false;
					WindowsCompiler Compiler = WindowsCompiler.Default;
					TargetRules Rules = null;
					if (Params.HasGameTargetDetected)
					{
						//Rules = Params.ProjectTargets[TargetType.Game].Rules;
						Rules = Params.ProjectTargets.Find(x => x.Rules.Type == TargetType.Game).Rules;
					}
					else if (Params.HasClientTargetDetected)
					{
						Rules = Params.ProjectTargets.Find(x => x.Rules.Type == TargetType.Game).Rules;
					}

					if (Rules != null)
					{
						UseDebugCrt = Params.ClientConfigsToBuild.Contains(UnrealTargetConfiguration.Debug) && Rules.bDebugBuildsActuallyUseDebugCRT;
						Compiler = Rules.WindowsPlatform.Compiler;
					}

					Dependencies.AddRange(GetPathToVCLibsPackages(UseDebugCrt, Compiler));

					portal.AppInstallStatus += Portal_AppInstallStatus;
					portal.InstallApplicationAsync(string.Empty, PackagePath, Dependencies, CertPath).Wait();
				}
				catch (AggregateException e)
				{
					if (e.InnerException is AutomationException)
					{
						throw e.InnerException;
					}
					else
					{
						throw new AutomationException(ExitCode.Error_AppInstallFailed, e.InnerException, e.InnerException.Message);
					}
				}
				catch (Exception e)
				{
					throw new AutomationException(ExitCode.Error_AppInstallFailed, e, e.Message);
				}
			}
			else
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, "Remote deployment of unpackaged apps is not supported.");
			}
		}


		private IProcessResult RunUsingLauncherTool(string DeviceAddress, ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		{
            if (!RuntimePlatform.IsWindows)
            {
                return null;
            }
			return RunUsingLauncherToolAsync(Params, ClientRunFlags).Result;
		}


		// Currently does not build on UE4 build servers. Temporarily blocking out until resolved.
#if false
		private async Task<HoloLensLauncherCreatedProcess> RunUsingLauncherToolAsync(ProjectParams Params, ERunOptions ClientRunFlags)
		{
			string Name;
			string Publisher;
			GetPackageInfo(Params, out Name, out Publisher);

			PackageManager PackMgr = new PackageManager();
			Windows.ApplicationModel.Package InstalledPackage = PackMgr.FindPackagesForUser("", Name, Publisher).FirstOrDefault();

			if (InstalledPackage == null)
			{
				throw new AutomationException(ExitCode.Error_LauncherFailed, "Could not find installed app (Name: {0}, Publisher: {1}", Name, Publisher);
			}

			IReadOnlyList<AppListEntry> entries = await InstalledPackage.GetAppListEntriesAsync();

			if (entries.Count == 0)
			{
				throw new AutomationException(ExitCode.Error_LauncherFailed, "Could not find appentry in installed app (Name: {0}, Publisher: {1}", Name, Publisher);
			}

			AppListEntry entry = entries.First();
			if (!await entry.LaunchAsync())
			{
				return null;
			}

			string LogFile;
			if (Params.CookOnTheFly)
			{
				LogFile = Path.Combine(Params.RawProjectPath.Directory.FullName, "Saved", "Cooked", PlatformType.ToString(), Params.ShortProjectName, "HoloLensLocalAppData", Params.ShortProjectName, "Saved", "Logs", Params.ShortProjectName + ".log");
			}
			else
			{
				LogFile = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Packages", InstalledPackage.Id.FamilyName, "LocalState", Params.ShortProjectName, "Saved", "Logs", Params.ShortProjectName + ".log");
			}

			Process[] Procs = Process.GetProcessesByName(Params.ShortProjectName + Params.SpecifiedArchitecture + ".exe");
			Process Proc = null;
			foreach (var P in Procs)
			{
				DirectoryReference WorkDir = new DirectoryReference(P.StartInfo.WorkingDirectory);
				if (WorkDir.IsUnderDirectory(Params.RawProjectPath.Directory))
				{
					Proc = P;
					break;
				}
			}

			if (Proc == null)
			{
				return null;
			}

			bool AllowSpew = ClientRunFlags.HasFlag(ERunOptions.AllowSpew);
			LogEventType SpewVerbosity = ClientRunFlags.HasFlag(ERunOptions.SpewIsVerbose) ? LogEventType.Verbose : LogEventType.Console;
			HoloLensLauncherCreatedProcess UwpProcessResult = new HoloLensLauncherCreatedProcess(Proc, LogFile, AllowSpew, SpewVerbosity);
			ProcessManager.AddProcess(UwpProcessResult);

			ProcessManager.AddProcess(UwpProcessResult);
			if (!ClientRunFlags.HasFlag(ERunOptions.NoWaitForExit))
			{
				UwpProcessResult.WaitForExit();
				UwpProcessResult.OnProcessExited();
				UwpProcessResult.DisposeProcess();
			}
			return UwpProcessResult;
		}
#else
		private Task<HoloLensLauncherCreatedProcess> RunUsingLauncherToolAsync(ProjectParams Params, ERunOptions ClientRunFlags)
		{
			return null;
		}
#endif

		private IProcessResult RunUsingDevicePortal(string DeviceAddress, ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
		{
            if (!RuntimePlatform.IsWindows)
            {
                return null;
            }

            string Name;
			string Publisher;
			GetPackageInfo(Params, out Name, out Publisher);

			DefaultDevicePortalConnection conn = new DefaultDevicePortalConnection(DeviceAddress, Params.DeviceUsername, Params.DevicePassword);
			DevicePortal portal = new DevicePortal(conn);
			portal.UnvalidatedCert += (sender, certificate, chain, sslPolicyErrors) =>
			{
				return ShouldAcceptCertificate(new System.Security.Cryptography.X509Certificates.X509Certificate2(certificate), Params.Unattended);
			};

			try
			{
				portal.ConnectAsync().Wait();
				string Aumid = string.Empty;
				string FullName = string.Empty;
				var AllAppsTask = portal.GetInstalledAppPackagesAsync();
				AllAppsTask.Wait();
				foreach (var App in AllAppsTask.Result.Packages)
				{
					// App.Name seems to report the localized name.
					if (App.FamilyName.StartsWith(Name) && App.Publisher == Publisher)
					{
						Aumid = App.AppId;
						FullName = App.FullName;
						break;
					}
				}

				var Result = new HoloLensDevicePortalCreatedProcess(portal, FullName, Params.ShortProjectName);
				var LaunchTask = portal.LaunchApplicationAsync(Aumid, FullName);

				// Message back to the UE4 Editor to correctly set the app id for each device
				Console.WriteLine("Running Package@Device:{0}@{1}", FullName, DeviceAddress);

				LaunchTask.Wait();
				Result.SetProcId(LaunchTask.Result);

				ProcessManager.AddProcess(Result);
				if (!ClientRunFlags.HasFlag(ERunOptions.NoWaitForExit))
				{
					Result.WaitForExit();
					Result.OnProcessExited();
					Result.DisposeProcess();
				}
				return Result;
			}
			catch (AggregateException e)
			{
				if (e.InnerException is AutomationException)
				{
					throw e.InnerException;
				}
				else
				{
					throw new AutomationException(ExitCode.Error_LauncherFailed, e.InnerException, e.InnerException.Message);
				}
			}
			catch (Exception e)
			{
				throw new AutomationException(ExitCode.Error_LauncherFailed, e, e.Message);
			}
		}

		private void GetPackageInfo(ProjectParams Params, out string Name, out string Publisher)
		{
			ConfigHierarchy PlatformEngineConfig = null;
			Params.GameConfigs.TryGetValue(PlatformType, out PlatformEngineConfig);

			if (PlatformEngineConfig == null || !PlatformEngineConfig.GetString("/Script/EngineSettings.GeneralProjectSettings", "ProjectName", out Name))
			{
				Name = "DefaultUE4Project";
			}

			Name = Regex.Replace(Name, "[^-.A-Za-z0-9]", "");

			if (PlatformEngineConfig == null || !PlatformEngineConfig.GetString("/Script/EngineSettings.GeneralProjectSettings", "CompanyDistinguishedName", out Publisher))
			{
				Publisher = "CN=NoPublisher";
			}
		}

		public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
		{
			string OutputNameBase = Params.HasDLCName ? Params.DLCFile.GetFileNameWithoutExtension() : Params.ShortProjectName;
			string PackagePath = SC.StageDirectory.FullName;
			SC.ArchiveFiles(PackagePath, OutputNameBase + Extension + "bundle");
			SC.ArchiveFiles(PackagePath, OutputNameBase + ".cer");
			SC.ArchiveFiles(PackagePath, OutputNameBase + Extension + "upload");

			SC.ArchiveFiles(PackagePath, "*VCLibs*.appx");
			SC.ArchiveFiles(PackagePath, OutputNameBase + ".appinstaller");
		}

		private bool ShouldAcceptCertificate(System.Security.Cryptography.X509Certificates.X509Certificate2 Certificate, bool Unattended)
		{
            if (!RuntimePlatform.IsWindows)
            {
                return false;
            }

            if (AcceptThumbprints.Contains(Certificate.Thumbprint))
			{
				return true;
			}

			if (Unattended)
			{
				throw new AutomationException(ExitCode.Error_CertificateNotFound, "Cannot connect to remote device: certificate is untrusted and cannot prompt for consent (running unattended).");
			}

			System.Windows.Forms.DialogResult AcceptResult = System.Windows.Forms.MessageBox.Show(
				string.Format("Do you want to accept the following certificate?\n\nThumbprint:\n\t{0}\nIssues:\n\t{1}", Certificate.Thumbprint, Certificate.Issuer),
				"Untrusted Certificate Detected",
				System.Windows.Forms.MessageBoxButtons.YesNo,
				System.Windows.Forms.MessageBoxIcon.Question,
				System.Windows.Forms.MessageBoxDefaultButton.Button2);
			if (AcceptResult == System.Windows.Forms.DialogResult.Yes)
			{
				AcceptThumbprints.Add(Certificate.Thumbprint);
				return true;
			}

			throw new AutomationException(ExitCode.Error_CertificateNotFound, "Cannot connect to remote device: certificate is untrusted and user declined to accept.");
		}

		private void Portal_AppInstallStatus(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.ApplicationInstallStatusEventArgs args)
		{
			if (args.Status == Microsoft.Tools.WindowsDevicePortal.ApplicationInstallStatus.Failed)
			{
				throw new AutomationException(ExitCode.Error_AppInstallFailed, args.Message);
			}
			else
			{
				LogLog(args.Message);
			}
		}

		private void Portal_ConnectionStatus(Microsoft.Tools.WindowsDevicePortal.DevicePortal sender, Microsoft.Tools.WindowsDevicePortal.DeviceConnectionStatusEventArgs args)
		{
			if (args.Status == Microsoft.Tools.WindowsDevicePortal.DeviceConnectionStatus.Failed)
			{
				throw new AutomationException(args.Message);
			}
			else
			{
				LogLog(args.Message);
			}
		}

		public string[] GetPathToVCLibsPackages(bool UseDebugCrt, WindowsCompiler Compiler)
		{
			return Gauntlet.TargetDeviceHoloLens.GetPathToVCLibsPackages(UseDebugCrt, Compiler, ActualArchitectures);
		}

		private void GenerateDLCManifestIfNecessary(ProjectParams Params, DeploymentContext SC)
		{
			// Only required for DLC
			if (!Params.HasDLCName)
			{
				return;
			}

			// Only required for the first stage (package or deploy) that requires a manifest.
			// Assumes that the staging directory is pre-cleaned
			if (FileReference.Exists(FileReference.Combine(SC.StageDirectory, "AppxManifest.xml")))
			{
				return;
			}


			HoloLensExports.CreateManifestForDLC(Params.DLCFile, SC.StageDirectory, Log.Logger);
		}

		private static List<string> AcceptThumbprints = new List<string>();
	}
}