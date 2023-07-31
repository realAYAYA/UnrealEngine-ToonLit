// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.Core;

namespace UnrealBuildTool
{
	/// <summary>
	/// The version format for .uplugin files. This rarely changes now; plugin descriptors should maintain backwards compatibility automatically.
	/// </summary>
	public enum PluginDescriptorVersion
	{
		/// <summary>
		/// Invalid
		/// </summary>
		Invalid = 0,

		/// <summary>
		/// Initial version
		/// </summary>
		Initial = 1,

		/// <summary>
		/// Adding SampleNameHash
		/// </summary>
		NameHash = 2,

		/// <summary>
		/// Unifying plugin/project files (since abandoned, but backwards compatibility maintained)
		/// </summary>
		ProjectPluginUnification = 3,

		/// <summary>
        /// This needs to be the last line, so we can calculate the value of Latest below
		/// </summary>
        LatestPlusOne,

		/// <summary>
		/// The latest plugin descriptor version
		/// </summary>
		Latest = LatestPlusOne - 1
	}

	/// <summary>
	/// In-memory representation of a .uplugin file
	/// </summary>
	public class PluginDescriptor
	{
		/// <summary>
		/// Descriptor version number
		/// </summary>
		public int FileVersion;

		/// <summary>
		/// Version number for the plugin.  The version number must increase with every version of the plugin, so that the system 
		/// can determine whether one version of a plugin is newer than another, or to enforce other requirements.  This version
		/// number is not displayed in front-facing UI.  Use the VersionName for that.
		/// </summary>
		public int Version;

		/// <summary>
		/// Name of the version for this plugin.  This is the front-facing part of the version number.  It doesn't need to match
		/// the version number numerically, but should be updated when the version number is increased accordingly.
		/// </summary>
		public string? VersionName;

		/// <summary>
		/// Friendly name of the plugin
		/// </summary>
		public string? FriendlyName;

		/// <summary>
		/// Description of the plugin
		/// </summary>
		public string? Description;

		/// <summary>
		/// The name of the category this plugin
		/// </summary>
		public string? Category;

		/// <summary>
		/// The company or individual who created this plugin.  This is an optional field that may be displayed in the user interface.
		/// </summary>
		public string? CreatedBy;

		/// <summary>
		/// Hyperlink URL string for the company or individual who created this plugin.  This is optional.
		/// </summary>
		public string? CreatedByURL;

		/// <summary>
		/// Documentation URL string.
		/// </summary>
		public string? DocsURL;

		/// <summary>
		/// Marketplace URL for this plugin. This URL will be embedded into projects that enable this plugin, so we can redirect to the marketplace if a user doesn't have it installed.
		/// </summary>
		public string? MarketplaceURL;

		/// <summary>
		/// Support URL/email for this plugin.
		/// </summary>
		public string? SupportURL;

		/// <summary>
		/// Sets the version of the engine that this plugin is compatible with.
		/// </summary>
		public string? EngineVersion;

		/// <summary>4
		/// If true, this plugin from a platform extension extending another plugin */
		/// </summary>
		public bool bIsPluginExtension;

		/// <summary>
		/// List of platforms supported by this plugin. This list will be copied to any plugin reference from a project file, to allow filtering entire plugins from staged builds.
		/// </summary>
		public List<UnrealTargetPlatform>? SupportedTargetPlatforms;

		/// <summary>
		/// List of programs supported by this plugin.
		/// </summary>
		public string[]? SupportedPrograms;

		/// <summary>
		/// List of all modules associated with this plugin
		/// </summary>
		public List<ModuleDescriptor>? Modules;

		/// <summary>
		/// List of all localization targets associated with this plugin
		/// </summary>
		public LocalizationTargetDescriptor[]? LocalizationTargets;

		/// <summary>
		/// The Verse path to the root of this plugin's content directory
		/// </summary>
		public string? VersePath;

		/// <summary>
		/// Origin/visibility of Verse code in this plugin's Content/Verse folder
		/// </summary>
		public VerseScope VerseScope = VerseScope.User;

		/// <summary>
		/// Whether this plugin should be enabled by default for all projects
		/// </summary>
		public Nullable<bool> bEnabledByDefault;

		/// <summary>
		/// Can this plugin contain content?
		/// </summary>
		public bool bCanContainContent;

		/// <summary>
		/// Can this plugin contain Verse code (either in content directory or in any of its modules)?
		/// </summary>
		public bool bCanContainVerse;

