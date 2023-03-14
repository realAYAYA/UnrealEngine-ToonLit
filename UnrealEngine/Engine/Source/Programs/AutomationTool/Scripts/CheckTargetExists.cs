// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildTool;

namespace AutomationTool
{
	[Help("Checks that the given target exists, by looking for the relevant receipt.")]
	[Help("-Target", "Name of the target to check")]
	[Help("-Platform", "Platform the target was built for")]
	[Help("-Configuration", "Configuration for the target")]
	[Help("-Architecture", "Architecture that the target was built for")]
	[Help("-Project", "Path to the project containing the target")]
	class CheckTargetExists : BuildCommand
	{
		public override void ExecuteBuild()
		{
			// Parse the target name
			string Target = ParseParamValue("Target");
			if(Target == null)
			{
				throw new AutomationException("Missing -Target=... argument");
			}

			// Parse the platform
			string PlatformParam = ParseParamValue("Platform");
			if(PlatformParam == null)
			{
				throw new AutomationException("Missing -Platform=... argument");
			}
			UnrealTargetPlatform Platform;
			if (!UnrealTargetPlatform.TryParse(PlatformParam, out Platform))
			{
				throw new AutomationException("Invalid platform '{0}'", PlatformParam);
			}

			// Parse the configuration
			string ConfigurationParam = ParseParamValue("Configuration");
			if(ConfigurationParam == null)
			{
				throw new AutomationException("Missing -Configuration=... argument");
			}
			UnrealTargetConfiguration Configuration;
			if(!Enum.TryParse(ConfigurationParam, true, out Configuration))
			{
				throw new AutomationException("Invalid configuration '{0}'", ConfigurationParam);
			}

			// Parse the project
			string Project = ParseParamValue("Project");
			if(Project != null && !File.Exists(Project))
			{
				throw new AutomationException("Specified project file '{0}' was not found", Project);
			}

			// Parse the architecture
			string Architecture = ParseParamValue("Architecture");

			// Check the receipt exists
			DirectoryReference ProjectDir = null;
			if(Project != null)
			{
				ProjectDir = new FileReference(Project).Directory;
			}
			FileReference ReceiptFile = TargetReceipt.GetDefaultPath(ProjectDir, Target, Platform, Configuration, Architecture);
			if(!FileReference.Exists(ReceiptFile))
			{
				throw new AutomationException("FortniteEditor receipt not found ({0})", ReceiptFile);
			}

			LogInformation("Found {0}", ReceiptFile);
		}
	}
}
