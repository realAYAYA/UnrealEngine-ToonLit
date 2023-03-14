// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Threading;
using AutomationTool;
using UnrealBuildTool;
using Ionic.Zip;
using EpicGames.Core;
using UnrealBuildBase;
using AutomationUtils.Automation;
using System.Text.RegularExpressions;
using AutomationScripts;

public class AndroidPlatform : Platform
{
	// Maximum allowed OBB size (1 GiB, 2 GiB or 4 GiB based on project settings)
	private const Int64 SmallOBBSizeAllowed = 1073741824;
	private const Int64 NormalOBBSizeAllowed = 2147483648;
	private const Int64 MaxOBBSizeAllowed = 4294967296;

	private const int DeployMaxParallelCommands = 6;

    private const string TargetAndroidLocation = "obb/";
	private const string TargetAndroidTemp = "/data/local/tmp/";

	public class AdbCreatedProcess : AutomationTool.IProcessResult
	{
		private readonly object StopSyncObject = new object();
		IProcessResult AdbLogProcess;
		string LogPath;
		string PackageName;
		string DeviceName;
		int LogFileProcessExitCode = 0;
		bool bStopped = false;

		public AdbCreatedProcess(
				IProcessResult InAdbLogProcess,
				string InLogPath,
				string InPackageName,
				string InDeviceName)
		{
			AdbLogProcess = InAdbLogProcess;
			LogPath = InLogPath;
			PackageName = InPackageName;
			DeviceName = InDeviceName;
			ProcessManager.AddProcess(this);
		}

		~AdbCreatedProcess()
		{
			ProcessManager.RemoveProcess(this);
		}

		public void StopProcess(bool KillDescendants = true)
		{
			lock (StopSyncObject)
			{
				if (!bStopped)
				{
					AndroidPlatform.RunAdbCommand(DeviceName, "shell am force-stop " + PackageName);
					if (!AdbLogProcess.HasExited)
					{
						AdbLogProcess.StopProcess(KillDescendants);
					}
					DumpDeviceOutputToLogFiles();
					bStopped = true;
				}
			}
		}

		public bool HasExited
		{
			get
			{
				if (!bStopped && (AdbLogProcess.HasExited || !IsPackageRunningOnDevice()))
				{
					StopProcess();
				}
				return bStopped;
			}
		}

		public string GetProcessName()
		{
			return String.Format("{0}@{1}", PackageName, DeviceName);
		}

		public void OnProcessExited()
		{
		}

		public void DisposeProcess()
		{
			AdbLogProcess.DisposeProcess();
		}

		public void StdOut(object sender, DataReceivedEventArgs e)
		{
		}

		public void StdErr(object sender, DataReceivedEventArgs e)
		{
		}

		public int ExitCode
		{
			get { return LogFileProcessExitCode; }
			set { LogFileProcessExitCode = value; }
		}

		public string Output
		{
			get { return AdbLogProcess.Output; }
		}

		public Process ProcessObject
		{
			get { return AdbLogProcess.ProcessObject; }
		}

		public void WaitForExit()
		{
			while (!AdbLogProcess.HasExited && IsPackageRunningOnDevice())
			{
				Thread.Sleep(100);
			}
			StopProcess();
		}

		public FileReference WriteOutputToFile(string FileName)
		{
			return AdbLogProcess.WriteOutputToFile(FileName);
		}

		private bool IsPackageRunningOnDevice()
		{
			ERunOptions Options = ERunOptions.Default | ERunOptions.SpewIsVerbose | ERunOptions.NoLoggingOfRunCommand;
			IProcessResult Result = AndroidPlatform.RunAdbCommand(DeviceName, "shell ps", null, Options);
			string ProcessList = Result.Output;
			bool bIsProcessRunning = ProcessList.Contains(PackageName);
			return bIsProcessRunning;
		}

		private void DumpDeviceOutputToLogFiles()
		{
			string SanitizedDeviceName = DeviceName.Replace(":", "_");
			string LogFilename = Path.Combine(LogPath, "devicelog" + SanitizedDeviceName + ".log");
			string ServerLogFilename = Path.Combine(CmdEnv.LogFolder, "devicelog" + SanitizedDeviceName + ".log");
			ERunOptions Options = ERunOptions.Default & ~ERunOptions.AllowSpew;
			IProcessResult LogFileProcess = RunAdbCommand(DeviceName, "logcat -d", null, Options);
			string AllOutput = LogFileProcess.Output;
			File.WriteAllText(LogFilename, AllOutput);
			File.WriteAllText(ServerLogFilename, AllOutput);

			ExitCode = LogFileProcess.ExitCode;
		}
	}

	public AndroidPlatform()
		: base(UnrealTargetPlatform.Android)
	{

	}


	public override string[] GetCodeSpecifiedSdkVersions()
	{
		UEBuildPlatformSDK AndroidSDK = UEBuildPlatformSDK.GetSDKForPlatform("Android");

		return AndroidSDK != null ? new string[] { AndroidSDK.GetMainVersion() } : Array.Empty<string>();
	}

	public override bool GetSDKInstallCommand(out string Command, out string Params, ref bool bRequiresPrivilegeElevation, ref bool bCreateWindow, ITurnkeyContext TurnkeyContext)
	{
		// run the Setup.bat in the engine, not coming from a normal FileSource

		if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
		{
			Command = "$(EngineDir)/Extras/Android/SetupAndroid.bat";
		}
		else if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Mac)
		{
			Command = "$(EngineDir)/Extras/Android/SetupAndroid.command";
		}
		else
		{
			Command = "$(EngineDir)/Extras/Android/SetupAndroid.sh";
		}

		// pull the desired version numbers to install
		UEBuildPlatformSDK AndroidSDK = UEBuildPlatformSDK.GetSDKForPlatform("Android");
		string PlatformsVersion = AndroidSDK.GetPlatformSpecificVersion("platforms");
		string BuildToolsVersion = AndroidSDK.GetPlatformSpecificVersion("build-tools");
		string CMakeVersion = AndroidSDK.GetPlatformSpecificVersion("cmake");
		string NDKVersion = AndroidSDK.GetPlatformSpecificVersion("ndk");

		Params = $"{PlatformsVersion} {BuildToolsVersion} {CMakeVersion} {NDKVersion} -noninteractive";

		// because this may bring up a license acceptance message that needs the user to respond, so we make a new window
		bCreateWindow = true;