		/// <summary>
		/// Marks the plugin as beta in the UI
		/// </summary>
		public bool bIsBetaVersion;

		/// <summary>
		/// Marks the plugin as experimental in the UI
		/// </summary>
		public bool bIsExperimentalVersion;

		/// <summary>
		/// Set for plugins which are installed
		/// </summary>
		public bool bInstalled;

		/// <summary>
		/// For plugins that are under a platform folder (eg. /IOS/), determines whether compiling the plugin requires the build platform and/or SDK to be available
		/// </summary>
		public bool bRequiresBuildPlatform;

		/// <summary>
		/// When true, this plugin's modules will not be loaded automatically nor will it's content be mounted automatically. It will load/mount when explicitly requested and LoadingPhases will be ignored
		/// </summary>
		public bool bExplicitlyLoaded;

		/// <summary>
		/// When true, an empty SupportedTargetPlatforms is interpeted as 'no platforms' with the expectation that explict platforms will be added in plugin platform extensions
		/// </summary>
		public bool bHasExplicitPlatforms;

		/// <summary>
		/// Set of pre-build steps to execute, keyed by host platform name.
		/// </summary>
		public CustomBuildSteps? PreBuildSteps;

		/// <summary>
		/// Set of post-build steps to execute, keyed by host platform name.
		/// </summary>
		public CustomBuildSteps? PostBuildSteps;

		/// <summary>
		/// Additional plugins that this plugin depends on
		/// </summary>
		public List<PluginReferenceDescriptor>? Plugins;

		/// <summary>
		/// Private constructor. This object should not be created directly; read it from disk using FromFile() instead.
		/// </summary>
		private PluginDescriptor()
		{
			FileVersion = (int)PluginDescriptorVersion.Latest;
		}

