// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using UnrealBuildBase;
using EpicGames.Core;
using System;

namespace UnrealBuildTool
{
	/// <summary>
	/// ModuleRules extension for low level tests.
	/// </summary>
	public class TestModuleRules : ModuleRules
	{
		[ConfigFile(ConfigHierarchyType.Engine, "LowLevelTestsSettings", "UpdateBuildGraphPropertiesFile")]
		bool bUpdateBuildGraphPropertiesFile = false;

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
			if (!string.IsNullOrEmpty(TestedModule.ShortName))
			{
				ShortName = TestedModule.ShortName + "Tests";
			}

			File = TestedModule.File;
			Directory = DirectoryReference.Combine(TestedModule.Directory, "Tests");

			Context = TestedModule.Context;

			PrivateDependencyModuleNames.AddRange(TestedModule.PrivateDependencyModuleNames);

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

			string SetupFile = Path.Combine("Private", "setup.cpp");
			string TeardownFile = Path.Combine("Private", "teardown.cpp");
			if (System.IO.File.Exists(Path.Combine(Directory.ToString(), SetupFile)))
			{
				BuildOrderSettings.AddBuildOrderOverride(SetupFile, SourceFileBuildOrder.First);
			}
			if (System.IO.File.Exists(Path.Combine(Directory.ToString(), TeardownFile)))
			{
				BuildOrderSettings.AddBuildOrderOverride(TeardownFile, SourceFileBuildOrder.Last);
			}

			string TestResourcesDir = Path.Combine(ModuleDirectory, "Resources");
			if (System.IO.Directory.Exists(TestResourcesDir))
			{
				AdditionalPropertiesForReceipt.Add("ResourcesFolder", TestResourcesDir);
			}

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
		}

		/// <summary>
		/// Set test-specific resources folder relative to module directory.
		/// This will be copied to the binaries path during deployment.
		/// </summary>
		protected void SetResourcesFolder(string ResourcesRelativeFolder)
		{
			string TestResourcesDir = Path.Combine(ModuleDirectory, ResourcesRelativeFolder);
			if (!System.IO.Directory.Exists(TestResourcesDir))
			{
				throw new Exception($"Resources folder not found {TestResourcesDir}");
			}
			else
			{
				ReceiptProperty? Existing = AdditionalPropertiesForReceipt.Inner.Find(Prop => Prop.Name == "ResourcesFolder");
				if (Existing != null)
				{
					Existing.Value = TestResourcesDir;
				}
				else
				{
					AdditionalPropertiesForReceipt.Add("ResourcesFolder", TestResourcesDir);
				}
			}
		}