		return true;
	}



	private static string GetAndroidStudioExe()
	{
		if (OperatingSystem.IsLinux())
		{
			string UserHome = Environment.GetEnvironmentVariable("HOME");
			string AndroidStudioExe = Path.Combine(UserHome, "android-studio", "bin", "studio.sh");

			return AndroidStudioExe;
		}
		else if (OperatingSystem.IsMacOS())
		{

			string AndroidStudioExe = "/Applications/Android Studio.app";
			if (Directory.Exists(AndroidStudioExe))
			{
				return AndroidStudioExe;
			}

			string UserHome = Environment.GetEnvironmentVariable("HOME");
			AndroidStudioExe = Path.Combine(UserHome, "Applications", "Android Studio.app");

			return AndroidStudioExe;
		}

		Debug.Assert(OperatingSystem.IsWindows());

		string DefaultAndroidStudioInstallDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Android", "Android Studio");
		string RegValue = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Android Studio", "Path", null) as string;
		string AndroidStudioInstallDir = RegValue == null ? DefaultAndroidStudioInstallDir : RegValue;
		return Path.Combine(AndroidStudioInstallDir, "bin", "studio64.exe");
	}

	private static string GetSdkDir()
	{
		string AndroidHome = Environment.GetEnvironmentVariable("ANDROID_HOME");
		if (!string.IsNullOrEmpty(AndroidHome) && Directory.Exists(AndroidHome))
		{
			return AndroidHome;
		}

		if (OperatingSystem.IsLinux())
		{
			string UserHome = Environment.GetEnvironmentVariable("HOME");
			string AndroidSdkPath = Path.Combine(UserHome, "Android", "Sdk");

			return AndroidSdkPath;
		}
		else if (OperatingSystem.IsMacOS())
		{
			string BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bash_profile");
			if (!File.Exists(BashProfilePath))
			{
				// Try .bashrc if didn't fine .bash_profile
				BashProfilePath = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), ".bashrc");
			}
			if (File.Exists(BashProfilePath))
			{
				string[] BashProfileContents = File.ReadAllLines(BashProfilePath);

				// Walk backwards so we keep the last export setting instead of the first
				string SdkKey = "ANDROID_HOME";
				for (int LineIndex = BashProfileContents.Length - 1; LineIndex >= 0; --LineIndex)
				{
					if (BashProfileContents[LineIndex].StartsWith("export " + SdkKey + "="))
					{
						string PathVar = BashProfileContents[LineIndex].Split
('=')[1].Replace("\"", "");
Log.TraceInformation("ANDROID_HOME = {0}", PathVar);
						return PathVar;
					}

				}
			}

			return Environment.GetEnvironmentVariable("ANDROID_HOME");
		}

		Debug.Assert(OperatingSystem.IsWindows());

		string DefaultSdkDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Android", "Sdk");
		string RegValue = Microsoft.Win32.Registry.GetValue("HKEY_LOCAL_MACHINE\\SOFTWARE\\Android", "SdkPath", null) as string;
		return RegValue == null ? DefaultSdkDir : RegValue;
	}

	public override bool UpdateHostPrerequisites(BuildCommand Command, ITurnkeyContext TurnkeyContext, bool bVerifyOnly)
	{
		string AndroidStudioExe = GetAndroidStudioExe();
		string SdkDir = GetSdkDir();

		bool bIsMac = (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Mac);
		bool bHaveAndroidStudio = (bIsMac && Directory.Exists(AndroidStudioExe)) || 
					(!bIsMac && FileExists(AndroidStudioExe));

		bool bIsInstalled = bHaveAndroidStudio && Directory.Exists(SdkDir);

		// if we are only verifying, just return the status, and if it's installed, we are done!
		if (bVerifyOnly || bIsInstalled)
		{
			if (!bHaveAndroidStudio)
			{
				TurnkeyContext.ReportError("Android Studio is not installed correctly.");
			}
			if (!Directory.Exists(SdkDir))
			{
				TurnkeyContext.ReportError("Android SDK directory is not set correctly.");
			}
			return bIsInstalled;
		}

		if (!bHaveAndroidStudio)
		{
			// get AS installer
			string OutputPath = TurnkeyContext.RetrieveFileSource("AndroidStudio");

			// Unset some envvars in case autosdk ran - they will mess up the first run of Android Studio
			string[] Vars = new string[]
			{
					"ANDROID_HOME",
					"ANDROID_SDK_HOME",
					"JAVA_HOME",
					"NDKROOT",
					"NDK_ROOT",
					"ANDROID_NDK_ROOT",
					"ANDROID_SWT"
			};
			Array.ForEach(Vars, x => Environment.SetEnvironmentVariable(x, null));

			if (OutputPath == null)
			{
				TurnkeyContext.PauseForUser("Unable to find Android Studio installer. Please download and install Android Studio 4.0.2 from https://developer.android.com/studio/archive before continuing.\n\nMake sure to use the Run Android Studio and complete the first-time setup!\n\nFollow the steps to install command-line tools (latest):\nhttps://docs.unrealengine.com/5.0/en-US/how-to-setup-up-android-sdk-and-ndk-for-your-unreal-engine-development-environment");
			}
			else
			{
				if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Linux)
				{
					// TODO finish GUI support for Linux here, otherwise this will throw. Using zenity
					// TurnkeyContext.PauseForUser("Running the Android Studio installer, and then Android Studio for first-time setup!\n\nChoose all default options unless you know what you are doing.");

					string UserHome = Environment.GetEnvironmentVariable("HOME");
					string Args = string.Format("-xf {0} -C {1}", OutputPath, UserHome);

					int ExitCode = TurnkeyContext.RunExternalCommand("/bin/tar", Args, false, true, true);

					if (ExitCode != 0)
					{
						TurnkeyContext.ReportError($"Android Studio installer failed. ExitCode = {ExitCode}");
						return false;
					}
				}
				else if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Mac)
				{

					string UserHome = Environment.GetEnvironmentVariable("HOME");
					string SourceApp = Path.Combine(OutputPath, "Android Studio.app");

					int ExitCode = TurnkeyContext.RunExternalCommand("/usr/bin/hdiutil", "attach " + OutputPath, false, true, true);

					if (ExitCode != 0)
					{
						TurnkeyContext.ReportError($"Android Studio installer failed. ExitCode = {ExitCode}");
						return false;
					}

					string AndroidStudioVolume = "";
					foreach (string Volume in Directory.GetDirectories("/Volumes"))

					{
						if (Volume.Contains("Android Studio"))
						{
							AndroidStudioVolume = Volume;
							break;
						}
					}
					if (AndroidStudioVolume == "")
					{
						TurnkeyContext.ReportError($"Android Studio installer failed. DMG did not mount");
						return false;
					}

					string SourcePath = Path.Combine(AndroidStudioVolume, "Android Studio.app");
					string DestPath = Path.Combine(UserHome, "Applications") + "/";
					if (SourcePath.Contains(" "))
					{
						SourcePath = "\"" + SourcePath + "\"";
					}
					if (DestPath.Contains(" "))
					{
						DestPath = "\"" + DestPath + "\"";
					}

					ExitCode = TurnkeyContext.RunExternalCommand("/bin/cp", "-R " + SourcePath + " " + DestPath, false, true, true);

					if (AndroidStudioVolume.Contains(" "))
					{
						AndroidStudioVolume = "\"" + AndroidStudioVolume + "\"";
					}
					int ExitCode2 = TurnkeyContext.RunExternalCommand("/usr/bin/hdiutil", "detach " + AndroidStudioVolume, false, true, true);

					// give error for cp, but can ignore detach failure
					if (ExitCode != 0)
					{
						TurnkeyContext.ReportError($"Android Studio installer failed. ExitCode = {ExitCode}");
						return false;
					}
				}
				else if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
				{
					TurnkeyContext.PauseForUser("Running the Android Studio installer, and then Android Studio for first-time setup!\n\nFollow the steps to install command-line tools (latest):\nhttps://docs.unrealengine.com/5.0/en-US/how-to-setup-up-android-sdk-and-ndk-for-your-unreal-engine-development-environment");
					int ExitCode = TurnkeyContext.RunExternalCommand(OutputPath, "/S", false, true, true);
					//				Utils.RunLocalProcessAndReturnStdOut(OutputPath, "", out ExitCode, true);

					// AS installer returns 1223 even on success ("user canceled" even tho there's no UI to cancel it...) when running with /S
					if (ExitCode != 0 && ExitCode != 1223)
					{
						TurnkeyContext.ReportError($"Android Studio installer failed. ExitCode = {ExitCode}");
						return false;
					}
				}
				else
				{
					TurnkeyContext.ReportError($"Invalid host platform");
					return false;
				}

				// re-query for AS location
				TurnkeyContext.RunExternalCommand(GetAndroidStudioExe(), "", false, true, bIsMac);
			}
		}
		else
		{
			TurnkeyContext.PauseForUser("The Sdk directory was not found. Running the Android Studio to perform first-time setup.\n\nFollow the steps to install command-line tools (latest):\nhttps://docs.unrealengine.com/5.0/en-US/how-to-setup-up-android-sdk-and-ndk-for-your-unreal-engine-development-environment");

			TurnkeyContext.RunExternalCommand(AndroidStudioExe, "", false, true, bIsMac);
		}

		// check to see if the installation worked. If so, continue on!
		AndroidStudioExe = GetAndroidStudioExe();
		SdkDir = GetSdkDir();

		bHaveAndroidStudio = (bIsMac && Directory.Exists(AndroidStudioExe)) || 
					(!bIsMac && FileExists(AndroidStudioExe));

		bIsInstalled = bHaveAndroidStudio && Directory.Exists(SdkDir);

		if (!bHaveAndroidStudio)
		{
			TurnkeyContext.ReportError("Android Studio is not installed correctly, after attempted installation.");
		}
		if (!Directory.Exists(SdkDir))
		{
			TurnkeyContext.ReportError("Android SDK directory is not set correctly, after attempted installation.");
		}

		return bIsInstalled;
	}










	private static string GetSONameWithoutArchitecture(ProjectParams Params, string DecoratedExeName)
	{
		return Path.Combine(Path.GetDirectoryName(Params.GetProjectExeForPlatform(UnrealTargetPlatform.Android).ToString()), DecoratedExeName) + ".so";
	}

	private static string GetFinalApkName(ProjectParams Params, string DecoratedExeName, bool bRenameUnrealGame, string Architecture)
	{
		string ProjectDir = Path.Combine(Path.GetDirectoryName(Path.GetFullPath(Params.RawProjectPath.FullName)), "Binaries/Android");

		if (Params.Prebuilt)
		{
			ProjectDir = Path.Combine(Params.BaseStageDirectory, "Android");
		}

		// Apk's go to project location, not necessarily where the .so is (content only packages need to output to their directory)
		string ApkName = Path.Combine(ProjectDir, DecoratedExeName) + Architecture + ".apk";

		// if the source binary was UnrealGame, handle using it or switching to project name
		if (Path.GetFileNameWithoutExtension(Params.GetProjectExeForPlatform(UnrealTargetPlatform.Android).ToString()) == "UnrealGame")
		{
			if (bRenameUnrealGame)
			{
				// replace UnrealGame with project name (only replace in the filename part)
				ApkName = Path.Combine(Path.GetDirectoryName(ApkName), Path.GetFileName(ApkName).Replace("UnrealGame", Params.ShortProjectName));
			}
			else
			{
				// if we want to use UE directly then use it from the engine directory not project directory
				ApkName = ApkName.Replace(ProjectDir, Path.Combine(CmdEnv.LocalRoot, "Engine/Binaries/Android"));
			}
		}

		return ApkName;
	}

	private static bool bHaveReadEngineVersion = false;
	private static string EngineMajorVersion = "4";
	private static string EngineMinorVersion = "0";
	private static string EnginePatchVersion = "0";

	#pragma warning disable CS0414
	private static string EngineChangelist = "0";

	private static string ReadEngineVersion(string EngineDirectory)
	{
		if (!bHaveReadEngineVersion)
		{
			string EngineVersionFile = Path.Combine(EngineDirectory, "Source", "Runtime", "Launch", "Resources", "Version.h");
			string[] EngineVersionLines = File.ReadAllLines(EngineVersionFile);
			for (int i = 0; i < EngineVersionLines.Length; ++i)
			{
				if (EngineVersionLines[i].StartsWith("#define ENGINE_MAJOR_VERSION"))
				{
					EngineMajorVersion = EngineVersionLines[i].Split('\t')[1].Trim(' ');
				}
				else if (EngineVersionLines[i].StartsWith("#define ENGINE_MINOR_VERSION"))
				{
					EngineMinorVersion = EngineVersionLines[i].Split('\t')[1].Trim(' ');
				}
				else if (EngineVersionLines[i].StartsWith("#define ENGINE_PATCH_VERSION"))
				{
					EnginePatchVersion = EngineVersionLines[i].Split('\t')[1].Trim(' ');
				}
				else if (EngineVersionLines[i].StartsWith("#define BUILT_FROM_CHANGELIST"))
				{
					EngineChangelist = EngineVersionLines[i].Split(new char[] { ' ', '\t' })[2].Trim(' ');
				}
			}

			bHaveReadEngineVersion = true;
		}

		return EngineMajorVersion + "." + EngineMinorVersion + "." + EnginePatchVersion;
	}

	#pragma warning restore CS0414


	private static string GetFinalSymbolizedSODirectory(string ApkName, DeploymentContext SC, string Architecture)
	{
		string PackageVersion = GetPackageInfo(ApkName, SC, true);
		if (PackageVersion == null || PackageVersion.Length == 0)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package version from " + ApkName);
		}

		return SC.ShortProjectName + "_Symbols_v" + PackageVersion + "/" + SC.ShortProjectName + Architecture;
	}

	private static string GetFinalObbName(string ApkName, DeploymentContext SC, bool bUseAppType = true)
	{
		// calculate the name for the .obb file
		string PackageName = GetPackageInfo(ApkName, SC, false);
		if (PackageName == null)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package name from " + ApkName);
		}

		string PackageVersion = GetPackageInfo(ApkName, SC, true);
		if (PackageVersion == null || PackageVersion.Length == 0)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package version from " + ApkName);
		}

		if (PackageVersion.Length > 0)
		{
			int IntVersion = int.Parse(PackageVersion);
			PackageVersion = IntVersion.ToString("0");
		}

		string AppType = bUseAppType ? GetMetaAppType() : "";
		if (AppType.Length > 0)
		{
			AppType += ".";
		}

		string ObbName = string.Format("main.{0}.{1}.{2}obb", PackageVersion, PackageName, AppType);

		// plop the .obb right next to the executable
		ObbName = Path.Combine(Path.GetDirectoryName(ApkName), ObbName);

		return ObbName;
	}

	private static string GetFinalPatchName(string ApkName, DeploymentContext SC, bool bUseAppType = true)
	{
		// calculate the name for the .obb file
		string PackageName = GetPackageInfo(ApkName, SC, false);
		if (PackageName == null)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package name from " + ApkName);
		}

		string PackageVersion = GetPackageInfo(ApkName, SC, true);
		if (PackageVersion == null || PackageVersion.Length == 0)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package version from " + ApkName);
		}

		if (PackageVersion.Length > 0)
		{
			int IntVersion = int.Parse(PackageVersion);
			PackageVersion = IntVersion.ToString("0");
		}

		string AppType = bUseAppType ? GetMetaAppType() : "";
		if (AppType.Length > 0)
		{
			AppType += ".";
		}

		string PatchName = string.Format("patch.{0}.{1}.{2}obb", PackageVersion, PackageName, AppType);

		// plop the .obb right next to the executable
		PatchName = Path.Combine(Path.GetDirectoryName(ApkName), PatchName);

		return PatchName;
	}

	private static string GetFinalOverflowName(string ApkName, DeploymentContext SC, int Index, bool bUseAppType = true)
	{
		// calculate the name for the .obb file
		string PackageName = GetPackageInfo(ApkName, SC, false);
		if (PackageName == null)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package name from " + ApkName);
		}

		string PackageVersion = GetPackageInfo(ApkName, SC, true);
		if (PackageVersion == null || PackageVersion.Length == 0)
		{
			throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package version from " + ApkName);
		}

		if (PackageVersion.Length > 0)
		{
			int IntVersion = int.Parse(PackageVersion);
			PackageVersion = IntVersion.ToString("0");
		}

		string AppType = bUseAppType ? GetMetaAppType() : "";
		if (AppType.Length > 0)
		{
			AppType += ".";
		}

		string OverflowName = string.Format("overflow{0}.{1}.{2}.{3}obb", Index, PackageVersion, PackageName, AppType);

		// plop the .obb right next to the executable
		OverflowName = Path.Combine(Path.GetDirectoryName(ApkName), OverflowName);

		return OverflowName;
	}


	public override string GetPlatformPakCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		string PakParams = " -patchpaddingalign=0";

		string OodleDllPath = DirectoryReference.Combine(SC.ProjectRoot, "Binaries/ThirdParty/Oodle/Win64/UnrealPakPlugin.dll").FullName;
		if (File.Exists(OodleDllPath))
		{
			PakParams += String.Format(" -customcompressor=\"{0}\"", OodleDllPath);
		}

		return PakParams;
	}

	private static string GetDeviceObbName(string ApkName, DeploymentContext SC)
	{
        string ObbName = GetFinalObbName(ApkName, SC, false);
        string PackageName = GetPackageInfo(ApkName, SC, false);
        return TargetAndroidLocation + PackageName + "/" + Path.GetFileName(ObbName);
	}

	private static string GetDevicePatchName(string ApkName, DeploymentContext SC)
	{
		string PatchName = GetFinalPatchName(ApkName, SC, false);
		string PackageName = GetPackageInfo(ApkName, SC, false);
		return TargetAndroidLocation + PackageName + "/" + Path.GetFileName(PatchName);
	}

	private static string GetDeviceOverflowName(string ApkName, DeploymentContext SC, int Index)
	{
		string OverflowName = GetFinalOverflowName(ApkName, SC, Index, false);
		string PackageName = GetPackageInfo(ApkName, SC, false);
		return TargetAndroidLocation + PackageName + "/" + Path.GetFileName(OverflowName);
	}

	public static string GetStorageQueryCommand(bool bForcePC = false)
    {
		if (!bForcePC && !RuntimePlatform.IsWindows)
		{
			return "shell 'echo $EXTERNAL_STORAGE'";
		}
		else
		{
			return "shell \"echo $EXTERNAL_STORAGE\"";
		}
    }

	enum EBatchType
	{
		Install,
		Uninstall,
		Symbolize,
	};
	private static string GetFinalBatchName(string ApkName, DeploymentContext SC, string Architecture, bool bNoOBBInstall, EBatchType BatchType, UnrealTargetPlatform Target)
	{
		string Extension = ".bat";
		if (Target == UnrealTargetPlatform.Linux || Target == UnrealTargetPlatform.LinuxArm64)
		{
			Extension = ".sh";
		}
		else if (Target == UnrealTargetPlatform.Mac)
		{
			Extension = ".command";
		}

		// Get the name of the APK to use for batch file
		string ExecutableName = Path.GetFileNameWithoutExtension(ApkName);
		
		switch(BatchType)
		{
			case EBatchType.Install:
			case EBatchType.Uninstall:
				return Path.Combine(Path.GetDirectoryName(ApkName), (BatchType == EBatchType.Uninstall ? "Uninstall_" : "Install_") + ExecutableName + (!bNoOBBInstall ? "" : "_NoOBBInstall") + Extension);
			case EBatchType.Symbolize:
				return Path.Combine(Path.GetDirectoryName(ApkName), "SymbolizeCrashDump_" + ExecutableName + Extension);
		}
		return "";
	}

	private List<string> CollectPluginDataPaths(DeploymentContext SC)
	{
		// collect plugin extra data paths from target receipts
		List<string> PluginExtras = new List<string>();
		foreach (StageTarget Target in SC.StageTargets)
		{
			TargetReceipt Receipt = Target.Receipt;
			var Results = Receipt.AdditionalProperties.Where(x => x.Name == "AndroidPlugin");
			foreach (var Property in Results)
			{
				// Keep only unique paths
				string PluginPath = Property.Value;
				if (PluginExtras.FirstOrDefault(x => x == PluginPath) == null)
				{
					PluginExtras.Add(PluginPath);
					LogInformation("AndroidPlugin: {0}", PluginPath);
				}
			}
		}
		return PluginExtras;
	}

	private bool UsingAndroidFileServer(ProjectParams Params, DeploymentContext SC, out bool bEnablePlugin, out string AFSToken, out bool bIsShipping, out bool bIncludeInShipping, out bool bAllowExternalStartInShipping)
	{
		FileReference RawProjectPath = SC != null ? SC.RawProjectPath : Params.RawProjectPath;
		UnrealTargetPlatform TargetPlatform = SC != null ? SC.StageTargetPlatform.PlatformType : Params.ClientTargetPlatforms[0].Type;
		UnrealTargetConfiguration TargetConfiguration = SC != null ? SC.StageTargetConfigurations[0] : Params.ClientConfigsToBuild[0];
		bIsShipping = TargetConfiguration == UnrealTargetConfiguration.Shipping;

		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(RawProjectPath), TargetPlatform);
		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bEnablePlugin", out bEnablePlugin))
		{
			bEnablePlugin = true;
		}
		if (!Ini.GetString("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "SecurityToken", out AFSToken))
		{
			AFSToken = "";
		}
		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bIncludeInShipping", out bIncludeInShipping))
		{
			bIncludeInShipping = false;
		}
		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bAllowExternalStartInShipping", out bAllowExternalStartInShipping))
		{
			bAllowExternalStartInShipping = false;
		}

		if (bIsShipping && !(bIncludeInShipping && bAllowExternalStartInShipping))
		{
			return false;
		}
		return bEnablePlugin;
	}

	enum EConnectionType
	{
		USBOnly,
		NetworkOnly,
		Combined
	}

	private EConnectionType GetAndroidFileServerNetworkConfig(DeploymentContext SC, out bool bUseCompression, out bool bLogFiles, out bool bReportStats, out bool bUseManualIPAddress, out string ManualIPAddress)
	{
		EConnectionType ConnectionType = EConnectionType.USBOnly;

		UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);

		string ConnectionString = "";
		Ini.GetString("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "ConnectionType", out ConnectionString);
		switch (ConnectionString)
		{
			case "USBOnly":
				ConnectionType = EConnectionType.USBOnly;
				break;
			case "NetworkOnly":
				ConnectionType = EConnectionType.NetworkOnly;
				break;
			case "Combined":
				ConnectionType = EConnectionType.Combined;
				break;
			default:
				ConnectionType = EConnectionType.USBOnly;
				break;
		}

		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bUseCompression", out bUseCompression))
		{
			bUseCompression = false;
		}
		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bLogFiles", out bLogFiles))
		{
			bLogFiles = false;
		}
		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bReportStats", out bReportStats))
		{
			bReportStats = false;
		}
		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bUseManualIPAddress", out bUseManualIPAddress))
		{
			bUseManualIPAddress = false;
		}
		if (!Ini.GetString("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "ManualIPAddress", out ManualIPAddress))
		{
			ManualIPAddress = "127.0.0.1";
		}

		bool bAllowNetworkConnection = true;
		if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bAllowNetworkConnection", out bAllowNetworkConnection))
		{
			bAllowNetworkConnection = true;
		}
		if (!bAllowNetworkConnection && ConnectionType != EConnectionType.USBOnly)
		{
			Log.TraceWarning("AFS will only use USB connection due to network connection disabled");
			ConnectionType = EConnectionType.USBOnly;
		}

		return ConnectionType;
	}

	private bool BuildWithHiddenSymbolVisibility(DeploymentContext SC)
	{
		UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bBuild = false;
		return TargetConfiguration == UnrealTargetConfiguration.Shipping && (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildWithHiddenSymbolVisibility", out bBuild) && bBuild);
	}

	private bool GetSaveSymbols(DeploymentContext SC)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bSave = false;
		return (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSaveSymbols", out bSave) && bSave);
	}

	private bool GetEnableBundle(DeploymentContext SC)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bEnableBundle = false;
		return (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableBundle", out bEnableBundle) && bEnableBundle);
	}

	private bool GetEnableUniversalAPK(DeploymentContext SC)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bEnableUniversalAPK = false;
		return (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableUniversalAPK", out bEnableUniversalAPK) && bEnableUniversalAPK);
	}

	private Int64 GetMaxOBBSizeAllowed(DeploymentContext SC)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bForceSmallOBBFiles = false;
		bool bAllowLargeOBBFiles = false;
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bForceSmallOBBFiles", out bForceSmallOBBFiles);
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bAllowLargeOBBFiles", out bAllowLargeOBBFiles);
		return bForceSmallOBBFiles ? SmallOBBSizeAllowed : (bAllowLargeOBBFiles ? MaxOBBSizeAllowed : NormalOBBSizeAllowed);
	}

	private bool AllowPatchOBBFile(DeploymentContext SC)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bAllowPatchOBBFile = false;
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bAllowPatchOBBFile", out bAllowPatchOBBFile);
		return bAllowPatchOBBFile;
	}

	private bool AllowOverflowOBBFiles(DeploymentContext SC)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bAllowOverflowOBBFiles = false;
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bAllowOverflowOBBFiles", out bAllowOverflowOBBFiles);
		return bAllowOverflowOBBFiles;
	}

	private bool CreateOBBFile(DeploymentContext SC, string OutputFilename, List<FileReference> FilesForObb)
	{
		LogInformation("Creating {0} from {1}", OutputFilename, SC.StageDirectory);
		using (ZipFile ObbFile = new ZipFile(OutputFilename))
		{
			ObbFile.CompressionMethod = CompressionMethod.None;
			ObbFile.CompressionLevel = Ionic.Zlib.CompressionLevel.None;
			ObbFile.UseZip64WhenSaving = Ionic.Zip.Zip64Option.Never;
			ObbFile.Comment = String.Format("{0,10}", "1");

			int ObbFileCount = 0;
			ObbFile.AddProgress +=
				delegate (object sender, AddProgressEventArgs e)
				{
					if (e.EventType == ZipProgressEventType.Adding_AfterAddEntry)
					{
						ObbFileCount += 1;
						LogInformation("[{0}/{1}] Adding {2} to OBB",
							ObbFileCount, e.EntriesTotal,
							e.CurrentEntry.FileName);
					}
				};

			foreach (FileReference FileRef in FilesForObb)
			{
				string DestinationDirectoryPath = Path.GetRelativePath(SC.StageDirectory.FullName, Path.GetDirectoryName(FileRef.FullName));
				ObbFile.AddFile(FileRef.FullName, DestinationDirectoryPath);
			}

			// ObbFile.AddDirectory(SC.StageDirectory+"/"+SC.ShortProjectName, SC.ShortProjectName);
			try
			{
				ObbFile.Save();
			}
			catch (Exception)
			{
				return false;
			}
		}
		return true;
	}

	private bool UpdateObbStoreVersion(string Filename)
	{
		string Version = Path.GetFileNameWithoutExtension(Filename).Split('.')[1];

		using (ZipFile ObbFile = ZipFile.Read(Filename))
		{
			// Add the store version from the filename as a comment
			ObbFile.Comment = String.Format("{0,10}", Version);
			try
			{
				ObbFile.Save();
			}
			catch (Exception)
			{
				return false;
			}
		}
		return true;
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException(ExitCode.Error_OnlyOneTargetConfigurationSupported, "Android is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];

		IAndroidToolChain ToolChain = AndroidExports.CreateToolChain(Params.RawProjectPath);
		var Architectures = ToolChain.GetAllArchitectures();
		bool bMakeSeparateApks = UnrealBuildTool.AndroidExports.ShouldMakeSeparateApks();
		bool bBuildWithHiddenSymbolVisibility = BuildWithHiddenSymbolVisibility(SC);
		bool bSaveSymbols = GetSaveSymbols(SC);
		bool bEnableBundle = GetEnableBundle(SC);
		bool bEnableUniversalAPK = GetEnableUniversalAPK(SC);

		var Deploy = AndroidExports.CreateDeploymentHandler(Params.RawProjectPath, Params.ForcePackageData);
		bool bPackageDataInsideApk = Deploy.GetPackageDataInsideApk();

		bool bUseAFS = false;
		bool bUseAFSProject = false;

		bool bAFSEnablePlugin;
		string AFSToken;
		bool bIsShipping;
		bool bAFSIncludeInShipping;
		bool bAFSAllowExternalStartInShipping;
		UsingAndroidFileServer(Params, SC, out bAFSEnablePlugin, out AFSToken, out bIsShipping, out bAFSIncludeInShipping, out bAFSAllowExternalStartInShipping);

		if (bAFSEnablePlugin && !bPackageDataInsideApk)
		{
			bUseAFS = true;
			// AFSProject APK should be used if shipping and AFS wasn't included
			if (bIsShipping && !(bAFSIncludeInShipping && bAFSAllowExternalStartInShipping))
			{
				bUseAFSProject = true;
			}
		}

		string BaseApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, "");
		LogInformation("BaseApkName = {0}", BaseApkName);

		// Create main OBB with entire contents of staging dir. This
		// includes any PAK files, movie files, etc.

		string LocalObbName = SC.StageDirectory.FullName+".obb";
		string LocalPatchName = SC.StageDirectory.FullName + ".patch.obb";
		string LocalOverflow1Name = SC.StageDirectory.FullName + ".overflow1.obb";
		string LocalOverflow2Name = SC.StageDirectory.FullName + ".overflow2.obb";

		FileFilter ObbFileFilter = new FileFilter(FileFilterType.Include);
		ConfigHierarchy EngineIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.Android);
		List<string> ObbFilters;
		EngineIni.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ObbFilters", out ObbFilters);
		if (ObbFilters != null)
		{
			ObbFileFilter.AddRules(ObbFilters);
		}
		// Filter out dynamic libraries from obb
		ObbFileFilter.Exclude("*.so");

		List<FileReference> FilesForObb = new List<FileReference>();
		// Add staged Engine files
		{
			string EngineStageDirectoryPath = Path.Combine(SC.StageDirectory.FullName, "Engine");
			FilesForObb.AddRange(ObbFileFilter.ApplyToDirectory(new DirectoryReference(EngineStageDirectoryPath), true));
		}
		// Add staged project files
		{
			string ProjectStageDirectoryPath = Path.Combine(SC.StageDirectory.FullName, SC.ShortProjectName);
			FilesForObb.AddRange(ObbFileFilter.ApplyToDirectory(new DirectoryReference(ProjectStageDirectoryPath), true));
		}

		bool OBBNeedsUpdate = false;

		if (File.Exists(LocalObbName))
		{ 
			System.DateTime OBBTimeStamp = File.GetLastWriteTimeUtc(LocalObbName);
			foreach (FileReference FileToObb in FilesForObb)
			{
				System.DateTime FileTimeStamp = File.GetLastWriteTimeUtc(FileToObb.FullName);
				if (FileTimeStamp > OBBTimeStamp)
				{
					OBBNeedsUpdate = true;
					break;
				}
			}
		}
		else
		{
			OBBNeedsUpdate = true;
		}
		Int64 OBBSizeAllowed = GetMaxOBBSizeAllowed(SC);
		string LimitString = (OBBSizeAllowed < NormalOBBSizeAllowed) ? "1 GiB" : ((OBBSizeAllowed < MaxOBBSizeAllowed) ? "2 GiB" : "4 GiB");

		if (!OBBNeedsUpdate)
		{
			LogInformation("OBB is up to date: " + LocalObbName);
		}
		else
		{
			// Always delete the target OBB file if it exists
			if (File.Exists(LocalObbName))
			{
				File.Delete(LocalObbName);
			}

			// Always delete the target patch OBB file if it exists
			if (File.Exists(LocalPatchName))
			{
				File.Delete(LocalPatchName);
			}

			// Always delete the target overflow1 OBB file if it exists
			if (File.Exists(LocalOverflow1Name))
			{
				File.Delete(LocalOverflow1Name);
			}

			// Always delete the target overflow2 OBB file if it exists
			if (File.Exists(LocalOverflow2Name))
			{
				File.Delete(LocalOverflow2Name);
			}

			List<FileReference> FilesToObb = FilesForObb;
			List<FileReference> FilesToPatch = new List<FileReference>();
			List<FileReference> FilesToOverflow1 = new List<FileReference>();
			List<FileReference> FilesToOverflow2 = new List<FileReference>();

			if (AllowPatchOBBFile(SC))
			{
				bool bAllowOverflowOBBs = AllowOverflowOBBFiles(SC);

				FilesToObb = new List<FileReference>();

				// Collect the filesize and place into Obb or Patch list
				Int64 StagingDirLength = SC.StageDirectory.FullName.Length;
				Int64 MinimumObbSize = 22 + 10;		// EOCD with comment (store version)
				Int64 MainObbSize = MinimumObbSize;
				Int64 PatchObbSize = MinimumObbSize;
				Int64 Overflow1ObbSize = MinimumObbSize;
				Int64 Overflow2ObbSize = MinimumObbSize;
				foreach (FileReference FileRef in FilesForObb)
				{
					FileInfo LocalFileInfo = new FileInfo(FileRef.FullName);
					Int64 LocalFileLength = LocalFileInfo.Length;
					Int64 FilenameLength = FileRef.FullName.Length - StagingDirLength - 1;

					Int64 LocalOverhead = (30 + FilenameLength + 36);		// local file descriptor
					Int64 GlobalOverhead = (46 + FilenameLength + 36);		// central directory cost
					Int64 FileRequirements = LocalFileLength + LocalOverhead + GlobalOverhead;

					if (MainObbSize + FileRequirements < OBBSizeAllowed)
					{
						FilesToObb.Add(FileRef);
						MainObbSize += FileRequirements;
					}
					else if (PatchObbSize + FileRequirements < OBBSizeAllowed)
					{
						FilesToPatch.Add(FileRef);
						PatchObbSize += FileRequirements;
					}
					else if (bAllowOverflowOBBs)
					{
						if (Overflow1ObbSize + FileRequirements < OBBSizeAllowed)
						{
							FilesToOverflow1.Add(FileRef);
							Overflow1ObbSize += FileRequirements;
						}
						else if (Overflow2ObbSize + FileRequirements < OBBSizeAllowed)
						{
							FilesToOverflow2.Add(FileRef);
							Overflow2ObbSize += FileRequirements;
						}
						else
						{
							// no room in either file
							LogInformation("Failed to build OBB: " + LocalObbName);
							throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. Could not build OBB {0}. The file may be too big to fit in an OBB ({1} limit)", LocalObbName, LimitString);
						}
					}
					else
					{
						// no room in either file
						LogInformation("Failed to build OBB: " + LocalObbName);
						throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. Could not build OBB {0}. The file may be too big to fit in an OBB ({1} limit)", LocalObbName, LimitString);
					}
				}
			}

			// Now create the main OBB as a ZIP archive.
			if (!CreateOBBFile(SC, LocalObbName, FilesToObb))
			{
				LogInformation("Failed to build OBB: " + LocalObbName);
				throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. Could not build OBB {0}. The file may be too big to fit in an OBB ({1} limit)", LocalObbName, LimitString);
			}

			// Now create the patch OBB as a ZIP archive if required.
			if (FilesToPatch.Count() > 0)
			{
				if (!CreateOBBFile(SC, LocalPatchName, FilesToPatch))
				{
					LogInformation("Failed to build OBB: " + LocalPatchName);
					throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. Could not build OBB {0}. The file may be too big to fit in an OBB ({1} limit)", LocalPatchName, LimitString);
				}
			}

			// Now create the overflow1 OBB as a ZIP archive if required.
			if (FilesToOverflow1.Count() > 0)
			{
				if (!CreateOBBFile(SC, LocalOverflow1Name, FilesToOverflow1))
				{
					LogInformation("Failed to build OBB: " + LocalOverflow1Name);
					throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. Could not build OBB {0}. The file may be too big to fit in an OBB ({1} limit)", LocalPatchName, LimitString);
				}
			}

			// Now create the overflow2 OBB as a ZIP archive if required.
			if (FilesToOverflow2.Count() > 0)
			{
				if (!CreateOBBFile(SC, LocalOverflow2Name, FilesToOverflow2))
				{
					LogInformation("Failed to build OBB: " + LocalOverflow2Name);
					throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. Could not build OBB {0}. The file may be too big to fit in an OBB ({1} limit)", LocalPatchName, LimitString);
				}
			}
		}

		// make sure the OBB is <= 2GiB (or 4GiB if large OBB enabled)
		FileInfo OBBFileInfo = new FileInfo(LocalObbName);
		Int64 ObbFileLength = OBBFileInfo.Length;
		if (ObbFileLength > OBBSizeAllowed)
		{
			LogInformation("OBB exceeds " + LimitString + " limit: " + ObbFileLength + " bytes");
			throw new AutomationException(ExitCode.Error_AndroidOBBError, "Stage Failed. OBB {0} exceeds {1} limit)", LocalObbName, LimitString);
		}

		// collect plugin extra data paths from target receipts
		Deploy.SetAndroidPluginData(Architectures, CollectPluginDataPaths(SC));

		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.Android);
		int MinSDKVersion;
		Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
		int TargetSDKVersion = MinSDKVersion;
		Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "TargetSDKVersion", out TargetSDKVersion);
		LogInformation("Target SDK Version " + TargetSDKVersion);
		bool bDisablePerfHarden = false;
        if (TargetConfiguration != UnrealTargetConfiguration.Shipping)
        {
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableMaliPerfCounters", out bDisablePerfHarden);
		}

		foreach (string Architecture in Architectures)
		{
			string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, bMakeSeparateApks ? Architecture : "");
			string ApkBareName = GetFinalApkName(Params, SC.StageExecutables[0], true, "");
			bool bHaveAPK = !bEnableBundle;     // do not have a standard APK if bundle enabled
			if (!SC.IsCodeBasedProject)
			{
				string UnrealSOName = GetFinalApkName(Params, SC.StageExecutables[0], false, bMakeSeparateApks ? Architecture : "");
                UnrealSOName = UnrealSOName.Replace(".apk", ".so");
                if (FileExists_NoExceptions(UnrealSOName) == false)
				{
					LogInformation("Failed to find game .so " + UnrealSOName);
                    throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find .so {0}. You may need to build the UE project with your target configuration and platform.", UnrealSOName);
				}
			}
				
			TargetReceipt Receipt = SC.StageTargets[0].Receipt;
				              
			// when we make an embedded executable, all we do is output to libUnreal.so - we don't need to make an APK at all
			// however, we still let package go through to make the .obb file
			string CookFlavor = SC.FinalCookPlatform.IndexOf("_") > 0 ? SC.FinalCookPlatform.Substring(SC.FinalCookPlatform.IndexOf("_")) : "";
			if (!Params.Prebuilt)
			{
				string SOName = GetSONameWithoutArchitecture(Params, SC.StageExecutables[0]);
				bool bShouldCompileAsDll = Receipt.HasValueForAdditionalProperty("CompileAsDll", "true");
				if (bShouldCompileAsDll)
				{
					// MakeApk
					SOName = Receipt.BuildProducts[0].Path.FullName;

					// saving package info, which will allow 
					TargetType Type = TargetType.Game;
					if (CookFlavor.EndsWith("Client"))
					{
						Type = TargetType.Client;
					}
					else if (CookFlavor.EndsWith("Server"))
					{
						Type = TargetType.Server;
					}
					LogInformation("SavePackageInfo");
					Deploy.SavePackageInfo(Params.ShortProjectName, SC.ProjectRoot.FullName, Type, true);
				}
				Deploy.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, SOName, SC.LocalRoot + "/Engine", Params.Distribution, CookFlavor, SC.StageTargets[0].Receipt.Configuration, false, bShouldCompileAsDll);
			}

			// Create APK specific OBB in case we have a detached OBB.
			string DeviceObbName = "";
			string ObbName = "";
			string DevicePatchName = "";
			string PatchName = "";
			string DeviceOverflow1Name = "";
			string Overflow1Name = "";
			string DeviceOverflow2Name = "";
			string Overflow2Name = "";
			if (!bPackageDataInsideApk)
			{
				DeviceObbName = GetDeviceObbName(ApkName, SC);
				ObbName = GetFinalObbName(ApkName, SC);
				CopyFile(LocalObbName, ObbName);

				// apply store version to OBB to make it unique for PlayStore upload
				UpdateObbStoreVersion(ObbName);

				if (File.Exists(LocalPatchName))
				{
					DevicePatchName = GetDevicePatchName(ApkName, SC);
					PatchName = GetFinalPatchName(ApkName, SC);
					CopyFile(LocalPatchName, PatchName);

					// apply store version to OBB to make it unique for PlayStore upload
					UpdateObbStoreVersion(PatchName);
				}

				if (File.Exists(LocalOverflow1Name))
				{
					DeviceOverflow1Name = GetDeviceOverflowName(ApkName, SC, 1);
					Overflow1Name = GetFinalOverflowName(ApkName, SC, 1);
					CopyFile(LocalOverflow1Name, Overflow1Name);

					// apply store version to OBB to make it unique for PlayStore upload
					UpdateObbStoreVersion(Overflow1Name);
				}

				if (File.Exists(LocalOverflow2Name))
				{
					DeviceOverflow2Name = GetDeviceOverflowName(ApkName, SC, 2);
					Overflow2Name = GetFinalOverflowName(ApkName, SC, 2);
					CopyFile(LocalOverflow2Name, Overflow2Name);

					// apply store version to OBB to make it unique for PlayStore upload
					UpdateObbStoreVersion(Overflow2Name);
				}
			}

			// check for optional universal apk
			string APKDirectory = Path.GetDirectoryName(ApkName);
			string APKNameWithoutExtension = Path.GetFileNameWithoutExtension(ApkName);
			string APKBareNameWithoutExtension = Path.GetFileNameWithoutExtension(ApkBareName);
			string UniversalApkName = Path.Combine(APKDirectory, APKNameWithoutExtension + "_universal.apk");
			bool bHaveUniversal = false;
			if (bEnableBundle && bEnableUniversalAPK)
			{
				if (FileExists(UniversalApkName))
				{
					bHaveUniversal = true;
				}
				else
				{
					UniversalApkName = Path.Combine(APKDirectory, APKBareNameWithoutExtension + "_universal.apk");
					if (FileExists(UniversalApkName))
					{
						bHaveUniversal = true;
					}
				}
			}

			//figure out which platforms we need to create install files for
			bool bNeedsPCInstall = false;
			bool bNeedsMacInstall = false;
			bool bNeedsLinuxInstall = false;
			GetPlatformInstallOptions(SC, out bNeedsPCInstall, out bNeedsMacInstall, out bNeedsLinuxInstall);

			//helper delegate to prevent code duplication but allow us access to all the local variables we need
			var CreateInstallFilesAction = new Action<UnrealTargetPlatform>(Target =>
			{
				bool bIsPC = (Target == UnrealTargetPlatform.Win64);
				string LineEnding = bIsPC ? "\r\n" : "\n";
				// Write install batch file(s).
				string PackageName = GetPackageInfo(ApkName, SC, false);
				string BatchName = GetFinalBatchName(ApkName, SC, bMakeSeparateApks ? Architecture : "", false, EBatchType.Install, Target);
				string[] BatchLines = GenerateInstallBatchFile(bPackageDataInsideApk, PackageName, ApkName, Params, ObbName, DeviceObbName, false, PatchName, DevicePatchName, false, 
					Overflow1Name, DeviceOverflow1Name, false, Overflow2Name, DeviceOverflow2Name, false, bIsPC, Params.Distribution, TargetSDKVersion > 22, bDisablePerfHarden, bUseAFS, bUseAFSProject, AFSToken, Target);
				if (bHaveAPK)
				{
					// make a batch file that can be used to install the .apk and .obb files
					File.WriteAllText(BatchName, string.Join(LineEnding, BatchLines) + LineEnding);
				}
				// make a batch file that can be used to uninstall the .apk and .obb files
				string UninstallBatchName = GetFinalBatchName(ApkName, SC, bMakeSeparateApks ? Architecture : "", false, EBatchType.Uninstall, Target);
				BatchLines = GenerateUninstallBatchFile(bPackageDataInsideApk, PackageName, ApkName, Params, bIsPC);
				if (bHaveAPK || bHaveUniversal)
				{
					File.WriteAllText(UninstallBatchName, string.Join(LineEnding, BatchLines) + LineEnding);
				}

				string UniversalBatchName = "";
				if (bHaveUniversal)
				{
					UniversalBatchName = GetFinalBatchName(UniversalApkName, SC, "", false, EBatchType.Install, Target);
					// make a batch file that can be used to install the .apk
					string[] UniversalBatchLines = GenerateInstallBatchFile(bPackageDataInsideApk, PackageName, UniversalApkName, Params, ObbName, DeviceObbName, false, PatchName, DevicePatchName, false,
						Overflow1Name, DeviceOverflow1Name, false, Overflow2Name, DeviceOverflow2Name, false, bIsPC, Params.Distribution, TargetSDKVersion > 22, bDisablePerfHarden, bUseAFS, bUseAFSProject, AFSToken, Target);
					File.WriteAllText(UniversalBatchName, string.Join(LineEnding, UniversalBatchLines) + LineEnding);
				}

				string SymbolizeBatchName = GetFinalBatchName(ApkName, SC, Architecture, false, EBatchType.Symbolize, Target);
				if(bBuildWithHiddenSymbolVisibility || bSaveSymbols)
				{
					BatchLines = GenerateSymbolizeBatchFile(Params, PackageName, ApkName, SC, Architecture, bIsPC);
					File.WriteAllText(SymbolizeBatchName, string.Join(LineEnding, BatchLines) + LineEnding);
				}

				if (!RuntimePlatform.IsWindows)
				{
					if (bHaveAPK)
					{
						CommandUtils.FixUnixFilePermissions(BatchName);
					}
					if (bHaveAPK || bHaveUniversal)
					{
						CommandUtils.FixUnixFilePermissions(UninstallBatchName);
					}
					if (bHaveUniversal)
					{
						CommandUtils.FixUnixFilePermissions(UniversalBatchName);
					}
					if (bBuildWithHiddenSymbolVisibility || bSaveSymbols)
					{
						CommandUtils.FixUnixFilePermissions(SymbolizeBatchName);
					}
					//if(File.Exists(NoInstallBatchName)) 
					//{
					//    CommandUtils.FixUnixFilePermissions(NoInstallBatchName);
					//}
				}
			});

			if (bNeedsPCInstall)
			{
				CreateInstallFilesAction.Invoke(UnrealTargetPlatform.Win64);
			}
			if (bNeedsMacInstall)
			{
				CreateInstallFilesAction.Invoke(UnrealTargetPlatform.Mac);
			}
			if (bNeedsLinuxInstall)
			{
				CreateInstallFilesAction.Invoke(UnrealTargetPlatform.Linux);
			}

			// If we aren't packaging data in the APK then lets write out a bat file to also let us test without the OBB
			// on the device.
			//String NoInstallBatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", true, false);
			// if(!bPackageDataInsideApk)
			//{
			//    BatchLines = GenerateInstallBatchFile(bPackageDataInsideApk, PackageName, ApkName, Params, ObbName, DeviceObbName, true);
			//    File.WriteAllLines(NoInstallBatchName, BatchLines);
			//}
		}

		PrintRunTime();
	}

	string GetAFSExecutable(UnrealTargetPlatform Target)
	{
		if (Target == UnrealTargetPlatform.Win64)
		{
			return "win-x64/UnrealAndroidFileTool.exe";
		}
		if (Target == UnrealTargetPlatform.Mac)
		{
			return "osx-x64/UnrealAndroidFileTool";
		}
		if (Target == UnrealTargetPlatform.Linux)
		{
			return "linux-x64/UnrealAndroidFileTool";
		}
		Log.TraceWarning("GetAFSExecutable unsupported target, assuming Win64");
		return "win-x64/UnrealAndroidFileTool.exe";
	}

	private string[] GenerateInstallBatchFile(bool bPackageDataInsideApk, string PackageName, string ApkName, ProjectParams Params, string ObbName, string DeviceObbName, bool bNoObbInstall,
		string PatchName, string DevicePatchName, bool bNoPatchInstall, string Overflow1Name, string DeviceOverflow1Name, bool bNoOverflow1Install, string Overflow2Name, string DeviceOverflow2Name, bool bNoOverflow2Install,
		bool bIsPC, bool bIsDistribution, bool bRequireRuntimeStoragePermission, bool bDisablePerfHarden, bool bUseAFS, bool bUseAFSProject, string AFSToken, UnrealTargetPlatform Target)
    {
        string[] BatchLines = null;
        string ReadPermissionGrantCommand = "shell pm grant " + PackageName + " android.permission.READ_EXTERNAL_STORAGE";
        string WritePermissionGrantCommand = "shell pm grant " + PackageName + " android.permission.WRITE_EXTERNAL_STORAGE";
		string DisablePerfHardenCommand = "shell setprop security.perf_harden 0";

		// We don't grant runtime permission for distribution build on purpose since we will push the obb file to the folder that doesn't require runtime storage permission.
		// This way developer can catch permission issue if they try to save/load game file in folder that requires runtime storage permission.
		bool bNeedGrantStoragePermission = bRequireRuntimeStoragePermission && !bIsDistribution;

		// We can't always push directly to Android/obb so uploads to Download then moves it
		bool bDontMoveOBB = bUseAFS ? true : (bPackageDataInsideApk || !bIsDistribution);

		bool bHavePatch = (PatchName != "");
		bool bHaveOverflow1 = (Overflow1Name != "");
		bool bHaveOverflow2 = (Overflow2Name != "");

		string AFSExecutable = GetAFSExecutable(Target);
		string AFSCommonArg = "-p " + PackageName;
		if (AFSToken != "")
		{
			AFSCommonArg += " -k " + AFSToken;
		}

		if (!bIsPC)
        {
			string APKInstallCommand = "$ADB $DEVICE install " + Path.GetFileName(ApkName);
			string APKReinstallCommand = "";

			// If it is a distribution build, push to $STORAGE/Android/obb folder instead of $STORAGE/obb folder.
			// Note that $STORAGE/Android/obb will be the folder that contains the obb if you download the app from playstore.
			string OBBInstallCommand = bNoObbInstall ? "\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + DeviceObbName + "'" : "\t$ADB $DEVICE push " + Path.GetFileName(ObbName) + (bIsDistribution ? " " + TargetAndroidTemp : " $STORAGE/") + DeviceObbName;
			string PatchInstallCommand = bNoPatchInstall ? "\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + DevicePatchName + "'" : "\t$ADB $DEVICE push " + Path.GetFileName(PatchName) + (bIsDistribution ? " " + TargetAndroidTemp : " $STORAGE/") + DevicePatchName;
			string Overflow1InstallCommand = bNoOverflow1Install ? "\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + DeviceOverflow1Name + "'" : "\t$ADB $DEVICE push " + Path.GetFileName(Overflow1Name) + (bIsDistribution ? " " + TargetAndroidTemp : " $STORAGE/") + DeviceOverflow1Name;
			string Overflow2InstallCommand = bNoOverflow2Install ? "\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + DeviceOverflow2Name + "'" : "\t$ADB $DEVICE push " + Path.GetFileName(Overflow2Name) + (bIsDistribution ? " " + TargetAndroidTemp : " $STORAGE/") + DeviceOverflow2Name;

			if (bUseAFS)
			{
				if (bUseAFSProject)
				{
					APKInstallCommand = "$ADB $DEVICE install AFS_" + Path.GetFileName(ApkName);
					APKReinstallCommand = "$ADB $DEVICE install -r " + Path.GetFileName(ApkName);
				}
				else
				{
					// stop the fileserver (not needed on reinstall above
					APKReinstallCommand = "$AFS $DEVICE " + AFSCommonArg + " stop-all";
				}
				string AFSCommand = "\t$AFS $DEVICE " + AFSCommonArg; 
				OBBInstallCommand = bNoObbInstall ? AFSCommand + " deletefile '^mainobb'" : AFSCommand + " push " + Path.GetFileName(ObbName) + " '^mainobb'";
				PatchInstallCommand = bNoPatchInstall ? AFSCommand + " deletefile '^patchobb'" : AFSCommand + " push " + Path.GetFileName(PatchName) + " '^patchobb'";
				Overflow1InstallCommand = bNoOverflow1Install ? AFSCommand + " deletefile '^overflow1obb'" : AFSCommand + " push " + Path.GetFileName(Overflow1Name) + " '^overflow1obb'";
				Overflow2InstallCommand = bNoOverflow2Install ? AFSCommand + " deletefile '^overflow2obb'" : AFSCommand + " push " + Path.GetFileName(Overflow2Name) + " '^overflow2obb'";
			}

			LogInformation("Writing shell script for install with {0}", bPackageDataInsideApk ? "data in APK" : "separate obb");
            BatchLines = new string[] {
						"#!/bin/sh",
						"cd \"`dirname \"$0\"`\"",
						"AFS=./" + AFSExecutable,
						"ADB=",
						"if [ \"$ANDROID_HOME\" != \"\" ]; then ADB=$ANDROID_HOME/platform-tools/adb; else ADB=" +Environment.GetEnvironmentVariable("ANDROID_HOME") + "/platform-tools/adb; fi",
						"DEVICE=",
						"if [ \"$1\" != \"\" ]; then DEVICE=\"-s $1\"; fi",
						"echo",
						"echo Uninstalling existing application. Failures here can almost always be ignored.",
						"$ADB $DEVICE uninstall " + PackageName,
						"echo",
						"echo Installing existing application. Failures here indicate a problem with the device \\(connection or storage permissions\\) and are fatal.",
						APKInstallCommand,
						"if [ $? -eq 0 ]; then",
                        "\techo",
						"\t$ADB $DEVICE shell pm list packages " + PackageName,
						bNeedGrantStoragePermission ? "\techo Grant READ_EXTERNAL_STORAGE and WRITE_EXTERNAL_STORAGE to the apk for reading OBB or game file in external storage." : "",
						bNeedGrantStoragePermission ? "\t$ADB $DEVICE " + ReadPermissionGrantCommand : "",
						bNeedGrantStoragePermission ? "\t$ADB $DEVICE " + WritePermissionGrantCommand : "",
						bDisablePerfHarden ? "\t$ADB $DEVICE " + DisablePerfHardenCommand : "",
                        "\techo",
						"\techo Removing old data. Failures here are usually fine - indicating the files were not on the device.",
                        "\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UnrealGame/" + Params.ShortProjectName + "'",
						"\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UnrealGame/UECommandLine.txt" + "'",
						"\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + TargetAndroidLocation + PackageName + "'",
						"\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/Android/" + TargetAndroidLocation + PackageName + "'",
						"\t$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/Download/" + TargetAndroidLocation + PackageName + "'",
						bPackageDataInsideApk ? "" : "\techo",
						bPackageDataInsideApk ? "" : "\techo Installing new data. Failures here indicate storage problems \\(missing SD card or bad permissions\\) and are fatal.",
						bPackageDataInsideApk ? "" : "\tSTORAGE=$(echo \"`$ADB $DEVICE shell 'echo $EXTERNAL_STORAGE'`\" | cat -v | tr -d '^M')",
						bPackageDataInsideApk ? "" : OBBInstallCommand,
						bPackageDataInsideApk ? "if [ 1 ]; then" : "\tif [ $? -eq 0 ]; then",
						!bHavePatch ? "" : (bPackageDataInsideApk ? "" : PatchInstallCommand),
						!bHaveOverflow1 ? "" : (bPackageDataInsideApk ? "" : Overflow1InstallCommand),
						!bHaveOverflow2 ? "" : (bPackageDataInsideApk ? "" : Overflow2InstallCommand),
						bDontMoveOBB ? "" : "\t\t$ADB $DEVICE shell mkdir $STORAGE/Android/" + TargetAndroidLocation + PackageName, // don't check for error since installing may create the obb directory
						bDontMoveOBB ? "" : "\t\t$ADB $DEVICE shell mv " + TargetAndroidTemp + TargetAndroidLocation + PackageName + " $STORAGE/Android/" + TargetAndroidLocation,
						bDontMoveOBB ? "" : "\t\t$ADB $DEVICE shell rm -r " + TargetAndroidTemp + TargetAndroidLocation,
						APKReinstallCommand,
						"\t\techo",
						"\t\techo Installation successful",
						"\t\texit 0",
						"\tfi",
						"fi",
						"echo",
						"echo There was an error installing the game or the obb file. Look above for more info.",
						"echo",
						"echo Things to try:",
						"echo 'Check that the device (and only the device) is listed with \\\"$ADB devices\\\" from a command prompt.'",
						"echo Make sure all Developer options look normal on the device",
						"echo Check that the device has an SD card.",
						"exit 1"
					};
        }
        else
        {
			string APKInstallCommand = "%ADB% %DEVICE% install " + Path.GetFileName(ApkName);
			string APKReinstallCommand = "";

			string OBBInstallCommand = bNoObbInstall ? "%ADB% %DEVICE% shell rm -r %STORAGE%/" + DeviceObbName : "%ADB% %DEVICE% push " + Path.GetFileName(ObbName) + (bIsDistribution ? " " + TargetAndroidTemp : " %STORAGE%/") + DeviceObbName;
			string PatchInstallCommand = bNoPatchInstall ? "%ADB% %DEVICE% shell rm -r %STORAGE%/" + DevicePatchName : "%ADB% %DEVICE% push " + Path.GetFileName(PatchName) + (bIsDistribution ? " " + TargetAndroidTemp : " %STORAGE%/") + DevicePatchName;
			string Overflow1InstallCommand = bNoOverflow1Install ? "%ADB% %DEVICE% shell rm -r %STORAGE%/" + DeviceOverflow1Name : "%ADB% %DEVICE% push " + Path.GetFileName(Overflow1Name) + (bIsDistribution ? " " + TargetAndroidTemp : " %STORAGE%/") + DeviceOverflow1Name;
			string Overflow2InstallCommand = bNoOverflow2Install ? "%ADB% %DEVICE% shell rm -r %STORAGE%/" + DeviceOverflow2Name : "%ADB% %DEVICE% push " + Path.GetFileName(Overflow2Name) + (bIsDistribution ? " " + TargetAndroidTemp : " %STORAGE%/") + DeviceOverflow2Name;

			if (bUseAFS)
			{
				if (bUseAFSProject)
				{
					APKInstallCommand = "%ADB% %DEVICE% install AFS_" + Path.GetFileName(ApkName);
					APKReinstallCommand = "%ADB% %DEVICE% install -r " + Path.GetFileName(ApkName);
				}
				else
				{
					// stop the fileserver (not needed on reinstall above
					APKReinstallCommand = "%AFS% %DEVICE% " + AFSCommonArg + " stop-all";
				}
				string AFSCommand = "\t%AFS% %DEVICE% " + AFSCommonArg;
				OBBInstallCommand = bNoObbInstall ? AFSCommand + " deletefile \"^mainobb\"" : AFSCommand + " push " + Path.GetFileName(ObbName) + " \"^mainobb\"";
				PatchInstallCommand = bNoPatchInstall ? AFSCommand + " deletefile \"^patchobb\"" : AFSCommand + " push " + Path.GetFileName(PatchName) + " \"^patchobb\"";
				Overflow1InstallCommand = bNoOverflow1Install ? AFSCommand + " deletefile \"^overflow1obb\"" : AFSCommand + " push " + Path.GetFileName(Overflow1Name) + " \"^overflow1obb\"";
				Overflow2InstallCommand = bNoOverflow2Install ? AFSCommand + " deletefile \"^overflow2obb\"" : AFSCommand + " push " + Path.GetFileName(Overflow2Name) + " \"^overflow2obb\"";
			}

			LogInformation("Writing bat for install with {0}", bPackageDataInsideApk ? "data in APK" : "separate OBB");
            BatchLines = new string[] {
						"setlocal",
						"if NOT \"%UE_SDKS_ROOT%\"==\"\" (call %UE_SDKS_ROOT%\\HostWin64\\Android\\SetupEnvironmentVars.bat)",
						"set ANDROIDHOME=%ANDROID_HOME%",		
						"if \"%ANDROIDHOME%\"==\"\" set ANDROIDHOME="+Environment.GetEnvironmentVariable("ANDROID_HOME"),
						"set ADB=%ANDROIDHOME%\\platform-tools\\adb.exe",
						"set AFS=.\\" + AFSExecutable.Replace("/", "\\"),
						"set DEVICE=",
                        "if not \"%1\"==\"\" set DEVICE=-s %1",
                        "for /f \"delims=\" %%A in ('%ADB% %DEVICE% " + GetStorageQueryCommand(true) +"') do @set STORAGE=%%A",
						"@echo.",
						"@echo Uninstalling existing application. Failures here can almost always be ignored.",
						"%ADB% %DEVICE% uninstall " + PackageName,
						"@echo.",
						"@echo Installing existing application. Failures here indicate a problem with the device (connection or storage permissions) and are fatal.",
						APKInstallCommand,
						"@if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
						"%ADB% %DEVICE% shell pm list packages " + PackageName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/" + Params.ShortProjectName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/UECommandLine.txt", // we need to delete the commandline in UnrealGame or it will mess up loading
						"%ADB% %DEVICE% shell rm -r %STORAGE%/" + TargetAndroidLocation + PackageName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/Android/" + TargetAndroidLocation + PackageName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/Download/" + TargetAndroidLocation + PackageName,
						bPackageDataInsideApk ? "" : "@echo.",
						bPackageDataInsideApk ? "" : "@echo Installing new data. Failures here indicate storage problems (missing SD card or bad permissions) and are fatal.",
						bPackageDataInsideApk ? "" : OBBInstallCommand,
						bPackageDataInsideApk ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
						!bHavePatch ? "" : (bPackageDataInsideApk ? "" : PatchInstallCommand),
						!bHavePatch ? "" : (bPackageDataInsideApk ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error"),
						!bHaveOverflow1 ? "" : (bPackageDataInsideApk ? "" : Overflow1InstallCommand),
						!bHaveOverflow1 ? "" : (bPackageDataInsideApk ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error"),
						!bHaveOverflow2 ? "" : (bPackageDataInsideApk ? "" : Overflow2InstallCommand),
						!bHaveOverflow2 ? "" : (bPackageDataInsideApk ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error"),
						bDontMoveOBB ? "" : "%ADB% %DEVICE% shell mkdir %STORAGE%/Android/" + TargetAndroidLocation + PackageName, // don't check for error since installing may create the obb directory
						bDontMoveOBB ? "" : "%ADB% %DEVICE% shell mv " + TargetAndroidTemp + TargetAndroidLocation + PackageName + " %STORAGE%/Android/" + TargetAndroidLocation,
						bDontMoveOBB ? "" : "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error",
						bDontMoveOBB ? "" : "%ADB% %DEVICE% shell rm -r " + TargetAndroidTemp + TargetAndroidLocation,
						APKReinstallCommand,
						bUseAFSProject ? "if \"%ERRORLEVEL%\" NEQ \"0\" goto Error" : "",
						"@echo.",
						bNeedGrantStoragePermission ? "@echo Grant READ_EXTERNAL_STORAGE and WRITE_EXTERNAL_STORAGE to the apk for reading OBB file or game file in external storage." : "",
						bNeedGrantStoragePermission ? "%ADB% %DEVICE% " + ReadPermissionGrantCommand : "",
						bNeedGrantStoragePermission ? "%ADB% %DEVICE% " + WritePermissionGrantCommand : "",
						bDisablePerfHarden ? "%ADB% %DEVICE% " + DisablePerfHardenCommand : "",
                        "@echo.",
                        "@echo Installation successful",
						"goto:eof",
						":Error",
						"@echo.",
						"@echo There was an error installing the game or the obb file. Look above for more info.",
						"@echo.",
						"@echo Things to try:",
						"@echo Check that the device (and only the device) is listed with \"%ADB$ devices\" from a command prompt.",
						"@echo Make sure all Developer options look normal on the device",
						"@echo Check that the device has an SD card.",
						"@pause"
					};
        }
        return BatchLines;
    }

	private string[] GenerateUninstallBatchFile(bool bPackageDataInsideApk, string PackageName, string ApkName, ProjectParams Params, bool bIsPC)
	{
		string[] BatchLines = null;

		if (!bIsPC)
		{
			LogInformation("Writing shell script for uninstall with {0}", bPackageDataInsideApk ? "data in APK" : "separate obb");
			BatchLines = new string[] {
						"#!/bin/sh",
						"cd \"`dirname \"$0\"`\"",
						"ADB=",
						"if [ \"$ANDROID_HOME\" != \"\" ]; then ADB=$ANDROID_HOME/platform-tools/adb; else ADB=" +Environment.GetEnvironmentVariable("ANDROID_HOME") + "/platform-tools/adb; fi",
						"DEVICE=",
						"if [ \"$1\" != \"\" ]; then DEVICE=\"-s $1\"; fi",
						"echo",
						"echo Uninstalling existing application. Failures here can almost always be ignored.",
						"$ADB $DEVICE uninstall " + PackageName,
						"echo",
						"echo Removing old data. Failures here are usually fine - indicating the files were not on the device.",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UnrealGame/" + Params.ShortProjectName + "'",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/UnrealGame/UECommandLine.txt" + "'",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/" + TargetAndroidLocation + PackageName + "'",
						"$ADB $DEVICE shell 'rm -r $EXTERNAL_STORAGE/Android/" + TargetAndroidLocation + PackageName + "'",
						"echo",
						"echo Uninstall completed",
						"exit 0",
					};
		}
		else
		{
			LogInformation("Writing bat for uninstall with {0}", bPackageDataInsideApk ? "data in APK" : "separate OBB");
			BatchLines = new string[] {
						"setlocal",
						"if NOT \"%UE_SDKS_ROOT%\"==\"\" (call %UE_SDKS_ROOT%\\HostWin64\\Android\\SetupEnvironmentVars.bat)",
						"set ANDROIDHOME=%ANDROID_HOME%",
						"if \"%ANDROIDHOME%\"==\"\" set ANDROIDHOME="+Environment.GetEnvironmentVariable("ANDROID_HOME"),
						"set ADB=%ANDROIDHOME%\\platform-tools\\adb.exe",
						"set DEVICE=",
						"if not \"%1\"==\"\" set DEVICE=-s %1",
						"for /f \"delims=\" %%A in ('%ADB% %DEVICE% " + GetStorageQueryCommand(true) +"') do @set STORAGE=%%A",
						"@echo.",
						"@echo Uninstalling existing application. Failures here can almost always be ignored.",
						"%ADB% %DEVICE% uninstall " + PackageName,
						"@echo.",
						"echo Removing old data. Failures here are usually fine - indicating the files were not on the device.",
						"%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/" + Params.ShortProjectName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/UnrealGame/UECommandLine.txt", // we need to delete the commandline in UnrealGame or it will mess up loading
						"%ADB% %DEVICE% shell rm -r %STORAGE%/" + TargetAndroidLocation + PackageName,
						"%ADB% %DEVICE% shell rm -r %STORAGE%/Android/" + TargetAndroidLocation + PackageName,
						"@echo.",
						"@echo Uninstall completed",
					};
		}
		return BatchLines;
	}

	private string[] GenerateSymbolizeBatchFile(ProjectParams Params, string PackageName, string ApkName, DeploymentContext SC, string Architecture, bool bIsPC)
	{
		string[] BatchLines = null;

		if (!bIsPC)
		{
			LogInformation("Writing shell script for symbolize with {0}", "data in APK" );
			BatchLines = new string[] {
				"#!/bin/sh",
				"if [ $? -ne 0]; then",
				 "echo \"Required argument missing, pass a dump of adb crash log.\"",
				 "exit 1",
				"fi",
				"cd \"`dirname \"$0\"`\"",
				"NDKSTACK=",
				"if [ \"$ANDROID_NDK_ROOT\" != \"\" ]; then NDKSTACK=$%ANDROID_NDK_ROOT/ndk-stack; else ADB=" + Environment.GetEnvironmentVariable("ANDROID_NDK_ROOT") + "/ndk-stack; fi",
				"$NDKSTACK -sym " + GetFinalSymbolizedSODirectory(ApkName, SC, Architecture) + " -dump \"%1\" > " + Params.ShortProjectName + "_SymbolizedCallStackOutput.txt",
				"exit 0",
				};
		}
		else
		{
			LogInformation("Writing bat for symbolize");
			BatchLines = new string[] {
						"@echo off",
						"IF %1.==. GOTO NoArgs",
						"setlocal",
						"set NDK_ROOT=%ANDROID_NDK_ROOT%",
						"if \"%ANDROID_NDK_ROOT%\"==\"\" set NDK_ROOT=\""+Environment.GetEnvironmentVariable("ANDROID_NDK_ROOT")+"\"",
						"set NDKSTACK=%NDK_ROOT%\\ndk-stack.cmd",
						"",
						"%NDKSTACK% -sym "+GetFinalSymbolizedSODirectory(ApkName, SC, Architecture)+" -dump \"%1\" > "+ Params.ShortProjectName+"_SymbolizedCallStackOutput.txt",
						"",
						"goto:eof",
						"",
						"",
						":NoArgs",
						"echo.",
						"echo Required argument missing, pass a dump of adb crash log. (SymboliseCallStackDump C:\\adbcrashlog.txt)",
						"pause"
					};
		}
		return BatchLines;
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException(ExitCode.Error_OnlyOneTargetConfigurationSupported, "Android is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];
		IAndroidToolChain ToolChain = AndroidExports.CreateToolChain(Params.RawProjectPath);
		var Architectures = ToolChain.GetAllArchitectures();
		bool bMakeSeparateApks = UnrealBuildTool.AndroidExports.ShouldMakeSeparateApks();
		bool bPackageDataInsideApk = UnrealBuildTool.AndroidExports.CreateDeploymentHandler(Params.RawProjectPath, Params.ForcePackageData).GetPackageDataInsideApk();

		bool bAFSEnablePlugin;
		string AFSToken;
		bool bIsShipping;
		bool bAFSIncludeInShipping;
		bool bAFSAllowExternalStartInShipping;
		UsingAndroidFileServer(Params, SC, out bAFSEnablePlugin, out AFSToken, out bIsShipping, out bAFSIncludeInShipping, out bAFSAllowExternalStartInShipping);
		bool bUseAFS = bAFSEnablePlugin && !bPackageDataInsideApk;

		List<string> AddedObbFiles = new List<string>();
		foreach (string Architecture in Architectures)
		{
			string ApkBareName = GetFinalApkName(Params, SC.StageExecutables[0], true, "");
			string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, bMakeSeparateApks ? Architecture : "");
			bool bHaveAPK = FileExists(ApkName);
			string ObbName = GetFinalObbName(ApkName, SC);
			string PatchName = GetFinalPatchName(ApkName, SC);
			string Overflow1Name = GetFinalOverflowName(ApkName, SC, 1);
			string Overflow2Name = GetFinalOverflowName(ApkName, SC, 2);
			bool bBuildWithHiddenSymbolVisibility = BuildWithHiddenSymbolVisibility(SC);
			bool bSaveSymbols = GetSaveSymbols(SC);
			//string NoOBBBatchName = GetFinalBatchName(ApkName, Params, bMakeSeparateApks ? Architecture : "", bMakeSeparateApks ? GPUArchitecture : "", true, false);

			string APKDirectory = Path.GetDirectoryName(ApkName);
			string APKNameWithoutExtension = Path.GetFileNameWithoutExtension(ApkName);
			string APKBareNameWithoutExtension = Path.GetFileNameWithoutExtension(ApkBareName);

			bool bHaveAAB = false;
			bool bHaveUniversal = false;

			// copy optional app bundle if exists
			string AppBundleName = Path.Combine(APKDirectory, APKNameWithoutExtension + ".aab");
			if (FileExists(AppBundleName))
			{
				bHaveAAB = true;
				SC.ArchiveFiles(APKDirectory, Path.GetFileName(AppBundleName));
			}
			else
			{
				AppBundleName = Path.Combine(APKDirectory, APKBareNameWithoutExtension + ".aab");
				if (FileExists(AppBundleName))
				{
					bHaveAAB = true;
					SC.ArchiveFiles(APKDirectory, Path.GetFileName(AppBundleName));
				}
			}

			// copy optional apks (zip of split apks) if exists
			string APKSName = Path.Combine(APKDirectory, APKNameWithoutExtension + ".apks");
			if (FileExists(APKSName))
			{
				SC.ArchiveFiles(APKDirectory, Path.GetFileName(APKSName));
			}
			else
			{
				APKSName = Path.Combine(APKDirectory, APKBareNameWithoutExtension + ".apks");
				if (FileExists(APKSName))
				{
					SC.ArchiveFiles(APKDirectory, Path.GetFileName(APKSName));
				}
			}

			// copy optional universal apk if exists
			string UniversalApkName = Path.Combine(APKDirectory, APKNameWithoutExtension + "_universal.apk");
			if (FileExists(UniversalApkName))
			{
				bHaveUniversal = true;
				SC.ArchiveFiles(APKDirectory, Path.GetFileName(UniversalApkName));
			}
			else
			{
				UniversalApkName = Path.Combine(APKDirectory, APKBareNameWithoutExtension + "_universal.apk");
				if (FileExists(UniversalApkName))
				{
					bHaveUniversal = true;
					SC.ArchiveFiles(APKDirectory, Path.GetFileName(UniversalApkName));
				}
			}

			// copy optional AFSProject apk if exists
			string AFSApkName = Path.Combine(APKDirectory, "AFS_" + APKNameWithoutExtension + ".apk");
			if (FileExists(AFSApkName))
			{
				SC.ArchiveFiles(APKDirectory, Path.GetFileName(AFSApkName));
			}

			// verify the files exist
			if (!FileExists(ApkName))
			{
				// still valid if we found an AAB
				if (!bHaveAAB)
				{
					throw new AutomationException(ExitCode.Error_AppNotFound, "ARCHIVE FAILED - {0} was not found", ApkName);
				}
			}
			else
			{
				SC.ArchiveFiles(Path.GetDirectoryName(ApkName), Path.GetFileName(ApkName));
			}

			if (!bPackageDataInsideApk && !FileExists(ObbName))
			{
                throw new AutomationException(ExitCode.Error_ObbNotFound, "ARCHIVE FAILED - {0} was not found", ObbName);
			}

			if (bBuildWithHiddenSymbolVisibility || bSaveSymbols)
			{
				string SymbolizedSODirectory = GetFinalSymbolizedSODirectory(ApkName, SC, Architecture);
				string SymbolizedSOPath = Path.Combine(Path.Combine(Path.GetDirectoryName(ApkName), SymbolizedSODirectory), "libUnreal.so");
				if (!FileExists(SymbolizedSOPath))
				{
					throw new AutomationException(ExitCode.Error_SymbolizedSONotFound, "ARCHIVE FAILED - {0} was not found", SymbolizedSOPath);
				}

				// Add symbolized .so directory
				SC.ArchiveFiles(Path.GetDirectoryName(SymbolizedSOPath), Path.GetFileName(SymbolizedSOPath), true, null, SymbolizedSODirectory);
			}

			if (!bPackageDataInsideApk)
			{
				// only add if not already in archive list
				if (!AddedObbFiles.Contains(ObbName))
				{
					AddedObbFiles.Add(ObbName);

					SC.ArchiveFiles(Path.GetDirectoryName(ObbName), Path.GetFileName(ObbName));
					if (FileExists(PatchName))
					{
						SC.ArchiveFiles(Path.GetDirectoryName(PatchName), Path.GetFileName(PatchName));
					}
					if (FileExists(Overflow1Name))
					{
						SC.ArchiveFiles(Path.GetDirectoryName(Overflow1Name), Path.GetFileName(Overflow1Name));
					}
					if (FileExists(Overflow2Name))
					{
						SC.ArchiveFiles(Path.GetDirectoryName(Overflow2Name), Path.GetFileName(Overflow2Name));
					}
				}
			}

			// copy optional unprotected APK if exists
			string UnprotectedApkName = Path.Combine(APKDirectory, "unprotected_" + APKNameWithoutExtension + ".apk");
			if (FileExists(UnprotectedApkName))
			{
				SC.ArchiveFiles(APKDirectory, Path.GetFileName(UnprotectedApkName));
			}

			// copy optional logs directory if exists
			string LogsDirName = Path.Combine(APKDirectory, APKNameWithoutExtension + ".logs");
			if (DirectoryExists(LogsDirName))
			{
				SC.ArchiveFiles(LogsDirName);
			}

			bool bNeedsPCInstall = false;
			bool bNeedsMacInstall = false;
			bool bNeedsLinuxInstall = false;
			GetPlatformInstallOptions(SC, out bNeedsPCInstall, out bNeedsMacInstall, out bNeedsLinuxInstall);

			//helper delegate to prevent code duplication but allow us access to all the local variables we need
			var CreateBatchFilesAndArchiveAction = new Action<UnrealTargetPlatform>(Target =>
			{
				if (bHaveAPK)
				{
					string BatchName = GetFinalBatchName(ApkName, SC, bMakeSeparateApks ? Architecture : "", false, EBatchType.Install, Target);
					SC.ArchiveFiles(Path.GetDirectoryName(BatchName), Path.GetFileName(BatchName));
				}
				if (bHaveAPK || bHaveUniversal)
				{
					string UninstallBatchName = GetFinalBatchName(ApkName, SC, bMakeSeparateApks ? Architecture : "", false, EBatchType.Uninstall, Target);
					SC.ArchiveFiles(Path.GetDirectoryName(UninstallBatchName), Path.GetFileName(UninstallBatchName));
				}
				if (bHaveUniversal)
				{
					string UniversalBatchName = GetFinalBatchName(UniversalApkName, SC, "", false, EBatchType.Install, Target);
					SC.ArchiveFiles(Path.GetDirectoryName(UniversalBatchName), Path.GetFileName(UniversalBatchName));
				}

				if (bBuildWithHiddenSymbolVisibility || bSaveSymbols)
				{
					string SymbolizeBatchName = GetFinalBatchName(ApkName, SC, Architecture, false, EBatchType.Symbolize, Target);
					SC.ArchiveFiles(Path.GetDirectoryName(SymbolizeBatchName), Path.GetFileName(SymbolizeBatchName));
				}
				if (bUseAFS && (bHaveAPK || bHaveUniversal))
				{
					SC.ArchiveFiles(Path.Combine(SC.EngineRoot.FullName, "Binaries", "DotNET", "Android", "UnrealAndroidFileTool"), GetAFSExecutable(Target));
				}
				//SC.ArchiveFiles(Path.GetDirectoryName(NoOBBBatchName), Path.GetFileName(NoOBBBatchName));
			}
			);

			//it's possible we will need both PC and Mac/Linux install files, do both
			if (bNeedsPCInstall)
			{
				CreateBatchFilesAndArchiveAction(UnrealTargetPlatform.Win64);
			}
			if (bNeedsMacInstall)
			{
				CreateBatchFilesAndArchiveAction(UnrealTargetPlatform.Mac);
			}
			if (bNeedsLinuxInstall)
			{
				CreateBatchFilesAndArchiveAction(UnrealTargetPlatform.Linux);
			}
		}
	}

	private void GetPlatformInstallOptions(DeploymentContext SC, out bool bNeedsPCInstall, out bool bNeedsMacInstall, out bool bNeedsLinuxInstall)
	{
		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
		bool bGenerateAllPlatformInstall = false;
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bCreateAllPlatformsInstall", out bGenerateAllPlatformInstall);

		bNeedsPCInstall = bNeedsMacInstall = bNeedsLinuxInstall = false;

		if (bGenerateAllPlatformInstall)
		{
			bNeedsPCInstall = bNeedsMacInstall = bNeedsLinuxInstall = true;
		}
		else
		{
			if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Mac)
			{
				bNeedsMacInstall = true;
			}
			else if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Linux)
			{
				bNeedsLinuxInstall = true;
			}
			else
			{
				bNeedsPCInstall = true;
			}
		}
	}

	private static string GetAdbCommandLine(string SerialNumber, string Args)
	{
	    if (string.IsNullOrEmpty(SerialNumber) == false)
		{
			SerialNumber = "-s " + SerialNumber;
		}

		return string.Format("{0} {1}", SerialNumber, Args);
	}

	static string LastSpewFilename = "";

	public static string ADBSpewFilter(string Message)
	{
		if (Message.StartsWith("[") && Message.Contains("%]"))
		{
			int LastIndex = Message.IndexOf(":");
			LastIndex = LastIndex == -1 ? Message.Length : LastIndex;

			if (Message.Length > 7)
			{
				string Filename = Message.Substring(7, LastIndex - 7);
				if (Filename == LastSpewFilename)
				{
					return null;
				}
				LastSpewFilename = Filename;
			}
			return Message;
		}
		return Message;
	}

	public static IProcessResult RunAdbCommand(ProjectParams Params, string SerialNumber, string Args, string Input = null, ERunOptions Options = ERunOptions.Default, bool bShouldLogCommand = false)
	{
		return RunAdbCommand(SerialNumber, Args, Input, Options, bShouldLogCommand);
	}

	private static IProcessResult RunAdbCommand(string SerialNumber, string Args, string Input = null, ERunOptions Options = ERunOptions.Default, bool bShouldLogCommand = false)
	{
		string AdbCommand = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/platform-tools/adb" + (RuntimePlatform.IsWindows ? ".exe" : ""));
		if (Options.HasFlag(ERunOptions.AllowSpew) || Options.HasFlag(ERunOptions.SpewIsVerbose))
		{
			LastSpewFilename = "";
			return Run(AdbCommand, GetAdbCommandLine(SerialNumber, Args), Input, Options, SpewFilterCallback: new ProcessResult.SpewFilterCallbackType(ADBSpewFilter));
		}
		return Run(AdbCommand, GetAdbCommandLine(SerialNumber, Args), Input, Options);
	}

	private string RunAndLogAdbCommand(ProjectParams Params, string SerialNumber, string Args, out int SuccessCode)
	{
		string AdbCommand = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/platform-tools/adb" + (RuntimePlatform.IsWindows ? ".exe" : ""));
		LastSpewFilename = "";
		return RunAndLog(CmdEnv, AdbCommand, GetAdbCommandLine(SerialNumber, Args), out SuccessCode, SpewFilterCallback: new ProcessResult.SpewFilterCallbackType(ADBSpewFilter));
	}

	public override void GetConnectedDevices(ProjectParams Params, out List<string> Devices)
	{
		Devices = new List<string>();
		IProcessResult Result = RunAdbCommand(Params, "", "devices");

		if (Result.Output.Length > 0)
		{
			string[] LogLines = Result.Output.Split(new char[] { '\n', '\r' });
			bool FoundList = false;
			for (int i = 0; i < LogLines.Length; ++i)
			{
				if (FoundList == false)
				{
					if (LogLines[i].StartsWith("List of devices attached"))
					{
						FoundList = true;
					}
					continue;
				}

				string[] DeviceLine = LogLines[i].Split(new char[] { '\t' });

				if (DeviceLine.Length == 2)
				{
					// the second param should be "device"
					// if it's not setup correctly it might be "unattached" or "powered off" or something like that
					// warning in that case
					if (DeviceLine[1] == "device")
					{
						Devices.Add("@" + DeviceLine[0]);
					}
					else
					{
						CommandUtils.LogWarning("Device attached but in bad state {0}:{1}", DeviceLine[0], DeviceLine[1]);
					}
				}
			}
		}
	}

	/*
	private class TimeRegion : System.IDisposable
	{
		private System.DateTime StartTime { get; set; }

		private string Format { get; set; }

		private System.Collections.Generic.List<object> FormatArgs { get; set; }

		public TimeRegion(string format, params object[] format_args)
		{
			Format = format;
			FormatArgs = new List<object>(format_args);
			StartTime = DateTime.UtcNow;
		}

		public void Dispose()
		{
			double total_time = (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000.0;
			FormatArgs.Insert(0, total_time);
			CommandUtils.Log(Format, FormatArgs.ToArray());
		}
	}
	*/

	private bool RetrieveDeployedManifestsAFS(ProjectParams Params, DeploymentContext SC, string DeviceName, out List<string> UFSManifests, out List<string> NonUFSManifests, string AFSToken)
	{
		UFSManifests = null;
		NonUFSManifests = null;

		string DeviceArchitecture = GetBestDeviceArchitecture(Params, DeviceName);
		string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, DeviceArchitecture);
		string PackageName = GetPackageInfo(ApkName, SC, false);

		AndroidFileClient client = new AndroidFileClient(DeviceName);
		if (!client.OpenConnection())
		{
			if (PackageName == null || PackageName == "")
			{
				Log.TraceInformation("Retrieve Manifests: Unable to start file server without package name, ignoring manifests");
				return false;
			}
			Log.TraceInformation("Retrieve Manifests: Trying to start file server {0}", PackageName);
			if (!client.StartServer(PackageName, AFSToken))
			{
				Log.TraceInformation("Retrieve Manifests: Failed to start server, ignoring manifests");
				return false;
			}
		}

		// verify connection to the correct server
		string DevicePackageName = client.Query("^packagename");
		if (DevicePackageName != PackageName)
		{
			if (PackageName == null || PackageName == "")
			{
				Log.TraceInformation("Retrieve Manifests: Unable to start file server without package name, ignoring manifests");
				client.CloseConnection();
				return false;
			}

			Log.TraceInformation("Connected to wrong server {0}, trying again", DevicePackageName);
			client.TerminateServer();

			Log.TraceInformation("Retrieve Manifests: Trying to start file server {0}", PackageName);
			if (!client.StartServer(PackageName, AFSToken))
			{
				Log.TraceInformation("Retrieve Manifests: Failed to start server, ignoring manifests");
				return false;
			}
		}

		// Try retrieving the UFS files manifest files from the device
		string UFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, SC.GetUFSDeployedManifestFileName(DeviceName));
		if (!client.FileRead("^project/" + SC.GetUFSDeployedManifestFileName(null), UFSManifestFileName))
		{
			return false;
		}

		// Try retrieving the non UFS files manifest files from the device
		string NonUFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, SC.GetNonUFSDeployedManifestFileName(DeviceName));
		if (!client.FileRead("^project/" + SC.GetNonUFSDeployedManifestFileName(null), NonUFSManifestFileName))
		{
			return false;
		}

		client.CloseConnection();

		// Return the manifest files
		UFSManifests = new List<string>();
		UFSManifests.Add(UFSManifestFileName);
		NonUFSManifests = new List<string>();
		NonUFSManifests.Add(NonUFSManifestFileName);

		Log.TraceInformation("Retrieve Manifests: Success!!");

		return true;
	}

	public override bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, string DeviceName, out List<string> UFSManifests, out List<string> NonUFSManifests)
	{
		bool bAFSEnablePlugin;
		string AFSToken;
		bool bIsShipping;
		bool bAFSIncludeInShipping;
		bool bAFSAllowExternalStartInShipping;
		if (UsingAndroidFileServer(Params, SC, out bAFSEnablePlugin, out AFSToken, out bIsShipping, out bAFSIncludeInShipping, out bAFSAllowExternalStartInShipping))
		{
			return RetrieveDeployedManifestsAFS(Params, SC, DeviceName, out UFSManifests, out NonUFSManifests, AFSToken);
		}

		UFSManifests = null;
		NonUFSManifests = null;

		// Query the storage path from the device
		string DeviceStorageQueryCommand = GetStorageQueryCommand();
		IProcessResult StorageResult = RunAdbCommand(Params, DeviceName, DeviceStorageQueryCommand, null, ERunOptions.AppMustExist);
		String StorageLocation = StorageResult.Output.Trim();
		string RemoteDir = StorageLocation + "/UnrealGame/" + Params.ShortProjectName;

		// Try retrieving the UFS files manifest files from the device
		string RetrievedUFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, $"Retrieved_{SC.GetUFSDeployedManifestFileName(DeviceName)}");
		IProcessResult UFSResult = RunAdbCommand(Params, DeviceName, " pull " + RemoteDir + "/" + SC.GetUFSDeployedManifestFileName(null) + " \"" + RetrievedUFSManifestFileName + "\"", null, ERunOptions.AppMustExist);
		if (!(UFSResult.Output.Contains("bytes") || UFSResult.Output.Contains("[100%]")))
		{
			LogWarning("Failed retrieving UFS Manifest: {0}", UFSResult.Output);
			return false;
		}

		// Try retrieving the non UFS files manifest files from the device
		string RetrievedNonUFSManifestFileName = CombinePaths(SC.StageDirectory.FullName, $"Retrieved_{SC.GetNonUFSDeployedManifestFileName(DeviceName)}");
		IProcessResult NonUFSResult = RunAdbCommand(Params, DeviceName, " pull " + RemoteDir + "/" + SC.GetNonUFSDeployedManifestFileName(null) + " \"" + RetrievedNonUFSManifestFileName + "\"", null, ERunOptions.AppMustExist);
		if (!(NonUFSResult.Output.Contains("bytes") || NonUFSResult.Output.Contains("[100%]")))
		{
			LogWarning("Failed retrieving NonUFS Manifest: {0}", NonUFSResult.Output);
			// Did not retrieve both so delete one we did retrieve
			File.Delete(RetrievedUFSManifestFileName);
			return false;
		}

		// Return the manifest files
		UFSManifests = new List<string>();
		UFSManifests.Add(RetrievedUFSManifestFileName);
		NonUFSManifests = new List<string>();
		NonUFSManifests.Add(RetrievedNonUFSManifestFileName);

		return true;
	}

	internal class LongestFirst : IComparer<string>
	{
		public int Compare(string a, string b)
		{
			if (a.Length == b.Length) return a.CompareTo(b);
			else return b.Length - a.Length;
		}
	}

	// Returns a filename from "adb shell ls -RF" output
	// or null if the input line is a directory.
	private string GetFileNameFromListing(string SingleLine)
	{
		if (SingleLine.StartsWith("- ")) // file on Samsung
			return SingleLine.Substring(2);
		else if (SingleLine.StartsWith("d ")) // directory on Samsung
			return null;
		else if (SingleLine.EndsWith("/")) // directory on Google
			return null;
		else // undecorated = file on Google
		{
			return SingleLine;
		}
	}

	private void DeployAndroidFileServer(ProjectParams Params, DeploymentContext SC, string AFSToken)
	{
		var AppArchitectures = AndroidExports.CreateToolChain(Params.RawProjectPath).GetAllArchitectures();

		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.Android);
		bool bDisablePerfHarden = false;
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableMaliPerfCounters", out bDisablePerfHarden);

		bool bUseCompression = false;
		bool bLogFiles = false;
		bool bReportStats = false;
		bool bUseManualIPAddress = false;
		string ManualIPAddress = "";
		EConnectionType ConnectionType = GetAndroidFileServerNetworkConfig(SC, out bUseCompression, out bLogFiles, out bReportStats, out bUseManualIPAddress, out ManualIPAddress);

		AndroidFileClient.OptimalADB adb = new AndroidFileClient.OptimalADB();

		foreach (var DeviceName in Params.DeviceNames)
		{
			string DeviceArchitecture = GetBestDeviceArchitecture(Params, DeviceName);
			string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, DeviceArchitecture);

			// make sure APK is up to date (this is fast if so)
			var Deploy = AndroidExports.CreateDeploymentHandler(Params.RawProjectPath, Params.ForcePackageData);
			if (!Params.Prebuilt)
			{
				string CookFlavor = SC.FinalCookPlatform.IndexOf("_") > 0 ? SC.FinalCookPlatform.Substring(SC.FinalCookPlatform.IndexOf("_")) : "";
				string SOName = GetSONameWithoutArchitecture(Params, SC.StageExecutables[0]);
				Deploy.SetAndroidPluginData(AppArchitectures, CollectPluginDataPaths(SC));
				Deploy.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, SOName, SC.LocalRoot + "/Engine", Params.Distribution, CookFlavor, SC.StageTargets[0].Receipt.Configuration, true, false);
			}

			// now we can use the apk to get more info
			string PackageName = GetPackageInfo(ApkName, SC, false);

			// start up AFS connection (allowed to fail here.. may not be a server installed yet)
			AndroidFileClient client = new AndroidFileClient(DeviceName);
			AndroidFileClient client2 = null;
			string IPAddress = (ConnectionType == EConnectionType.NetworkOnly && bUseManualIPAddress) ? ManualIPAddress : "127.0.0.1";
			if (!client.OpenConnection(IPAddress))
			{
				if (!client.StartServer(PackageName, AFSToken, IPAddress))
				{
				}
			}

			// Try to get IP address from device if we connected
			string DeviceIPAddress = client.Query("^ip");

			// Setup the OBB name and add the storage path (queried from the device) to it
			string QueryStorageResult = adb.Shell(DeviceName, "echo $EXTERNAL_STORAGE");
			string ExternalStorage = QueryStorageResult.Trim();		// "mnt/sdcard"
			string StorageLocation = ExternalStorage + "/Android";	// "mnt/sdcard/Android"
			string DeviceObbName = StorageLocation + "/" + GetDeviceObbName(ApkName, SC);
			string DevicePatchName = StorageLocation + "/" + GetDevicePatchName(ApkName, SC);
			string DeviceOverflow1Name = StorageLocation + "/" + GetDeviceOverflowName(ApkName, SC, 1);
			string DeviceOverflow2Name = StorageLocation + "/" + GetDeviceOverflowName(ApkName, SC, 2);
			string RemoteDir = client.Query("^project", true);
			if (RemoteDir == null)
			{
				RemoteDir = StorageLocation + "/data/" + PackageName + "/files/UnrealGame/" + Params.ShortProjectName;
			}
			string ExtFiles = StorageLocation + "/data/" + PackageName + "/files";

			if (bDisablePerfHarden)
			{
				adb.Shell(DeviceName, "setprop security.perf_harden 0");
			}

			// remove any main or patch OBB file which would override deployed data (note: not the same as later delete or deploy)
			{
				string DeviceOldObbName = ExternalStorage + "/" + GetDeviceObbName(ApkName, SC);
				string DeviceOldPatchName = ExternalStorage + "/" + GetDevicePatchName(ApkName, SC);

				adb.Shell(DeviceName, "rm " + DeviceOldObbName);
				adb.Shell(DeviceName, "rm " + DeviceOldPatchName);
			}

			// close connection to since uninstall/reinstall will reset server
			client.CloseConnection();

			// determine if APK out of date
			string APKLastUpdateTime = new FileInfo(ApkName).LastWriteTime.ToString();
			bool bNeedAPKInstall = true;
			bool bFreshInstall = false;
			bool bFastDeploy = false;
			if (Params.IterativeDeploy)
			{
				// Check for apk installed with this package name on the device
				String InstalledResult = adb.Shell(DeviceName, "pm list packages " + PackageName);
				if (InstalledResult.Contains(PackageName))
				{
					Log.TraceInformation("{0} already installed!", PackageName);
					// already installed so enable --fast-deploy option if need to update apk
					bFastDeploy = true;

					// See if apk is up to date on device
					InstalledResult = adb.Shell(DeviceName, "cat " + ExtFiles + "/APKFileStamp.txt");
					if (InstalledResult.StartsWith("APK: "))
					{
						Log.TraceInformation("Found APKFileStamp.txt! {0}", InstalledResult);
						if (InstalledResult.Substring(5).Trim() == APKLastUpdateTime)
							bNeedAPKInstall = false;

						if (InstalledResult.Substring(5).Trim() != APKLastUpdateTime)
						{
							Log.TraceInformation("{0} != {1}", InstalledResult.Substring(5).Trim(), APKLastUpdateTime);
						}

						// Stop the previously running copy (uninstall/install did this before)
						InstalledResult = adb.Shell(DeviceName, "am force-stop " + PackageName);
						if (InstalledResult.Contains("Error"))
						{
							// force-stop not supported (Android < 3.0) so check if package is actually running
							// Note: cannot use grep here since it may not be installed on device
							InstalledResult = adb.Shell(DeviceName, "ps");
							if (InstalledResult.Contains(PackageName))
							{
								// it is actually running so use the slow way to kill it (uninstall and reinstall)
								bNeedAPKInstall = true;
							}
						}
					}
				}
				else
				{
					// If not already installed we must do a full deploy
					bFreshInstall = true;
				}
			}

			// install new APK if needed
			if (bNeedAPKInstall)
			{
				// try reinstall the apk to preserve data
				int SuccessCode = 0;
				string InstallCommandline = "install -r " + (bFastDeploy ? "--fastdeploy \"" : "\"") + ApkName + "\"";
				string InstallOutput = RunAndLogAdbCommand(Params, DeviceName, InstallCommandline, out SuccessCode);
				int FailureIndex = InstallOutput.IndexOf("Failure");
				// adb install doesn't always return an error code on failure, and instead prints "Failure", followed by an error code.
				if (SuccessCode != 0 || FailureIndex != -1)
				{
					string ErrorMessage = string.Format("Installation of apk '{0}' failed", ApkName);
					if (FailureIndex != -1)
					{
						string FailureString = InstallOutput.Substring(FailureIndex + 7).Trim();
						if (FailureString != "")
						{
							ErrorMessage += ": " + FailureString;
						}
					}
					if (ErrorMessage.Contains("OLDER_SDK"))
					{
						LogError("minSdkVersion is higher than Android version installed on device, possibly due to NDK API Level");
						throw new AutomationException(ExitCode.Error_AppInstallFailed, ErrorMessage);
					}

					// try uninstalling an old app with the same identifier.
					// NOTE: uninstall -k will preserve data/cache.. consider using instead?
					bFreshInstall = true;
					SuccessCode = 0;
					adb.Shell(DeviceName, "pm uninstall " + PackageName);

					// install the apk
					InstallCommandline = "install \"" + ApkName + "\"";
					InstallOutput = RunAndLogAdbCommand(Params, DeviceName, InstallCommandline, out SuccessCode);
					FailureIndex = InstallOutput.IndexOf("Failure");

					// adb install doesn't always return an error code on failure, and instead prints "Failure", followed by an error code.
					if (SuccessCode != 0 || FailureIndex != -1)
					{
						ErrorMessage = string.Format("Installation of apk '{0}' failed", ApkName);
						if (FailureIndex != -1)
						{
							string FailureString = InstallOutput.Substring(FailureIndex + 7).Trim();
							if (FailureString != "")
							{
								ErrorMessage += ": " + FailureString;
							}
						}
						if (ErrorMessage.Contains("OLDER_SDK"))
						{
							LogError("minSdkVersion is higher than Android version installed on device, possibly due to NDK API Level");
						}
						throw new AutomationException(ExitCode.Error_AppInstallFailed, ErrorMessage);
					}
				}

				// giving EXTERNAL_STORAGE_WRITE permission to the apk for API23+
				// without this permission apk can't access to the assets put into the device
				string ReadPermissionCommandLine = "pm grant " + PackageName + " android.permission.READ_EXTERNAL_STORAGE";
				string WritePermissionCommandLine = "pm grant " + PackageName + " android.permission.WRITE_EXTERNAL_STORAGE";
				adb.Shell(DeviceName, ReadPermissionCommandLine);
				adb.Shell(DeviceName, WritePermissionCommandLine);

				// time for receivers to be registered by pm after install
				Thread.Sleep(350);
			}

			// reopen file server connection (either USB or Network)
			IPAddress = (ConnectionType == EConnectionType.NetworkOnly) ? (bUseManualIPAddress ? ManualIPAddress : (DeviceIPAddress != null ? DeviceIPAddress : "127.0.0.1")) : "127.0.0.1";
			Log.TraceInformation("Attempting to connect to file server [{0}]", (IPAddress == "127.0.0.1" ? "USB" : IPAddress));
			if (!client.OpenConnection(IPAddress))
			{
				Log.TraceInformation("Not connected, attempting to start file server");
				if (!client.StartServer(PackageName, AFSToken, IPAddress))
				{
					// try one more time with longer delay
					Log.TraceInformation("Trying again");
					Thread.Sleep(1000);
					if (!client.StartServer(PackageName, AFSToken, IPAddress))
					{
						Log.TraceWarning("Failed to start Android file server for {0}, skipping deploy for {1}", PackageName, DeviceName);
						continue;
					}
				}
			}

			// verify we connected to the right server
			string DevicePackageName = client.Query("^packagename");
			if (DevicePackageName != PackageName)
			{
				Log.TraceInformation("Connected to wrong server {0}, trying again", DevicePackageName);
				client.TerminateServer();

				Log.TraceInformation("Trying to start file server {0}", PackageName);
				if (!client.StartServer(PackageName, AFSToken, IPAddress))
				{
					// try one more time with longer delay
					Log.TraceInformation("Trying again");
					Thread.Sleep(1000);
					if (!client.StartServer(PackageName, AFSToken, IPAddress))
					{
						Log.TraceWarning("Failed to start Android file server for {0}, skipping deploy for {1}", PackageName, DeviceName);
						continue;
					}
				}
			}

			if (ConnectionType == EConnectionType.Combined)
			{
				IPAddress = (bUseManualIPAddress ? ManualIPAddress : client.Query("^ip"));
				client2 = new AndroidFileClient(DeviceName);

				Log.TraceInformation("Attempting to connect to file server [{0}]", IPAddress);
				if (!client2.OpenConnection(IPAddress))
				{
					Log.TraceInformation("Not connected, attempting to start file server");
					if (!client2.StartServer(PackageName, AFSToken, IPAddress, false))
					{
						Log.TraceWarning("Failed to start Android file server for {0}, only using one connection for {1}", PackageName, DeviceName);
						client2 = null;
					}
				}

				// verify we connected to the right server
				DevicePackageName = client2.Query("^packagename");
				if (DevicePackageName != PackageName)
				{
					Log.TraceInformation("Connected to wrong server {0} for [{1}], not using network", DevicePackageName, IPAddress, DeviceName);
					client2.CloseConnection();
					client2 = null;
				}
			}

			// get RemoteDir again (should be valid now after install / restart)
			RemoteDir = client.Query("^project", true);

			// write new timestamp for APK (do it here since RemoteDir now available)
			if (bNeedAPKInstall)
			{
				client.FileWriteString("APK: " + APKLastUpdateTime + "\n", "^ext/APKFileStamp.txt");
			}

			// update the uecommandline.txt
			// update and deploy uecommandline.txt
			// always delete the existing commandline text file, so it doesn't reuse an old one
			FileReference IntermediateCmdLineFile = FileReference.Combine(SC.StageDirectory, "UECommandLine.txt");
			Project.WriteStageCommandline(IntermediateCmdLineFile, Params, SC);

			// copy files to device if we were staging
			if (SC.Stage)
			{
				HashSet<string> EntriesToDeploy = new HashSet<string>();

				// Fresh install always needs full deploy
				if (Params.IterativeDeploy && !bFreshInstall)
				{
					// always send UECommandLine.txt (it was written above after delta checks applied)
					EntriesToDeploy.Add(IntermediateCmdLineFile.FullName);

					// Add non UFS files if any to deploy
					String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(DeviceName);
					if (File.Exists(NonUFSManifestPath))
					{
						string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
						foreach (string Filename in NonUFSFiles.Split('\n'))
						{
							if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
							{
								EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
							}
						}
					}

					// Add UFS files if any to deploy
					String UFSManifestPath = SC.GetUFSDeploymentDeltaPath(DeviceName);
					if (File.Exists(UFSManifestPath))
					{
						string UFSFiles = File.ReadAllText(UFSManifestPath);
						foreach (string Filename in UFSFiles.Split('\n'))
						{
							if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
							{
								EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
							}
						}
					}

					// For now, if too many files may be better to just push them all
					if (EntriesToDeploy.Count > 500)
					{
						// make sure device is at a clean state
						client.DirDeleteRecurse(RemoteDir);

						EntriesToDeploy.Clear();
						EntriesToDeploy.TrimExcess();
						EntriesToDeploy.Add(SC.StageDirectory.FullName);
					}
					else
					{
						// Discover & remove any files on device that are not in staging

						// get listing of remote directory from device
						string CommandResult = client.DirListFlat(RemoteDir);

						if (CommandResult == null)
						{
							Log.TraceWarning("Failed to read remote dir: {0}", RemoteDir);
							RemoteDir = client.Query("^project", true);
							CommandResult = client.DirListFlat(RemoteDir);
							if (CommandResult == null)
							{
								Log.TraceWarning("Failed to read remote dir again: {0}", RemoteDir);
							}
						}

						{
							// listing output is of the form
							// [Samsung]                 [Google]
							//
							// RemoteDir/RestOfPath:     RemoteDir/RestOfPath:
							// - File1.png               File1.png
							// - File2.txt               File2.txt
							// d SubDir1                 SubDir1/
							// d SubDir2                 Subdir2/
							//
							// RemoteDir/RestOfPath/SubDir1:

							HashSet<string> DirsToDeleteFromDevice = new HashSet<string>();
							List<string> FilesToDeleteFromDevice = new List<string>();

							using (var reader = new StringReader(CommandResult))
							{
								string ProjectSaved = Params.ShortProjectName + "/Saved";
								string ProjectConfig = Params.ShortProjectName + "/Config";
								const string EngineSaved = "Engine/Saved"; // is this safe to use, or should we use SC.EngineRoot.GetDirectoryName()?
								const string EngineConfig = "Engine/Config";
								LogWarning("Excluding {0} {1} {2} {3} from clean during deployment.", ProjectSaved, ProjectConfig, EngineSaved, EngineConfig);

								string CurrentDir = "";
								bool SkipFiles = false;
								for (string Line = reader.ReadLine(); Line != null; Line = reader.ReadLine())
								{
									if (String.IsNullOrWhiteSpace(Line))
									{
										continue; // ignore blank lines
									}

									if (Line.EndsWith(":"))
									{
										// RemoteDir/RestOfPath:
										//      keep ^--------^
										CurrentDir = Line.Substring(RemoteDir.Length + 1, Math.Max(0, Line.Length - RemoteDir.Length - 2));
										// Max is there for the case of base "RemoteDir:" --> ""

										// We want to keep config & logs between deployments.
										if (CurrentDir.StartsWith(ProjectSaved) || CurrentDir.StartsWith(ProjectConfig) || CurrentDir.StartsWith(EngineSaved) || CurrentDir.StartsWith(EngineConfig))
										{
											SkipFiles = true;
											continue;
										}

										bool DirExistsInStagingArea = Directory.Exists(Path.Combine(SC.StageDirectory.FullName, CurrentDir));
										if (DirExistsInStagingArea)
										{
											SkipFiles = false;
										}
										else
										{
											// delete directory from device
											SkipFiles = true;
											DirsToDeleteFromDevice.Add(CurrentDir);
										}
									}
									else
									{
										if (SkipFiles)
										{
											continue;
										}

										string FileName = GetFileNameFromListing(Line);
										if (FileName != null)
										{
											bool FileExistsInStagingArea = File.Exists(Path.Combine(SC.StageDirectory.FullName, CurrentDir, FileName));
											if (FileExistsInStagingArea)
											{
												// keep or overwrite
											}
											else if (FileName == "APKFileStamp.txt")
											{
												// keep it
											}
											else
											{
												// delete file from device
												string FilePath = CurrentDir.Length == 0 ? FileName : (CurrentDir + "/" + FileName); // use / for Android target, no matter the development system
												LogWarning("Deleting {0} from device; not found in staging area", FilePath);
												FilesToDeleteFromDevice.Add(FilePath);
											}
										}
										// We ignore subdirs here as each will have its own "RemoteDir/CurrentDir/SubDir:" entry.
									}
								}
							}

							// delete directories
							foreach (var DirToDelete in DirsToDeleteFromDevice)
							{
								// if a whole tree is to be deleted, don't spend extra commands deleting its branches
								int FinalSlash = DirToDelete.LastIndexOf('/');
								string ParentDir = FinalSlash >= 0 ? DirToDelete.Substring(0, FinalSlash) : "";
								bool ParentMarkedForDeletion = DirsToDeleteFromDevice.Contains(ParentDir);
								if (!ParentMarkedForDeletion)
								{
									LogWarning("Deleting {0} and its contents from device; not found in staging area", DirToDelete);
									client.DirDeleteRecurse(RemoteDir + "/" + DirToDelete);
								}
							}

							// delete loose files
							foreach (var FileToDelete in FilesToDeleteFromDevice)
							{
								client.FileDelete(RemoteDir + "/" + FileToDelete);
							}
						}
					}
				}
				else
				{
					// make sure device is at a clean state
					client.DirDeleteRecurse(RemoteDir);

					// Copy UFS files..
					string[] Files = Directory.GetFiles(SC.StageDirectory.FullName, "*", SearchOption.AllDirectories);
					System.Array.Sort(Files);

					// Find all the files we exclude from copying. And include
					// the directories we need to individually copy.
					HashSet<string> ExcludedFiles = new HashSet<string>();
					SortedSet<string> IndividualCopyDirectories
						= new SortedSet<string>((IComparer<string>)new LongestFirst());
					foreach (string Filename in Files)
					{
						bool Exclude = false;
						// Don't push the apk, we install it
						Exclude |= Path.GetExtension(Filename).Equals(".apk", StringComparison.InvariantCultureIgnoreCase);
						// For excluded files we add the parent dirs to our
						// tracking of stuff to individually copy.
						if (Exclude)
						{
							ExcludedFiles.Add(Filename);
							// We include all directories up to the stage root in having
							// to individually copy the files.
							for (string FileDirectory = Path.GetDirectoryName(Filename);
								!FileDirectory.Equals(SC.StageDirectory);
								FileDirectory = Path.GetDirectoryName(FileDirectory))
							{
								if (!IndividualCopyDirectories.Contains(FileDirectory))
								{
									IndividualCopyDirectories.Add(FileDirectory);
								}
							}
							if (!IndividualCopyDirectories.Contains(SC.StageDirectory.FullName))
							{
								IndividualCopyDirectories.Add(SC.StageDirectory.FullName);
							}
						}
					}

					// The directories are sorted above in "deepest" first. We can
					// therefore start copying those individual dirs which will
					// recreate the tree. As the subtrees will get copied at each
					// possible individual level.
					foreach (string DirectoryName in IndividualCopyDirectories)
					{
						string[] Entries
							= Directory.GetFileSystemEntries(DirectoryName, "*", SearchOption.TopDirectoryOnly);
						foreach (string Entry in Entries)
						{
							// We avoid excluded files and the individual copy dirs
							// (the individual copy dirs will get handled as we iterate).
							if (ExcludedFiles.Contains(Entry) || IndividualCopyDirectories.Contains(Entry))
							{
								continue;
							}
							else
							{
								EntriesToDeploy.Add(Entry);
							}
						}
					}

					if (EntriesToDeploy.Count == 0)
					{
						EntriesToDeploy.Add(SC.StageDirectory.FullName);
					}
				}

				// delete the .obb file, since it will cause nothing we deploy to be used
				client.FileDelete(DeviceObbName);
				client.FileDelete(DevicePatchName);
				client.FileDelete(DeviceOverflow1Name);
				client.FileDelete(DeviceOverflow2Name);

				// We now have a minimal set of file & dir entries we need
				// to deploy. Files we deploy will get individually copied
				// and dirs will get the tree copies by default (that's
				// what ADB does, too).
				Log.TraceInformation("Deploying files using AFS");
				string SourceDir = SC.StageDirectory.FullName;
				client.Deploy(EntriesToDeploy, SourceDir, RemoteDir, bUseCompression, bLogFiles, bReportStats, client2);
			}
			else if (SC.Archive)
			{
				// deploy the obb if there is one
				string ObbPath = Path.Combine(SC.StageDirectory.FullName, GetFinalObbName(ApkName, SC));
				if (File.Exists(ObbPath))
				{
					client.FileWrite(ObbPath, DeviceObbName);
				}

				// deploy the patch if there is one
				string PatchPath = Path.Combine(SC.StageDirectory.FullName, GetFinalPatchName(ApkName, SC));
				if (File.Exists(PatchPath))
				{
					client.FileWrite(PatchPath, DevicePatchName);
				}

				// deploy the overflow1 if there is one
				string Overflow1Path = Path.Combine(SC.StageDirectory.FullName, GetFinalOverflowName(ApkName, SC, 1));
				if (File.Exists(Overflow1Path))
				{
					client.FileWrite(Overflow1Path, DeviceOverflow1Name);
				}

				// deploy the overflow2 if there is one
				string Overflow2Path = Path.Combine(SC.StageDirectory.FullName, GetFinalOverflowName(ApkName, SC, 2));
				if (File.Exists(Overflow2Path))
				{
					client.FileWrite(Overflow2Path, DeviceOverflow2Name);
				}
			}
			else
			{
				// cache some strings
				string RemoteFilename = IntermediateCmdLineFile.FullName.Replace(SC.StageDirectory.FullName, RemoteDir).Replace("\\", "/");
				client.FileWrite(IntermediateCmdLineFile.FullName, RemoteFilename);
			}

			// terminate server and close AFS connections
			if (client != null)
			{
				client.TerminateServer();
				client.CloseConnection();
			}
			if (client2 != null)
			{
				client2.TerminateServer();
				client2.CloseConnection();
			}
		}
	}

	private void DeployADB(ProjectParams Params, DeploymentContext SC)
    {
		var AppArchitectures = AndroidExports.CreateToolChain(Params.RawProjectPath).GetAllArchitectures();

		ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), UnrealTargetPlatform.Android);
		bool bDisablePerfHarden = false;
		Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableMaliPerfCounters", out bDisablePerfHarden);

		foreach (var DeviceName in Params.DeviceNames)
        {
            string DeviceArchitecture = GetBestDeviceArchitecture(Params, DeviceName);
            string ApkName = GetFinalApkName(Params, SC.StageExecutables[0], true, DeviceArchitecture);

            // make sure APK is up to date (this is fast if so)
            var Deploy = AndroidExports.CreateDeploymentHandler(Params.RawProjectPath, Params.ForcePackageData);
            if (!Params.Prebuilt)
            {
                string CookFlavor = SC.FinalCookPlatform.IndexOf("_") > 0 ? SC.FinalCookPlatform.Substring(SC.FinalCookPlatform.IndexOf("_")) : "";
				string SOName = GetSONameWithoutArchitecture(Params, SC.StageExecutables[0]);
				Deploy.SetAndroidPluginData(AppArchitectures, CollectPluginDataPaths(SC));
                Deploy.PrepForUATPackageOrDeploy(Params.RawProjectPath, Params.ShortProjectName, SC.ProjectRoot, SOName, SC.LocalRoot + "/Engine", Params.Distribution, CookFlavor, SC.StageTargets[0].Receipt.Configuration, true, false);
            }

            // now we can use the apk to get more info
            string PackageName = GetPackageInfo(ApkName, SC, false);

            // Setup the OBB name and add the storage path (queried from the device) to it
            string DeviceStorageQueryCommand = GetStorageQueryCommand();
            IProcessResult Result = RunAdbCommand(Params, DeviceName, DeviceStorageQueryCommand, null, ERunOptions.AppMustExist);
            String StorageLocation = Result.Output.Trim(); // "/mnt/sdcard";
            string DeviceObbName = StorageLocation + "/" + GetDeviceObbName(ApkName, SC);
			string DevicePatchName = StorageLocation + "/" + GetDevicePatchName(ApkName, SC);
			string DeviceOverflow1Name = StorageLocation + "/" + GetDeviceOverflowName(ApkName, SC, 1);
			string DeviceOverflow2Name = StorageLocation + "/" + GetDeviceOverflowName(ApkName, SC, 2);
			string RemoteDir = StorageLocation + "/UnrealGame/" + Params.ShortProjectName;

			if (bDisablePerfHarden)
			{
				RunAdbCommand(Params, DeviceName, "shell setprop security.perf_harden 0");
			}

			// remove any main or patch OBB file which would override deployed data (note: not the same as later delete or deploy)
			{
				string DeviceOldObbName = StorageLocation + "/Android/" + GetDeviceObbName(ApkName, SC);
				string DeviceOldPatchName = StorageLocation + "/Android/" + GetDevicePatchName(ApkName, SC);

				RunAdbCommand(Params, DeviceName, "shell rm " + DeviceOldObbName);
				RunAdbCommand(Params, DeviceName, "shell rm " + DeviceOldPatchName);
			}

            // determine if APK out of date
            string APKLastUpdateTime = new FileInfo(ApkName).LastWriteTime.ToString();
            bool bNeedAPKInstall = true;
            if (Params.IterativeDeploy)
            {
                // Check for apk installed with this package name on the device
                IProcessResult InstalledResult = RunAdbCommand(Params, DeviceName, "shell pm list packages " + PackageName, null, ERunOptions.AppMustExist);
                if (InstalledResult.Output.Contains(PackageName))
                {
                    // See if apk is up to date on device
                    InstalledResult = RunAdbCommand(Params, DeviceName, "shell cat " + RemoteDir + "/APKFileStamp.txt", null, ERunOptions.AppMustExist);
                    if (InstalledResult.Output.StartsWith("APK: "))
                    {
                        if (InstalledResult.Output.Substring(5).Trim() == APKLastUpdateTime)
                            bNeedAPKInstall = false;

                        // Stop the previously running copy (uninstall/install did this before)
                        InstalledResult = RunAdbCommand(Params, DeviceName, "shell am force-stop " + PackageName, null, ERunOptions.AppMustExist);
                        if (InstalledResult.Output.Contains("Error"))
                        {
                            // force-stop not supported (Android < 3.0) so check if package is actually running
                            // Note: cannot use grep here since it may not be installed on device
                            InstalledResult = RunAdbCommand(Params, DeviceName, "shell ps", null, ERunOptions.AppMustExist);
                            if (InstalledResult.Output.Contains(PackageName))
                            {
                                // it is actually running so use the slow way to kill it (uninstall and reinstall)
                                bNeedAPKInstall = true;
                            }

                        }
                    }
                }
            }

            // install new APK if needed
            if (bNeedAPKInstall)
            {
                // try uninstalling an old app with the same identifier.
                int SuccessCode = 0;
                string UninstallCommandline = "uninstall " + PackageName;
                RunAndLogAdbCommand(Params, DeviceName, UninstallCommandline, out SuccessCode);

                // install the apk
                string InstallCommandline = "install \"" + ApkName + "\"";
                string InstallOutput = RunAndLogAdbCommand(Params, DeviceName, InstallCommandline, out SuccessCode);
                int FailureIndex = InstallOutput.IndexOf("Failure");

                // adb install doesn't always return an error code on failure, and instead prints "Failure", followed by an error code.
                if (SuccessCode != 0 || FailureIndex != -1)
                {
                    string ErrorMessage = string.Format("Installation of apk '{0}' failed", ApkName);
                    if (FailureIndex != -1)
                    {
                        string FailureString = InstallOutput.Substring(FailureIndex + 7).Trim();
                        if (FailureString != "")
                        {
                            ErrorMessage += ": " + FailureString;
                        }
                    }
                    if (ErrorMessage.Contains("OLDER_SDK"))
                    {
                        LogError("minSdkVersion is higher than Android version installed on device, possibly due to NDK API Level");
                    }
                    throw new AutomationException(ExitCode.Error_AppInstallFailed, ErrorMessage);
                }
                else
                {
                    // giving EXTERNAL_STORAGE_WRITE permission to the apk for API23+
                    // without this permission apk can't access to the assets put into the device
                    string ReadPermissionCommandLine = "shell pm grant " + PackageName + " android.permission.READ_EXTERNAL_STORAGE";
                    string WritePermissionCommandLine = "shell pm grant " + PackageName + " android.permission.WRITE_EXTERNAL_STORAGE";
                    RunAndLogAdbCommand(Params, DeviceName, ReadPermissionCommandLine, out SuccessCode);
                    RunAndLogAdbCommand(Params, DeviceName, WritePermissionCommandLine, out SuccessCode);
                }
            }

            // update the uecommandline.txt
            // update and deploy uecommandline.txt
            // always delete the existing commandline text file, so it doesn't reuse an old one
            FileReference IntermediateCmdLineFile = FileReference.Combine(SC.StageDirectory, "UECommandLine.txt");
            Project.WriteStageCommandline(IntermediateCmdLineFile, Params, SC);

            // copy files to device if we were staging
            if (SC.Stage)
            {
                // cache some strings
                string BaseCommandline = "push";

                HashSet<string> EntriesToDeploy = new HashSet<string>();

                if (Params.IterativeDeploy)
                {
                    // always send UECommandLine.txt (it was written above after delta checks applied)
                    EntriesToDeploy.Add(IntermediateCmdLineFile.FullName);

                    // Add non UFS files if any to deploy
                    String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(DeviceName);
                    if (File.Exists(NonUFSManifestPath))
                    {
                        string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
                        foreach (string Filename in NonUFSFiles.Split('\n'))
                        {
                            if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
                            {
                                EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
                            }
                        }
                    }

                    // Add UFS files if any to deploy
                    String UFSManifestPath = SC.GetUFSDeploymentDeltaPath(DeviceName);
                    if (File.Exists(UFSManifestPath))
                    {
                        string UFSFiles = File.ReadAllText(UFSManifestPath);
                        foreach (string Filename in UFSFiles.Split('\n'))
                        {
                            if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
                            {
                                EntriesToDeploy.Add(CombinePaths(SC.StageDirectory.FullName, Filename.Trim()));
                            }
                        }
                    }

                    // For now, if too many files may be better to just push them all
                    if (EntriesToDeploy.Count > 500)
                    {
                        // make sure device is at a clean state
                        RunAdbCommand(Params, DeviceName, "shell rm -r " + RemoteDir);

                        EntriesToDeploy.Clear();
                        EntriesToDeploy.TrimExcess();
                        EntriesToDeploy.Add(SC.StageDirectory.FullName);
                    }
					else
					{
						// Discover & remove any files on device that are not in staging

						// get listing of remote directory from device
						string Commandline = "shell ls -RF1 " + RemoteDir;
						var CommandResult = RunAdbCommand(Params, DeviceName, Commandline, null, ERunOptions.AppMustExist);
						// CommandResult.ExitCode is adb shell's exit code, not ls exit code, which is what we need.
						// Check output for error message instead.
						if (CommandResult.Output.StartsWith("ls: "))
						{
							// list command failed, try simpler options
							Commandline = "shell ls -RF " + RemoteDir;
							CommandResult = RunAdbCommand(Params, DeviceName, Commandline, null, ERunOptions.AppMustExist);
						}

						if (CommandResult.Output.StartsWith("ls: "))
						{
							// list command failed, so clean the remote dir instead of selectively deleting files
							RunAdbCommand(Params, DeviceName, "shell rm -r " + RemoteDir);
						}
						else
						{
						// listing output is of the form
						// [Samsung]                 [Google]
						//
						// RemoteDir/RestOfPath:     RemoteDir/RestOfPath:
						// - File1.png               File1.png
						// - File2.txt               File2.txt
						// d SubDir1                 SubDir1/
						// d SubDir2                 Subdir2/
						//
						// RemoteDir/RestOfPath/SubDir1:

						HashSet<string> DirsToDeleteFromDevice = new HashSet<string>();
						List<string> FilesToDeleteFromDevice = new List<string>();

						using (var reader = new StringReader(CommandResult.Output))
						{
							string ProjectSaved = Params.ShortProjectName + "/Saved";
							string ProjectConfig = Params.ShortProjectName + "/Config";
							const string EngineSaved = "Engine/Saved"; // is this safe to use, or should we use SC.EngineRoot.GetDirectoryName()?
							const string EngineConfig = "Engine/Config";
							LogWarning("Excluding {0} {1} {2} {3} from clean during deployment.", ProjectSaved, ProjectConfig, EngineSaved, EngineConfig);

							string CurrentDir = "";
							bool SkipFiles = false;
							for (string Line = reader.ReadLine(); Line != null; Line = reader.ReadLine())
							{
								if (String.IsNullOrWhiteSpace(Line))
								{
									continue; // ignore blank lines
								}

								if (Line.EndsWith(":"))
								{
									// RemoteDir/RestOfPath:
									//      keep ^--------^
									CurrentDir = Line.Substring(RemoteDir.Length + 1, Math.Max(0, Line.Length - RemoteDir.Length - 2));
									// Max is there for the case of base "RemoteDir:" --> ""

									// We want to keep config & logs between deployments.
									if (CurrentDir.StartsWith(ProjectSaved) || CurrentDir.StartsWith(ProjectConfig) || CurrentDir.StartsWith(EngineSaved) || CurrentDir.StartsWith(EngineConfig))
									{
										SkipFiles = true;
										continue;
									}

									bool DirExistsInStagingArea = Directory.Exists(Path.Combine(SC.StageDirectory.FullName, CurrentDir));
									if (DirExistsInStagingArea)
									{
										SkipFiles = false;
									}
									else
									{
										// delete directory from device
										SkipFiles = true;
										DirsToDeleteFromDevice.Add(CurrentDir);
									}
								}
								else
								{
									if (SkipFiles)
									{
										continue;
									}

									string FileName = GetFileNameFromListing(Line);
									if (FileName != null)
									{
										bool FileExistsInStagingArea = File.Exists(Path.Combine(SC.StageDirectory.FullName, CurrentDir, FileName));
										if (FileExistsInStagingArea)
										{
											// keep or overwrite
										}
										else if (FileName == "APKFileStamp.txt")
										{
											// keep it
										}
										else
										{
											// delete file from device
											string FilePath = CurrentDir.Length == 0 ? FileName : (CurrentDir + "/" + FileName); // use / for Android target, no matter the development system
											LogWarning("Deleting {0} from device; not found in staging area", FilePath);
											FilesToDeleteFromDevice.Add(FilePath);
										}
									}
									// We ignore subdirs here as each will have its own "RemoteDir/CurrentDir/SubDir:" entry.
								}
							}
						}

						// delete directories
						foreach (var DirToDelete in DirsToDeleteFromDevice)
						{
							// if a whole tree is to be deleted, don't spend extra commands deleting its branches
							int FinalSlash = DirToDelete.LastIndexOf('/');
							string ParentDir = FinalSlash >= 0 ? DirToDelete.Substring(0, FinalSlash) : "";
							bool ParentMarkedForDeletion = DirsToDeleteFromDevice.Contains(ParentDir);
							if (!ParentMarkedForDeletion)
							{
								LogWarning("Deleting {0} and its contents from device; not found in staging area", DirToDelete);
								RunAdbCommand(Params, DeviceName, "shell rm -r " + RemoteDir + "/" + DirToDelete);
							}
						}

						// delete loose files
						if (FilesToDeleteFromDevice.Count > 0)
						{
							// delete all stray files with one command
							Commandline = String.Format("shell cd {0}; rm ", RemoteDir);
							RunAdbCommand(Params, DeviceName, Commandline + String.Join(" ", FilesToDeleteFromDevice));
						}
						}
					}
                }
                else
                {
                    // make sure device is at a clean state
                    RunAdbCommand(Params, DeviceName, "shell rm -r " + RemoteDir);

                    // Copy UFS files..
                    string[] Files = Directory.GetFiles(SC.StageDirectory.FullName, "*", SearchOption.AllDirectories);
                    System.Array.Sort(Files);

                    // Find all the files we exclude from copying. And include
                    // the directories we need to individually copy.
                    HashSet<string> ExcludedFiles = new HashSet<string>();
                    SortedSet<string> IndividualCopyDirectories
                        = new SortedSet<string>((IComparer<string>)new LongestFirst());
                    foreach (string Filename in Files)
                    {
                        bool Exclude = false;
                        // Don't push the apk, we install it
                        Exclude |= Path.GetExtension(Filename).Equals(".apk", StringComparison.InvariantCultureIgnoreCase);
                        // For excluded files we add the parent dirs to our
                        // tracking of stuff to individually copy.
                        if (Exclude)
                        {
                            ExcludedFiles.Add(Filename);
                            // We include all directories up to the stage root in having
                            // to individually copy the files.
                            for (string FileDirectory = Path.GetDirectoryName(Filename);
                                !FileDirectory.Equals(SC.StageDirectory);
                                FileDirectory = Path.GetDirectoryName(FileDirectory))
                            {
                                if (!IndividualCopyDirectories.Contains(FileDirectory))
                                {
                                    IndividualCopyDirectories.Add(FileDirectory);
                                }
                            }
                            if (!IndividualCopyDirectories.Contains(SC.StageDirectory.FullName))
                            {
                                IndividualCopyDirectories.Add(SC.StageDirectory.FullName);
                            }
                        }
                    }

                    // The directories are sorted above in "deepest" first. We can
                    // therefore start copying those individual dirs which will
                    // recreate the tree. As the subtrees will get copied at each
                    // possible individual level.
                    foreach (string DirectoryName in IndividualCopyDirectories)
                    {
                        string[] Entries
                            = Directory.GetFileSystemEntries(DirectoryName, "*", SearchOption.TopDirectoryOnly);
                        foreach (string Entry in Entries)
                        {
                            // We avoid excluded files and the individual copy dirs
                            // (the individual copy dirs will get handled as we iterate).
                            if (ExcludedFiles.Contains(Entry) || IndividualCopyDirectories.Contains(Entry))
                            {
                                continue;
                            }
                            else
                            {
                                EntriesToDeploy.Add(Entry);
                            }
                        }
                    }

                    if (EntriesToDeploy.Count == 0)
                    {
                        EntriesToDeploy.Add(SC.StageDirectory.FullName);
                    }
                }

                // We now have a minimal set of file & dir entries we need
                // to deploy. Files we deploy will get individually copied
                // and dirs will get the tree copies by default (that's
                // what ADB does).
                HashSet<IProcessResult> DeployCommands = new HashSet<IProcessResult>();
                foreach (string Entry in EntriesToDeploy)
                {
                    string FinalRemoteDir = RemoteDir;
                    string RemotePath = Entry.Replace(SC.StageDirectory.FullName, FinalRemoteDir).Replace("\\", "/");
                    string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, Entry, RemotePath);
                    // We run deploy commands in parallel to maximize the connection
                    // throughput.
                    DeployCommands.Add(
                        RunAdbCommand(Params, DeviceName, Commandline, null,
                            ERunOptions.Default | ERunOptions.NoWaitForExit));
                    // But we limit the parallel commands to avoid overwhelming
                    // memory resources.
                    if (DeployCommands.Count == DeployMaxParallelCommands)
                    {
                        while (DeployCommands.Count > DeployMaxParallelCommands / 2)
                        {
                            Thread.Sleep(1);
                            DeployCommands.RemoveWhere(
                                delegate (IProcessResult r)
                                {
                                    return r.HasExited;
                                });
                        }
                    }
                }
                foreach (IProcessResult deploy_result in DeployCommands)
                {
                    deploy_result.WaitForExit();
                }

                // delete the .obb file, since it will cause nothing we just deployed to be used
                RunAdbCommand(Params, DeviceName, "shell rm " + DeviceObbName);
				RunAdbCommand(Params, DeviceName, "shell rm " + DevicePatchName);
				RunAdbCommand(Params, DeviceName, "shell rm " + DeviceOverflow1Name);
				RunAdbCommand(Params, DeviceName, "shell rm " + DeviceOverflow2Name);
			}
			else if (SC.Archive)
            {
                // deploy the obb if there is one
                string ObbPath = Path.Combine(SC.StageDirectory.FullName, GetFinalObbName(ApkName, SC));
                if (File.Exists(ObbPath))
                {
                    // cache some strings
                    string BaseCommandline = "push";

                    string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, ObbPath, DeviceObbName);
                    RunAdbCommand(Params, DeviceName, Commandline);
                }

				// deploy the patch if there is one
				string PatchPath = Path.Combine(SC.StageDirectory.FullName, GetFinalPatchName(ApkName, SC));
				if (File.Exists(PatchPath))
				{
					// cache some strings
					string BaseCommandline = "push";

					string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, PatchPath, DevicePatchName);
					RunAdbCommand(Params, DeviceName, Commandline);
				}

				// deploy the overflow1 if there is one
				string Overflow1Path = Path.Combine(SC.StageDirectory.FullName, GetFinalOverflowName(ApkName, SC, 1));
				if (File.Exists(Overflow1Path))
				{
					// cache some strings
					string BaseCommandline = "push";

					string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, Overflow1Path, DeviceOverflow1Name);
					RunAdbCommand(Params, DeviceName, Commandline);
				}

				// deploy the overflow2 if there is one
				string Overflow2Path = Path.Combine(SC.StageDirectory.FullName, GetFinalOverflowName(ApkName, SC, 2));
				if (File.Exists(Overflow2Path))
				{
					// cache some strings
					string BaseCommandline = "push";

					string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, Overflow2Path, DeviceOverflow2Name);
					RunAdbCommand(Params, DeviceName, Commandline);
				}
			}
			else
            {
                // cache some strings
                string BaseCommandline = "push";

                string FinalRemoteDir = RemoteDir;
                /*
			    // handle the special case of the UECommandline.txt when using content only game (UnrealGame)
			    if (!Params.IsCodeBasedProject)
			    {
				    FinalRemoteDir = "/mnt/sdcard/UnrealGame";
			    }
			    */

                string RemoteFilename = IntermediateCmdLineFile.FullName.Replace(SC.StageDirectory.FullName, FinalRemoteDir).Replace("\\", "/");
                string Commandline = string.Format("{0} \"{1}\" \"{2}\"", BaseCommandline, IntermediateCmdLineFile, RemoteFilename);
                RunAdbCommand(Params, DeviceName, Commandline);
            }

            // write new timestamp for APK (do it here since RemoteDir will now exist)
            if (bNeedAPKInstall)
            {
                int SuccessCode = 0;
                RunAndLogAdbCommand(Params, DeviceName, "shell \"echo 'APK: " + APKLastUpdateTime + "' > " + RemoteDir + "/APKFileStamp.txt\"", out SuccessCode);
            }
        }
    }

	public override void Deploy(ProjectParams Params, DeploymentContext SC)
	{
		bool bAFSEnablePlugin;
		string AFSToken;
		bool bIsShipping;
		bool bAFSIncludeInShipping;
		bool bAFSAllowExternalStartInShipping;

		// Pick the proper deploy method
		if (UsingAndroidFileServer(Params, SC, out bAFSEnablePlugin, out AFSToken, out bIsShipping, out bAFSIncludeInShipping, out bAFSAllowExternalStartInShipping))
		{
			DeployAndroidFileServer(Params, SC, AFSToken);
		}
		else
		{
			DeployADB(Params, SC);
		}
	}

	/** Internal usage for GetPackageName */
	private static string PackageLine = null;
	private static Mutex PackageInfoMutex = new Mutex();
	private static string LaunchableActivityLine = null;
	private static string MetaAppTypeLine = null;
	private static Dictionary<string,string> MetaDataMap = null;

	/** Run an external exe (and capture the output), given the exe path and the commandline. */
	public static string GetPackageInfo(string ApkName, bool bRetrieveVersionCode)
	{
		string ReturnValue = null;

		// we expect there to be one, so use the first one
		string AaptPath = GetAaptPath();

		PackageInfoMutex.WaitOne();

		if (File.Exists(ApkName))
		{
			var ExeInfo = new ProcessStartInfo(AaptPath, "dump --include-meta-data badging \"" + ApkName + "\"");
			ExeInfo.UseShellExecute = false;
			ExeInfo.RedirectStandardOutput = true;
			using (var GameProcess = Process.Start(ExeInfo))
			{
				PackageLine = null;
				LaunchableActivityLine = null;
				MetaAppTypeLine = null;
				MetaDataMap = null;
				GameProcess.BeginOutputReadLine();
				GameProcess.OutputDataReceived += ParsePackageName;
				GameProcess.WaitForExit();
			}

			PackageInfoMutex.ReleaseMutex();
			
			if (PackageLine != null)
			{
				// the line should look like: package: name='com.epicgames.qagame' versionCode='1' versionName='1.0'
				string[] Tokens = PackageLine.Split("'".ToCharArray());
				int TokenIndex = bRetrieveVersionCode ? 3 : 1;
				if (Tokens.Length >= TokenIndex + 1)
				{
					ReturnValue = Tokens[TokenIndex];
				}
			}
			LogInformation("GetPackageInfo ReturnValue: {0}", ReturnValue);
		}

		return ReturnValue;
	}

	public static string GetPackageInfo(string ApkName, DeploymentContext SC, bool bRetrieveVersionCode)
	{
		string ReturnValue = GetPackageInfo(ApkName, bRetrieveVersionCode);

		if (ReturnValue == null || ReturnValue.Length == 0)
		{
			/** If APK does not exist or we cant find package info in apk use the packageInfo file */
			ReturnValue = GetPackageInfoFromInfoFile(ApkName, SC, bRetrieveVersionCode);
		}

		return ReturnValue;
	}

	/** Lookup package info in packageInfo.txt file in same directory as the APK would have been */
	private static string GetPackageInfoFromInfoFile(string ApkName, DeploymentContext SC, bool bRetrieveVersionCode)
	{
		string ReturnValue = null;
		String PackageInfoPath = Path.Combine(Path.GetDirectoryName(ApkName), "packageInfo.txt");
		Boolean fileExists = File.Exists(PackageInfoPath);
		if (fileExists)
		{
			string[] Lines = File.ReadAllLines(PackageInfoPath);
			int LineIndex = bRetrieveVersionCode ? 1 : 0;
			LogInformation("packageInfo line index: {0}", LineIndex);
			if (Lines.Length >= 2)
			{
				ReturnValue = Lines[LineIndex];
			}
			// parse extra info that the aapt-based method got
			MetaAppTypeLine = Lines[3];
		}

		if (bRetrieveVersionCode)
		{
			int StoreVersion = 1;
			int.TryParse(ReturnValue, out StoreVersion);

			int StoreVersionOffset = 0;
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(SC.RawProjectPath), SC.StageTargetPlatform.PlatformType);
			if (ApkName.Contains("-arm64-"))
			{
				Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersionOffsetArm64", out StoreVersionOffset);
			}
			else if (ApkName.Contains("-x64-"))
			{
				Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersionOffsetX8664", out StoreVersionOffset);
			}
			StoreVersion += StoreVersionOffset;
			ReturnValue = StoreVersion.ToString("0");
		}

		LogInformation("packageInfo.txt file exists: {0}", fileExists);
		LogInformation("packageInfo return MetaAppTypeLine: {0}", MetaAppTypeLine);
		LogInformation("packageInfo return value: {0}", ReturnValue);

		return ReturnValue;
	}

	/** Returns the launch activity name to launch (must call GetPackageInfo first), returns "com.epicgames.unreal.SplashActivity" default if not found */
	public static string GetLaunchableActivityName()
	{
		string ReturnValue = "com.epicgames.unreal.SplashActivity";
		if (LaunchableActivityLine != null)
		{
			// the line should look like: launchable-activity: name='com.epicgames.unreal.SplashActivity'  label='TappyChicken' icon=''
			string[] Tokens = LaunchableActivityLine.Split("'".ToCharArray());
			if (Tokens.Length >= 2)
			{
				ReturnValue = Tokens[1];
			}
		}
		return ReturnValue;
	}

	/** Returns the app type from the packaged APK metadata, returns "" if not found */
	public static string GetMetaAppType()
	{
		string ReturnValue = "";
		if (MetaAppTypeLine != null)
		{
			// the line should look like: meta-data: name='com.epicgames.unreal.GameActivity.AppType' value='Client'
			string[] Tokens = MetaAppTypeLine.Split("'".ToCharArray());
			if (Tokens.Length >= 4)
			{
				ReturnValue = Tokens[3];
			}
		}
		return ReturnValue;
	}

	public static string GetMetadataValue(string MetadataKey)
	{
		if(MetaDataMap != null)
		{
			string MetadataValue;
			if ( MetaDataMap.TryGetValue(MetadataKey, out MetadataValue) )
			{
				return MetadataValue;
			}
		}
		return null;
	}

	/** Simple function to pipe output asynchronously */
	private static void ParsePackageName(object Sender, DataReceivedEventArgs Event)
	{
		// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
		// print anything for that event.
		if (!String.IsNullOrEmpty(Event.Data))
		{
			string Line = Event.Data;
			if (PackageLine == null)
			{
				if (Line.StartsWith("package:"))
				{
					PackageLine = Line;
				}
			}
			if (LaunchableActivityLine == null)
			{
				if (Line.StartsWith("launchable-activity:"))
				{
					LaunchableActivityLine = Line;
				}
			}
			if (MetaAppTypeLine == null)
			{
				if (Line.StartsWith("meta-data: name='com.epicgames.unreal.GameActivity.AppType'"))
				{
					MetaAppTypeLine = Line;
				}
			}
			if(Line.StartsWith("meta-data: name='com.epicgames.unreal.GameActivity"))
			{
				// We expect the meta-data string to be in the format of " meta-data: name='...' value='...' "
				Match MetaDataMatch = Regex.Match(Line, @"meta-data: name='com.epicgames.unreal.GameActivity.(.*?)'.*?'(.*?)'");
				if(MetaDataMatch.Groups.Count == 3)
				{
					if (MetaDataMap == null)
					{
						MetaDataMap = new Dictionary<string, string>();
					}
					try
					{
						MetaDataMap.Add(MetaDataMatch.Groups[1].Value, MetaDataMatch.Groups[2].Value);
					}
					catch (Exception ex)
					{
						LogWarning(@"Ignoring duplicate package metadata entry '"+Line+"\'.\n" + ex.ToString());
					}
				}
				else
				{
					LogWarning("Unexpected layout of package metadata: " + Line);
				}
			}
		}
	}

	static private string CachedAaptPath = null;
	static private string LastAndroidHomePath = null;

	private static uint GetRevisionValue(string VersionString)
	{
		// read up to 4 sections (ie. 20.0.3.5), first section most significant
		// each section assumed to be 0 to 255 range
		uint Value = 0;
		try
		{
			string[] Sections= VersionString.Split(".".ToCharArray());
			Value |= (Sections.Length > 0) ? (uint.Parse(Sections[0]) << 24) : 0;
			Value |= (Sections.Length > 1) ? (uint.Parse(Sections[1]) << 16) : 0;
			Value |= (Sections.Length > 2) ? (uint.Parse(Sections[2]) <<  8) : 0;
			Value |= (Sections.Length > 3) ?  uint.Parse(Sections[3])        : 0;
		}
		catch (Exception)
		{
			// ignore poorly formed version
		}
		return Value;
	}	

	private static string GetAaptPath()
	{
		// return cached path if ANDROID_HOME has not changed
        string HomePath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%");
		if (CachedAaptPath != null && LastAndroidHomePath == HomePath)
		{
			return CachedAaptPath;
		}

		// get a list of the directories in build-tools.. may be more than one set installed (or none which is bad)
		string[] Subdirs = Directory.GetDirectories(Path.Combine(HomePath, "build-tools"));
        if (Subdirs.Length == 0)
        {
            throw new AutomationException(ExitCode.Error_AndroidBuildToolsPathNotFound, "Failed to find %ANDROID_HOME%/build-tools subdirectory. Run SDK manager and install build-tools.");
        }

		// valid directories will have a source.properties with the Pkg.Revision (there is no guarantee we can use the directory name as revision)
		string BestToolPath = null;
		uint BestVersion = 0;
		foreach (string CandidateDir in Subdirs)
		{
			string AaptFilename = Path.Combine(CandidateDir, RuntimePlatform.IsWindows ? "aapt.exe" : "aapt");
			uint RevisionValue = 0;

			if (File.Exists(AaptFilename))
			{
				string SourcePropFilename = Path.Combine(CandidateDir, "source.properties");
				if (File.Exists(SourcePropFilename))
				{
					string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
					foreach (string PropertyLine in PropertyContents)
					{
						if (PropertyLine.StartsWith("Pkg.Revision="))
						{
							RevisionValue = GetRevisionValue(PropertyLine.Substring(13));
							break;
						}
					}
				}
			}

			// remember it if newer version or haven't found one yet
			if (RevisionValue > BestVersion || BestToolPath == null)
			{
				BestVersion = RevisionValue;
				BestToolPath = AaptFilename;
			}
		}

		if (BestToolPath == null)
		{
            throw new AutomationException(ExitCode.Error_AndroidBuildToolsPathNotFound, "Failed to find %ANDROID_HOME%/build-tools subdirectory with aapt. Run SDK manager and install build-tools.");
		}

		CachedAaptPath = BestToolPath;
		LastAndroidHomePath = HomePath;

		LogInformation("Using this aapt: {0}", CachedAaptPath);

		return CachedAaptPath;
	}

	private string GetBestDeviceArchitecture(ProjectParams Params, string DeviceName)
	{
		bool bMakeSeparateApks = UnrealBuildTool.AndroidExports.ShouldMakeSeparateApks();
		// if we are joining all .so's into a single .apk, there's no need to find the best one - there is no other one
		if (!bMakeSeparateApks)
		{
			return "";
		}

		var AppArchitectures = AndroidExports.CreateToolChain(Params.RawProjectPath).GetAllArchitectures();

		// ask the device
		IProcessResult ABIResult = RunAdbCommand(Params, DeviceName, " shell getprop ro.product.cpu.abi", null, ERunOptions.AppMustExist);

		// the output is just the architecture
		string DeviceArch = UnrealBuildTool.AndroidExports.GetUnrealArch(ABIResult.Output.Trim());

		// if the architecture wasn't built, look for a backup
		if (!AppArchitectures.Contains(DeviceArch))
		{
			// go from 64 to 32-bit
			if (DeviceArch == "-arm64")
			{
				DeviceArch = "-armv7";
			}
			// go from 64 to 32-bit
			else if (DeviceArch == "-x64")
			{
				if (!AppArchitectures.Contains("-x86"))
				{
					DeviceArch = "-x86";
				}
				// if it didn't have 32-bit x86, look for 64-bit arm for emulation
				// @todo android 64-bit: x86_64 most likely can't emulate arm64 at this ponit
// 				else if (Array.IndexOf(AppArchitectures, "-arm64") == -1)
// 				{
// 					DeviceArch = "-arm64";
// 				}
				// finally try for 32-bit arm emulation (Houdini)
				else
				{
					DeviceArch = "-armv7";
				}
			}
			// use armv7 (with Houdini emulation)
			else if (DeviceArch == "-x86")
			{
				DeviceArch = "-armv7";
			}
            else
            {
                // future-proof by dropping back to armv7 for unknown
                DeviceArch = "-armv7";
            }
		}

		// if after the fallbacks, we still don't have it, we can't continue
		if (!AppArchitectures.Contains(DeviceArch))
		{
            throw new AutomationException(ExitCode.Error_NoApkSuitableForArchitecture, "Unable to run because you don't have an apk that is usable on {0}. Looked for {1}", DeviceName, DeviceArch);
		}

		return DeviceArch;
	}

	private bool DeployClientCmdLineAFS(ProjectParams Params, string DeviceName, string PackageName, string AFSToken, string ClientCmdLine)
	{
		AndroidFileClient client = new AndroidFileClient(DeviceName);
		if (!client.OpenConnection())
		{
			Log.TraceInformation("DeployClientCmdLine: Trying to start file server {0}", PackageName);
			if (!client.StartServer(PackageName, AFSToken))
			{
				Log.TraceInformation("DeployClientCmdLine: Failed to start server {0}, ignoring client command line", PackageName);
				return false;
			}
		}

		// verify connection to the correct server
		string DevicePackageName = client.Query("^packagename");
		if (DevicePackageName != PackageName)
		{
			Log.TraceInformation("DeployClientCmdLine: Connected to wrong server {0}, trying again", DevicePackageName);
			client.TerminateServer();

			Log.TraceInformation("DeployClientCmdLine: Trying to start file server {0}", PackageName);
			if (!client.StartServer(PackageName, AFSToken))
			{
				Log.TraceInformation("DeployClientCmdLine: Failed to start server {0}, ignoring client command line", PackageName);
				return false;
			}
		}

		Log.TraceInformation("Writing ClientCmdLine to remote ^commandfile: {0}", ClientCmdLine);
		client.FileWriteString(ClientCmdLine, "^commandfile");
		client.TerminateServer();
		client.CloseConnection();
		return true;
	}

	private bool DeployClientCmdLineADB(ProjectParams Params, string DeviceName, string PackageName, string ClientCmdLine)
	{
		string DeviceStorageQueryCommand = GetStorageQueryCommand();
		IProcessResult StorageResult = RunAdbCommand(Params, DeviceName, DeviceStorageQueryCommand, null, ERunOptions.AppMustExist);
		string StorageLocation = StorageResult.Output.Trim();
		string RemoteDir = StorageLocation + "/UnrealGame/" + Params.ShortProjectName;
		string ClientCmdLineTmpFile = Path.GetTempFileName();
		string ClientCmdLineRemoteFile = RemoteDir + "/UECommandLine.txt";
		File.WriteAllText(ClientCmdLineTmpFile, ClientCmdLine);
		Log.TraceInformation("Pushing ClientCmdLine to remote file {0}: {1}", ClientCmdLineRemoteFile, ClientCmdLine);
		RunAdbCommand(DeviceName, String.Format("push {0} {1}", ClientCmdLineTmpFile, ClientCmdLineRemoteFile));
		File.Delete(ClientCmdLineTmpFile);
		return true;
	}

	public override void ModifyFileHostAddresses(List<string> HostAddresses)
	{
		HostAddresses.Insert(0, "127.0.0.1");
	}

	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		IProcessResult Result = null;

		string LogPath = Path.Combine(Params.BaseStageDirectory, "Android\\logs");
		Directory.CreateDirectory(LogPath);

		foreach (string DeviceName in Params.DeviceNames)
		{
			//get the package name and save that
			string DeviceArchitecture = GetBestDeviceArchitecture(Params, DeviceName);

			//strip off the device, GPU architecture and extension (.so)
			int DashIndex = ClientApp.LastIndexOf("-");
			if (DashIndex >= 0)
			{
				ClientApp = ClientApp.Substring(0, DashIndex);
			}

			string ApkName = GetFinalApkName(Params, Path.GetFileNameWithoutExtension(ClientApp), true, DeviceArchitecture);

			if (!File.Exists(ApkName))
			{
				throw new AutomationException(ExitCode.Error_AppNotFound, "Failed to find application " + ApkName);
			}

			// run aapt to get the name of the intent
			string PackageName = GetPackageInfo(ApkName, false);
			if (PackageName == null)
			{
				throw new AutomationException(ExitCode.Error_FailureGettingPackageInfo, "Failed to get package name from " + ClientApp);
			}

			// push ClientCmdLine args as a file to the device to override the stage/apk command line
			{
				bool bAFSEnablePlugin;
				string AFSToken;
				bool bIsShipping;
				bool bAFSIncludeInShipping;
				bool bAFSAllowExternalStartInShipping;
				if (UsingAndroidFileServer(Params, null, out bAFSEnablePlugin, out AFSToken, out bIsShipping, out bAFSIncludeInShipping, out bAFSAllowExternalStartInShipping))
				{
					DeployClientCmdLineAFS(Params, DeviceName, PackageName, AFSToken, ClientCmdLine);
				}
				else
				{
					DeployClientCmdLineADB(Params, DeviceName, PackageName, ClientCmdLine);
				}
			}

			// Message back to the Unreal Editor to correctly set the app id for each device
			CommandUtils.LogInformation("Running Package@Device:{0}@{1}", PackageName, DeviceName);

			// clear the log for the device
			RunAdbCommand(DeviceName, "logcat -c");

			// start the app on device!
			string CommandLine = "shell am start -n " + PackageName + "/" + GetLaunchableActivityName();
			RunAdbCommand(DeviceName, CommandLine);

			// wait before getting the process list with "adb shell ps" from AdbCreatedProcess
			// on some devices the list is not yet ready
			Thread.Sleep(2000);

			// Start logging process and return immediately.
			// Stdout from the title is continuosly emitted to stdout in UAT.
			// When process is done the AdbCreatedProcess wrapper will save the output to the log directories
			Result = RunAdbCommand(DeviceName, "logcat -s UE debug Debug DEBUG", null, ClientRunFlags | ERunOptions.NoWaitForExit);
			Result = new AdbCreatedProcess(Result, LogPath, PackageName, DeviceName);
		}

		return Result;
	}

	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		// Add any Android shader cache files
		DirectoryReference ProjectShaderDir = DirectoryReference.Combine(Params.RawProjectPath.Directory, "Build", "ShaderCaches", "Android");
		if(DirectoryReference.Exists(ProjectShaderDir))
		{
			SC.StageFiles(StagedFileType.UFS, ProjectShaderDir, StageFilesSearch.AllDirectories);
		}
	}

    /// <summary>
    /// Gets cook platform name for this platform.
    /// </summary>
    /// <returns>Cook platform string.</returns>
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return bIsClientOnly ? "AndroidClient" : "Android";
	}

	public override bool DeployLowerCaseFilenames(StagedFileType FileType)
	{
		return false;
	}

	public override string LocalPathToTargetPath(string LocalPath, string LocalRoot)
	{
		return LocalPath.Replace("\\", "/").Replace(LocalRoot, "../../..");
	}

	public override bool IsSupported { get { return true; } }

	public override PakType RequiresPak(ProjectParams Params)
	{
		// if packaging is enabled, always create a pak, otherwise use the Params.Pak value
		return Params.Package ? PakType.Always : PakType.DontCare;
	}
    public override bool SupportsMultiDeviceDeploy
    {
        get
        {
            return true;
        }
    }

    /*
        public override bool RequiresPackageToDeploy
        {
            get { return true; }
        }
    */

	public override List<string> GetDebugFileExtensions()
	{
		return new List<string> { };
	}

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		AndroidExports.StripSymbols(SourceFile, TargetFile, Log.Logger);
	}
}

public class AndroidPlatformMulti : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
		return bIsClientOnly ? "Android_MultiClient" : "Android_Multi";
    }
    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "Multi");
    }
}

public class AndroidPlatformDXT : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
		return bIsClientOnly ? "Android_DXTClient" : "Android_DXT";
    }

    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "DXT");
    }
}
public class AndroidPlatformETC2 : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
		return bIsClientOnly ? "Android_ETC2Client" : "Android_ETC2";
    }
    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "ETC2");
    }
}

public class AndroidPlatformASTC : AndroidPlatform
{
    public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
    {
		return bIsClientOnly ? "Android_ASTCClient" : "Android_ASTC";
    }
    public override TargetPlatformDescriptor GetTargetPlatformDescriptor()
    {
        return new TargetPlatformDescriptor(TargetPlatformType, "ASTC");
    }
}
