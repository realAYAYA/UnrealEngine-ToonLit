// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Asio : ModuleRules
{
	public Asio(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string AsioPath = Target.UEThirdPartySourceDirectory + "asio/1.12.2/";

		PublicSystemIncludePaths.Add(AsioPath);

		PublicDefinitions.Add("ASIO_SEPARATE_COMPILATION");
		PublicDefinitions.Add("ASIO_STANDALONE");
		PublicDefinitions.Add("ASIO_NO_EXCEPTIONS");
		PublicDefinitions.Add("ASIO_NO_TYPEID");

		// The following are explicitly set because IncludeTool is unable to
		// parse the __has_include() preprocessor statements in Asio's config.hpp
		PublicDefinitions.Add("ASIO_HAS_STD_ARRAY");
		PublicDefinitions.Add("ASIO_HAS_STD_ATOMIC");
		//PublicDefinitions.Add("ASIO_HAS_STD_CALL_FUTURE");
		//PublicDefinitions.Add("ASIO_HAS_STD_CALL_ONCE");
		//PublicDefinitions.Add("ASIO_HAS_STD_CHRONO");
		//PublicDefinitions.Add("ASIO_HAS_STD_MUTEX_AND_CONDVAR");
		//PublicDefinitions.Add("ASIO_HAS_STD_STRING_VIEW");
		PublicDefinitions.Add("ASIO_HAS_STD_SYSTEM_ERROR");
		//PublicDefinitions.Add("ASIO_HAS_STD_THREAD");
		PublicDefinitions.Add("ASIO_HAS_STD_TYPE_TRAITS");
	}
}

