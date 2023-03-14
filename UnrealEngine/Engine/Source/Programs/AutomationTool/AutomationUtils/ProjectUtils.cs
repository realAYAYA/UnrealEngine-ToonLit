// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using UnrealBuildTool;
using System.Diagnostics;
using EpicGames.Core;
using System.Reflection;
using UnrealBuildBase;
using System.Runtime.Serialization;
using System.Collections;

namespace AutomationTool
{
	public class SingleTargetProperties
	{
		public string TargetName;
		public string TargetClassName;
		public TargetRules Rules;
	}

	/// <summary>
	/// Autodetected project properties.
	/// </summary>
	public class ProjectProperties
	{
		/// <summary>
		/// Full Project path. Must be a .uproject file
		/// </summary>
		public FileReference RawProjectPath;

		/// <summary>
		/// True if the uproject contains source code.
		/// </summary>
		public bool bIsCodeBasedProject;

		/// <summary>
		/// List of all targets detected for this project.
		/// </summary>
		public List<SingleTargetProperties> Targets = new List<SingleTargetProperties>();

		/// <summary>
		/// List of all scripts that were compiled to create the list of Targets
		/// </summary>
		public List<FileReference> TargetScripts = new List<FileReference>();

		/// <summary>
		/// List of all Engine ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigHierarchy> EngineConfigs = new Dictionary<UnrealTargetPlatform,ConfigHierarchy>();

		/// <summary>
		/// List of all Game ini files for this project
		/// </summary>
		public Dictionary<UnrealTargetPlatform, ConfigHierarchy> GameConfigs = new Dictionary<UnrealTargetPlatform, ConfigHierarchy>();

		/// <summary>
		/// List of all programs detected for this project.
		/// </summary>
		public List<SingleTargetProperties> Programs = new List<SingleTargetProperties>();

		/// <summary>
		/// Specifies if the target files were generated
		/// </summary>
		public bool bWasGenerated = false;

		internal ProjectProperties()
		{
		}
	}

	/// <summary>
	/// Project related utility functions.
	/// </summary>
	public class ProjectUtils
	{

		/// <summary>
		/// Struct that acts as a key for the project property cache. Based on these attributes 
		/// DetectProjectProperties may return different answers, e.g. Some platforms require a 
		///  codebased project for targets
		/// </summary>
		struct PropertyCacheKey : IEquatable<PropertyCacheKey>
		{
			string ProjectName;

			UnrealTargetPlatform[] TargetPlatforms;

			UnrealTargetConfiguration[] TargetConfigurations;

			public PropertyCacheKey(string InProjectName, IEnumerable<UnrealTargetPlatform> InTargetPlatforms, IEnumerable<UnrealTargetConfiguration> InTargetConfigurations)
			{
				ProjectName = InProjectName.ToLower();
				TargetPlatforms = InTargetPlatforms != null ? InTargetPlatforms.ToArray() : new UnrealTargetPlatform[0];
				TargetConfigurations = InTargetConfigurations != null ? InTargetConfigurations.ToArray() : new UnrealTargetConfiguration[0];
			}

			public bool Equals(PropertyCacheKey Other)
			{
				return ProjectName == Other.ProjectName &&
						StructuralComparisons.StructuralEqualityComparer.Equals(TargetPlatforms, Other.TargetPlatforms) &&
						StructuralComparisons.StructuralEqualityComparer.Equals(TargetConfigurations, Other.TargetConfigurations);
			}

			public override bool Equals(object Other)
			{
				return Other is PropertyCacheKey OtherKey && Equals(OtherKey);
			}

			public override int GetHashCode()
			{
				return HashCode.Combine(
					ProjectName.GetHashCode(),
					StructuralComparisons.StructuralEqualityComparer.GetHashCode(TargetPlatforms),
					StructuralComparisons.StructuralEqualityComparer.GetHashCode(TargetConfigurations));
			}

			public static bool operator==(PropertyCacheKey A, PropertyCacheKey B)
			{
				return A.Equals(B);
			}

			public static bool operator!=(PropertyCacheKey A, PropertyCacheKey B)
			{
				return !(A == B);
			}
		}


		private static Dictionary<PropertyCacheKey, ProjectProperties> PropertiesCache = new Dictionary<PropertyCacheKey, ProjectProperties>();

		/// <summary>
		/// Gets a short project name (QAGame, Elemental, etc)
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="bIsUProjectFile">True if a uproject.</param>
		/// <returns>Short project name</returns>
		public static string GetShortProjectName(FileReference RawProjectPath)
		{
			return CommandUtils.GetFilenameWithoutAnyExtensions(RawProjectPath.FullName);
		}

		/// <summary>
		/// Gets a short alphanumeric identifier for the project path.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>Project path identifier</returns>
		public static string GetProjectPathId(FileReference RawProjectPath)
		{
			string UniformProjectPath = FileReference.FindCorrectCase(RawProjectPath).ToNormalizedPath();
			string ProjectPathHash = ContentHash.MD5(Encoding.UTF8.GetBytes(UniformProjectPath)).ToString();
			return String.Format("{0}.{1}", GetShortProjectName(RawProjectPath), ProjectPathHash.Substring(0, 8));
		}

		/// <summary>
		/// Gets project properties.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>Properties of the project.</returns>
		public static ProjectProperties GetProjectProperties(FileReference RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms = null, List<UnrealTargetConfiguration> ClientTargetConfigurations = null, bool AssetNativizationRequested = false)
		{
			string ProjectKey = "UE4";
			if (RawProjectPath != null)
			{
				ProjectKey = CommandUtils.ConvertSeparators(PathSeparator.Slash, RawProjectPath.FullName);
			}
			ProjectProperties Properties;
			PropertyCacheKey PropertyKey = new PropertyCacheKey(ProjectKey, ClientTargetPlatforms, ClientTargetConfigurations);

			if (PropertiesCache.TryGetValue(PropertyKey, out Properties) == false)
			{
                Properties = DetectProjectProperties(RawProjectPath, ClientTargetPlatforms, ClientTargetConfigurations, AssetNativizationRequested);
				PropertiesCache.Add(PropertyKey, Properties);
			}
			return Properties;
		}

