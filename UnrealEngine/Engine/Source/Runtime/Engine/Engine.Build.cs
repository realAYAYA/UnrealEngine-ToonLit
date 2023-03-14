// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Engine : ModuleRules
{
	public Engine(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("../Shaders/Shared");
		
		PrivatePCHHeaderFile = "Private/EnginePrivatePCH.h";

		SharedPCHHeaderFile = "Public/EngineSharedPCH.h";

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"AnimationCore",
				"AudioMixer", 
				"AudioMixerCore",
				"MovieSceneCapture", 
				"PacketHandler", 
				"Renderer", 
			}
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(GetModuleDirectory("NetCore"), "Private"),
				Path.Combine(GetModuleDirectory("Renderer"), "Private"),
				Path.Combine(GetModuleDirectory("SynthBenchmark"), "Private"),
				Path.Combine(GetModuleDirectory("Virtualization"), "Private"),
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DerivedDataCache",
				"DistributedBuildInterface",
				"TargetPlatform",
				"ImageWrapper",
				"ImageWriteQueue",
				"HeadMountedDisplay",
				"EyeTracker",
				"MRMesh",
				"Advertising",
				"MovieSceneCapture",
				"AutomationWorker",
				"MovieSceneCapture",
				"DesktopPlatform"
			}
		);

		if (Target.Configuration != UnrealTargetConfiguration.Shipping || Target.bCompileAgainstEditor)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"TaskGraph",
					"SlateReflector",
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SlateReflector",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorAnalyticsSession",
				}
			);
		}

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreOnline",
				"CoreUObject",
				"NetCore",
				"ApplicationCore",
				"ImageCore",
				"Json",
				"JsonUtilities",
				"SlateCore",
				"Slate",
				"InputCore",
				"Messaging",
				"MessagingCommon",
				"RenderCore",
				"AnalyticsET",
				"RHI",
				"Sockets",
				"AssetRegistry", // Here until we update all modules using AssetRegistry to add a dependency on it
				"EngineMessages",
				"EngineSettings",
				"SynthBenchmark",
				"GameplayTags",
				"PacketHandler",
				"AudioPlatformConfiguration",
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"PakFile",
				"NetworkReplayStreaming",
				"PhysicsCore",
				"SignalProcessing",
				"AudioExtensions",
				"DeveloperSettings",
				"AudioLinkCore",
				"CookOnTheFly"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"TypedElementFramework",
				"TypedElementRuntime",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AnimationCore",
				"AppFramework",
				"BuildSettings",
				"Networking",
				"Landscape",
				"UMG",
				"Projects",
				"TypedElementFramework",
				"TypedElementRuntime",
				"MaterialShaderQualitySettings",
				"MoviePlayerProxy",
				"CinematicCamera",
				"Analytics",
				"AudioMixer",
				"AudioMixerCore",
				"SignalProcessing",
				"IntelISPC",
				"TraceLog",
				"ColorManagement",
				"Icmp",
				"XmlParser"
			}
		);

		// Cross platform Audio Codecs:
		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"UEOgg",
			"Vorbis",
			"VorbisFile",
			"libOpus"
			);

		DynamicallyLoadedModuleNames.Add("EyeTracker");

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.Add("Localization");
			DynamicallyLoadedModuleNames.Add("Localization");
		}

		// to prevent "causes WARNING: Non-editor build cannot depend on non-redistributable modules."
		if (Target.bCompileAgainstEditor)
		{

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"TextureBuildUtilities"
				}
			);

			// for now we depend on these
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RawMesh",
					"Zen"
				}
			);
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"MessagingRpc",
				"PortalRpc",
				"PortalServices",
			}
		);

		if (Target.bCompileAgainstEditor)
		{
			// these modules require variadic templates
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MessagingRpc",
					"PortalRpc",
					"PortalServices",
				}
			);
		}

		CircularlyReferencedDependentModules.Add("GameplayTags");
		CircularlyReferencedDependentModules.Add("Landscape");
		CircularlyReferencedDependentModules.Add("UMG");
		CircularlyReferencedDependentModules.Add("MaterialShaderQualitySettings");
		CircularlyReferencedDependentModules.Add("CinematicCamera");
		CircularlyReferencedDependentModules.Add("AudioMixer");

		if (Target.bCompileAgainstEditor)
		{
			PrivateIncludePathModuleNames.Add("Foliage");
		}

		// The AnimGraphRuntime module is not needed by Engine proper, but it is loaded in LaunchEngineLoop.cpp,
		// and needs to be listed in an always-included module in order to be compiled into standalone games
		DynamicallyLoadedModuleNames.Add("AnimGraphRuntime");

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"MovieScene",
				"MovieSceneCapture",
				"MovieSceneTracks",
				"LevelSequence",
				"HeadMountedDisplay",
				"MRMesh",
				"StreamingPauseRendering",
			}
		);

		if (Target.Type != TargetType.Server)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"SlateNullRenderer",
					"SlateRHIRenderer"
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
					"SlateNullRenderer",
					"SlateRHIRenderer"
				}
			);
		}

		if (Target.Type == TargetType.Server || Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.Add("PerfCounters");
		}

		if (Target.bCompileAgainstEditor)
		{
			PrivateIncludePathModuleNames.Add("MaterialUtilities");
			PrivateDependencyModuleNames.Add("MaterialUtilities");

			PrivateIncludePathModuleNames.Add("MeshUtilities");
			DynamicallyLoadedModuleNames.Add("MeshUtilities");

			PrivateIncludePathModuleNames.Add("MeshUtilitiesCommon");

			PublicIncludePathModuleNames.Add("AnimationDataController");
			DynamicallyLoadedModuleNames.Add("AnimationDataController");

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"RawMesh"
				}
			);

			PrivateDependencyModuleNames.Add("CollisionAnalyzer");
			CircularlyReferencedDependentModules.Add("CollisionAnalyzer");

			PrivateDependencyModuleNames.Add("LogVisualizer");
			CircularlyReferencedDependentModules.Add("LogVisualizer");

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"WindowsTargetPlatform",
						"WindowsPlatformEditor",
					}
				);
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"MacTargetPlatform",
						"MacPlatformEditor",
					}
				);
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				DynamicallyLoadedModuleNames.AddRange(
					new string[] {
						"LinuxTargetPlatform",
						"LinuxPlatformEditor",
					}
				);
			}
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"NullNetworkReplayStreaming",
				"LocalFileNetworkReplayStreaming",
				"HttpNetworkReplayStreaming",
				"Advertising"
			}
		);

		if (Target.bWithLiveCoding)
		{
			DynamicallyLoadedModuleNames.Add("LiveCoding");
		}

		if (Target.Type != TargetType.Server)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"ImageWrapper"
				}
			);
		}

		AllowedRestrictedFolders.Add("Private/NotForLicensees");

		if (!Target.bBuildRequiresCookedData && Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("DeveloperToolSettings");


		}

		if (Target.bBuildEditor == true)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"UnrealEd",
					"Kismet"
				}
			);  // @todo api: Only public because of WITH_EDITOR and UNREALED_API

			CircularlyReferencedDependentModules.AddRange(
				new string[] {
					"UnrealEd",
					"Kismet"
				}
			);

			PrivateDependencyModuleNames.Add("DerivedDataCache");
			PrivateDependencyModuleNames.Add("TextureCompressor");

			PrivateIncludePathModuleNames.Add("TextureCompressor");
			PrivateIncludePaths.Add("Developer/TextureCompressor/Public");

			PrivateIncludePathModuleNames.Add("HierarchicalLODUtilities");
			DynamicallyLoadedModuleNames.Add("HierarchicalLODUtilities");

			DynamicallyLoadedModuleNames.Add("AnimationModifiers");

			PrivateIncludePathModuleNames.Add("AssetTools");
			DynamicallyLoadedModuleNames.Add("AssetTools");

			PrivateIncludePathModuleNames.Add("PIEPreviewDeviceProfileSelector");

			PrivateIncludePathModuleNames.Add("NaniteBuilder");
			DynamicallyLoadedModuleNames.Add("NaniteBuilder");

			DynamicallyLoadedModuleNames.Add("LevelInstanceEditor");
		}

		SetupModulePhysicsSupport(Target);

		// Engine public headers need to know about some types (enums etc.)
		PublicIncludePathModuleNames.Add("ClothingSystemRuntimeInterface");
		PublicDependencyModuleNames.Add("ClothingSystemRuntimeInterface");

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("ClothingSystemEditorInterface");
			PrivateIncludePathModuleNames.Add("ClothingSystemEditorInterface");
			PrivateDependencyModuleNames.Add("DesktopPlatform");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// Head Mounted Display support
			//			PrivateIncludePathModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
			//			DynamicallyLoadedModuleNames.AddRange(new string[] { "HeadMountedDisplay" });
		}

		if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// Allow VirtualTextureUploadCache to use the UpdateTexture path.
			PublicDefinitions.Add("ALLOW_UPDATE_TEXTURE=1");

			PublicFrameworks.AddRange(new string[] { "AVFoundation", "CoreVideo", "CoreMedia" });
		}

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile"
				);

			PrivateIncludePathModuleNames.Add("AndroidRuntimeSettings");
		}

		if (Target.Platform == UnrealTargetPlatform.IOS || Target.Platform == UnrealTargetPlatform.TVOS)
		{
			PublicIncludePaths.AddRange(
            	new string[] {
               		"Runtime/IOS/IOSPlatformFeatures/Public"
                });

			PrivateIncludePathModuleNames.Add("IOSRuntimeSettings");
		}

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"UEOgg",
				"Vorbis",
				"VorbisFile",
				"libOpus"
				);
		}

		PublicDefinitions.Add("GPUPARTICLE_LOCAL_VF_ONLY=0");

		// Add a reference to the stats HTML files referenced by UEngine::DumpFPSChartToHTML. Previously staged by CopyBuildToStagingDirectory.
		if (Target.bBuildEditor || Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			RuntimeDependencies.Add("$(EngineDir)/Content/Stats/...", StagedFileType.UFS);
		}

		if (Target.bBuildEditor == false && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PublicDefinitions.Add("WITH_ODSC=1");
		}
        else
        {
			PublicDefinitions.Add("WITH_ODSC=0");
		}

		const bool bIrisAddAsPublicDepedency = true;
		SetupIrisSupport(Target, bIrisAddAsPublicDepedency);

		PrivateDefinitions.Add("UE_DEPRECATE_LEGACY_MATH_CONSTANT_MACRO_NAMES=1");
	}
}
