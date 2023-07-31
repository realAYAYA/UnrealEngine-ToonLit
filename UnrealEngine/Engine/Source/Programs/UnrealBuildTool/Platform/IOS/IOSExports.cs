// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Linq;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public IOS functions exposed to UAT
	/// </summary>
	public static class IOSExports
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="InProject"></param>
		/// <param name="Distribution"></param>
		/// <param name="MobileProvision"></param>
		/// <param name="SigningCertificate"></param>
		/// <param name="TeamUUID"></param>
		/// <param name="bAutomaticSigning"></param>
		public static void GetProvisioningData(FileReference InProject, bool Distribution, out string? MobileProvision, out string? SigningCertificate, out string? TeamUUID, out bool bAutomaticSigning)
		{
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProjectSettings(InProject);
			if (ProjectSettings == null)
			{
				MobileProvision = null;
				SigningCertificate = null;
				TeamUUID = null;
				bAutomaticSigning = false;
				return;
			}
			if (ProjectSettings.bAutomaticSigning)
			{
				MobileProvision = null;
				SigningCertificate = null;
				TeamUUID = ProjectSettings.TeamID;
				bAutomaticSigning = true;
			}
			else
			{
				IOSProvisioningData Data = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(UnrealTargetPlatform.IOS)).ReadProvisioningData(ProjectSettings, Distribution);
				if (Data == null)
				{ // no provisioning, swith to automatic
					MobileProvision = null;
					SigningCertificate = null;
					TeamUUID = ProjectSettings.TeamID;
					bAutomaticSigning = true;
				}
				else
				{
					MobileProvision = Data.MobileProvision;
					SigningCertificate = Data.SigningCertificate;
					TeamUUID = Data.TeamUUID;
					bAutomaticSigning = false;
				}
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Config"></param>
		/// <param name="ProjectFile"></param>
		/// <param name="InProjectName"></param>
		/// <param name="InProjectDirectory"></param>
		/// <param name="InExecutablePath"></param>
		/// <param name="InEngineDir"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="CookFlavor"></param>
		/// <param name="bIsDataDeploy"></param>
		/// <param name="bCreateStubIPA"></param>
		/// <param name="BuildReceiptFileName"></param>
		/// <param name="Logger">Logger for output</param>
		/// <returns></returns>
		public static bool PrepForUATPackageOrDeploy(UnrealTargetConfiguration Config, FileReference ProjectFile, string InProjectName, DirectoryReference InProjectDirectory, string InExecutablePath, DirectoryReference InEngineDir, bool bForDistribution, string CookFlavor, bool bIsDataDeploy, bool bCreateStubIPA, FileReference BuildReceiptFileName, ILogger Logger)
		{
			TargetReceipt Receipt = TargetReceipt.Read(BuildReceiptFileName);
			return new UEDeployIOS(Logger).PrepForUATPackageOrDeploy(Config, ProjectFile, InProjectName, InProjectDirectory.FullName, InExecutablePath, InEngineDir.FullName, bForDistribution, CookFlavor, bIsDataDeploy, bCreateStubIPA, Receipt);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="Config"></param>
		/// <param name="ProjectDirectory"></param>
		/// <param name="bIsUnrealGame"></param>
		/// <param name="GameName"></param>
		/// <param name="bIsClient"></param>
		/// <param name="ProjectName"></param>
		/// <param name="InEngineDir"></param>
		/// <param name="AppDirectory"></param>
		/// <param name="BuildReceiptFileName"></param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="bSupportsPortrait"></param>
		/// <param name="bSupportsLandscape"></param>
		/// <returns></returns>
		public static bool GeneratePList(FileReference ProjectFile, UnrealTargetConfiguration Config, DirectoryReference ProjectDirectory, bool bIsUnrealGame, string GameName, bool bIsClient, string ProjectName, DirectoryReference InEngineDir, DirectoryReference AppDirectory, FileReference BuildReceiptFileName, ILogger Logger, out bool bSupportsPortrait, out bool bSupportsLandscape)
		{
			TargetReceipt Receipt = TargetReceipt.Read(BuildReceiptFileName);
			return new UEDeployIOS(Logger).GeneratePList(ProjectFile, Config, ProjectDirectory.FullName, bIsUnrealGame, GameName, bIsClient, ProjectName, InEngineDir.FullName, AppDirectory.FullName, Receipt, out bSupportsPortrait, out bSupportsLandscape);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="PlatformType"></param>
		/// <param name="SourceFile"></param>
		/// <param name="TargetFile"></param>
		/// <param name="Logger"></param>
		public static void StripSymbols(UnrealTargetPlatform PlatformType, FileReference SourceFile, FileReference TargetFile, ILogger Logger)
		{
			IOSProjectSettings ProjectSettings = ((IOSPlatform)UEBuildPlatform.GetBuildPlatform(PlatformType)).ReadProjectSettings(null);
			IOSToolChain ToolChain = new IOSToolChain(null, ProjectSettings, ClangToolChainOptions.None, Logger);
			ToolChain.StripSymbols(SourceFile, TargetFile);
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="Executable"></param>
		/// <param name="StageDirectory"></param>
		/// <param name="Platform"></param>
		/// <param name="Logger"></param>
		public static void GenerateAssetCatalog(FileReference ProjectFile, FileReference Executable, DirectoryReference StageDirectory, UnrealTargetPlatform Platform, ILogger Logger)
		{
			// Determine whether the user has modified icons that require a remote Mac to build.
			bool bUserImagesExist = false;
			DirectoryReference ResourcesDir = IOSToolChain.GenerateAssetCatalog(ProjectFile, Platform, ref bUserImagesExist);

			// Don't attempt to do anything remotely if the user is using the default UE images.
			if (!bUserImagesExist)
			{
				return;
			}

            // Also don't attempt to use a remote Mac if packaging for TVOS on PC.
            if (Platform == UnrealTargetPlatform.TVOS && BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
            {
                return;
            }

			// Compile the asset catalog immediately
			if(BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				FileReference OutputFile = FileReference.Combine(StageDirectory, "Assets.car");

				RemoteMac Remote = new RemoteMac(ProjectFile, Logger);
				Remote.RunAssetCatalogTool(Platform, ResourcesDir, OutputFile, Logger);
			}
			else
			{
				// Get the output file
				FileReference OutputFile = IOSToolChain.GetAssetCatalogFile(Platform, Executable);

				// Delete the Assets.car file to force the asset catalog to build every time, because
				// removals of files or copies of icons (for instance) with a timestamp earlier than
				// the last generated Assets.car will result in nothing built.
				if (FileReference.Exists(OutputFile))
				{
					FileReference.Delete(OutputFile);
				}

				// Run the process locally
				using(Process Process = new Process())
				{
					Process.StartInfo.FileName = "/usr/bin/xcrun";
					Process.StartInfo.Arguments = IOSToolChain.GetAssetCatalogArgs(Platform, ResourcesDir.FullName, OutputFile.Directory.FullName);; 
					Process.StartInfo.UseShellExecute = false;
					Utils.RunLocalProcess(Process);
				}
			}
		}

		/// <summary>
		/// Reads the per-platform .PackageVersionCounter file to get a version, and modifies it with updated value so that next build will get new version
		/// </summary>
		/// <param name="UProjectFile">Location of .uproject file (or null for the engine project)</param>
		/// <param name="Platform">Which plaform to look up</param>
		/// <returns></returns>
		public static string GetAndUpdateVersionFile(FileReference? UProjectFile, UnrealTargetPlatform Platform)
		{
			FileReference RunningVersionFilename = UProjectFile == null ?
				FileReference.Combine(Unreal.EngineDirectory, "Build", Platform.ToString(), "Engine.PackageVersionCounter") :
				FileReference.Combine(UProjectFile.Directory, "Build", Platform.ToString(), $"{UProjectFile.GetFileNameWithoutAnyExtensions()}.PackageVersionCounter");

			string CurrentVersion = "0.0";
			if (FileReference.Exists(RunningVersionFilename))
			{
				CurrentVersion = FileReference.ReadAllText(RunningVersionFilename);
			}

			string[] VersionParts = CurrentVersion.Split('.');
			int Major = int.Parse(VersionParts[0]);
			int Minor = int.Parse(VersionParts[1]);

			Minor++;

			DirectoryReference.CreateDirectory(RunningVersionFilename.Directory);
			FileReference.WriteAllText(RunningVersionFilename, $"{Major}.{Minor}");

			return CurrentVersion;
		}

		/// <summary>
		/// Genearate an run-only Xcode project, that is not meant to be used for anything else besides code-signing/running/etc of the native .app bundle
		/// </summary>
		/// <param name="UProjectFile">Location of .uproject file (or null for the engine project</param>
		/// <param name="Platform">The platform to generate a project for</param>
		/// <param name="bForDistribution">True if this is making a bild for uploading to app store</param>
		/// <param name="Logger">Logging object</param>
		/// <param name="GeneratedProjectFile">Returns the .xcworkspace that was made</param>
		public static void GenerateRunOnlyXcodeProject(FileReference? UProjectFile, UnrealTargetPlatform Platform, bool bForDistribution, ILogger Logger, out DirectoryReference? GeneratedProjectFile)
		{
			List<string> Options = new()
			{
				$"-platforms={Platform}",
				$"-{Platform}DeployOnly",
				"-NoIntellisens",
				"-IngnoreJunk",
				bForDistribution ? "-distribution" : "-development",
				"-IncludeTempTargets",
				"-projectfileformat = XCode",
				"-automated",
			};

			if (UProjectFile == null || UProjectFile.IsUnderDirectory(Unreal.EngineDirectory))
			{
				// @todo do we need these? where would the bundleid come from if there's no project?
//				Options.Add("-bundleID=" + BundleID);
//				Options.Add("-appname=" + AppName);
				// @todo add an option to only add Engine target?
			}
			else
			{
				Options.Add($"-project=\"{UProjectFile.FullName}\"");
				Options.Add("-game");
			}

			IOSToolChain.GenerateProjectFiles(UProjectFile, Options.ToArray(), Logger, out GeneratedProjectFile);
		}

		/// <summary>
		/// Version of FinalizeAppWithXcode that is meant for modern xcode mode, where we assume all codesigning is setup already in the project, so nothing else is needed
		/// </summary>
		/// <param name="XcodeProject">The .xcworkspace file to build</param>
		/// <param name="Platform">THe platform to make the .app for</param>
		/// <param name="SchemeName">The name of the scheme (basically the target on the .xcworkspace)</param>
		/// <param name="Configuration">Which configuration to make (Debug, etc)</param>
		/// <param name="bForDistribution">True if this is making a bild for uploading to app store</param>
		/// <param name="Logger">Logging object</param>
		public static void FinalizeAppWithModernXcode(DirectoryReference XcodeProject, UnrealTargetPlatform Platform, string SchemeName, string Configuration, bool bForDistribution, ILogger Logger)
		{
			FinalizeAppWithXcode(XcodeProject, Platform, SchemeName, Configuration, null, null, null, false, bForDistribution, bUseModernXcode: true, Logger);
		}

		/// <summary>
		/// Runs xcodebuild on a project (likely created with GenerateRunOnlyXcodeProject) to perform codesigning and creation of final .app
		/// </summary>
		/// <param name="XcodeProject">The .xcworkspace file to build</param>
		/// <param name="Platform">THe platform to make the .app for</param>
		/// <param name="SchemeName">The name of the scheme (basically the target on the .xcworkspace)</param>
		/// <param name="Configuration">Which configuration to make (Debug, etc)</param>
		/// <param name="Provision">An optional provision to codesign with (ignored in modern mod)</param>
		/// <param name="Certificate">An optional certificate to codesign with (ignored in modern mode)</param>
		/// <param name="Team">Optional Team to use when codesigning (ignored in modern mode)</param>
		/// <param name="bAutomaticSigning">True if using automatic codesigning (where provision and certificate are not used) (ignored in modern mode)</param>
		/// <param name="bForDistribution">True if this is making a bild for uploading to app store</param>
		/// <param name="bUseModernXcode">True if the project was made with modern xcode mode</param>
		/// <param name="Logger">Logging object</param>
		/// <returns></returns>
		public static int FinalizeAppWithXcode(DirectoryReference XcodeProject, UnrealTargetPlatform Platform, string SchemeName, string Configuration,
			string? Provision, string? Certificate, string? Team, bool bAutomaticSigning, bool bForDistribution, bool bUseModernXcode, ILogger Logger)
		{
			List<string> Arguments = new()
			{
				"UBT_NO_POST_DEPLOY=true",
				new IOSToolChainSettings(Logger).XcodeDeveloperDir + "usr/bin/xcodebuild",
				"build",
				$"-workspace \"{XcodeProject.FullName}\"",
				$"-scheme \"{SchemeName}\"",
				$"-configuration \"{Configuration}\"",
				$"-destination generic/platform=" + (Platform == UnrealTargetPlatform.TVOS ? "tvOS" : "iOS"),
				//$"-sdk {SDKName}",
			};

			if (bUseModernXcode)
			{
				Arguments.Add("-allowProvisioningUpdates");
				// xcode gets confused it we _just_ wrote out entitlements while generating the temp project, and it thinks it was modified _during_ building
				// but it wasn't, it was written before the build started
				Arguments.Add("CODE_SIGN_ALLOW_ENTITLEMENTS_MODIFICATION=YES");
			}
			else
			{
				if (bAutomaticSigning)
				{
					Arguments.Add("CODE_SIGN_IDENTITY=" + (bForDistribution ? "\"iPhone Distribution\"" : "\"iPhone Developer\""));
					Arguments.Add("CODE_SIGN_STYLE=\"Automatic\"");
					Arguments.Add("-allowProvisioningUpdates");
					Arguments.Add($"DEVELOPMENT_TEAM={Team}");
				}
				else
				{
					if (!string.IsNullOrEmpty(Certificate))
					{
						Arguments.Add($"CODE_SIGN_IDENTITY=\"{Certificate}\"");
					}
					else
					{
						Arguments.Add("CODE_SIGN_IDENTITY=" + (bForDistribution ? "\"iPhone Distribution\"" : "\"iPhone Developer\""));
					}
					if (!string.IsNullOrEmpty(Provision))
					{
						// read the provision to get the UUID
						if (File.Exists(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Provision))
						{
							string UUID = "";
							string AllText = File.ReadAllText(Environment.GetEnvironmentVariable("HOME") + "/Library/MobileDevice/Provisioning Profiles/" + Provision);
							int idx = AllText.IndexOf("<key>UUID</key>");
							if (idx > 0)
							{
								idx = AllText.IndexOf("<string>", idx);
								if (idx > 0)
								{
									idx += "<string>".Length;
									UUID = AllText.Substring(idx, AllText.IndexOf("</string>", idx) - idx);
									Arguments.Add($"PROVISIONING_PROFILE_SPECIFIER={UUID}");

									Logger.LogInformation("Extracted Provision UUID {0} from {1}", UUID, Provision);
								}
							}
						}
					}
				}
			}

			int ReturnCode = Utils.RunLocalProcessAndLogOutput("/usr/bin/env", string.Join(" ", Arguments), Logger);
//			DirectoryReference.Delete(XcodeProject, true);
			return ReturnCode;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="Platform"></param>
		/// <param name="PlatformGameConfig"></param>
		/// <param name="AppName"></param>
		/// <param name="MobileProvisionFile"></param>
		/// <param name="bForDistribution"></param>
		/// <param name="IntermediateDir"></param>
		public static void WriteEntitlements(UnrealTargetPlatform Platform, ConfigHierarchy PlatformGameConfig,
		string AppName, FileReference? MobileProvisionFile, bool bForDistribution, string IntermediateDir)
		{
			// get some info from the mobileprovisioning file
			// the iCloud identifier and the bundle id may differ
			string iCloudContainerIdentifier = "";
			string iCloudContainerIdentifiersXML = "<array><string>iCloud.$(CFBundleIdentifier)</string></array>";
			string UbiquityContainerIdentifiersXML = "<array><string>iCloud.$(CFBundleIdentifier)</string></array>";
			string iCloudServicesXML = "<array><string>CloudKit</string><string>CloudDocuments</string></array>";
			string UbiquityKVStoreIdentifiersXML = "\t<string>$(TeamIdentifierPrefix)$(CFBundleIdentifier)</string>";

			string OutputFileName = Path.Combine(IntermediateDir, AppName + ".entitlements");

			if (MobileProvisionFile != null && File.Exists(MobileProvisionFile.FullName))
			{
				Console.WriteLine("Write entitlements from provisioning file {0}", MobileProvisionFile);
				
				MobileProvisionContents MobileProvisionContent = MobileProvisionContents.Read(MobileProvisionFile);

				iCloudContainerIdentifier = MobileProvisionContent.GetNodeValueByName("com.apple.developer.icloud-container-identifiers");
				iCloudContainerIdentifiersXML = MobileProvisionContent.GetNodeXMLValueByName("com.apple.developer.icloud-container-identifiers");

				string entitlementXML = MobileProvisionContent.GetNodeXMLValueByName("com.apple.developer.icloud-services");

				if (!entitlementXML.Contains("*") || Platform == UnrealTargetPlatform.TVOS)
				{
					// for iOS, replace the generic value (*) with the default
					iCloudServicesXML = entitlementXML;
				}

				entitlementXML = MobileProvisionContent.GetNodeXMLValueByName("com.apple.developer.ubiquity-container-identifiers");
				if (!entitlementXML.Contains("*") || !bForDistribution)
				{
					// for distribution, replace the generic value (*) with the default
					UbiquityContainerIdentifiersXML = entitlementXML;
				}

				entitlementXML = MobileProvisionContent.GetNodeXMLValueByName("com.apple.developer.ubiquity-kvstore-identifier");
				if (!entitlementXML.Contains("*") || !bForDistribution)
				{
					// for distribution, replace the generic value (*) with the default
					UbiquityKVStoreIdentifiersXML = entitlementXML;
				}
			}
			else
			{
				Console.WriteLine("Couldn't locate the mobile provisioning file {0}", MobileProvisionFile);
			}

			// write the entitlements file
			{
				bool bCloudKitSupported = false;
				PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableCloudKitSupport", out bCloudKitSupported);
				Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName)!);
				// we need to have something so Xcode will compile, so we just set the get-task-allow, since we know the value,
				// which is based on distribution or not (true means debuggable)
				StringBuilder Text = new StringBuilder();
				Text.AppendLine("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
				Text.AppendLine("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">");
				Text.AppendLine("<plist version=\"1.0\">");
				Text.AppendLine("<dict>");
				Text.AppendLine("\t<key>get-task-allow</key>");
				Text.AppendLine(string.Format("\t<{0}/>", bForDistribution ? "false" : "true"));
				if (bCloudKitSupported)
				{
					if (iCloudContainerIdentifiersXML != "")
					{
						Text.AppendLine("\t<key>com.apple.developer.icloud-container-identifiers</key>");
						Text.AppendLine(iCloudContainerIdentifiersXML);
					}

					if (iCloudServicesXML != "")
					{
						Text.AppendLine("\t<key>com.apple.developer.icloud-services</key>");
						Text.AppendLine(iCloudServicesXML);
					}

					if (UbiquityContainerIdentifiersXML != "")
					{
						Text.AppendLine("\t<key>com.apple.developer.ubiquity-container-identifiers</key>");
						Text.AppendLine(UbiquityContainerIdentifiersXML);
					}

					if (UbiquityKVStoreIdentifiersXML != "")
					{
						Text.AppendLine("\t<key>com.apple.developer.ubiquity-kvstore-identifier</key>");
						Text.AppendLine(UbiquityKVStoreIdentifiersXML);
					}

					Text.AppendLine("\t<key>com.apple.developer.icloud-container-environment</key>");
					Text.AppendLine(string.Format("\t<string>{0}</string>", bForDistribution ? "Production" : "Development"));
				}

				bool bRemoteNotificationsSupported = false;
				PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableRemoteNotificationsSupport", out bRemoteNotificationsSupported);

				// for TVOS we need push notifications when building for distribution with CloudKit
				if (bCloudKitSupported && bForDistribution && Platform == UnrealTargetPlatform.TVOS)
				{
					bRemoteNotificationsSupported = true;
				}

				if (bRemoteNotificationsSupported)
				{
					Text.AppendLine("\t<key>aps-environment</key>");
					Text.AppendLine(string.Format("\t<string>{0}</string>", bForDistribution ? "production" : "development"));
				}

				// for Sign in with Apple
				bool bSignInWithAppleSupported = false;
				PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bEnableSignInWithAppleSupport", out bSignInWithAppleSupported);

				if (bSignInWithAppleSupported)
				{
					Text.AppendLine("\t<key>com.apple.developer.applesignin</key>");
					Text.AppendLine("\t<array><string>Default</string></array>");
				}

				// Add Multi-user support for tvOS
				bool bRunAsCurrentUser = false;
				PlatformGameConfig.GetBool("/Script/IOSRuntimeSettings.IOSRuntimeSettings", "bRunAsCurrentUser", out bRunAsCurrentUser);

				if (bRunAsCurrentUser && Platform == UnrealTargetPlatform.TVOS)
				{
					Text.AppendLine("\t<key>com.apple.developer.user-management</key>");
					Text.AppendLine("\t<array><string>runs-as-current-user</string></array>");
				}


				// End of entitlements
				Text.AppendLine("</dict>");
				Text.AppendLine("</plist>");

				if (File.Exists(OutputFileName))
				{
					// read existing file
					string ExisitingFileContents = File.ReadAllText(OutputFileName);
					bool bFileChanged = !ExisitingFileContents.Equals(Text.ToString(), StringComparison.Ordinal);
					// overwrite file if there are content changes
					if (bFileChanged)
					{
						File.WriteAllText(OutputFileName, Text.ToString());
					}
				}
				else
				{
					File.WriteAllText(OutputFileName, Text.ToString());
				}
			}

			// create a pList key named ICloudContainerIdentifier
			// to be used at run-time when intializing the CloudKit services
			if (iCloudContainerIdentifier != "")
			{
				string PListFile = IntermediateDir + "/" + AppName + "-Info.plist";
				if (File.Exists(PListFile))
				{
					string OldPListData = File.ReadAllText(PListFile);
					XDocument XDoc;
					try
					{
						XDoc = XDocument.Parse(OldPListData);
						if (XDoc.DocumentType != null)
						{
							XDoc.DocumentType.InternalSubset = null;
						}

						XElement? dictElement = XDoc.Root?.Element("dict");
						if (dictElement != null)
						{
							XElement containerIdKeyNew = new XElement("key", "ICloudContainerIdentifier");
							XElement containerIdValueNew = new XElement("string", iCloudContainerIdentifier);

							XElement? containerIdKey = dictElement.Elements("key").FirstOrDefault(x => x.Value == "ICloudContainerIdentifier");
							if (containerIdKey != null)
							{
								// if ICloudContainerIdentifier already exists in the pList file, update its value
								XElement? containerIdValue = containerIdKey.ElementsAfterSelf("string").FirstOrDefault();
								if (containerIdValue != null)
								{
									containerIdValue.Value = iCloudContainerIdentifier;
								}
								else
								{
									containerIdKey.AddAfterSelf(containerIdValueNew);
								}
							}
							else
							{
								// add ICloudContainerIdentifier to the pList
								dictElement.Add(containerIdKeyNew);
								dictElement.Add(containerIdValueNew);
							}

							XDoc.Save(PListFile);
						}
					}
					catch (Exception e)
					{
						throw new BuildException("plist is invalid {0}\n{1}", e, OldPListData);
					}

				}
			}
		}

	}
}