		/// <summary>
		/// Checks if the project is a UProject file with source code.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>True if the project is a UProject file with source code.</returns>
		public static bool IsCodeBasedUProjectFile(FileReference RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms = null, List < UnrealTargetConfiguration> ClientTargetConfigurations = null)
		{
			return GetProjectProperties(RawProjectPath, ClientTargetPlatforms, ClientTargetConfigurations).bIsCodeBasedProject;
		}

		/// <summary>
		/// Checks if the project is a UProject file with source code.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>True if the project is a UProject file with source code.</returns>
		public static bool IsCodeBasedUProjectFile(FileReference RawProjectPath, UnrealTargetPlatform ClientTargetPlatform, List<UnrealTargetConfiguration> ClientTargetConfigurations = null)
		{
			return GetProjectProperties(RawProjectPath, new List<UnrealTargetPlatform>() { ClientTargetPlatform }, ClientTargetConfigurations).bIsCodeBasedProject;
		}

		/// <summary>
		/// Returns a path to the client binaries folder.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="Platform">Platform type.</param>
		/// <returns>Path to the binaries folder.</returns>
		public static DirectoryReference GetProjectClientBinariesFolder(DirectoryReference ProjectClientBinariesPath, UnrealTargetPlatform Platform)
		{
			ProjectClientBinariesPath = DirectoryReference.Combine(ProjectClientBinariesPath, Platform.ToString());
			return ProjectClientBinariesPath;
		}

