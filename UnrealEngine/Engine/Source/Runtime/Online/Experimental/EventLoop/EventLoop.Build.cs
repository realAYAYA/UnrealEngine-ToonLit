// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EventLoop : ModuleRules
{
	public EventLoop(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"NetCommon"
			});

		PublicDefinitions.Add(string.Format("HAS_EVENTLOOP_PLATFORM_SOCKET_IMPLEMENTATION={0}", bHasPlatformSocketImplementation ? 1 : 0));
		PublicDefinitions.Add(string.Format("HAS_EVENTLOOP_PLATFORM_BSD_SOCKET_HEADER={0}", bHasPlatformBSDSocketHeader ? 1 : 0));
	}

	public virtual bool bHasPlatformSocketImplementation
	{
		get
		{
			return false;
			// Other platforms may override this property.
		}
	}

	public virtual bool bHasPlatformBSDSocketHeader
	{
		get
		{
			return false;
			// Other platforms may override this property.
		}
	}
}
