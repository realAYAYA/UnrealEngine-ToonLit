// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// ModuleRules extension for low level tests.
	/// </summary>
	public class TestModuleRules : ModuleRules
	{
		private readonly XNamespace BuildGraphNamespace = XNamespace.Get("http://www.epicgames.com/BuildGraph");
		private readonly XNamespace SchemaInstance = XNamespace.Get("http://www.w3.org/2001/XMLSchema-instance");
		private readonly XNamespace SchemaLocation = XNamespace.Get("http://www.epicgames.com/BuildGraph ../../Build/Graph/Schema.xsd");

		private bool bUsesCatch2 = true;

		/// <summary>
		/// Associated tested module of this test module.
		/// </summary>
		public ModuleRules? TestedModule { get; private set; }

		/// <summary>
		/// Constructs a TestModuleRules object as its own test module.
		/// </summary>
		/// <param name="Target"></param>
		public TestModuleRules(ReadOnlyTargetRules Target) : base(Target)
		{
			SetupCommonProperties(Target);
		}

		/// <summary>
		/// Constructs a TestModuleRules object as its own test module.
		/// Sets value of bUsesCatch2.
		/// </summary>
		public TestModuleRules(ReadOnlyTargetRules Target, bool InUsesCatch2) : base(Target)
		{
			bUsesCatch2 = InUsesCatch2;
			if (bUsesCatch2)
			{
				SetupCommonProperties(Target);
			}
		}

		/// <summary>
		/// Constructs a TestModuleRules object with an associated tested module.
		/// </summary>
		public TestModuleRules(ModuleRules TestedModule) : base(TestedModule.Target)
		{
			this.TestedModule = TestedModule;

			Name = TestedModule.Name + "Tests";
			if (!String.IsNullOrEmpty(TestedModule.ShortName))
			{
				ShortName = TestedModule.ShortName + "Tests";
			}

			File = TestedModule.File;
			Directory = DirectoryReference.Combine(TestedModule.Directory, "Tests");

			Context = TestedModule.Context;

			PrivateDependencyModuleNames.AddRange(TestedModule.PrivateDependencyModuleNames);
			PublicDependencyModuleNames.AddRange(TestedModule.PublicDependencyModuleNames);

			DirectoriesForModuleSubClasses = new Dictionary<Type, DirectoryReference>();

			// Tests can refer to tested module's Public and Private paths
			string ModulePublicDir = Path.Combine(TestedModule.ModuleDirectory, "Public");
			if (System.IO.Directory.Exists(ModulePublicDir))
			{
				PublicIncludePaths.Add(ModulePublicDir);
			}

			string ModulePrivateDir = Path.Combine(TestedModule.ModuleDirectory, "Private");
			if (System.IO.Directory.Exists(ModulePrivateDir))
			{
				PrivateIncludePaths.Add(ModulePrivateDir);
			}

			SetupCommonProperties(Target);
		}

		private void SetupCommonProperties(ReadOnlyTargetRules Target)
		{
			bIsTestModuleOverride = true;

			PCHUsage = PCHUsageMode.NoPCHs;
			PrecompileForTargets = PrecompileTargetsType.None;

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.Platform == UnrealTargetPlatform.Linux)
			{
				OptimizeCode = CodeOptimization.Never;
			}

			bAllowConfidentialPlatformDefines = true;
			bLegalToDistributeObjectCode = true;

			// Required false for catch.hpp
			bUseUnity = false;

			// Disable exception handling so that tests can assert for exceptions
			bEnableObjCExceptions = false;
			bEnableExceptions = false;

			SetResourcesFolder("Resources");

			if (!PublicDependencyModuleNames.Contains("Catch2"))
			{
				PublicDependencyModuleNames.Add("Catch2");
			}

			if (!PrivateDependencyModuleNames.Contains("LowLevelTestsRunner"))
			{
				PrivateDependencyModuleNames.Add("LowLevelTestsRunner");
			}

			// Platforms specific setup
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				PublicDefinitions.Add("CATCH_CONFIG_NOSTDOUT");
			}

			if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
			{
				// Fix missing frameworks from ApplicationCore
				
				// Needed for CADisplayLink
				PublicFrameworks.Add("QuartzCore");

				// Needed for MTLCreateSystemDefaultDevice
				PublicWeakFrameworks.Add("Metal");
			}
		}

		/// <summary>
		/// Set test-specific resources folder relative to module directory.
		/// This will be copied to the binaries path during deployment.
		/// </summary>
		protected void SetResourcesFolder(string ResourcesRelativeFolder)
		{
			AdditionalPropertiesForReceipt.RemoveAll(Prop => Prop.Name == "ResourcesFolder");

			foreach (DirectoryReference Directory in GetAllModuleDirectories())
			{
				string TestResourcesDir = Path.Combine(Directory.FullName, ResourcesRelativeFolder);
				if (System.IO.Directory.Exists(TestResourcesDir))
				{
					AdditionalPropertiesForReceipt.Add("ResourcesFolder", TestResourcesDir);
				}
			}
		}

