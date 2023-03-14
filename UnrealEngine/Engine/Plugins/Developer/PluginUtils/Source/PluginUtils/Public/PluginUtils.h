// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "PluginDescriptor.h"
#include "ModuleDescriptor.h"

class IPlugin;

class PLUGINUTILS_API FPluginUtils
{
public:
	/**
	 * Returns the plugin folder.
	 * @param PluginLocation directory that contains the plugin folder
	 * @param PluginName name of the plugin
	 * @param bFullPath ensures a full path is returned
	 */
	static FString GetPluginFolder(const FString& PluginLocation, const FString& PluginName, bool bFullPath = false);

	/** 
	 * Returns the uplugin file path.
	 * @param PluginLocation directory that contains the plugin folder
	 * @param PluginName name of the plugin
	 * @param bFullPath ensures a full path is returned
	 */
	static FString GetPluginFilePath(const FString& PluginLocation, const FString& PluginName, bool bFullPath = false);

	/**
	 * Returns the plugin Content folder.
	 * @param PluginLocation directory that contains the plugin folder
	 * @param PluginName name of the plugin
	 * @param bFullPath ensures a full path is returned
	 */
	static FString GetPluginContentFolder(const FString& PluginLocation, const FString& PluginName, bool bFullPath = false);

	/**
	 * Returns the plugin Resources folder.
	 * @param PluginLocation directory that contains the plugin folder
	 * @param PluginName name of the plugin
	 * @param bFullPath ensures a full path is returned
	 */
	static FString GetPluginResourcesFolder(const FString& PluginLocation, const FString& PluginName, bool bFullPath = false);

	/**
	 * Parameters for creating a new plugin.
	 */
	struct FNewPluginParams
	{
		/** The author of this plugin */
		FString CreatedBy;

		/** Hyperlink for the author's website  */
		FString CreatedByURL;

		/** A friendly name for this plugin. Set to the plugin name by default */
		FString FriendlyName;

		/** A description for this plugin */
		FString Description;

		/** Path to plugin icon to copy in the plugin resources folder */
		FString PluginIconPath;

		/** 
		 * Folders containing template files to copy into the plugin folder (Required if bHasModules).
		 * Occurrences of the string PLUGIN_NAME in the filename or file content will be replaced by the plugin name. 
		 */
		TArray<FString> TemplateFolders;

		/** Marks this content as being in beta */
		bool bIsBetaVersion = false;

		/** Can this plugin contain content */
		bool bCanContainContent = false;

		/** Can this plugin contain Verse */
		bool bCanContainVerse = false;

		/** Does this plugin have Source files? */
		bool bHasModules = false;

		/**
		 * When true, this plugin's modules will not be loaded automatically nor will it's content be mounted automatically.
		 * It will load/mount when explicitly requested and LoadingPhases will be ignored.
		 */
		bool bExplicitelyLoaded = false;

		/** The Verse path to the root of this plugin's content directory */
		FString VersePath;

		/** Whether this plugin should be enabled/disabled by default for any project. */
		EPluginEnabledByDefault EnabledByDefault = EPluginEnabledByDefault::Unspecified;

		/** If this plugin has Source, what is the type of Source included (so it can potentially be excluded in the right builds) */
		EHostType::Type ModuleDescriptorType = EHostType::Runtime;

		/** If this plugin has Source, when should the module be loaded (may need to be earlier than default if used in blueprints) */
		ELoadingPhase::Type LoadingPhase = ELoadingPhase::Default;
	};

	/**
	 * Parameters for creating a new plugin.
	 */
	struct FNewPluginParamsWithDescriptor
	{
		/** The description of the plugin */
		FPluginDescriptor Descriptor;

		/** Path to plugin icon to copy in the plugin resources folder */
		FString PluginIconPath;

		/**
		 * Folders containing template files to copy into the plugin folder (Required if Descriptor.Modules is not empty).
		 * Occurrences of the string PLUGIN_NAME in the filename or file content will be replaced by the plugin name.
		 */
		TArray<FString> TemplateFolders;
	};

	/**
	 * Parameters for loading/mounting a plugin
	 */
	struct FLoadPluginParams
	{
		/** Whether to synchronously scan all assets in the plugin */
		bool bSynchronousAssetsScan = false;

		/** Whether to select the plugin Content folder (if any) in the content browser */
		bool bSelectInContentBrowser = false;

		/** Whether to enable the plugin in the current project config */
		bool bEnablePluginInProject = false;

		/**
		 * Whether to update the project additional plugin directories (persistently saved in uproject file)
		 * if the plugin location is not under the engine or project plugin folder
		 */
		bool bUpdateProjectPluginSearchPath = false;

		/** Outputs whether the plugin was already loaded */
		bool bOutAlreadyLoaded =  false;

		/** Outputs the reason the plugin loading failed (if applicable) */
		FText* OutFailReason = nullptr;
	};