		/// <summary>
		/// Generates or updates include file for LowLevelTests.xml containing test flags: name, short name, target name, relative binaries path, supported platforms etc.
		/// <paramref name="TestMetadata">The test metadata specifying name, short name etc used to populate the BuildGraph properties file.</paramref>
		/// </summary>
		protected void UpdateBuildGraphPropertiesFile(Metadata TestMetadata, bool AddToTestNames = true)
		{
			if (!bUpdateBuildGraphPropertiesFile)
			{
				return;
			}

			bool IsPublic = false;
			string GeneratedPropertiesScriptFile;

			string RestrictedFolder = Path.Combine(Unreal.EngineDirectory.FullName, "Restricted");
			string NotForLicenseesFolder = Path.Combine(RestrictedFolder, "NotForLicensees");
			string NonPublicFolder = Path.Combine(NotForLicenseesFolder, "Build");
			string NonPublicPath = Path.Combine(NonPublicFolder, "LowLevelTests_GenProps.xml");

			if (IsRestrictedPath(ModuleDirectory))
			{
				GeneratedPropertiesScriptFile = NonPublicPath;
			}
			else
			{
				IsPublic = true;
				GeneratedPropertiesScriptFile = Path.Combine(Unreal.EngineDirectory.FullName, "Build", "LowLevelTests_GenProps.xml");
			}

			// UE-133126
			if (System.IO.File.Exists(NonPublicPath) && System.IO.Directory.GetFileSystemEntries(NonPublicFolder).Length == 1)
			{
				System.IO.File.Delete(NonPublicPath);
				System.IO.Directory.Delete(NonPublicFolder);
				System.IO.Directory.Delete(NotForLicenseesFolder);
				System.IO.Directory.Delete(RestrictedFolder);
			}

			if (!System.IO.File.Exists(GeneratedPropertiesScriptFile))
			{
				string? DirGenProps = Path.GetDirectoryName(GeneratedPropertiesScriptFile);
				if (DirGenProps != null)
				{
					System.IO.Directory.CreateDirectory(DirGenProps);
				}
				using FileStream FileStream = System.IO.File.Create(GeneratedPropertiesScriptFile);
				XDocument XInitFile = new XDocument(new XElement(BuildGraphNamespace + "BuildGraph", new XAttribute(XNamespace.Xmlns + "xsi", SchemaInstance), new XAttribute(SchemaInstance + "schemaLocation", SchemaLocation)));
				XInitFile.Root?.Add(
					new XElement(
						BuildGraphNamespace + "Property",
						new XAttribute("Name", "TestNames" + (!IsPublic ? "Restricted" : "")),
						new XAttribute("Value", string.Empty)));

				XInitFile.Save(FileStream);
			}

			// All relevant properties
			string TestTargetName = Target.LaunchModuleName ?? "NoLaunchModule";
			string TestBinariesPath = TryGetBinariesPath();

			// Do not save full paths
			if (Path.IsPathRooted(TestBinariesPath))
			{
				TestBinariesPath = Path.GetRelativePath(Unreal.RootDirectory.FullName, TestBinariesPath);
			}

			XDocument GenPropsDoc = XDocument.Load(GeneratedPropertiesScriptFile);
			XElement? Root = GenPropsDoc.Root;
			// First descendant must be TestNames
			if (Root != null && Root.FirstNode != null)
			{
				XElement TestNames = (XElement)Root.FirstNode;
				if (TestNames != null)
				{
					string? AllTestNames = TestNames.Attribute("Value")?.Value;
					if (AllTestNames != null && !AllTestNames.Contains(TestMetadata.TestName) && AddToTestNames)
					{
						if (string.IsNullOrEmpty(AllTestNames))
						{
							AllTestNames += TestMetadata.TestName;
						}
						else if (!AllTestNames.Contains(TestMetadata.TestName))
						{
							AllTestNames += ";" + TestMetadata.TestName;
						}
					}
					TestNames.Attribute("Value")?.SetValue(AllTestNames ?? "");


					XElement lastUpdatedNode = TestNames;
					InsertOrUpdateTestFlag(ref lastUpdatedNode, TestMetadata.TestName, "Disabled", TestMetadata.Disabled.ToString());
					InsertOrUpdateTestFlag(ref lastUpdatedNode, TestMetadata.TestName, "Short", TestMetadata.TestShortName);
					InsertOrUpdateTestFlag(ref lastUpdatedNode, TestMetadata.TestName, "Target", TestTargetName);
					InsertOrUpdateTestFlag(ref lastUpdatedNode, TestMetadata.TestName, "BinariesRelative", TestBinariesPath);
					InsertOrUpdateTestFlag(ref lastUpdatedNode, TestMetadata.TestName, "ReportType", TestMetadata.ReportType.ToString());

					InsertOrUpdateTestOption(ref lastUpdatedNode, TestMetadata.TestName, TestMetadata.TestShortName, "Run", "Tests", false.ToString());

					List<UnrealTargetPlatform> AllSupportedPlatforms = new List<UnrealTargetPlatform>();
					var SupportedPlatforms = GetType().GetCustomAttributes(typeof(SupportedPlatformsAttribute), false);
					// If none specified we assume all platforms are supported by default
					if (SupportedPlatforms.Length == 0)
					{
						UnrealTargetPlatform[] SupportedByDefault = { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Mac, UnrealTargetPlatform.Linux, UnrealTargetPlatform.Android };
						AllSupportedPlatforms.AddRange(SupportedByDefault);
					}
					else
					{
						foreach (var Platform in SupportedPlatforms)
						{
							AllSupportedPlatforms.AddRange(((SupportedPlatformsAttribute)Platform).Platforms);
						}
					}

					InsertOrUpdateTestFlag(ref lastUpdatedNode, TestMetadata.TestName, "SupportedPlatforms", AllSupportedPlatforms.Aggregate("", (current, next) => (current == "" ? next.ToString() : current + ";" + next.ToString())));

					try
					{
						GenPropsDoc.Save(GeneratedPropertiesScriptFile);
					}
					catch (UnauthorizedAccessException)
					{
						// Expected on build machines.
						// TODO: Ability to build for generate files and runnable tests.
					}
				}
			}
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
				throw new Exception("Could not detect source folder path for module " + GetType());
			}
			return ModuleDirectory.Substring(0, SourceFolderIndex) + "Binaries";
		}