#pragma warning disable 8602
#pragma warning disable 8604
		/// <summary>
		/// Generates or updates metadata file for LowLevelTests.xml containing test flags: name, short name, target name, relative binaries path, supported platforms etc.
		/// <paramref name="TestMetadata">The test metadata specifying name, short name etc used to populate the BuildGraph properties file.</paramref>
		/// </summary>
		protected void UpdateBuildGraphPropertiesFile(Metadata TestMetadata)
		{
			bool bUpdateBuildGraphPropertiesFile = false;
			TestTargetRules? TestTargetRules = Target.InnerTestTargetRules;
			if (TestTargetRules != null)
			{
				bUpdateBuildGraphPropertiesFile = TestTargetRules.bUpdateBuildGraphPropertiesFile;
			}

			bool bIsBuildMachine = Unreal.IsBuildMachine();
			if (bIsBuildMachine || !bUpdateBuildGraphPropertiesFile || TestMetadata == null)
			{
				return;
			}

			string BaseFolder = GetBaseFolder();

			string GeneratedPropertiesScriptFile;

			string NonPublicPath = Path.Combine(BaseFolder, "Restricted", "NotForLicensees", "Build", "LowLevelTests", $"{TestMetadata.TestName}.xml");

			bool ModuleInRestrictedPath = IsRestrictedPath(ModuleDirectory);

			if (ModuleInRestrictedPath)
			{
				GeneratedPropertiesScriptFile = NonPublicPath;
			}
			else
			{
				GeneratedPropertiesScriptFile = Path.Combine(BaseFolder, "Build", "LowLevelTests", $"{TestMetadata.TestName}.xml");
			}

			if (!System.IO.File.Exists(GeneratedPropertiesScriptFile))
			{
				string? DirGenProps = Path.GetDirectoryName(GeneratedPropertiesScriptFile);
				if (DirGenProps != null && !System.IO.Directory.Exists(DirGenProps))
				{
					System.IO.Directory.CreateDirectory(DirGenProps);
				}
				using (FileStream FileStream = System.IO.File.Create(GeneratedPropertiesScriptFile))
				{
					XDocument XInitFile = new XDocument(new XElement(BuildGraphNamespace + "BuildGraph", new XAttribute(XNamespace.Xmlns + "xsi", SchemaInstance), new XAttribute(SchemaInstance + "schemaLocation", SchemaLocation)));
					XInitFile.Root?.Add(
						new XElement(
							BuildGraphNamespace + "Property",
							new XAttribute("Name", "TestNames"),
							new XAttribute("Value", "$(TestNames);" + TestMetadata.TestName)));

					XInitFile.Save(FileStream);
				}
			}

			// All relevant properties
			string TestTargetName = Target.LaunchModuleName ?? "Launch";
			string TestBinariesPath = TryGetBinariesPath();

			// Do not save full paths
			if (Path.IsPathRooted(TestBinariesPath))
			{
				TestBinariesPath = Path.GetRelativePath(Unreal.RootDirectory.FullName, TestBinariesPath);
			}

			MakeFileWriteable(GeneratedPropertiesScriptFile);
			XDocument GenPropsDoc = XDocument.Load(GeneratedPropertiesScriptFile);
			XElement? Root = GenPropsDoc.Root;
			// First descendant must be TestNames
			if (Root != null && Root.FirstNode != null)
			{
				XElement TestNames = (XElement)Root.FirstNode;
				if (TestNames != null)
				{
					XElement lastUpdatedNode = TestNames;

					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "Disabled", Convert.ToString(TestMetadata.Disabled));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "Short", Convert.ToString(TestMetadata.TestShortName));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "StagesWithProjectFile", Convert.ToString(TestMetadata.StagesWithProjectFile));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "Target", Convert.ToString(TestTargetName));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "BinariesRelative", Convert.ToString(TestBinariesPath));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "ReportType", Convert.ToString(TestMetadata.ReportType));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "GauntletArgs", Convert.ToString(TestMetadata.InitialExtraArgs) + Convert.ToString(TestMetadata.GauntletArgs));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "ExtraArgs", Convert.ToString(TestMetadata.ExtraArgs));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "HasAfterSteps", Convert.ToString(TestMetadata.HasAfterSteps));
					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "UsesCatch2", Convert.ToString(TestMetadata.UsesCatch2));

					InsertOrUpdateTestOption(ref lastUpdatedNode, TestMetadata.TestName, $"Run {TestMetadata.TestShortName} Tests", "Run", "Tests", false.ToString());

					InsertOrUpdateTestFlagProperty(ref lastUpdatedNode, TestMetadata.TestName, "SupportedPlatforms", TestMetadata.SupportedPlatforms.Aggregate("", (current, next) => (String.IsNullOrEmpty(current) ? next.ToString() : current + ";" + next.ToString())));
				}
			}

			GenPropsDoc.Save(GeneratedPropertiesScriptFile);

			// Platform-specific configurations
			string GeneratedPropertiesPlatformFile;

			string NonPublicPathPlatform;

			// Generate peroperty file for each platform
			foreach (UnrealTargetPlatform ValidPlatform in UnrealTargetPlatform.GetValidPlatforms())
			{
				bool IsRestrictedPlatformName = IsPlatformRestricted(ValidPlatform);
				if (IsRestrictedPlatformName)
				{
					NonPublicPathPlatform = Path.Combine(BaseFolder, "Restricted", "NotForLicensees", "Platforms", ValidPlatform.ToString(), "Build", "LowLevelTests", $"{TestMetadata.TestName}.xml");
				}
				else
				{
					NonPublicPathPlatform = Path.Combine(BaseFolder, "Restricted", "NotForLicensees", "Build", "LowLevelTests", $"{TestMetadata.TestName}.xml");
				}

				if (ModuleInRestrictedPath)
				{
					GeneratedPropertiesPlatformFile = NonPublicPathPlatform;
				}
				else
				{
					if (IsRestrictedPlatformName)
					{
						GeneratedPropertiesPlatformFile = Path.Combine(BaseFolder, "Platforms", ValidPlatform.ToString(), "Build", "LowLevelTests", $"{TestMetadata.TestName}.xml");
					}
					else
					{
						GeneratedPropertiesPlatformFile = Path.Combine(BaseFolder, "Build", "LowLevelTests", $"{TestMetadata.TestName}.xml");
					}
				}

				if (!System.IO.File.Exists(GeneratedPropertiesPlatformFile))
				{
					string? DirGenPropsPlatforms = Path.GetDirectoryName(GeneratedPropertiesPlatformFile);
					if (DirGenPropsPlatforms != null && !System.IO.Directory.Exists(DirGenPropsPlatforms))
					{
						System.IO.Directory.CreateDirectory(DirGenPropsPlatforms);
					}
					using (FileStream FileStream = System.IO.File.Create(GeneratedPropertiesPlatformFile))
					{
						new XDocument(new XElement(BuildGraphNamespace + "BuildGraph", new XAttribute(XNamespace.Xmlns + "xsi", SchemaInstance), new XAttribute(SchemaInstance + "schemaLocation", SchemaLocation))).Save(FileStream);
					}
				}

				MakeFileWriteable(GeneratedPropertiesPlatformFile);
				XDocument XInitPlatformFile = XDocument.Load(GeneratedPropertiesPlatformFile);

				// Adding per-test and per-platform tags
				string TagsValue = TestMetadata.PlatformTags.ContainsKey(ValidPlatform) ? TestMetadata.PlatformTags[ValidPlatform] : String.Empty;
				AppendOrUpdateTestFlagProperty(ref XInitPlatformFile, TestMetadata.TestName, ValidPlatform.ToString(), "Tags", TagsValue);

				string ExtraCompilationArgsValue = TestMetadata.PlatformCompilationExtraArgs.ContainsKey(ValidPlatform) ? TestMetadata.PlatformCompilationExtraArgs[ValidPlatform] : String.Empty;
				AppendOrUpdateTestFlagProperty(ref XInitPlatformFile, TestMetadata.TestName, ValidPlatform.ToString(), "ExtraCompilationArgs", ExtraCompilationArgsValue);

				string RunSupportedValue = TestMetadata.PlatformsRunUnsupported.Contains(ValidPlatform) ? "False" : "True";
				AppendOrUpdateTestFlagProperty(ref XInitPlatformFile, TestMetadata.TestName, ValidPlatform.ToString(), "RunSupported", RunSupportedValue);

				string RunContainerizedValue = TestMetadata.PlatformRunContainerized.ContainsKey(ValidPlatform) ? "True" : "False";
				AppendOrUpdateTestFlagProperty(ref XInitPlatformFile, TestMetadata.TestName, ValidPlatform.ToString(), "RunContainerized", RunContainerizedValue);

				XInitPlatformFile.Save(GeneratedPropertiesPlatformFile);
			}
		}

		private string GetBaseFolder()
		{
			string RelativeModulePath = Path.GetRelativePath(Unreal.RootDirectory.FullName, ModuleDirectory);
			string[] BreadCrumbs = RelativeModulePath.Split(new char[] { '\\', '/' }, StringSplitOptions.RemoveEmptyEntries);
			if (BreadCrumbs.Length > 0)
			{
				return Path.Combine(Unreal.RootDirectory.FullName, BreadCrumbs[0]);
			}
			return Unreal.EngineDirectory.FullName;
		}

		private bool IsPlatformRestricted(UnrealTargetPlatform Platform)
		{
			return RestrictedFolder.GetNames().Contains(Platform.ToString());
		}

		private bool IsRestrictedPath(string ModuleDirectory)
		{
			foreach (string RestrictedFolderName in RestrictedFolder.GetNames())
			{
				if (ModuleDirectory.Contains(RestrictedFolderName))
				{
					return true;
				}
			}

			return false;
		}

		private string TryGetBinariesPath()
		{
			int SourceFolderIndex = ModuleDirectory.IndexOf("Source");
			if (SourceFolderIndex < 0)
			{
				int PluginFolderIndex = ModuleDirectory.IndexOf("Plugins");
				if (PluginFolderIndex >= 0)
				{
					return ModuleDirectory.Substring(0, PluginFolderIndex) + "Binaries";
				}
				throw new Exception("Could not detect source folder path for module " + GetType());
			}
			return ModuleDirectory.Substring(0, SourceFolderIndex) + "Binaries";
		}

		private void AppendOrUpdateTestFlagProperty(ref XDocument Document, string FlagRadix, string FlagPrefix, string FlagSuffix, string FlagValue)
		{
			XElement? Existing = Document.Root!.Elements().Where(element => element.Attribute("Name").Value == FlagPrefix + FlagRadix + FlagSuffix).FirstOrDefault();
			if (Existing != null)
			{
				Existing!.SetAttributeValue("Value", FlagValue);
			}
			else
			{
				XElement ElementAppend = new XElement(BuildGraphNamespace + "Property");
				ElementAppend.SetAttributeValue("Name", FlagPrefix + FlagRadix + FlagSuffix);
				ElementAppend.SetAttributeValue("Value", FlagValue);
				Document.Root!.Add(ElementAppend);
			}
		}
		private void InsertOrUpdateTestFlagProperty(ref XElement ElementUpsertAfter, string TestName, string FlagSuffix, string FlagValue)
		{
			IEnumerable<XElement> NextChunk = ElementUpsertAfter.ElementsAfterSelf(BuildGraphNamespace + "Property")
				.Where(prop => prop.Attribute("Name").Value.EndsWith(FlagSuffix));
			if (!NextChunk
				.Where(prop => prop.Attribute("Name").Value == TestName + FlagSuffix).Any())
			{
				XElement ElementInsert = new XElement(BuildGraphNamespace + "Property");
				ElementInsert.SetAttributeValue("Name", TestName + FlagSuffix);
				ElementInsert.SetAttributeValue("Value", FlagValue);
				ElementUpsertAfter.AddAfterSelf(ElementInsert);
			}
			else
			{
				NextChunk
					.Where(prop => prop.Attribute("Name").Value == TestName + FlagSuffix).First().SetAttributeValue("Value", FlagValue);
			}
			ElementUpsertAfter = NextChunk.Last();
		}

		private void InsertOrUpdateTestOption(ref XElement ElementUpsertAfter, string OptionRadix, string Description, string OptionPrefix, string OptionSuffix, string DefaultValue)
		{
			IEnumerable<XElement> NextChunk = ElementUpsertAfter.ElementsAfterSelf(BuildGraphNamespace + "Option")
				.Where(prop => prop.Attribute("Name").Value.StartsWith(OptionPrefix) && prop.Attribute("Name").Value.EndsWith(OptionSuffix));
			if (!NextChunk
				.Where(prop => prop.Attribute("Name").Value == OptionPrefix + OptionRadix + OptionSuffix).Any())
			{
				XElement ElementInsert = new XElement(BuildGraphNamespace + "Option");
				ElementInsert.SetAttributeValue("Name", OptionPrefix + OptionRadix + OptionSuffix);
				ElementInsert.SetAttributeValue("DefaultValue", DefaultValue);
				ElementInsert.SetAttributeValue("Description", Description);
				ElementUpsertAfter.AddAfterSelf(ElementInsert);
			}
			else
			{
				XElement ElementUpdate = NextChunk
					.Where(prop => prop.Attribute("Name").Value == OptionPrefix + OptionRadix + OptionSuffix).First();
				ElementUpdate.SetAttributeValue("Description", Description);
				ElementUpdate.SetAttributeValue("DefaultValue", DefaultValue);
			}
			ElementUpsertAfter = NextChunk.Last();
		}

