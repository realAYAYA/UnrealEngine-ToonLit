// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using Microsoft.Win32;

using System.Threading;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System;

[SupportedPlatforms("Win64")]
public abstract class DatasmithSolidworksBaseTarget : TargetRules
{
	// tries to fetch Solidworks' installation folder from registry
	//
	private string CheckSolidworksInstalledSub(string swKey)
	{
		string result = "";

		if (OperatingSystem.IsWindows())
		{
			RegistryKey lKey202x = Registry.LocalMachine.OpenSubKey("SOFTWARE\\SolidWorks\\" + swKey + "\\Setup");
			if (lKey202x != null)
			{
				string[] names = lKey202x.GetValueNames();
				object value = null;
				foreach (var nn in names)
				{
					if (nn.ToUpper() == "SOLIDWORKS FOLDER")
					{
						value = lKey202x.GetValue("SolidWorks Folder");
						break;
					}
				}
				if (value != null)
				{
					string fullPath = Path.Combine(value as string, "solidworkstools.dll");
					if (File.Exists(fullPath))
					{
						result = Path.GetDirectoryName(fullPath);
					}
				}
			}
		}
		return result;
	}

	public DatasmithSolidworksBaseTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		SolutionDirectory = "Programs/Datasmith";
		bBuildInSolutionByDefault = false;
		ExeBinariesSubFolder = "Solidworks/" + GetVersionShort();
		bShouldCompileAsDLL = true;
		LinkType = TargetLinkType.Monolithic;
		bBuildDeveloperTools = false;
		bBuildWithEditorOnlyData = true;
		bCompileAgainstEngine = false;
		bCompileAgainstCoreUObject = false;
		bCompileICU = false;
		bUsesSlate = false;

		string ProjectName = "DatasmithSolidworks";
		LaunchModuleName = ProjectName + GetVersionShort();

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			string SolidworksSDKPath = CheckSolidworksInstalledSub(GetVersionLong());
			if (!Directory.Exists(SolidworksSDKPath))
			{
				// Try with build machine setup
				string SDKRootEnvVar = System.Environment.GetEnvironmentVariable("UE_SDKS_ROOT");
				if (SDKRootEnvVar != null && SDKRootEnvVar != "")
				{
					SolidworksSDKPath = Path.Combine(SDKRootEnvVar, "HostWin64", "Win64", "Solidworks", GetVersionShort());
				}
			}

			// Define post-build step
			// Since the Datasmith Solidworks Exporter is a C# project, build in batch the release configuration of the Visual Studio C# project file.
			string Config = "Release";
			string SolidworksExporterPath = @"$(EngineDir)\Source\Programs\Enterprise\Datasmith\DatasmithSolidworksExporter";
			string ProjectFile = Path.Combine(SolidworksExporterPath, ProjectName, ProjectName+".csproj");

			string BuildCommand = string.Format(@"$(EngineDir)\Build\BatchFiles\MSBuild.bat /t:Build /p:Configuration={1} /p:EngineDir=""$(EngineDir)"" /p:ExternalAssemblies=""{2}"" ""{0}""",
				ProjectFile, Config, SolidworksSDKPath);

			PostBuildSteps.Add(string.Format(@"echo BuildCommand: {0}", BuildCommand));
			PostBuildSteps.Add(BuildCommand);
		}
	}

	public abstract string GetVersionLong();
	public abstract string GetVersionShort();
}

public class DatasmithSolidworks2021Target : DatasmithSolidworksBaseTarget
{
	public DatasmithSolidworks2021Target(TargetInfo Target)
		: base(Target)
	{
	}

	public override string GetVersionLong() { return "SOLIDWORKS 2021"; }
	public override string GetVersionShort() { return "2021"; }
}
