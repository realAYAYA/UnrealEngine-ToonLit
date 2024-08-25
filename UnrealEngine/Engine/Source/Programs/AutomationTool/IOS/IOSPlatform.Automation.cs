// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using Ionic.Zip;
using Ionic.Zlib;
using System.Security.Principal;
using System.Threading;
using System.Diagnostics;
using EpicGames.Core;
using System.Xml;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

static class IOSEnvVarNames
{
	// Should we code sign when staging?  (defaults to 1 if not present)
	static public readonly string CodeSignWhenStaging = "uebp_CodeSignWhenStaging";
}

class IOSClientProcess : IProcessResult
{
	private IProcessResult childProcess;
	private Thread consoleLogWorker;
	//private bool			processConsoleLogs;

	public IOSClientProcess(IProcessResult inChildProcess, string inDeviceID)
	{
		childProcess = inChildProcess;

		// Startup another thread that collect device console logs
		//processConsoleLogs = true;
		consoleLogWorker = new Thread(() => ProcessConsoleOutput(inDeviceID));
		consoleLogWorker.Start();
	}

	public void StopProcess(bool KillDescendants = true)
	{
		childProcess.StopProcess(KillDescendants);
		StopConsoleOutput();
	}

	public bool HasExited
	{
		get
		{
			bool result = childProcess.HasExited;

			if (result)
			{
				StopConsoleOutput();
			}

			return result;
		}
	}

	public string GetProcessName()
	{
		return childProcess.GetProcessName();
	}

	public void OnProcessExited()
	{
		childProcess.OnProcessExited();
		StopConsoleOutput();
	}

	public void DisposeProcess()
	{
		childProcess.DisposeProcess();
	}

	public void StdOut(object sender, DataReceivedEventArgs e)
	{
		childProcess.StdOut(sender, e);
	}

	public void StdErr(object sender, DataReceivedEventArgs e)
	{
		childProcess.StdErr(sender, e);
	}

	public int ExitCode
	{
		get { return childProcess.ExitCode; }
		set { childProcess.ExitCode = value; }
	}

	public bool bExitCodeSuccess => ExitCode == 0;

	public string Output
	{
		get { return childProcess.Output; }
	}

	public Process ProcessObject
	{
		get { return childProcess.ProcessObject; }
	}

	public new string ToString()
	{
		return childProcess.ToString();
	}

	public void WaitForExit()
	{
		childProcess.WaitForExit();
	}

	public FileReference WriteOutputToFile(string FileName)
	{
		return childProcess.WriteOutputToFile(FileName);
	}

	private void StopConsoleOutput()
	{
		//processConsoleLogs = false;
		consoleLogWorker.Join();
	}

	public void ProcessConsoleOutput(string inDeviceID)
	{
		// 		MobileDeviceInstance	targetDevice = null;
		// 		foreach(MobileDeviceInstance curDevice in MobileDeviceInstanceManager.GetSnapshotInstanceList())
		// 		{
		// 			if(curDevice.DeviceId == inDeviceID)
		// 			{
		// 				targetDevice = curDevice;
		// 				break;
		// 			}
		// 		}
		// 		
		// 		if(targetDevice == null)
		// 		{
		// 			return;
		// 		}
		// 		
		// 		targetDevice.StartSyslogService();
		// 		
		// 		while(processConsoleLogs)
		// 		{
		// 			string logData = targetDevice.GetSyslogData();
		// 			
		// 			Console.WriteLine("DeviceLog: " + logData);
		// 		}
		// 		
		// 		targetDevice.StopSyslogService();
	}

}

public class IOSPlatform : ApplePlatform
{
	bool bCreatedIPA = false;

	private string PlatformName = null;
	private string SDKName = null;
	public IOSPlatform()
		: this(UnrealTargetPlatform.IOS)
	{
	}

	public IOSPlatform(UnrealTargetPlatform TargetPlatform)
		: base(TargetPlatform)
	{
		PlatformName = TargetPlatform.ToString();
		SDKName = (TargetPlatform == UnrealTargetPlatform.TVOS) ? "appletvos" : "iphoneos";
	}

	public override bool GetDeviceUpdateSoftwareCommand(out string Command, out string Params, ref bool bRequiresPrivilegeElevation, ref bool bCreateWindow, ITurnkeyContext TurnkeyContext, DeviceInfo Device)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{
			Command = Params = null;
			return true;
		}

		TurnkeyContext.Log("Installing an offline downloaded .ipsw onto your device using the Apple Configurator application.");

		// cfgtool needs ECID, not UDID, so find it
		string Configurator = Path.Combine(GetConfiguratorLocation().Replace(" ", "\\ "), "Contents/MacOS/cfgutil");

		string CfgUtilParams = string.Format("-c '{0} list | grep {1}'", Configurator, Device.Id);
		string CfgUtilOutput = UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("sh", CfgUtilParams);
		bRequiresPrivilegeElevation = false;

		Match Result = Regex.Match(CfgUtilOutput, @"Type: (\S*).*ECID: (\S*)");
		if (!Result.Success)
		{
			TurnkeyContext.ReportError($"Unable to find the given deviceid: {Device} in cfgutil output");
			Command = Params = null;
			return false;
		}

		Command = "sh";
		Params = string.Format("-c '{0} --ecid {1} update --ipsw $(CopyOutputPath)'", Configurator, Result.Groups[2]);

