// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenSSL_HoloLens : OpenSSL
	{
		public OpenSSL_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			// We do not currently have hololens OpenSSL binaries, lets not pretend we do.
			// This means that builds that depend on OpenSSL (like EngineTest) will succeed, but use of it would fail at runtime.

			//string VSVersion = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			//// Add includes
			//PublicIncludePaths.Add(Path.Combine(OpenSSL111kPath, "include", PlatformSubdir, VSVersion));

			//// Add Libs
			//string LibPath = Path.Combine(OpenSSL111kPath, "lib", PlatformSubdir, VSVersion, ConfigFolder);

			//PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.lib"));
			//PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.lib"));
			//PublicSystemLibraries.Add("crypt32.lib");

		}
	}
}
