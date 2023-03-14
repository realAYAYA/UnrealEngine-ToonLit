// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System.Collections.Generic;
using System.Linq;

namespace UnrealBuildTool
{
	partial class MicrosoftPlatformSDK : UEBuildPlatformSDK
	{
		/// <summary>
		/// The default Windows SDK version to be used, if installed.
		/// </summary>
		static readonly VersionNumber[] PreferredWindowsSdkVersions = new VersionNumber[]
		{
			VersionNumber.Parse("10.0.18362.0")
		};

		/// <summary>
		/// The default compiler version to be used, if installed. 
		/// </summary>
		static readonly VersionNumberRange[] PreferredClangVersions =
		{
			VersionNumberRange.Parse("13.0.0", "13.999"),
		};

		static readonly VersionNumber MinimumClangVersion = new VersionNumber(13, 0, 0);

		/// <summary>
		/// Ranges of tested compiler toolchains to be used, in order of preference. If multiple toolchains in a range are present, the latest version will be preferred.
		/// Note that the numbers here correspond to the installation *folders* rather than precise executable versions. 
		/// </summary>
		static readonly VersionNumberRange[] PreferredVisualCppVersions = new VersionNumberRange[]
		{
			VersionNumberRange.Parse("14.29.30133", "14.29.99999"), // VS2019 16.11.x
			VersionNumberRange.Parse("14.33.31629", "14.33.99999"), // VS2022 17.3.x
		};

		/// <summary>
		/// Tested compiler toolchains that should not be allowed.
		/// </summary>
		static readonly VersionNumberRange[] BannedVisualCppVersions = new VersionNumberRange[]
		{
		};

		static readonly VersionNumber MinimumVisualCppVersion = new VersionNumber(14, 29, 30133);

		/// <summary>
		/// The default compiler version to be used, if installed. 
		/// </summary>
		static readonly VersionNumberRange[] PreferredIntelOneApiVersions =
		{
			VersionNumberRange.Parse("2022.0.0", "2022.9999"),
		};

		static readonly VersionNumber MinimumIntelOneApiVersion = new VersionNumber(2021, 0, 0);

		/// <summary>
		/// The default set of components that should be suggested to be installed for Visual Studio 2019 or 2022.
		/// This or the 2019\2022 specific components should be updated if the preferred visual cpp version changes
		/// </summary>
		static readonly string[] VisualStudioSuggestedComponents = new string[]
		{
			"Microsoft.VisualStudio.Workload.CoreEditor",
			"Microsoft.VisualStudio.Workload.Universal",
			"Microsoft.VisualStudio.Workload.NativeDesktop",
			"Microsoft.VisualStudio.Workload.NativeGame",
			"Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
			"Microsoft.VisualStudio.Component.VC.Tools.ARM64",
			"Microsoft.VisualStudio.Component.Windows10SDK",
		};

		/// <summary>
		/// Additional set of components that should be suggested to be installed for Visual Studio 2019.
		/// </summary>
		static readonly string[] VisualStudio2019SuggestedComponents = new string[]
		{
		};

		/// <summary>
		/// Additional set of components that should be suggested to be installed for Visual Studio 2022.
		/// </summary>
		static readonly string[] VisualStudio2022SuggestedComponents = new string[]
		{
			"Microsoft.VisualStudio.Workload.ManagedDesktop",
			"Microsoft.VisualStudio.Workload.NativeCrossPlat",
			"Microsoft.VisualStudio.Component.VC.14.33.17.3.ARM64",
			"Microsoft.VisualStudio.Component.VC.14.33.17.3.x86.x64",
			"Microsoft.Net.Component.4.6.2.TargetingPack",
		};

		/// <summary>
		/// Returns the list of suggested of components that should be suggested to be installed for Visual Studio.
		/// Used to generate a .vsconfig file which will prompt Visual Studio to ask the user to install these components.
		/// </summary>
		public static IEnumerable<string> GetVisualStudioSuggestedComponents(VCProjectFileFormat Format)
		{
			SortedSet<string> Components = new SortedSet<string>();
			Components.UnionWith(VisualStudioSuggestedComponents);

			switch (Format)
			{
				case VCProjectFileFormat.VisualStudio2019:
					Components.UnionWith(VisualStudio2019SuggestedComponents);
					break;
				case VCProjectFileFormat.VisualStudio2022:
					Components.UnionWith(VisualStudio2022SuggestedComponents);
					break;
				default:
					throw new BuildException("Unsupported Visual Studio version");
			}

			return Components;
		}

		public override string GetMainVersion()
		{
			// preferred/main version is the top of the Preferred list - 
			return PreferredWindowsSdkVersions.First().ToString();
		}

		protected override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			MinVersion = "10.0.00000.0";
			MaxVersion = "10.9.99999.0";
		}

		protected override void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			// minimum version is the oldest version in the Preferred list -
			MinVersion = PreferredWindowsSdkVersions.Min()?.ToString();
			MaxVersion = null;
		}

		/// <summary>
		/// Whether toolchain errors should be ignored. Enable to ignore banned toolchains when generating projects,
		/// as components such as the recommended toolchain can be installed by opening the generated solution via the .vsconfig file.
		/// If enabled the error will be downgraded to a warning.
		/// </summary>
		public static bool IgnoreToolchainErrors = false;
	}
}
