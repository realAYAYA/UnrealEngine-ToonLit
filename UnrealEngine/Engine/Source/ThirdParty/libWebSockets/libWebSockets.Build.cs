// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class libWebSockets : ModuleRules
{
	protected virtual bool Supported
	{
		get
		{
			return
				Target.Platform == UnrealTargetPlatform.Android ||
				Target.Platform == UnrealTargetPlatform.IOS ||
				Target.Platform == UnrealTargetPlatform.Mac ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
				Target.Platform == UnrealTargetPlatform.Win64;
		}
	}

	protected virtual string LibRootDirectory { get { return ModuleDirectory; } }

	protected virtual string WebSocketsVersion { get { return "libwebsockets"; } }
	protected virtual string WebSocketsPackagePath { get { return Path.Combine(LibRootDirectory, WebSocketsVersion); } }

	protected virtual string ConfigName { get { return (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release"; } }
	
	protected virtual bool bRequireOpenSSL { get { return true; } }

	protected virtual string DefaultLibraryName { get { return "libwebsockets.a"; } }

	protected virtual string IncludeDirectory
	{
		get
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				return Path.Combine(WebSocketsPackagePath, "include", Target.Platform.ToString(), "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				return Path.Combine(WebSocketsPackagePath, "include", "Unix", Target.Architecture);
			}
			else
			{
				return Path.Combine(WebSocketsPackagePath, "include", Target.Platform.ToString());
			}
		}
	}

	protected virtual string LibraryDirectory
	{
		get
		{
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				return Path.Combine(WebSocketsPackagePath, "lib", Target.Platform.ToString(), "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), ConfigName);
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				return Path.Combine(WebSocketsPackagePath, "lib", "Unix", Target.Architecture, ConfigName);
			}
			else
			{
				return Path.Combine(WebSocketsPackagePath, "lib", Target.Platform.ToString(), ConfigName);
			}
		}
	}

	public libWebSockets(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (!Supported)
		{
			return;
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicIncludePaths.Add(IncludeDirectory);
			PublicAdditionalLibraries.Add(Path.Combine(WebSocketsPackagePath, "lib", Target.Platform.ToString(), "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName(), ConfigName, "websockets_static.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicIncludePaths.Add(Path.Combine(WebSocketsPackagePath, "include", Target.Platform.ToString(), "ARM64"));
			PublicIncludePaths.Add(Path.Combine(WebSocketsPackagePath, "include", Target.Platform.ToString(), "x64"));
			PublicIncludePaths.Add(Path.Combine(WebSocketsPackagePath, "include", Target.Platform.ToString()));
			PublicAdditionalLibraries.Add(Path.Combine(WebSocketsPackagePath, "lib", Target.Platform.ToString(), "ARM64", ConfigName, "libwebsockets.a"));
			PublicAdditionalLibraries.Add(Path.Combine(WebSocketsPackagePath, "lib", Target.Platform.ToString(), "x64", ConfigName, "libwebsockets.a"));
		}
		else
		{
			PublicIncludePaths.Add(IncludeDirectory);
			PublicAdditionalLibraries.Add(Path.Combine(LibraryDirectory, DefaultLibraryName));
		}

		if (bRequireOpenSSL)
		{
			PublicDependencyModuleNames.Add("OpenSSL");
		}
	}
}