		/// <summary>
		/// Reads a plugin descriptor from a json object
		/// </summary>
		/// <param name="RawObject">The object to read from</param>
		/// <param name="PluginPath"></param>
		/// <returns>New plugin descriptor</returns>
		public PluginDescriptor(JsonObject RawObject, FileReference PluginPath)
		{
			// Read the version
			if (!RawObject.TryGetIntegerField("FileVersion", out FileVersion))
			{
				if (!RawObject.TryGetIntegerField("PluginFileVersion", out FileVersion))
				{
					throw new BuildException("Plugin descriptor does not contain a valid FileVersion entry");
				}
			}

			// Check it's not newer than the latest version we can parse
			if (FileVersion > (int)PluginDescriptorVersion.Latest)
			{
				throw new BuildException("Plugin descriptor appears to be in a newer version ({0}) of the file format that we can load (max version: {1}).", FileVersion, (int)PluginDescriptorVersion.Latest);
			}

			// Read the other fields
			RawObject.TryGetIntegerField("Version", out Version);
			RawObject.TryGetStringField("VersionName", out VersionName);
			RawObject.TryGetStringField("FriendlyName", out FriendlyName);
			RawObject.TryGetStringField("Description", out Description);

			if (!RawObject.TryGetStringField("Category", out Category))
			{
				// Category used to be called CategoryPath in .uplugin files
				RawObject.TryGetStringField("CategoryPath", out Category);
			}

			// Due to a difference in command line parsing between Windows and Mac, we shipped a few Mac samples containing
			// a category name with escaped quotes. Remove them here to make sure we can list them in the right category.
			if (Category != null && Category.Length >= 2 && Category.StartsWith("\"") && Category.EndsWith("\""))
			{
				Category = Category.Substring(1, Category.Length - 2);
			}

			RawObject.TryGetStringField("CreatedBy", out CreatedBy);
			RawObject.TryGetStringField("CreatedByURL", out CreatedByURL);
			RawObject.TryGetStringField("DocsURL", out DocsURL);
			RawObject.TryGetStringField("MarketplaceURL", out MarketplaceURL);
			RawObject.TryGetStringField("SupportURL", out SupportURL);
			RawObject.TryGetStringField("EngineVersion", out EngineVersion);
			RawObject.TryGetStringArrayField("SupportedPrograms", out SupportedPrograms);
			RawObject.TryGetBoolField("bIsPluginExtension", out bIsPluginExtension);

			string[]? SupportedTargetPlatformNames;
			if (RawObject.TryGetStringArrayField("SupportedTargetPlatforms", out SupportedTargetPlatformNames))
			{
				SupportedTargetPlatforms = new List<UnrealTargetPlatform>();
				foreach (string TargetPlatformName in SupportedTargetPlatformNames)
				{
					UnrealTargetPlatform Platform;
					if (UnrealTargetPlatform.TryParse(TargetPlatformName, out Platform))
					{
						SupportedTargetPlatforms.Add(Platform);
					}
					else
					{
						Log.TraceWarningTask(PluginPath, $"Unknown platform {TargetPlatformName} listed in plugin with FriendlyName \"{FriendlyName}\"");
					}
				}
			}

			JsonObject[]? ModulesArray;
			if (RawObject.TryGetObjectArrayField("Modules", out ModulesArray))
			{
				Modules = Array.ConvertAll(ModulesArray, x => ModuleDescriptor.FromJsonObject(x, PluginPath)).ToList();
			}

			JsonObject[]? LocalizationTargetsArray;
			if (RawObject.TryGetObjectArrayField("LocalizationTargets", out LocalizationTargetsArray))
			{
				LocalizationTargets = Array.ConvertAll(LocalizationTargetsArray, x => LocalizationTargetDescriptor.FromJsonObject(x));
			}

			RawObject.TryGetStringField("VersePath", out VersePath);

			VerseScope PluginVerseScope;
			if (RawObject.TryGetEnumField<VerseScope>("VerseScope", out PluginVerseScope))
			{
				VerseScope = PluginVerseScope;
			}

			bool bEnabledByDefaultValue;
			if(RawObject.TryGetBoolField("EnabledByDefault", out bEnabledByDefaultValue))
			{
				bEnabledByDefault = bEnabledByDefaultValue;
			}

			RawObject.TryGetBoolField("CanContainContent", out bCanContainContent);
			RawObject.TryGetBoolField("CanContainVerse", out bCanContainVerse);
			RawObject.TryGetBoolField("IsBetaVersion", out bIsBetaVersion);
			RawObject.TryGetBoolField("IsExperimentalVersion", out bIsExperimentalVersion);
			RawObject.TryGetBoolField("Installed", out bInstalled);

			bool bCanBeUsedWithUnrealHeaderTool;
			if(RawObject.TryGetBoolField("CanBeUsedWithUnrealHeaderTool", out bCanBeUsedWithUnrealHeaderTool) && bCanBeUsedWithUnrealHeaderTool)
			{
				Array.Resize(ref SupportedPrograms, (SupportedPrograms == null)? 1 : SupportedPrograms.Length + 1);
				SupportedPrograms[SupportedPrograms.Length - 1] = "UnrealHeaderTool";
			}

			RawObject.TryGetBoolField("RequiresBuildPlatform", out bRequiresBuildPlatform);
			RawObject.TryGetBoolField("ExplicitlyLoaded", out bExplicitlyLoaded);
			RawObject.TryGetBoolField("HasExplicitPlatforms", out bHasExplicitPlatforms);

			CustomBuildSteps.TryRead(RawObject, "PreBuildSteps", out PreBuildSteps);
			CustomBuildSteps.TryRead(RawObject, "PostBuildSteps", out PostBuildSteps);

			JsonObject[]? PluginsArray;
			if(RawObject.TryGetObjectArrayField("Plugins", out PluginsArray))
			{
				Plugins = Array.ConvertAll(PluginsArray, x => PluginReferenceDescriptor.FromJsonObject(x)).ToList();
			}
		}

		/// <summary>
		/// Creates a plugin descriptor from a file on disk
		/// </summary>
		/// <param name="FileName">The filename to read</param>
		/// <returns>New plugin descriptor</returns>
		public static PluginDescriptor FromFile(FileReference FileName)
		{
			JsonObject RawObject = JsonObject.Read(FileName);
			try
			{
				PluginDescriptor Descriptor = new PluginDescriptor(RawObject, FileName);
				if (Descriptor.Modules != null)
				{
					foreach (ModuleDescriptor Module in Descriptor.Modules)
					{
						Module.Validate(FileName);
					}
				}
				return Descriptor;
			}
			catch (JsonParseException ParseException)
			{
				throw new JsonParseException("{0} (in {1})", ParseException.Message, FileName);
			}
		}

		/// <summary>
		/// Saves the descriptor to disk
		/// </summary>
		/// <param name="FileName">The filename to write to</param>
		public void Save(string FileName)
		{
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();
				Write(Writer);
				Writer.WriteObjectEnd();
			}
		}