#pragma warning disable 8602
		private void InsertOrUpdateTestFlag(ref XElement ElementUpsertAfter, string TestName, string FlagSuffix, string FlagValue)
		{
			IEnumerable<XElement> NextChunk = ElementUpsertAfter.ElementsAfterSelf(BuildGraphNamespace + "Property")
				.Where(prop => prop.Attribute("Name").Value.EndsWith(FlagSuffix));
			if (NextChunk
				.Where(prop => prop.Attribute("Name").Value == TestName + FlagSuffix)
				.Count() == 0)
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

		private void InsertOrUpdateTestOption(ref XElement ElementUpsertAfter, string TestName, string TestShortName, string OptionPrefix, string OptionSuffix, string DefaultValue)
		{
			IEnumerable<XElement> NextChunk = ElementUpsertAfter.ElementsAfterSelf(BuildGraphNamespace + "Option")
				.Where(prop => prop.Attribute("Name").Value.StartsWith(OptionPrefix) && prop.Attribute("Name").Value.EndsWith(OptionSuffix));
			if (NextChunk
				.Where(prop => prop.Attribute("Name").Value == OptionPrefix + TestName + OptionSuffix)
				.Count() == 0)
			{
				XElement ElementInsert = new XElement(BuildGraphNamespace + "Option");
				ElementInsert.SetAttributeValue("Name", OptionPrefix + TestName + OptionSuffix);
				ElementInsert.SetAttributeValue("DefaultValue", DefaultValue);
				ElementInsert.SetAttributeValue("Description", string.Format("{0} {1} {2}", OptionPrefix, TestShortName, OptionSuffix));
				ElementUpsertAfter.AddAfterSelf(ElementInsert);
			}
			else
			{
				XElement ElementUpdate = NextChunk
					.Where(prop => prop.Attribute("Name").Value == OptionPrefix + TestName + OptionSuffix).First();
				ElementUpdate.SetAttributeValue("Description", string.Format("{0} {1} {2}", OptionPrefix, TestShortName, OptionSuffix));
				ElementUpdate.SetAttributeValue("DefaultValue", DefaultValue);
			}
			ElementUpsertAfter = NextChunk.Last();
		}
#pragma warning restore 8602

		/// <summary>
		/// Test metadata class.
		/// </summary>
		public class Metadata
		{
			/// <summary>
			/// Catch2 report type console - prints results to stdout.
			/// </summary>
			public static readonly string ConsoleReportType = "console";

			/// <summary>
			/// Test long name.
			/// </summary>
			public string TestName { get; }

			/// <summary>
			/// Test short name used for display in build system.
			/// </summary>
			public string TestShortName { get; }

			/// <summary>
			/// Type of Catch2 report, defaults to console.
			/// </summary>
			public string ReportType { get; }

			/// <summary>
			/// Is this test disabled?
			/// </summary>
			public bool Disabled { get; }

			/// <summary>
			/// Ctor that sets short name to test name.
			/// </summary>
			public Metadata(string InTestName)
			{
				TestShortName = TestName = InTestName;
				ReportType = ConsoleReportType;
			}

			/// <summary>
			/// Ctor that sets short name and test name.
			/// </summary>
			public Metadata(string InTestName, string InTestShortName)
			{
				TestName = InTestName;
				TestShortName = InTestShortName;
				ReportType = ConsoleReportType;
			}

			/// <summary>
			/// Ctor that sets short name, test name, report type and disabled status.
			/// </summary>
			public Metadata(string InTestName, string InTestShortName, string InReportType, bool InDisabled)
			{
				TestName = InTestName;
				TestShortName = InTestShortName;
				ReportType = InReportType;
				Disabled = InDisabled;
			}
		}
	}
}