	struct UE_DEPRECATED(5.0, "FMountPluginParams is deprecated; please use FLoadPluginParams instead") FMountPluginParams;
	/**
	 * Parameters for mounting a plugin.
	 */
	struct FMountPluginParams
	{
		/** Whether to enable the plugin in the current project config. */
		bool bEnablePluginInProject = true;

		/**
		 * Whether to update the project additional plugin directories (persistently saved in uproject file)
		 * if the plugin location is not under the engine or project plugin folder.
		 */
		bool bUpdateProjectPluginSearchPath = true;

		/** Whether to select the plugin Content folder (if any) in the content browser. */
		bool bSelectInContentBrowser = true;
	};

	/**
	 * Helper to create and load a new plugin
	 * @param PluginName Plugin name
	 * @param PluginLocation Directory that contains the plugin folder
	 * @param CreationParams Plugin creation parameters
	 * @param MountParams Plugin loading parameters
	 * @return The newly created plugin. If something goes wrong during the creation process, the plugin folder gets deleted and null is returned.
	 * @note MountParams.OutFailReason outputs the reason the plugin creation or loading failed (if applicable)
	 * @note Will fail if the plugin already exists
	 */
	static TSharedPtr<IPlugin> CreateAndLoadNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParams& CreationParams, FLoadPluginParams& LoadParams);

	/**
	 * Helper to create and load a new plugin
	 * @param PluginName Plugin name
	 * @param PluginLocation Directory that contains the plugin folder
	 * @param CreationParams Plugin creation parameters
	 * @param LoadParams Plugin loading parameters
	 * @return The newly created plugin. If something goes wrong during the creation process, the plugin folder gets deleted and null is returned.
	 * @note MountParams.OutFailReason outputs the reason the plugin creation or loading failed (if applicable)
	 * @note Will fail if the plugin already exists
	 */
	static TSharedPtr<IPlugin> CreateAndLoadNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParamsWithDescriptor& CreationParams, FLoadPluginParams& LoadParams);

	/**
	 * Helper to create and mount a new plugin.
	 * @param PluginName Plugin name
	 * @param PluginLocation Directory that contains the plugin folder
	 * @param CreationParams Plugin creation parameters
	 * @param MountParams Plugin mounting parameters
	 * @param FailReason Reason the plugin creation/mount failed
	 * @return The newly created plugin. If something goes wrong during the creation process, the plugin folder gets deleted and null is returned.
	 * @note Will fail if the plugin already exists
	 */
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.0, "CreateAndMountNewPlugin is deprecated; please use CreateAndLoadNewPlugin instead")
	static TSharedPtr<IPlugin> CreateAndMountNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParams& CreationParams, const FMountPluginParams& MountParams, FText& FailReason);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Helper to create and mount a new plugin.
	 * @param PluginName Plugin name
	 * @param PluginLocation Directory that contains the plugin folder
	 * @param CreationParams Plugin creation parameters
	 * @param MountParams Plugin mounting parameters
	 * @param FailReason Reason the plugin creation/mount failed
	 * @return The newly created plugin. If something goes wrong during the creation process, the plugin folder gets deleted and null is returned.
	 * @note Will fail if the plugin already exists
	 */
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.0, "CreateAndMountNewPlugin is deprecated; please use CreateAndLoadNewPlugin instead")
	static TSharedPtr<IPlugin> CreateAndMountNewPlugin(const FString& PluginName, const FString& PluginLocation, const FNewPluginParamsWithDescriptor& CreationParams, const FMountPluginParams& MountParams, FText& FailReason);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Load/mount the specified plugin
	 * @param PluginName Plugin name
	 * @param PluginLocation Directory that contains the plugin folder
	 * @param LoadParams Plugin loading parameters
	 * @return The loaded plugin or null on failure
	 */
	static TSharedPtr<IPlugin> LoadPlugin(const FString& PluginName, const FString& PluginLocation, FLoadPluginParams& LoadParams);

	/**
	 * Load/mount the specified plugin
	 * @param PluginName Plugin name
	 * @param PluginLocation Directory that contains the plugin folder
	 * @return The loaded plugin or null on failure
	 */
	static TSharedPtr<IPlugin> LoadPlugin(const FString& PluginName, const FString& PluginLocation)
	{
		FLoadPluginParams Params;
		return LoadPlugin(PluginName, PluginLocation, Params);
	}

	/**
	 * Load/mount the specified plugin
	 * @param PluginFileName Plugin descriptor file path
	 * @param LoadParams Plugin loading parameters
	 * @return The loaded plugin or null on failure
	 */
	static TSharedPtr<IPlugin> LoadPlugin(const FString& PluginFileName, FLoadPluginParams& LoadParams);

	/**
	 * Load/mount the specified plugin
	 * @param PluginFileName Plugin descriptor file path
	 * @return The loaded plugin or null on failure
	 */
	static TSharedPtr<IPlugin> LoadPlugin(const FString& PluginFileName)
	{
		FLoadPluginParams Params;
		return LoadPlugin(PluginFileName, Params);
	}

	/**
	 * Load/mount the specified plugin.
	 * @param PluginName Plugin name
	 * @param PluginLocation Directory that contains the plugin folder
	 * @param MountParams Plugin mounting parameters
	 * @param FailReason Reason the plugin failed to load
	 * @return The mounted plugin or null on failure
	 */
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.0, "MountPlugin is deprecated; please use LoadPlugin instead")
	static TSharedPtr<IPlugin> MountPlugin(const FString& PluginName, const FString& PluginLocation, const FMountPluginParams& MountParams, FText& FailReason);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Finds a loaded plugin from a plugin descriptor file path
	 */
	static TSharedPtr<IPlugin> FindLoadedPlugin(const FString& PluginDescriptorFileName);

	/**
	 * Unload assets from the specified plugin and unmount it
	 * @note Only works on content-only plugins; plugins with code modules cannot be safely unloaded
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param Plugin Plugin to unload
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether the plugin was successfully unloaded
	 */
	static bool UnloadPlugin(const TSharedRef<IPlugin>& Plugin, FText* OutFailReason = nullptr);

	/**
	 * Unload assets from the specified plugin and unmount it
	 * @note Only works on content-only plugins; plugins with code modules cannot be safely unloaded
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param PluginName Name of the plugin to unload
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether the plugin was successfully unloaded
	 */
	static bool UnloadPlugin(const FString& PluginName, FText* OutFailReason = nullptr);

	/**
	 * Unload assets from the specified plugins and unmount them
	 * @note Only works on content-only plugins; plugins with code modules cannot be safely unloaded
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param Plugins Plugins to unload
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether all plugins were successfully unloaded
	 */
	static bool UnloadPlugins(const TConstArrayView<TSharedRef<IPlugin>> Plugins, FText* OutFailReason = nullptr);

	/**
	 * Unload assets from the specified plugins and unmount them
	 * @note Only works on content-only plugins; plugins with code modules cannot be safely unloaded
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param PluginNames Names of the plugins to unload
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether all plugins were successfully unloaded
	 */
	static bool UnloadPlugins(const TConstArrayView<FString> PluginNames, FText* OutFailReason = nullptr);

	/**
	 * Unload assets from the specified plugin but does not unmount it
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param Plugin Plugin to unload assets from
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether plugin assets were successfully unloaded
	 */
	static bool UnloadPluginAssets(const TSharedRef<IPlugin>& Plugin, FText* OutFailReason = nullptr);

	/**
	 * Unload assets from the specified plugin but does not unmount it
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param PluginName Name of the plugin to unload assets from
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether plugin assets were successfully unloaded
	 */
	static bool UnloadPluginAssets(const FString& PluginName, FText* OutFailReason = nullptr);

	/**
	 * Unload assets from the specified plugins but does not unmount them
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param Plugin Plugins to unload assets from
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether plugin assets were successfully unloaded
	 */
	static bool UnloadPluginsAssets(const TConstArrayView<TSharedRef<IPlugin>> Plugins, FText* OutFailReason = nullptr);

	/**
	 * Unload assets from the specified plugin but does not unmount them
	 * @warning Dirty assets that need to be saved will be unloaded anyway
	 * @param PluginNames Names of the plugins to unload assets from
	 * @param OutFailReason Outputs the reason of the failure if any
	 * @return Whether plugin assets were successfully unloaded
	 */
	static bool UnloadPluginsAssets(const TConstArrayView<FString> PluginNames, FText* OutFailReason = nullptr);

	/**
	 * Adds a directory to the list of paths that are recursively searched for plugins, 
	 * if that directory isn't already under the search paths.
	 * @param Dir Directory to add (doesn't have to be an absolute or normalized path)
	 * @param bRefreshPlugins Whether to refresh plugins if the search path list gets modified
	 * @param bUpdateProjectFile Whether to update the project additional plugin directories (persistently saved in uproject file) if needed
	 * @return Whether the plugin search path was modified
	 */
	static bool AddToPluginSearchPathIfNeeded(const FString& Dir, bool bRefreshPlugins = false, bool bUpdateProjectFile = false);

	/**
	 * Validate that the plugin name is valid, that the name isn't already used by a registered plugin
	 * and optionally that there isn't an unregistered plugin with that name that exists at the specified location.
	 * @param PluginName Plugin name
	 * @param PluginLocation Optional directory in which to look for a plugin that might not be registered
	 * @param FailReason Optional output text describing why the validation failed
	 * @return
	 */
	static bool ValidateNewPluginNameAndLocation(const FString& PluginName, const FString& PluginLocation = FString(), FText* FailReason = nullptr);

	/**
	 * Returns whether the specified plugin name is valid, regardless of whether it's already used
	 * @param PluginName Plugin name
	 * @param FailReason Optional output text specifying what is wrong with the plugin name
	 * @param PluginTermReplacement If set, replaces the term "plugin" in the fail reason message
	 */
	static bool IsValidPluginName(const FString& PluginName, FText* FailReason = nullptr, const FText* PluginTermReplacement = nullptr);
};

#endif //if WITH_EDITOR