		return true;
	}





	private class VerifyIOSSettings
	{
		public string CodeSigningIdentity = null;
		public string BundleId = null;
		public string Account = null;
		public string Password = null;
		public string Team = null;
		public string Provision = null;

		public string RubyScript = Path.Combine(Unreal.EngineDirectory.FullName, "Build/Turnkey/VerifyIOS.ru");
		public string InstallCertScript = Path.Combine(Unreal.EngineDirectory.FullName, "Build/Turnkey/InstallCert.ru");

		private ITurnkeyContext TurnkeyContext;

		public VerifyIOSSettings(BuildCommand Command, ITurnkeyContext TurnkeyContext)
		{
			this.TurnkeyContext = TurnkeyContext;

			FileReference ProjectPath = Command.ParseProjectParam();
			string ProjectName = ProjectPath == null ? "" : ProjectPath.GetFileNameWithoutAnyExtensions();

			ConfigHierarchy EngineConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectPath == null ? null : ProjectPath.Directory, UnrealTargetPlatform.IOS);

			// first look for settings on the commandline:
			CodeSigningIdentity = Command.ParseParamValue("certificate");
			BundleId = Command.ParseParamValue("bundleid");
			Account = Command.ParseParamValue("devcenterusername");
			Password = Command.ParseParamValue("devcenterpassword");
			Team = Command.ParseParamValue("teamid");
			Provision = Command.ParseParamValue("provision");

			if (string.IsNullOrEmpty(Team)) Team = TurnkeyContext.GetVariable("User_AppleDevCenterTeamID");
			if (string.IsNullOrEmpty(Account)) Account = TurnkeyContext.GetVariable("User_AppleDevCenterUsername");
			if (string.IsNullOrEmpty(Provision)) Provision = TurnkeyContext.GetVariable("User_IOSProvisioningProfile");

			// fall back to ini for anything else
			if (string.IsNullOrEmpty(CodeSigningIdentity)) EngineConfig.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "DevCodeSigningIdentity", out CodeSigningIdentity);
			if (string.IsNullOrEmpty(BundleId)) EngineConfig.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier", out BundleId);
			if (string.IsNullOrEmpty(Team)) EngineConfig.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "IOSTeamID", out Team);
			if (string.IsNullOrEmpty(Account)) EngineConfig.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "DevCenterUsername", out Account);
			if (string.IsNullOrEmpty(Password)) EngineConfig.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "DevCenterPassword", out Password);
			if (string.IsNullOrEmpty(Provision)) EngineConfig.GetString("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MobileProvision", out Provision);
			
			BundleId = BundleId.Replace("[PROJECT_NAME]", ProjectName);

			// some are required
			if (string.IsNullOrEmpty(BundleId))
			{
				throw new AutomationException("Turnkey IOS verification requires bundle id (have '{1}', ex: com.company.foo)", CodeSigningIdentity, BundleId);
			}
		}

		public bool RunCommandMaybeInteractive(string Command, string Params, bool bInteractive)
		{
			Console.WriteLine("Running Command '{0} {1}'", Command, Params);

			int ExitCode;
			// if non-interactive, we can just run directly in the current shell
			if (!bInteractive)
			{
				UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut(Command, Params, Log.Logger, out ExitCode);
			}
			else
			{
				// otherwise, run in a new Terminal window via AppleScript
				string ReturnCodeFilename = Path.GetTempFileName();

				// run potentially interactive scripts in a Terminal window
				Params = string.Format(
						" -e \"tell application \\\"Finder\\\"\"" +
						" -e   \"set desktopBounds to bounds of window of desktop\"" +
						" -e \"end tell\"" +
						" -e \"tell application \\\"Terminal\\\"\"" +
						" -e   \"activate\"" +
						" -e   \"set newTab to do script (\\\"{3}; {0} {1}; echo $? > {2}; {3}; exit\\\")\"" +
						" -e   \"set newWindow to window 1\"" +
						" -e   \"set size of newWindow to {{ item 3 of desktopBounds / 2, item 4 of desktopBounds / 2 }}\"" +
						" -e   \"repeat\"" +
						" -e     \"delay 1\"" +
						" -e     \"if not busy of newTab then exit repeat\"" +
						" -e   \"end repeat\"" +
						" -e   \"set exitCode to item 1 of paragraphs of (read \\\"{2}\\\")\"" +
						" -e   \"if exitCode is equal to \\\"0\\\" then\"" +
						" -e     \"close newWindow\"" +
						" -e   \"end if\"" +
						" -e \"end tell\"",
						Command, Params.Replace("\"", "\\\\\\\""), ReturnCodeFilename, "printf \\\\\\\"\\\\\\n\\\\\\n\\\\\\n\\\\\\n\\\\\\\"");

				Console.WriteLine("\n\n\n{0}\n\n\n", Params);

				UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("osascript", Params, Log.Logger, out ExitCode);
				if (ExitCode == 0)
				{
					ExitCode = int.Parse(File.ReadAllText(ReturnCodeFilename));
					File.Delete(ReturnCodeFilename);
				}
				
			}

			if (ExitCode != 0)
			{
				// only ExitCode 3 (needs cert) can be handled. Any other error can't be fixed (Interactive means it can't be fixed)
				if (!bInteractive || ExitCode != 3)
				{
					if (ExitCode == 3)
					{
						TurnkeyContext.ReportError("Signing certificate is required.");
					}
					else
					{
						// @todo turnkey: turn exitcodes into useful messages
						TurnkeyContext.ReportError($"Ruby command exited with code {ExitCode}");
					}

					return false;
				}

				// only here with ExitCode 3
				if (!InstallCert())
				{
					TurnkeyContext.ReportError($"Certificate installation failed.");
					return false;
				}
			}

			return ExitCode == 0;
		}

		public bool RunRubyCommand(bool bVerifyOnly, string DeviceName)
		{
			string Params;

			Params = string.Format("--bundleid {0}", BundleId);

			if (!string.IsNullOrEmpty(CodeSigningIdentity))
			{
				Params += string.Format(" --identity \"{0}\"", CodeSigningIdentity);
			}

			if (!string.IsNullOrEmpty(Account))
			{
				Params += string.Format(" --login {0}", Account);
			}
			if (!string.IsNullOrEmpty(Password))
			{
				Params += string.Format(" --password {0}", Password);
			}
			if (!string.IsNullOrEmpty(Team))
			{
				Params += string.Format(" --team {0}", Team);
			}

			if (!string.IsNullOrEmpty(Provision))
			{
				Params += string.Format(" --provision {0}", Provision);
			}

			if (!string.IsNullOrEmpty(DeviceName))
			{
				Params += string.Format(" --device {0}", DeviceName);
			}

			if (bVerifyOnly)
			{
				Params += string.Format(" --verifyonly");
			}

			return RunCommandMaybeInteractive(RubyScript, Params, !bVerifyOnly);
		}

		private bool InstallCert()
		{
			//		string ProjectName = TurnkeyContext.GetVariable("Project");

			string CertLoc = null;

			if (!string.IsNullOrEmpty(BundleId))
			{
				CertLoc = TurnkeyContext.RetrieveFileSource("DevCert: " + BundleId);
			}
			if (CertLoc == null)
			{
				CertLoc = TurnkeyContext.RetrieveFileSource("DevCert");
			}

			if (CertLoc != null)
			{
				// get the cert password from Studio settings
				string CertPassword = TurnkeyContext.GetVariable("Studio_AppleSigningCertPassword");

				TurnkeyContext.Log($"Will install cert from: '{CertLoc}'");

				// osascript -e 'Tell application "System Events" to display dialog "Enter the network password:" with hidden answer default answer ""' -e 'text returned of result' 2>/dev/null
				string CommandLine = string.Format("'{0}' '{1}'", CertLoc, CertPassword);

				// run ruby script to install cert
				return RunCommandMaybeInteractive(InstallCertScript, CommandLine, true);
			}
			else
			{
				TurnkeyContext.ReportError("Unable to find a tagged source for DevCert");

				return false;
			}

		}
	}



	string GetConfiguratorLocation()
	{
		string FindCommand = "-c 'mdfind \"kMDItemKind == Application\" | grep \"Apple Configurator 2.app\"'";
		return UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("sh", FindCommand);
	}

	//Disabling for 5.0 early access as this code was not executing and has not been tested.
	/*
	public override bool UpdateHostPrerequisites(BuildCommand Command, ITurnkeyContext TurnkeyContext, bool bVerifyOnly)
	{
		int ExitCode;

		if (HostPlatform.Current.HostEditorPlatform != UnrealTargetPlatform.Mac)
		{
			return base.UpdateHostPrerequisites(Command, TurnkeyContext, bVerifyOnly);
		}

		// make sure the Configurator is installed
		string ConfiguratorLocation = GetConfiguratorLocation();

		if (ConfiguratorLocation == "")
		{
			if (bVerifyOnly)
			{
				TurnkeyContext.ReportError("Apple Configurator 2 is required.");
				return false;
			}

			TurnkeyContext.PauseForUser("Apple Configurator 2 is required for some automation to work. You should install it from the App Store. Launching...");

			// we need to install Configurator 2, and we will block until it's done
			UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("open", "macappstore://apps.apple.com/us/app/apple-configurator-2/id1037126344?mt=12");

			while ((ConfiguratorLocation = GetConfiguratorLocation()) == "")
			{
				Thread.Sleep(1000);
			}
		}

		string IsFastlaneInstalled = UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("/usr/bin/gem", "list -ie fastlane");
		if (IsFastlaneInstalled != "true")
		{
			Console.WriteLine("Fastlane is not installed");
			if (bVerifyOnly)
			{
				TurnkeyContext.ReportError("Fastlane is not installed.");
				return false;
			}

			TurnkeyContext.PauseForUser("Installing Fastlane from internet source. You may ignore the error about the bin directory not in your path.");

			// install missing fastlane without needing sudo
			UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("/usr/bin/gem", "install fastlane --user-install --no-document", out ExitCode, true);

			if (ExitCode != 0)
			{
				return false;
			}
		}

		VerifyIOSSettings Settings = new VerifyIOSSettings(Command, TurnkeyContext);

		// look if we have a cert that matches it
		return Settings.RunRubyCommand(bVerifyOnly, null);
	}

	public override bool UpdateDevicePrerequisites(DeviceInfo Device, BuildCommand Command, ITurnkeyContext TurnkeyContext, bool bVerifyOnly)
	{
		if (HostPlatform.Current.HostEditorPlatform != UnrealTargetPlatform.Mac)
		{
			return base.UpdateDevicePrerequisites(Device, Command, TurnkeyContext, bVerifyOnly);
		}

		VerifyIOSSettings Settings = new VerifyIOSSettings(Command, TurnkeyContext);

		// @todo turnkey - better to use the device's udid if it's set properly in DeviceInfo
		string DeviceName = Device == null ? null : Device.Name;

		// now look for a provision that can be used with a (maybe newly) instally cert
		return Settings.RunRubyCommand(bVerifyOnly, DeviceName);
	}
*/

	public override DeviceInfo[] GetDevices()
	{

		List<DeviceInfo> Devices = new List<DeviceInfo>();

		var IdeviceIdPath = GetPathToLibiMobileDeviceTool("idevice_id");
		string Output = Utils.RunLocalProcessAndReturnStdOut(IdeviceIdPath, "");
		var ConnectedDevicesUDIDs = Output.Split(new string[] { Environment.NewLine }, StringSplitOptions.None);

		foreach (string UnparsedUDID in ConnectedDevicesUDIDs)
		{
			DeviceInfo CurrentDevice = new DeviceInfo(TargetPlatformType);
			var IdeviceInfoPath = GetPathToLibiMobileDeviceTool("ideviceinfo");
			String ParsedUDID = UnparsedUDID.Split(" ").First();
			String IdeviceInfoArgs = "-u " + ParsedUDID;

			if (UnparsedUDID.Contains("Network"))
			{
				CurrentDevice.PlatformValues["Connection"] = "Network";
				IdeviceInfoArgs = "-n " + IdeviceInfoArgs;
			}
			else
			{
				CurrentDevice.PlatformValues["Connection"] = "USB";
			}

			string OutputInfo = Utils.RunLocalProcessAndReturnStdOut(IdeviceInfoPath, IdeviceInfoArgs);

			foreach (string Line in OutputInfo.Split(Environment.NewLine.ToCharArray()))
			{
				// check we are returning the proper device for this class
				if (Line.StartsWith("DeviceClass:"))
				{
					bool bIsDeviceTVOS = Line.Split(": ").Last().ToLower() == "tvos";
					if (bIsDeviceTVOS != (TargetPlatformType == UnrealTargetPlatform.TVOS))
					{
						Devices.Remove(CurrentDevice);
					}
				}
				else if (Line.StartsWith("DeviceName: "))
				{
					CurrentDevice.Name = Line.Split(": ").Last();
				}
				else if (Line.StartsWith("UniqueDeviceID: "))
				{
					CurrentDevice.Id = Line.Split(": ").Last();
				}
				else if (Line.StartsWith("ProductType: "))
				{
					CurrentDevice.Type = Line.Split(": ").Last();
				}
				else if (Line.StartsWith("ProductVersion: "))
				{
					CurrentDevice.SoftwareVersion = Line.Split(": ").Last();
				}
			}
			Devices.Add(CurrentDevice);
		}
		return Devices.ToArray();
	}



	public override string GetPlatformPakCommandLine(ProjectParams Params, DeploymentContext SC)
	{
		string PakParams = "";

		string OodleDllPath = DirectoryReference.Combine(SC.ProjectRoot, "Binaries/ThirdParty/Oodle/Mac/libUnrealPakPlugin.dylib").FullName;
		if (File.Exists(OodleDllPath))
		{
			PakParams += String.Format(" -customcompressor=\"{0}\"", OodleDllPath);
		}

		return PakParams;
	}

	public virtual bool PrepForUATPackageOrDeploy(UnrealTargetConfiguration Config, FileReference ProjectFile, string InProjectName, DirectoryReference InProjectDirectory, FileReference Executable, DirectoryReference InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy, bool bCreateStubIPA, bool bIsUEGame)
	{
		FileReference TargetReceiptFileName = GetTargetReceiptFileName(Config, Executable.FullName, InEngineDir, InProjectDirectory, ProjectFile, bIsUEGame);

		return IOSExports.PrepForUATPackageOrDeploy(Config, ProjectFile, InProjectName, InProjectDirectory, Executable, InEngineDir, bForDistribution, CookFlavor, bIsDataDeploy, bCreateStubIPA, TargetReceiptFileName, Log.Logger);
	}


	private FileReference GetTargetReceiptFileName(UnrealTargetConfiguration Config, string InExecutablePath, DirectoryReference InEngineDir, DirectoryReference InProjectDirectory, FileReference ProjectFile, bool bIsUEGame)
	{
		string TargetName = Path.GetFileNameWithoutExtension(InExecutablePath).Split("-".ToCharArray())[0];
		FileReference TargetReceiptFileName;
		UnrealArchitectures Architectures = UnrealArchitectureConfig.ForPlatform(UnrealTargetPlatform.IOS).ActiveArchitectures(ProjectFile, TargetName);
		if (bIsUEGame)
		{
			TargetReceiptFileName = TargetReceipt.GetDefaultPath(InEngineDir, "UnrealGame", UnrealTargetPlatform.IOS, Config, Architectures);
		}
		else
		{
			TargetReceiptFileName = TargetReceipt.GetDefaultPath(ProjectFile.Directory, TargetName, UnrealTargetPlatform.IOS, Config, Architectures);
		}
		return TargetReceiptFileName;
	}

	public virtual void GetProvisioningData(FileReference InProject, bool bDistribution, out string MobileProvision, out string SigningCertificate, out string TeamUUID, out bool bAutomaticSigning)
	{
		IOSExports.GetProvisioningData(InProject, bDistribution, out MobileProvision, out SigningCertificate, out TeamUUID, out bAutomaticSigning);
	}

	public virtual bool DeployGeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, DirectoryReference ProjectDirectory, bool bIsUEGame, string GameName, bool bIsClient, string ProjectName, DirectoryReference InEngineDir, DirectoryReference AppDirectory, string InExecutablePath)
	{
		FileReference TargetReceiptFileName = GetTargetReceiptFileName(Config, InExecutablePath, InEngineDir, ProjectDirectory, ProjectFile, bIsUEGame);
		return IOSExports.GeneratePList(ProjectFile, Config, ProjectDirectory, bIsUEGame, GameName, bIsClient, ProjectName, InEngineDir, AppDirectory, TargetReceiptFileName, Log.Logger);
	}

	protected string MakeIPAFileName(UnrealTargetConfiguration TargetConfiguration, ProjectParams Params, DeploymentContext SC, bool bAllowDistroPrefix)
	{
		string ExeName = SC.StageExecutables[0];
		if (!SC.IsCodeBasedProject)
		{
			ExeName = ExeName.Replace("UnrealGame", Params.RawProjectPath.GetFileNameWithoutExtension());
		}
		return Path.Combine(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", PlatformName,
			((bAllowDistroPrefix && Params.Distribution) ? "Distro_" : "") + ExeName + ".ipa");
	}

	// Determine if we should code sign
	protected bool GetCodeSignDesirability(ProjectParams Params)
	{
		//@TODO: Would like to make this true, as it's the common case for everyone else
		bool bDefaultNeedsSign = true;

		bool bNeedsSign = false;
		string EnvVar = InternalUtils.GetEnvironmentVariable(IOSEnvVarNames.CodeSignWhenStaging, bDefaultNeedsSign ? "1" : "0", /*bQuiet=*/ false);
		if (!bool.TryParse(EnvVar, out bNeedsSign))
		{
			int BoolAsInt;
			if (int.TryParse(EnvVar, out BoolAsInt))
			{
				bNeedsSign = BoolAsInt != 0;
			}
			else
			{
				bNeedsSign = bDefaultNeedsSign;
			}
		}

		if (!String.IsNullOrEmpty(Params.BundleName))
		{
			// Have to sign when a bundle name is specified
			bNeedsSign = true;
		}

		return bNeedsSign;
	}

	private bool IsBuiltAsFramework(ProjectParams Params, DeploymentContext SC)
	{
		UnrealTargetConfiguration Config = SC.StageTargetConfigurations[0];
		string InExecutablePath = CombinePaths(Path.GetDirectoryName(Params.GetProjectExeForPlatform(TargetPlatformType).ToString()), SC.StageExecutables[0]);
		DirectoryReference InEngineDir = DirectoryReference.Combine(SC.LocalRoot, "Engine");
		DirectoryReference InProjectDirectory = Params.RawProjectPath.Directory;
		bool bIsUEGame = !SC.IsCodeBasedProject;

		FileReference ReceiptFileName = GetTargetReceiptFileName(Config, InExecutablePath, InEngineDir, InProjectDirectory, Params.RawProjectPath, bIsUEGame);
		TargetReceipt Receipt;
		bool bIsReadSuccessful = TargetReceipt.TryRead(ReceiptFileName, out Receipt);

		bool bIsBuiltAsFramework = false;
		if (bIsReadSuccessful)
		{
			bIsBuiltAsFramework = Receipt.HasValueForAdditionalProperty("CompileAsDll", "true");
		}

		return bIsBuiltAsFramework;
	}

	private void StageCustomLocalizationResources(ProjectParams Params, DeploymentContext SC)
	{
		string RelativeResourcesPath = CombinePaths("Build", "IOS", "Resources", "Localizations");
		DirectoryReference LocalizationDirectory = DirectoryReference.Combine(Params.RawProjectPath.Directory, RelativeResourcesPath);
		if (DirectoryReference.Exists(LocalizationDirectory))
		{
			IEnumerable<DirectoryReference> LocalizationDirsToStage = DirectoryReference.EnumerateDirectories(LocalizationDirectory, "*.lproj", SearchOption.TopDirectoryOnly);
			Logger.LogInformation("There are {0} Localization directories.", LocalizationDirsToStage.Count());

			foreach (DirectoryReference FullLocDirPath in LocalizationDirsToStage)
			{
				StagedDirectoryReference LocInStageDir = new StagedDirectoryReference(FullLocDirPath.GetDirectoryName());
				SC.StageFiles(StagedFileType.SystemNonUFS, FullLocDirPath, StageFilesSearch.TopDirectoryOnly, LocInStageDir);
			}
		}
		else
		{
			Logger.LogInformation("App has no custom Localization resources");
		}
	}

	private void StageCustomLaunchScreenStoryboard(ProjectParams Params, DeploymentContext SC)
	{
		string InterfaceSBDirectory = Path.GetDirectoryName(Params.RawProjectPath.FullName) + "/Build/IOS/Resources/Interface/";
		if (Directory.Exists(InterfaceSBDirectory + "LaunchScreen.storyboardc"))
		{
			string[] StoryboardFilesToStage = Directory.GetFiles(InterfaceSBDirectory + "LaunchScreen.storyboardc", "*", SearchOption.TopDirectoryOnly);

			if (!DirectoryExists(SC.StageDirectory + "/LaunchScreen.storyboardc"))
			{
				DirectoryInfo createddir = Directory.CreateDirectory(SC.StageDirectory + "/LaunchScreen.storyboardc");
			}

			foreach (string Filename in StoryboardFilesToStage)
			{
				string workingFileName = Filename;
				while (workingFileName.Contains("/"))
				{
					workingFileName = workingFileName.Substring(workingFileName.IndexOf('/') + 1);
				}
				workingFileName = workingFileName.Substring(workingFileName.IndexOf('/') + 1);

				InternalUtils.SafeCopyFile(Filename, SC.StageDirectory + "/" + workingFileName);
			}

			string[] StoryboardAssetsToStage = Directory.GetFiles(InterfaceSBDirectory + "Assets/", "*", SearchOption.TopDirectoryOnly);

			foreach (string Filename in StoryboardAssetsToStage)
			{
				string workingFileName = Filename;
				while (workingFileName.Contains("/"))
				{
					workingFileName = workingFileName.Substring(workingFileName.IndexOf('/') + 1);
				}
				workingFileName = workingFileName.Substring(workingFileName.IndexOf('/') + 1);

				InternalUtils.SafeCopyFile(Filename, SC.StageDirectory + "/" + workingFileName);
			}
		}
		else
		{
			Logger.LogWarning("Use Custom Launch Screen Storyboard is checked but not compiled storyboard could be found. Have you compiled on Mac first ? Falling back to Standard Storyboard");
			StageStandardLaunchScreenStoryboard(Params, SC);
		}
	}

	private void StageStandardLaunchScreenStoryboard(ProjectParams Params, DeploymentContext SC)
	{
		string BuildGraphicsDirectory = Path.GetDirectoryName(Params.RawProjectPath.FullName) + "/Build/IOS/Resources/Graphics/";
		if (File.Exists(BuildGraphicsDirectory + "LaunchScreenIOS.png"))
		{
			InternalUtils.SafeCopyFile(BuildGraphicsDirectory + "LaunchScreenIOS.png", SC.StageDirectory + "/LaunchScreenIOS.png");
		}
	}

	private void StageLaunchScreenStoryboard(ProjectParams Params, DeploymentContext SC)
	{
		bool bCustomLaunchscreenStoryboard = false;
		ConfigHierarchy PlatformGameConfig;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bCustomLaunchscreenStoryboard", out bCustomLaunchscreenStoryboard);
		}

		if (bCustomLaunchscreenStoryboard)
		{
			StageCustomLaunchScreenStoryboard(Params, SC);
		}
		else
		{
			StageStandardLaunchScreenStoryboard(Params, SC);
		}
	}

	public override void Package(ProjectParams Params, DeploymentContext SC, int WorkingCL)
	{
		// use the shared packaging with modern mode
		if (AppleExports.UseModernXcode(Params.RawProjectPath))
		{
			base.Package(Params, SC, WorkingCL);
			return;
		}

		Logger.LogInformation("Package {Arg0}", Params.RawProjectPath);

		bool bIsBuiltAsFramework = IsBuiltAsFramework(Params, SC);

		// ensure the UnrealGame binary exists, if applicable
#if !PLATFORM_MAC
		string ProjectGameExeFilename = Params.GetProjectExeForPlatform(TargetPlatformType).ToString();
		string FullExePath = CombinePaths(Path.GetDirectoryName(ProjectGameExeFilename), SC.StageExecutables[0] + (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac ? ".stub" : ""));
		if (!SC.IsCodeBasedProject && !FileExists_NoExceptions(FullExePath) && !bIsBuiltAsFramework)
		{
			Logger.LogError("{Text}", "Failed to find game binary " + FullExePath);
			throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Could not find binary {0}. You may need to build the Unreal Engine project with your target configuration and platform.", FullExePath);
		}
#endif // PLATFORM_MAC

		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}

		var TargetConfiguration = SC.StageTargetConfigurations[0];

		string MobileProvision;
		string SigningCertificate;
		string TeamUUID;
		bool bAutomaticSigning;
		GetProvisioningData(Params.RawProjectPath, Params.Distribution, out MobileProvision, out SigningCertificate, out TeamUUID, out bAutomaticSigning);

		//@TODO: We should be able to use this code on both platforms, when the following issues are sorted:
		//   - Raw executable is unsigned & unstripped (need to investigate adding stripping to IPP)
		//   - IPP needs to be able to codesign a raw directory
		//   - IPP needs to be able to take a .app directory instead of a Payload directory when doing RepackageFromStage (which would probably be renamed)
		//   - Some discrepancy in the loading screen pngs that are getting packaged, which needs to be investigated
		//   - Code here probably needs to be updated to write 0 byte files as 1 byte (difference with IPP, was required at one point when using Ionic.Zip to prevent issues on device, maybe not needed anymore?)
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			// If we're building as a framework, then we already have everything we need in the .app
			// so simply package it up as an ipa
            if (bIsBuiltAsFramework)
            {
                PackageIPA(Params, ProjectGameExeFilename, SC);
                return;
            }


			// copy in all of the artwork and plist
			PrepForUATPackageOrDeploy(TargetConfiguration, Params.RawProjectPath,
				Params.ShortProjectName,
				Params.RawProjectPath.Directory,
				FileReference.Combine(new FileReference(ProjectGameExeFilename).Directory!, SC.StageExecutables[0]),
				DirectoryReference.Combine(SC.LocalRoot, "Engine"),
				Params.Distribution,
				"",
				false,
				false,
				!SC.IsCodeBasedProject);

			// figure out where to pop in the staged files
			string AppDirectory = string.Format("{0}/Payload/{1}.app",
				Path.GetDirectoryName(ProjectGameExeFilename),
				Path.GetFileNameWithoutExtension(ProjectGameExeFilename));

			// delete the old cookeddata
			InternalUtils.SafeDeleteDirectory(AppDirectory + "/cookeddata", true);
			InternalUtils.SafeDeleteFile(AppDirectory + "/uecommandline.txt", true);

			SearchOption searchMethod;
			if (!Params.IterativeDeploy)
			{
				searchMethod = SearchOption.AllDirectories; // copy the Staged files to the AppDirectory
			}
			else
			{
				searchMethod = SearchOption.TopDirectoryOnly; // copy just the root stage directory files
			}

			string[] StagedFiles = Directory.GetFiles(SC.StageDirectory.FullName, "*", searchMethod);
			foreach (string Filename in StagedFiles)
			{
				string DestFilename = Filename.Replace(SC.StageDirectory.FullName, AppDirectory);
				Directory.CreateDirectory(Path.GetDirectoryName(DestFilename));
				InternalUtils.SafeCopyFile(Filename, DestFilename, true);
			}
		}

		StageLaunchScreenStoryboard(Params, SC);

		IOSExports.GenerateAssetCatalog(Params.RawProjectPath, new FileReference(FullExePath), new DirectoryReference(CombinePaths(Params.BaseStageDirectory, (TargetPlatformType == UnrealTargetPlatform.IOS ? "IOS" : "TVOS"))), TargetPlatformType, Log.Logger);

		bCreatedIPA = false;
		bool bNeedsIPA = false;
		if (Params.IterativeDeploy)
		{
			if (Params.Devices.Count != 1)
			{
				throw new AutomationException("Can only interatively deploy to a single device, but {0} were specified", Params.Devices.Count);
			}

			String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]);
			// check to determine if we need to update the IPA
			if (File.Exists(NonUFSManifestPath))
			{
				string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
				string[] Lines = NonUFSFiles.Split('\n');
				bNeedsIPA = Lines.Length > 0 && !string.IsNullOrWhiteSpace(Lines[0]);
			}
		}

		if (String.IsNullOrEmpty(Params.Provision))
		{
			Params.Provision = MobileProvision;
		}
		if (String.IsNullOrEmpty(Params.Certificate))
		{
			Params.Certificate = SigningCertificate;
		}
		if (String.IsNullOrEmpty(Params.Team))
		{
			Params.Team = TeamUUID;
		}

		Params.AutomaticSigning = bAutomaticSigning;

		// Scheme name and configuration for code signing with Xcode project
		string SchemeName = SC.StageTargets[0].Receipt.TargetName;
		string SchemeConfiguration = TargetConfiguration.ToString();

		WriteEntitlements(Params, SC);

		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{
			var ProjectIPA = MakeIPAFileName(TargetConfiguration, Params, SC, Params.Distribution);
			var ProjectStub = Path.GetFullPath(ProjectGameExeFilename);
			var IPPProjectIPA = "";
			if (ProjectStub.Contains("UnrealGame"))
			{
				IPPProjectIPA = Path.Combine(Path.GetDirectoryName(ProjectIPA), Path.GetFileName(ProjectIPA).Replace(Params.RawProjectPath.GetFileNameWithoutExtension(), "UnrealGame"));
			}

			// package a .ipa from the now staged directory
			var IPPExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/IPhonePackager.exe");

			Logger.LogDebug("ProjectName={Arg0}", Params.ShortProjectName);
			Logger.LogDebug("ProjectStub={ProjectStub}", ProjectStub);
			Logger.LogDebug("ProjectIPA={ProjectIPA}", ProjectIPA);
			Logger.LogDebug("IPPProjectIPA={IPPProjectIPA}", IPPProjectIPA);
			Logger.LogDebug("IPPExe={IPPExe}", IPPExe);

			bool cookonthefly = Params.CookOnTheFly || Params.SkipCookOnTheFly;

			// if we are incremental check to see if we need to even update the IPA
			if (!Params.IterativeDeploy || !File.Exists(ProjectIPA) || bNeedsIPA)
			{
				// delete the .ipa to make sure it was made
				DeleteFile(ProjectIPA);
				if (IPPProjectIPA.Length > 0)
				{
					DeleteFile(IPPProjectIPA);
				}

				bCreatedIPA = true;

				string IPPArguments = "RepackageFromStage \"" + (Params.IsCodeBasedProject ? Params.RawProjectPath.FullName : "Engine") + "\"";
				IPPArguments += " -config " + TargetConfiguration.ToString();
				IPPArguments += " -schemename " + SchemeName + " -schemeconfig \"" + SchemeConfiguration + "\"";

				// targetname will be eg FooClient for a Client Shipping build.
				IPPArguments += " -targetname " + SC.StageExecutables[0].Split("-".ToCharArray())[0];

				if (TargetConfiguration == UnrealTargetConfiguration.Shipping)
				{
					IPPArguments += " -compress=best";
				}

				// Determine if we should sign
				bool bNeedToSign = GetCodeSignDesirability(Params);

				if (!String.IsNullOrEmpty(Params.BundleName))
				{
					// Have to sign when a bundle name is specified
					bNeedToSign = true;
					IPPArguments += " -bundlename " + Params.BundleName;
				}

				if (bNeedToSign)
				{
					IPPArguments += " -sign";
					if (Params.Distribution)
					{
						IPPArguments += " -distribution";
					}
					if (Params.IsCodeBasedProject)
					{
						IPPArguments += (" -codebased");
					}
				}

				if (IsBuiltAsFramework(Params, SC))
				{
					IPPArguments += " -buildasframework";
				}

				IPPArguments += (cookonthefly ? " -cookonthefly" : "");

				string CookPlatformName = GetCookPlatform(Params.DedicatedServer, Params.Client);
				IPPArguments += " -stagedir \"" + CombinePaths(Params.BaseStageDirectory, CookPlatformName) + "\"";
				IPPArguments += " -project \"" + Params.RawProjectPath + "\"";
				if (Params.IterativeDeploy)
				{
					IPPArguments += " -iterate";
				}
				if (!string.IsNullOrEmpty(Params.Provision))
				{
					IPPArguments += " -provision \"" + Params.Provision + "\"";
				}
				if (!string.IsNullOrEmpty(Params.Certificate))
				{
					IPPArguments += " -certificate \"" + Params.Certificate + "\"";
				}
				if (PlatformName == "TVOS")
				{
					IPPArguments += " -tvos";
				}
				RunAndLog(CmdEnv, IPPExe, IPPArguments);

				if (IPPProjectIPA.Length > 0)
				{
					CopyFile(IPPProjectIPA, ProjectIPA);
					DeleteFile(IPPProjectIPA);
				}
			}

			// verify the .ipa exists
			if (!FileExists(ProjectIPA))
			{
				throw new AutomationException(ExitCode.Error_FailedToCreateIPA, "PACKAGE FAILED - {0} was not created", ProjectIPA);
			}

			if (WorkingCL > 0)
			{
				// Open files for add or edit
				var ExtraFilesToCheckin = new List<string>
				{
					ProjectIPA
				};

				// check in the .ipa along with everything else
				UnrealBuild.AddBuildProductsToChangelist(WorkingCL, ExtraFilesToCheckin);
			}

			//@TODO: This automatically deploys after packaging, useful for testing on PC when iterating on IPP
			//Deploy(Params, SC);
		}
		else
		{
			// create the ipa
			string IPAName = CombinePaths(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Binaries", PlatformName, (Params.Distribution ? "Distro_" : "") + Params.ShortProjectName + (SC.StageTargetConfigurations[0] != UnrealTargetConfiguration.Development ? ("-" + PlatformName + "-" + SC.StageTargetConfigurations[0].ToString()) : "") + ".ipa");

			if (!Params.IterativeDeploy || !File.Exists(IPAName) || bNeedsIPA)
			{
				bCreatedIPA = true;

				// code sign the app
				CodeSign(Path.GetDirectoryName(ProjectGameExeFilename), Params.IsCodeBasedProject ? Params.ShortProjectName : Path.GetFileNameWithoutExtension(ProjectGameExeFilename), Params.RawProjectPath, SC.StageTargetConfigurations[0], SC.LocalRoot.FullName, Params.ShortProjectName, Path.GetDirectoryName(Params.RawProjectPath.FullName), SC.IsCodeBasedProject, Params.Distribution, Params.Provision, Params.Certificate, Params.Team, Params.AutomaticSigning, SchemeName, SchemeConfiguration);

				// now generate the ipa
				PackageIPA(Params, ProjectGameExeFilename, SC);

				// generate a .app that can be run directly on an Apple Silicon Mac (the Mac needs to be in the mobileprovision, just like an IOS device)
				// @todo IOSOnMac
				// CreateIOSOnMacApp(ProjectGameExeFilename, Params, SC);
			}
		}


		PrintRunTime();
	}

	private string EnsureXcodeProjectExists(FileReference RawProjectPath, string LocalRoot, string ShortProjectName, string ProjectRoot, bool IsCodeBasedProject, out bool bWasGenerated)
	{
		// first check for the .xcodeproj
		bWasGenerated = false;
		DirectoryReference RawProjectDir = RawProjectPath.Directory;
		DirectoryReference XcodeProj = DirectoryReference.Combine(RawProjectDir, "Intermediate/ProjectFiles", $"{RawProjectPath.GetFileNameWithoutAnyExtensions()}_{PlatformName}.xcworkspace");
		Console.WriteLine("Project: " + XcodeProj.FullName);
		{
			// project.xcodeproj doesn't exist, so generate temp project
			string Arguments = "-project=\"" + RawProjectPath + "\"";
			Arguments += " -platforms=" + PlatformName + " -game -nointellisense -" + PlatformName + "deployonly -ignorejunk -projectfileformat=XCode -includetemptargets -automated";

			// If engine is installed then UBT doesn't need to be built
			if (Unreal.IsEngineInstalled())
			{
				Arguments = "-XcodeProjectFiles " + Arguments;
				RunUBT(CmdEnv, UnrealBuild.UnrealBuildToolDll, Arguments);
			}
			else
			{
				string Script = CombinePaths(CmdEnv.LocalRoot, "Engine/Build/BatchFiles/Mac/GenerateProjectFiles.sh");
				string CWD = Directory.GetCurrentDirectory();
				Directory.SetCurrentDirectory(Path.GetDirectoryName(Script));
				Run(Script, Arguments, null, ERunOptions.Default);
				Directory.SetCurrentDirectory(CWD);
			}

			bWasGenerated = true;

			if (!DirectoryReference.Exists(XcodeProj))
			{
				// something very bad happened
				throw new AutomationException("iOS couldn't find the appropriate Xcode Project " + XcodeProj.FullName);
			}
		}

		return XcodeProj.FullName;
	}

	private void CodeSign(string BaseDirectory, string GameName, FileReference RawProjectPath, UnrealTargetConfiguration TargetConfig, string LocalRoot, string ProjectName, string ProjectDirectory, bool IsCode, bool Distribution, string Provision, string Certificate, string Team, bool bAutomaticSigning, string SchemeName, string SchemeConfiguration)
	{
		if (AppleExports.UseModernXcode(RawProjectPath))
		{
			throw new AutomationException("Modern Xcode is not expected to enter this function");
		}

		// check for the proper xcodeproject
		bool bWasGenerated = false;
		string XcodeProj = EnsureXcodeProjectExists(RawProjectPath, LocalRoot, ProjectName, ProjectDirectory, IsCode, out bWasGenerated);

		string Arguments = "UBT_NO_POST_DEPLOY=true";
		Arguments += " /usr/bin/xcrun xcodebuild build -workspace \"" + XcodeProj + "\"";
		Arguments += " -scheme '";
		Arguments += SchemeName != null ? SchemeName : GameName;
		Arguments += "'";
		Arguments += " -configuration \"" + (SchemeConfiguration != null ? SchemeConfiguration : TargetConfig.ToString()) + "\"";
		Arguments += $" -destination generic/platform=\"{AppleExports.GetDestinationPlatform(TargetPlatformType, new UnrealArchitectures(UnrealArch.Arm64))}\"";
		Arguments += " -sdk " + SDKName;

		if (bAutomaticSigning)
		{
			Arguments += " CODE_SIGN_IDENTITY=" + (Distribution ? "\"iPhone Distribution\"" : "\"iPhone Developer\"");
			Arguments += " CODE_SIGN_STYLE=\"Automatic\" -allowProvisioningUpdates";
			Arguments += " DEVELOPMENT_TEAM=\"" + Team + "\"";
		}
		else
		{
			if (!string.IsNullOrEmpty(Certificate))
			{
				Arguments += " CODE_SIGN_IDENTITY=\"" + Certificate + "\"";
			}
			else
			{
				Arguments += " CODE_SIGN_IDENTITY=" + (Distribution ? "\"iPhone Distribution\"" : "\"iPhone Developer\"");
			}
			if (!string.IsNullOrEmpty(Provision))
			{
				// read the provision to get the UUID
				if (File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Provision))
				{
					string UUID = "";
					string AllText = File.ReadAllText(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Provision);
					int idx = AllText.IndexOf("<key>UUID</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							UUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
							Arguments += " PROVISIONING_PROFILE_SPECIFIER=" + UUID;

							Logger.LogInformation("Extracted Provision UUID {UUID} from {Provision}", UUID, Provision);
						}
					}
				}
			}
		}
		IProcessResult Result = Run("/usr/bin/env", Arguments, null, ERunOptions.Default);
		if (bWasGenerated)
		{
			InternalUtils.SafeDeleteDirectory(XcodeProj, true);
		}
		if (Result.ExitCode != 0)
		{
			throw new AutomationException(ExitCode.Error_FailedToCodeSign, "CodeSign Failed");
		}
	}

	private bool ShouldUseMaxIPACompression(ProjectParams Params)
	{
		if (!string.IsNullOrEmpty(Params.AdditionalPackageOptions))
		{
			string[] OptionsArray = Params.AdditionalPackageOptions.Split(' ');
			foreach (string Option in OptionsArray)
			{
				if (Option.Equals("-ForceMaxIPACompression", StringComparison.InvariantCultureIgnoreCase))
				{
					return true;
				}
			}
		}

		return false;
	}

	private void PackageIPA(ProjectParams Params, string ProjectGameExeFilename, DeploymentContext SC)
	{
		string BaseDirectory = Path.GetDirectoryName(ProjectGameExeFilename);
		string ExeName = Params.IsCodeBasedProject ? SC.StageExecutables[0] : Path.GetFileNameWithoutExtension(ProjectGameExeFilename);
		string GameName = ExeName.Split("-".ToCharArray())[0];
		string ProjectName = Params.ShortProjectName;
		UnrealTargetConfiguration TargetConfig = SC.StageTargetConfigurations[0];

		// create the ipa
		string IPAName = MakeIPAFileName(TargetConfig, Params, SC, true);
		// delete the old one
		if (File.Exists(IPAName))
		{
			File.Delete(IPAName);
		}

		// make the subdirectory if needed
		string DestSubdir = Path.GetDirectoryName(IPAName);
		if (!Directory.Exists(DestSubdir))
		{
			Directory.CreateDirectory(DestSubdir);
		}

		// set up the directories
		string ZipWorkingDir = String.Format("Payload/{0}.app/", GameName);
		string ZipSourceDir = string.Format("{0}/Payload/{1}.app", BaseDirectory, GameName);

		// create the file
		using (ZipFile Zip = new ZipFile())
		{
			// Set encoding to support unicode filenames
			Zip.AlternateEncodingUsage = ZipOption.Always;
			Zip.AlternateEncoding = Encoding.UTF8;
			Zip.UseZip64WhenSaving = Zip64Option.AsNecessary;

			// set the compression level
			bool bUseMaxIPACompression = ShouldUseMaxIPACompression(Params);
			if (Params.Distribution || bUseMaxIPACompression)
			{
				Zip.CompressionLevel = CompressionLevel.BestCompression;
			}

			// add the entire directory
			Zip.AddDirectory(ZipSourceDir, ZipWorkingDir);

			// Update permissions to be UNIX-style
			// Modify the file attributes of any added file to unix format
			foreach (ZipEntry E in Zip.Entries)
			{
				const byte FileAttributePlatform_NTFS = 0x0A;
				const byte FileAttributePlatform_UNIX = 0x03;
				const byte FileAttributePlatform_FAT = 0x00;

				const int UNIX_FILETYPE_NORMAL_FILE = 0x8000;
				//const int UNIX_FILETYPE_SOCKET = 0xC000;
				//const int UNIX_FILETYPE_SYMLINK = 0xA000;
				//const int UNIX_FILETYPE_BLOCKSPECIAL = 0x6000;
				const int UNIX_FILETYPE_DIRECTORY = 0x4000;
				//const int UNIX_FILETYPE_CHARSPECIAL = 0x2000;
				//const int UNIX_FILETYPE_FIFO = 0x1000;

				const int UNIX_EXEC = 1;
				const int UNIX_WRITE = 2;
				const int UNIX_READ = 4;


				int MyPermissions = UNIX_READ | UNIX_WRITE;
				int OtherPermissions = UNIX_READ;

				int PlatformEncodedBy = (E.VersionMadeBy >> 8) & 0xFF;
				int LowerBits = 0;

				// Try to preserve read-only if it was set
				bool bIsDirectory = E.IsDirectory;

				// Check to see if this 
				bool bIsExecutable = false;
				if (Path.GetFileNameWithoutExtension(E.FileName).Equals(ExeName, StringComparison.InvariantCultureIgnoreCase))
				{
					bIsExecutable = true;
				}

				if (bIsExecutable && !bUseMaxIPACompression)
				{
					// The executable will be encrypted in the final distribution IPA and will compress very poorly, so keeping it
					// uncompressed gives a better indicator of IPA size for our distro builds
					E.CompressionLevel = CompressionLevel.None;
				}

				if ((PlatformEncodedBy == FileAttributePlatform_NTFS) || (PlatformEncodedBy == FileAttributePlatform_FAT))
				{
					FileAttributes OldAttributes = E.Attributes;
					//LowerBits = ((int)E.Attributes) & 0xFFFF;

					if ((OldAttributes & FileAttributes.Directory) != 0)
					{
						bIsDirectory = true;
					}

					// Permissions
					if ((OldAttributes & FileAttributes.ReadOnly) != 0)
					{
						MyPermissions &= ~UNIX_WRITE;
						OtherPermissions &= ~UNIX_WRITE;
					}
				}

				if (bIsDirectory || bIsExecutable)
				{
					MyPermissions |= UNIX_EXEC;
					OtherPermissions |= UNIX_EXEC;
				}

				// Re-jigger the external file attributes to UNIX style if they're not already that way
				if (PlatformEncodedBy != FileAttributePlatform_UNIX)
				{
					int NewAttributes = bIsDirectory ? UNIX_FILETYPE_DIRECTORY : UNIX_FILETYPE_NORMAL_FILE;

					NewAttributes |= (MyPermissions << 6);
					NewAttributes |= (OtherPermissions << 3);
					NewAttributes |= (OtherPermissions << 0);

					// Now modify the properties
					E.AdjustExternalFileAttributes(FileAttributePlatform_UNIX, (NewAttributes << 16) | LowerBits);
				}
			}

			// Save it out
			Zip.Save(IPAName);
		}
	}

	// Generate a .app that can be run directly on an Apple Silicon Mac (the Mac needs to be in the mobileprovision, just like an IOS device)
	private void CreateIOSOnMacApp(string ProjectGameExeFilename, ProjectParams Params, DeploymentContext SC)
	{
		// make a version that can run on the Mac directly
		string ExeName = Params.IsCodeBasedProject ? SC.StageExecutables[0] : Path.GetFileNameWithoutExtension(ProjectGameExeFilename);
		string GameName = ExeName.Split("-".ToCharArray())[0];
		string SourceApp = Path.Combine(Path.GetDirectoryName(ProjectGameExeFilename), "Payload", $"{GameName}.app");
		string DestOuterApp = Path.Combine(Path.GetDirectoryName(ProjectGameExeFilename), "IOSOnMac", $"{GameName}.app");
		string DestSymlink = Path.Combine(DestOuterApp, "WrappedBundle");
		string DestInnerApp = Path.Combine(DestOuterApp, "Wrapper", $"{GameName}.app");

		Logger.LogInformation("Creating IOSOnMac app {DestOuterApp}", DestOuterApp);

		DeleteDirectory_NoExceptions(DestOuterApp);
		CopyDirectory_NoExceptions(SourceApp, DestInnerApp);
		// the link uses path relative to the symlink itself
		File.CreateSymbolicLink(DestSymlink, $"Wrapper/{GameName}.app");
	}


	public override void GetFilesToDeployOrStage(ProjectParams Params, DeploymentContext SC)
	{
		//		if (UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
		{

			// copy any additional framework assets that will be needed at runtime
			{
				DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Intermediate", "IOS", "FrameworkAssets");
				if (DirectoryReference.Exists(SourcePath))
				{
					SC.StageFiles(StagedFileType.SystemNonUFS, SourcePath, StageFilesSearch.AllDirectories, StagedDirectoryReference.Root);
				}
			}


			// copy the plist (only if code signing, as it's protected by the code sign blob in the executable and can't be modified independently)
			if (GetCodeSignDesirability(Params))
			{
				// this would be FooClient when making a client-only build
				string TargetName = SC.StageExecutables[0].Split("-".ToCharArray())[0];
				DirectoryReference SourcePath = DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : DirectoryReference.Combine(SC.LocalRoot, "Engine")), "Intermediate", PlatformName);
				FileReference TargetPListFile = FileReference.Combine(SourcePath, (SC.IsCodeBasedProject ? TargetName : "UnrealGame") + "-Info.plist");

				//				if (!File.Exists(TargetPListFile))
				{
					// ensure the plist, entitlements, and provision files are properly copied
					Console.WriteLine("CookPlat {0}, this {1}", GetCookPlatform(false, false), ToString());
					if (!SC.IsCodeBasedProject)
					{
						UnrealBuildTool.PlatformExports.SetRemoteIniPath(SC.ProjectRoot.FullName);
					}

					if (SC.StageTargetConfigurations.Count != 1)
					{
						throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
					}

					var TargetConfiguration = SC.StageTargetConfigurations[0];

					DirectoryReference ProjectRoot = SC.ProjectRoot;
					// keep old logic for BP projects with legacy
					if (!AppleExports.UseModernXcode(SC.RawProjectPath) && !SC.IsCodeBasedProject)
					{
						ProjectRoot = DirectoryReference.Combine(SC.LocalRoot, "Engine");
					}

					DeployGeneratePList(
							SC.RawProjectPath,
							TargetConfiguration,
							ProjectRoot,
							!SC.IsCodeBasedProject,
							(SC.IsCodeBasedProject ? TargetName : "UnrealGame"),
							SC.IsCodeBasedProject ? false : Params.Client, // Code based projects will have Client in their executable name already
							SC.ShortProjectName, DirectoryReference.Combine(SC.LocalRoot, "Engine"),
							DirectoryReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : DirectoryReference.Combine(SC.LocalRoot, "Engine")), "Binaries", PlatformName, "Payload", (SC.IsCodeBasedProject ? SC.ShortProjectName : "UnrealGame") + ".app"),
							SC.StageExecutables[0]);

					if (!AppleExports.UseModernXcode(SC.RawProjectPath))
					{
						// copy the plist to the stage dir
						SC.StageFile(StagedFileType.SystemNonUFS, TargetPListFile, new StagedFileReference("Info.plist"));
					}
				}	

				// copy the udebugsymbols if they exist
				{
					ConfigHierarchy PlatformGameConfig;
					bool bIncludeSymbols = false;
					if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
					{
						PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateCrashReportSymbols", out bIncludeSymbols);
					}
					if (bIncludeSymbols)
					{
						FileReference SymbolFileName = FileReference.Combine((SC.IsCodeBasedProject ? SC.ProjectRoot : SC.EngineRoot), "Binaries", "IOS", SC.StageExecutables[0] + ".udebugsymbols");
						if (FileReference.Exists(SymbolFileName))
						{
							SC.StageFile(StagedFileType.NonUFS, SymbolFileName, new StagedFileReference((Params.ShortProjectName + ".udebugsymbols").ToLowerInvariant()));
						}
					}
				}
			}
		}

		{
			// Stage any *.metallib files as NonUFS.
			// Get the final output directory for cooked data
			DirectoryReference CookOutputDir;
			if (!String.IsNullOrEmpty(Params.CookOutputDir))
			{
				CookOutputDir = DirectoryReference.Combine(new DirectoryReference(Params.CookOutputDir), SC.CookPlatform);
			}
			else if (Params.CookInEditor)
			{
				CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "EditorCooked", SC.CookPlatform);
			}
			else
			{
				CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "Cooked", SC.CookPlatform);
			}
			if (DirectoryReference.Exists(CookOutputDir))
			{
				List<FileReference> CookedFiles = DirectoryReference.EnumerateFiles(CookOutputDir, "*.metallib", SearchOption.AllDirectories).ToList();
				foreach (FileReference CookedFile in CookedFiles)
				{
					SC.StageFile(StagedFileType.NonUFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(CookOutputDir)));
				}
			}
		}

		{
			// Stage the mute.caf file used by SoundSwitch for mute switch detection
			FileReference MuteCafFile = FileReference.Combine(SC.EngineRoot, "Source", "ThirdParty", "IOS", "SoundSwitch", "SoundSwitch", "SoundSwitch", "mute.caf");
			if (FileReference.Exists(MuteCafFile))
			{
				SC.StageFile(StagedFileType.SystemNonUFS, MuteCafFile, new StagedFileReference("mute.caf"));
			}
		}

		// Copy any project defined iOS specific localization resources into Staging (ie: App Display Name)
		StageCustomLocalizationResources(Params, SC);
	}

	protected void StageMovieFiles(DirectoryReference InputDir, DeploymentContext SC)
	{
		if (DirectoryReference.Exists(InputDir))
		{
			foreach (FileReference InputFile in DirectoryReference.EnumerateFiles(InputDir, "*", SearchOption.AllDirectories))
			{
				if (!InputFile.HasExtension(".uasset") && !InputFile.HasExtension(".umap"))
				{
					SC.StageFile(StagedFileType.NonUFS, InputFile);
				}
			}
		}
	}
	protected void StageMovieFile(DirectoryReference InputDir, string Filename, DeploymentContext SC)
	{
		if (DirectoryReference.Exists(InputDir))
		{
			foreach (FileReference InputFile in DirectoryReference.EnumerateFiles(InputDir, "*", SearchOption.AllDirectories))
			{

				if (!InputFile.HasExtension(".uasset") && !InputFile.HasExtension(".umap") && InputFile.GetFileNameWithoutExtension().Contains(Filename))
				{
					SC.StageFile(StagedFileType.NonUFS, InputFile);
				}
			}
		}
	}

	public override void GetFilesToArchive(ProjectParams Params, DeploymentContext SC)
	{
		// use the shared archiving with modern mode
		if (AppleExports.UseModernXcode(Params.RawProjectPath))
		{
			base.GetFilesToArchive(Params, SC);
			return;
		}

		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}
		var TargetConfiguration = SC.StageTargetConfigurations[0];
		var ProjectIPA = MakeIPAFileName(TargetConfiguration, Params, SC, true);

		// verify the .ipa exists
		if (!FileExists(ProjectIPA))
		{
			throw new AutomationException("ARCHIVE FAILED - {0} was not found", ProjectIPA);
		}

		ConfigHierarchy PlatformGameConfig;
		bool bXCArchive = false;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateXCArchive", out bXCArchive);
		}

		if (bXCArchive && !RuntimePlatform.IsWindows)
		{
			// Always put the archive in the current user's Library/Developer/Xcode/Archives path if not on the build machine
			string ArchivePath = "/Users/" + Environment.UserName + "/Library/Developer/Xcode/Archives";
			if (IsBuildMachine)
			{
				ArchivePath = Params.ArchiveDirectoryParam;
			}
			if (!DirectoryExists(ArchivePath))
			{
				CreateDirectory(ArchivePath);
			}

			Console.WriteLine("Generating xc archive package in " + ArchivePath);

			string ArchiveName = Path.Combine(ArchivePath, Path.GetFileNameWithoutExtension(ProjectIPA) + ".xcarchive");
			if (!DirectoryExists(ArchiveName))
			{
				CreateDirectory(ArchiveName);
			}
			DeleteDirectoryContents(ArchiveName);

			// create the Products archive folder
			CreateDirectory(Path.Combine(ArchiveName, "Products", "Applications"));

			// copy in the application
			string AppName = Path.GetFileNameWithoutExtension(ProjectIPA) + ".app";
			if (!File.Exists(ProjectIPA))
			{
				Console.WriteLine("Couldn't find IPA: " + ProjectIPA);
			}
			using (ZipFile Zip = new ZipFile(ProjectIPA))
			{
				Zip.ExtractAll(ArchivePath, ExtractExistingFileAction.OverwriteSilently);

				List<string> Dirs = new List<string>(Directory.EnumerateDirectories(Path.Combine(ArchivePath, "Payload"), "*.app"));
				AppName = Dirs[0].Substring(Dirs[0].LastIndexOf(UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac ? "\\" : "/") + 1);
				foreach (string Dir in Dirs)
				{
					if (Dir.Contains(Params.ShortProjectName + ".app"))
					{
						Console.WriteLine("Using Directory: " + Dir);
						AppName = Dir.Substring(Dir.LastIndexOf(UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac ? "\\" : "/") + 1);
					}
				}
				CopyDirectory_NoExceptions(Path.Combine(ArchivePath, "Payload", AppName), Path.Combine(ArchiveName, "Products", "Applications", AppName));
			}

			// copy in the dSYM if found
			var ProjectExe = MakeIPAFileName(TargetConfiguration, Params, SC, false);
			string dSYMName = (SC.IsCodeBasedProject ? Path.GetFileNameWithoutExtension(ProjectExe) : "UnrealGame") + ".dSYM";
			string dSYMDestName = AppName + ".dSYM";
			string dSYMSrcPath = Path.Combine(SC.ProjectBinariesFolder.FullName, dSYMName);
			string dSYMZipSrcPath = Path.Combine(SC.ProjectBinariesFolder.FullName, dSYMName + ".zip");
			if (File.Exists(dSYMZipSrcPath))
			{
				// unzip the dsym
				using (ZipFile Zip = new ZipFile(dSYMZipSrcPath))
				{
					Zip.ExtractAll(SC.ProjectBinariesFolder.FullName, ExtractExistingFileAction.OverwriteSilently);
				}
			}

			if (DirectoryExists(dSYMSrcPath))
			{
				// Create the dsyms archive folder
				CreateDirectory(Path.Combine(ArchiveName, "dSYMs"));
				string dSYMDstPath = Path.Combine(ArchiveName, "dSYMs", dSYMDestName);
				// /Volumes/MacOSDrive1/pfEpicWorkspace/Dev-Platform/Samples/Sandbox/PlatformShowcase/Binaries/IOS/PlatformShowcase.dSYM/Contents/Resources/DWARF/PlatformShowcase
				CopyFile_NoExceptions(Path.Combine(dSYMSrcPath, "Contents", "Resources", "DWARF", SC.IsCodeBasedProject ? Path.GetFileNameWithoutExtension(ProjectExe) : "UnrealGame"), dSYMDstPath);
			}
			else if (File.Exists(dSYMSrcPath))
			{
				// Create the dsyms archive folder
				CreateDirectory(Path.Combine(ArchiveName, "dSYMs"));
				string dSYMDstPath = Path.Combine(ArchiveName, "dSYMs", dSYMDestName);
				CopyFile_NoExceptions(dSYMSrcPath, dSYMDstPath);
			}

			// get the settings from the app plist file
			string AppPlist = Path.Combine(ArchiveName, "Products", "Applications", AppName, "Info.plist");
			string OldPListData = File.Exists(AppPlist) ? File.ReadAllText(AppPlist) : "";

			string BundleIdentifier = "";
			string BundleShortVersion = "";
			string BundleVersion = "";
			if (!string.IsNullOrEmpty(OldPListData))
			{
				// bundle identifier
				int index = OldPListData.IndexOf("CFBundleIdentifier");
				index = OldPListData.IndexOf("<string>", index) + 8;
				int length = OldPListData.IndexOf("</string>", index) - index;
				BundleIdentifier = OldPListData.Substring(index, length);

				// short version
				index = OldPListData.IndexOf("CFBundleShortVersionString");
				index = OldPListData.IndexOf("<string>", index) + 8;
				length = OldPListData.IndexOf("</string>", index) - index;
				BundleShortVersion = OldPListData.Substring(index, length);

				// bundle version
				index = OldPListData.IndexOf("CFBundleVersion");
				index = OldPListData.IndexOf("<string>", index) + 8;
				length = OldPListData.IndexOf("</string>", index) - index;
				BundleVersion = OldPListData.Substring(index, length);
			}
			else
			{
				Console.WriteLine("Could not load Info.plist");
			}

			// date we made this
			const string Iso8601DateTimeFormat = "yyyy-MM-ddTHH:mm:ssZ";
			string TimeStamp = DateTime.UtcNow.ToString(Iso8601DateTimeFormat);

			// create the archive plist
			StringBuilder Text = new StringBuilder();
			Text.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
			Text.AppendLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
			Text.AppendLine("<plist version=\"1.0\">");
			Text.AppendLine("<dict>");
			Text.AppendLine("\t<key>ApplicationProperties</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>ApplicationPath</key>");
			Text.AppendLine("\t\t<string>Applications/" + AppName + "</string>");
			Text.AppendLine("\t\t<key>CFBundleIdentifier</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", BundleIdentifier));
			Text.AppendLine("\t\t<key>CFBundleShortVersionString</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", BundleShortVersion));
			Text.AppendLine("\t\t<key>CFBundleVersion</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", BundleVersion));
			Text.AppendLine("\t\t<key>SigningIdentity</key>");
			Text.AppendLine(string.Format("\t\t<string>{0}</string>", Params.Certificate));
			Text.AppendLine("\t</dict>");
			Text.AppendLine("\t<key>ArchiveVersion</key>");
			Text.AppendLine("\t<integer>2</integer>");
			Text.AppendLine("\t<key>CreationDate</key>");
			Text.AppendLine(string.Format("\t<date>{0}</date>", TimeStamp));
			Text.AppendLine("\t<key>DefaultToolchainInfo</key>");
			Text.AppendLine("\t<dict>");
			Text.AppendLine("\t\t<key>DisplayName</key>");
			Text.AppendLine("\t\t<string>Xcode 7.3 Default</string>");
			Text.AppendLine("\t\t<key>Identifier</key>");
			Text.AppendLine("\t\t<string>com.apple.dt.toolchain.XcodeDefault</string>");
			Text.AppendLine("\t</dict>");
			Text.AppendLine("\t<key>Name</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", SC.ShortProjectName));
			Text.AppendLine("\t<key>SchemeName</key>");
			Text.AppendLine(string.Format("\t<string>{0}</string>", SC.ShortProjectName));
			Text.AppendLine("</dict>");
			Text.AppendLine("</plist>");
			File.WriteAllText(Path.Combine(ArchiveName, "Info.plist"), Text.ToString());
		}
		else if (bXCArchive && RuntimePlatform.IsWindows)
		{
			Logger.LogWarning("Can not produce an XCArchive on windows");
		}
		SC.ArchiveFiles(Path.GetDirectoryName(ProjectIPA), Path.GetFileName(ProjectIPA));
	}

	public override bool RetrieveDeployedManifests(ProjectParams Params, DeploymentContext SC, string DeviceName, out List<string> UFSManifests, out List<string> NonUFSManifests)
	{
		if (Params.Devices.Count != 1)
		{
			throw new AutomationException("Can only retrieve deployed manifests from a single device, but {0} were specified", Params.Devices.Count);
		}

		bool Result = true;
		UFSManifests = new List<string>();
		NonUFSManifests = new List<string>();
		try
		{
			var TargetConfiguration = SC.StageTargetConfigurations[0];
			string BundleIdentifier = "";
			if (File.Exists(Params.BaseStageDirectory + "/" + PlatformName + "/Info.plist"))
			{
				string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}

			string IdeviceInstallerArgs = "--list-apps -u " + Params.DeviceNames[0];
			IdeviceInstallerArgs = GetLibimobileDeviceNetworkedArgument(IdeviceInstallerArgs, Params.DeviceNames[0]);

			var DeviceInstaller = GetPathToLibiMobileDeviceTool("ideviceinstaller");
			Logger.LogInformation("Checking if bundle '{BundleIdentifier}' is installed", BundleIdentifier);

			string Output = "";
			IProcessResult RunResult = CommandUtils.Run(DeviceInstaller, IdeviceInstallerArgs, null, CommandUtils.ERunOptions.NoLoggingOfRunCommand | CommandUtils.ERunOptions.AppMustExist);
			if (RunResult.ExitCode == 0)
			{
				if (!String.IsNullOrEmpty(RunResult.Output))
				{
					Output = RunResult.Output;
				}
			}
			else
			{
				throw new CommandFailedException((ExitCode)RunResult.ExitCode, 
												 String.Format("Command failed (Result:{3}): {0} {1}",
												 DeviceInstaller, IdeviceInstallerArgs, RunResult.ExitCode)){ OutputFormat = AutomationExceptionOutputFormat.Minimal };
			}

			bool bBundleIsInstalled = Output.Contains(string.Format("CFBundleIdentifier -> {0}{1}", BundleIdentifier, Environment.NewLine));
			int ExitCode = 0;

			if (bBundleIsInstalled)
			{
				Logger.LogInformation("Bundle {BundleIdentifier} found, retrieving deployed manifests...", BundleIdentifier);

				var DeviceFS = GetPathToLibiMobileDeviceTool("idevicefs");

				string AllCommandsToPush = " push " + CombinePaths(Params.BaseStageDirectory, PlatformName, SC.GetUFSDeployedManifestFileName(null)) + "\n"
										+ " push " + CombinePaths(Params.BaseStageDirectory, PlatformName, SC.GetNonUFSDeployedManifestFileName(null));		
				System.IO.File.WriteAllText(Directory.GetCurrentDirectory() + "\\CommandsToPush.txt", AllCommandsToPush);

				string IdeviceFSArgs = "-b " + "\"" + BundleIdentifier + " -x " + Directory.GetCurrentDirectory() + "\\CommandsToPush.txt -u " + "\"" + Params.DeviceNames[0];
				IdeviceFSArgs = GetLibimobileDeviceNetworkedArgument(IdeviceFSArgs, Params.DeviceNames[0]);

				Utils.RunLocalProcessAndReturnStdOut(DeviceFS, IdeviceFSArgs, Log.Logger, out ExitCode);
				if (ExitCode != 0)
				{
					throw new AutomationException("Failed to deploy manifest to mobile device.");
				}

				string[] ManifestFiles = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, PlatformName), "*_Manifest_UFS*.txt");
				UFSManifests.AddRange(ManifestFiles);

				ManifestFiles = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, PlatformName), "*_Manifest_NonUFS*.txt");
				NonUFSManifests.AddRange(ManifestFiles);
			}
			else
			{
				Logger.LogInformation("Bundle '{BundleIdentifier}' not found, skipping retrieving deployed manifests", BundleIdentifier);
			}
		}
		catch (System.Exception)
		{
			// delete any files that did get copied
			string[] Manifests = Directory.GetFiles(CombinePaths(Params.BaseStageDirectory, PlatformName), "*_Manifest_*.txt");
			foreach (string Manifest in Manifests)
			{
				File.Delete(Manifest);
			}
			Result = false;
		}

		return Result;
	}

	private string GetPathToLibiMobileDeviceTool(string LibimobileExec)
	{
		string ExecWithPath = "";
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
		{
			ExecWithPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Extras/ThirdPartyNotUE/libimobiledevice/x64/" + LibimobileExec + ".exe");
		}
		else if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			ExecWithPath = CombinePaths(CmdEnv.LocalRoot, "Engine/Extras/ThirdPartyNotUE/libimobiledevice/Mac/" + LibimobileExec);
		}
		if (!File.Exists(ExecWithPath) || ExecWithPath == "")
		{
			throw new AutomationException("Failed to locate LibiMobileDevice executable.");

		}
		return ExecWithPath;
	}

	private string GetLibimobileDeviceNetworkedArgument(string EntryArguments, string UDID)
	{
		DeviceInfo[] CachedDevices = GetDevices();

		if (CachedDevices.Where(CachedDevice => CachedDevice.Id == UDID && CachedDevice.PlatformValues["Connection"] == "Network").Count() > 0)
		{
			return EntryArguments + " -n";
		}
		return EntryArguments;
	}

	private void DeployManifestContent(string BaseFolder, DeploymentContext SC, ProjectParams Params, ref string Files, ref string BundleIdentifier)
	{
		var DeviceFS = GetPathToLibiMobileDeviceTool("idevicefs");

		string[] FileList = Files.Split('\n');
		string AllCommandsToPush = "";
		foreach (string Filename in FileList)
		{
			if (!string.IsNullOrEmpty(Filename) && !string.IsNullOrWhiteSpace(Filename))
			{
				string Trimmed = Filename.Trim();
				string SourceFilename = BaseFolder + "\\" + Trimmed;
				SourceFilename = SourceFilename.Replace('/', '\\');
				string DestFilename = "/Library/Caches/" + Trimmed.Replace("cookeddata/", "");
				DestFilename = DestFilename.Replace('\\', '/');
				DestFilename = "\"" + DestFilename + "\"";
				SourceFilename = SourceFilename.Replace('\\', Path.DirectorySeparatorChar);
				string CommandToPush = "push -p \"" + SourceFilename + "\" " + DestFilename + "\n";
				AllCommandsToPush += CommandToPush;
			}
		}
		System.IO.File.WriteAllText(Directory.GetCurrentDirectory() + "\\CommandsToPush.txt", AllCommandsToPush);
		int ExitCode = 0;

		string IdeviceFSArgs = "-u " + Params.DeviceNames[0] + " -b " + BundleIdentifier + " -x " + "\"" + Directory.GetCurrentDirectory() + "\\CommandsToPush.txt" + "\"";
		IdeviceFSArgs = GetLibimobileDeviceNetworkedArgument(IdeviceFSArgs, Params.DeviceNames[0]);

		using (Process IDeviceFSProcess = new Process())
		{
			DataReceivedEventHandler StdOutHandler = (E, Args) => { if (Args.Data != null) { Logger.LogInformation("{Text}", Args.Data); } };
			DataReceivedEventHandler StdErrHandler = (E, Args) => { if (Args.Data != null) { Logger.LogError("{Text}", Args.Data); } };

			IDeviceFSProcess.StartInfo.FileName = DeviceFS;
			IDeviceFSProcess.StartInfo.Arguments = IdeviceFSArgs;
			IDeviceFSProcess.OutputDataReceived += StdOutHandler;
			IDeviceFSProcess.ErrorDataReceived += StdErrHandler;

			ExitCode = Utils.RunLocalProcess(IDeviceFSProcess);

			if (ExitCode != 0)
			{
				throw new AutomationException("Failed to push content to mobile device.");
			}
		}

		File.Delete(Directory.GetCurrentDirectory() + "\\CommandsToPush.txt");
	}

	public override void Deploy(ProjectParams Params, DeploymentContext SC)
	{
		if (Params.Devices.Count != 1)
		{
			throw new AutomationException("Can only deploy to a single specified device, but {0} were specified", Params.Devices.Count);
		}

		if (SC.StageTargetConfigurations.Count != 1)
		{
			throw new AutomationException("iOS is currently only able to package one target configuration at a time, but StageTargetConfigurations contained {0} configurations", SC.StageTargetConfigurations.Count);
		}
		if (Params.Distribution)
		{
			throw new AutomationException("iOS cannot deploy a package made for distribution.");
		}

		// modern mode simply deploys the staged .app which has been fully completed
		string AppToDeploy;
		if (AppleExports.UseModernXcode(Params.RawProjectPath))
		{
			// modern uses a project-name for the content-only .app, but the StageExecutable is UnrealGame-IOS-Shipping or similar, so
			// fix up the Unreal part
			string AppBaseName = SC.StageExecutables[0];
			if (!SC.IsCodeBasedProject)
			{
				TargetReceipt Target = SC.StageTargets[0].Receipt;
				AppBaseName = AppleExports.MakeBinaryFileName(SC.ShortProjectName, Target.Platform, Target.Configuration, Target.Architectures, UnrealTargetConfiguration.Development, null);
			}
			AppToDeploy = FileReference.Combine(SC.StageDirectory, AppBaseName + ".app").FullName;

			// @todo: deal with iterative deploy - we have the uncombined .app in the Staged dir inin Binaries/IOS
			if (Params.IterativeDeploy)
			{
				throw new AutomationException("Iterative deploy is currently not supported with modern, but it shojld be straightforward");
			}
		}
		else
		{
			UnrealTargetConfiguration TargetConfiguration = SC.StageTargetConfigurations[0];
			string ProjectIPA = MakeIPAFileName(TargetConfiguration, Params, SC, true);
			string StagedIPA = SC.StageDirectory + "\\" + Path.GetFileName(ProjectIPA);

			// verify the .ipa exists
			if (!FileExists(StagedIPA))
			{
				StagedIPA = ProjectIPA;
				if (!FileExists(StagedIPA))
				{
					throw new AutomationException("DEPLOY FAILED - {0} was not found", ProjectIPA);
				}
			}

			AppToDeploy = StagedIPA;
		}


		// if iterative deploy, determine the file delta
		string BundleIdentifier = "";
		bool bNeedsIPA = true;
		if (Params.IterativeDeploy)
		{
			if (File.Exists(Params.BaseStageDirectory + "/" + PlatformName + "/Info.plist"))
			{
				string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}

			// check to determine if we need to update the IPA
			String NonUFSManifestPath = SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]);
			if (File.Exists(NonUFSManifestPath))
			{
				string NonUFSFiles = File.ReadAllText(NonUFSManifestPath);
				string[] Lines = NonUFSFiles.Split('\n');
				bNeedsIPA = Lines.Length > 0 && !string.IsNullOrWhiteSpace(Lines[0]);
			}
		}

		// deploy the .ipa
		var DeviceInstaller = GetPathToLibiMobileDeviceTool("ideviceinstaller");

		string LibimobileDeviceArguments =  "-u " + Params.DeviceNames[0] + " -i " + "\"" +  Path.GetFullPath(AppToDeploy) + "\"";
		LibimobileDeviceArguments = GetLibimobileDeviceNetworkedArgument(LibimobileDeviceArguments, Params.DeviceNames[0]);

		// If we deploying to a Simulator, use "xcrun simctl" instead
		{
			TargetReceipt Targets = SC.StageTargets[0].Receipt;
			if (Targets.Architectures.SingleArchitecture == UnrealArch.IOSSimulator)
			{
				DeviceInstaller = "xcrun";
				LibimobileDeviceArguments = "simctl install " + Params.DeviceNames[0] + " \"" + Path.GetFullPath(AppToDeploy) + "\"";
			}
		}

		// check for it in the stage directory
		string CurrentDir = Directory.GetCurrentDirectory();
		Directory.SetCurrentDirectory(CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/DotNET/IOS/"));
		if (!Params.IterativeDeploy || bCreatedIPA || bNeedsIPA)
		{
			RunAndLog(CmdEnv, DeviceInstaller, LibimobileDeviceArguments);
		}

		// deploy the assets
		if (Params.IterativeDeploy)
		{
			string BaseFolder = Path.GetDirectoryName(SC.GetUFSDeploymentDeltaPath(Params.DeviceNames[0]));
			string FilesString = File.ReadAllText(SC.GetUFSDeploymentDeltaPath(Params.DeviceNames[0]));
			DeployManifestContent(BaseFolder, SC, Params, ref FilesString, ref BundleIdentifier);
			if (bNeedsIPA)
			{
				BaseFolder = Path.GetDirectoryName(SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]));
				FilesString = File.ReadAllText(SC.GetNonUFSDeploymentDeltaPath(Params.DeviceNames[0]));
				DeployManifestContent(BaseFolder, SC, Params, ref FilesString, ref BundleIdentifier);
			}
			
			Directory.SetCurrentDirectory(CurrentDir);
			PrintRunTime();
		}
	}

	public override string GetCookPlatform(bool bDedicatedServer, bool bIsClientOnly)
	{
		return bIsClientOnly ? "IOSClient" : "IOS";
	}

	public override bool DeployLowerCaseFilenames(StagedFileType FileType)
	{
		// we shouldn't modify the case on files like Info.plist or the icons
		return true;
	}

	public override string LocalPathToTargetPath(string LocalPath, string LocalRoot)
	{
		return LocalPath.Replace("\\", "/").Replace(LocalRoot, "../../..");
	}

	public override bool IsSupported { get { return true; } }

	public override bool LaunchViaUFE { get { return UnrealBuildTool.BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac; } }

	public override bool UseAbsLog
	{
		get
		{
			return !LaunchViaUFE;
		}
	}

	public override bool RemapFileType(StagedFileType FileType)
	{
		return (
			FileType == StagedFileType.UFS || 
			FileType == StagedFileType.NonUFS ||
			FileType == StagedFileType.DebugNonUFS);
	}

	public override StagedFileReference Remap(StagedFileReference Dest)
	{
		return new StagedFileReference("cookeddata/" + Dest.Name);
	}
	public override List<string> GetDebugFileExtensions()
	{
		return new List<string> { ".dsym", ".udebugsymbols" };
	}
	public override IProcessResult RunClient(ERunOptions ClientRunFlags, string ClientApp, string ClientCmdLine, ProjectParams Params)
	{
		if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac || UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
		{
			if (Params.Devices.Count != 1)
			{
				throw new AutomationException("Can only run on a single specified device, but {0} were specified", Params.Devices.Count);
			}

			string PlistFile;
			string BundleIdentifier = "";

			// Params.StageDirectory will be something like /.../Saved/StagedBuilds
			// ClientApp will be something like /.../Saved/StagedBuilds/IOSClient/QAGame/Binaries/IOS/QAGameClient
			// so we want the first directory in the CLientApp after Params.StageDirectory	

			// Currently (2023/09), Remote Mac builds from a Windows System are forced to Legacy mode and uses a different ClientApp value
			if (OperatingSystem.IsMacOS())
			{
				string TargetStageDir = ClientApp.Substring(0, ClientApp.IndexOf('/', Params.BaseStageDirectory.Length + 1));
				if (AppleExports.UseModernXcode(Params.RawProjectPath))
				{
					// modern mode runs from the staged dir
					PlistFile = Path.Combine(TargetStageDir, Path.GetFileNameWithoutExtension(ClientApp) + ".app", "Info.plist");
				}
				else
				{
					PlistFile = Path.Combine(TargetStageDir, "Info.plist");
				}
			}
			else
			{
				PlistFile = Path.Combine(Params.BaseStageDirectory, PlatformName);
				PlistFile = Path.Combine(PlistFile, "Info.plist");
			}

			Logger.LogWarning("LOOKING IN PLIST {PListfile}", PlistFile);
			Logger.LogWarning("ClientApp is {CLient}", ClientApp);
			Logger.LogWarning("BaseStageDir is is {stage}", Params.BaseStageDirectory);

			if (File.Exists(PlistFile))
			{
				string Contents = File.ReadAllText(PlistFile);
				int Pos = Contents.IndexOf("CFBundleIdentifier");
				Pos = Contents.IndexOf("<string>", Pos) + 8;
				int EndPos = Contents.IndexOf("</string>", Pos);
				BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
			}

			string Program = GetPathToLibiMobileDeviceTool("idevicedebug"); ;
			string Arguments = " -u '" + Params.DeviceNames[0] + "'";
			Arguments += " --detach";
			Arguments = GetLibimobileDeviceNetworkedArgument(Arguments, Params.DeviceNames[0]);
			Arguments += " run '" + BundleIdentifier + "'";
			
			// ClientCmdLine is only relevant when running on a Mac
			if (OperatingSystem.IsMacOS())
			{
				Arguments += " " + ClientCmdLine;
			}

			IProcessResult ClientProcess = Run(Program, Arguments, null, ClientRunFlags);
			if (ClientProcess.ExitCode == -1)
			{
				Console.WriteLine("The application {0} has been installed on the device {1} but it cannot be launched automatically because the device does not contain the required developer software. You can launch {0} the manually by clicking its icon on the device.", BundleIdentifier, Params.DeviceNames[0]);
				Console.WriteLine("To install the developer software tools, connect it to a Mac running Xcode, open the Devices and Simulators window and wait for the tools to be installed.");
				IProcessResult Result = new ProcessResult("DummyApp", null, false);
				Result.ExitCode = 0;
				return Result;

			}
			return ClientProcess;

		}
		else
		{
			IProcessResult Result = new ProcessResult("DummyApp", null, false);
			Result.ExitCode = 0;
			return Result;
		}
	}

	private static int GetChunkCount(ProjectParams Params, DeploymentContext SC)
	{
		var ChunkListFilename = GetChunkPakManifestListFilename(Params, SC);
		var ChunkArray = ReadAllLines(ChunkListFilename);
		return ChunkArray.Length;
	}

	private static string GetChunkPakManifestListFilename(ProjectParams Params, DeploymentContext SC)
	{
		return CombinePaths(GetTmpPackagingPath(Params, SC), "pakchunklist.txt");
	}

	private static string GetTmpPackagingPath(ProjectParams Params, DeploymentContext SC)
	{
		return CombinePaths(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Saved", "TmpPackaging", SC.StageTargetPlatform.GetCookPlatform(SC.DedicatedServer, false));
	}

	private static StringBuilder AppendKeyValue(StringBuilder Text, string Key, object Value, int Level)
	{
		// create indent level
		string Indent = "";
		for (int i = 0; i < Level; ++i)
		{
			Indent += "\t";
		}

		// output key if we have one
		if (Key != null)
		{
			Text.AppendLine(Indent + "<key>" + Key + "</key>");
		}

		// output value
		if (Value is Array)
		{
			Text.AppendLine(Indent + "<array>");
			Array ValArray = Value as Array;
			foreach (var Item in ValArray)
			{
				AppendKeyValue(Text, null, Item, Level + 1);
			}
			Text.AppendLine(Indent + "</array>");
		}
		else if (Value is Dictionary<string, object>)
		{
			Text.AppendLine(Indent + "<dict>");
			Dictionary<string, object> ValDict = Value as Dictionary<string, object>;
			foreach (var Item in ValDict)
			{
				AppendKeyValue(Text, Item.Key, Item.Value, Level + 1);
			}
			Text.AppendLine(Indent + "</dict>");
		}
		else if (Value is string)
		{
			Text.AppendLine(Indent + "<string>" + Value + "</string>");
		}
		else if (Value is bool)
		{
			if ((bool)Value == true)
			{
				Text.AppendLine(Indent + "<true/>");
			}
			else
			{
				Text.AppendLine(Indent + "<false/>");
			}
		}
		else
		{
			Console.WriteLine("PLIST: Unknown array item type");
		}
		return Text;
	}

	private static void GeneratePlist(Dictionary<string, object> KeyValues, string PlistFile)
	{
		// generate the plist file
		StringBuilder Text = new StringBuilder();

		// boiler plate top
		Text.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
		Text.AppendLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
		Text.AppendLine("<plist version=\"1.0\">");
		Text.AppendLine("<dict>");

		foreach (var KeyValue in KeyValues)
		{
			AppendKeyValue(Text, KeyValue.Key, KeyValue.Value, 1);
		}
		Text.AppendLine("</dict>");
		Text.AppendLine("</plist>");

		// write the file out
		if (!Directory.Exists(Path.GetDirectoryName(PlistFile)))
		{
			Directory.CreateDirectory(Path.GetDirectoryName(PlistFile));
		}
		File.WriteAllText(PlistFile, Text.ToString());
	}

	private static void GenerateAssetPlist(string BundleIdentifier, string[] Tags, string AssetDir)
	{
		Dictionary<string, object> KeyValues = new Dictionary<string, object>();
		KeyValues.Add("CFBundleIdentifier", BundleIdentifier);
		KeyValues.Add("Tags", Tags);
		GeneratePlist(KeyValues, CombinePaths(AssetDir, "Info.plist"));
	}

	private static void GenerateAssetPackManifestPlist(KeyValuePair<string, string>[] ChunkData, string AssetDir)
	{
		Dictionary<string, object>[] Resources = new Dictionary<string, object>[ChunkData.Length];
		for (int i = 0; i < ChunkData.Length; ++i)
		{
			Dictionary<string, object> Data = new Dictionary<string, object>();
			Data.Add("URL", CombinePaths("OnDemandResources", ChunkData[i].Value));
			Data.Add("bundleKey", ChunkData[i].Key);
			Data.Add("isStreamable", false);
			Resources[i] = Data;
		}

		Dictionary<string, object> KeyValues = new Dictionary<string, object>();
		KeyValues.Add("resources", Resources);
		GeneratePlist(KeyValues, CombinePaths(AssetDir, "AssetPackManifest.plist"));
	}

	private static void GenerateOnDemandResourcesPlist(KeyValuePair<string, string>[] ChunkData, string AssetDir)
	{
		Dictionary<string, object> RequestTags = new Dictionary<string, object>();
		Dictionary<string, object> AssetPacks = new Dictionary<string, object>();
		Dictionary<string, object> Requests = new Dictionary<string, object>();
		for (int i = 0; i < ChunkData.Length; ++i)
		{
			string ChunkName = "Chunk" + (i + 1).ToString();
			RequestTags.Add(ChunkName, new string[] { ChunkData[i].Key });
			AssetPacks.Add(ChunkData[i].Key, new string[] { ("pak" + ChunkName + "-ios.pak").ToLowerInvariant() });
			Dictionary<string, object> Packs = new Dictionary<string, object>();
			Packs.Add("NSAssetPacks", new string[] { ChunkData[i].Key });
			Requests.Add(ChunkName, Packs);
		}

		Dictionary<string, object> KeyValues = new Dictionary<string, object>();
		KeyValues.Add("NSBundleRequestTags", RequestTags);
		KeyValues.Add("NSBundleResourceRequestAssetPacks", AssetPacks);
		KeyValues.Add("NSBundleResourceRequestTags", Requests);
		GeneratePlist(KeyValues, CombinePaths(AssetDir, "OnDemandResources.plist"));
	}

	public override void PostStagingFileCopy(ProjectParams Params, DeploymentContext SC)
	{
		/*		if (Params.CreateChunkInstall)
				{
					// get the bundle identifier
					string BundleIdentifier = "";
					if (File.Exists(Params.BaseStageDirectory + "/" + PlatformName + "/Info.plist"))
					{
						string Contents = File.ReadAllText(SC.StageDirectory + "/Info.plist");
						int Pos = Contents.IndexOf("CFBundleIdentifier");
						Pos = Contents.IndexOf("<string>", Pos) + 8;
						int EndPos = Contents.IndexOf("</string>", Pos);
						BundleIdentifier = Contents.Substring(Pos, EndPos - Pos);
					}

					// generate the ODR resources
					// create the ODR directory
					string DestSubdir = SC.StageDirectory + "/OnDemandResources";
					if (!Directory.Exists(DestSubdir))
					{
						Directory.CreateDirectory(DestSubdir);
					}

					// read the chunk list and generate the data
					var ChunkCount = GetChunkCount(Params, SC);
					var ChunkData = new KeyValuePair<string, string>[ChunkCount - 1];
					for (int i = 1; i < ChunkCount; ++i)
					{
						// chunk name
						string ChunkName = "Chunk" + i.ToString ();

						// asset name
						string AssetPack = BundleIdentifier + ".Chunk" + i.ToString () + ".assetpack";

						// bundle key
						byte[] bytes = new byte[ChunkName.Length * sizeof(char)];
						System.Buffer.BlockCopy(ChunkName.ToCharArray(), 0, bytes, 0, bytes.Length);
						string BundleKey = BundleIdentifier + ".asset-pack-" + BitConverter.ToString(System.Security.Cryptography.MD5.Create().ComputeHash(bytes)).Replace("-", string.Empty);

						// add to chunk data
						ChunkData[i-1] = new KeyValuePair<string, string>(BundleKey, AssetPack);

						// create the sub directory
						string AssetDir = CombinePaths (DestSubdir, AssetPack);
						if (!Directory.Exists(AssetDir))
						{
							Directory.CreateDirectory(AssetDir);
						}

						// generate the Info.plist for each ODR bundle (each chunk for install past 0)
						GenerateAssetPlist (BundleKey, new string[] { ChunkName }, AssetDir);

						// copy the files to the OnDemandResources directory
						string PakName = "pakchunk" + i.ToString ();
						string FileName =  PakName + "-" + PlatformName.ToLower() + ".pak";
						string P4Change = "UnknownCL";
						string P4Branch = "UnknownBranch";
						if (CommandUtils.P4Enabled)
						{
							P4Change = CommandUtils.P4Env.ChangelistString;
							P4Branch = CommandUtils.P4Env.BuildRootEscaped;
						}
						string ChunkInstallBasePath = CombinePaths(SC.ProjectRoot.FullName, "ChunkInstall", SC.FinalCookPlatform);
						string RawDataPath = CombinePaths(ChunkInstallBasePath, P4Branch + "-CL-" + P4Change, PakName);
						string RawDataPakPath = CombinePaths(RawDataPath, PakName + "-" + SC.FinalCookPlatform + ".pak");
						string DestFile = CombinePaths (AssetDir, FileName);
						CopyFile (RawDataPakPath, DestFile);
					}

					// generate the AssetPackManifest.plist
					GenerateAssetPackManifestPlist (ChunkData, SC.StageDirectory.FullName);

					// generate the OnDemandResources.plist
					GenerateOnDemandResourcesPlist (ChunkData, SC.StageDirectory.FullName);
				}*/

		base.PostStagingFileCopy(Params, SC);
	}

	public override bool RequiresPackageToDeploy(ProjectParams Params)
	{
		return !AppleExports.UseModernXcode(Params.RawProjectPath);
	}

	public override HashSet<StagedFileReference> GetFilesForCRCCheck()
	{
		HashSet<StagedFileReference> FileList = base.GetFilesForCRCCheck();
		FileList.Add(new StagedFileReference("Info.plist"));
		return FileList;
	}
	public override bool SupportsMultiDeviceDeploy
	{
		get
		{
			return true;
		}
	}

	public override void StripSymbols(FileReference SourceFile, FileReference TargetFile)
	{
		IOSExports.StripSymbols(PlatformType, SourceFile, TargetFile, Log.Logger);
	}


	private void WriteEntitlements(ProjectParams Params, DeploymentContext SC)
	{
		// game name
		string AppName = SC.IsCodeBasedProject ? SC.StageExecutables[0].Split("-".ToCharArray())[0] : "UnrealGame";

		// mobile provisioning file
		DirectoryReference MobileProvisionDir;
		if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
		{
			MobileProvisionDir = DirectoryReference.Combine(new DirectoryReference(Environment.GetEnvironmentVariable("HOME")), "Library", "MobileDevice", "Provisioning Profiles");
		}
		else
		{
			MobileProvisionDir = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData), "Apple Computer", "MobileDevice", "Provisioning Profiles");
		}

		FileReference MobileProvisionFile = null;
		
		if(MobileProvisionDir != null && Params.Provision != null)
		{
			MobileProvisionFile = FileReference.Combine(MobileProvisionDir, Params.Provision);
		}

		// distribution build
		bool bForDistribution = Params.Distribution;

		// intermediate directory
		string IntermediateDir = SC.ProjectRoot + "/Intermediate/" + (TargetPlatformType == UnrealTargetPlatform.IOS ? "IOS" : "TVOS");

		//	entitlements file name
		string OutputFilename = Path.Combine(IntermediateDir, AppName + ".entitlements");

		// ios configuration from the ini file
		ConfigHierarchy PlatformGameConfig;
		if (Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformGameConfig))
		{
			IOSExports.WriteEntitlements(TargetPlatformType, PlatformGameConfig, AppName, MobileProvisionFile, bForDistribution, IntermediateDir);
		}
	}

	public override DirectoryReference GetProjectRootForStage(DirectoryReference RuntimeRoot, StagedDirectoryReference RelativeProjectRootForStage)
	{
		return DirectoryReference.Combine(RuntimeRoot, "cookeddata/" + RelativeProjectRootForStage.Name);
	}

	public override void SetSecondaryRemoteMac(string ProjectFilePath, string ClientPlatform)
	{
		PrepareForDebugging("", ProjectFilePath, ClientPlatform);
		IOSExports.SetSecondaryRemoteMac(ClientPlatform, new FileReference(ProjectFilePath), Log.Logger);
	}

	public override void PrepareForDebugging(string SourcePackage, string ProjectFilePath, string ClientPlatform)
	{
		Logger.LogInformation("Preparing for Debug ...");
		Logger.LogInformation("SourcePackage : {SourcePackage}", SourcePackage);
		Logger.LogInformation("ProjectFilePath : {SourcePackage}", ProjectFilePath);
		Logger.LogInformation("ClientPlatform : {ClientPlatform}", ClientPlatform);

		if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64 || HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Mac)
		{
			int StartPos = 0;
			if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
			{
				StartPos = ProjectFilePath.LastIndexOf('/');
			}
			else
			{
				StartPos = ProjectFilePath.LastIndexOf(Path.DirectorySeparatorChar);
			}
			int StringLength = ProjectFilePath.Length - 10; // 9 for .uproject, 1 for the /
    		string PackageName = ProjectFilePath.Substring(StartPos + 1, StringLength - StartPos);
			string PackagePath = Path.Combine(Path.GetDirectoryName(ProjectFilePath), "Binaries", ClientPlatform);
			Logger.LogInformation("PackagePath : {PackagePath}", PackagePath);
			if (string.IsNullOrEmpty(SourcePackage))
    		{
				SourcePackage = Path.Combine(PackagePath, PackageName + ".ipa");
			}

			string[] IPAFiles;
			if (!File.Exists(SourcePackage))
            {
				IPAFiles = Directory.GetFiles(PackagePath, "*.ipa");
				Logger.LogWarning("Source package not found : {SourcePackage}, trying to find another IPA file in the same folder.", SourcePackage);
				if (IPAFiles.Length == 0)
                {
					Logger.LogError("No IPA file found in : {PackagePath}. Aborting.", PackagePath);
					throw new AutomationException(ExitCode.Error_MissingExecutable, "No IPA file found in {0}.", PackagePath);
				}
				else
                {
					if (IPAFiles.Length > 1)
                    {
						Logger.LogWarning("More than one IPA file found. Taking the first one found, {Arg0}", IPAFiles[0]);
					}
					SourcePackage = IPAFiles[0];
				}					

			}
			string ZipFile = Path.ChangeExtension(SourcePackage, "zip");

			string PayloadPath = SourcePackage;
			Logger.LogInformation("PayloadPath : {PayloadPath}", PayloadPath);
			if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
			{
				PayloadPath = PayloadPath.Substring(0, PayloadPath.LastIndexOf('\\'));
			}
			else
			{
				PayloadPath = PayloadPath.Substring(0, PayloadPath.LastIndexOf('/'));
			}
			string CookedDataDirectory = Path.Combine(Path.GetDirectoryName(PayloadPath), ClientPlatform, "Payload", PackageName + ".app", "cookeddata");

			Logger.LogInformation("ClientPlatform : {ClientPlatform}", ClientPlatform);
			Logger.LogInformation("ProjectFilePath : {ProjectFilePath}", ProjectFilePath);
			Logger.LogInformation("Source : {SourcePackage}", SourcePackage);
			Logger.LogInformation("ZipFile {ZipFile}", ZipFile);
			Logger.LogInformation("PackageName {PackageName}", PackageName);
			Logger.LogInformation("PayloadPath {PayloadPath}", PayloadPath);

    		if (File.Exists(ZipFile))
    		{
    			Logger.LogInformation("Deleting previously present ZIP file created from IPA");
    			File.Delete(ZipFile);
    		}

    		File.Copy(SourcePackage, ZipFile);
    		UnzipPackage(ZipFile);

    		if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
    		{
    			IOSExports.PrepareRemoteMacForDebugging(CookedDataDirectory, new FileReference(ProjectFilePath), Log.Logger);
    		}
    		else
        	{
        		string ProjectPath = ProjectFilePath;
        		if (Directory.Exists(Path.Combine(ProjectPath, "Binaries", ClientPlatform, "Payload", PackageName, ".app")))
        		{
        			CopyDirectory_NoExceptions(CookedDataDirectory, Path.Combine(ProjectPath, "Binaries", ClientPlatform, "Payload", PackageName, ".app/cookeddata/"), true);
        		}
        		else
        		{
        			string ProjectRoot = SourcePackage;
        			ProjectRoot = ProjectRoot.Substring(0, ProjectRoot.LastIndexOf('/'));
        			CopyFile(SourcePackage, ProjectRoot + "/Binaries/" + ClientPlatform + "/Payload/" + PackageName + ".ipa", true);
        		}
			}
			//cleanup
			Logger.LogInformation("Deleting temp files ...");
			File.Delete(ZipFile);
			Logger.LogInformation("{ZipFile} deleted", ZipFile);
		}
		else
		{
			Logger.LogInformation("Wrangling data for debug for an iOS/tvOS app for XCode is a Mac and Windows (Remote) only feature. Aborting command.");
			return;
		}
	}

	public void UnzipPackage(string PackageToUnzip)
	{
		Logger.LogInformation("PackageToUnzip : {PackageToUnzip}", PackageToUnzip);
		string UnzipPath = PackageToUnzip;
		if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
		{
			UnzipPath = UnzipPath.Substring(0, UnzipPath.LastIndexOf('\\'));
		}
		else
		{
			UnzipPath = UnzipPath.Substring(0, UnzipPath.LastIndexOf(Path.DirectorySeparatorChar));
		}
		Logger.LogInformation("Unzipping to {UnzipPath}", UnzipPath);

		using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile(PackageToUnzip))
		{
			foreach (Ionic.Zip.ZipEntry Entry in Zip.Entries.Where(x => !x.IsDirectory))
			{
				string OutputFileName = Path.Combine(UnzipPath, Entry.FileName);
				Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName));
				using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
				{
					Entry.Extract(OutputStream);
				}
				Logger.LogInformation("Extracted {OutputFileName}", OutputFileName);
			}
		}
	}
}
