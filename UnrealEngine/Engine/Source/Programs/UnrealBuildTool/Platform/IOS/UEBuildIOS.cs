// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	partial struct UnrealArch
	{
		// @todo add x64 simulators to run on old macs?
		/// <summary>
		/// IOS Simulator
		/// </summary>
		public static UnrealArch IOSSimulator = FindOrAddByName("iossimulator", bIsX64: false);

		/// <summary>
		/// TVOS Simulator
		/// </summary>
		public static UnrealArch TVOSSimulator = FindOrAddByName("tvossimulator", bIsX64: false);

		private static IReadOnlyDictionary<UnrealArch, string> AppleToolchainArchitectures = new Dictionary<UnrealArch, string>()
		{
			{ UnrealArch.Arm64,         "arm64" },
			{ UnrealArch.X64,           "x86_64" },
			{ UnrealArch.IOSSimulator,  "arm64" },
			{ UnrealArch.TVOSSimulator, "arm64" },
		};

		/// <summary>
		/// Apple-specific low level name for the generic platforms
		/// </summary>
		public string AppleName
		{
			get
			{
				if (AppleToolchainArchitectures.ContainsKey(this))
				{
					return AppleToolchainArchitectures[this];
				}

				throw new BuildException($"Unknown architecture {ToString()} passed to UnrealArch.AppleName");
			}
		}
	}

	/// <summary>
	/// IOS-specific target settings
	/// </summary>
	public partial class IOSTargetRules
	{
		/// <summary>
		/// Whether to strip iOS symbols or not (implied by Shipping config).
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		[CommandLine("-stripsymbols", Value = "true")]
		public bool bStripSymbols = false;

		/// <summary>
		///
		/// </summary>
		public bool bShipForBitcode = false;

		/// <summary>
		/// If true, then a stub IPA will be generated when compiling is done (minimal files needed for a valid IPA).
		/// </summary>
		[CommandLine("-CreateStub", Value = "true")]
		public bool bCreateStubIPA = false;

		/// <summary>
		/// Whether to generate a native Xcode project as a wrapper for the framework.
		/// </summary>
		public bool bGenerateFrameworkWrapperProject = false;

		/// <summary>
		/// Don't generate crashlytics data
		/// </summary>
		[CommandLine("-alwaysgeneratedsym", Value = "true")]
		public bool bGeneratedSYM = false;

		/// <summary>
		/// Don't generate crashlytics data
		/// </summary>
		[CommandLine("-skipcrashlytics")]
		public bool bSkipCrashlytics = false;

		/// <summary>
		/// Disables clang build verification checks on static libraries
		/// </summary>
		[CommandLine("-skipclangvalidation", Value = "true")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bSkipClangValidation")]
		public bool bSkipClangValidation = false;

		/// <summary>
		/// Mark the build for distribution
		/// </summary>
		[CommandLine("-distribution")]
		public bool bForDistribution = false;

		/// <summary>
		/// Manual override for the provision to use. Should be a full path.
		/// </summary>
		[CommandLine("-ImportProvision=")]
		public string? ImportProvision = null;

		/// <summary>
		/// Imports the given certificate (inc private key) into a temporary keychain before signing.
		/// </summary>
		[CommandLine("-ImportCertificate=")]
		public string? ImportCertificate = null;

		/// <summary>
		/// Password for the imported certificate
		/// </summary>
		[CommandLine("-ImportCertificatePassword=")]
		public string? ImportCertificatePassword = null;

		/// <summary>
		/// Cached project settings for the target (set in ResetTarget)
		/// </summary>
		public IOSProjectSettings? ProjectSettings = null;

		/// <summary>
		/// Enables address sanitizer (ASan)
		/// </summary>
		[CommandLine("-EnableASan")]
		public bool bEnableAddressSanitizer = false;

		/// <summary>
		/// Enables thread sanitizer (TSan)
		/// </summary>
		[CommandLine("-EnableTSan")]
		public bool bEnableThreadSanitizer = false;

		/// <summary>
		/// Enables undefined behavior sanitizer (UBSan)
		/// </summary>
		[CommandLine("-EnableUBSan")]
		public bool bEnableUndefinedBehaviorSanitizer = false;
	}

	/// <summary>
	/// Read-only wrapper for IOS-specific target settings
	/// </summary>
	public partial class ReadOnlyIOSTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private IOSTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyIOSTargetRules(IOSTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
#pragma warning disable CS1591
		public bool bStripSymbols => Inner.bStripSymbols;

		public bool bGenerateFrameworkWrapperProject => Inner.bGenerateFrameworkWrapperProject;

		public bool bGeneratedSYM => Inner.bGeneratedSYM;

		public bool bCreateStubIPA => Inner.bCreateStubIPA;

		public bool bSkipCrashlytics => Inner.bSkipCrashlytics;

		public bool bSkipClangValidation => Inner.bSkipClangValidation;

		public bool bForDistribution => Inner.bForDistribution;

		public string? ImportProvision => Inner.ImportProvision;

		public string? ImportCertificate => Inner.ImportCertificate;

		public string? ImportCertificatePassword => Inner.ImportCertificatePassword;

		public float RuntimeVersion => Single.Parse(Inner.ProjectSettings!.RuntimeVersion, System.Globalization.CultureInfo.InvariantCulture);

		public bool bEnableAddressSanitizer => Inner.bEnableAddressSanitizer;

		public bool bEnableThreadSanitizer => Inner.bEnableThreadSanitizer;

		public bool bEnableUndefinedBehaviorSanitizer => Inner.bEnableUndefinedBehaviorSanitizer;

#pragma warning restore CS1591
		#endregion
	}

	/// <summary>
	/// Stores project-specific IOS settings. Instances of this object are cached by IOSPlatform.
	/// </summary>
	public class IOSProjectSettings
	{
		/// <summary>
		/// The cached project file location
		/// </summary>
		public readonly FileReference? ProjectFile;

		/// <summary>
		/// Whether to build the iOS project as a framework.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bBuildAsFramework")]
		[CommandLine("-build-as-framework")]
		public readonly bool bBuildAsFramework = false;

		/// <summary>
		/// Whether to generate a native Xcode project as a wrapper for the framework.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateFrameworkWrapperProject")]
		public readonly bool bGenerateFrameworkWrapperProject = false;

		/// <summary>
		/// Whether to generate a dSYM file or not.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGeneratedSYMFile")]
		[CommandLine("-generatedsymfile")]
		public readonly bool bGeneratedSYMFile = false;

		/// <summary>
		/// Whether to generate a dSYM bundle (as opposed to single file dSYM)
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGeneratedSYMBundle")]
		[CommandLine("-generatedsymbundle")]
		public readonly bool bGeneratedSYMBundle = false;

		/// <summary>
		/// Whether to generate a dSYM file or not.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bGenerateCrashReportSymbols")]
		public readonly bool bGenerateCrashReportSymbols = false;

		/// <summary>
		/// The minimum supported version
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MinimumiOSVersion")]
		private readonly string? MinimumIOSVersion = null;

		/// <summary>
		/// Whether to support iPhone
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPhone")]
		private readonly bool bSupportsIPhone = true;

		/// <summary>
		/// Whether to support iPad
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsIPad")]
		private readonly bool bSupportsIPad = true;

		/// <summary>
		/// additional linker flags for shipping
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalShippingLinkerFlags")]
		public readonly string AdditionalShippingLinkerFlags = "";

		/// <summary>
		/// additional linker flags for non-shipping
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "AdditionalLinkerFlags")]
		public readonly string AdditionalLinkerFlags = "";

		/// <summary>
		/// mobile provision to use for code signing
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "MobileProvision")]
		public readonly string MobileProvision = "";

		/// <summary>
		/// signing certificate to use for code signing
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "SigningCertificate")]
		public readonly string SigningCertificate = "";

		/// <summary>
		/// true if notifications are enabled
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableRemoteNotificationsSupport")]
		public readonly bool bNotificationsEnabled = false;

		/// <summary>
		/// true if notifications are enabled
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableBackgroundFetch")]
		public readonly bool bBackgroundFetchEnabled = false;

		/// <summary>
		/// true if iTunes file sharing support is enabled
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bSupportsITunesFileSharing")]
		public readonly bool bFileSharingEnabled = false;

		/// <summary>
		/// The bundle identifier
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "BundleIdentifier")]
		public readonly string BundleIdentifier = "";

		/// <summary>
		/// true if using Xcode managed provisioning, else false
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bAutomaticSigning")]
		public readonly bool bAutomaticSigning = false;

		/// <summary>
		/// The IOS Team ID
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "IOSTeamID")]
		public readonly string TeamID = "";

		/// <summary>
		/// true to change FORCEINLINE to a regular INLINE.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bDisableForceInline")]
		public readonly bool bDisableForceInline = false;

		/// <summary>
		/// true if IDFA are enabled
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableAdvertisingIdentifier")]
		public readonly bool bEnableAdvertisingIdentifier = false;

		/// <summary>
		/// true when building for distribution
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/UnrealEd.ProjectPackagingSettings", "ForDistribution")]
		public readonly bool bForDistribution = false;

		/// <summary>
		/// override for the app's display name if different from the project name
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/UnrealEd.ProjectPackagingSettings", "BundleName")]
		public readonly string BundleName = "";

		/// <summary>
		/// longer display name than BundleName if needed
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/UnrealEd.ProjectPackagingSettings", "BundleDisplayName")]
		public readonly string BundleDisplayName = "";

		/// <summary>
		/// Which version of the iOS to allow at run time
		/// </summary>
		public virtual string RuntimeVersion
		{
			get
			{
				switch (MinimumIOSVersion)
				{
					case "IOS_Minimum":
					case "IOS_15":
						return "15.0";
					case "IOS_16":
						return "16.0";
					case "IOS_17":
						return "17.0";
					default:
						return "15.0";
				}
			}
		}

		/// <summary>
		/// which devices the game is allowed to run on
		/// </summary>
		public virtual string RuntimeDevices
		{
			get
			{
				if (bSupportsIPad && !bSupportsIPhone)
				{
					return "2";
				}
				else if (!bSupportsIPad && bSupportsIPhone)
				{
					return "1";
				}
				else
				{
					return "1,2";
				}
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ProjectFile">The project file to read settings for</param>
		/// <param name="Bundle">Bundle identifier needed when project file is empty</param>
		public IOSProjectSettings(FileReference? ProjectFile, string? Bundle)
			: this(ProjectFile, UnrealTargetPlatform.IOS, Bundle)
		{
		}

		/// <summary>
		/// Protected constructor. Used by TVOSProjectSettings.
		/// </summary>
		/// <param name="ProjectFile">The project file to read settings for</param>
		/// <param name="Platform">The platform to read settings for</param>
		/// <param name="Bundle">Bundle identifier needed when project file is empty</param>
		protected IOSProjectSettings(FileReference? ProjectFile, UnrealTargetPlatform Platform, string? Bundle)
		{
			this.ProjectFile = ProjectFile;
			ConfigCache.ReadSettings(DirectoryReference.FromFile(ProjectFile), Platform, this);
			if ((ProjectFile == null || String.IsNullOrEmpty(ProjectFile.FullName)) && !String.IsNullOrEmpty(Bundle))
			{
				BundleIdentifier = Bundle;
			}
			BundleIdentifier = BundleIdentifier.Replace("[PROJECT_NAME]", ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UnrealGame")).Replace("_", "");
		}
	}

	/// <summary>
	/// IOS provisioning data
	/// </summary>
	class IOSProvisioningData
	{
		public string? SigningCertificate;
		public FileReference? MobileProvisionFile;
		public string? MobileProvisionUUID;
		public string? MobileProvisionName;
		public string? TeamUUID;
		public string? BundleIdentifier;
		public bool bHaveCertificate = false;

		public string? MobileProvision => MobileProvisionFile?.GetFileName();

		public IOSProvisioningData(IOSProjectSettings ProjectSettings, bool bForDistribution, ILogger Logger)
			: this(ProjectSettings, false, bForDistribution, Logger)
		{
		}

		protected IOSProvisioningData(IOSProjectSettings ProjectSettings, bool bIsTVOS, bool bForDistribution, ILogger Logger)
		{
			SigningCertificate = ProjectSettings.SigningCertificate;
			string? MobileProvision = ProjectSettings.MobileProvision;

			FileReference? ProjectFile = ProjectSettings.ProjectFile;
			CodeSigningConfig.Initialize(ProjectFile, bIsTVOS);

			if (!String.IsNullOrEmpty(SigningCertificate))
			{
				List<string> Certs = AppleCodeSign.FindCertificates();
				List<FileReference> Provisions = AppleCodeSign.FindProvisions(ProjectSettings.BundleIdentifier, bForDistribution, out _);
			}
			else
			{
				SigningCertificate = bForDistribution ? "iPhone Distribution" : "iPhone Developer";
				bHaveCertificate = true;
			}

			if (!String.IsNullOrEmpty(MobileProvision))
			{
				DirectoryReference MobileProvisionDir;
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					MobileProvisionDir = DirectoryReference.Combine(new DirectoryReference(Environment.GetEnvironmentVariable("HOME")!), "Library", "MobileDevice", "Provisioning Profiles");
				}
				else
				{
					MobileProvisionDir = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData)!, "Apple Computer", "MobileDevice", "Provisioning Profiles");
				}

				FileReference PossibleMobileProvisionFile = FileReference.Combine(MobileProvisionDir, MobileProvision);
				if (FileReference.Exists(PossibleMobileProvisionFile))
				{
					MobileProvisionFile = PossibleMobileProvisionFile;
				}
			}

			if (MobileProvisionFile == null || !bHaveCertificate)
			{

				SigningCertificate = "";
				MobileProvision = "";
				MobileProvisionFile = null;
				Logger.LogInformation("Provision not specified or not found for {Project}, searching for compatible match...", ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UnrealGame"));

				if (AppleCodeSign.FindCertAndProvision(ProjectSettings.BundleIdentifier, out MobileProvisionFile, out SigningCertificate))
				{
					MobileProvision = MobileProvisionFile!.FullName;
				}

				if (MobileProvisionFile != null)
				{
					Logger.LogInformation("Provision found for {Project}, Provision: {Provision}, Certificate: {Certificate}", ((ProjectFile != null) ? ProjectFile.GetFileNameWithoutAnyExtensions() : "UnrealGame"), MobileProvisionFile, SigningCertificate);
				}
			}

			// add to the dictionary
			SigningCertificate = SigningCertificate.Replace("\"", "");

			// read the provision to get the UUID
			if (MobileProvisionFile == null)
			{
				Logger.LogInformation("No matching provision file was discovered for {ProjectFile}. Please ensure you have a compatible provision installed.", ProjectFile);
			}
			else if (!FileReference.Exists(MobileProvisionFile))
			{
				Logger.LogInformation("Selected mobile provision for {ProjectFile} ({MobileProvisionFile}) was not found. Please ensure you have a compatible provision installed.", ProjectFile, MobileProvisionFile);
			}
			else
			{
				byte[] AllBytes = FileReference.ReadAllBytes(MobileProvisionFile);

				uint StartIndex = (uint)AllBytes.Length;
				uint EndIndex = (uint)AllBytes.Length;

				for (uint i = 0; i + 4 < AllBytes.Length; i++)
				{
					if (AllBytes[i] == '<' && AllBytes[i + 1] == '?' && AllBytes[i + 2] == 'x' && AllBytes[i + 3] == 'm' && AllBytes[i + 4] == 'l')
					{
						StartIndex = i;
						break;
					}
				}

				if (StartIndex < AllBytes.Length)
				{
					for (uint i = StartIndex; i + 7 < AllBytes.Length; i++)
					{
						if (AllBytes[i] == '<' && AllBytes[i + 1] == '/' && AllBytes[i + 2] == 'p' && AllBytes[i + 3] == 'l' && AllBytes[i + 4] == 'i' && AllBytes[i + 5] == 's' && AllBytes[i + 6] == 't' && AllBytes[i + 7] == '>')
						{
							EndIndex = i + 7;
							break;
						}
					}
				}

				if (StartIndex < AllBytes.Length && EndIndex < AllBytes.Length)
				{
					byte[] TextBytes = new byte[EndIndex - StartIndex];
					Buffer.BlockCopy(AllBytes, (int)StartIndex, TextBytes, 0, (int)(EndIndex - StartIndex));

					string AllText = Encoding.UTF8.GetString(TextBytes);
					int idx = AllText.IndexOf("<key>UUID</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							MobileProvisionUUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
						}
					}
					idx = AllText.IndexOf("<key>com.apple.developer.team-identifier</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							TeamUUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
						}
					}
					idx = AllText.IndexOf("<key>application-identifier</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							string FullID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
							BundleIdentifier = FullID.Substring(FullID.IndexOf('.') + 1);
						}
					}
					idx = AllText.IndexOf("<key>Name</key>");
					if (idx > 0)
					{
						idx = AllText.IndexOf("<string>", idx);
						if (idx > 0)
						{
							idx += "<string>".Length;
							MobileProvisionName = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
						}
					}
				}

				if (String.IsNullOrEmpty(MobileProvisionUUID) || String.IsNullOrEmpty(TeamUUID))
				{
					MobileProvision = null;
					SigningCertificate = null;
					Logger.LogInformation("Failed to parse the mobile provisioning profile.");
				}
			}
		}

		void IPPDataReceivedHandler(Object Sender, DataReceivedEventArgs Line, ILogger Logger)
		{
			if ((Line != null) && (Line.Data != null))
			{
				if (Line.Data.StartsWith("IPP WARNING:"))
				{
					// Don't output IPP warnings to the console as they may not be warnings relevant to the build and could cause build failures.
					Logger.LogDebug("{LineData}", Line.Data);
				}
				else
				{
					Logger.LogInformation("{LineData}", Line.Data);
				}

				if (!String.IsNullOrEmpty(SigningCertificate))
				{
					if (Line.Data.Contains("CERTIFICATE-") && Line.Data.Contains(SigningCertificate))
					{
						bHaveCertificate = true;
					}
				}
				else
				{
					int cindex = Line.Data.IndexOf("CERTIFICATE-");
					int pindex = Line.Data.IndexOf("PROVISION-");
					if (cindex > -1 && pindex > -1)
					{
						cindex += "CERTIFICATE-".Length;
						SigningCertificate = Line.Data.Substring(cindex, pindex - cindex - 1);
						pindex += "PROVISION-".Length;
						if (pindex < Line.Data.Length)
						{
							MobileProvisionFile = new FileReference(Line.Data.Substring(pindex));
						}
					}
				}
			}
		}
	}

	class IOSArchitectureConfig : UnrealArchitectureConfig
	{
		public IOSArchitectureConfig()
			: base(UnrealArchitectureMode.SingleTargetCompileSeparately, new[] { UnrealArch.Arm64, UnrealArch.IOSSimulator })
		{

		}

		public override UnrealArchitectures ActiveArchitectures(FileReference? ProjectFile, string? TargetName)
		{
			// always use arm64 unless overridden on command line
			return new UnrealArchitectures(UnrealArch.Arm64);
		}
	}

	class IOSPlatform : AppleBuildPlatform
	{
		List<IOSProjectSettings> CachedProjectSettings = new List<IOSProjectSettings>();
		List<IOSProjectSettings> CachedProjectSettingsByBundle = new List<IOSProjectSettings>();
		Dictionary<string, IOSProvisioningData> ProvisionCache = new Dictionary<string, IOSProvisioningData>();

		public IOSPlatform(UEBuildPlatformSDK InSDK, ILogger Logger)
			: this(InSDK, UnrealTargetPlatform.IOS, Logger)
		{
		}

		protected IOSPlatform(UEBuildPlatformSDK InSDK, UnrealTargetPlatform TargetPlatform, ILogger Logger)
			: base(TargetPlatform, InSDK, new IOSArchitectureConfig(), Logger)
		{
		}

		public override List<FileReference> FinalizeBinaryPaths(FileReference BinaryName, FileReference? ProjectFile, ReadOnlyTargetRules Target)
		{
			List<FileReference> BinaryPaths = new List<FileReference>();
			if (Target.bShouldCompileAsDLL)
			{
				BinaryPaths.Add(FileReference.Combine(BinaryName.Directory, Target.Configuration.ToString(), Target.Name + ".framework", Target.Name));
			}
			else
			{
				BinaryPaths.Add(BinaryName);
			}
			return BinaryPaths;
		}

		public override void ResetTarget(TargetRules Target)
		{
			Target.bDeployAfterCompile = true;

			Target.IOSPlatform.ProjectSettings = ((IOSPlatform)GetBuildPlatform(Target.Platform)).ReadProjectSettings(Target.ProjectFile);

			if (!AppleExports.UseModernXcode(Target.ProjectFile))
			{
				// always strip in shipping configuration (commandline could have set it also)
				if (Target.Configuration == UnrealTargetConfiguration.Shipping)
				{
					Target.IOSPlatform.bStripSymbols = true;
				}

				// if we are stripping the executable, or if the project requested it, or if it's a buildmachine, generate the dsym
				if (Target.IOSPlatform.bStripSymbols || Target.IOSPlatform.ProjectSettings.bGeneratedSYMFile || Unreal.IsBuildMachine())
				{
					Target.IOSPlatform.bGeneratedSYM = true;
				}
			}

			// Set bShouldCompileAsDLL when building as a framework
			Target.bShouldCompileAsDLL = Target.IOSPlatform.ProjectSettings.bBuildAsFramework;
		}

		public override void ValidateTarget(TargetRules Target)
		{
			if (!String.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE")))
			{
				Target.StaticAnalyzer = StaticAnalyzer.Default;
				Target.StaticAnalyzerOutputType = (Environment.GetEnvironmentVariable("CLANG_ANALYZER_OUTPUT")?.Contains("html", StringComparison.OrdinalIgnoreCase) == true) ? StaticAnalyzerOutputType.Html : StaticAnalyzerOutputType.Text;
				Target.StaticAnalyzerMode = String.Equals(Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE"), "shallow", StringComparison.OrdinalIgnoreCase) ? StaticAnalyzerMode.Shallow : StaticAnalyzerMode.Deep;
			}
			else if (Target.StaticAnalyzer == StaticAnalyzer.Clang)
			{
				Target.StaticAnalyzer = StaticAnalyzer.Default;
			}

			// Disable linking and ignore build outputs if we're using a static analyzer
			if (Target.StaticAnalyzer == StaticAnalyzer.Default)
			{
				Target.bDisableLinking = true;
				Target.bIgnoreBuildOutputs = true;

				// Clang static analysis requires non unity builds
				Target.bUseUnityBuild = false;
			}

			// we assume now we are building with IOS8 or later
			if (Target.bCompileAgainstEngine)
			{
				Target.GlobalDefinitions.Add("HAS_METAL=1");
				Target.ExtraModuleNames.Add("MetalRHI");
			}
			else
			{
				Target.GlobalDefinitions.Add("HAS_METAL=0");
			}

			if (Target.bShouldCompileAsDLL)
			{
				int PreviousDefinition = Target.GlobalDefinitions.FindIndex(s => s.Contains("BUILD_EMBEDDED_APP"));
				if (PreviousDefinition >= 0)
				{
					Target.GlobalDefinitions.RemoveAt(PreviousDefinition);
				}

				Target.GlobalDefinitions.Add("BUILD_EMBEDDED_APP=1");

				if (Target.Platform == UnrealTargetPlatform.IOS)
				{
					Target.ExportPublicHeader = "Headers/PreIOSEmbeddedView.h";
				}
			}

			Target.bCheckSystemHeadersForModification = false;
		}

		public override void ValidateModule(UEBuildModule Module, ReadOnlyTargetRules Target)
		{
			base.ValidateModule(Module, Target);

			// @todo temporarily disabling due to VisionOS rquiring newer Xcode than this will allow - we may remove this entirely
#if false
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac && !Target.IOSPlatform.bSkipClangValidation)
			{
				ApplePlatformSDK SDK = (ApplePlatformSDK?)GetSDK() ?? new ApplePlatformSDK(Logger);
				foreach (FileReference LibLoc in Module.PublicLibraries)
				{
					switch (LibLoc.GetExtension())
					{
						case ".a":
							{
								// When static lib, grep it
								string Args = "-c \"strings ";
								Args += LibLoc.FullName;
								Args += " | grep -m1 -i \\(clang\"";
								string StdOutResult = Utils.RunLocalProcessAndReturnStdOut("bash", Args);
								if (String.IsNullOrEmpty(StdOutResult))
								{
									continue;
								}

								// This Regex will extract a 2-4 segment version code from string containing a 2-5 segment code 
								// ie: if given string: "Apple clang version 14.0.0 (clang-1400.0.17.3.1)"
								//     it'll extract: "1400.0.17.3"  (note the dropped 5th segment)
								Match M = Regex.Match(StdOutResult, @"(\(clang-(?<ver>\d+.\d+(.(\d+))?(.(\d+))?)(.(\d+))?\))");
								if (M.Success)
								{
									string LibString = M.Groups["ver"].ToString();
									Version? LibVersion = new Version(LibString);
									if (LibVersion != null && LibVersion > SDK.MinimumStaticLibClangVersion)
									{
										throw new BuildException("iOS Static Library:'{0}' is built with a version of clang newer than UE supports ({1} > {2}). \nPlease rebuild {3} with the minimum supported version of Xcode/clang.", LibLoc.GetFileName(), LibString, SDK.MinimumStaticLibClangVersion, LibLoc);
									}
								}
							}
							break;

						default:
							// For now, we don't validate any other types of libs (dylib, Framework, etc)
							break;
					}
				}
			}
#endif
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, "")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".stub")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dylib")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dSYM")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dSYM.zip")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".o");
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".dylib";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		public IOSProjectSettings ReadProjectSettings(FileReference? ProjectFile, string? Bundle = "")
		{
			IOSProjectSettings? ProjectSettings = null;

			// Use separate lists to prevent an overridden Bundle id polluting the standard project file. 
			bool bCacheByBundle = !String.IsNullOrEmpty(Bundle);
			if (bCacheByBundle)
			{
				lock (CachedProjectSettingsByBundle)
				{
					ProjectSettings = CachedProjectSettingsByBundle.FirstOrDefault(x => x.ProjectFile == ProjectFile && x.BundleIdentifier == Bundle);
					if (ProjectSettings == null)
					{
						ProjectSettings = CreateProjectSettings(ProjectFile, Bundle);
						CachedProjectSettingsByBundle.Add(ProjectSettings);
					}
				}
			}
			else
			{
				lock (CachedProjectSettings)
				{
					ProjectSettings = CachedProjectSettings.FirstOrDefault(x => x.ProjectFile == ProjectFile);
					if (ProjectSettings == null)
					{
						ProjectSettings = CreateProjectSettings(ProjectFile, Bundle);
						CachedProjectSettings.Add(ProjectSettings);
					}
				}
			}

			return ProjectSettings;
		}

		protected virtual IOSProjectSettings CreateProjectSettings(FileReference? ProjectFile, string? Bundle)
		{
			return new IOSProjectSettings(ProjectFile, Bundle);
		}

		public IOSProvisioningData ReadProvisioningData(FileReference? ProjectFile, bool bForDistribution = false, string? Bundle = "")
		{
			IOSProjectSettings ProjectSettings = ReadProjectSettings(ProjectFile, Bundle);
			return ReadProvisioningData(ProjectSettings, bForDistribution);
		}

		public IOSProvisioningData ReadProvisioningData(IOSProjectSettings ProjectSettings, bool bForDistribution = false)
		{
			string ProvisionKey = ProjectSettings.BundleIdentifier + " " + bForDistribution.ToString();

			IOSProvisioningData? ProvisioningData;

			lock (ProvisionCache)
			{
				if (!ProvisionCache.TryGetValue(ProvisionKey, out ProvisioningData))
				{
					ProvisioningData = CreateProvisioningData(ProjectSettings, bForDistribution);
					ProvisionCache.Add(ProvisionKey, ProvisioningData);
				}
			}
			return ProvisioningData;
		}

		protected virtual IOSProvisioningData CreateProvisioningData(IOSProjectSettings ProjectSettings, bool bForDistribution)
		{
			return new IOSProvisioningData(ProjectSettings, bForDistribution, Logger);
		}

		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			if (InTarget.IOSPlatform.bGeneratedSYM)
			{
				IOSProjectSettings ProjectSettings = ReadProjectSettings(InTarget.ProjectFile);

				// which format?
				if (ProjectSettings.bGeneratedSYMBundle)
				{
					return new string[] { ".dSYM.zip" };
				}
				else
				{
					return new string[] { ".dSYM" };
				}
			}

			return new string[] { };
		}

		public override bool CanUseXGE()
		{
			return BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac;
		}

		public override bool CanUseFASTBuild()
		{
			return true;
		}

		public bool HasCustomIcons(DirectoryReference ProjectDirectoryName, ILogger Logger)
		{
			string IconDir = Path.Combine(ProjectDirectoryName.FullName, "Build", "IOS", "Resources", "Graphics");
			if (Directory.Exists(IconDir))
			{
				foreach (string f in Directory.EnumerateFiles(IconDir))
				{
					if (f.Contains("Icon") && Path.GetExtension(f).Contains(".png"))
					{
						Logger.LogInformation("Requiring custom build because project {Project} has custom icons", Path.GetFileName(ProjectDirectoryName.FullName));
						return true;
					}
				}
			}
			return false;
		}

		/// <summary>
		/// Check for the default configuration
		/// return true if the project uses the default build config
		/// </summary>
		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			string[] BoolKeys = new string[] {
				"bShipForBitcode", "bGeneratedSYMFile",
				"bGeneratedSYMBundle", "bEnableRemoteNotificationsSupport", "bEnableCloudKitSupport",
				"bGenerateCrashReportSymbols", "bEnableBackgroundFetch"
			};
			string[] StringKeys = new string[] {
				"MinimumiOSVersion",
				"AdditionalLinkerFlags",
				"AdditionalShippingLinkerFlags"
			};

			// check for custom icons
			if (HasCustomIcons(ProjectDirectoryName, Logger))
			{
				return false;
			}

			// look up iOS specific settings
			if (!DoProjectSettingsMatchDefault(Platform, ProjectDirectoryName, "/Script/IOSRuntimeSettings.IOSRuntimeSettings",
					BoolKeys, null, StringKeys, Logger))
			{
				return false;
			}

			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectDirectoryName);
		}

		/// <summary>
		/// Check for the build requirement due to platform requirements
		/// return true if the project requires a build
		/// </summary>
		public override bool RequiresBuild(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			// check for custom icons
			return HasCustomIcons(ProjectDirectoryName, Logger);
		}

		public override bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			// This platform currently always compiles monolithic
			return true;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			// don't do any target platform stuff if SDK is not available
			if (!UEBuildPlatform.IsPlatformAvailableForTarget(Platform, Target))
			{
				return;
			}

			if ((Target.Platform == UnrealTargetPlatform.Win64) || (Target.Platform == UnrealTargetPlatform.Mac))
			{
				bool bBuildShaderFormats = Target.bForceBuildShaderFormats;
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
						}
					}
					else if (ModuleName == "TargetPlatform")
					{
						bBuildShaderFormats = true;
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatASTC");
						Rules.DynamicallyLoadedModuleNames.Add("TextureFormatETC2");
						if (Target.bBuildDeveloperTools && Target.bCompileAgainstEngine)
						{
							Rules.DynamicallyLoadedModuleNames.Add("AudioFormatADPCM");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (Target.bForceBuildTargetPlatforms)
					{
						Rules.DynamicallyLoadedModuleNames.Add("IOSTargetPlatform");
						Rules.DynamicallyLoadedModuleNames.Add("TVOSTargetPlatform");
					}

					if (bBuildShaderFormats)
					{
						Rules.DynamicallyLoadedModuleNames.Add("MetalShaderFormat");
					}
				}

				if (ModuleName == "UnrealEd")
				{
					Rules.DynamicallyLoadedModuleNames.Add("IOSPlatformEditor");
				}
			}
		}

		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if (ModuleName == "Launch")
			{
				Rules.PrivateDependencyModuleNames.AddRange(new string[] {
					"AudioMixerAudioUnit",
					"IOSAudio",
					"LaunchDaemonMessages",
				});

				Rules.DynamicallyLoadedModuleNames.AddRange(new string[] {
					"IOSLocalNotification",
					"IOSRuntimeSettings",
				});

				// needed for Metal layer
				Rules.PublicFrameworks.Add("QuartzCore");
			}
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			return true;
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(Target.Platform)).ReadProjectSettings(Target.ProjectFile);
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				Logger.LogInformation("Compiling against OS Version {RuntimeVersion} [minimum allowed at runtime]", ProjectSettings.RuntimeVersion);
			}

			CompileEnvironment.Definitions.Add("PLATFORM_IOS=1");
			CompileEnvironment.Definitions.Add("PLATFORM_APPLE=1");

			CompileEnvironment.Definitions.Add("WITH_TTS=0");
			CompileEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			CompileEnvironment.Definitions.Add("WITH_EDITOR=0");
			CompileEnvironment.Definitions.Add("USE_NULL_RHI=0");

			if (ProjectSettings.bNotificationsEnabled)
			{
				CompileEnvironment.Definitions.Add("NOTIFICATIONS_ENABLED=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("NOTIFICATIONS_ENABLED=0");
			}
			if (ProjectSettings.bBackgroundFetchEnabled)
			{
				CompileEnvironment.Definitions.Add("BACKGROUNDFETCH_ENABLED=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("BACKGROUNDFETCH_ENABLED=0");
			}
			if (ProjectSettings.bFileSharingEnabled)
			{
				CompileEnvironment.Definitions.Add("FILESHARING_ENABLED=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("FILESHARING_ENABLED=0");
			}

			CompileEnvironment.Definitions.Add("UE_DISABLE_FORCE_INLINE=" + (ProjectSettings.bDisableForceInline ? "1" : "0"));

			if (Target.Architecture == UnrealArch.IOSSimulator || Target.Architecture == UnrealArch.TVOSSimulator)
			{
				CompileEnvironment.Definitions.Add("WITH_IOS_SIMULATOR=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("WITH_IOS_SIMULATOR=0");
			}

			if (ProjectSettings.bEnableAdvertisingIdentifier)
			{
				CompileEnvironment.Definitions.Add("ENABLE_ADVERTISING_IDENTIFIER=1");
			}

			// if the project has an Oodle compression Dll, enable the decompressor on IOS
			if (Target.ProjectFile != null)
			{
				DirectoryReference ProjectDir = Target.ProjectFile.Directory;
				string OodleDllPath = DirectoryReference.Combine(ProjectDir, "Binaries/ThirdParty/Oodle/Mac/libUnrealPakPlugin.dylib").FullName;
				if (File.Exists(OodleDllPath))
				{
					Logger.LogDebug("        Registering custom oodle compressor for {Platform}", UnrealTargetPlatform.IOS.ToString());
					CompileEnvironment.Definitions.Add("REGISTER_OODLE_CUSTOM_COMPRESSOR=1");
				}
			}

			// convert runtime version into standardized integer
			float TargetFloat = Target.IOSPlatform.RuntimeVersion;
			int IntPart = (int)TargetFloat;
			int FracPart = (int)((TargetFloat - IntPart) * 10);
			int TargetNum = IntPart * 10000 + FracPart * 100;
			CompileEnvironment.Definitions.Add("MINIMUM_UE_COMPILED_IOS_VERSION=" + TargetNum);

			LinkEnvironment.AdditionalFrameworks.Add(new UEBuildFramework("GameKit"));
			LinkEnvironment.AdditionalFrameworks.Add(new UEBuildFramework("StoreKit"));
			LinkEnvironment.AdditionalFrameworks.Add(new UEBuildFramework("DeviceCheck"));

		}

		/// <summary>
		/// Setup the binaries for this specific platform.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="ExtraModuleNames"></param>
		public override void AddExtraModules(ReadOnlyTargetRules Target, List<string> ExtraModuleNames)
		{
			if (Target.Type != TargetType.Program)
			{
				ExtraModuleNames.Add("IOSPlatformFeatures");
			}
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(ReadOnlyTargetRules Target)
		{
			ClangToolChainOptions Options = ClangToolChainOptions.None;
			if (Target.IOSPlatform.bEnableAddressSanitizer)
			{
				Options |= ClangToolChainOptions.EnableAddressSanitizer;
			}
			if (Target.IOSPlatform.bEnableThreadSanitizer)
			{
				Options |= ClangToolChainOptions.EnableThreadSanitizer;
			}
			if (Target.IOSPlatform.bEnableUndefinedBehaviorSanitizer)
			{
				Options |= ClangToolChainOptions.EnableUndefinedBehaviorSanitizer;
			}

			IOSProjectSettings ProjectSettings = ReadProjectSettings(Target.ProjectFile);
			return new IOSToolChain(Target, ProjectSettings, Options, Logger);
		}

		/// <inheritdoc/>
		public override void Deploy(TargetReceipt Receipt)
		{
			if (Receipt.HasValueForAdditionalProperty("CompileAsDll", "true"))
			{
				// IOSToolchain.PostBuildSync handles the copy, nothing else to do here
			}
			else
			{
				new UEDeployIOS(Logger).PrepTargetForDeployment(Receipt);
			}
		}
	}

	class IOSPlatformFactory : UEBuildPlatformFactory
	{
		public override UnrealTargetPlatform TargetPlatform => UnrealTargetPlatform.IOS;

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms(ILogger Logger)
		{
			ApplePlatformSDK SDK = new ApplePlatformSDK(Logger);

			// Register this build platform for IOS
			UEBuildPlatform.RegisterBuildPlatform(new IOSPlatform(SDK, Logger), Logger);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.Apple);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.IOS, UnrealPlatformGroup.IOS);
		}
	}
}