#pragma warning restore 8604
#pragma warning restore 8602

		private void MakeFileWriteable(string InFilePath)
		{
			System.IO.File.SetAttributes(InFilePath, System.IO.File.GetAttributes(InFilePath) & ~FileAttributes.ReadOnly);
		}

#pragma warning disable 8618
		/// <summary>
		/// Test metadata class.
		/// </summary>
		public class Metadata
		{
			/// <summary>
			/// Test long name.
			/// </summary>
			public string TestName { get; set; }

			/// <summary>
			/// Test short name used for display in build system.
			/// </summary>
			public string TestShortName { get; set; }

			private string ReportTypePrivate = "console";
			/// <summary>
			/// Type of Catch2 report, defaults to console.
			/// </summary>
			public string ReportType {
				get => ReportTypePrivate;
				set => ReportTypePrivate = value;
			}

			/// <summary>
			/// Does this test use project files for staging additional files
			/// and cause the build to use BuildCookRun instead of a Compile step
			/// </summary>
			public bool StagesWithProjectFile { get; set; }

			/// <summary>
			/// Is this test disabled?
			/// </summary>
			public bool Disabled { get; set; }

			/// <summary>
			/// Depercated, use GauntletArgs or ExtraArgs instead to help indicate arguments to launch the test under.
			/// </summary>
			public string InitialExtraArgs
			{ 
				get;
				[Obsolete]
				set; 
			}

			/// <summary>
			/// Any initial Gauntlet args to be passed to the test executable
			/// </summary>
			public string GauntletArgs { get; set; }

			/// <summary>
			/// Any extra args to be passed to the test executable as --extra-args
			/// </summary>
			public string ExtraArgs { get; set; }

			/// <summary>
			/// Whether there's a step that gets executed after the tests have finished.
			/// Typically used for cleanup of resources.
			/// </summary>
			public bool HasAfterSteps { get; set; }

			private bool UsesCatch2Private = true;
			/// <summary>
			/// Test built with a frakework other than Catch2
			/// </summary>
			public bool UsesCatch2
			{
				get => UsesCatch2Private;
				set => UsesCatch2Private = value;
			}

			/// <summary>
			/// Set of supported platforms.
			/// </summary>
			public HashSet<UnrealTargetPlatform> SupportedPlatforms { get; set; } = new HashSet<UnrealTargetPlatform>() { UnrealTargetPlatform.Win64 };

			private Dictionary<UnrealTargetPlatform, string> PlatformTagsPrivate = new Dictionary<UnrealTargetPlatform, string>();
			/// <summary>
			/// Per-platform tags.
			/// </summary>
			public Dictionary<UnrealTargetPlatform, string> PlatformTags
			{
				get => PlatformTagsPrivate;
				set => PlatformTagsPrivate = value;
			}

			private Dictionary<UnrealTargetPlatform, string> PlatformCompilationExtraArgsPrivate = new Dictionary<UnrealTargetPlatform, string>();
			/// <summary>
			/// Per-platform extra compilation arguments.
			/// </summary>
			public Dictionary<UnrealTargetPlatform, string> PlatformCompilationExtraArgs
			{
				get => PlatformCompilationExtraArgsPrivate;
				set => PlatformCompilationExtraArgsPrivate = value;
			}

			private List<UnrealTargetPlatform> PlatformsRunUnsupportedPrivate = new List<UnrealTargetPlatform>() {
				UnrealTargetPlatform.Android,
				UnrealTargetPlatform.IOS,
				UnrealTargetPlatform.TVOS,
				UnrealTargetPlatform.VisionOS };

			/// <summary>
			/// List of platforms that cannot run tests.
			/// </summary>
			public List<UnrealTargetPlatform> PlatformsRunUnsupported
			{
				get => PlatformsRunUnsupportedPrivate;
				set => PlatformsRunUnsupportedPrivate = value;
			}

			private Dictionary<UnrealTargetPlatform, bool> PlatformRunContainerizedPrivate = new Dictionary<UnrealTargetPlatform, bool>();
			/// <summary>
			/// Whether or not the test is run inside a Docker container for a given platform.
			/// </summary>
			public Dictionary<UnrealTargetPlatform, bool> PlatformRunContainerized
			{
				get => PlatformRunContainerizedPrivate;
				set => PlatformRunContainerizedPrivate = value;
			}

		}
#pragma warning restore 8618
	}
}