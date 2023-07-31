// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Text.RegularExpressions;
using EpicGames.Core;
using System.Linq;
using UnrealBuildBase;

class CheckCsprojDotNetVersion : BuildCommand
{
	public override void ExecuteBuild()
	{
		// just calling this DesiredTargetVersion because TargetVersion and TargetedVersion get super confusing.
		string DesiredTargetVersionParam = this.ParseParamValue("TargetVersion", null);
		if (string.IsNullOrEmpty(DesiredTargetVersionParam))
		{
			throw new AutomationException("-TargetVersion was not specified.");
		}

		string[] DesiredTargetVersions = DesiredTargetVersionParam.Split('+');

		CommandUtils.LogInformation("Scanning for all csproj's...");
		// Check for all csproj's in the engine dir
		DirectoryReference EngineDir = Unreal.EngineDirectory;

		// grab the targeted version.,
		Regex FrameworkRegex = new Regex("<TargetFrameworkVersion>v(\\d\\.\\d\\.?\\d?)<\\/TargetFrameworkVersion>");
		Regex PossibleAppConfigRegex = new Regex("<TargetFrameworkProfile>(.+)<\\/TargetFrameworkProfile>");
		Regex AppConfigRegex = new Regex("<supportedRuntime version=\"v(\\d\\.\\d\\.?\\d?)\" sku=\"\\.NETFramework,Version=v(\\d\\.\\d\\.?\\d?),Profile=(.+)\"\\/>");
        Regex DotNetCoreRegex = new Regex("<TargetFramework>(netcoreapp2.0|netstandard2.0)<\\/TargetFramework>");
        foreach (FileReference CsProj in DirectoryReference.EnumerateFiles(EngineDir, "*.csproj", SearchOption.AllDirectories))
        {
            if (CsProj.ContainsName("ThirdParty", EngineDir) ||
                (CsProj.ContainsName("UE4TemplateProject", EngineDir) && CsProj.GetFileName().Equals("ProjectTemplate.csproj")) ||
                CsProj.GetFileNameWithoutExtension().ToLower().Contains("_mono") ||
                CsProj.GetFileNameWithoutExtension().ToLower().Contains("unrealvs") ||
				CsProj.ContainsName("DatasmithRevitExporter", EngineDir) ||
				CsProj.ContainsName("DatasmithNavisworksExporter", EngineDir) ||
				CsProj.ContainsName("CSVTools", EngineDir))

            {
                continue;
            }

            // read in the file
            string Contents = File.ReadAllText(CsProj.FullName);
            Match Match = DotNetCoreRegex.Match(Contents);
            // Check if we're a _NETCore app, ignore these.
            if (Match.Success)
            {
                continue;
            }


            Match = FrameworkRegex.Match(Contents);
            if (Match.Success)
			{
				string TargetedVersion = Match.Groups[1].Value;
				// make sure we match, throw warning otherwise
				if (!DesiredTargetVersions.Any(DesiredTargetVersion => DesiredTargetVersion.Equals(TargetedVersion, StringComparison.InvariantCultureIgnoreCase)))
				{
					CommandUtils.LogWarning("Targeted Framework version for project: {0} was not {1}! Targeted Version: {2}", CsProj, String.Join("/", DesiredTargetVersions), TargetedVersion);
				}
			}
			// if we don't have a TargetFrameworkVersion, check for the existence of TargetFrameworkProfile.
			else
			{
                Match = PossibleAppConfigRegex.Match(Contents);
				if (!Match.Success)
				{
					CommandUtils.LogInformation("No TargetFrameworkVersion or TargetFrameworkProfile found for project {0}, is it a mono project? If not, does it compile properly?", CsProj);
					continue;
				}

				// look for the app config
				FileReference AppConfigFile = FileReference.Combine(CsProj.Directory, "app.config");
				string Profile = Match.Groups[1].Value;
				if (!FileReference.Exists(AppConfigFile))
				{
					CommandUtils.LogInformation("Found TargetFrameworkProfile but no associated app.config containing the version for project {0}.", CsProj);
					continue;
				}

				// read in the app config
				Contents = File.ReadAllText(AppConfigFile.FullName);
                Match = AppConfigRegex.Match(Contents);
				if (!Match.Success)
				{
					CommandUtils.LogInformation("Couldn't find a supportedRuntime match for the version in the app.config for project {0}.", CsProj);
					continue;
				}

				// Version1 is the one that appears right after supportedRuntime
				// Version2 is the one in the sku
				// ProfileString should match the TargetFrameworkProfile from the csproj
				string Version1String = Match.Groups[1].Value;
				string Version2String = Match.Groups[2].Value;
				string ProfileString = Match.Groups[3].Value;

				// not sure how this is possible, but check for it anyway
				if (!ProfileString.Equals(Profile, StringComparison.InvariantCultureIgnoreCase))
				{
					CommandUtils.LogWarning("The TargetFrameworkProfile in csproj {0} ({1}) doesn't match the sku in it's app.config ({2}).", CsProj, Profile, ProfileString);
					continue;
				}

				// if the version numbers don't match the app.config is probably corrupt.
				if (!Version1String.Equals(Version2String, StringComparison.InvariantCultureIgnoreCase))
				{
					CommandUtils.LogWarning("The supportedRunTimeVersion ({0}) and the sku version ({1}) in the app.config for project {2} don't match.", Version1String, Version2String, CsProj);
					continue;
				}

				// make sure the versions match
				if (!(DesiredTargetVersions.Any(DesiredTargetVersion => DesiredTargetVersion.Equals(Version1String, StringComparison.InvariantCultureIgnoreCase))))
				{
					CommandUtils.LogWarning("Targeted Framework version for project: {0} was not {1}! Targeted Version: {2}", CsProj, String.Join("/", DesiredTargetVersions), Version1String);
				}
			}
		}
	}
}