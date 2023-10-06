// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using EpicGames.Core;

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
		/// The minimum Windows SDK version to be used. If this is null then it means there is no minimum version
		/// </summary>
		static readonly VersionNumber? MinimumWindowsSDKVersion = new VersionNumber(10, 0, 18362, 0);

		/// <summary>
		/// The maximum Windows SDK version to be used. If this is null then it means "Latest"
		/// </summary>
		static readonly VersionNumber? MaximumWindowsSDKVersion = null;

		/// <summary>
		/// The default compiler version to be used, if installed. 
		/// </summary>
		static readonly VersionNumberRange[] PreferredClangVersions =
		{
			VersionNumberRange.Parse("16.0.0", "16.999"), // VS2022 17.7.x runtime requires Clang 16
			VersionNumberRange.Parse("15.0.0", "15.999"), // VS2022 17.5.x runtime requires Clang 15
		};

		/// <summary>
		/// The minimum supported Clang compiler
		/// </summary>
		static readonly VersionNumber MinimumClangVersion = new VersionNumber(15, 0, 0);

		/// <summary>
		/// Ranges of tested compiler toolchains to be used, in order of preference. If multiple toolchains in a range are present, the latest version will be preferred.
		/// Note that the numbers here correspond to the installation *folders* rather than precise executable versions. 
		/// </summary>
		static readonly VersionNumberRange[] PreferredVisualCppVersions = new VersionNumberRange[]
		{
			VersionNumberRange.Parse("14.36.32532", "14.36.99999"), // VS2022 17.6.x
			VersionNumberRange.Parse("14.35.32215", "14.35.99999"), // VS2022 17.5.x
			VersionNumberRange.Parse("14.34.31933", "14.34.99999"), // VS2022 17.4.x
			VersionNumberRange.Parse("14.29.30133", "14.29.99999"), // VS2019 16.11.x
		};

		/// <summary>
		/// Minimum Clang version required for MSVC toolchain versions
		/// </summary>
		static readonly IReadOnlyDictionary<VersionNumber, VersionNumber> MinimumRequiredClangVersion = new Dictionary<VersionNumber, VersionNumber>()
		{
			{ new VersionNumber(14, 37), new VersionNumber(16) }, // VS2022 17.7.x
			{ new VersionNumber(14, 35), new VersionNumber(15) }, // VS2022 17.5.x - 17.6.x
			{ new VersionNumber(14, 34), new VersionNumber(14) }, // VS2022 17.4.x
			{ new VersionNumber(14, 29), new VersionNumber(13) }, // VS2019 16.11.x

		};

		/// <summary>
		/// Tested compiler toolchains that should not be allowed.
		/// </summary>
		static readonly VersionNumberRange[] BannedVisualCppVersions = new VersionNumberRange[]
		{
			VersionNumberRange.Parse("14.30.0", "14.33.99999"), // VS2022 17.0.x - 17.3.x
		};

		/// <summary>
		/// The minimum supported MSVC compiler
		/// </summary>
		static readonly VersionNumber MinimumVisualCppVersion = new VersionNumber(14, 29, 30133);

		/// <summary>
		/// The default compiler version to be used, if installed. 
		/// https://www.intel.com/content/www/us/en/developer/articles/tool/oneapi-standalone-components.html#dpcpp-cpp
		/// </summary>
		static readonly VersionNumberRange[] PreferredIntelOneApiVersions =
		{
			VersionNumberRange.Parse("2023.1.0", "2023.9999"),
		};

		/// <summary>
		/// The minimum supported Intel compiler
		/// </summary>
		static readonly VersionNumber MinimumIntelOneApiVersion = new VersionNumber(2023, 0, 0);

		/// <inheritdoc/>
		protected override string GetMainVersionInternal()
		{
			// preferred/main version is the top of the Preferred list - 
			return PreferredWindowsSdkVersions.First().ToString();
		}

		/// <inheritdoc/>
		protected override void GetValidVersionRange(out string MinVersion, out string MaxVersion)
		{
			MinVersion = "10.0.00000.0";
			MaxVersion = "10.9.99999.0";
		}

		/// <inheritdoc/>
		protected override void GetValidSoftwareVersionRange(out string? MinVersion, out string? MaxVersion)
		{
			MinVersion = MinimumWindowsSDKVersion?.ToString();
			MaxVersion = MaximumWindowsSDKVersion?.ToString();
		}

		/// <summary>
		/// The minimum supported Clang version for a given MSVC toolchain
		/// </summary>
		/// <param name="VcVersion"></param>
		/// <returns></returns>
		public static VersionNumber GetMinimumClangVersionForVcVersion(VersionNumber VcVersion)
		{
			foreach (KeyValuePair<VersionNumber, VersionNumber> Item in MinimumRequiredClangVersion)
			{
				if (VcVersion >= Item.Key)
				{
					return Item.Value;
				}
			}
			return MinimumClangVersion;
		}

		/// <summary>
		/// The base Clang version for a given Intel toolchain
		/// </summary>
		/// <param name="IntelCompilerPath"></param>
		/// <returns></returns>
		public static VersionNumber GetClangVersionForIntelCompiler(FileReference IntelCompilerPath)
		{
			FileReference LdLLdPath = FileReference.Combine(IntelCompilerPath.Directory, "..", "bin-llvm", "ld.lld.exe");
			if (FileReference.Exists(LdLLdPath))
			{
				FileVersionInfo VersionInfo = FileVersionInfo.GetVersionInfo(LdLLdPath.FullName);
				VersionNumber Version = new VersionNumber(VersionInfo.FileMajorPart, VersionInfo.FileMinorPart, VersionInfo.FileBuildPart);
				return Version;
			}

			return MinimumClangVersion;
		}

		/// <summary>
		/// Whether toolchain errors should be ignored. Enable to ignore banned toolchains when generating projects,
		/// as components such as the recommended toolchain can be installed by opening the generated solution via the .vsconfig file.
		/// If enabled the error will be downgraded to a warning.
		/// </summary>
		public static bool IgnoreToolchainErrors = false;
	}
}