		private static bool ProjectHasCode(FileReference RawProjectPath)
		{
			// check to see if we already have a Target.cs file
			if (File.Exists(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Source", RawProjectPath.GetFileNameWithoutExtension() + ".Target.cs")))
			{
				return true;
			}
			else if (Directory.Exists(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Source")))
			{
				// wasn't one in the main Source directory, let's check all sub-directories
				//@todo: may want to read each target.cs to see if it has a target corresponding to the project name as a final check
				FileInfo[] Files = (new DirectoryInfo(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Source")).GetFiles("*.Target.cs", SearchOption.AllDirectories));
				if (Files.Length > 0)
				{
					return true;
				}
			}
			return false;
		}

		private static bool RequiresTempTarget(FileReference RawProjectPath, List<UnrealTargetPlatform> Platforms, List<UnrealTargetConfiguration> Configurations, bool AssetNativizationRequested)
		{
			bool bHasCode = ProjectHasCode(RawProjectPath);
			foreach (UnrealTargetPlatform Platform in Platforms)
			{
				foreach(UnrealTargetConfiguration Configuration in Configurations)
				{
					string Reason;
					if(RequiresTempTarget(RawProjectPath, bHasCode, Platform, Configuration, TargetType.Game, AssetNativizationRequested, true, out Reason))
					{
						Log.TraceInformation("{0} requires a temporary target.cs to be generated ({1})", RawProjectPath.GetFileName(), Reason);
						return true;
					}
				}
			}
			return false;
		}

		/// <summary>
		/// NOTE: This function must mirror the functionality of TargetPlatformBase::RequiresTempTarget
		/// </summary>
		private static bool RequiresTempTarget(FileReference RawProjectPath, bool bProjectHasCode, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, TargetType TargetType, bool bRequiresAssetNativization, bool bRequiresCookedData, out string OutReason)
		{
			// check to see if we already have a Target.cs file
			if (bProjectHasCode)
			{
				OutReason = null;
				return false;
			}

			// check if asset nativization is enabled
			if (bRequiresAssetNativization)
            {
				OutReason = "asset nativization is enabled";
                return true;
            }

			// Check if encryption or signing is enabled
			EncryptionAndSigning.CryptoSettings Settings = EncryptionAndSigning.ParseCryptoSettings(RawProjectPath.Directory, Platform, Log.Logger);
			if (Settings.IsAnyEncryptionEnabled() || Settings.IsPakSigningEnabled())
			{
				OutReason = "encryption/signing is enabled";
				return true;
			}

			// check the target platforms for any differences in build settings or additional plugins
			if(!Unreal.IsEngineInstalled() && !PlatformExports.HasDefaultBuildConfig(RawProjectPath, Platform))
			{
				OutReason = "project has non-default build configuration";
				return true;
			}
			if(PlatformExports.RequiresBuild(RawProjectPath, Platform))
			{
				OutReason = "overriden by target platform";
				return true;
			}

			// Read the project descriptor, and find all the plugins available to this project
			ProjectDescriptor Project = ProjectDescriptor.FromFile(RawProjectPath);

			// Enumerate all the available plugins
			Dictionary<string, PluginInfo> AllPlugins = Plugins.ReadAvailablePlugins(Unreal.EngineDirectory, DirectoryReference.FromFile(RawProjectPath), new List<DirectoryReference>()).ToDictionary(x => x.Name, x => x, StringComparer.OrdinalIgnoreCase);

			// find if there are any plugins enabled or disabled which differ from the default
			string Reason;
			if (RequiresTempTargetForCodePlugin(Project, Platform, Configuration, TargetType, AllPlugins, out Reason))
			{
				OutReason = Reason;
				return true;
			}

			OutReason = null;
			return false;
		}

		/// <summary>
		/// NOTE: This function must mirror FPluginManager::RequiresTempTargetForCodePlugin
		/// </summary>
		static bool RequiresTempTargetForCodePlugin(ProjectDescriptor ProjectDescriptor, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, TargetType TargetType, Dictionary<string, PluginInfo> AllPlugins, out string OutReason)
		{
			PluginReferenceDescriptor MissingPlugin;

			HashSet<string> ProjectCodePlugins = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			if (!GetCodePluginsForTarget(ProjectDescriptor, Platform, Configuration, TargetType, ProjectCodePlugins, AllPlugins, out MissingPlugin))
			{
				OutReason = String.Format("{0} plugin is referenced by target but not found", MissingPlugin.Name);
				return true;
			}

			HashSet<string> DefaultCodePlugins = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			if (!GetCodePluginsForTarget(null, Platform, Configuration, TargetType, DefaultCodePlugins, AllPlugins, out MissingPlugin))
			{
				OutReason = String.Format("{0} plugin is referenced by the default target but not found", MissingPlugin.Name);
				return true;
			}

			foreach (string ProjectCodePlugin in ProjectCodePlugins)
			{
				if (!DefaultCodePlugins.Contains(ProjectCodePlugin))
				{
					OutReason = String.Format("{0} plugin is enabled", ProjectCodePlugin);
					return true;
				}
			}

			foreach (string DefaultCodePlugin in DefaultCodePlugins)
			{
				if (!ProjectCodePlugins.Contains(DefaultCodePlugin))
				{
					OutReason = String.Format("{0} plugin is disabled", DefaultCodePlugin);
					return true;
				}
			}

			OutReason = null;
			return false;
		}

		/// <summary>
		/// NOTE: This function must mirror FPluginManager::GetCodePluginsForTarget
		/// </summary>
		static bool GetCodePluginsForTarget(ProjectDescriptor ProjectDescriptor, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, TargetType TargetType, HashSet<string> CodePluginNames, Dictionary<string, PluginInfo> AllPlugins, out PluginReferenceDescriptor OutMissingPlugin)
		{
			bool bLoadPluginsForTargetPlatforms = (TargetType == TargetType.Editor);

			// Map of all enabled plugins
			Dictionary<string, PluginInfo> EnabledPlugins = new Dictionary<string, PluginInfo>(StringComparer.OrdinalIgnoreCase);
					   			 
			// Keep a set of all the plugin names that have been configured. We read configuration data from different places, but only configure a plugin from the first place that it's referenced.
			HashSet<string> ConfiguredPluginNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

			bool bAllowEnginePluginsEnabledByDefault = true;

			// Find all the plugin references in the project file
			if (ProjectDescriptor != null)
			{
				bAllowEnginePluginsEnabledByDefault = !ProjectDescriptor.DisableEnginePluginsByDefault;
				if (ProjectDescriptor.Plugins != null)
				{
					// Copy the plugin references, since we may modify the project if any plugins are missing
					foreach (PluginReferenceDescriptor PluginReference in ProjectDescriptor.Plugins)
					{
						if (!ConfiguredPluginNames.Contains(PluginReference.Name))
						{
							PluginReferenceDescriptor MissingPlugin;
							if (!ConfigureEnabledPluginForTarget(PluginReference, ProjectDescriptor, null, Platform, Configuration, TargetType, bLoadPluginsForTargetPlatforms, AllPlugins, EnabledPlugins, out MissingPlugin))
							{
								OutMissingPlugin = MissingPlugin;
								return false;
							}
							ConfiguredPluginNames.Add(PluginReference.Name);
						}
					}
				}
			}

			// Add the plugins which are enabled by default
			foreach (KeyValuePair<string, PluginInfo> PluginPair in AllPlugins)
			{
				if (PluginPair.Value.IsEnabledByDefault(bAllowEnginePluginsEnabledByDefault) && !ConfiguredPluginNames.Contains(PluginPair.Key))
				{
					PluginReferenceDescriptor MissingPlugin;
					if (!ConfigureEnabledPluginForTarget(new PluginReferenceDescriptor(PluginPair.Key, null, true), ProjectDescriptor, null, Platform, Configuration, TargetType, bLoadPluginsForTargetPlatforms, AllPlugins, EnabledPlugins, out MissingPlugin))
					{
						OutMissingPlugin = MissingPlugin;
						return false;
					}
					ConfiguredPluginNames.Add(PluginPair.Key);
				}
			}

			// Figure out which plugins have code 
			bool bBuildDeveloperTools = (TargetType == TargetType.Editor || TargetType == TargetType.Program || (Configuration != UnrealTargetConfiguration.Test && Configuration != UnrealTargetConfiguration.Shipping));
			bool bRequiresCookedData = (TargetType != TargetType.Editor);
			foreach (KeyValuePair<string, PluginInfo> Pair in EnabledPlugins)
			{
				if (Pair.Value.Descriptor.Modules != null)
				{
					foreach (ModuleDescriptor Module in Pair.Value.Descriptor.Modules)
					{
						if (Module.IsCompiledInConfiguration(Platform, Configuration, null, TargetType, bBuildDeveloperTools, bRequiresCookedData))
						{
							CodePluginNames.Add(Pair.Key);
							break;
						}
					}
				}
			}

			OutMissingPlugin = null;
			return true;
		}

		/// <summary>
		/// NOTE: This function should mirror FPluginManager::ConfigureEnabledPluginForTarget
		/// </summary>
		static bool ConfigureEnabledPluginForTarget(PluginReferenceDescriptor FirstReference, ProjectDescriptor ProjectDescriptor, string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, TargetType TargetType, bool bLoadPluginsForTargetPlatforms, Dictionary<string, PluginInfo> AllPlugins, Dictionary<string, PluginInfo> EnabledPlugins, out PluginReferenceDescriptor OutMissingPlugin)
		{
			if (!EnabledPlugins.ContainsKey(FirstReference.Name))
			{
				// Set of plugin names we've added to the queue for processing
				HashSet<string> NewPluginNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
				NewPluginNames.Add(FirstReference.Name);

				// Queue of plugin references to consider
				List<PluginReferenceDescriptor> NewPluginReferences = new List<PluginReferenceDescriptor>();
				NewPluginReferences.Add(FirstReference);

				// Loop through the queue of plugin references that need to be enabled, queuing more items as we go
				for (int Idx = 0; Idx < NewPluginReferences.Count; Idx++)
				{
					PluginReferenceDescriptor Reference = NewPluginReferences[Idx];

					// Check if the plugin is required for this platform
					if(!Reference.IsEnabledForPlatform(Platform) || !Reference.IsEnabledForTargetConfiguration(Configuration) || !Reference.IsEnabledForTarget(TargetType))
					{
						Log.TraceLog("Ignoring plugin '{0}' for platform/configuration", Reference.Name);
						continue;
					}

					// Check if the plugin is required for this platform
					if(!bLoadPluginsForTargetPlatforms && !Reference.IsSupportedTargetPlatform(Platform))
					{
						Log.TraceLog("Ignoring plugin '{0}' due to unsupported platform", Reference.Name);
						continue;
					}

					// Find the plugin being enabled
					PluginInfo Plugin;
					if (!AllPlugins.TryGetValue(Reference.Name, out Plugin))
					{
						// Ignore any optional plugins
						if (Reference.bOptional)
						{
							Log.TraceLog("Ignored optional reference to '%s' plugin; plugin was not found.", Reference.Name);
							continue;
						}

						// Add it to the missing list
						OutMissingPlugin = Reference;
						return false;
					}

					// Check the plugin supports this platform
					if(!bLoadPluginsForTargetPlatforms && !Plugin.Descriptor.SupportsTargetPlatform(Platform))
					{
						Log.TraceLog("Ignoring plugin '{0}' due to unsupported platform in plugin descriptor", Reference.Name);
						continue;
					}

					// Check that this plugin supports the current program
					if (TargetType == TargetType.Program && !Plugin.Descriptor.SupportedPrograms.Contains(TargetName))
					{
						Log.TraceLog("Ignoring plugin '{0}' due to absence from the supported programs list", Reference.Name);
						continue;
					}

					// Add references to all its dependencies
					if (Plugin.Descriptor.Plugins != null)
					{
						foreach (PluginReferenceDescriptor NextReference in Plugin.Descriptor.Plugins)
						{
							if (!EnabledPlugins.ContainsKey(NextReference.Name) && !NewPluginNames.Contains(NextReference.Name))
							{
								NewPluginNames.Add(NextReference.Name);
								NewPluginReferences.Add(NextReference);
							}
						}
					}

					// Add the plugin
					EnabledPlugins.Add(Plugin.Name, Plugin);
				}
			}

			OutMissingPlugin = null;
			return true;
		}

		private static void GenerateTempTarget(FileReference RawProjectPath)
		{
			DirectoryReference TempDir = DirectoryReference.Combine(RawProjectPath.Directory, "Intermediate", "Source");
			DirectoryReference.CreateDirectory(TempDir);

			// Get the project name for use in temporary files
			string ProjectName = RawProjectPath.GetFileNameWithoutExtension();

			// Create a target.cs file
			MemoryStream TargetStream = new MemoryStream();
			using (StreamWriter Writer = new StreamWriter(TargetStream))
			{
				Writer.WriteLine("using UnrealBuildTool;");
				Writer.WriteLine();
				Writer.WriteLine("public class {0}Target : TargetRules", ProjectName);
				Writer.WriteLine("{");
				Writer.WriteLine("\tpublic {0}Target(TargetInfo Target) : base(Target)", ProjectName);
				Writer.WriteLine("\t{");
				Writer.WriteLine("\t\tDefaultBuildSettings = BuildSettingsVersion.V2;");
				Writer.WriteLine("\t\tType = TargetType.Game;");
				Writer.WriteLine("\t\tExtraModuleNames.Add(\"{0}\");", ProjectName);
				Writer.WriteLine("\t}");
				Writer.WriteLine("}");
			}
			FileReference TargetLocation = FileReference.Combine(TempDir, ProjectName + ".Target.cs");
			FileReference.WriteAllBytesIfDifferent(TargetLocation, TargetStream.ToArray());

			// Create a build.cs file
			MemoryStream ModuleStream = new MemoryStream();
			using (StreamWriter Writer = new StreamWriter(ModuleStream))
			{
				Writer.WriteLine("using UnrealBuildTool;");
				Writer.WriteLine();
				Writer.WriteLine("public class {0} : ModuleRules", ProjectName);
				Writer.WriteLine("{");
				Writer.WriteLine("\tpublic {0}(ReadOnlyTargetRules Target) : base(Target)", ProjectName);
				Writer.WriteLine("\t{");
				Writer.WriteLine("\t\tPCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;");
				Writer.WriteLine();
				Writer.WriteLine("\t\tPrivateDependencyModuleNames.Add(\"Core\");");
				Writer.WriteLine("\t\tPrivateDependencyModuleNames.Add(\"Core\");");
				Writer.WriteLine("\t}");
				Writer.WriteLine("}");
			}
			FileReference ModuleLocation = FileReference.Combine(TempDir, ProjectName + ".Build.cs");
			FileReference.WriteAllBytesIfDifferent(ModuleLocation, ModuleStream.ToArray());

			// Create a main module cpp file
			MemoryStream SourceFileStream = new MemoryStream();
			using (StreamWriter Writer = new StreamWriter(SourceFileStream))
			{
				Writer.WriteLine("#include \"CoreTypes.h\"");
				Writer.WriteLine("#include \"Modules/ModuleManager.h\"");
				Writer.WriteLine();
				Writer.WriteLine("IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultModuleImpl, {0}, \"{0}\");", ProjectName);
			}
			FileReference SourceFileLocation = FileReference.Combine(TempDir, ProjectName + ".cpp");
			FileReference.WriteAllBytesIfDifferent(SourceFileLocation, SourceFileStream.ToArray());
		}

		/// <summary>
		/// Attempts to autodetect project properties.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <returns>Project properties.</returns>
        private static ProjectProperties DetectProjectProperties(FileReference RawProjectPath, List<UnrealTargetPlatform> ClientTargetPlatforms, List<UnrealTargetConfiguration> ClientTargetConfigurations, bool AssetNativizationRequested)
		{
			ProjectProperties Properties = new ProjectProperties();
			Properties.RawProjectPath = RawProjectPath;

			// detect if the project is content only, but has non-default build settings
			List<string> ExtraSearchPaths = null;
			if (RawProjectPath != null)
			{
				// no Target file, now check to see if build settings have changed
				List<UnrealTargetPlatform> TargetPlatforms = ClientTargetPlatforms;
				if (ClientTargetPlatforms == null || ClientTargetPlatforms.Count < 1)
				{
					// No client target platforms, add all in
					TargetPlatforms = new List<UnrealTargetPlatform>();
					foreach (UnrealTargetPlatform TargetPlatformType in UnrealTargetPlatform.GetValidPlatforms())
					{
						TargetPlatforms.Add(TargetPlatformType);
					}
				}

				List<UnrealTargetConfiguration> TargetConfigurations = ClientTargetConfigurations;
				if (TargetConfigurations == null || TargetConfigurations.Count < 1)
				{
					// No client target configurations, add all in
					TargetConfigurations = new List<UnrealTargetConfiguration>();
					foreach (UnrealTargetConfiguration TargetConfigurationType in Enum.GetValues(typeof(UnrealTargetConfiguration)))
					{
						if (TargetConfigurationType != UnrealTargetConfiguration.Unknown)
						{
							TargetConfigurations.Add(TargetConfigurationType);
						}
					}
				}

				string TempTargetDir = CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath.FullName), "Intermediate", "Source");
                if (RequiresTempTarget(RawProjectPath, TargetPlatforms, TargetConfigurations, AssetNativizationRequested))
				{
					GenerateTempTarget(RawProjectPath);
					Properties.bWasGenerated = true;
					ExtraSearchPaths = new List<string>();
                    ExtraSearchPaths.Add(TempTargetDir);
				}
				else if (File.Exists(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Intermediate", "Source", Path.GetFileNameWithoutExtension(RawProjectPath.FullName) + ".Target.cs")))
				{
					File.Delete(Path.Combine(Path.GetDirectoryName(RawProjectPath.FullName), "Intermediate", "Source", Path.GetFileNameWithoutExtension(RawProjectPath.FullName) + ".Target.cs"));
				}

                // in case the RulesCompiler (what we use to find all the 
                // Target.cs files) has already cached the contents of this 
                // directory, then we need to invalidate that cache (so 
                // it'll find/use the new Target.cs file)
                Rules.InvalidateRulesFileCache(TempTargetDir);
            }

			if (CommandUtils.CmdEnv.HasCapabilityToCompile)
			{
				DetectTargetsForProject(Properties, ExtraSearchPaths);
				Properties.bIsCodeBasedProject = !CommandUtils.IsNullOrEmpty(Properties.Targets) || !CommandUtils.IsNullOrEmpty(Properties.Programs);
			}
			else
			{
				// should never ask for engine targets if we can't compile
				if (RawProjectPath == null)
				{
					throw new AutomationException("Cannot determine engine targets if we can't compile.");
				}

				Properties.bIsCodeBasedProject = Properties.bWasGenerated;
				// if there's a Source directory with source code in it, then mark us as having source code
				string SourceDir = CommandUtils.CombinePaths(Path.GetDirectoryName(RawProjectPath.FullName), "Source");
				if (Directory.Exists(SourceDir))
				{
					string[] CppFiles = Directory.GetFiles(SourceDir, "*.cpp", SearchOption.AllDirectories);
					string[] HFiles = Directory.GetFiles(SourceDir, "*.h", SearchOption.AllDirectories);
					Properties.bIsCodeBasedProject |= (CppFiles.Length > 0 || HFiles.Length > 0);
				}
			}

			// check to see if the uproject loads modules, only if we haven't already determined it is a code based project
			if (!Properties.bIsCodeBasedProject && RawProjectPath != null)
			{
				string uprojectStr = File.ReadAllText(RawProjectPath.FullName);
				Properties.bIsCodeBasedProject = uprojectStr.Contains("\"Modules\"");
			}

			// Get all ini files
			if (RawProjectPath != null)
			{
				CommandUtils.LogVerbose("Loading ini files for {0}", RawProjectPath);

				foreach (UnrealTargetPlatform TargetPlatformType in UnrealTargetPlatform.GetValidPlatforms())
				{
					ConfigHierarchy EngineConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, RawProjectPath.Directory, TargetPlatformType);
					Properties.EngineConfigs.Add(TargetPlatformType, EngineConfig);

					ConfigHierarchy GameConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, RawProjectPath.Directory, TargetPlatformType);
					Properties.GameConfigs.Add(TargetPlatformType, GameConfig);
				}
			}

			return Properties;
		}


		/// <summary>
		/// Gets the project's root binaries folder.
		/// </summary>
		/// <param name="RawProjectPath">Full project path.</param>
		/// <param name="TargetType">Target type.</param>
		/// <param name="bIsUProjectFile">True if uproject file.</param>
		/// <returns>Binaries path.</returns>
		public static DirectoryReference GetClientProjectBinariesRootPath(FileReference RawProjectPath, TargetType TargetType, bool bIsCodeBasedProject)
		{
			DirectoryReference BinPath = null;
			switch (TargetType)
			{
				case TargetType.Program:
					BinPath = DirectoryReference.Combine(Unreal.RootDirectory, "Engine", "Binaries");
					break;
				case TargetType.Client:
				case TargetType.Game:
					if (!bIsCodeBasedProject)
					{
						BinPath = DirectoryReference.Combine(Unreal.RootDirectory, "Engine", "Binaries");
					}
					else
					{
						BinPath = DirectoryReference.Combine(RawProjectPath.Directory, "Binaries");
					}
					break;
			}
			return BinPath;
		}

		/// <summary>
		/// Gets the location where all rules assemblies should go
		/// </summary>
		private static string GetRulesAssemblyFolder()
		{
			string RulesFolder;
			if (Unreal.IsEngineInstalled())
			{
				RulesFolder = CommandUtils.CombinePaths(Path.GetTempPath(), "UAT", CommandUtils.EscapePath(CommandUtils.CmdEnv.LocalRoot), "Rules"); 
			}
			else
			{
				RulesFolder = CommandUtils.CombinePaths(CommandUtils.CmdEnv.EngineSavedFolder, "Rules");
			}
			return RulesFolder;
		}

		/// <summary>
		/// Finds all targets for the project.
		/// </summary>
		/// <param name="Properties">Project properties.</param>
		/// <param name="ExtraSearchPaths">Additional search paths.</param>
		private static void DetectTargetsForProject(ProjectProperties Properties, List<string> ExtraSearchPaths = null)
		{
			Properties.Targets = new List<SingleTargetProperties>();
			FileReference TargetsDllFilename;
			string FullProjectPath = null;

			List<DirectoryReference> GameFolders = new List<DirectoryReference>();
			DirectoryReference RulesFolder = new DirectoryReference(GetRulesAssemblyFolder());
			if (Properties.RawProjectPath != null)
			{
				CommandUtils.LogVerbose("Looking for targets for project {0}", Properties.RawProjectPath);

				TargetsDllFilename = FileReference.Combine(RulesFolder, String.Format("UATRules-{0}.dll", ContentHash.MD5(Properties.RawProjectPath.FullName.ToUpperInvariant()).ToString()));

				FullProjectPath = CommandUtils.GetDirectoryName(Properties.RawProjectPath.FullName);
				GameFolders.Add(new DirectoryReference(FullProjectPath));
				CommandUtils.LogVerbose("Searching for target rule files in {0}", FullProjectPath);
			}
			else
			{
				TargetsDllFilename = FileReference.Combine(RulesFolder, String.Format("UATRules{0}.dll", "_BaseEngine_"));
			}

			// the UBT code assumes a certain CWD, but artists don't have this CWD.
			string SourceDir = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine", "Source");
			bool DirPushed = false;
			if (CommandUtils.DirectoryExists_NoExceptions(SourceDir))
			{
				CommandUtils.PushDir(SourceDir);
				DirPushed = true;
			}
			List<DirectoryReference> ExtraSearchDirectories = (ExtraSearchPaths == null)? null : ExtraSearchPaths.Select(x => new DirectoryReference(x)).ToList();
			List<FileReference> TargetScripts = Rules.FindAllRulesSourceFiles(Rules.RulesFileType.Target, GameFolders: GameFolders, ForeignPlugins: null, AdditionalSearchPaths: ExtraSearchDirectories);
			if (DirPushed)
			{
				CommandUtils.PopDir();
			}

			if (!CommandUtils.IsNullOrEmpty(TargetScripts))
			{
				// We only care about project target script so filter out any scripts not in the project folder, or take them all if we are just doing engine stuff
				List<FileReference> ProjectTargetScripts = new List<FileReference>();
				foreach (FileReference TargetScript in TargetScripts)
				{
					if (FullProjectPath == null || TargetScript.IsUnderDirectory(new DirectoryReference(FullProjectPath)))
					{
						// skip target rules that are platform extension or platform group specializations
						string[] TargetPathSplit = TargetScript.GetFileNameWithoutAnyExtensions().Split(new char[]{'_'}, StringSplitOptions.RemoveEmptyEntries );
						if (TargetPathSplit.Length > 1 && (UnrealTargetPlatform.IsValidName(TargetPathSplit.Last()) || UnrealPlatformGroup.IsValidName(TargetPathSplit.Last()) ) )
						{
							continue;
						}

						ProjectTargetScripts.Add(TargetScript);
					}
				}
				TargetScripts = ProjectTargetScripts;
			}

			if (!CommandUtils.IsNullOrEmpty(TargetScripts))
			{
				CommandUtils.LogVerbose("Found {0} target rule files:", TargetScripts.Count);
				foreach (FileReference Filename in TargetScripts)
				{
					CommandUtils.LogVerbose("  {0}", Filename);
				}

				// Check if the scripts require compilation
				bool DoNotCompile = false;

				if (!CommandUtils.IsBuildMachine && !CheckIfScriptAssemblyIsOutOfDate(TargetsDllFilename, TargetScripts))
				{
					Log.TraceVerbose("Targets DLL {0} is up to date.", TargetsDllFilename);
					DoNotCompile = true;
				}
				if (!DoNotCompile && CommandUtils.FileExists_NoExceptions(TargetsDllFilename.FullName))
				{
					if (!CommandUtils.DeleteFile_NoExceptions(TargetsDllFilename.FullName, true))
					{
						DoNotCompile = true;
						CommandUtils.LogVerbose("Could not delete {0} assuming it is up to date and reusable for a recursive UAT call.", TargetsDllFilename);
					}
				}

				CompileAndLoadTargetsAssembly(Properties, TargetsDllFilename, DoNotCompile, TargetScripts);
			}
		}

		/// <summary>
		/// Optionally compiles and loads target rules assembly.
		/// </summary>
		/// <param name="Properties"></param>
		/// <param name="TargetsDllFilename"></param>
		/// <param name="DoNotCompile"></param>
		/// <param name="TargetScripts"></param>
		private static void CompileAndLoadTargetsAssembly(ProjectProperties Properties, FileReference TargetsDllFilename, bool DoNotCompile, List<FileReference> TargetScripts)
		{
			Properties.TargetScripts = new List<FileReference>(TargetScripts);

			CommandUtils.LogVerbose("Compiling targets DLL: {0}", TargetsDllFilename);

			List<string> ReferencedAssemblies = new List<string>() 
					{ 
						typeof(UnrealBuildTool.PlatformExports).Assembly.Location
					};
			List<string> PreprocessorDefinitions = RulesAssembly.GetPreprocessorDefinitions();
			Assembly TargetsDLL = DynamicCompilation.CompileAndLoadAssembly(TargetsDllFilename, new HashSet<FileReference>(TargetScripts), Log.Logger, ReferencedAssemblies, PreprocessorDefinitions, DoNotCompile);
			Type[] AllCompiledTypes = TargetsDLL.GetTypes();
			foreach (Type TargetType in AllCompiledTypes)
			{
				// Find TargetRules but skip all "UnrealEditor", "UnrealGame" targets.
				if (typeof(TargetRules).IsAssignableFrom(TargetType) && !TargetType.IsAbstract)
				{
					string TargetName = GetTargetName(TargetType);

					TargetInfo DummyTargetInfo = new TargetInfo(TargetName, BuildHostPlatform.Current.Platform, UnrealTargetConfiguration.Development, "", Properties.RawProjectPath, null);

					// Create an instance of this type
					CommandUtils.LogVerbose("Creating target rules object: {0}", TargetType.Name);
					TargetRules Rules = TargetRules.Create(TargetType, DummyTargetInfo, null, null, null, Log.Logger);
					CommandUtils.LogVerbose("Adding target: {0} ({1})", TargetType.Name, Rules.Type);

					SingleTargetProperties TargetData = new SingleTargetProperties();
					TargetData.TargetName = GetTargetName(TargetType);
					TargetData.TargetClassName = TargetType.FullName;
					TargetData.Rules = Rules;
					if (Rules.Type == global::UnrealBuildTool.TargetType.Program)
					{
						Properties.Programs.Add(TargetData);
					}
					else
					{
						Properties.Targets.Add(TargetData);
					}
				}
			}
		}

		/// <summary>
		/// Checks if any of the script files in newer than the generated assembly.
		/// </summary>
		/// <param name="TargetsDllFilename"></param>
		/// <param name="TargetScripts"></param>
		/// <returns>True if the generated assembly is out of date.</returns>
		private static bool CheckIfScriptAssemblyIsOutOfDate(FileReference TargetsDllFilename, List<FileReference> TargetScripts)
		{
			bool bOutOfDate = false;
			FileInfo AssemblyInfo = new FileInfo(TargetsDllFilename.FullName);
			if (AssemblyInfo.Exists)
			{
				foreach (FileReference ScriptFilename in TargetScripts)
				{
					FileInfo ScriptInfo = new FileInfo(ScriptFilename.FullName);
					if (ScriptInfo.Exists && ScriptInfo.LastWriteTimeUtc > AssemblyInfo.LastWriteTimeUtc)
					{
						bOutOfDate = true;
						break;
					}
				}
			}
			else
			{
				bOutOfDate = true;
			}
			return bOutOfDate;
		}

		/// <summary>
		/// Converts class type name (usually ends with Target) to a target name (without the postfix).
		/// </summary>
		/// <param name="TargetRulesType">Tagert class.</param>
		/// <returns>Target name</returns>
		private static string GetTargetName(Type TargetRulesType)
		{
			const string TargetPostfix = "Target";
			string Name = TargetRulesType.Name;
			if (Name.EndsWith(TargetPostfix, StringComparison.InvariantCultureIgnoreCase))
			{
				Name = Name.Substring(0, Name.Length - TargetPostfix.Length);
			}
			return Name;
		}

		/// <summary>
		/// Performs initial cleanup of target rules folder
		/// </summary>
		public static void CleanupFolders()
		{
			CommandUtils.LogVerbose("Cleaning up project rules folder");
			string RulesFolder = GetRulesAssemblyFolder();
			if (CommandUtils.DirectoryExists(RulesFolder))
			{
				CommandUtils.DeleteDirectoryContents(RulesFolder);
			}
		}

		/// <summary>
		/// Takes a game name (e.g "ShooterGame") and tries to find the path to the project file
		/// </summary>
		/// <param name="GameName"></param>
		/// <returns></returns>
		public static FileReference FindProjectFileFromName(string GameName)
		{
			// if they passed in a path then easy.
			if (File.Exists(GameName))
			{
				return new FileReference(GameName);
			}

			// Start with the gamename regardless of what they passed in
			GameName = Path.GetFileNameWithoutExtension(GameName);

			// Turn Foo into Foo.uproject
			string ProjectFile = GameName;

			if (string.IsNullOrEmpty(Path.GetExtension(ProjectFile)))
			{
				// if project was specified but had no extension then just add it.
				ProjectFile = Path.ChangeExtension(GameName, ".uproject");
			}

			// Turn Foo.uproject into Foo/Foo.uproject
			ProjectFile = Path.Combine(GameName, ProjectFile);

			GameName = Path.GetFileNameWithoutExtension(GameName);

			// check for sibling to engine
			if (File.Exists(ProjectFile))
			{
				return new FileReference(ProjectFile);
			}

			// Search NativeProjects (sibling folders).
			IEnumerable<FileReference> Projects = NativeProjects.EnumerateProjectFiles(Log.Logger);

			FileReference ProjectPath = Projects.Where(R => string.Equals(R.GetFileName(), ProjectFile, StringComparison.OrdinalIgnoreCase)).FirstOrDefault();

			if (ProjectPath == null)
			{
				// read .uprojectdirs
				List<string> SearchPaths = new List<string>();
				SearchPaths.Add("");
				string ProjectDirsFile = Directory.EnumerateFiles(Environment.CurrentDirectory, "*.uprojectdirs").FirstOrDefault();
				if (ProjectDirsFile != null)
				{
					foreach (string FilePath in File.ReadAllLines(ProjectDirsFile))
					{
						string Trimmed = FilePath.Trim();
						if (!Trimmed.StartsWith("./", StringComparison.OrdinalIgnoreCase) &&
							!Trimmed.StartsWith(";", StringComparison.OrdinalIgnoreCase) &&
							Trimmed.IndexOfAny(Path.GetInvalidPathChars()) < 0)
						{
							SearchPaths.Add(Trimmed);
						}
					}

					string ResolvedFile = SearchPaths.Select(P => Path.Combine(P, ProjectFile))
											.Where(P => File.Exists(P))
											.FirstOrDefault();

					if (ResolvedFile != null)
					{
						ProjectPath = new FileReference(ResolvedFile);
					}
				}
			}
						
			// either valid or we're out of ideas...
			return ProjectPath;
		}

		/// <summary>
		/// Full path to the Project executable for the current platform.
		/// </summary>
		/// <param name="ProjectFile">Path to Project file</param>
		/// <param name="TargetType">Target type</param>
		/// <param name="TargetPlatform">Target platform</param>
		/// <param name="TargetConfiguration">Target build configuration</param>
		/// <param name="Cmd">Do you want the console subsystem/commandlet executable?</param>
		/// <returns></returns>
		public static FileSystemReference GetProjectTarget(FileReference ProjectFile, UnrealBuildTool.TargetType TargetType, UnrealBuildTool.UnrealTargetPlatform TargetPlatform, UnrealBuildTool.UnrealTargetConfiguration TargetConfiguration = UnrealBuildTool.UnrealTargetConfiguration.Development, bool Cmd = false)
		{
			ProjectProperties Properties = ProjectUtils.GetProjectProperties(ProjectFile);
			List<SingleTargetProperties> Targets = Properties.Targets.Where(x => x.Rules.Type == TargetType).ToList();
			string TargetName = null;
			switch (Targets.Count)
			{
				case 0:
					return null;
				case 1:
					TargetName = Targets.First().TargetName;
					break;
				default:
					Properties.EngineConfigs[TargetPlatform].GetString("/Script/BuildSettings.BuildSettings", "DefaultEditorTarget", out TargetName);
					break;
			}

			FileReference TargetReceiptFileName = UnrealBuildTool.TargetReceipt.GetDefaultPath(ProjectFile.Directory, TargetName, TargetPlatform, TargetConfiguration, "");
			UnrealBuildTool.TargetReceipt TargetReceipt = UnrealBuildTool.TargetReceipt.Read(TargetReceiptFileName);

			if (Cmd)
			{
				return TargetReceipt.LaunchCmd;
			}
			
			if (TargetPlatform == UnrealTargetPlatform.Mac)
			{
				// Remove trailing "/Contents/MacOS/UnrealEngine" to get back to .app directory
				return TargetReceipt.Launch.Directory.ParentDirectory.ParentDirectory;
			}
			
			return TargetReceipt.Launch;
		}
	}

    public class BranchInfo
    {
		[DebuggerDisplay("{GameName}")]
        public class BranchUProject
        {
            public string GameName;
            public FileReference FilePath;

			private ProjectProperties CachedProperties;
			
			public ProjectProperties Properties
			{
				get
				{
					if(CachedProperties == null)
					{
						CachedProperties = ProjectUtils.GetProjectProperties(FilePath);
					}
					return CachedProperties;
				}
			}

            public BranchUProject(FileReference ProjectFile)
            {
                GameName = ProjectFile.GetFileNameWithoutExtension();

                //not sure what the heck this path is relative to
                FilePath = ProjectFile;

                if (!CommandUtils.FileExists_NoExceptions(FilePath.FullName))
                {
                    throw new AutomationException("Could not resolve relative path corrctly {0} -> {1} which doesn't exist.", ProjectFile, FilePath);
                }
            }
        }

		public List<BranchUProject> AllProjects = new List<BranchUProject>();

        public BranchInfo()
        {
            IEnumerable<FileReference> ProjectFiles = UnrealBuildTool.NativeProjects.EnumerateProjectFiles(Log.Logger);
			foreach (FileReference InfoEntry in ProjectFiles)
			{
				AllProjects.Add(new BranchUProject(InfoEntry));
			}

			CommandUtils.LogVerbose("  {0} projects:", AllProjects.Count);
			foreach (BranchUProject Proj in AllProjects)
			{
				CommandUtils.LogLog(" {0}: {1}", Proj.GameName, Proj.FilePath);
			}
        }

        public BranchUProject FindGame(string GameName)
        {
			foreach (BranchUProject Proj in AllProjects)
            {
                if (Proj.GameName.Equals(GameName, StringComparison.InvariantCultureIgnoreCase))
                {
                    return Proj;
                }
            }
            return null;
        }

		public BranchUProject FindGameChecked(string GameName)
		{
			BranchUProject Project = FindGame(GameName);
			if(Project == null)
			{
				throw new AutomationException("Cannot find project '{0}' in branch", GameName);
			}
			return Project;
		}
    }
}