		/// <summary>
		/// Writes the plugin descriptor to an existing Json writer
		/// </summary>
		/// <param name="Writer">The writer to receive plugin data</param>
		public void Write(JsonWriter Writer)
		{
			Writer.WriteValue("FileVersion", (int)ProjectDescriptorVersion.Latest);
			Writer.WriteValue("Version", Version);
			Writer.WriteValue("VersionName", VersionName);
			Writer.WriteValue("FriendlyName", FriendlyName);
			Writer.WriteValue("Description", Description);
			Writer.WriteValue("Category", Category);
			Writer.WriteValue("CreatedBy", CreatedBy);
			Writer.WriteValue("CreatedByURL", CreatedByURL);
			Writer.WriteValue("DocsURL", DocsURL);
			Writer.WriteValue("MarketplaceURL", MarketplaceURL);
			Writer.WriteValue("SupportURL", SupportURL);
			if(!String.IsNullOrEmpty(EngineVersion))
			{
				Writer.WriteValue("EngineVersion", EngineVersion);
			}
			if (!String.IsNullOrEmpty(VersePath))
			{
				Writer.WriteValue("VersePath", VersePath);
			}
			if (VerseScope != VerseScope.User)
			{
				Writer.WriteValue("VerseScope", VerseScope.ToString());
			}
			if (bEnabledByDefault.HasValue)
			{
				Writer.WriteValue("EnabledByDefault", bEnabledByDefault.Value);
			}
			Writer.WriteValue("CanContainContent", bCanContainContent);
			if (bCanContainVerse)
			{
				Writer.WriteValue("CanContainVerse", bCanContainVerse);
			}
			if (bIsBetaVersion)
			{
				Writer.WriteValue("IsBetaVersion", bIsBetaVersion);
			}
			if (bIsExperimentalVersion)
			{
				Writer.WriteValue("IsExperimentalVersion", bIsExperimentalVersion);
			}
			if (bInstalled)
			{
				Writer.WriteValue("Installed", bInstalled);
			}

			if(bRequiresBuildPlatform)
			{
				Writer.WriteValue("RequiresBuildPlatform", bRequiresBuildPlatform);
			}

			if (bExplicitlyLoaded)
			{
				Writer.WriteValue("ExplicitlyLoaded", bExplicitlyLoaded);
			}

			if (bHasExplicitPlatforms)
			{
				Writer.WriteValue("HasExplicitPlatforms", bHasExplicitPlatforms);
			}

			if(SupportedTargetPlatforms != null && SupportedTargetPlatforms.Count > 0)
			{
				Writer.WriteStringArrayField("SupportedTargetPlatforms", SupportedTargetPlatforms.Select<UnrealTargetPlatform, string>(x => x.ToString()).ToArray());
			}

			if (SupportedPrograms != null && SupportedPrograms.Length > 0)
			{
				Writer.WriteStringArrayField("SupportedPrograms", SupportedPrograms);
			}
			if (bIsPluginExtension)
			{
				Writer.WriteValue("bIsPluginExtension", bIsPluginExtension);
			}

			if (Modules != null && Modules.Count > 0)
			{
				ModuleDescriptor.WriteArray(Writer, "Modules", Modules.ToArray());
			}

			LocalizationTargetDescriptor.WriteArray(Writer, "LocalizationTargets", LocalizationTargets);

			if(PreBuildSteps != null)
			{
				PreBuildSteps.Write(Writer, "PreBuildSteps");
			}

			if(PostBuildSteps != null)
			{
				PostBuildSteps.Write(Writer, "PostBuildSteps");
			}

			if (Plugins != null && Plugins.Count > 0)
			{
				PluginReferenceDescriptor.WriteArray(Writer, "Plugins", Plugins.ToArray());
			}
		}

		/// <summary>
		/// Determines if this reference enables the plugin for a given platform
		/// </summary>
		/// <param name="Platform">The platform to check</param>
		/// <returns>True if the plugin should be enabled</returns>
		public bool SupportsTargetPlatform(UnrealTargetPlatform Platform)
		{
			if (bHasExplicitPlatforms)
			{
				return SupportedTargetPlatforms != null && SupportedTargetPlatforms.Contains(Platform);
			}
			else
			{
				return SupportedTargetPlatforms == null || SupportedTargetPlatforms.Count == 0 || SupportedTargetPlatforms.Contains(Platform);
			}
		}

		/// <summary>
		/// Retrieve the list of supported target platforms as a string list
		/// </summary>
		/// <returns>String list of supported target platforms</returns>
		public string[]? GetSupportedTargetPlatformNames()
		{
			return SupportedTargetPlatforms?.Select( P => P.ToString() ).ToArray();
		}
	}
}
