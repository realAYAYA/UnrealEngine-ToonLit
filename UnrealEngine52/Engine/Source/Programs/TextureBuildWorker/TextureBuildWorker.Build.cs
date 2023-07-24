// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Linq;
using EpicGames.Core;
using UnrealBuildTool;

// Abstract base class for texture worker targets.  Not a valid target by itself, hence it is not put into a *.target.cs file.
[SupportedPlatforms(UnrealPlatformClass.Desktop)]
public abstract class TextureBuildWorkerTarget : DerivedDataBuildWorkerTarget
{
	[ConfigFile(ConfigHierarchyType.Engine, "TextureBuildWorker", "ProjectOodlePlugin")]
	public string ProjectOodlePlugin;

	[ConfigFile(ConfigHierarchyType.Engine, "TextureBuildWorker", "ProjectOodleTextureFormatModule")]
	public string ProjectOodleTextureFormatModule;

	public TextureBuildWorkerTarget(TargetInfo Target) : base(Target)
	{
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		SolutionDirectory += "/Texture";

		//always add the engine TextureFormatOodle (if enabled)
		// then also add project Oodle (if enabled and configured)
		// projects can use neither or both
		AddOodleModule(Target,false);
		AddOodleModule(Target,true);
	}

	private void AddOodleModule(TargetInfo Target,bool bProjectOodle)
	{
		FileReference OodleUPluginFile = new FileReference("../Plugins/Developer/TextureFormatOodle/TextureFormatOodle.uplugin");
		PluginType OodlePluginType = PluginType.Engine;
		string OodleTextureFormatModule = "TextureFormatOodle";
		if (bProjectOodle)
		{
			if (Target.ProjectFile != null && !String.IsNullOrEmpty(ProjectOodlePlugin))
			{
				ProjectOodlePlugin = Path.Combine(Target.ProjectFile.Directory.ToString(), ProjectOodlePlugin);

				OodleUPluginFile = FileReference.FromString(ProjectOodlePlugin);
				OodlePluginType = PluginType.Project;
			}
			else
			{
				// bProjectOodle true but no project oodle set up
				return;
			}

			if (!String.IsNullOrEmpty(ProjectOodleTextureFormatModule))
			{
				OodleTextureFormatModule = ProjectOodleTextureFormatModule;
			}
		}

		// Determine if TextureFormatOodle is enabled.
		var ProjectDesc = ProjectFile != null ? ProjectDescriptor.FromFile(ProjectFile) : null;
		var OodlePlugin = new PluginInfo(OodleUPluginFile, OodlePluginType);
		bool bOodlePluginEnabled =
		Plugins.IsPluginEnabledForTarget(OodlePlugin, ProjectDesc, Target.Platform, Target.Configuration, TargetType.Program);

		if (bOodlePluginEnabled)
		{
			ExtraModuleNames.Add(OodleTextureFormatModule);
		}
	}
}

public class TextureBuildWorker : ModuleRules
{
	public TextureBuildWorker(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("DerivedDataCache");
		PrivateDependencyModuleNames.Add("DerivedDataBuildWorker");
	}
}
