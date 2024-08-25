// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Net.NetworkInformation;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Linq;
using UnrealBuildTool;
using System.Text;
using EpicGames.Core;
using System.Diagnostics;
using System.Collections.Concurrent;
using UnrealBuildBase;
using System.Text.Json;
using System.Net.Http;
using EpicGames.Serialization;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationScripts
{
	/// <summary>
	/// Helper command used for cooking.
	/// </summary>
	/// <remarks>
	/// Command line parameters used by this command:
	/// -clean
	/// </remarks>
	public partial class Project : CommandUtils
	{

		private static readonly object SyncLock = new object();

		public struct OrderFile : IComparable<OrderFile>
		{
			public enum OrderFileType
			{
				Custom,
				Game,
				Cooker,
				Editor
			}

			public OrderFile(FileReference FileRef, OrderFileType InType, int SpecIndex, int InPriority)
			{
				File = FileRef;
				OrderSpecIndex = SpecIndex;

				string FileName = FileRef.GetFileNameWithoutExtension();
				OrderType = InType;
				Priority = InPriority;

				//Check if we have an order number
				AppendOrder = -1;
				int index = FileName.LastIndexOf("_");
				if (index != -1)
				{
					index++;
					string Number = FileName.Substring(index, FileName.Length - index);
					AppendOrder = Convert.ToInt32(Number);
				}

				if(OrderType == OrderFileType.Cooker && AppendOrder == -1)
				{
					// make sure to sort CookerOpenOrder.log at the end since its produce by the cooking stage
					AppendOrder = Int32.MaxValue;
				}
			}

			public int CompareTo(OrderFile Other)
			{
				int compare = (OrderSpecIndex.CompareTo(Other.OrderSpecIndex));
				if (compare == 0)
				{
					return (AppendOrder.CompareTo(Other.AppendOrder));
				}
				return compare;
			}

			// Order files should be sorted by OrderSpecIndex and then by AppendOrder
			// OrderType can be used for filtering
			public int AppendOrder;
			public int OrderSpecIndex;
			// Priority at which this order file can claim files even if it appears later in the list than another order file
			public int Priority;

			public OrderFileType OrderType;

			public FileReference File;
		};

		struct OrderFileSpec
		{
			public string FileNamePattern;
			public OrderFile.OrderFileType OrderType;
			public bool Required;
			public int Priority;
		};

		private static List<string> GetHostAddresses(DeploymentContext SC)
		{
			List<string> HostAddresses = new List<string>();

			// Add localhost first for host platforms and skip it completely for other platforms.
			// Any Platform can implement ModifyFileHostAddresses to tweak this default behavior.
			string LocalHost = "127.0.0.1";
			if (BuildHostPlatform.Current.Platform == SC.StageTargetPlatform.PlatformType)
			{
				HostAddresses.Add(LocalHost);
			}
			
			NetworkInterface[] Interfaces = NetworkInterface.GetAllNetworkInterfaces();
			foreach (NetworkInterface Adapter in Interfaces)
			{
				if (BuildHostPlatform.Current.Platform != SC.StageTargetPlatform.PlatformType && Adapter.NetworkInterfaceType == NetworkInterfaceType.Loopback)
				{
					continue;
				}
				
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
				{
					if (Adapter.NetworkInterfaceType == NetworkInterfaceType.Loopback)
					{
						continue;
					}
				}
				else
				{
					if (Adapter.OperationalStatus != OperationalStatus.Up)
					{
						continue;
					}
				}

				IPInterfaceProperties IP = Adapter.GetIPProperties();
				foreach (UnicastIPAddressInformation UnicastAddress in IP.UnicastAddresses)
				{
					if (!InternalUtils.IsDnsEligible(UnicastAddress))
					{
						continue;
					}

					if (UnicastAddress.Address.AddressFamily != System.Net.Sockets.AddressFamily.InterNetwork)
					{
						continue;
					}

					string HostAddress = UnicastAddress.Address.ToString();
					if (HostAddress == LocalHost)
					{
						continue;
					}
					HostAddresses.Add(HostAddress);
				}
			}

			return HostAddresses.ToList();
		}

		/// <returns>The path for the BuildPatchTool executable depending on host platform.</returns>
		private static string GetBuildPatchToolExecutable()
		{
			if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
			{
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Win64/BuildPatchTool.exe");
			}
			if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Mac/BuildPatchTool");
			}
			if (UnrealBuildTool.BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
			{
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Engine/Binaries/Linux/BuildPatchTool");
			}

			throw new AutomationException(string.Format("Unknown host platform for BuildPatchTool - {0}", UnrealBuildTool.BuildHostPlatform.Current.Platform));
		}

		/// <summary>
		/// Checks the existence of the BuildPatchTool executable exists and builds it if it is missing
		/// </summary>
		private static void EnsureBuildPatchToolExists()
		{
			string BuildPatchToolExe = GetBuildPatchToolExecutable();
			if (!CommandUtils.FileExists_NoExceptions(BuildPatchToolExe))
			{
				lock (SyncLock)
				{
					if (!CommandUtils.FileExists_NoExceptions(BuildPatchToolExe))
					{
						UnrealBuildUtils.BuildBuildPatchTool(null, UnrealBuildTool.BuildHostPlatform.Current.Platform);
					}
				}
			}
		}

		/// <summary>
		/// Writes a pak response file to disk
		/// </summary>
		/// <param name="Filename"></param>
		/// <param name="ResponseFile"></param>
		private static void WritePakResponseFile(string Filename, Dictionary<string, string> ResponseFile, bool Compressed, bool RehydrateAssets, EncryptionAndSigning.CryptoSettings CryptoSettings, bool bForceFullEncryption)
		{
			using (var Writer = new StreamWriter(Filename, false, new System.Text.UTF8Encoding(true)))
			{
				foreach (var Entry in ResponseFile)
				{
					string Extension = Path.GetExtension(Entry.Key);
					string Line = String.Format("\"{0}\" \"{1}\"", Entry.Key, Entry.Value);

					// explicitly exclude some file types from compression
					if (Compressed && !Path.GetExtension(Entry.Key).Contains(".mp4") && !Extension.Contains("ushaderbytecode") && !Path.GetExtension(Entry.Key).Contains("upipelinecache"))
					{
						Line += " -compress";
					}

					// todo: Ideally we would know if the package is virtualized and only opt to rehydrate those packages, but we'd need to be able
					// to pipe that info this far.
					if(RehydrateAssets && (Extension.Contains(".uasset") || Extension.Contains(".umap")))
					{
						Line += " -rehydrate";
					}

					if (CryptoSettings != null)
					{
						bool bEncryptFile = bForceFullEncryption || CryptoSettings.bEnablePakFullAssetEncryption;
						bEncryptFile = bEncryptFile || (CryptoSettings.bEnablePakUAssetEncryption && Extension.Contains(".uasset"));
						bEncryptFile = bEncryptFile || (CryptoSettings.bEnablePakIniEncryption && Extension.Contains(".ini"));

						if (bEncryptFile)
						{
							Line += " -encrypt";
						}
					}

					Writer.WriteLine(Line);
				}
			}
		}

		/// <summary>
		/// Loads streaming install chunk manifest file from disk
		/// </summary>
		/// <param name="Filename"></param>
		/// <returns></returns>
		private static HashSet<string> ReadPakChunkManifest(string Filename)
		{
			var ResponseFile = ReadAllLines(Filename);
			var Result = new HashSet<string>(ResponseFile, StringComparer.InvariantCultureIgnoreCase);
			return Result;
		}

		private static string GetCommonUnrealPakArguments(List<OrderFile> PakOrderFileLocations, string AdditionalOptions, EncryptionAndSigning.CryptoSettings CryptoSettings, FileReference CryptoKeysCacheFilename, List<OrderFile> SecondaryPakOrderFileLocations, bool bUnattended)
		{
			StringBuilder CmdLine = new StringBuilder();
			if (CryptoKeysCacheFilename != null)
			{
				CmdLine.AppendFormat(" -cryptokeys={0}", CommandUtils.MakePathSafeToUseWithCommandLine(CryptoKeysCacheFilename.FullName));
			}
			if (PakOrderFileLocations != null && PakOrderFileLocations.Count() > 0)
			{
				CmdLine.AppendFormat(" -order={0}", CommandUtils.MakePathSafeToUseWithCommandLine(string.Join(",", PakOrderFileLocations.Select(u => u.File.FullName).ToArray())));
			}
			if (SecondaryPakOrderFileLocations != null && SecondaryPakOrderFileLocations.Count() > 0)
			{
				CmdLine.AppendFormat(" -secondaryOrder={0}", CommandUtils.MakePathSafeToUseWithCommandLine(string.Join(",", SecondaryPakOrderFileLocations.Select(u => u.File.FullName).ToArray())));
			}

			if (CryptoSettings != null && CryptoSettings.bDataCryptoRequired)
			{
				if (CryptoSettings.bEnablePakIndexEncryption)
				{
					CmdLine.AppendFormat(" -encryptindex");
				}
				if (CryptoSettings.bDataCryptoRequired && CryptoSettings.bEnablePakSigning && CryptoSettings.SigningKey.IsValid())
				{
					CmdLine.AppendFormat(" -sign");
				}
			}

			if (bUnattended)
			{
				// We don't want unrealpak popping up interactive dialogs while we're running a build
				CmdLine.AppendFormat(" -unattended");
			}

			CmdLine.Append(AdditionalOptions);

			return CmdLine.ToString();
		}

		static private string GetPakFileSpecificUnrealPakArguments(Dictionary<string, string> UnrealPakResponseFile, FileReference OutputLocation, string AdditionalOptions, bool Compressed, bool RehydrateAssets,  EncryptionAndSigning.CryptoSettings CryptoSettings, String PatchSourceContentPath, string EncryptionKeyGuid)
		{
			StringBuilder CmdLine = new StringBuilder(MakePathSafeToUseWithCommandLine(OutputLocation.FullName));

			// Force encryption of ALL files if we're using specific encryption key. This should be made an option per encryption key in the settings, but for our initial
			// implementation we will just assume that we require maximum security for this data.
			bool bForceEncryption = !string.IsNullOrEmpty(EncryptionKeyGuid);
			string PakName = Path.GetFileNameWithoutExtension(OutputLocation.FullName);
			string ResponseFilesPath = CombinePaths(CmdEnv.EngineSavedFolder, "ResponseFiles");
			InternalUtils.SafeCreateDirectory(ResponseFilesPath);
			string UnrealPakResponseFileName = CombinePaths(ResponseFilesPath, "PakList_" + PakName + ".txt");
			WritePakResponseFile(UnrealPakResponseFileName, UnrealPakResponseFile, Compressed, RehydrateAssets, CryptoSettings, bForceEncryption);
			CmdLine.AppendFormat(" -create={0}", CommandUtils.MakePathSafeToUseWithCommandLine(UnrealPakResponseFileName));

			if (!String.IsNullOrEmpty(PatchSourceContentPath))
			{
				CmdLine.AppendFormat(" -generatepatch={0} -tempfiles={1}", CommandUtils.MakePathSafeToUseWithCommandLine(PatchSourceContentPath), CommandUtils.MakePathSafeToUseWithCommandLine(CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "TempFiles" + Path.GetFileNameWithoutExtension(OutputLocation.FullName))));
			}
			if (CryptoSettings != null && CryptoSettings.bDataCryptoRequired)
			{
				if (!string.IsNullOrEmpty(EncryptionKeyGuid))
				{
					CmdLine.AppendFormat(" -EncryptionKeyOverrideGuid={0}", EncryptionKeyGuid);
				}
			}

			return CmdLine.ToString();
		}

		static public string GetUnrealPakArguments(FileReference ProjectPath, Dictionary<string, string> UnrealPakResponseFile, FileReference OutputLocation, List<OrderFile> PakOrderFileLocations, string AdditionalOptions, bool Compressed, EncryptionAndSigning.CryptoSettings CryptoSettings, FileReference CryptoKeysCacheFilename, String PatchSourceContentPath, string EncryptionKeyGuid, List<OrderFile> SecondaryPakOrderFileLocations, bool bUnattended)
		{
			bool RehydrateAssets = false;

			StringBuilder CmdLine = new StringBuilder(MakePathSafeToUseWithCommandLine(ProjectPath.FullName));
			CmdLine.Append(" ");
			CmdLine.Append(GetPakFileSpecificUnrealPakArguments(UnrealPakResponseFile, OutputLocation, "", Compressed, RehydrateAssets, CryptoSettings, PatchSourceContentPath, EncryptionKeyGuid));
			CmdLine.Append(" ");
			CmdLine.Append(GetCommonUnrealPakArguments(PakOrderFileLocations, AdditionalOptions, CryptoSettings, CryptoKeysCacheFilename, SecondaryPakOrderFileLocations, bUnattended));
			return CmdLine.ToString();
		}

		static private string GetIoStoreCommandArguments(
			Dictionary<string, string> UnrealPakResponseFile,
			string ContainerName,
			FileReference PakOutputLocation,
			DirectoryReference OptionalOutputLocation,
			bool bOnDemand,
			bool bCompressed,
			DirectoryReference OptionalStageLooseFileRootPath,
			EncryptionAndSigning.CryptoSettings CryptoSettings,
			string EncryptionKeyGuid,
			string PatchSourceContentPath,
			bool bGenerateDiffPatch,
			bool bIsDLC)
		{
			StringBuilder CmdLine = new StringBuilder();
			CmdLine.AppendFormat("-Output={0}", MakePathSafeToUseWithCommandLine(Path.ChangeExtension(PakOutputLocation.FullName, ".utoc")));

			if (OptionalOutputLocation != null)
			{
				CmdLine.AppendFormat(" -OptionalOutput={0}", MakePathSafeToUseWithCommandLine(OptionalOutputLocation.FullName));
			}
			CmdLine.AppendFormat(" -ContainerName={0}", ContainerName);
			if (!bIsDLC && !String.IsNullOrEmpty(PatchSourceContentPath))
			{
				CmdLine.AppendFormat(" -PatchSource={0}", CommandUtils.MakePathSafeToUseWithCommandLine(PatchSourceContentPath));
			}
			if (bGenerateDiffPatch)
			{
				CmdLine.Append(" -GenerateDiffPatch");
			}

			if (OptionalStageLooseFileRootPath != null)
			{
				CmdLine.AppendFormat(" -StageLooseFileRootPath=\"{0}\"", OptionalStageLooseFileRootPath);
			}

			// Force encryption of ALL files if we're using specific encryption key. This should be made an option per encryption key in the settings, but for our initial
			// implementation we will just assume that we require maximum security for this data.
			bool bForceEncryption = !string.IsNullOrEmpty(EncryptionKeyGuid);
			bool RehydrateAssets = false;
			string ResponseFilesPath = CombinePaths(CmdEnv.EngineSavedFolder, "ResponseFiles");
			InternalUtils.SafeCreateDirectory(ResponseFilesPath);
			string UnrealPakResponseFileName = CombinePaths(ResponseFilesPath, "PakListIoStore_" + ContainerName + ".txt");
			WritePakResponseFile(UnrealPakResponseFileName, UnrealPakResponseFile, bCompressed, RehydrateAssets, CryptoSettings, bForceEncryption);
			CmdLine.AppendFormat(" -ResponseFile={0}", CommandUtils.MakePathSafeToUseWithCommandLine(UnrealPakResponseFileName));

			if (CryptoSettings != null && CryptoSettings.bDataCryptoRequired)
			{
				if (CryptoSettings.bEnablePakIndexEncryption)
				{
					CmdLine.AppendFormat(" -encryptindex");
				}
				if (!string.IsNullOrEmpty(EncryptionKeyGuid))
				{
					CmdLine.AppendFormat(" -EncryptionKeyOverrideGuid={0}", EncryptionKeyGuid);
				}
				if (CryptoSettings.bDataCryptoRequired && CryptoSettings.bEnablePakSigning && CryptoSettings.SigningKey.IsValid())
				{
					CmdLine.AppendFormat(" -sign");
				}
			}

			if (bOnDemand)
			{
				CmdLine.AppendFormat(" -OnDemand");
			}

			return CmdLine.ToString();
		}

		public static FileReference GetUnrealPakLocation()
		{
			if (HostPlatform.Current.HostEditorPlatform == UnrealTargetPlatform.Win64)
			{
				return FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Win64", "UnrealPak.exe");
			}
			else
			{
				return FileReference.Combine(Unreal.EngineDirectory, "Binaries", HostPlatform.Current.HostEditorPlatform.ToString(), "UnrealPak");
			}
		}

		static public void RunUnrealPak(ProjectParams Params, Dictionary<string, string> UnrealPakResponseFile, FileReference OutputLocation, List<OrderFile> PakOrderFileLocations, string PlatformOptions, bool Compressed, EncryptionAndSigning.CryptoSettings CryptoSettings, FileReference CryptoKeysCacheFilename, String PatchSourceContentPath, string EncryptionKeyGuid, List<OrderFile> SecondaryPakOrderFileLocations = null)
		{
			if (UnrealPakResponseFile.Count < 1)
			{
				return;
			}

			string Arguments = GetUnrealPakArguments(Params.RawProjectPath, UnrealPakResponseFile, OutputLocation, PakOrderFileLocations, PlatformOptions, Compressed, CryptoSettings, CryptoKeysCacheFilename, PatchSourceContentPath, EncryptionKeyGuid, SecondaryPakOrderFileLocations, Params.Unattended);
			RunAndLog(CmdEnv, GetUnrealPakLocation().FullName, Arguments, Options: ERunOptions.Default | ERunOptions.UTF8Output);
		}

		static public void LogDeploymentContext(DeploymentContext SC)
		{
			Logger.LogDebug("Deployment Context **************");
			Logger.LogDebug("ProjectFile = {Arg0}", SC.RawProjectPath);
			Logger.LogDebug("ArchiveDir = {Arg0}", SC.ArchiveDirectory);
			Logger.LogDebug("IsCodeBasedUprojectFile = {Arg0}", SC.IsCodeBasedProject);
			Logger.LogDebug("DedicatedServer = {Arg0}", SC.DedicatedServer);
			Logger.LogDebug("Stage = {Arg0}", SC.Stage);
			Logger.LogDebug("StageTargetPlatform = {Arg0}", SC.StageTargetPlatform.PlatformType.ToString());
			Logger.LogDebug("InputRootDir = {Arg0}", SC.LocalRoot);
			Logger.LogDebug("InputProjectDir = {Arg0}", SC.ProjectRoot);
			Logger.LogDebug("PlatformDir = {Arg0}", SC.PlatformDir);
			Logger.LogDebug("StagedOutputDir = {Arg0}", SC.StageDirectory);
			Logger.LogDebug("ShortProjectName = {Arg0}", SC.ShortProjectName);
			Logger.LogDebug("ProjectArgForCommandLines = {Arg0}", SC.ProjectArgForCommandLines);
			Logger.LogDebug("RunRootDir = {Arg0}", SC.RuntimeRootDir);
			Logger.LogDebug("RunProjectDir = {Arg0}", SC.RuntimeProjectRootDir);
			Logger.LogDebug("PakFileInternalRoot = {Arg0}", SC.PakFileInternalRoot);
			Logger.LogDebug("PlatformUsesChunkManifests = {Arg0}", SC.PlatformUsesChunkManifests);
			Logger.LogDebug("End Deployment Context **************");
		}

		private static string GetInternationalizationPreset(ProjectParams Params, ConfigHierarchy PlatformGameConfig, bool bMustExist = true)
		{
			// Initialize internationalization preset.
			string InternationalizationPreset = Params.InternationalizationPreset;

			// Use configuration if otherwise lacking an internationalization preset.
			if (string.IsNullOrEmpty(InternationalizationPreset))
			{
				if (PlatformGameConfig != null)
				{
					PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "InternationalizationPreset", out InternationalizationPreset);
				}
			}

			// Error if no preset has been provided.
			if (bMustExist && string.IsNullOrEmpty(InternationalizationPreset))
			{
				throw new AutomationException("No internationalization preset was specified for packaging. This will lead to fatal errors when launching. Specify preset via commandline (-I18NPreset=) or project packaging settings (InternationalizationPreset).");
			}

			return InternationalizationPreset;
		}

		public static List<string> GetCulturesToStage(ProjectParams Params, ConfigHierarchy PlatformGameConfig, bool bMustExist = true)
		{
			// Initialize cultures to stage.
			List<string> CulturesToStage = null;

			// Use parameters if provided.
			if (Params.CulturesToCook != null && Params.CulturesToCook.Count > 0)
			{
				CulturesToStage = Params.CulturesToCook;
			}

			// Use configuration if otherwise lacking cultures to stage.
			if (CulturesToStage == null || CulturesToStage.Count == 0)
			{
				if (PlatformGameConfig != null)
				{
					PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "CulturesToStage", out CulturesToStage);
				}
			}

			// Error if no cultures have been provided.
			if (bMustExist && (CulturesToStage == null || CulturesToStage.Count == 0))
			{
				throw new AutomationException("No cultures were specified for cooking and packaging. This will lead to fatal errors when launching. Specify culture codes via commandline (-CookCultures=) or using project packaging settings (+CulturesToStage).");
			}

			return CulturesToStage;
		}

		public static bool ShouldStageLocalizationTarget(DeploymentContext SC, ConfigHierarchy PlatformGameConfig, string LocalizationTarget)
		{
			if (SC.LocalizationTargetsDenyList.Contains(LocalizationTarget))
			{
				return false;
			}

			// Chunked targets can be added to the manifest during cook, so skip them during staging otherwise we'll overwrite the chunk 0 data with the non-chunked version
			if (PlatformGameConfig != null)
			{
				List<string> LocalizationTargetsToChunk = null;
				PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "LocalizationTargetsToChunk", out LocalizationTargetsToChunk);

				if (LocalizationTargetsToChunk != null && LocalizationTargetsToChunk.Contains(LocalizationTarget))
				{
					var CookedLocalizationTargetDirectory = DirectoryReference.Combine(SC.PlatformCookDir, SC.ShortProjectName, "Content", "Localization", LocalizationTarget);
					return !DirectoryReference.Exists(CookedLocalizationTargetDirectory);
				}
			}

			return true;
		}

		public static void StageLocalizationDataForPlugin(DeploymentContext SC, List<string> CulturesToStage, FileReference Plugin)
		{
			PluginDescriptor Descriptor = PluginDescriptor.FromFile(Plugin);
			if (Descriptor.LocalizationTargets != null)
			{
				foreach (LocalizationTargetDescriptor LocalizationTarget in Descriptor.LocalizationTargets)
				{
					if (LocalizationTarget.LoadingPolicy == LocalizationTargetDescriptorLoadingPolicy.Always || LocalizationTarget.LoadingPolicy == LocalizationTargetDescriptorLoadingPolicy.Game)
					{
						DirectoryReference PluginLocTargetDirectory = DirectoryReference.Combine(Plugin.Directory, "Content", "Localization", LocalizationTarget.Name);
						if (ShouldStageLocalizationTarget(SC, null, LocalizationTarget.Name) && DirectoryReference.Exists(PluginLocTargetDirectory))
						{
							StageLocalizationDataForTarget(SC, CulturesToStage, PluginLocTargetDirectory);
						}
					}
				}
			}
		}

		public static void StageLocalizationDataForTarget(DeploymentContext SC, List<string> CulturesToStage, DirectoryReference SourceDirectory)
		{
			if (!DirectoryReference.Exists(SourceDirectory))
			{
				Logger.LogWarning("Failed to stage '{Arg0}' localization target as the directory does not exist! Full path: {Arg1}", SourceDirectory.GetDirectoryName(), SourceDirectory.FullName);
				return;
			}

			var PlatformSourceDirectory = new DirectoryReference(CombinePaths(SourceDirectory.FullName, "Platforms", ConfigHierarchy.GetIniPlatformName(SC.StageTargetPlatform.IniPlatformType)));
			if (!DirectoryReference.Exists(PlatformSourceDirectory))
			{
				PlatformSourceDirectory = null;
			}

			foreach (FileReference SourceFile in DirectoryReference.EnumerateFiles(SourceDirectory, "*.locmeta"))
			{
				SC.StageFile(StagedFileType.UFS, SourceFile);
			}

			foreach (string Culture in CulturesToStage)
			{
				StageLocalizationDataForCulture(SC, Culture, SourceDirectory);

				if (PlatformSourceDirectory != null)
				{
					StageLocalizationDataForCulture(SC, Culture, PlatformSourceDirectory);
				}
			}
		}

		public static void StageLocalizationDataForTargetsInDirectory(DeploymentContext SC, ConfigHierarchy PlatformGameConfig, List<string> CulturesToStage, DirectoryReference RootDirectory)
		{
			if (DirectoryReference.Exists(RootDirectory))
			{
				foreach (DirectoryReference LocTargetDirectory in DirectoryReference.EnumerateDirectories(RootDirectory))
				{
					if (ShouldStageLocalizationTarget(SC, PlatformGameConfig, LocTargetDirectory.MakeRelativeTo(RootDirectory)))
					{
						StageLocalizationDataForTarget(SC, CulturesToStage, LocTargetDirectory);
					}
				}
			}
		}

		private static void StageLocalizationDataForCulture(DeploymentContext SC, string CultureName, DirectoryReference SourceDirectory)
		{
			CultureName = CultureName.Replace('-', '_');

			string[] LocaleTags = CultureName.Replace('-', '_').Split('_');

			List<string> PotentialParentCultures = new List<string>();

			if (LocaleTags.Length > 0)
			{
				if (LocaleTags.Length > 1 && LocaleTags.Length > 2)
				{
					PotentialParentCultures.Add(string.Join("_", LocaleTags[0], LocaleTags[1], LocaleTags[2]));
				}
				if (LocaleTags.Length > 2)
				{
					PotentialParentCultures.Add(string.Join("_", LocaleTags[0], LocaleTags[2]));
				}
				if (LocaleTags.Length > 1)
				{
					PotentialParentCultures.Add(string.Join("_", LocaleTags[0], LocaleTags[1]));
				}
				PotentialParentCultures.Add(LocaleTags[0]);
			}

			foreach (DirectoryReference FoundDirectory in DirectoryReference.EnumerateDirectories(SourceDirectory, "*", SearchOption.TopDirectoryOnly))
			{
				string DirectoryName = CommandUtils.GetLastDirectoryName(FoundDirectory.FullName);
				string CanonicalizedPotentialCulture = DirectoryName.Replace('-', '_');

				if (PotentialParentCultures.Contains(CanonicalizedPotentialCulture))
				{
					foreach (FileReference SourceFile in DirectoryReference.EnumerateFiles(FoundDirectory, "*.locres", SearchOption.AllDirectories))
					{
						SC.StageFile(StagedFileType.UFS, SourceFile);
					}
				}
			}
		}

		private static void StageCookerSupportFilesFromReceipt(DeploymentContext SC)
		{
			if (string.IsNullOrEmpty(SC.CookerSupportFilesSubdirectory))
			{
				return;
			}

			// sdk components expected to be in the form: $(SomeVar)/sub/path/to/filespec
			// so we capture the SomeVar and sub/path/to/filespec
			// note that filespec could be a file or a wildcard
			Regex Var = new Regex(@"^\$\((.*)\)/(.*)$");

			HashSet<string> EnvVars = new();
			foreach (StageTarget Target in SC.StageTargets)
			{
				TargetReceipt Receipt = Target.Receipt;
				foreach (ReceiptProperty Prop in Receipt.AdditionalProperties)
				{
					if (Prop.Name.Equals("CookerSupportFiles", StringComparison.InvariantCultureIgnoreCase))
					{
						// Value is a path under a envvar: $(MySdk)/Tools/Converter.exe
						Match Match = Var.Match(Prop.Value);
						if (Match.Success)
						{
							string EnvVar = Match.Groups[1].Value;
							string SubPath = Match.Groups[2].Value.Replace("\\", "/");
							string EnvVarValue = Environment.GetEnvironmentVariable(EnvVar);

							if (EnvVarValue == null)
							{
								Logger.LogWarning("Using envvar that isn't set, so it can't be used to pull files from : '{EnvVar}'", EnvVar);
								continue;
							}
							if (!Directory.Exists(EnvVarValue))
							{
								Logger.LogWarning("Using envvar path that doesn't exist, so it can't be used to pull files from : '{EnvVarValue}' (from env var {EnvVar})", EnvVarValue, EnvVar);
								continue;
							}

							FileReference ExpandedPath = FileReference.Combine(new DirectoryReference(EnvVarValue), SubPath);
							foreach (FileReference File in DirectoryReference.EnumerateFiles(ExpandedPath.Directory, ExpandedPath.GetFileName()))
							{
								string ExpandedSubPath = File.MakeRelativeTo(new DirectoryReference(EnvVarValue));
								// insert the envvar into the path so that differnt sdks/platforms won't stomp on each other (since we don't know the platform, we can't use that)
								ExpandedSubPath = $"{SC.CookerSupportFilesSubdirectory}/{EnvVar}/{ExpandedSubPath}";

								StagedFileReference DestPath = new StagedFileReference(ExpandedSubPath);
								Logger.LogInformation("  Staging support file '{ExpandedPath}' to '{DestPath}'", ExpandedPath, DestPath);
								SC.StageFile(StagedFileType.SystemNonUFS, File, DestPath);
							}

							EnvVars.Add(EnvVar);
						}
					}
				}
			}


			// write a temp .bat file that will set the envvar 
			string TempBatch = Path.Combine(Path.GetTempPath(), "AutomationTool", Path.ChangeExtension(Path.GetRandomFileName(), "bat"));
			Directory.CreateDirectory(Path.GetDirectoryName(TempBatch));
			// add the Envvar onto the path, like we did above
			List<string> Lines = EnvVars.Select(x => $"set {x}=%~dp0{x}").ToList();
			// make sure the AutoSDK var is unset, as it will mess with testing (probably not needed for final dist but can't hurt if everything else works)
			Lines.Insert(0, "set UE_SDKS_ROOT=");
			File.WriteAllLines(TempBatch, Lines);

			// and stage it
			SC.StageFile(StagedFileType.SystemNonUFS, new FileReference(TempBatch), new StagedFileReference($"{SC.CookerSupportFilesSubdirectory}/ActivateSupportFiles.bat"));
		}

		private static void StageAdditionalDirectoriesFromConfig(DeploymentContext SC, DirectoryReference ProjectContentRoot, StagedDirectoryReference StageContentRoot, ConfigHierarchy PlatformGameConfig, bool bUFS, string ConfigKeyName)
		{
			List<string> ExtraDirs;
			if (PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", ConfigKeyName, out ExtraDirs))
			{
				// Each string has the format '(Path="TheDirToStage")'
				foreach (var PathStr in ExtraDirs)
				{
					string RelativePath = null;
					var PathParts = PathStr.Split('"');
					if (PathParts.Length == 3)
					{
						RelativePath = PathParts[1];
					}
					else if (PathParts.Length == 1)
					{
						RelativePath = PathParts[0];
					}
					if (RelativePath != null)
					{
						DirectoryReference InputDir = DirectoryReference.Combine(ProjectContentRoot, RelativePath);
						if (Directory.Exists(InputDir.FullName)==false)
						{
							Logger.LogWarning("Unable to find directory \"{Arg0}\" for staging, retrieved from \"/Script/UnrealEd.ProjectPackagingSettings\" \"{ConfigKeyName}\"", InputDir.FullName, ConfigKeyName);
							continue;
						}
						StagedDirectoryReference OutputDir = StagedDirectoryReference.Combine(StageContentRoot, RelativePath);
						if (bUFS)
						{
							List<FileReference> Files = SC.FindFilesToStage(InputDir, StageFilesSearch.AllDirectories);
							Files.RemoveAll(x => x.HasExtension(".uasset") || x.HasExtension(".umap") || (SC.DedicatedServer && x.HasExtension(".mp4")));
							SC.StageFiles(StagedFileType.UFS, InputDir, Files, OutputDir);
						}
						else
						{
							SC.StageFiles(StagedFileType.NonUFS, InputDir, StageFilesSearch.AllDirectories, OutputDir);
						}
					}
				}
			}
		}

		/// <summary>
		/// Figures out which plugins are enabled for a content-only plugins. This may include content-only plugins and editor plugins, which will not be in the default 
		/// target manifest, since it was compiled elsewhere.
		/// </summary>
		/// <param name="ProjectFile">The project being built</param>
		/// <param name="Targets">List of targets being staged</param>
		/// <returns>List of plugin files that should be staged</returns>
		private static List<FileReference> GetPluginsForContentProject(FileReference ProjectFile,
			List<TargetReceipt> Targets, List<DirectoryReference> AdditionalPluginDirectories)
		{
			ProjectDescriptor Project = ProjectDescriptor.FromFile(ProjectFile);
			List<PluginInfo> AvailablePlugins = UnrealBuildTool.Plugins.ReadAvailablePlugins(Unreal.EngineDirectory,
				ProjectFile.Directory, AdditionalPluginDirectories);

			HashSet<FileReference> Plugins = new HashSet<FileReference>();
			foreach (TargetReceipt Target in Targets)
			{
				// Find all the specifically enabled plugins for this target
				Dictionary<string, bool> EnabledPlugins = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);
				if (Project.Plugins != null)
				{
					foreach (PluginReferenceDescriptor Reference in Project.Plugins)
					{
						bool bEnabled = false;
						if (Reference.IsEnabledForPlatform(Target.Platform) && Reference.IsEnabledForTargetConfiguration(Target.Configuration) && Reference.IsEnabledForTarget(Target.TargetType))
						{
							bEnabled = true;
						}
						EnabledPlugins[Reference.Name] = bEnabled;
					}
				}

				// Add all the enabled-by-default plugins
				foreach (PluginInfo AvailablePlugin in AvailablePlugins)
				{
					if (!EnabledPlugins.ContainsKey(AvailablePlugin.Name))
					{
						EnabledPlugins[AvailablePlugin.Name] = AvailablePlugin.Descriptor.SupportsTargetPlatform(Target.Platform) && AvailablePlugin.IsEnabledByDefault(!Project.DisableEnginePluginsByDefault);
					}
				}

				// Add the enabled plugins
				foreach (PluginInfo AvailablePlugin in AvailablePlugins)
				{
					if (AvailablePlugin.Descriptor.SupportsTargetPlatform(Target.Platform) && EnabledPlugins[AvailablePlugin.Name])
					{
						Plugins.Add(AvailablePlugin.File);
					}
				}

				// Exclude all the plugins that would have been considered for the base target (editor only plugins that are enabled by default)
				foreach (PluginInfo AvailablePlugin in AvailablePlugins)
				{
					if (AvailablePlugin.LoadedFrom == PluginLoadedFrom.Engine && !AvailablePlugin.Descriptor.bInstalled && AvailablePlugin.IsEnabledByDefault(!Project.DisableEnginePluginsByDefault))
					{
						Plugins.Remove(AvailablePlugin.File);
					}
				}

				// Exclude all the existing plugin descriptors
				Plugins.ExceptWith(Target.RuntimeDependencies.Select(x => x.Path));
			}
			return Plugins.ToList();
		}

		private static void LoadPackageStoreManifest(ProjectParams Params, DeploymentContext SC)
		{
			if (!ShouldCreateIoStoreContainerFiles(Params, SC))
			{
				return;
			}
			FileReference PackageStoreManifestFile = FileReference.Combine(SC.MetadataDir, "packagestore.manifest");
			System.IO.FileInfo PackageStoreManifestFileInfo = PackageStoreManifestFile.ToFileInfo();
			if (PackageStoreManifestFileInfo.Exists)
			{
				SC.PackageStoreManifest = new PackageStoreManifest();
				SC.PackageStoreManifest.FullPath = PackageStoreManifestFile.FullName;
				if (PackageStoreManifestFileInfo.Length > (64 << 10))
				{
					// We assume that this is a loose file deploy and we only need to read zen manifests here
					return;
				}

				{
					// Temporary solution until we can reliably read the oplog from UAT
					FileReference CookedFilesManifestFile = FileReference.Combine(SC.MetadataDir, "cookedfiles.manifest");
					if (FileReference.Exists(CookedFilesManifestFile))
					{
						SC.PackageStoreManifest.ZenCookedFiles = new List<string>();
						string[] CookedFiles = FileReference.ReadAllLines(CookedFilesManifestFile);
						foreach (string RelativeFilename in CookedFiles)
						{
							SC.PackageStoreManifest.ZenCookedFiles.Add(FileReference.Combine(SC.PlatformCookDir, RelativeFilename).FullName);
						}
						return;
					}
				}

				byte[] ManifestData = File.ReadAllBytes(PackageStoreManifestFile.FullName);
				CbObject ManifestObject = new CbField(ManifestData).AsObject();
				CbObject ZenServerObject = ManifestObject["zenserver"].AsObject();
				if (ZenServerObject != CbObject.Empty)
				{
					Logger.LogInformation("Reading oplog from Zen...");
					string Host = "localhost";
					int Port = 8558;
					string ProjectId = ZenServerObject["projectid"].AsString();
					string OplogId = ZenServerObject["oplogid"].AsString();
					bool bAutoLaunch = false;
					CbObject SettingsObject = ZenServerObject["settings"].AsObject();
					if (SettingsObject != CbObject.Empty)
					{
						bAutoLaunch = SettingsObject["bAutoLaunch"].AsBool();
						CbObject AutoLaunchSettingsObject = SettingsObject["AutoLaunchSettings"].AsObject();
						if (AutoLaunchSettingsObject != CbObject.Empty)
						{
							Port = AutoLaunchSettingsObject["DesiredPort"].AsInt16();
						}
						CbObject ConnectExistingSettingsObject = SettingsObject["ConnectExistingSettings"].AsObject();
						if (ConnectExistingSettingsObject != CbObject.Empty)
						{
							Host = ConnectExistingSettingsObject["HostName"].AsString();
							Port = ConnectExistingSettingsObject["Port"].AsInt16();
						}
					}

					SC.PackageStoreManifest.ZenCookedFiles = new List<string>();

					bool bAttemptAutoLaunchOnFailure = bAutoLaunch;
					HttpResponseMessage HttpGetResult = null;
					for (; ; )
					{
						HttpClient HttpClient = new HttpClient();
						using var Request = new HttpRequestMessage(HttpMethod.Get, string.Format("http://{0}:{1}/prj/{2}/oplog/{3}/entries", Host, Port, ProjectId, OplogId));
						Request.Headers.Add("Accept", "application/x-ue-cb");
						try
						{
							HttpGetResult = HttpClient.Send(Request);
							break;
						}
						catch
						{
							if (bAttemptAutoLaunchOnFailure)
							{
								string Arguments = String.Format("IoStore -StartZenServerForStage -PackageStoreManifest={0}", MakePathSafeToUseWithCommandLine(SC.PackageStoreManifest.FullPath));
								if (Params.Unattended)
								{
									Arguments += " -unattended";
								}

								Logger.LogInformation("Running UnrealPak with arguments: {UnrealPakParams}", Arguments);
								RunAndLog(CmdEnv, GetUnrealPakLocation().FullName, Arguments, Options: ERunOptions.Default | ERunOptions.UTF8Output);
								bAttemptAutoLaunchOnFailure = false;
							}
							else
							{
								throw new AutomationException(String.Format("Failed sending oplog request to Zen at {0}:{1}. Ensure that the server is running.", Host, Port));
							}
						}
					}
					if (!HttpGetResult.IsSuccessStatusCode)
					{
						throw new AutomationException(String.Format("Failed reading oplog {0}.{1} from Zen. Ensure that cooking was successful.", ProjectId, OplogId));
					}
					Task<byte[]> ReadOplogTask = HttpGetResult.Content.ReadAsByteArrayAsync();
					ReadOplogTask.Wait();
					byte[] OplogData = ReadOplogTask.Result;
					CbObject OplogObject = new CbField(OplogData).AsObject();
					foreach (CbField EntryField in OplogObject["entries"].AsArray())
					{
						foreach (CbField PackageDataField in EntryField["packagedata"].AsArray())
						{
							string RelativeFilename = PackageDataField["filename"].AsString();
							SC.PackageStoreManifest.ZenCookedFiles.Add(FileReference.Combine(SC.PlatformCookDir, RelativeFilename).FullName);
						}
						foreach (CbField PackageDataField in EntryField["bulkdata"].AsArray())
						{
							string RelativeFilename = PackageDataField["filename"].AsString();
							SC.PackageStoreManifest.ZenCookedFiles.Add(FileReference.Combine(SC.PlatformCookDir, RelativeFilename).FullName);
						}
					}
				}
			}
			else
			{
				throw new AutomationException(String.Format("A package store manifest is required when staging to IoStore. Expected to find {0}. Ensure that cooking was successful.", PackageStoreManifestFile.FullName));
			}
		}

		public static bool SetUpStagingSourceDirectories(ProjectParams Params, DeploymentContext SC)
		{
			if (SC.CookPlatform == null)
			{
				return false;
			}

			if (SC.PlatformCookDir != null && SC.MetadataDir != null)
			{
				return true;
			}
			if (Params.HasDLCName)
			{
				// We are making a plugin
				string DLCCookedSubDir = Params.FindPluginRelativePathFromPlatformCookDir(Params.DLCFile, SC.ProjectRoot, SC.EngineRoot, SC.LocalRoot, SC.ShortProjectName);

				// Put all of the cooked dir into the staged dir
				if (String.IsNullOrEmpty(Params.CookOutputDir))
				{
					SC.PlatformCookDir = DirectoryReference.Combine(Params.DLCFile.Directory, "Saved", "Cooked", SC.CookPlatform);
				}
				else
				{
					SC.PlatformCookDir = new DirectoryReference(Params.CookOutputDir);
					if (!SC.PlatformCookDir.GetDirectoryName().Equals(SC.CookPlatform, StringComparison.InvariantCultureIgnoreCase))
					{
						SC.PlatformCookDir = DirectoryReference.Combine(SC.PlatformCookDir, SC.CookPlatform);
					}
				}
				SC.MetadataDir = DirectoryReference.Combine(SC.PlatformCookDir, DLCCookedSubDir, "Metadata");
			}
			else
			{
				if (!Params.CookOnTheFly && !Params.SkipCookOnTheFly) // only stage the UFS files if we are not using cook on the fly
				{
					// Get the final output directory for cooked data
					DirectoryReference CookOutputDir;
					if (!String.IsNullOrEmpty(Params.CookOutputDir))
					{
						CookOutputDir = new DirectoryReference(Params.CookOutputDir);
						if (!CookOutputDir.GetDirectoryName().Equals(SC.CookPlatform, StringComparison.InvariantCultureIgnoreCase))
						{
							CookOutputDir = DirectoryReference.Combine(CookOutputDir, SC.CookPlatform);
						}
					}
					else if (Params.CookInEditor)
					{
						CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "EditorCooked", SC.CookPlatform);
					}
					else
					{
						CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "Cooked", SC.CookPlatform);
					}

					SC.PlatformCookDir = CookOutputDir;
					SC.MetadataDir = DirectoryReference.Combine(CookOutputDir, SC.ShortProjectName, "Metadata");
				}
				else
				{
					// UE-58423
					DirectoryReference CookOutputDir;
					if (!String.IsNullOrEmpty(Params.CookOutputDir))
					{
						CookOutputDir = DirectoryReference.Combine(new DirectoryReference(Params.CookOutputDir), SC.CookPlatform);
					}
					else if (Params.CookInEditor)
					{
						CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "EditorCooked", SC.CookPlatform);
					}
					else
					{
						CookOutputDir = DirectoryReference.Combine(SC.ProjectRoot, "Saved", "Cooked", SC.CookPlatform);
					}
					SC.PlatformCookDir = CookOutputDir;
					SC.MetadataDir = DirectoryReference.Combine(CookOutputDir, SC.ShortProjectName, "Metadata");
				}
			}
			return true;
		}

		public static void CreateStagingManifest(ProjectParams Params, DeploymentContext SC)
		{
			if (!Params.Stage)
			{
				return;
			}
			var ThisPlatform = SC.StageTargetPlatform;

			Logger.LogInformation("Creating Staging Manifest...");

			if (!SetUpStagingSourceDirectories(Params, SC))
			{
				return;
			}

			ConfigHierarchy PlatformGameConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, DirectoryReference.FromFile(Params.RawProjectPath), SC.StageTargetPlatform.IniPlatformType, SC.CustomConfig);

			if (Params.HasIterateSharedCookedBuild)
			{
				// can't do shared cooked builds with DLC that's madness!!
				//check( Params.HasDLCName == false );

				// stage all the previously staged files
				SC.StageFiles(StagedFileType.NonUFS, DirectoryReference.Combine(SC.ProjectRoot, "Saved", "SharedIterativeBuild", SC.CookPlatform, "Staged"), StageFilesSearch.AllDirectories, StagedDirectoryReference.Root); // remap to the root directory
			}
			bool bCreatePluginManifest = false;
			if (Params.HasDLCName)
			{
				// Making a plugin
				DirectoryReference PlatformEngineDir = DirectoryReference.Combine(SC.PlatformCookDir, "Engine");
				DirectoryReference ProjectMetadataDir = DirectoryReference.Combine(SC.PlatformCookDir, SC.ShortProjectName, "Metadata");

				// The .uplugin file is staged differently for different DLC
				// The .uplugin file doesn't actually exist for mobile DLC
				if (FileReference.Exists(Params.DLCFile))
				{
					StagedFileType StageType = Params.DLCPakPluginFile ? StagedFileType.UFS : StagedFileType.NonUFS;
					if (Params.DLCOverrideCookedSubDir != null)
					{
						SC.StageFile(StageType, Params.DLCFile, new StagedFileReference(Path.Combine(Params.DLCOverrideCookedSubDir, Params.DLCFile.GetFileName())));
					}
					else
					{
						SC.StageFile(StageType, Params.DLCFile);
					}

					// Stage DLC localization targets
					List<string> CulturesToStage = GetCulturesToStage(Params, PlatformGameConfig, false);
					if (CulturesToStage != null)
					{
						StageLocalizationDataForPlugin(SC, CulturesToStage, Params.DLCFile);
					}
				}

				// Put the binaries into the staged dir
				DirectoryReference BinariesDir = DirectoryReference.Combine(Params.DLCFile.Directory, "Binaries");
				if (DirectoryReference.Exists(BinariesDir))
				{
					SC.StageFiles(StagedFileType.NonUFS, BinariesDir, "libUE4-*.so", StageFilesSearch.AllDirectories);
					SC.StageFiles(StagedFileType.NonUFS, BinariesDir, "UE4-*.dll", StageFilesSearch.AllDirectories);
					SC.StageFiles(StagedFileType.NonUFS, BinariesDir, "libUE4Server-*.so", StageFilesSearch.AllDirectories);
					SC.StageFiles(StagedFileType.NonUFS, BinariesDir, "UnrealServer-*.dll", StageFilesSearch.AllDirectories);
				}

				// Put the config files into the staged dir
				DirectoryReference ConfigDir = DirectoryReference.Combine(Params.DLCFile.Directory, "Config");
				if (DirectoryReference.Exists(ConfigDir))
				{
					SC.StageFiles(StagedFileType.UFS, ConfigDir, "*.ini", StageFilesSearch.AllDirectories);
				}

				if (Params.DLCActLikePatch)
				{
					DirectoryReference DLCContent = DirectoryReference.Combine(Params.DLCFile.Directory, "Content");
					string RelativeDLCContentPath = DLCContent.MakeRelativeTo(SC.LocalRoot);
					string RelativeRootContentPath = DirectoryReference.Combine(SC.ProjectRoot, "Content").MakeRelativeTo(SC.LocalRoot);

					SC.RemapDirectories.Add(Tuple.Create(new StagedDirectoryReference(RelativeDLCContentPath), new StagedDirectoryReference(RelativeRootContentPath)));
				}

				// Stage all the cooked data, this is the same rule as normal stage except we may skip Engine
				List<FileReference> CookedFiles = new List<FileReference>();

				if (DirectoryReference.Exists(SC.PlatformCookDir))
				{
					CookedFiles.AddRange(DirectoryReference.EnumerateFiles(SC.PlatformCookDir, "*", SearchOption.AllDirectories).ToList());
				}

				// When cooking to Zen get the list of cooked package files from the manifest
				LoadPackageStoreManifest(Params, SC);
				if (SC.PackageStoreManifest != null && SC.PackageStoreManifest.ZenCookedFiles != null)
				{
					foreach (string FilePath in SC.PackageStoreManifest.ZenCookedFiles)
					{
						CookedFiles.Add(new FileReference(FilePath));
					}
				}

				foreach (FileReference CookedFile in CookedFiles)
				{
					// Skip metadata directory
					if (CookedFile.Directory.IsUnderDirectory(SC.MetadataDir) || CookedFile.Directory.IsUnderDirectory(ProjectMetadataDir))
					{
						continue;
					}

					// Dedicated server cook doesn't save shaders so no Engine dir is created
					if ((!SC.DedicatedServer) && (!Params.DLCIncludeEngineContent) && CookedFile.Directory.IsUnderDirectory(PlatformEngineDir))
					{
						continue;
					}

					// Skip region files. These are cook-time metadata files that never need to ship.
					if (CookedFile.HasExtension(FileRegion.RegionsFileExtension))
					{
						continue;
					}

					// json files have never been staged
					// metallib files cannot *currently* be staged as UFS as the Metal API needs to mmap them from files on disk in order to function efficiently
					if (!CookedFile.HasExtension(".json") && !CookedFile.HasExtension(".metallib") && !CookedFile.HasExtension(".utoc") && !CookedFile.HasExtension(".ucas"))
					{
						SC.StageFile(StagedFileType.UFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(SC.PlatformCookDir)));
					}
				}

				FileReference PluginSettingsFile = FileReference.Combine(Params.DLCFile.Directory, "Config", "PluginSettings.ini");
				if (FileReference.Exists(PluginSettingsFile))
				{
					ConfigFile File = new ConfigFile(PluginSettingsFile);
					ConfigFileSection StageSettings;
					if (File.TryGetSection("StageSettings", out StageSettings))
					{
						foreach (ConfigLine Line in StageSettings.Lines)
						{
							if (Line.Key == "AdditionalNonUSFDirectories")
							{
								SC.StageFiles(StagedFileType.NonUFS, DirectoryReference.Combine(Params.DLCFile.Directory, Line.Value), "*.*", StageFilesSearch.AllDirectories);
							}
						}
					}

					if (SC.DedicatedServer)
					{
						ConfigFileSection StageSettingsServer;
						if (File.TryGetSection("StageSettingsServer", out StageSettingsServer))
						{
							foreach (ConfigLine Line in StageSettingsServer.Lines)
							{
								if (Line.Key == "AdditionalNonUSFDirectories")
								{
									SC.StageFiles(StagedFileType.NonUFS, DirectoryReference.Combine(Params.DLCFile.Directory, Line.Value), "*.*", StageFilesSearch.AllDirectories);
								}
							}
						}
					}
				}

				if (SC.CustomDeployment == null || !SC.CustomDeployment.GetFilesToStageForDLC(Params, SC))
				{
					ThisPlatform.GetFilesToStageForDLC(Params, SC);
				}

				bCreatePluginManifest = true;
			}
			else
			{
				if (!Params.IterateSharedBuildUsePrecompiledExe)
				{
					if (SC.CustomDeployment == null || !SC.CustomDeployment.PreGetFilesToDeployOrStage(Params, SC))
					{
						ThisPlatform.GetFilesToDeployOrStage(Params, SC);
					}
					SC.CustomDeployment?.PostGetFilesToDeployOrStage(Params, SC);

					// Stage any extra runtime dependencies from the receipts
					foreach (StageTarget Target in SC.StageTargets)
					{
						SC.StageRuntimeDependenciesFromReceipt(Target.Receipt, Target.RequireFilesExist, Params.UsePak(SC.StageTargetPlatform));
					}

					// Stage any content-only plugins for content-only projects. We don't have a custom executable for these.
					if (!Params.IsCodeBasedProject)
					{
						List<FileReference> PluginFiles = GetPluginsForContentProject(Params.RawProjectPath,
							SC.StageTargets.ConvertAll(x => x.Receipt), Params.AdditionalPluginDirectories);
						SC.StageFiles(StagedFileType.UFS, PluginFiles);
					}
				}

				// move the UECommandline.txt file to the root of the stage
				// this file needs to be treated as a UFS file for casing, but NonUFS for being put into the .pak file
				// @todo: Maybe there should be a new category - UFSNotForPak - and reevaluate all SystemNonUFS references
				FileReference CommandLineFile = FileReference.Combine(GetIntermediateCommandlineDir(SC), "UECommandLine.txt");
				if (FileReference.Exists(CommandLineFile))
				{
					StagedFileReference StagedCommandLineFile = new StagedFileReference("UECommandLine.txt");
					if (SC.StageTargetPlatform.DeployLowerCaseFilenames(StagedFileType.SystemNonUFS))
					{
						StagedCommandLineFile = StagedCommandLineFile.ToLowerInvariant();
					}
					SC.StageFile(StagedFileType.SystemNonUFS, CommandLineFile, StagedCommandLineFile);
				}

				DirectoryReference ProjectContentRoot = DirectoryReference.Combine(SC.ProjectRoot, "Content");
				StagedDirectoryReference StageContentRoot = StagedDirectoryReference.Combine(SC.RelativeProjectRootForStage, "Content");

				if (!Params.CookOnTheFly && !Params.SkipCookOnTheFly) // only stage the UFS files if we are not using cook on the fly
				{
					// Initialize internationalization preset.
					string InternationalizationPreset = GetInternationalizationPreset(Params, PlatformGameConfig);

					// Initialize cultures to stage.
					List<string> CulturesToStage = GetCulturesToStage(Params, PlatformGameConfig);

					// Initialize platform extensions to stage.
					var PlatformExtensionsToStage = new List<string>();
					{
						string PlatformExtensionName = ConfigHierarchy.GetIniPlatformName(SC.StageTargetPlatform.PlatformType);
						PlatformExtensionsToStage.Add(PlatformExtensionName);

						DataDrivenPlatformInfo.ConfigDataDrivenPlatformInfo Info = DataDrivenPlatformInfo.GetDataDrivenInfoForPlatform(SC.StageTargetPlatform.PlatformType);
						if (Info != null && Info.IniParentChain != null)
						{
							PlatformExtensionsToStage.AddRange(Info.IniParentChain);
						}
					}

					// Stage ICU internationalization data from Engine.
					var ICUDataVersion = SC.StageTargetPlatform.ICUDataVersion;
					SC.StageFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.LocalRoot, "Engine", "Content", "Internationalization", InternationalizationPreset, ICUDataVersion), StageFilesSearch.AllDirectories, new StagedDirectoryReference(String.Format("Engine/Content/Internationalization/{0}", ICUDataVersion)));

					// Engine ufs (content)
					StageConfigFiles(SC, DirectoryReference.Combine(SC.LocalRoot, "Engine", "Config"), null);

					// Stage the engine localization target
					if (ShouldStageLocalizationTarget(SC, null, "Engine"))
					{
						StageLocalizationDataForTarget(SC, CulturesToStage, DirectoryReference.Combine(SC.LocalRoot, "Engine", "Content", "Localization", "Engine"));
					}

					// Game ufs (content)
					SC.StageFile(StagedFileType.UFS, SC.RawProjectPath);

					StageConfigFiles(SC, DirectoryReference.Combine(SC.ProjectRoot, "Config"), null);

					if (SC.DedicatedServer)
					{
						var ToRestoreRestrictedFolder = SC.RestrictedFolderNames;

						// For every platform we depend on also stage their platform config files
						foreach (var PlatformDepends in Params.ServerDependentPlatformMap)
						{
							// StageConfigFiles will fail to find these config files due to filtering out restricted folder names
							// Just for this loop we remove the filter for these platorms and stage the config files
							// After the loop we restore the restricted folder to prevent staging anything unwanted
							SC.RestrictedFolderNames.ExceptWith(PlatformExports.GetIncludedFolderNames(PlatformDepends.Key.Type));

							StageConfigFiles(SC, DirectoryReference.Combine(SC.EngineRoot, "Config"), PlatformDepends.Key.ToString());
							StageConfigFiles(SC, DirectoryReference.Combine(SC.ProjectRoot, "Config"), PlatformDepends.Key.ToString());
							StagePlatformExtensionConfigFiles(SC, PlatformDepends.Key.ToString());
						}

						SC.RestrictedFolderNames = ToRestoreRestrictedFolder;
					}

					// Stage platform extension config files
					foreach (string PlatformExtensionToStage in PlatformExtensionsToStage)
					{
						StagePlatformExtensionConfigFiles(SC, PlatformExtensionToStage);
					}

					// Stage plugin config files
					List<KeyValuePair<StagedFileReference, FileReference>> StagedPlugins = SC.FilesToStage.UFSFiles.Where(x => x.Value.HasExtension(".uplugin")).ToList();
					List<PluginInfo> AvailablePlugins = new List<PluginInfo>();
					if (StagedPlugins.Count > 0 && PlatformExtensionsToStage.Count > 0)
					{
						AvailablePlugins = UnrealBuildTool.Plugins.ReadAvailablePlugins(Unreal.EngineDirectory,
							Params.RawProjectPath.Directory, Params.AdditionalPluginDirectories);
					}
					foreach (KeyValuePair<StagedFileReference, FileReference> StagedPlugin in StagedPlugins)
					{
						//PluginDescriptor Descriptor = PluginDescriptor.FromFile(StagedPlugin.Value);
						DirectoryReference PluginConfigDirectory = DirectoryReference.Combine(StagedPlugin.Value.Directory, "Config");
						if (DirectoryReference.Exists(PluginConfigDirectory))
						{
							SC.StageFiles(StagedFileType.UFS, PluginConfigDirectory, "*.ini", StageFilesSearch.AllDirectories);
						}

						// look in platform extension locations of this plugin for config files
						PluginInfo Info = AvailablePlugins.Find(x => x.File == StagedPlugin.Value);
						if (Info != null)
						{
							foreach (string PlatformExtensionToStage in PlatformExtensionsToStage)
							{
								foreach (FileReference ChildPlugin in Info.ChildFiles)
								{
									// only look at a child plugin for a platform we are staging (ie, it is in a /PlatformName/ subdir)
									if (ChildPlugin.ContainsName(PlatformExtensionToStage, 0))
									{
										// stage any config files that exist, into the destination plugin Config dir, so that Runtime will find them
										DirectoryReference PlatformConfigDirectory = DirectoryReference.Combine(ChildPlugin.Directory, "Config");
										if (DirectoryReference.Exists(PlatformConfigDirectory))
										{
											// stage info Plugin/Config/PlatformName/
											SC.StageFiles(StagedFileType.UFS, PlatformConfigDirectory, "*.ini", StageFilesSearch.AllDirectories, StagedDirectoryReference.Combine(StagedPlugin.Key.Directory, "Config", PlatformExtensionToStage));
										}
									}
								}
							}
						}
					}

					// Stage all project localization targets
					StageLocalizationDataForTargetsInDirectory(SC, PlatformGameConfig, CulturesToStage, DirectoryReference.Combine(SC.ProjectRoot, "Content", "Localization"));

					// Stage all plugin localization targets
					foreach (KeyValuePair<StagedFileReference, FileReference> StagedPlugin in StagedPlugins)
					{
						StageLocalizationDataForPlugin(SC, CulturesToStage, StagedPlugin.Value);
					}

					// Stage any platform extension localization targets
					foreach (string PlatformExtensionToStage in PlatformExtensionsToStage)
					{
						StageLocalizationDataForTargetsInDirectory(SC, null, CulturesToStage, DirectoryReference.Combine(SC.LocalRoot, "Engine", "Platforms", PlatformExtensionToStage, "Content", "Localization"));
						StageLocalizationDataForTargetsInDirectory(SC, PlatformGameConfig, CulturesToStage, DirectoryReference.Combine(SC.ProjectRoot, "Platforms", PlatformExtensionToStage, "Content", "Localization"));
					}

					// Stage any additional UFS and NonUFS paths specified in the project ini files; these dirs are relative to the game content directory
					if (PlatformGameConfig != null)
					{
						StageAdditionalDirectoriesFromConfig(SC, ProjectContentRoot, StageContentRoot, PlatformGameConfig, true, "DirectoriesToAlwaysStageAsUFS");
						// NonUFS files are never in pak files and should always be remapped
						StageAdditionalDirectoriesFromConfig(SC, ProjectContentRoot, StageContentRoot, PlatformGameConfig, false, "DirectoriesToAlwaysStageAsNonUFS");

						if (SC.DedicatedServer)
						{
							StageAdditionalDirectoriesFromConfig(SC, ProjectContentRoot, StageContentRoot, PlatformGameConfig, true, "DirectoriesToAlwaysStageAsUFSServer");
							// NonUFS files are never in pak files and should always be remapped
							StageAdditionalDirectoriesFromConfig(SC, ProjectContentRoot, StageContentRoot, PlatformGameConfig, false, "DirectoriesToAlwaysStageAsNonUFSServer");
						}
					}

					// Per-project, per-platform setting to skip all movies. By default this is false.
					bool bSkipMovies = false;
					PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bSkipMovies", out bSkipMovies);

					if (!bSkipMovies && !SC.DedicatedServer)
					{
						List<string> MovieDenyList;
						PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "MovieDenyList", out MovieDenyList);
						if (MovieDenyList == null)
						{
							// Make an empty list to avoid having to null check below
							MovieDenyList = new List<string>();
						}

						// UFS is required when using a file server
						StagedFileType MovieFileType = Params.FileServer ? StagedFileType.UFS : StagedFileType.NonUFS;

						DirectoryReference EngineMoviesDir = DirectoryReference.Combine(SC.EngineRoot, "Content", "Movies");
						if (DirectoryReference.Exists(EngineMoviesDir))
						{
							List<FileReference> MovieFiles = SC.FindFilesToStage(EngineMoviesDir, StageFilesSearch.AllDirectories);
							SC.StageFiles(MovieFileType, MovieFiles.Where(x => !x.HasExtension(".uasset") && !x.HasExtension(".umap") && !MovieDenyList.Contains(x.GetFileNameWithoutAnyExtensions())));
						}

						DirectoryReference ProjectMoviesDir = DirectoryReference.Combine(SC.ProjectRoot, "Content", "Movies");
						if (DirectoryReference.Exists(ProjectMoviesDir))
						{
							List<FileReference> MovieFiles = SC.FindFilesToStage(ProjectMoviesDir, StageFilesSearch.AllDirectories);
							SC.StageFiles(MovieFileType, MovieFiles.Where(x => !x.HasExtension(".uasset") && !x.HasExtension(".umap") && !MovieDenyList.Contains(x.GetFileNameWithoutAnyExtensions())));
						}
					}
					else if (!SC.DedicatedServer)
					{
						// check to see if we have any specific movies we want to stage for non ufs and ufs files
						// we still use the movie directories to find the paths to the movies and the list in the ini file is just the substring filename you wish to have staged
						List<string> NonUFSMovieList;
						List<string> UFSMovieList;
						PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "NonUFSMovies", out NonUFSMovieList);
						PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "UFSMovies", out UFSMovieList);
						DirectoryReference EngineMoviesDir = DirectoryReference.Combine(SC.EngineRoot, "Content", "Movies");
						if (DirectoryReference.Exists(EngineMoviesDir))
						{
							List<FileReference> MovieFiles = SC.FindFilesToStage(EngineMoviesDir, StageFilesSearch.AllDirectories);
							if (NonUFSMovieList != null)
							{
								SC.StageFiles(StagedFileType.NonUFS, MovieFiles.Where(x => !x.HasExtension(".uasset") && !x.HasExtension(".umap") && NonUFSMovieList.Where(y => x.GetFileNameWithoutExtension().Contains(y)).Any()));
							}
							if (UFSMovieList != null)
							{
								SC.StageFiles(StagedFileType.UFS, MovieFiles.Where(x => !x.HasExtension(".uasset") && !x.HasExtension(".umap") && UFSMovieList.Where(y => x.GetFileNameWithoutExtension().Contains(y)).Any()));
							}
						}

						DirectoryReference ProjectMoviesDir = DirectoryReference.Combine(SC.ProjectRoot, "Content", "Movies");
						if (DirectoryReference.Exists(ProjectMoviesDir))
						{
							List<FileReference> MovieFiles = SC.FindFilesToStage(ProjectMoviesDir, StageFilesSearch.AllDirectories);
							if (NonUFSMovieList != null)
							{
								SC.StageFiles(StagedFileType.NonUFS, MovieFiles.Where(x => !x.HasExtension(".uasset") && !x.HasExtension(".umap") && NonUFSMovieList.Where(y => x.GetFileNameWithoutExtension().Contains(y)).Any()));
							}
							if (UFSMovieList != null)
							{
								SC.StageFiles(StagedFileType.UFS, MovieFiles.Where(x => !x.HasExtension(".uasset") && !x.HasExtension(".umap") && UFSMovieList.Where(y => x.GetFileNameWithoutExtension().Contains(y)).Any()));
							}
						}
					}

					// shader cache
					{
						DirectoryReference ShaderCacheRoot = DirectoryReference.Combine(SC.ProjectRoot, "Content");
						List<FileReference> ShaderCacheFiles = SC.FindFilesToStage(ShaderCacheRoot, "DrawCache-*.ushadercache", StageFilesSearch.TopDirectoryOnly);
						SC.StageFiles(StagedFileType.UFS, ShaderCacheFiles);
					}
					// pipeline cache
					{
						DirectoryReference ShaderCacheRoot = DirectoryReference.Combine(SC.ProjectRoot, "Content", "PipelineCaches", SC.PlatformDir);
						if (DirectoryReference.Exists(ShaderCacheRoot))
						{
							List<FileReference> ShaderCacheFiles = SC.FindFilesToStage(ShaderCacheRoot, "*.upipelinecache", StageFilesSearch.TopDirectoryOnly);
							SC.StageFiles(StagedFileType.UFS, ShaderCacheFiles);
						}
					}

					// Stage all the cooked data. Currently not filtering this by restricted folders, since we shouldn't mask invalid references by filtering them out.
					if (DirectoryReference.Exists(SC.PlatformCookDir))
					{
						List<FileReference> CookedFiles = DirectoryReference.EnumerateFiles(SC.PlatformCookDir, "*", SearchOption.AllDirectories).ToList();

						// When cooking to Zen get the list of cooked package files from the manifest
						LoadPackageStoreManifest(Params, SC);
						if (SC.PackageStoreManifest != null && SC.PackageStoreManifest.ZenCookedFiles != null)
						{
							foreach (string FilePath in SC.PackageStoreManifest.ZenCookedFiles)
							{
								CookedFiles.Add(new FileReference(FilePath));
							}
						}

						foreach (FileReference CookedFile in CookedFiles)
						{
							// Skip metadata directory
							if (CookedFile.Directory.IsUnderDirectory(SC.MetadataDir))
							{
								continue;
							}

							// Skip region files. These are cook-time metadata files that never need to ship.
							if (CookedFile.HasExtension(FileRegion.RegionsFileExtension))
							{
								continue;
							}

							if (CookedFile.HasExtension(".projectstore"))
							{
								if (!Params.UsePak(SC.StageTargetPlatform))
								{
									SC.StageFile(StagedFileType.NonUFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(SC.PlatformCookDir)));
								}
								continue;
							}

							// json files have never been staged
							// metallib files cannot *currently* be staged as UFS as the Metal API needs to mmap them from files on disk in order to function efficiently
							if (!CookedFile.HasExtension(".json") && !CookedFile.HasExtension(".metallib") && !CookedFile.HasExtension(".utoc") && !CookedFile.HasExtension(".ucas"))
							{
								SC.StageFile(StagedFileType.UFS, CookedFile, new StagedFileReference(CookedFile.MakeRelativeTo(SC.PlatformCookDir)));
							}
						}
					}

					SC.StageVulkanValidationLayerFiles(Params, StagedFileType.NonUFS, DirectoryReference.Combine(SC.EngineRoot, "Binaries", "ThirdParty", "Vulkan", SC.PlatformDir), StageFilesSearch.AllDirectories);

					// CrashReportClient is a standalone slate app that does not look in the generated pak file, so it needs the Content/Slate and Shaders/StandaloneRenderer folders Non-UFS
					// @todo Make CrashReportClient more portable so we don't have to do this
					UnrealTargetPlatform CrashReportPlatform = ThisPlatform.CrashReportPlatform ?? SC.StageTargetPlatform.PlatformType;
					if (SC.bStageCrashReporter && PlatformSupportsCrashReporter(CrashReportPlatform) && (Params.IterateSharedBuildUsePrecompiledExe == false))
					{
						SC.StageCrashReporterFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.EngineRoot, "Content", "Slate", "Common"), StageFilesSearch.AllDirectories);
						SC.StageCrashReporterFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.EngineRoot, "Content", "Slate", "Old"), StageFilesSearch.AllDirectories);
						SC.StageCrashReporterFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.EngineRoot, "Shaders", "StandaloneRenderer"), StageFilesSearch.AllDirectories);
						SC.StageCrashReporterFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.EngineRoot, "Content", "Internationalization", InternationalizationPreset), StageFilesSearch.AllDirectories, new StagedDirectoryReference("Engine/Content/Internationalization"));

						// Get the architecture in use
						UnrealArchitectures Architecture = Params.ProgramArchitecture;
						if (Architecture == null)
						{
							if (PlatformExports.IsPlatformAvailable(CrashReportPlatform))
							{
								Architecture = UnrealArchitectureConfig.ForPlatform(CrashReportPlatform).ActiveArchitectures(Params.RawProjectPath, TargetName: null);
							}
						}

						// Get the target receipt path for CrashReportClient
						FileReference ReceiptFileName = TargetReceipt.GetDefaultPath(SC.EngineRoot, "CrashReportClient", CrashReportPlatform, UnrealTargetConfiguration.Shipping, Architecture);
						if (!FileReference.Exists(ReceiptFileName))
						{
							throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Missing receipt '{0}'. Check that this target has been built.", ReceiptFileName);
						}

						// Read the receipt for this target
						TargetReceipt Receipt;
						if (!TargetReceipt.TryRead(ReceiptFileName, out Receipt))
						{
							throw new AutomationException("Missing or invalid target receipt ({0})", ReceiptFileName);
						}

						foreach (RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
						{
							StagedFileReference StagedFile = new StagedFileReference(RuntimeDependency.Path.MakeRelativeTo(Unreal.RootDirectory));
							SC.StageCrashReporterFile(RuntimeDependency.Type, RuntimeDependency.Path, StagedFile);
						}

						// Add config files.
						// TODO: Consider staging all engine ini files for parity with running in a workspace.
						//SC.StageCrashReporterFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.EngineRoot, "Config"), StageFilesSearch.TopDirectoryOnly);
						//SC.StageCrashReporterFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.EngineRoot, "Config", CrashReportPlatform.ToString()), StageFilesSearch.TopDirectoryOnly);
						var BaseIni = FileReference.Combine(SC.EngineRoot, "Config", "Base.ini");
						var StagedBaseIni = new StagedFileReference(BaseIni.MakeRelativeTo(Unreal.RootDirectory));
						SC.StageCrashReporterFile(StagedFileType.UFS, BaseIni, StagedBaseIni);
						SC.StageCrashReporterFiles(StagedFileType.UFS, DirectoryReference.Combine(SC.EngineRoot, "Programs", "CrashReportClient", "Config"), StageFilesSearch.AllDirectories);
					}
					// check if the game will be verifying ssl connections - if not, we can skip staging files that won't be needed
					bool bStageSSLCertificates = false;
					ConfigHierarchy PlatformEngineConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Params.RawProjectPath), SC.StageTargetPlatform.IniPlatformType, SC.CustomConfig);
					if (PlatformEngineConfig != null)
					{
						PlatformEngineConfig.GetBool("/Script/Engine.NetworkSettings", "n.VerifyPeer", out bStageSSLCertificates);
					}

					if (bStageSSLCertificates)
					{
						// Game's SSL certs
						FileReference ProjectCertFile = FileReference.Combine(SC.ProjectRoot, "Content", "Certificates", "cacert.pem");
						if (FileReference.Exists(ProjectCertFile))
						{
							SC.StageFile(StagedFileType.UFS, ProjectCertFile);
						}
						else
						{
							// if the game had any files to be staged, then we don't need to stage the engine one - it will just added hundreds of kb of data that is never used
							FileReference EngineCertFile = FileReference.Combine(SC.EngineRoot, "Content", "Certificates", "ThirdParty", "cacert.pem");
							if (FileReference.Exists(EngineCertFile))
							{
								SC.StageFile(StagedFileType.UFS, EngineCertFile);
							}
						}

						// now stage any other game certs besides cacert
						DirectoryReference CertificatesDir = DirectoryReference.Combine(SC.ProjectRoot, "Certificates");
						if (DirectoryReference.Exists(CertificatesDir))
						{
							SC.StageFiles(StagedFileType.UFS, CertificatesDir, "*.pem", StageFilesSearch.AllDirectories);
						}
					}

					// now make the fast-load ini/plugin file if requested
					// if -skippak is on, this indicates that the data is already ready to go, we are staging non-data files to re-use
					// data from a previous run, but still want to stage executables, etc. SkipPak indicates we _would_ have made a 
					// pak file, but we aren't this time, so we can assume the binary config is already inside the .pak file (and
					// the UnrealEditor.exe may not even exist if we are making a target-code-only build on top of existing build)
					if (Params.GenerateOptimizationData && !Params.SkipPak)
					{
						// get the list of plugins that need to be processed
						List<string> PluginFilesForTarget = new List<string>();
						foreach (KeyValuePair<StagedFileReference, FileReference> StagedPlugin in StagedPlugins)
						{
							PluginFilesForTarget.Add(StagedPlugin.Value.FullName);
						}
						FileReference PluginListFile = FileReference.Combine(SC.ProjectRoot, "Intermediate", "Config", "PluginList.txt");
						DirectoryReference.CreateDirectory(PluginListFile.Directory);
						File.WriteAllLines(PluginListFile.FullName, PluginFilesForTarget.ToArray());

						// run the commandlet to generate a binary file
						String TargetPlatformName = ThisPlatform.GetCookPlatform(Params.DedicatedServer, Params.Client);
						FileReference OutputFile = FileReference.Combine(SC.ProjectRoot, "Intermediate", "Config", TargetPlatformName, "BinaryConfig.ini");
						String UnrealPakParams = String.Format("MakeBinaryConfig -Project=\"{0}\" -Platform={1} -OutputFile=\"{2}\" -StagedPluginsFile=\"{3}\"", SC.RawProjectPath, ConfigHierarchy.GetIniPlatformName(ThisPlatform.IniPlatformType), OutputFile.FullName, PluginListFile.FullName);

						Logger.LogInformation("Running UnrealPak with arguments: {UnrealPakParams}", UnrealPakParams);
						RunAndLog(CmdEnv, GetUnrealPakLocation().FullName, UnrealPakParams, Options: ERunOptions.Default | ERunOptions.UTF8Output);

						SC.StageFile(StagedFileType.UFS, OutputFile, StagedFileReference.Combine(SC.RelativeProjectRootForStage, "Config", OutputFile.GetFileName()));
					}

					bCreatePluginManifest = true;
				}
				else
				{
					if (PlatformGameConfig != null)
					{
						List<string> ExtraNonUFSDirs;
						if (PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "DirectoriesToAlwaysStageAsNonUFS", out ExtraNonUFSDirs))
						{
							// Each string has the format '(Path="TheDirToStage")'
							foreach (var PathStr in ExtraNonUFSDirs)
							{
								var PathParts = PathStr.Split('"');
								if (PathParts.Length == 3)
								{
									var RelativePath = PathParts[1];
									SC.StageFiles(StagedFileType.NonUFS, DirectoryReference.Combine(ProjectContentRoot, RelativePath), StageFilesSearch.AllDirectories);
								}
								else if (PathParts.Length == 1)
								{
									var RelativePath = PathParts[0];
									SC.StageFiles(StagedFileType.UFS, DirectoryReference.Combine(ProjectContentRoot, RelativePath), StageFilesSearch.AllDirectories, StagedDirectoryReference.Combine(StageContentRoot, RelativePath));
								}
							}
						}
					}
				}

				StageCookerSupportFilesFromReceipt(SC);
			}

			// Allow the calling scripts to make modifications to the deployment context before we finalize it
			if (Params.ModifyDeploymentContextCallback != null)
			{
				Params.ModifyDeploymentContextCallback(Params, SC);
			}

			// Apply all the directory mappings
			SC.FilesToStage.UFSFiles = SC.FilesToStage.UFSFiles.ToDictionary(x => DeploymentContext.ApplyDirectoryRemap(SC, x.Key), x => x.Value);
			SC.FilesToStage.NonUFSFiles = SC.FilesToStage.NonUFSFiles.ToDictionary(x => DeploymentContext.ApplyDirectoryRemap(SC, x.Key), x => x.Value);
			SC.FilesToStage.NonUFSDebugFiles = SC.FilesToStage.NonUFSDebugFiles.ToDictionary(x => DeploymentContext.ApplyDirectoryRemap(SC, x.Key), x => x.Value);
			SC.FilesToStage.NonUFSSystemFiles = SC.FilesToStage.NonUFSSystemFiles.ToDictionary(x => DeploymentContext.ApplyDirectoryRemap(SC, x.Key), x => x.Value);

			// Create plugin manifests after the directory mappings
			if (bCreatePluginManifest && Params.UsePak(SC.StageTargetPlatform))
			{
				// Generate a plugin manifest if we're using a pak file and not creating a mod. Mods can be enumerated independently by users copying them into the Mods directory.
				if (Params.HasDLCName)
				{
					CreatePluginManifest(SC, SC.FilesToStage.UFSFiles, StagedFileType.UFS, Params.DLCFile.GetFileNameWithoutAnyExtensions());
				}
				else
				{
					CreatePluginManifest(SC, SC.FilesToStage.UFSFiles, StagedFileType.UFS, Params.ShortProjectName);
				}
			}

			// Make sure there are no restricted folders in the output
			HashSet<StagedFileReference> RestrictedFiles = new HashSet<StagedFileReference>();
			foreach (string RestrictedName in SC.RestrictedFolderNames)
			{
				RestrictedFiles.UnionWith(SC.FilesToStage.UFSFiles.Keys.Where(x => x.ContainsName(RestrictedName)));
				RestrictedFiles.UnionWith(SC.FilesToStage.NonUFSFiles.Keys.Where(x => x.ContainsName(RestrictedName)));
				RestrictedFiles.UnionWith(SC.FilesToStage.NonUFSDebugFiles.Keys.Where(x => x.ContainsName(RestrictedName)));
				RestrictedFiles.UnionWith(SC.CrashReporterUFSFiles.Keys.Where(x => x.ContainsName(RestrictedName)));
			}
			RestrictedFiles.RemoveWhere(RestrictedFile => SC.DirectoriesAllowList.Any(TestDirectory => RestrictedFile.Directory.IsUnderDirectory(TestDirectory)));
			RestrictedFiles.RemoveWhere(RestrictedFile => SC.ConfigFilesAllowList.Contains(RestrictedFile));
			RestrictedFiles.RemoveWhere(RestrictedFile => SC.ExtraFilesAllowList.Contains(RestrictedFile));

			if (RestrictedFiles.Count > 0)
			{
				List<string> RestrictedNames = new List<string>();
				foreach (string RestrictedFolderName in SC.RestrictedFolderNames)
				{
					if (RestrictedFiles.Any(x => x.ContainsName(RestrictedFolderName)))
					{
						RestrictedNames.Add(String.Format("\"{0}\"", RestrictedFolderName));
					}
				}

				StringBuilder Message = new StringBuilder();
				Message.AppendFormat("The following files are set to be staged, but contain restricted folder names ({0}):", String.Join(", ", RestrictedNames));
				foreach (StagedFileReference RestrictedFile in RestrictedFiles.OrderBy(x => x.Name))
				{
					Message.AppendFormat("\n{0}", RestrictedFile);
				}
				Message.Append("\n[Restrictions]");
				foreach (string RestrictedName in SC.RestrictedFolderNames)
				{
					Message.AppendFormat("\n{0}", RestrictedName);
				}
				Message.Append("\nIf these files are intended to be distributed in packaged builds, move the source files out of a restricted folder, or remap them during staging using the following syntax in DefaultGame.ini:");
				Message.Append("\n[Staging]");
				Message.Append("\n+RemapDirectories=(From=\"Foo/NoRedist\", To=\"Foo\")");
				if (RestrictedNames.Any(x => x != "NotForLicensees" && x != "NoRedist")) // We don't ever want internal stuff white-listing folders like this
				{
					Message.Append("\nAlternatively, allow list them using this syntax in DefaultGame.ini:");
					Message.Append("\n[Staging]");
					Message.Append("\n+AllowedDirectories=MyGame/Content/Foo");
				}
				throw new AutomationException(Message.ToString());
			}

			// Make all the filenames lowercase
			if (SC.StageTargetPlatform.DeployLowerCaseFilenames(StagedFileType.NonUFS))
			{
				SC.FilesToStage.NonUFSFiles = SC.FilesToStage.NonUFSFiles.ToDictionary(x => x.Key.ToLowerInvariant(), x => x.Value);
			}
			// DebugNonUFS is weird, so ask per-file
			SC.FilesToStage.NonUFSDebugFiles = SC.FilesToStage.NonUFSDebugFiles.ToDictionary(x => SC.StageTargetPlatform.DeployLowerCaseFile(x.Value, StagedFileType.DebugNonUFS) ? x.Key.ToLowerInvariant() : x.Key, x => x.Value);
			if (!Params.UsePak(SC.StageTargetPlatform) && SC.StageTargetPlatform.DeployLowerCaseFilenames(StagedFileType.UFS))
			{
				SC.FilesToStage.UFSFiles = SC.FilesToStage.UFSFiles.ToDictionary(x => x.Key.ToLowerInvariant(), x => x.Value);
			}

			// Remap all the non-ufs files if not using a PAK file
			// Shared NonUFS files are staged in their remapped location, and may be duplicated in the to-stage list.
			if (SC.StageTargetPlatform.RemapFileType(StagedFileType.NonUFS))
			{
				SC.FilesToStage.NonUFSFiles = SC.FilesToStage.NonUFSFiles.ToDictionary(x => SC.StageTargetPlatform.Remap(x.Key), x => x.Value);
			}

			if (SC.StageTargetPlatform.RemapFileType(StagedFileType.DebugNonUFS))
			{
				SC.FilesToStage.NonUFSDebugFiles = SC.FilesToStage.NonUFSDebugFiles.ToDictionary(x => SC.StageTargetPlatform.Remap(x.Key), x => x.Value);
			}

			if (SC.StageTargetPlatform.RemapFileType(StagedFileType.UFS) && !Params.UsePak(SC.StageTargetPlatform))
			{
				SC.FilesToStage.UFSFiles = SC.FilesToStage.UFSFiles.ToDictionary(x => SC.StageTargetPlatform.Remap(x.Key), x => x.Value);
			}

			// Merge all the NonUFS system files back into the NonUFS list. Deployment is currently only set up to read from that.
			foreach (KeyValuePair<StagedFileReference, FileReference> Pair in SC.FilesToStage.NonUFSSystemFiles)
			{
				SC.FilesToStage.NonUFSFiles[Pair.Key] = Pair.Value;
			}

			// Allow the calling scripts to make modifications to the deployment context before we finalize it
			if (Params.FinalizeDeploymentContextCallback != null)
			{
				Params.FinalizeDeploymentContextCallback(Params, SC);
			}

		}

		/// <summary>
		/// Stage the appropriate config files from the given input directory
		/// </summary>
		/// <param name="SC">The staging context</param>
		/// <param name="ConfigDir">Directory containing the config files</param>
		/// <param name="PlatformExtensionName">The name of the platform when staging config files from a platform extension</param>
		static void StageConfigFiles(DeploymentContext SC, DirectoryReference ConfigDir, string PlatformExtensionName)
		{
			List<FileReference> ConfigFiles = SC.FindFilesToStage(ConfigDir, "*.ini", StageFilesSearch.AllDirectories);
			foreach (FileReference ConfigFile in ConfigFiles)
			{
				Nullable<bool> ShouldStage = ShouldStageConfigFile(SC, ConfigDir, ConfigFile, PlatformExtensionName);
				if (ShouldStage == null)
				{
					StagedFileReference StagedFile = SC.GetStagedFileLocation(ConfigFile);
					Logger.LogWarning("The config file '{StagedFile}' will be staged, but is not explicitly allowed or denied. Add +AllowedConfigFiles={AllowedFile} or +DisallowedConfigFiles={DisallowedFile} to the [Staging] section of DefaultGame.ini"
						, StagedFile, StagedFile, StagedFile);
				}

				if (ShouldStage ?? true)
				{
					SC.StageFile(StagedFileType.UFS, ConfigFile);
					Logger.LogInformation("Including config file {ConfigFile}", ConfigFile);
				}
				else
				{
					Logger.LogInformation("Excluding config file {ConfigFile}", ConfigFile);
				}
			}
		}

		/// <summary>
		/// Looks for a potential PlatformExtension config directory
		/// </summary>
		/// <param name="SC">The staging context</param>
		/// <param name="PlatformName">The name of the platform/platform parent</param>
		static void StagePlatformExtensionConfigFiles(DeploymentContext SC, string PlatformName)
		{
			DirectoryReference PlatformEngineConfigDir = DirectoryReference.Combine(SC.LocalRoot, "Engine", "Platforms", PlatformName, "Config");
			DirectoryReference PlatformProjectConfigDir = DirectoryReference.Combine(SC.ProjectRoot, "Platforms", PlatformName, "Config");
			List<DirectoryReference> PlatformProgramExtendedConfigDirs = Unreal.GetExtensionDirs(DirectoryReference.Combine(SC.LocalRoot, "Engine"), "Programs")
				.Where( InPath => InPath.ContainsName(PlatformName,0))
				.ToList();

			void StageConfigFileIfExist(DirectoryReference InDir)
			{
				if (DirectoryReference.Exists(InDir))
				{
					StageConfigFiles(SC, InDir, PlatformName);
				}
			}

			StageConfigFileIfExist(PlatformEngineConfigDir);
			StageConfigFileIfExist(PlatformProjectConfigDir);

			// Find All Sub Program directories nested under Engine/<ExtendedDirs>/Platforms/PlatformName/Programs/ProgramName
			PlatformProgramExtendedConfigDirs.ForEach((InConfigDir) =>
			{
				DirectoryReference ProgramConfigDir = DirectoryReference.Combine(InConfigDir, SC.ShortProjectName, "Config");
				Logger.LogDebug("Platform Extended Directory checking: {ProgramConfigDir}", ProgramConfigDir);
				StageConfigFileIfExist(ProgramConfigDir);
			});
		}

		/// <summary>
		/// Determines if an individual config file should be staged
		/// </summary>
		/// <param name="SC">The staging context</param>
		/// <param name="ConfigDir">Directory containing the config files</param>
		/// <param name="ConfigFile">The config file to check</param>
		/// <param name="PlatformExtenionName">Name of platform to scan outside the main directory</param>
		/// <returns>True if the file should be staged, false otherwise</returns>
		static Nullable<bool> ShouldStageConfigFile(DeploymentContext SC, DirectoryReference ConfigDir, FileReference ConfigFile, string PlatformExtensionName)
		{
			StagedFileReference StagedConfigFile = SC.GetStagedFileLocation(ConfigFile);
			if (SC.ConfigFilesAllowList.Contains(StagedConfigFile))
			{
				return true;
			}
			if (SC.ConfigFilesDenyList.Contains(StagedConfigFile))
			{
				return false;
			}

			const string CustomPrefix = "custom/";
			string NormalizedPath = ConfigFile.MakeRelativeTo(ConfigDir).ToLowerInvariant().Replace('\\', '/');

			bool? ProcessCustomConfigPath(ref string Path)
			{
				if (Path.StartsWith(CustomPrefix))
				{
					// Strip custom prefix, then check if it matches our specified custom config
					Path = Path.Substring(CustomPrefix.Length);

					if (!string.IsNullOrEmpty(SC.CustomConfig))
					{
						string PrefixToStrip = SC.CustomConfig.ToLowerInvariant() + "/";
						if (Path.StartsWith(PrefixToStrip))
						{
							// Strip custom config path off the front
							Path = Path.Substring(PrefixToStrip.Length);
							return true;
						}
						else
						{
							// If custom config is specified, we want to ignore others automatically
							return false;
						}
					}
					else
					{
						// Ignore all custom config directories
						return false;
					}
				}

				return null;
			}

			if(ProcessCustomConfigPath(ref NormalizedPath) == false)
			{
				return false;
			}

			int DirectoryIdx = NormalizedPath.IndexOf('/');
			if (DirectoryIdx == -1)
			{
				const string BasePrefix = "base";
				if (NormalizedPath.StartsWith(BasePrefix))
				{
					string ShortName = NormalizedPath.Substring(BasePrefix.Length);
					if (PlatformExtensionName != null)
					{
						if (!ShortName.StartsWith(PlatformExtensionName, StringComparison.InvariantCultureIgnoreCase))
						{
							// Ignore config files in the platform directory that don't start with the platform name.
							return false;
						}

						ShortName = ShortName.Substring(PlatformExtensionName.Length);
					}

					return ShouldStageConfigSuffix(SC, ConfigFile, ShortName);
				}

				const string DefaultPrefix = "default";
				if (NormalizedPath.StartsWith(DefaultPrefix))
				{
					string ShortName = NormalizedPath.Substring(DefaultPrefix.Length);
					if (PlatformExtensionName != null)
					{
						if (!ShortName.StartsWith(PlatformExtensionName, StringComparison.InvariantCultureIgnoreCase))
						{
							// Ignore config files in the platform directory that don't start with the platform name.
							return false;
						}

						ShortName = ShortName.Substring(PlatformExtensionName.Length);
					}

					return ShouldStageConfigSuffix(SC, ConfigFile, ShortName);
				}

				const string DedicatedServerPrefix = "dedicatedserver";
				if (NormalizedPath.StartsWith(DedicatedServerPrefix))
				{
					return SC.DedicatedServer ? ShouldStageConfigSuffix(SC, ConfigFile, NormalizedPath.Substring(DedicatedServerPrefix.Length)) : false;
				}

				if (NormalizedPath == "consolevariables.ini")
				{
					return SC.StageTargetConfigurations.Any(x => x != UnrealTargetConfiguration.Test && x != UnrealTargetConfiguration.Shipping);
				}

				// this is needed for all targets
				if (NormalizedPath == "configredirects.ini" || NormalizedPath == "pluginbase.ini")
				{
					return true;
				}

				if (NormalizedPath == "locgatherconfig.ini")
				{
					return false;
				}

				if (NormalizedPath == "designertoolsconfig.ini")
				{
					return false;
				}

				if (PlatformExtensionName != null)
				{
					if (NormalizedPath.StartsWith(PlatformExtensionName, StringComparison.InvariantCultureIgnoreCase))
					{
						string ShortName = NormalizedPath.Substring(PlatformExtensionName.Length);
						return ShouldStageConfigSuffix(SC, ConfigFile, ShortName);
					}

					if (NormalizedPath == "datadrivenplatforminfo.ini")
					{
						return true;
					}
				}
			}
			else
			{
				if (NormalizedPath.StartsWith("layouts/"))
				{
					return true;
				}

				if (NormalizedPath.StartsWith("localization/"))
				{
					return false;
				}

				int INIStartIdx = NormalizedPath.LastIndexOf('/');
				string PlatformDirectoryName = NormalizedPath.Substring(0, DirectoryIdx);

				// Test for a platform file under a custom config.
				// Custom config files could have special setups per platform.  For non extension platforms
				// this will take the form of:
				// PlatformName/custom/CustomConfig/PlatformName{ini type}.ini
				if (INIStartIdx != DirectoryIdx)
				{
					string PlatformCustomPath = NormalizedPath.Substring(DirectoryIdx + 1);
					{
						// Test for a custom path under the platform name
						bool? CustomPlatformTest = ProcessCustomConfigPath(ref PlatformCustomPath);
						if (CustomPlatformTest == false)
						{
							return false;
						}
						else if (CustomPlatformTest == true)
						{
							// Valid custom path, strip out the customization parts and test
							NormalizedPath = $"{PlatformDirectoryName}/{PlatformCustomPath}";
						}
					}
				}

				string PlatformPrefix = $"{PlatformDirectoryName}/{PlatformDirectoryName}";
				if (NormalizedPath.StartsWith(PlatformPrefix))
				{
					string subPath = NormalizedPath.Substring(PlatformPrefix.Length);
					return ShouldStageConfigSuffix(SC, ConfigFile, subPath);
				}

				string PlatformBasePrefix = String.Format("{0}/base{0}", NormalizedPath.Substring(0, DirectoryIdx));
				if (NormalizedPath.StartsWith(PlatformBasePrefix))
				{
					return ShouldStageConfigSuffix(SC, ConfigFile, NormalizedPath.Substring(PlatformBasePrefix.Length));
				}

				if (NormalizedPath.EndsWith("/datadrivenplatforminfo.ini"))
				{
					return true;
				}

			}
			return null;
		}

		/// <summary>
		/// Determines if the given config file suffix ("engine", "game", etc...) should be staged for the given context.
		/// </summary>
		/// <param name="SC">The staging context</param>
		/// <param name="ConfigFile">Full path to the config file</param>
		/// <param name="InvariantSuffix">Suffix for the config file, as a lowercase invariant string</param>
		/// <returns>True if the suffix should be staged, false if not, null if unknown</returns>
		static Nullable<bool> ShouldStageConfigSuffix(DeploymentContext SC, FileReference ConfigFile, string InvariantSuffix)
		{
			if (SC.IniSuffixAllowList != null && SC.IniSuffixAllowList.Contains(InvariantSuffix))
			{
				return true;
			}

			if (SC.IniSuffixDenyList != null && SC.IniSuffixDenyList.Contains(InvariantSuffix))
			{
				return false;
			}

			return null;
		}

		static List<string> ParseInputPaths(List<string> ConfigLines)
		{
			// Each string has the format '(Path="TheDirToStage")'
			List<string> InputPaths = new List<string>();
			foreach (string ConfigLine in ConfigLines)
			{
				string[] PathParts = ConfigLine.Split('"');
				if (PathParts.Length == 3)
				{
					InputPaths.Add(PathParts[1]);
				}
				else if (PathParts.Length == 1)
				{
					InputPaths.Add(PathParts[0]);
				}
			}
			return InputPaths;
		}

		static PluginDescriptor CreatePluginDescriptorForManifest(DeploymentContext SC, FileReference File)
		{
			if (SC.CustomStageCopyHandler != null)
			{
				return SC.CustomStageCopyHandler.CreateDescriptorForPluginManifest(Logger, File);
			}
			else
			{
				return PluginDescriptor.FromFile(File);
			}
		}

		static void CreatePluginManifest(DeploymentContext SC, Dictionary<StagedFileReference, FileReference> FileMapping, StagedFileType FileType, string ManifestName)
		{
			// Get the path to the project's mods directory. We wont include anything under here in the manifest
			DirectoryReference ModsDir = DirectoryReference.Combine(SC.ProjectRoot, "Mods");

			// Find all the plugins that are being staged
			Dictionary<StagedFileReference, PluginDescriptor> StagedPlugins = new Dictionary<StagedFileReference, PluginDescriptor>();
			foreach (KeyValuePair<StagedFileReference, FileReference> File in FileMapping)
			{
				if (!File.Value.IsUnderDirectory(ModsDir) && File.Value.HasExtension(".uplugin"))
				{
					PluginDescriptor Descriptor = CreatePluginDescriptorForManifest(SC, File.Value);
					StagedPlugins[File.Key] = Descriptor;
				}
			}

			// If we have plugins, write out the manifest
			if (StagedPlugins.Count > 0)
			{
				string PluginManifestName = String.Format("{0}.upluginmanifest", ManifestName);

			FileReference PluginManifestFile = FileReference.Combine(SC.ProjectRoot, "Intermediate", "Staging", SC.StageTargetPlatform.PlatformType.ToString() , PluginManifestName);
				DirectoryReference.CreateDirectory(PluginManifestFile.Directory);

				using (JsonWriter Writer = new JsonWriter(PluginManifestFile.FullName))
				{
					Writer.WriteObjectStart();
					Writer.WriteArrayStart("Contents");
					foreach (KeyValuePair<StagedFileReference, PluginDescriptor> StagedPlugin in StagedPlugins)
					{
						Writer.WriteObjectStart();
						Writer.WriteValue("File", String.Format("../../../{0}", StagedPlugin.Key.Name));
						Writer.WriteObjectStart("Descriptor");
						StagedPlugin.Value.Write(Writer);
						Writer.WriteObjectEnd();
						Writer.WriteObjectEnd();
					}
					Writer.WriteArrayEnd();
					Writer.WriteObjectEnd();
				}

				SC.StageFile(FileType, PluginManifestFile, StagedFileReference.Combine(SC.RelativeProjectRootForStage, "Plugins", PluginManifestName));
			}
		}

		public static void DumpTargetManifest(Dictionary<StagedFileReference, FileReference> Mapping, FileReference Filename, DirectoryReference StageDir, HashSet<StagedFileReference> CRCFiles)
		{
			// const string Iso8601DateTimeFormat = "yyyy-MM-ddTHH:mm:ssZ"; // probably should work
			// const string Iso8601DateTimeFormat = "o"; // predefined universal Iso standard format (has too many millisecond spaces for our read code in FDateTime.ParseISO8601
			const string Iso8601DateTimeFormat = "yyyy'-'MM'-'dd'T'HH':'mm':'ss'.'fffZ";


			if (Mapping.Count > 0)
			{
				var Lines = new List<string>();
				foreach (var Pair in Mapping)
				{
					string TimeStamp = FileReference.GetLastWriteTimeUtc(Pair.Value).ToString(Iso8601DateTimeFormat);
					if (CRCFiles.Contains(Pair.Key))
					{
						byte[] FileData = File.ReadAllBytes(StageDir + "/" + Pair.Key);
						TimeStamp = BitConverter.ToString(System.Security.Cryptography.MD5.Create().ComputeHash(FileData)).Replace("-", string.Empty);
					}
					string Dest = Pair.Key.Name + "\t" + TimeStamp;

					Lines.Add(Dest);
				}

				DirectoryReference.CreateDirectory(Filename.Directory);
				WriteAllLines(Filename.FullName, Lines.ToArray());
			}
		}

		private static void CopyManifestFilesToStageDir(DeploymentContext SC, Dictionary<StagedFileReference, FileReference> Mapping, string ManifestName)
		{
			CopyOrWriteManifestFilesToStageDir(true, SC, Mapping, ManifestName);
		}

		private static void WriteManifestFilesToStageDir(DeploymentContext SC, Dictionary<StagedFileReference, FileReference> Mapping, string ManifestName)
		{
			CopyOrWriteManifestFilesToStageDir(false, SC, Mapping, ManifestName);
		}

		private static void CopyOrWriteManifestFilesToStageDir(bool bPerformCopy, DeploymentContext SC, Dictionary<StagedFileReference, FileReference> Mapping, string ManifestName)
		{
			if (bPerformCopy)
			{
				OverrideCopyDelegate OverrideCopyHandler = (SC.CustomStageCopyHandler != null) ? SC.CustomStageCopyHandler.StageFile : null;

				var StageDirectory = ManifestName == "DebugFiles" ? SC.DebugStageDirectory : SC.StageDirectory;
				Logger.LogInformation("Copying {ManifestName} to staging directory: {StageDirectory}", ManifestName, StageDirectory);
				foreach (KeyValuePair<StagedFileReference, FileReference> Pair in Mapping)
				{
					FileReference Src = Pair.Value;
					FileReference Dest = FileReference.Combine(StageDirectory, Pair.Key.Name);
					if (Src != Dest)  // special case for things created in the staging directory, like the pak file
					{
						CopyFileIncremental(Src, Dest, OverrideCopyHandler, IniKeyDenyList: SC.IniKeyDenyList, IniSectionDenyList: SC.IniSectionDenyList);
					}
				}
			}

			if (Mapping.Count > 0)
			{
				var ManifestPathStaged = FileReference.Combine(SC.DebugStageDirectory, $"Manifest_{ManifestName}_{SC.StageTargetPlatform.PlatformType}.txt");
				var ManifestPathLog = FileReference.Combine(new DirectoryReference(CmdEnv.LogFolder), ManifestPathStaged.GetFileName());

				DeleteFile(ManifestPathStaged.FullName);
				DeleteFile(ManifestPathLog.FullName);

				DumpTargetManifest(Mapping, ManifestPathLog, SC.StageDirectory, SC.StageTargetPlatform.GetFilesForCRCCheck());

				if (!FileReference.Exists(ManifestPathLog))
				{
					throw new AutomationException("Failed to write manifest {0}", ManifestPathLog);
				}

				if (SC.StageTargetPlatform.RequiresManifestFiles)
				{
					// Also copy the manifest file to the staged directory
					CopyFile(ManifestPathLog.FullName, ManifestPathStaged.FullName);
				}
			}
		}

		public static void DumpManifest(Dictionary<StagedFileReference, FileReference> Mapping, string Filename)
		{
			if (Mapping.Count > 0)
			{
				int NumItems = Mapping.Count;
				Logger.LogInformation("DumpManifest: {Filename}, NumItems: {NumItems}", Filename, NumItems);

				List<string> Lines = new List<string>();
				foreach (KeyValuePair<StagedFileReference, FileReference> Pair in Mapping)
				{
					Lines.Add("\"" + Pair.Value + "\" \"" + Pair.Key + "\"");
				}
				WriteAllLines(Filename, Lines.ToArray());
			}
		}

		public static void DumpManifest(DeploymentContext SC, string BaseFilename, bool DumpUFSFiles = true)
		{
			DumpManifest(SC.FilesToStage.NonUFSFiles, BaseFilename + "_NonUFSFiles.txt");
			if (DumpUFSFiles)
			{
				DumpManifest(SC.FilesToStage.NonUFSDebugFiles, BaseFilename + "_NonUFSFilesDebug.txt");
			}
			DumpManifest(SC.FilesToStage.UFSFiles, BaseFilename + "_UFSFiles.txt");
		}

		public static void CopyUsingStagingManifest(ProjectParams Params, DeploymentContext SC)
		{
			CopyManifestFilesToStageDir(SC, SC.FilesToStage.NonUFSFiles, "NonUFSFiles");

			Dictionary<StagedFileReference, FileReference> UFSFiles = new Dictionary<StagedFileReference, FileReference>(SC.FilesToStage.UFSFiles);
			bool bStageUnrealFileSystemFiles = !Params.CookOnTheFly && !Params.UsePak(SC.StageTargetPlatform) && !Params.FileServer;
			if (bStageUnrealFileSystemFiles)
			{
				foreach (KeyValuePair<StagedFileReference, FileReference> Pair in SC.CrashReporterUFSFiles)
				{
					FileReference ExistingLocation;
					if (!UFSFiles.TryGetValue(Pair.Key, out ExistingLocation))
					{
						UFSFiles.Add(Pair.Key, Pair.Value);
					}
					else if (ExistingLocation != Pair.Value)
					{
						throw new AutomationException("File '{0}' is set to be staged from '{1}' for project and '{2}' for crash reporter", Pair.Key, ExistingLocation, Pair.Value);
					}
				}
				CopyManifestFilesToStageDir(SC, UFSFiles, "UFSFiles");
			}
			else
			{
				// write out the manifest file even if we don't copy the files (so we can easily tell what went into the .pak file)
				WriteManifestFilesToStageDir(SC, UFSFiles, "UFSFiles");
			}

			// Copy debug files last
			// they do not respect the DeployLowerCaseFilenames() setting, but if copied to a case-insensitive staging directory first they determine the casing for outer directories (like Engine/Content) 
			if (!Params.NoDebugInfo)
			{
				CopyManifestFilesToStageDir(SC, SC.FilesToStage.NonUFSDebugFiles, "DebugFiles");
			}
		}

		// Pak file rules to apply to a list of files
		private struct PakFileRules
		{
			// Name of config section
			public string Name;

			// Combined filter from all +Files= entries
			public FileFilter Filter;

			// An alternative and efficient way to match files
			public HashSet<string> ExactFileMatches;

			// To use exact file match or file filter
			public bool bUseExactFilePathMatch;

			// Rather to exclude entirely from paks
			public bool bExcludeFromPaks;

			// Rather to allow overriding the chunk manifest, if false will only modify loose files
			public bool bOverrideChunkManifest;

			// Whether pak file rule is disabled
			public bool bDisabled;

			// List of pak files to use instead of manifest
			public List<string> OverridePaks;

			// Whether this rule is defined for content-on-demand
			public bool bOnDemand;

			public bool bStageLoose;

			// Encryption key
			public string EncryptionKeyGuid;

			// List of allowed chunk names when generating on demand chunk(s) 
			public List<Regex> OnDemandAllowedChunkFilters;

			public static bool IsMatch(PakFileRules PakRules, KeyValuePair<string, string> StagingFile)
			{
				bool bMatched = !PakRules.bDisabled &&
					((!PakRules.bUseExactFilePathMatch && PakRules.Filter != null && PakRules.Filter.Matches(StagingFile.Key)) ||
					(PakRules.bUseExactFilePathMatch && PakRules.ExactFileMatches != null && PakRules.ExactFileMatches.Contains(StagingFile.Value)));
				
				return bMatched;
			}
		};

		/// <summary>
		/// Reads Default/BasePakFileRules.ini and returns all PakFileRules objects that apply for this deployment
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		private static List<PakFileRules> GetPakFileRules(ProjectParams Params, DeploymentContext SC)
		{
			// if we want to ignore the rules while staging, just don't read any rules in!
			if (SC.UsePakFileRulesIni == false)
			{
				return new List<PakFileRules>();
			}

			bool bWarnedAboutMultipleTargets = false;

			/*List<ConfigFile> ConfigFiles = new List<ConfigFile>();
			FileReference BaseConfigFileReference = FileReference.Combine(SC.EngineRoot, "Config", "BasePakFileRules.ini");
			if (FileReference.Exists(BaseConfigFileReference))
			{
				ConfigFiles.Add(new ConfigFile(BaseConfigFileReference));
				bFoundConfig = true;
			}

			FileReference ProjectConfigFileReference = FileReference.Combine(SC.ProjectRoot, "Config", "DefaultPakFileRules.ini");
			if (FileReference.Exists(ProjectConfigFileReference))
			{
				ConfigFiles.Add(new ConfigFile(ProjectConfigFileReference));
				bFoundConfig = true;
			}

			if (!bFoundConfig)
			{
				return null;
			}
			ConfigHierarchy PakRulesConfig = new ConfigHierarchy(ConfigFiles);*/



			ConfigHierarchy PakRulesConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.PakFileRules, DirectoryReference.FromFile(Params.RawProjectPath), SC.StageTargetPlatform.IniPlatformType, SC.CustomConfig);

			string IniPlatformName = ConfigHierarchy.GetIniPlatformName(SC.StageTargetPlatform.IniPlatformType);

			bool bChunkedBuild = SC.PlatformUsesChunkManifests && DoesChunkPakManifestExist(Params, SC);

			// pull a list of sections that are not rules that will be skipped over
			List<string> SectionsWithoutRulesList;
			HashSet<string> SectionsWithoutRules = new();
			if (PakRulesConfig.GetArray("SectionsWithoutRules", "Section", out SectionsWithoutRulesList))
			{
				SectionsWithoutRules = SectionsWithoutRulesList.ToHashSet();
			}

			List<PakFileRules> RulesList = new List<PakFileRules>();
			foreach (string SectionName in PakRulesConfig.SectionNames)
			{
				// skip any sections marked to not be Rules, above
				if (SectionsWithoutRules.Contains(SectionName))
				{
					continue;
				}

				//LogInformation("Building PakFileRules for Section {0}", SectionName);

				bool bOnlyChunkedBuilds = false;
				if (!PakRulesConfig.TryGetValue(SectionName, "bOnlyChunkedBuilds", out bOnlyChunkedBuilds))
				{
					bOnlyChunkedBuilds = false;
				}

				bool bOnlyNonChunkedBuilds = false;
				if (!PakRulesConfig.TryGetValue(SectionName, "bOnlyNonChunkedBuilds", out bOnlyNonChunkedBuilds))
				{
					bOnlyNonChunkedBuilds = false;
				}

				if (bChunkedBuild && bOnlyNonChunkedBuilds)
				{
					continue;
				}

				if(!bChunkedBuild && bOnlyChunkedBuilds)
				{
					continue;
				}

				string PlatformString;
				if (PakRulesConfig.TryGetValue(SectionName, "Platforms", out PlatformString))
				{
					string[] PlatformStrings = PlatformString.Split(new char[] { ',', ' ' }, StringSplitOptions.RemoveEmptyEntries);
					bool bMatches = false;

					// Check platform string
					foreach (string Platform in PlatformStrings)
					{
						if (IniPlatformName.Equals(Platform, StringComparison.OrdinalIgnoreCase))
						{
							bMatches = true;
							break;
						}
					}

					if (!bMatches)
					{
						Logger.LogInformation("No matching platform for PakFileRules for Section {SectionName} : {IniPlatformName}", SectionName, IniPlatformName);
						continue;
					}
				}

				string TargetString;
				if (PakRulesConfig.TryGetValue(SectionName, "Targets", out TargetString))
				{
					string[] TargetStrings = TargetString.Split(new char[] { ',', ' ' }, StringSplitOptions.RemoveEmptyEntries);
					bool bMatches = false;

					// Check target string
					foreach (string Target in TargetStrings)
					{
						foreach (UnrealTargetConfiguration C in SC.StageTargetConfigurations)
						{
							if (C.ToString() == Target)
							{
								bMatches = true;
								break;
							}
						}
					}

					if (!bMatches)
					{
						continue;
					}
					else if (SC.StageTargetConfigurations.Count > 0 && !bWarnedAboutMultipleTargets)
					{
						bWarnedAboutMultipleTargets = true;
						Logger.LogInformation("Staging with more than one target, PakFileRules may apply too many rules!");
					}
				}

				PakFileRules PakRules = new PakFileRules();
				PakRules.Name = SectionName;
				PakRulesConfig.TryGetValue(SectionName, "bUseExactFilePathMatch", out PakRules.bUseExactFilePathMatch);
				PakRulesConfig.TryGetValue(SectionName, "bExcludeFromPaks", out PakRules.bExcludeFromPaks);
				PakRulesConfig.TryGetValue(SectionName, "bStageLoose", out PakRules.bStageLoose);
				PakRulesConfig.TryGetValue(SectionName, "bOverrideChunkManifest", out PakRules.bOverrideChunkManifest);
				PakRulesConfig.TryGetValue(SectionName, "bDisabled", out PakRules.bDisabled);
				PakRulesConfig.TryGetValue(SectionName, "bOnDemand", out PakRules.bOnDemand);
				PakRulesConfig.TryGetValue(SectionName, "EncryptionKeyGuid", out PakRules.EncryptionKeyGuid);
				string PakString;
				PakRulesConfig.TryGetValue(SectionName, "OverridePaks", out PakString);

				if (PakString != null && PakString.Length > 0)
				{
					PakRules.OverridePaks = PakString.Split(new char[] { ',', ' ' }, StringSplitOptions.RemoveEmptyEntries).ToList();
				}
				else
				{
					PakRules.OverridePaks = null;
				}

				if (PakRules.bExcludeFromPaks)
				{
					if (PakRules.OverridePaks != null)
					{
						Logger.LogWarning("Error in PakFileRules {Arg0}, set to exclude but also sets override!", PakRules.Name);
						continue;
					}
					if (PakRules.bOnDemand)
					{
						Logger.LogWarning("Error in PakFileRules {Arg0}, set to exclude but also sets to ondemand!", PakRules.Name);
						continue;
					}
				}
				else if (PakRules.OverridePaks == null && !PakRules.bOnDemand)
				{
					Logger.LogWarning("Error in PakFileRules {Arg0}, set to include but did not specify paks!", PakRules.Name);
					continue;
				}

				IReadOnlyList<string> AllowedChunkFilters;
				if (PakRulesConfig.TryGetValues(SectionName, "OnDemandAllowedChunkFilters", out AllowedChunkFilters))
				{
					PakRules.OnDemandAllowedChunkFilters = new List<Regex>();
					foreach (string Filter in AllowedChunkFilters)
					{
						try
						{
							PakRules.OnDemandAllowedChunkFilters.Add(new Regex(Filter));
						}
						catch(System.Text.RegularExpressions.RegexParseException Ex)
						{
							Logger.LogError("Failed to parse allowed chunk filter, reason '{Arg1}'", Ex.Message);
						}
					}
				}

				IReadOnlyList<string> FilesEnumberable;
				if (PakRulesConfig.TryGetValues(SectionName, "Files", out FilesEnumberable))
				{
					// Only add if we have actual files, we can end up with none due to config overriding
					PakRules.Filter = new FileFilter();
					PakRules.ExactFileMatches = new HashSet<string>();

					if (PakRules.bUseExactFilePathMatch)
					{
						PakRules.ExactFileMatches.UnionWith(FilesEnumberable);
					}
					else
					{
						PakRules.Filter.AddRules(FilesEnumberable);
					}

					RulesList.Add(PakRules);
				}
			}
			if (RulesList.Count == 0)
			{
				return null;
			}
			return RulesList;
		}

		/// <summary>
		/// Attempts to apply the pak file rules to a specific staging file, returns false if it should be excluded
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		private static PakFileRules? ApplyPakFileRules(
			List<PakFileRules> RulesList,
			KeyValuePair<string, string> StagingFile,
			HashSet<ChunkDefinition> ModifyPakList,
			ConcurrentDictionary<string, ChunkDefinition> ChunkNameToDefinition,
			out bool bExcludeFromPaks)
		{
			bExcludeFromPaks = false;

			if (RulesList == null)
			{
				return null;
			}

			// Search in order, return on first match
			foreach (var PakRules in RulesList)
			{
				if (PakFileRules.IsMatch(PakRules, StagingFile))
				{
					if (PakRules.bStageLoose)
					{
						return PakRules;
					}

					bool bOverrideChunkAssignment = false;
					if (ModifyPakList != null && ModifyPakList.Count > 0)
					{
						// Only override the existing list if bOverrideChunkManifest is set
						if (!PakRules.bOverrideChunkManifest)
						{
							return PakRules;
						}

						bOverrideChunkAssignment = true;
						if (PakRules.OverridePaks != null && !PakRules.bOnDemand)
						{
							Logger.LogInformation("Overridding chunk assignment {Arg0} to {Arg1}, this can cause broken references", StagingFile.Key, string.Join(", ", PakRules.OverridePaks));
						}
						else if (bExcludeFromPaks)
						{
							Logger.LogInformation("Removing {Arg0} from pak despite chunk assignment, this can cause broken references", StagingFile.Key);
						}
					}

					bExcludeFromPaks = PakRules.bExcludeFromPaks;
					if (PakRules.OverridePaks != null && ModifyPakList != null && ChunkNameToDefinition != null)
					{
						if (!bOverrideChunkAssignment)
						{
							Logger.LogInformation("Setting pak assignment for file {Arg0} to {Arg1}", StagingFile.Key, string.Join(", ", PakRules.OverridePaks));
						}

						ModifyPakList.Clear();
						ModifyPakList.UnionWith(PakRules.OverridePaks.Select(OverrideChunkName =>
						{
							if (!ChunkNameToDefinition.ContainsKey(OverrideChunkName))
							{
								ChunkNameToDefinition.TryAdd(OverrideChunkName, new ChunkDefinition(OverrideChunkName));
							}

							return ChunkNameToDefinition[OverrideChunkName];
						}));
					}
					else if (bExcludeFromPaks)
					{
						if (!bOverrideChunkAssignment)
						{
							Logger.LogInformation("Excluding {Arg0} from pak files", StagingFile.Key);
						}
					}

					return PakRules;
				}
			}

			return null;
		}

		/// <summary>
		/// Creates a pak file using staging context (single manifest)
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		private static void CreatePakUsingStagingManifest(ProjectParams Params, DeploymentContext SC)
		{
			Logger.LogInformation("Creating pak using staging manifest.");

			DumpManifest(SC, CombinePaths(CmdEnv.LogFolder, "PrePak" + (SC.DedicatedServer ? "_Server" : "") + "_" + SC.CookPlatform));

			var UnrealPakResponseFile = CreatePakResponseFileFromStagingManifest(SC, SC.FilesToStage.UFSFiles);

			List<PakFileRules> PakRulesList = GetPakFileRules(Params, SC);

			ConcurrentBag<string> FilesToRemove = new ConcurrentBag<string>();

			// Apply the pak file rules, this can remove things but will not override the pak file name
			Parallel.ForEach(UnrealPakResponseFile, StagingFile =>
			{
				bool bExcludeFromPaks = false;
				ApplyPakFileRules(PakRulesList, StagingFile, null, null, out bExcludeFromPaks);

				if (bExcludeFromPaks)
				{
					FilesToRemove.Add(StagingFile.Key);
				}
			});

			foreach (var FileToRemove in FilesToRemove.ToArray())
			{
				UnrealPakResponseFile.Remove(FileToRemove);
			}

			EncryptionAndSigning.CryptoSettings PakCryptoSettings = EncryptionAndSigning.ParseCryptoSettings(DirectoryReference.FromFile(Params.RawProjectPath), SC.StageTargetPlatform.IniPlatformType, Log.Logger);
			FileReference CryptoKeysCacheFilename = FileReference.Combine(SC.MetadataDir, "Crypto.json");
			PakCryptoSettings.Save(CryptoKeysCacheFilename);

			List<CreatePakParams> PakInputs = new List<CreatePakParams>();
			PakInputs.Add(new CreatePakParams(SC.ShortProjectName, UnrealPakResponseFile, Params.Compressed, Params.RehydrateAssets, null));
			CreatePaks(Params, SC, PakInputs, PakCryptoSettings, CryptoKeysCacheFilename);
		}

		/// <summary>
		/// Creates a standalone pak file for crash reporter
		/// </summary>
		/// <param name="Params">The packaging parameters</param>
		/// <param name="SC">Staging context</param>
		private static void CreatePakForCrashReporter(ProjectParams Params, DeploymentContext SC)
		{
			Logger.LogInformation("Creating pak for crash reporter.");

			Dictionary<string, string> PakResponseFile = CreatePakResponseFileFromStagingManifest(SC, SC.CrashReporterUFSFiles);
			FileReference OutputLocation = FileReference.Combine(SC.RuntimeRootDir, "Engine", "Programs", "CrashReportClient", "Content", "Paks", "CrashReportClient.pak");

			RunUnrealPak(Params, PakResponseFile, OutputLocation, null, null, Params.Compressed, null, null, null, null, null);
		}

		/// <summary>
		/// Creates a pak response file using stage context
		/// </summary>
		/// <param name="SC"></param>
		/// <returns></returns>
		private static Dictionary<string, string> CreatePakResponseFileFromStagingManifest(DeploymentContext SC, Dictionary<StagedFileReference, FileReference> FilesToStage)
		{
			// look for optional packaging deny list if only one config active
			List<string> DenyList = null;
			if (SC.StageTargetConfigurations.Count == 1)
			{
				FileReference PakDenyListFilename = FileReference.Combine(SC.ProjectRoot, "Platforms", SC.PlatformDir, "Build", string.Format("PakDenylist-{0}.txt", SC.StageTargetConfigurations[0].ToString()));
				if (!FileReference.Exists(PakDenyListFilename))
				{
					PakDenyListFilename = FileReference.Combine(SC.ProjectRoot, "Build", SC.PlatformDir, string.Format("PakDenylist-{0}.txt", SC.StageTargetConfigurations[0].ToString()));
				}
				if (FileReference.Exists(PakDenyListFilename))
				{
					Logger.LogInformation("Applying PAK deny list file {PakDenyListFilename}. This is deprecated in favor of DefaultPakFileRules.ini", PakDenyListFilename);
					string[] DenyListContents = FileReference.ReadAllLines(PakDenyListFilename);
					foreach (string Candidate in DenyListContents)
					{
						if (Candidate.Trim().Length > 0)
						{
							if (DenyList == null)
							{
								DenyList = new List<string>();
							}
							DenyList.Add(Candidate);
						}
					}
				}
			}

			var UnrealPakResponseFile = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
			foreach (KeyValuePair<StagedFileReference, FileReference> Pair in FilesToStage)
			{
				FileReference Src = Pair.Value;
				string Dest = Pair.Key.Name;

				Dest = CombinePaths(PathSeparator.Slash, SC.PakFileInternalRoot, Dest);

				if (DenyList != null)
				{
					bool bExcludeFile = false;
					foreach (string ExcludePath in DenyList)
					{
						if (Dest.StartsWith(ExcludePath))
						{
							bExcludeFile = true;
							break;
						}
					}

					if (bExcludeFile)
					{
						Logger.LogInformation("Excluding {Src}", Src);
						continue;
					}
				}

				// Filter I/O store container files
				if (Src.HasExtension(".ucas") || Src.HasExtension(".utoc"))
				{
					Logger.LogInformation("Excluding {Src}", Src);
					continue;
				}

				// Do a filtered copy of all ini files to allow stripping of values that we don't want to distribute
				if (Src.HasExtension(".ini"))
				{
					string SubFolder = Pair.Key.Name.Replace('/', Path.DirectorySeparatorChar);
					FileReference NewIniFilename = FileReference.Combine(SC.ProjectRoot, "Saved", "Temp", SC.PlatformDir, SubFolder);
					InternalUtils.SafeCreateDirectory(NewIniFilename.Directory.FullName, true);
					InternalUtils.SafeCopyFile(Src.FullName, NewIniFilename.FullName, IniKeyDenyList: SC.IniKeyDenyList, IniSectionDenyList: SC.IniSectionDenyList);
					Src = NewIniFilename;
				}

				// Allow the custom stage copy handler to copy the file.
				if (SC.CustomStageCopyHandler != null && SC.CustomStageCopyHandler.CanCopyFile(Src.FullName))
				{
					string SubFolder = Pair.Key.Name.Replace('/', Path.DirectorySeparatorChar);
					FileReference NewFilename = FileReference.Combine(SC.ProjectRoot, "Saved", "Temp", SC.PlatformDir, SubFolder);
					InternalUtils.SafeCreateDirectory(NewFilename.Directory.FullName, true);

					if (SC.CustomStageCopyHandler.StageFile(Logger, Src.FullName, NewFilename.FullName))
					{
						Src = NewFilename;
					}
				}

				// there can be files that only differ in case only, we don't support that in paks as paks are case-insensitive
				if (UnrealPakResponseFile.ContainsKey(Src.FullName))
				{
					if (UnrealPakResponseFile[Src.FullName] != Dest)
					{
						throw new AutomationException("Staging manifest already contains {0} (or a file that differs in case only)", Src);
					}
					Logger.LogWarning("Tried to add duplicate file to stage " + Src + " ignoring second attempt pls fix");
					continue;
				}

				UnrealPakResponseFile.Add(Src.FullName, Dest);
			}

			return UnrealPakResponseFile;
		}

		/// <summary>
		/// Parameter class for CreatePaks(). Each instance stores information about a pak file to be created.
		/// </summary>
		[DebuggerDisplay("PakName")]
		class CreatePakParams
		{
			/// <summary>
			/// Path to the base output file for this pak file 
			/// </summary>
			public string PakName;

			/// <summary>
			/// Map of files within the pak file to their source file on disk
			/// </summary>
			public Dictionary<string, string> UnrealPakResponseFile;

			/// <summary>
			/// Whether to enable compression
			/// </summary>
			public bool bCompressed;

			/// <summary>
			/// Whether to rehydrate the assets when creating the pak file or not
			/// </summary>
			public bool bRehydrateAssets;

			/// <summary>
			/// Allow the per-chunk bCompressed flag to be used instead of the global parameter
			/// </summary>
			public bool bAllowPerChunkCompression;

			/// <summary>
			/// Whether this pak file is for content-on-demand content delivery or not
			/// </summary>
			public bool bOnDemand;

			/// <summary>
			/// Whether this pak file is to copy the source files as loose files
			/// </summary>
			public bool bStageLoose;

			/// <summary>
			/// GUID of the encryption key for this pak file
			/// </summary>
			public string EncryptionKeyGuid;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="PakName">Path to the base output file for this pak file</param>
			/// <param name="UnrealPakResponseFile">Map of files within the pak file to their source file on disk</param>
			/// <param name="bCompressed">Whether to enable compression</param>
			public CreatePakParams(
				string PakName, 
				Dictionary<string, string> UnrealPakResponseFile, 
				bool bCompressed, 
				bool RehydrateAssets, 
				string EncryptionKeyGuid, 
				bool bOnDemand = false, 
				bool bStageLoose = false,
				bool bAllowPerChunkCompression = false)
			{
				this.PakName = PakName;
				this.UnrealPakResponseFile = UnrealPakResponseFile;
				this.bCompressed = bCompressed;
				this.bRehydrateAssets = RehydrateAssets;
				this.EncryptionKeyGuid = EncryptionKeyGuid;
				this.bOnDemand = bOnDemand;
				this.bStageLoose = bStageLoose;
				this.bAllowPerChunkCompression = bAllowPerChunkCompression;
			}
		}

		private static bool ShouldSkipGeneratingPatch(ConfigHierarchy PlatformGameConfig, string Filename)
		{
			bool bIsEarlyDownloaderPakFile = false;
			string EarlyDownloaderPakFilePrefix = "";
			if (PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "EarlyDownloaderPakFilePrefix", out EarlyDownloaderPakFilePrefix))
			{
				bIsEarlyDownloaderPakFile = String.Equals(EarlyDownloaderPakFilePrefix, Filename, StringComparison.OrdinalIgnoreCase);
			}

			return bIsEarlyDownloaderPakFile;
		}

		private static string GetPakFilePostFix(bool bShouldGeneratePatch, bool bHasIterateSharedCookedBuild, ConfigHierarchy PlatformGameConfig, string Filename)
		{
			if (ShouldSkipGeneratingPatch(PlatformGameConfig, Filename))
			{
				return String.Empty;
			}
			else if(bShouldGeneratePatch)
			{
				return "_P";
			}
			else if(bHasIterateSharedCookedBuild)
			{
				// shared cooked builds will produce a patch
				// then be combined with the shared cooked build
				return "_S_P";
			}

			return String.Empty;
		}

		private static bool CopyExistingContainer(FileReference InUtocFile, FileReference OutputLocation)
		{
			bool bCopiedExistingPak = true;
			FileReference InUcasFile = InUtocFile.ChangeExtension(".ucas");
			if (FileReference.Exists(InUcasFile))
			{
				FileReference OutUcasFile = OutputLocation.ChangeExtension(".ucas");
				Logger.LogInformation("Copying ucas from {InUcasFile} to {OutUcasFile}", InUcasFile, OutUcasFile);
				if (!InternalUtils.SafeCopyFile(InUcasFile.FullName, OutUcasFile.FullName))
				{
					Logger.LogInformation("Failed to copy ucas {InUcasFile} to {OutUcasFile}, creating new pak", InUcasFile, OutUcasFile);
					bCopiedExistingPak = false;
				}
			}
			else
			{
				Logger.LogInformation("Missing ucas file {InUcasFile}, creating new pak", InUcasFile);
				bCopiedExistingPak = false;
			}

			if (FileReference.Exists(InUtocFile))
			{
				FileReference OutUtocFile = OutputLocation.ChangeExtension(".utoc");
				Logger.LogInformation("Copying utoc from {InUtocFile} to {OutUtocFile}", InUtocFile, OutUtocFile);
				if (!InternalUtils.SafeCopyFile(InUtocFile.FullName, OutUtocFile.FullName))
				{
					Logger.LogInformation("Failed to copy utoc {InUtocFile} to {OutUtocFile}, creating new pak", InUtocFile, OutUtocFile);
					bCopiedExistingPak = false;
				}
			}
			else
			{
				Logger.LogInformation("Missing utoc file {InUtocFile}, creating new pak", InUtocFile);
				bCopiedExistingPak = false;
			}
			return bCopiedExistingPak;
		}

		/// <summary>
		/// Creates a pak file using response file.
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		/// <param name="UnrealPakResponseFile"></param>
		/// <param name="PakName"></param>
		private static void CreatePaks(ProjectParams Params, DeploymentContext SC, List<CreatePakParams> PakParamsList, EncryptionAndSigning.CryptoSettings CryptoSettings, FileReference CryptoKeysCacheFilename)
		{
			bool? bCustomDeploymentNeedsDiffPak = SC.CustomDeployment?.GetPlatformPatchesWithDiffPak(Params, SC);
			bool bShouldCreateIoStoreContainerFiles = ShouldCreateIoStoreContainerFiles(Params, SC);
			bool bShouldGeneratePatch = Params.IsGeneratingPatch && (bCustomDeploymentNeedsDiffPak ?? SC.StageTargetPlatform.GetPlatformPatchesWithDiffPak(Params, SC));

			if (bShouldGeneratePatch && !Params.HasBasedOnReleaseVersion)
			{
				Logger.LogInformation("Generating patch required a based on release version flag");
			}

			const string OutputFilenameExtension = ".pak";

			// read some compression settings from the project (once, shared across all pak commands)

			ConfigHierarchy PlatformGameConfig = ConfigCache.ReadHierarchy(
					ConfigHierarchyType.Game,
					DirectoryReference.FromFile(Params.RawProjectPath),
					SC.StageTargetPlatform.IniPlatformType,
					SC.CustomConfig,
					Params.ConfigOverrideParams.ToArray());

			// in standard runs, ProjectPackagingSettings/bCompressed is read by the program invoking this script
			//	and used to pass "-compressed" on the command line
			// so typically "-compressed" == ProjectPackagingSettings/bCompressed
			// if ProjectPackagingSettings/bCompressed is true, but "-compressed" was not passed, we now turn it on
			// if ProjectPackagingSettings/bCompressed is true but you really don't want compression, use "-forceuncompressed"
			bool bCompressed_ProjectPackagingSettings = false;
			PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bCompressed", out bCompressed_ProjectPackagingSettings);

			bool bForceUseProjectCompressionFormatIgnoreHardwareOverride = false;// do we want to override HW compression with project? if so, read it below
			PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bForceUseProjectCompressionFormatIgnoreHardwareOverride", out bForceUseProjectCompressionFormatIgnoreHardwareOverride);

			string HardwareCompressionFormat = null;
			if ( ! bForceUseProjectCompressionFormatIgnoreHardwareOverride )
			{
				DataDrivenPlatformInfo.ConfigDataDrivenPlatformInfo DDPI = DataDrivenPlatformInfo.GetDataDrivenInfoForPlatform(SC.StageTargetPlatform.IniPlatformType);
				if (DDPI != null)
				{
					HardwareCompressionFormat = DDPI.HardwareCompressionFormat;
				}
			}

			// get the compression format from platform or the project
			string CompressionFormats;
			if (!string.IsNullOrEmpty(HardwareCompressionFormat))
			{
				Logger.LogInformation("Overriding to HardwareCompressionFormat = {HardwareCompressionFormat}", HardwareCompressionFormat);
				CompressionFormats = HardwareCompressionFormat;
			}
			else
			{
				PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionFormat", out CompressionFormats);
				// empty CompressionFormats string means "use default" eg. zlib
			}

			if (Params.ForceUncompressed)
			{
				//-ForceUncompressed on the command line means really don't do it even if the ini or DDPI say to do it
				Params.Compressed = false;
				bCompressed_ProjectPackagingSettings = false;
			}

			bool bIsCompressionFormatNone = (CompressionFormats == "None" || CompressionFormats == "none");

			if (Params.ForceCompressed)
			{
				Params.Compressed = true;
				
				// If we are forcing compression make sure we have a valid compression format
				if (bIsCompressionFormatNone)
				{
					PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionFormat", out CompressionFormats);
					// empty CompressionFormats string means "use default" eg. zlib;
				}	
			}
			else if (bIsCompressionFormatNone)
			{
				// rather than pass "-compressed" + format=None through to UnrealPak/iostore
				//	just turn off compression
				if (Params.Compressed)
				{
					//LogInformation(" -compressed was on command line but override to None, turning OFF!");
					Params.Compressed = false;
				}
			}
			else if (!Params.Compressed && bCompressed_ProjectPackagingSettings)
			{
				//LogInformation(" -compressed was not on command line but was in ini, turning on!");
				Params.Compressed = true;
			}

			// Params.Compressed is done being changed; push it to the PakParams list :
			// note this is a change of behavior; PakParams.bCompressed could have been true from a Chunk Manifest
			//	even though compression was otherwise turned off
			// we now overwrite that with the overall compression setting unless specifically flagged for per-chunk compression
			//	this should always be an improvement in behavior
			bool bAnyCompressed = Params.Compressed;
			foreach (CreatePakParams PakParams in PakParamsList)
			{
				if (!PakParams.bAllowPerChunkCompression) // Use values from chunk manifest only if allowed
				{
					PakParams.bCompressed = Params.Compressed;
				}
				bAnyCompressed = bAnyCompressed || PakParams.bCompressed;
			}

			if (bAnyCompressed && !string.IsNullOrWhiteSpace(CompressionFormats))
			{
				CompressionFormats = " -compressionformats=" + CompressionFormats;
			}
			else
			{
				CompressionFormats = "";
			}

			// Always pass -compresslevel because we use it to inform the compression level of
			// shaders, which are always compressed even if the containers aren't compressed.
			int CompressionLevel = 0;
			// GetInt32 fills out with = 0 if not found
			if (Params.Distribution)
			{
				PlatformGameConfig.GetInt32("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionLevel_Distribution", out CompressionLevel);
			}
			else if (SC.StageTargetConfigurations.Any(Config => Config == UnrealTargetConfiguration.Test || Config == UnrealTargetConfiguration.Shipping))
			{
				PlatformGameConfig.GetInt32("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionLevel_TestShipping", out CompressionLevel);
			}
			else
			{
				PlatformGameConfig.GetInt32("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionLevel_DebugDevelopment", out CompressionLevel);
			}

			if (CompressionLevel != 0)
			{
				CompressionFormats += " -compresslevel=" + CompressionLevel;
			}

			string AdditionalCompressionOptionsOnCommandLine = "";
			if (bAnyCompressed)
			{
				// the game may want to control compression settings, but since it may be in a plugin that checks the commandline for the settings, we need to pass
				// the settings directly on the UnrealPak commandline, and not put it into the batch file lines (plugins can't get the unrealpak command list, and
				// there's not a great way to communicate random strings down into the plugins during plugin init time)
				PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PackageAdditionalCompressionOptions", out AdditionalCompressionOptionsOnCommandLine);

				string CompressionMethod;
				PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionMethod", out CompressionMethod);
				if (!string.IsNullOrWhiteSpace(CompressionMethod))
				{
					CompressionFormats += " -compressmethod=" + CompressionMethod;
				}


				// unlike the other config, the DLL should be fetched from the build platform not the target platform
				// it's the DLL to use during packaging, not during runtime, so it needs to be for the platform the packager runs on, not the platform you're packaging for.
				// Most of the compression options come from the platform you're packaging FOR

				ConfigHierarchy BuildPlatformEngineConfig = ConfigCache.ReadHierarchy(
					ConfigHierarchyType.Engine,
					DirectoryReference.FromFile(Params.RawProjectPath),
					BuildHostPlatform.Current.Platform,
					SC.CustomConfig,
					Params.ConfigOverrideParams.ToArray());

				string OodleCompressDLL;
				BuildPlatformEngineConfig.GetString("OodleDataCompressionFormat", "OodleCompressDLL", out OodleCompressDLL);
				if (!string.IsNullOrWhiteSpace(Params.ForceOodleDllVersion))
				{
					if (string.Equals(Params.ForceOodleDllVersion, "latest", StringComparison.OrdinalIgnoreCase))
					{
						OodleCompressDLL = "";
					}
					else
					{
						OodleCompressDLL = Params.ForceOodleDllVersion;
					}
				}
				if (!string.IsNullOrWhiteSpace(OodleCompressDLL))
				{
					CompressionFormats += " -OodleCompressDLL=" + OodleCompressDLL;
				}
			}

			Func<string, string> GetPostFix = PakFilename => GetPakFilePostFix(bShouldGeneratePatch, Params.HasIterateSharedCookedBuild, PlatformGameConfig, PakFilename);

			List<string> PakCommands = new List<string>();
			List<string> LogNames = new List<string>();
			List<string> IoStoreCommands = new List<string>();

			// Calculate target patch index by iterating all pak files in source build
			int TargetPatchIndex = 0;
			int NumPakFiles = 0;
			if (bShouldGeneratePatch)
			{
				foreach (CreatePakParams PakParams in PakParamsList)
				{
					if(ShouldSkipGeneratingPatch(PlatformGameConfig, PakParams.PakName))
					{
						continue;
					}

					string OutputFilename = PakParams.PakName + "-" + SC.FinalCookPlatform;
					string ExistingPatchSearchPath = SC.StageTargetPlatform.GetReleasePakFilePath(SC, Params, null);
					if (Directory.Exists(ExistingPatchSearchPath))
					{
						string PostFix = GetPostFix(PakParams.PakName);
						IEnumerable<string> PakFileSet = Directory.EnumerateFiles(ExistingPatchSearchPath, OutputFilename + "*" + PostFix + OutputFilenameExtension);
						NumPakFiles += PakFileSet.Count();

						foreach (string PakFilePath in PakFileSet)
						{
							string PakFileName = Path.GetFileName(PakFilePath);
							int StartIndex = OutputFilename.Length + 1;
							int LengthVar = PakFileName.Length - (OutputFilename.Length + 1 + PostFix.Length + OutputFilenameExtension.Length);
							if (LengthVar > 0)
							{
								string PakFileIndex = PakFileName.Substring(StartIndex, LengthVar);
								int ChunkIndex;
								if (int.TryParse(PakFileIndex, out ChunkIndex))
								{
									if (ChunkIndex > TargetPatchIndex)
									{
										TargetPatchIndex = ChunkIndex;
									}
								}
							}
						}
					}
				}

				if (Params.ShouldAddPatchLevel && NumPakFiles > 0)
				{
					TargetPatchIndex++;
				}
			}

			ConfigHierarchy PlatformEngineConfig;
			bool bHasPlatformEngineConfig = false;
			bHasPlatformEngineConfig = Params.EngineConfigs.TryGetValue(SC.StageTargetPlatform.PlatformType, out PlatformEngineConfig);
			string BulkOption = "";
			if (bHasPlatformEngineConfig)
			{
				bool bEnabled = false;
				PlatformEngineConfig.GetBool("MemoryMappedFiles", "Enable", out bEnabled);
				if (bEnabled)
				{
					int Value = 0;
					PlatformEngineConfig.GetInt32("MemoryMappedFiles", "Alignment", out Value);
					if (Value > 0)
					{
						BulkOption = String.Format(" -AlignForMemoryMapping={0}", Value);
					}
				}
			}

			string PatchOptions = String.Empty;
			if (bShouldGeneratePatch)
			{
				string PatchSeekOptMode = String.Empty;
				string PatchSeekOptMaxGapSize = String.Empty;
				string PatchSeekOptMaxInflationPercent = String.Empty;
				string PatchSeekOptMaxAdjacentOrderDiff = String.Empty;
				if (PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PatchSeekOptMode", out PatchSeekOptMode))
				{
					PatchOptions += String.Format(" -PatchSeekOptMode={0}", PatchSeekOptMode);
				}
				if (PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PatchSeekOptMaxGapSize", out PatchSeekOptMaxGapSize))
				{
					PatchOptions += String.Format(" -PatchSeekOptMaxGapSize={0}", PatchSeekOptMaxGapSize);
				}
				if (PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PatchSeekOptMaxInflationPercent", out PatchSeekOptMaxInflationPercent))
				{
					PatchOptions += String.Format(" -PatchSeekOptMaxInflationPercent={0}", PatchSeekOptMaxInflationPercent);
				}
				if (PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "PatchSeekOptMaxAdjacentOrderDiff", out PatchSeekOptMaxAdjacentOrderDiff))
				{
					PatchOptions += String.Format(" -PatchSeekOptMaxAdjacentOrderDiff={0}", PatchSeekOptMaxAdjacentOrderDiff);
				}
			}

			// Find primary and secondary order files if they exist
			List<FileReference> PakOrderFileLocations = new List<FileReference>();

			List<OrderFileSpec> OrderFileSpecs = new List<OrderFileSpec>();
			if (PlatformGameConfig.TryGetValues("/Script/UnrealEd.ProjectPackagingSettings", "PakOrderFileSpecs", out IReadOnlyList<string> OrderFileConfigValues))
			{
				foreach (string Config in OrderFileConfigValues)
				{
					Dictionary<string, string> Fields;
					if (!ConfigHierarchy.TryParse(Config, out Fields))
					{
						throw new AutomationException(String.Format("Failed to parse pak order file config \"{0}\"", Config));
					}

					string FilePattern;
					if (!Fields.TryGetValue("Pattern", out FilePattern))
					{
						throw new AutomationException(String.Format("Missing field 'Pattern' in pak order file config \"{0}\"", Config));
					}

					// Priority missing is fine, we default to 0
					int Priority = 0;
					if( Fields.TryGetValue("Priority", out string PriorityString)) 
					{
						if (!int.TryParse(PriorityString, out Priority))
						{
							throw new AutomationException(String.Format("Failed to parse int from field 'Priority' in pak order file config \"{0}\"", Config));
						}
					}

					OrderFileSpecs.Add(new OrderFileSpec { FileNamePattern = FilePattern, OrderType = OrderFile.OrderFileType.Custom, Required = FilePattern.Contains("*") == false, Priority = Priority });
				}
			}

			if(OrderFileSpecs.Count == 0)
			{
				// Default filespecs
				OrderFileSpecs.AddRange(new OrderFileSpec[] {
					new OrderFileSpec{ FileNamePattern = "GameOpenOrder*.log", OrderType = OrderFile.OrderFileType.Game, Required=false, Priority = 0 },
					new OrderFileSpec{ FileNamePattern = "CookerOpenOrder*.log", OrderType = OrderFile.OrderFileType.Cooker, Required=false, Priority = 0 },
					new OrderFileSpec{ FileNamePattern = "EditorOpenOrder.log", OrderType = OrderFile.OrderFileType.Editor, Required=false, Priority = 0 }
				});
			}

			// search CookPlaform (e.g. IOSClient and then regular platform (e.g. IOS).
			string[] OrderPlatformNames = new string[] { SC.FinalCookPlatform, SC.StageTargetPlatform.GetTargetPlatformDescriptor().Type.ToString() };

			List<DirectoryReference> OrderPathsToSearch = new List<DirectoryReference>();
			OrderPathsToSearch.Add(DirectoryReference.Combine(SC.ProjectRoot, "Build", "FileOpenOrder")); // Search common directory first
			foreach (string OrderLocation in OrderPlatformNames)
			{
				DirectoryReference PakOrderFileLocationBase = DirectoryReference.Combine(SC.ProjectRoot, "Platforms", OrderLocation, "Build", "FileOpenOrder");
				if (!DirectoryReference.Exists(PakOrderFileLocationBase))
				{
					PakOrderFileLocationBase = DirectoryReference.Combine(SC.ProjectRoot, "Build", OrderLocation, "FileOpenOrder");
				}

				OrderPathsToSearch.Add(PakOrderFileLocationBase);
			}

			List<OrderFile> OrderFiles = new List<OrderFile>();

			for (int OrderFileIndex = 0; OrderFileIndex < OrderFileSpecs.Count; OrderFileIndex++)
			{
				OrderFileSpec Spec = OrderFileSpecs[OrderFileIndex];
				bool bAnyFound = false;
				foreach (DirectoryReference BaseDir in OrderPathsToSearch)
				{
					// Add input file to control order of file within the pak
					FileReference[] FileLocations = CommandUtils.FindFiles(Spec.FileNamePattern, false, BaseDir);

					if (FileLocations != null)
					{
						foreach (var File in FileLocations)
						{
							bAnyFound = true;
							OrderFile FileOrder = new OrderFile(File, Spec.OrderType, OrderFileIndex, Spec.Priority);

							//check if the file is alreay in the list
							if (!OrderFiles.Any(o => o.File.FullName.Equals(File.FullName)))
							{
								OrderFiles.Add(FileOrder);
							}
						}
					}
				}
				if (!bAnyFound && Spec.Required)
				{
					throw new AutomationException(String.Format("Did not find any order files from rule {0}", Spec.FileNamePattern));
				}
			}

			OrderFiles.Sort();

			//Only allow Editor open order as a primary input
			if (OrderFiles.Count() > 1 && OrderFiles[0].OrderType != OrderFile.OrderFileType.Editor)
			{
				OrderFiles.RemoveAll(x => x.OrderType == OrderFile.OrderFileType.Editor);
			}

			// Check second order 
			bool bUseSecondaryOrder = false;
			PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bPakUsesSecondaryOrder", out bUseSecondaryOrder);

			if (bShouldCreateIoStoreContainerFiles)
			{
				bUseSecondaryOrder = false;
			}

			if (!bUseSecondaryOrder)
			{
				OrderFiles.RemoveAll(x => (x.OrderType == OrderFile.OrderFileType.Cooker && x.AppendOrder == Int32.MaxValue));

				// chekc if theres a merging cookopenorder file
				bUseSecondaryOrder = OrderFiles.Any(x => x.OrderType == OrderFile.OrderFileType.Cooker);
			}

			Logger.LogInformation("Using {Arg0} pak order files:", OrderFiles.Count);
			foreach( OrderFile File in OrderFiles )
			{
				Logger.LogInformation("    {Arg0} Priority {Arg1}", File.File.ToString(), File.Priority);
			}

			List<OrderFile> PrimaryOrderFiles = OrderFiles.FindAll(x => (x.OrderType != OrderFile.OrderFileType.Cooker));
			List<OrderFile> SecondaryOrderFiles = null;

			// Add a secondary order if there is one specified
			if (bUseSecondaryOrder && OrderFiles.Count >= 1)
			{
				SecondaryOrderFiles = OrderFiles.FindAll(x => x.OrderType == OrderFile.OrderFileType.Cooker);
			}

			string PlatformPakCmdLine = "";
			if (SC.CustomDeployment == null || !SC.CustomDeployment.GetPlatformPakCommandLine(Params, SC, ref PlatformPakCmdLine))
			{
				PlatformPakCmdLine += SC.StageTargetPlatform.GetPlatformPakCommandLine(Params, SC);
			}
			string CommonAdditionalArgs = PlatformPakCmdLine + BulkOption + PatchOptions + CompressionFormats + " " + Params.AdditionalPakOptions;

			bool bPakFallbackOrderForNonUassetFiles = false;
			PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bPakFallbackOrderForNonUassetFiles", out bPakFallbackOrderForNonUassetFiles);
			if (bPakFallbackOrderForNonUassetFiles)
			{
				CommonAdditionalArgs += " -fallbackOrderForNonUassetFiles";
			}

			if (bHasPlatformEngineConfig)
			{
				// if the runtime will want to reduce memory usage, we have to disable the pak index freezing
				bool bUnloadPakEntries = false;
				bool bShrinkPakEntries = false;
				PlatformEngineConfig.GetBool("Pak", "UnloadPakEntryFilenamesIfPossible", out bUnloadPakEntries);
				PlatformEngineConfig.GetBool("Pak", "ShrinkPakEntriesMemoryUsage", out bShrinkPakEntries);
				if (bUnloadPakEntries || bShrinkPakEntries)
				{
					CommonAdditionalArgs += " -allowForIndexUnload";
				}
			}

			// pass the targetplatform so the index may be able to be frozen
			CommonAdditionalArgs += " -platform=" + ConfigHierarchy.GetIniPlatformName(SC.StageTargetPlatform.IniPlatformType);

			CommonAdditionalArgs += " " + AdditionalCompressionOptionsOnCommandLine;

			bool bCopiedExistingGlobalUtoc = false;
			List<Tuple<FileReference, StagedFileReference, string>> Outputs = new List<Tuple<FileReference, StagedFileReference, string>>();
			foreach (CreatePakParams PakParams in PakParamsList)
			{
				string OutputFilename = PakParams.PakName + "-" + SC.FinalCookPlatform;
				string PostFix = GetPostFix(PakParams.PakName);
				if (bShouldGeneratePatch && !ShouldSkipGeneratingPatch(PlatformGameConfig, PakParams.PakName))
				{
					OutputFilename = OutputFilename + "_" + TargetPatchIndex;
				}
				OutputFilename = OutputFilename + PostFix;

				StagedFileReference OutputRelativeLocation;
				if (Params.HasDLCName)
				{
					if (Params.DLCOverrideStagedSubDir != null)
					{
						OutputRelativeLocation = StagedFileReference.Combine(SC.RelativeProjectRootForStage, Params.DLCOverrideStagedSubDir, "Content", "Paks", Params.DLCFile.GetFileNameWithoutExtension() + OutputFilename + ".pak");
					}
					else
					{
						string PluginSubdirectory;
						if (Params.DLCFile.Directory.IsUnderDirectory(SC.ProjectRoot))
						{
							PluginSubdirectory = Params.DLCFile.Directory.MakeRelativeTo(SC.ProjectRoot);
						}
						else
						{
							// if the plugin is external, the MakeRelativeTo above will fail, so just use the plugin name as the directory
							PluginSubdirectory = Params.DLCFile.GetFileNameWithoutAnyExtensions();
						}
						OutputRelativeLocation = StagedFileReference.Combine(SC.RelativeProjectRootForStage, PluginSubdirectory, "Content", "Paks", SC.FinalCookPlatform, Params.DLCFile.GetFileNameWithoutExtension() + OutputFilename + ".pak");
					}
				}
				else
				{
					OutputRelativeLocation = StagedFileReference.Combine(SC.RelativeProjectRootForStage, "Content", "Paks", OutputFilename + OutputFilenameExtension);
				}
				if (SC.StageTargetPlatform.DeployLowerCaseFilenames(StagedFileType.UFS))
				{
					OutputRelativeLocation = OutputRelativeLocation.ToLowerInvariant();
				}
				OutputRelativeLocation = SC.StageTargetPlatform.Remap(OutputRelativeLocation);
				
				FileReference OutputLocation = FileReference.Combine(SC.RuntimeRootDir, OutputRelativeLocation.Name);
				bool bCopiedExistingPak = false;

				if (!PakParams.bOnDemand && !PakParams.bStageLoose && SC.StageTargetPlatform != SC.CookSourcePlatform && !Params.IgnorePaksFromDifferentCookSource)
				{
					// Check to see if we have an existing pak file we can use

					StagedFileReference SourceOutputRelativeLocation = StagedFileReference.Combine(SC.RelativeProjectRootForStage, "Content/Paks/", PakParams.PakName + "-" + SC.CookPlatform + PostFix + ".pak");
					if (SC.CookSourcePlatform.DeployLowerCaseFilenames(StagedFileType.UFS))
					{
						SourceOutputRelativeLocation = SourceOutputRelativeLocation.ToLowerInvariant();
					}
					SourceOutputRelativeLocation = SC.CookSourcePlatform.Remap(SourceOutputRelativeLocation);

					FileReference SourceOutputLocation = FileReference.Combine(SC.CookSourceRuntimeRootDir, SourceOutputRelativeLocation.Name);
					if (FileExists_NoExceptions(SourceOutputLocation.FullName))
					{
						InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(OutputLocation.FullName), true);

						if (InternalUtils.SafeCopyFile(SourceOutputLocation.FullName, OutputLocation.FullName))
						{
							Logger.LogInformation("Copying source pak from {SourceOutputLocation} to {OutputLocation} instead of creating new pak", SourceOutputLocation, OutputLocation);
							bCopiedExistingPak = true;

							FileReference InSigFile = SourceOutputLocation.ChangeExtension(".sig");
							if (FileReference.Exists(InSigFile))
							{
								FileReference OutSigFile = OutputLocation.ChangeExtension(".sig");

								Logger.LogInformation("Copying pak sig from {InSigFile} to {OutSigFile}", InSigFile, OutSigFile);

								if (!InternalUtils.SafeCopyFile(InSigFile.FullName, OutSigFile.FullName))
								{
									Logger.LogInformation("Failed to copy pak sig {InSigFile} to {InSigFile2}, creating new pak", InSigFile, InSigFile);
									bCopiedExistingPak = false;
								}
							}

							FileReference InUtocFile = SourceOutputLocation.ChangeExtension(".utoc");
							if (FileExists_NoExceptions(InUtocFile.FullName))
							{
								if (!CopyExistingContainer(InUtocFile, OutputLocation))
								{
									bCopiedExistingPak = false;
								}
							}
						}
					}
					if (!bCopiedExistingPak)
					{
						Logger.LogInformation("Failed to copy source pak from {SourceOutputLocation} to {OutputLocation}, creating new pak", SourceOutputLocation, OutputLocation);
					}

					if (bCopiedExistingPak && !bCopiedExistingGlobalUtoc)
					{
						string GlobalFileName = "global.utoc";
						FileReference InUtocFile = FileReference.Combine(SourceOutputLocation.Directory, GlobalFileName);
						FileReference OutUtocFile = FileReference.Combine(OutputLocation.Directory, GlobalFileName);
						if (FileExists_NoExceptions(InUtocFile.FullName))
						{
							bCopiedExistingGlobalUtoc = CopyExistingContainer(InUtocFile, OutUtocFile);
						}
					}
				}

				string PatchSourceContentPath = null;
				if (bShouldGeneratePatch && !ShouldSkipGeneratingPatch(PlatformGameConfig, PakParams.PakName))
				{
					// don't include the post fix in this filename because we are looking for the source pak path
					string PakFilename = PakParams.PakName + "-" + SC.FinalCookPlatform + "*.pak";
					PatchSourceContentPath = SC.StageTargetPlatform.GetReleasePakFilePath(SC, Params, PakFilename);
				}

				if (!bCopiedExistingPak)
				{
					if (FileReference.Exists(OutputLocation) && !Params.IgnorePaksFromDifferentCookSource)
					{
						string UnrealPakResponseFileName = CombinePaths(CmdEnv.EngineSavedFolder, "ResponseFiles", "PakList_" + OutputLocation.GetFileNameWithoutExtension() + ".txt");
						if (File.Exists(UnrealPakResponseFileName) && FileReference.GetLastWriteTimeUtc(OutputLocation) > File.GetLastWriteTimeUtc(UnrealPakResponseFileName))
						{
							bCopiedExistingPak = true;
						}
					}
					if (!bCopiedExistingPak)
					{
						Dictionary<string, string> UnrealPakResponseFile = PakParams.UnrealPakResponseFile;
						if (bShouldCreateIoStoreContainerFiles)
						{
							UnrealPakResponseFile = new Dictionary<string, string>();
							Dictionary<string, string> IoStoreResponseFile = new Dictionary<string, string>();
							foreach (var Entry in PakParams.UnrealPakResponseFile)
							{
								// Solution to filter non cooked packages from I/O store container file(s)
								if (SC.OnlyAllowPackagesFromStdCookPathInIoStore &&
									(!string.IsNullOrEmpty(Params.CookOutputDir) ?
									!Entry.Key.StartsWith(Params.CookOutputDir, StringComparison.InvariantCultureIgnoreCase) :
									// Temporarily leave the hardcoded path check since CookOutputDir can be empty in some cases 
									!Entry.Key.ToLower().Contains("\\saved\\cooked\\")))
								{
									UnrealPakResponseFile.Add(Entry.Key, Entry.Value);
									continue;
								}

								if (Path.GetExtension(Entry.Key).Contains(".uasset") ||
									Path.GetExtension(Entry.Key).Contains(".umap"))
								{
									IoStoreResponseFile.Add(Entry.Key, Entry.Value);
								}
								else if(Path.GetExtension(Entry.Key).Contains(".ubulk") ||
										Path.GetExtension(Entry.Key).Contains(".uptnl"))
								{
									IoStoreResponseFile.Add(Entry.Key, Entry.Value);
								}
								else if (Path.GetExtension(Entry.Key).Contains(".ushaderbytecode"))
								{
									IoStoreResponseFile.Add(Entry.Key, Entry.Value);
								}
								else if (!Path.GetExtension(Entry.Key).Contains(".uexp"))
								{
									// all other extensions go in Pak not iostore
									//	for example the compression-excluded extensions (ini,res) go in here
									UnrealPakResponseFile.Add(Entry.Key, Entry.Value);
								}

							}

							string ContainerPatchSourcePath = null;
							if (Params.HasBasedOnReleaseVersion)
							{
								string ContainerWildcard = PakParams.PakName + "-" + SC.FinalCookPlatform + "*.utoc";
								ContainerPatchSourcePath = CombinePaths(Params.GetBasedOnReleaseVersionPath(SC, Params.Client), ContainerWildcard);
							}
							bool bGenerateDiffPatch = bShouldGeneratePatch && !ShouldSkipGeneratingPatch(PlatformGameConfig, PakParams.PakName);
							bool bCompressContainers = PakParams.bCompressed;
							DirectoryReference StageLooseFileRootPath = PakParams.bStageLoose ? DirectoryReference.Combine(SC.RuntimeRootDir, "Engine", "Binaries", SC.PlatformDir) : null;

							IoStoreCommands.Add(GetIoStoreCommandArguments(
								IoStoreResponseFile,
								OutputLocation.GetFileNameWithoutAnyExtensions(),
								OutputLocation,
								SC.OptionalFileStageDirectory,
								PakParams.bOnDemand,
								bCompressContainers,
								StageLooseFileRootPath,
								Params.SkipEncryption ? null : CryptoSettings,
								Params.SkipEncryption ? "" : PakParams.EncryptionKeyGuid,
								ContainerPatchSourcePath,
								bGenerateDiffPatch,
								Params.HasDLCName));
						}

						if (!PakParams.bStageLoose)
						{
							string PakFileSpecificAdditionalArgs = "";
							if (bShouldGeneratePatch && ShouldSkipGeneratingPatch(PlatformGameConfig, PakParams.PakName))
							{
								PakFileSpecificAdditionalArgs = " -SkipPatch";
							}
							string PakEncryptionKeyGuid = Params.SkipEncryption ? "" : PakParams.EncryptionKeyGuid;
							PakCommands.Add(GetPakFileSpecificUnrealPakArguments(
								UnrealPakResponseFile,
								OutputLocation,
								PakFileSpecificAdditionalArgs,
								PakParams.bCompressed,
								PakParams.bRehydrateAssets,
								Params.SkipEncryption ? null : CryptoSettings,
								PatchSourceContentPath,
								Params.SkipEncryption ? "" : PakParams.EncryptionKeyGuid));
						}
						LogNames.Add(OutputLocation.GetFileNameWithoutExtension());
					}
				}

				Outputs.Add(Tuple.Create(OutputLocation, OutputRelativeLocation, PatchSourceContentPath));
			}

			// Actually execute UnrealPak
			if (PakCommands.Count > 0)
			{
				string PakCommandsFileName = CombinePaths(CmdEnv.LogFolder, "PakCommands.txt");
				using (var Writer = new StreamWriter(PakCommandsFileName, false, new System.Text.UTF8Encoding(true)))
				{
					foreach (string Command in PakCommands)
					{
						Writer.WriteLine(Command);
					}
				}

				StringBuilder Arguments = new StringBuilder(MakePathSafeToUseWithCommandLine(Params.RawProjectPath.FullName));
				string CommonArguments = GetCommonUnrealPakArguments(
					PrimaryOrderFiles,
					CommonAdditionalArgs,
					Params.SkipEncryption ? null : CryptoSettings,
					Params.SkipEncryption ? null : CryptoKeysCacheFilename,
					SecondaryOrderFiles,
					Params.Unattended
				);
				Arguments.AppendFormat(" {0}", CommonArguments);
				Arguments.AppendFormat(" -CreateMultiple={0}", CommandUtils.MakePathSafeToUseWithCommandLine(PakCommandsFileName));
				RunAndLog(CmdEnv, GetUnrealPakLocation().FullName, Arguments.ToString(), Options: ERunOptions.Default | ERunOptions.UTF8Output);
			}

			if (IoStoreCommands.Count > 0)
			{
				string IoStoreCommandsFileName = CombinePaths(CmdEnv.LogFolder, "IoStoreCommands.txt");
				using (var Writer = new StreamWriter(IoStoreCommandsFileName, false, new System.Text.UTF8Encoding(true)))
				{
					foreach (string Command in IoStoreCommands)
					{
						Writer.WriteLine(Command);
					}
				}

				List<OrderFile> FinalOrderFiles = OrderFiles.FindAll(x => (x.OrderType != OrderFile.OrderFileType.Cooker && x.OrderType != OrderFile.OrderFileType.Editor));
				if (bUseSecondaryOrder && OrderFiles.Count >= 1)
				{
					FinalOrderFiles.AddRange(OrderFiles.FindAll(x => x.OrderType == OrderFile.OrderFileType.Cooker));
				}

				string AdditionalPlatformPakCmdLine = "";
				if (SC.CustomDeployment == null || !SC.CustomDeployment.GetPlatformPakCommandLine(Params, SC, ref AdditionalPlatformPakCmdLine))
				{
					AdditionalPlatformPakCmdLine += SC.StageTargetPlatform.GetPlatformPakCommandLine(Params, SC);
				}

				string AdditionalPlatformIoStoreCmdLine = "";
				if (SC.CustomDeployment == null || !SC.CustomDeployment.GetPlatformIoStoreCommandLine(Params, SC, ref AdditionalPlatformIoStoreCmdLine))
				{
					AdditionalPlatformIoStoreCmdLine += SC.StageTargetPlatform.GetPlatformIoStoreCommandLine(Params, SC);
				}

				string AdditionalArgs =
					AdditionalPlatformPakCmdLine
					+ AdditionalPlatformIoStoreCmdLine
					+ BulkOption
					+ CompressionFormats
					+ " " + AdditionalCompressionOptionsOnCommandLine
					+ " " + Params.AdditionalPakOptions;

				if (CryptoKeysCacheFilename != null)
				{
					AdditionalArgs += String.Format(" -cryptokeys={0}", CommandUtils.MakePathSafeToUseWithCommandLine(CryptoKeysCacheFilename.FullName));
				}

				if (CryptoSettings != null)
				{
					if (CryptoSettings.bDataCryptoRequired && CryptoSettings.bEnablePakSigning && CryptoSettings.SigningKey.IsValid())
					{
						AdditionalArgs += String.Format(" -sign");
					}
				}

				int MaxIoStorePartitionSizeMB = 0;
				if (PlatformGameConfig.GetInt32("/Script/UnrealEd.ProjectPackagingSettings", "MaxIoStorePartitionSizeMB", out MaxIoStorePartitionSizeMB))
				{
					if (MaxIoStorePartitionSizeMB > 0)
					{
						AdditionalArgs += String.Format(" -maxPartitionSize={0}MB", MaxIoStorePartitionSizeMB);
					}
				}

				int CompressionMinBytesSaved = 0;
				if (PlatformGameConfig.GetInt32("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionMinBytesSaved", out CompressionMinBytesSaved))
				{
					if (CompressionMinBytesSaved > 0)
					{
						AdditionalArgs += String.Format(" -compressionMinBytesSaved={0}", CompressionMinBytesSaved);
					}
				}

				int CompressionMinPercentSaved = 0;
				if (PlatformGameConfig.GetInt32("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionMinPercentSaved", out CompressionMinPercentSaved))
				{
					if (CompressionMinBytesSaved > 0)
					{
						AdditionalArgs += String.Format(" -compressionMinPercentSaved={0}", CompressionMinPercentSaved);
					}
				}

				bool CompresionEnableDDC = false;
				if (PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bPackageCompressionEnableDDC", out CompresionEnableDDC))
				{
					if (CompresionEnableDDC)
					{
						AdditionalArgs += " -compressionEnableDDC";
					}
				}

				int CompressionMinSizeToConsiderDDC = 0;
				if (PlatformGameConfig.GetInt32("/Script/UnrealEd.ProjectPackagingSettings", "PackageCompressionMinSizeToConsiderDDC", out CompressionMinSizeToConsiderDDC))
				{
					if (CompressionMinSizeToConsiderDDC > 0)
					{
						AdditionalArgs += String.Format(" -compressionMinSizeToConsiderDDC={0}", CompressionMinSizeToConsiderDDC);
					}
				}


				if (Params.WriteBackMetadataToAssetRegistry.Length != 0)
				{
					AdditionalArgs += " -WriteBackMetadataToAssetRegistry=" + Params.WriteBackMetadataToAssetRegistry;
				}
				else
				{
					string WriteBackMetadataToAssetRegistry;
					if (PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "WriteBackMetadataToAssetRegistry", out WriteBackMetadataToAssetRegistry))
					{
						AdditionalArgs += " -WriteBackMetadataToAssetRegistry=" + WriteBackMetadataToAssetRegistry;
					}

					bool bWritePluginSizeSummaryJsons = false;
					if (PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bWritePluginSizeSummaryJsons", out bWritePluginSizeSummaryJsons) &&
						bWritePluginSizeSummaryJsons)
					{
						AdditionalArgs += " -WritePluginSizeSummaryJsons";
					}
				}


				if (Params.ReferenceContainerGlobalFileName.Length != 0)
				{
					AdditionalArgs += " -ReferenceContainerGlobalFileName=\"" + Params.ReferenceContainerGlobalFileName + "\"";
					if (Params.ReferenceContainerCryptoKeys.Length != 0)
					{
						AdditionalArgs += " -ReferenceContainerCryptoKeys=\"" + Params.ReferenceContainerCryptoKeys + "\"";
					}
				}


				AdditionalArgs += " " + Params.AdditionalIoStoreOptions;

				RunIoStore(Params, SC, IoStoreCommandsFileName, FinalOrderFiles, AdditionalArgs);
			}

			if (!string.IsNullOrWhiteSpace(Params.Upload))
			{
				StringBuilder UploadArgs = new StringBuilder();
				if (Params.Upload.ToLower() == "localzen")
				{
					int Port = 8558;
					string ServiceUrl = string.Format("http://127.0.0.1:{0}", Port);
					List<string> Addresses = GetHostAddresses(SC);
					if (Addresses.Count > 0)
					{
						//TODO: Support multiple addresses
						ServiceUrl = string.Format("http://{0}:{1}", Addresses[0], Port);
					}

					if (!string.IsNullOrWhiteSpace(Params.NoZenAutoLaunch))
					{
						ServiceUrl = Params.NoZenAutoLaunch.Trim();
						if (!ServiceUrl.StartsWith("http://"))
						{
							int Sep = ServiceUrl.LastIndexOf(':');
							if (Sep < 0)
							{
								ServiceUrl = string.Format("http://{0}:{1}", ServiceUrl, Port);
							}
							else
							{
								ServiceUrl = "http://" + ServiceUrl;
							}
						}
					}

					string PakPath = CombinePaths(SC.StageDirectory.FullName, SC.ShortProjectName, "Content", "Paks");
					string CloudDir = CombinePaths(SC.StageDirectory.FullName, "Cloud");
					string ConfigFilePath = CombinePaths(CloudDir, "IoStoreOnDemand.ini");

					UploadArgs.Append(string.Format("-Upload={0}", PakPath));
					UploadArgs.Append(string.Format(" -ServiceUrl={0}/obj/bucket", ServiceUrl));
					UploadArgs.Append(" -Bucket=" + SC.ShortProjectName.ToLower());
					UploadArgs.Append(" -ConfigFilePath=\"" + ConfigFilePath + "\"");
					UploadArgs.Append(" -TargetPlatform=" + SC.StageTargetPlatform.PlatformType.ToString());
					UploadArgs.Append(" -KeepContainerFiles");
					UploadArgs.Append(" -KeepPakFiles");
					UploadArgs.Append(" -BuildVersion=0");
					UploadArgs.Append(" -WriteTocToDisk");
					if (Params.AdditionalPakOptions.Length > 0)
					{
						UploadArgs.Append(" " + Params.AdditionalPakOptions);
					}

					FileReference CryptoKeysFilename = FileReference.Combine(SC.MetadataDir, "Crypto.json");
					if (FileExists_NoExceptions(CryptoKeysFilename.FullName))
					{
						UploadArgs.Append(string.Format(" -CryptoKeys={0}", CryptoKeysFilename.FullName));
					}
				}
				else
				{
					UploadArgs.Append(Params.Upload);
				}

				if (UploadArgs.Length > 0)
				{
					string PakArgs = UploadArgs.ToString();
					Logger.LogInformation("Running UnrealPak with arguments: {CommandletParams}", PakArgs);
					RunAndLog(CmdEnv, GetUnrealPakLocation().FullName, PakArgs, Options: ERunOptions.Default | ERunOptions.UTF8Output);
				}
			}

			// Do any additional processing on the command output
			for (int Idx = 0; Idx < PakParamsList.Count; Idx++)
			{
				string PakName = PakParamsList[Idx].PakName;
				FileReference OutputLocation = Outputs[Idx].Item1;
				StagedFileReference OutputRelativeLocation = Outputs[Idx].Item2;
				string PatchSourceContentPath = Outputs[Idx].Item3;

				if (Params.HasCreateReleaseVersion)
				{
					// copy the created pak to the release version directory we might need this later if we want to generate patches
					//string ReleaseVersionPath = CombinePaths( SC.ProjectRoot, "Releases", Params.CreateReleaseVersion, SC.StageTargetPlatform.GetCookPlatform(Params.DedicatedServer, false), Path.GetFileName(OutputLocation) );
					string ReleaseVersionPath = SC.StageTargetPlatform.GetReleasePakFilePath(SC, Params, OutputLocation.GetFileName());

					InternalUtils.SafeCreateDirectory(Path.GetDirectoryName(ReleaseVersionPath));
					InternalUtils.SafeCopyFile(OutputLocation.FullName, ReleaseVersionPath);
					if (bShouldCreateIoStoreContainerFiles)
					{
						InternalUtils.SafeCopyFile(Path.ChangeExtension(OutputLocation.FullName, ".utoc"), Path.ChangeExtension(ReleaseVersionPath, ".utoc"));
						InternalUtils.SafeCopyFile(Path.ChangeExtension(OutputLocation.FullName, ".ucas"), Path.ChangeExtension(ReleaseVersionPath, ".ucas"));
					}
				}

				if (Params.CreateChunkInstall)
				{
					var RegEx = new Regex("pakchunk(\\d+)", RegexOptions.IgnoreCase | RegexOptions.Compiled);
					var Matches = RegEx.Matches(PakName);

					int ChunkID = 0;
					if (Matches.Count != 0 && Matches[0].Groups.Count > 1)
					{
						ChunkID = Convert.ToInt32(Matches[0].Groups[1].ToString());
					}
					else if (Params.HasDLCName)
					{
						// Assuming DLC is a single pack file
						ChunkID = 1;
					}
					else
					{
						throw new AutomationException(String.Format("Failed Creating Chunk Install Data, Unable to parse chunk id from {0}", PakName));
					}

					if (ChunkID != 0)
					{
						var BPTExe = GetBuildPatchToolExecutable();
						EnsureBuildPatchToolExists();

						string VersionString = Params.ChunkInstallVersionString;
						string ChunkInstallBasePath = CombinePaths(Params.ChunkInstallDirectory, SC.FinalCookPlatform);
						string RawDataPath = CombinePaths(ChunkInstallBasePath, VersionString, PakName);
						string RawDataPakPath = CombinePaths(RawDataPath, PakName + "-" + SC.FinalCookPlatform + GetPostFix(PakName) + ".pak");
						bool bPakFilesAreSigned = InternalUtils.SafeFileExists(Path.ChangeExtension(OutputLocation.FullName, ".sig"));

						//copy the pak chunk to the raw data folder
						InternalUtils.SafeDeleteFile(RawDataPakPath, true);
						InternalUtils.SafeDeleteFile(Path.ChangeExtension(RawDataPakPath, ".sig"), true);
						InternalUtils.SafeCreateDirectory(RawDataPath, true);
						InternalUtils.SafeCopyFile(OutputLocation.FullName, RawDataPakPath);
						if (bPakFilesAreSigned)
						{
							InternalUtils.SafeCopyFile(Path.ChangeExtension(OutputLocation.FullName, ".sig"), Path.ChangeExtension(RawDataPakPath, ".sig"), true);
						}
						if (bShouldCreateIoStoreContainerFiles)
						{
							InternalUtils.SafeCopyFile(Path.ChangeExtension(OutputLocation.FullName, ".utoc"), Path.ChangeExtension(RawDataPakPath, ".utoc"), true);
							InternalUtils.SafeCopyFile(Path.ChangeExtension(OutputLocation.FullName, ".ucas"), Path.ChangeExtension(RawDataPakPath, ".ucas"), true);
						}
						InternalUtils.SafeDeleteFile(OutputLocation.FullName, true);

						if (Params.IsGeneratingPatch)
						{
							if (String.IsNullOrEmpty(PatchSourceContentPath))
							{
								throw new AutomationException(String.Format("Failed Creating Chunk Install Data. No source pak for patch pak {0} given", OutputLocation));
							}
							// If we are generating a patch, then we need to copy the original pak across
							// for distribution.
							string SourceRawDataPakPath = CombinePaths(RawDataPath, PakName + "-" + SC.FinalCookPlatform + ".pak");
							InternalUtils.SafeCopyFile(PatchSourceContentPath, SourceRawDataPakPath);
							if (bPakFilesAreSigned)
							{
								InternalUtils.SafeCopyFile(PatchSourceContentPath, Path.ChangeExtension(SourceRawDataPakPath, ".sig"), true);
							}
							if (bShouldCreateIoStoreContainerFiles)
							{
								InternalUtils.SafeCopyFile(PatchSourceContentPath, Path.ChangeExtension(SourceRawDataPakPath, ".utoc"), true);
								InternalUtils.SafeCopyFile(PatchSourceContentPath, Path.ChangeExtension(SourceRawDataPakPath, ".ucas"), true);
							}
						}

						string BuildRoot = MakePathSafeToUseWithCommandLine(RawDataPath);
						string CloudDir = MakePathSafeToUseWithCommandLine(CombinePaths(ChunkInstallBasePath, "CloudDir"));
						string ManifestDir = CombinePaths(ChunkInstallBasePath, "ManifestDir");
						var AppID = 1; // For a chunk install this value doesn't seem to matter
						string AppName = String.Format("{0}_{1}", SC.ShortProjectName, PakName);
						string AppLaunch = ""; // For a chunk install this value doesn't seem to matter
						string ManifestFilename = AppName + VersionString + ".manifest";
						string SourceManifestPath = CombinePaths(CloudDir, ManifestFilename);
						string DestManifestPath = CombinePaths(ManifestDir, ManifestFilename);
						InternalUtils.SafeCreateDirectory(ManifestDir, true);

						string CmdLine = String.Format("-BuildRoot={0} -CloudDir={1} -AppID={2} -AppName=\"{3}\" -BuildVersion=\"{4}\" -AppLaunch=\"{5}\"", BuildRoot, CloudDir, AppID, AppName, VersionString, AppLaunch);
						CmdLine += " -AppArgs=\"\"";
						CmdLine += " -custom=\"bIsPatch=false\"";
						CmdLine += String.Format(" -customint=\"ChunkID={0}\"", ChunkID);
						CmdLine += " -customint=\"PakReadOrdering=0\"";
						CmdLine += " -stdout";

						string BuildPatchToolLogFileName = "BuildPatchTool_" + PakName;
						RunAndLog(CmdEnv, BPTExe, CmdLine, BuildPatchToolLogFileName, Options: ERunOptions.Default | ERunOptions.UTF8Output);

						InternalUtils.SafeCopyFile(SourceManifestPath, DestManifestPath);

						// generate the primary manifest
						GeneratePrimaryChunkManifest(CloudDir, Params.ChunkInstallVersionString, SC.FinalCookPlatform);
					}
					else
					{
						// add the first pak file as needing deployment and convert to lower case again if needed
						SC.FilesToStage.UFSFiles.Add(OutputRelativeLocation, OutputLocation);
					}
				}
				else
				{
					// add the pak file as needing deployment and convert to lower case again if needed
					SC.FilesToStage.UFSFiles.Add(OutputRelativeLocation, OutputLocation);

					// add the base pak files to deployment as well
					if (bShouldGeneratePatch && !ShouldSkipGeneratingPatch(PlatformGameConfig, PakName))
					{
						string ExistingPatchSearchPath = SC.StageTargetPlatform.GetReleasePakFilePath(SC, Params, null);
						if (Directory.Exists(ExistingPatchSearchPath))
						{
							HashSet<string> IncludedExtensions = new HashSet<string>();
							IncludedExtensions.Add(OutputFilenameExtension);
							if (bShouldCreateIoStoreContainerFiles)
							{
								IncludedExtensions.Add(".ucas");
								IncludedExtensions.Add(".utoc");
							}
							IEnumerable<string> PakFileSet = Directory.EnumerateFiles(ExistingPatchSearchPath, PakName + "-" + SC.FinalCookPlatform + "*.*");
							foreach (string PakFilePath in PakFileSet)
							{
								if (!IncludedExtensions.Contains(Path.GetExtension(PakFilePath).ToLower()))
								{
									continue;
								}
								FileReference OutputDestinationPath = FileReference.Combine(OutputLocation.Directory, Path.GetFileName(PakFilePath));
								if (!File.Exists(OutputDestinationPath.FullName))
								{
									InternalUtils.SafeCopyFile(PakFilePath, OutputDestinationPath.FullName);
									StagedFileReference OutputDestinationRelativeLocation = StagedFileReference.Combine(SC.RelativeProjectRootForStage, "Content/Paks/", Path.GetFileName(PakFilePath));
									if (SC.StageTargetPlatform.DeployLowerCaseFilenames(StagedFileType.UFS))
									{
										OutputDestinationRelativeLocation = OutputDestinationRelativeLocation.ToLowerInvariant();
									}
									OutputDestinationRelativeLocation = SC.StageTargetPlatform.Remap(OutputDestinationRelativeLocation);
									SC.FilesToStage.UFSFiles.Add(OutputDestinationRelativeLocation, OutputDestinationPath);
								}
							}
						}
					}
				}
			}
		}

		private static void RunIoStore(ProjectParams Params, DeploymentContext SC, string CommandsFileName, List<OrderFile> OrderFiles, string AdditionalArgs)
		{
			StagedFileReference GlobalContainerOutputRelativeLocation;
			GlobalContainerOutputRelativeLocation = StagedFileReference.Combine(SC.RelativeProjectRootForStage, "Content", "Paks", "global.utoc");
			if (SC.StageTargetPlatform.DeployLowerCaseFilenames(StagedFileType.UFS))
			{
				GlobalContainerOutputRelativeLocation = GlobalContainerOutputRelativeLocation.ToLowerInvariant();
			}
			GlobalContainerOutputRelativeLocation = SC.StageTargetPlatform.Remap(GlobalContainerOutputRelativeLocation);
			FileReference GlobalContainerOutputLocation = FileReference.Combine(SC.RuntimeRootDir, GlobalContainerOutputRelativeLocation.Name);

			string CommandletParams = String.Empty;

			if (Params.HasDLCName)
			{
				CommandletParams += String.Format("-CreateDLCContainer={0}", MakePathSafeToUseWithCommandLine(Params.DLCFile.FullName));

				DirectoryReference DLCRoot = Params.DLCFile.Directory;
				string DLCName = Params.DLCFile.GetFileNameWithoutExtension();

				//TODO: Find a better way. Create Plugin ConfigType and interate all ini files in plugin?
				bool bRemapPluginContentToGame = false;
				FileReference PluginConfigFile = FileReference.Combine(DLCRoot, "Config", String.Format("Default{0}.ini", DLCName));
				if (FileReference.Exists(PluginConfigFile))
				{
					ConfigFile File = new ConfigFile(PluginConfigFile);
					ConfigFileSection PluginSettings;
					if (File.TryGetSection("PluginSettings", out PluginSettings))
					{
						foreach (ConfigLine Line in PluginSettings.Lines)
						{
							if (Line.Key == "RemapPluginContentToGame")
							{
								bool.TryParse(Line.Value, out bRemapPluginContentToGame);
								break;
							}
						}
					}
				}

				if (bRemapPluginContentToGame)
				{
					CommandletParams += " -RemapPluginContentToGame";
				}
			}
			else
			{
				CommandletParams += String.Format("-CreateGlobalContainer={0}", MakePathSafeToUseWithCommandLine(GlobalContainerOutputLocation.FullName));
			}

			CommandletParams += String.Format(" -CookedDirectory={0} -PackageStoreManifest={1} -Commands={2}",
				MakePathSafeToUseWithCommandLine(SC.PlatformCookDir.ToString()),
				MakePathSafeToUseWithCommandLine(SC.PackageStoreManifest.FullPath),
				MakePathSafeToUseWithCommandLine(CommandsFileName));
			if (SC.PackageStoreManifest.ZenCookedFiles == null)
			{
				FileReference ScriptObjectsFile = FileReference.Combine(SC.MetadataDir, "scriptobjects.bin");
				if (!FileReference.Exists(ScriptObjectsFile))
				{
					throw new AutomationException(String.Format("A script objects descriptor file is required when staging to IoStore. Expected to find {0}. Ensure cooking was successful.", ScriptObjectsFile.FullName));
				}
				CommandletParams += String.Format(" -ScriptObjects={0}", MakePathSafeToUseWithCommandLine(ScriptObjectsFile.FullName));
			}

			if (OrderFiles != null && OrderFiles.Count() > 0)
			{
				CommandletParams += String.Format(" -Order={0}", MakePathSafeToUseWithCommandLine(string.Join(",", OrderFiles.Select(u => u.File.FullName).ToArray())));
				CommandletParams += String.Format(" -OrderPriority={0}", MakePathSafeToUseWithCommandLine(string.Join(",", OrderFiles.Select(u => u.Priority).ToArray())));
			}
			if (!string.IsNullOrWhiteSpace(AdditionalArgs))
			{
				CommandletParams += AdditionalArgs;
			}

			if (Params.HasBasedOnReleaseVersion)
			{
				CommandletParams += String.Format(" -BasedOnReleaseVersionPath={0}", Params.GetBasedOnReleaseVersionPath(SC, Params.Client));
			}

			if (Params.Unattended)
			{
				// We don't want unrealpak popping up interactive dialogs while we're running a build
				CommandletParams += " -unattended";
			}

			Logger.LogInformation("Running UnrealPak with arguments: {CommandletParams}", CommandletParams);
			RunAndLog(CmdEnv, GetUnrealPakLocation().FullName, CommandletParams, Options: ERunOptions.Default | ERunOptions.UTF8Output);
		}

		/// <summary>
		/// Creates pak files using streaming install chunk manifests.
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		private static void CopyPaksFromNetwork(ProjectParams Params, DeploymentContext SC)
		{
			Logger.LogInformation("Copying paks from network.");

			if (!CommandUtils.P4Enabled)
			{
				throw new AutomationException("-PrePak requires -P4");
			}
			if (CommandUtils.P4Env.Changelist < 1000)
			{
				throw new AutomationException("-PrePak requires a CL from P4 and we have {0}", CommandUtils.P4Env.Changelist);
			}

			string BuildRoot = CombinePaths(CommandUtils.RootBuildStorageDirectory());
			string CachePath = InternalUtils.GetEnvironmentVariable("UE-BuildCachePath", "");

			string SrcBuildPath = CombinePaths(BuildRoot, Params.ShortProjectName);
			string SrcBuildPath2 = CombinePaths(BuildRoot, Params.ShortProjectName.Replace("Game", "").Replace("game", ""));

			string SrcBuildPath_Cache = CombinePaths(CachePath, Params.ShortProjectName);
			string SrcBuildPath2_Cache = CombinePaths(CachePath, Params.ShortProjectName.Replace("Game", "").Replace("game", ""));

			if (!InternalUtils.SafeDirectoryExists(SrcBuildPath))
			{
				if (!InternalUtils.SafeDirectoryExists(SrcBuildPath2))
				{
					throw new AutomationException("PrePak: Neither {0} nor {1} exists.", SrcBuildPath, SrcBuildPath2);
				}
				SrcBuildPath = SrcBuildPath2;
				SrcBuildPath_Cache = SrcBuildPath2_Cache;
			}
			string SrcCLPath = CombinePaths(SrcBuildPath, CommandUtils.EscapePath(CommandUtils.P4Env.Branch) + "-CL-" + CommandUtils.P4Env.Changelist.ToString());
			string SrcCLPath_Cache = CombinePaths(SrcBuildPath_Cache, CommandUtils.EscapePath(CommandUtils.P4Env.Branch) + "-CL-" + CommandUtils.P4Env.Changelist.ToString());
			if (!InternalUtils.SafeDirectoryExists(SrcCLPath))
			{
				throw new AutomationException("PrePak: {0} does not exist.", SrcCLPath);
			}
			string PlatformPath = CombinePaths(SrcCLPath, SC.FinalCookPlatform);
			string PlatformPath_Cache = CombinePaths(SrcCLPath_Cache, SC.FinalCookPlatform);
			if (!InternalUtils.SafeDirectoryExists(PlatformPath))
			{
				throw new AutomationException("PrePak: {0} does not exist.", PlatformPath);
			}
			string PakPath = CombinePaths(PlatformPath, "Staged", SC.ShortProjectName, "Content", "Paks");
			string PakPath_Cache = CombinePaths(PlatformPath_Cache, "Staged", SC.ShortProjectName, "Content", "Paks");
			if (!InternalUtils.SafeDirectoryExists(PakPath))
			{
				throw new AutomationException("PrePak: {0} does not exist.", PakPath);
			}

			string DestPath = CombinePaths(SC.StageDirectory.FullName, SC.ShortProjectName, "Content", "Paks");

			{
				var PakFiles = CommandUtils.FindFiles("*.pak", false, PakPath);
				var SigFiles = CommandUtils.FindFiles("*.sig", false, PakPath);

				var Files = new List<string>();

				Files.AddRange(PakFiles);
				Files.AddRange(SigFiles);

				if (Files.Count < 1)
				{
					throw new AutomationException("PrePak: {0} exists but does not have any paks in it.", PakPath);
				}


				foreach (var SrcFile in Files)
				{
					string DestFileName = CombinePaths(DestPath, new FileReference(SrcFile).GetFileName());
					if (!string.IsNullOrEmpty(CachePath))
					{
						string CacheSrcFile = CombinePaths(PakPath_Cache, new FileReference(SrcFile).GetFileName());
						try
						{
							if (InternalUtils.SafeFileExists(CacheSrcFile))
							{
								var Info = new System.IO.FileInfo(SrcFile);
								var InfoCache = new System.IO.FileInfo(CacheSrcFile);
								if (Info.Exists && InfoCache.Exists && Info.Length == InfoCache.Length)
								{
									Logger.LogInformation("Copying from cache {CacheSrcFile} -> {DestFileName}", CacheSrcFile, DestFileName);
									CopyFileIncremental(new FileReference(CacheSrcFile), new FileReference(DestFileName));
									continue;
								}
							}
						}
						catch (Exception)
						{

						}
					}
					Logger.LogInformation("Copying {SrcFile} -> {DestFileName}", SrcFile, DestFileName);
					CopyFileIncremental(new FileReference(SrcFile), new FileReference(DestFileName));
				}
			}
		}

		// Defines contents of a specific chunk/pak file
		private class ChunkDefinition
		{
			public ChunkDefinition(string InChunkName)
			{
				ChunkName = InChunkName;
				ResponseFile = new ConcurrentDictionary<string, string>();
				Manifest = null;
				bCompressed = false;
				bAllowPerChunkCompression = false;
				bOnDemand = false;
				bStageLoose = false;
			}

			// Name of pak file without extension, ie pakchunk0
			public string ChunkName;

			// List of files to include in this chunk
			public ConcurrentDictionary<string, string> ResponseFile;

			// Parsed copy of pakchunk*.txt
			public HashSet<string> Manifest;

			// Data for compression and encryption
			public bool bCompressed;
			public string EncryptionKeyGuid;
			public string RequestedEncryptionKeyGuid;

			// Allow the per-chunk bCompressed flag to be used instead of the global parameter
			public bool bAllowPerChunkCompression;

			// Whether the chunk is used for content-on-demand content delivery or not
			public bool bOnDemand;

			// Whether the chunk is used for content-on-demand content delivery or not
			public bool bStageLoose;
		}

		private static void AddChunkDefinition(ConcurrentDictionary<string, ChunkDefinition> ChunkSet, ChunkDefinition OriginalChunk, string ModifiedChunkName)
		{
			if (!ChunkSet.ContainsKey(ModifiedChunkName))
			{
				ChunkDefinition NewChunk = new ChunkDefinition(ModifiedChunkName);
				NewChunk.RequestedEncryptionKeyGuid = OriginalChunk.RequestedEncryptionKeyGuid;
				NewChunk.EncryptionKeyGuid = OriginalChunk.EncryptionKeyGuid;
				ChunkSet.TryAdd(ModifiedChunkName, NewChunk);
			}
		}

		/// <summary>
		/// Creates pak files using streaming install chunk manifests.
		/// </summary>
		/// <param name="Params"></param>
		/// <param name="SC"></param>
		private static void CreatePaksUsingChunkManifests(ProjectParams Params, DeploymentContext SC)
		{
			Logger.LogInformation("Creating pak using streaming install manifests.");
			DumpManifest(SC, CombinePaths(CmdEnv.LogFolder, "PrePak" + (SC.DedicatedServer ? "_Server" : "") + "_" + SC.CookPlatform));

			ConfigHierarchy PlatformGameConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, DirectoryReference.FromFile(Params.RawProjectPath), SC.StageTargetPlatform.IniPlatformType, SC.CustomConfig);

			bool bShouldGenerateEarlyDownloaderPakFile = false, bForceOneChunkPerFile = false;
			string EarlyDownloaderPakFilePrefix = string.Empty;
			if (!Params.HasDLCName)
			{
				PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "GenerateEarlyDownloaderPakFile", out bShouldGenerateEarlyDownloaderPakFile);
			}
			PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bForceOneChunkPerFile", out bForceOneChunkPerFile);
			PlatformGameConfig.GetString("/Script/UnrealEd.ProjectPackagingSettings", "EarlyDownloaderPakFilePrefix", out EarlyDownloaderPakFilePrefix);

			List<ChunkDefinition> ChunkDefinitions = new List<ChunkDefinition>();
			List<PakFileRules> PakRulesList = GetPakFileRules(Params, SC);

			var TmpPackagingPath = GetTmpPackagingPath(Params, SC);

			// Parse and cache crypto settings from INI file
			EncryptionAndSigning.CryptoSettings PakCryptoSettings = EncryptionAndSigning.ParseCryptoSettings(DirectoryReference.FromFile(Params.RawProjectPath), SC.StageTargetPlatform.IniPlatformType, Log.Logger);
			FileReference CryptoKeysCacheFilename = FileReference.Combine(SC.MetadataDir, "Crypto.json");
			PakCryptoSettings.Save(CryptoKeysCacheFilename);

			// We still want to have a list of all files to stage. We will use the chunk manifests
			// to put the files from staging manifest into the right chunk
			var StagingManifestResponseFile = CreatePakResponseFileFromStagingManifest(SC, SC.FilesToStage.UFSFiles);

			{
				// DefaultChunkIndex assumes 0 is the 'base' chunk
				const int DefaultChunkIndex = 0;

				var ChunkListFilename = GetChunkPakManifestListFilename(Params, SC);
				List<string> ChunkList = new List<string>(ReadAllLines(ChunkListFilename));
				Logger.LogInformation("Reading chunk list file {ChunkListFilename} which contains {Arg1} entries", ChunkListFilename, ChunkList.Count);

				for (int Index = 0; Index < ChunkList.Count; ++Index)
				{
					string[] ChunkOptions = ChunkList[Index].Split(' ');

					// Set chunk name to string like "pakchunk0"
					var ChunkManifestFilename = CombinePaths(TmpPackagingPath, ChunkOptions[0]);
					ChunkDefinition CD = new ChunkDefinition(Path.GetFileNameWithoutExtension(ChunkOptions[0]));
					for (int IOption = 1; IOption < ChunkOptions.Length; ++IOption)
					{
						if (string.Compare(ChunkOptions[IOption], "compressed", true) == 0)
						{
							CD.bCompressed = true;
						}
						else if(string.Compare(ChunkOptions[IOption], "AllowPerChunkCompression", true) == 0)
						{
							CD.bAllowPerChunkCompression = true;
						}
						else if (ChunkOptions[IOption].StartsWith("encryptionkeyguid="))
						{
							CD.RequestedEncryptionKeyGuid = ChunkOptions[IOption].Substring(ChunkOptions[IOption].IndexOf('=') + 1);

							if (PakCryptoSettings.SecondaryEncryptionKeys != null)
							{
								foreach (EncryptionAndSigning.EncryptionKey Key in PakCryptoSettings.SecondaryEncryptionKeys)
								{
									if (string.Compare(Key.Guid, CD.RequestedEncryptionKeyGuid, true) == 0)
									{
										CD.EncryptionKeyGuid = CD.RequestedEncryptionKeyGuid;
										break;
									}
								}
							}
						}
					}
					CD.Manifest = ReadPakChunkManifest(ChunkManifestFilename);
					Logger.LogInformation("Reading chunk manifest {ChunkManifestFilename} which contains {Arg1} entries", ChunkManifestFilename, CD.Manifest.Count);
					ChunkDefinitions.Add(CD);
				}

				const string OptionalBulkDataFileExtension = ".uptnl";
				ConcurrentDictionary<string, ChunkDefinition> AdditionalChunks = new ConcurrentDictionary<string, ChunkDefinition>(StringComparer.InvariantCultureIgnoreCase);
				ChunkDefinition DefaultChunk = ChunkDefinitions[DefaultChunkIndex];

				Dictionary<string, List<ChunkDefinition>> FileNameToChunks = new Dictionary<string, List<ChunkDefinition>>(StringComparer.InvariantCultureIgnoreCase);
				foreach (ChunkDefinition Chunk in ChunkDefinitions)
				{
					foreach (string FileName in Chunk.Manifest)
					{
						List<ChunkDefinition> Chunks;
						if (!FileNameToChunks.TryGetValue(FileName, out Chunks))
						{
							Chunks = new List<ChunkDefinition>();
							FileNameToChunks.Add(FileName, Chunks);
						}
						Chunks.Add(Chunk);
					}
				}

				// Add chunk definitions
				if (PakRulesList != null)
				{
					foreach (PakFileRules Rule in PakRulesList)
					{
						string ChunkName = Rule.Name;
						if (Rule.OverridePaks != null && Rule.OverridePaks.Count > 0)
						{
							ChunkName = Rule.OverridePaks[0];
						}

						if (ChunkDefinitions.Any(D => D.ChunkName == ChunkName))
						{
							continue;
						}

						bool bIsEncryptionKeyValid = true;
						if (Rule.bOnDemand)
						{
							Logger.LogInformation("Creating content-on-demand chunk definition '{Arg0}' for PAK rule '{Arg1}'", ChunkName, Rule.Name);

							if (string.IsNullOrEmpty(Rule.EncryptionKeyGuid))
							{
								bIsEncryptionKeyValid = false;
							}
							else
							{
								Guid KeyGuid;
								if (!Guid.TryParse(Rule.EncryptionKeyGuid, out KeyGuid))
								{
									bIsEncryptionKeyValid = false;
									Logger.LogWarning("Invalid encryption key GUID specified for content-on-demand PAK rule '{Arg0}'", Rule.Name);
								}

								if (!PakCryptoSettings.ContainsEncryptionKey(Rule.EncryptionKeyGuid))
								{
									bIsEncryptionKeyValid = false;
									Logger.LogWarning("No matching encryption key found for PAK rule '{Arg0}', GUID '{Arg1}'", Rule.Name, Rule.EncryptionKeyGuid);
								}
							}
						}
						else
						{
							Logger.LogInformation("Creating chunk definition '{Arg0}' for PAK rule '{Arg1}'", ChunkName, Rule.Name);
						}

						ChunkDefinition Chunk = new ChunkDefinition(ChunkName);
						Chunk.bOnDemand = Rule.bOnDemand;
						Chunk.bStageLoose = Rule.bStageLoose;
						Chunk.bCompressed = !Rule.bStageLoose;
						Chunk.EncryptionKeyGuid = bIsEncryptionKeyValid ? Rule.EncryptionKeyGuid : null;
						ChunkDefinitions.Add(Chunk);
					}
				}

				ConcurrentDictionary<string, ChunkDefinition> ChunkNameToDefinition = new ConcurrentDictionary<string, ChunkDefinition>(StringComparer.InvariantCultureIgnoreCase);
				foreach (ChunkDefinition Chunk in ChunkDefinitions)
				{
					ChunkNameToDefinition.TryAdd(Chunk.ChunkName, Chunk);
				}

				Parallel.ForEach(StagingManifestResponseFile, StagingFile =>
				{
					bool bAddedToChunk = false;
					bool bExcludeFromPaks = false;
					HashSet<ChunkDefinition> PakList = new HashSet<ChunkDefinition>();

					string OriginalFilename = StagingFile.Key;
					string NoExtension = CombinePaths(Path.GetDirectoryName(OriginalFilename), Path.GetFileNameWithoutExtension(OriginalFilename));
					string AdditionalExtension = Path.GetExtension(NoExtension);
					if (AdditionalExtension == ".m" || AdditionalExtension == ".o")
					{
						// Hack around .m.ubulk files having a double extension
						NoExtension = CombinePaths(Path.GetDirectoryName(OriginalFilename), Path.GetFileNameWithoutExtension(NoExtension));
					}
					string OriginalReplaceSlashes = OriginalFilename.Replace('/', '\\');
					string NoExtensionReplaceSlashes = NoExtension.Replace('/', '\\');

					// First read manifest
					List<ChunkDefinition> Chunks;
					if (FileNameToChunks.TryGetValue(OriginalFilename, out Chunks))
					{
						PakList.UnionWith(Chunks);
					}
					if (FileNameToChunks.TryGetValue(OriginalReplaceSlashes, out Chunks))
					{
						PakList.UnionWith(Chunks);
					}
					if (FileNameToChunks.TryGetValue(NoExtension, out Chunks))
					{
						PakList.UnionWith(Chunks);
					}
					if (FileNameToChunks.TryGetValue(NoExtensionReplaceSlashes, out Chunks))
					{
						PakList.UnionWith(Chunks);
					}

					// Now run through the pak rules which may override things
					PakFileRules? MatchingRule = ApplyPakFileRules(PakRulesList, StagingFile, PakList, ChunkNameToDefinition, out bExcludeFromPaks);

					bool bStageLoose = MatchingRule.HasValue && MatchingRule.Value.bStageLoose;
					if (bStageLoose)
					{
						string StageLooseChunkName = "_stage_loose_files";
						if (!AdditionalChunks.ContainsKey(StageLooseChunkName))
						{
							ChunkDefinition NewChunk = new ChunkDefinition(StageLooseChunkName);
							NewChunk.bStageLoose = true;
							AdditionalChunks.TryAdd(StageLooseChunkName, NewChunk);
						}
						ChunkDefinition TargetChunk = AdditionalChunks[StageLooseChunkName];
						TargetChunk.ResponseFile.TryAdd(StagingFile.Key, StagingFile.Value);

						// Loose files get uploaded through BPT as well as copied to "ondemand" pak files
						//return;
					}

					if (bExcludeFromPaks)
					{
						return;
					}

					// Actually add to chunk
					foreach (ChunkDefinition Chunk in PakList)
					{
						ChunkDefinition TargetChunk = Chunk;

						string OrigExt = Path.GetExtension(OriginalFilename);

						bool bOnDemand = false;
						if (MatchingRule.HasValue && MatchingRule.Value.bOnDemand)
						{
							bOnDemand = true;
							if (MatchingRule.Value.OnDemandAllowedChunkFilters != null)
							{
								bOnDemand = MatchingRule.Value.OnDemandAllowedChunkFilters.Any(Filter => Filter.IsMatch(TargetChunk.ChunkName));
							}
						}

						if (bStageLoose || bOnDemand)
						{
							// make a new separate pak if not already marked as on demand
							if (!Chunk.bOnDemand)
							{
								string StreamingChunkName = Chunk.ChunkName + (OrigExt.Equals(OptionalBulkDataFileExtension) ? "ondemandoptional" : "ondemand");
								AddChunkDefinition(AdditionalChunks, Chunk, StreamingChunkName);
								TargetChunk = AdditionalChunks[StreamingChunkName];

								if (bOnDemand)
								{
									TargetChunk.bOnDemand = true;
									if (!string.IsNullOrEmpty(MatchingRule.Value.EncryptionKeyGuid))
									{
										TargetChunk.EncryptionKeyGuid = MatchingRule.Value.EncryptionKeyGuid;
										TargetChunk.RequestedEncryptionKeyGuid = MatchingRule.Value.EncryptionKeyGuid;
									}
								}
							}
						}
						else if (OrigExt.Equals(OptionalBulkDataFileExtension))
						{
							// any optional files encountered we want to put in a separate pak file
							string OptionalChunkName = Chunk.ChunkName + "optional";
							AddChunkDefinition(AdditionalChunks, Chunk, OptionalChunkName);
							TargetChunk = AdditionalChunks[OptionalChunkName];
						}

						TargetChunk.ResponseFile.TryAdd(StagingFile.Key, StagingFile.Value);
						bAddedToChunk = true;

						if (bForceOneChunkPerFile)
						{
							// Files are only allowed to be in a single chunk
							break;
						}
					}

					if (!bAddedToChunk)
					{
						if (bStageLoose)
						{
							Logger.LogInformation("No chunk assignment found for OnDemand file {Arg0}. Skipping.", StagingFile.Key);
						}
						else
						{
							//LogInformation("No chunk assigned found for {0}. Using default chunk.", StagingFile.Key);
							DefaultChunk.ResponseFile.TryAdd(StagingFile.Key, StagingFile.Value);
						}
					}
				});

				foreach (var OptionalChunkIt in AdditionalChunks)
				{
					ChunkDefinitions.Add(OptionalChunkIt.Value);
				}
			}

			// chunk downloader pak file is a minimal pak file which contains no content.  It can be provided with as a minimal download so that the game can download all the content from another source.
			// Deprecated in favor of DefaultPakFileRules.ini
			if (bShouldGenerateEarlyDownloaderPakFile)
			{
				string EarlyChunkName = EarlyDownloaderPakFilePrefix;
				ChunkDefinition EarlyChunk = new ChunkDefinition(EarlyChunkName);

				EarlyChunk.bCompressed = true;

				ConcurrentDictionary<string,string> EarlyPakFile = EarlyChunk.ResponseFile;

				// find the list of files to put in the early downloader pak file
				List<string> FilesInEarlyPakFile = new List<string>();
				PlatformGameConfig.GetArray("/Script/UnrealEd.ProjectPackagingSettings", "EarlyDownloaderPakFileFiles", out FilesInEarlyPakFile);

				FileFilter EarlyPakFileFilter = new FileFilter();
				foreach (string FileFilter in FilesInEarlyPakFile)
				{
					EarlyPakFileFilter.AddRule(FileFilter);
				}

				List<string> FilesToFilter = new List<string>();
				foreach (var ResponseFile in StagingManifestResponseFile)
				{
					FilesToFilter.Add(ResponseFile.Key);
				}

				foreach ( string FilteredFile in EarlyPakFileFilter.ApplyTo(FilesToFilter) )
				{
					EarlyPakFile.TryAdd(FilteredFile, StagingManifestResponseFile[FilteredFile]);
				}

				ChunkDefinitions.Add(EarlyChunk);
			}

			if (Params.CreateChunkInstall)
			{
				string ManifestDir = CombinePaths(Params.ChunkInstallDirectory, SC.FinalCookPlatform, "ManifestDir");
				if (InternalUtils.SafeDirectoryExists(ManifestDir))
				{
					foreach (string ManifestFile in Directory.GetFiles(ManifestDir, "*.manifest"))
					{
						InternalUtils.SafeDeleteFile(ManifestFile, true);
					}
				}
				string DestDir = CombinePaths(Params.ChunkInstallDirectory, SC.FinalCookPlatform, Params.ChunkInstallVersionString);
				if (InternalUtils.SafeDirectoryExists(DestDir))
				{
					InternalUtils.SafeDeleteDirectory(DestDir);
				}
			}

			System.Threading.Tasks.ParallelOptions Options = new System.Threading.Tasks.ParallelOptions();

			Logger.LogInformation("Creating Pak files utilizing {Arg0} cores", Environment.ProcessorCount);
			Options.MaxDegreeOfParallelism = Environment.ProcessorCount;

			// Check for chunks that requested an encryption key that we don't have. This can happen for Epic contractors who don't have access to our keychains. Emit a warning though, just in case
			// this somehow happens when the keychain is available
			foreach (ChunkDefinition CD in ChunkDefinitions)
			{
				if (!string.IsNullOrEmpty(CD.RequestedEncryptionKeyGuid) && (CD.RequestedEncryptionKeyGuid != CD.EncryptionKeyGuid))
				{
					Logger.LogWarning("Chunk '" + CD.ChunkName + "' requested encryption key '" + CD.RequestedEncryptionKeyGuid + "' but it couldn't be found in the project keychain.");
				}
			}

			List<CreatePakParams> PakInputs = new List<CreatePakParams>();
			foreach (ChunkDefinition Chunk in ChunkDefinitions)
			{
				if (Chunk.ResponseFile.Count > 0)
				{
					string EncryptionKeyToUse = Params.SkipEncryption ? "" : Chunk.EncryptionKeyGuid;
					PakInputs.Add(new CreatePakParams(Chunk.ChunkName,
						Chunk.ResponseFile.ToDictionary(entry => entry.Key, entry => entry.Value),
						Chunk.bCompressed, 
						Params.RehydrateAssets, EncryptionKeyToUse, Chunk.bOnDemand, Chunk.bStageLoose, Chunk.bAllowPerChunkCompression));
				}
			}

			CreatePaks(Params, SC, PakInputs, PakCryptoSettings, CryptoKeysCacheFilename);

			String ChunkLayerFilename = CombinePaths(GetTmpPackagingPath(Params, SC), GetChunkPakLayerListName());
			String OutputChunkLayerFilename = Path.Combine(SC.ProjectRoot.FullName, "Build", SC.FinalCookPlatform, "ChunkLayerInfo", GetChunkPakLayerListName());
			Directory.CreateDirectory(Path.GetDirectoryName(OutputChunkLayerFilename));
			File.Copy(ChunkLayerFilename, OutputChunkLayerFilename, true);
		}

		private static void GeneratePrimaryChunkManifest(string Dir, string Version, string PlatformStr)
		{
			//Create the directory if it doesn't exist
			InternalUtils.SafeCreateDirectory(Dir);

			string FileName = CombinePaths(Dir, PlatformStr.ToLower() + ".manifestlist");
			using (JsonWriter Writer = new JsonWriter(FileName))
			{
				Writer.WriteObjectStart();
				Writer.WriteValue("ClientVersion", Version);
				Writer.WriteValue("BuildUrl", Version + "/" + PlatformStr);
				Writer.WriteArrayStart("files");
				// iterate over all of the files in the directory
				DirectoryInfo di = new DirectoryInfo(Dir);
				foreach (var fi in di.EnumerateFiles("*.manifest"))
				{
					if (fi.Name == PlatformStr.ToLower() + ".manifest")
						continue;
					FileStream fs = fi.OpenRead();
					byte[] hash = System.Security.Cryptography.SHA1.Create().ComputeHash(fs);
					fs.Seek(0, SeekOrigin.Begin);
					byte[] hash256 = System.Security.Cryptography.SHA256.Create().ComputeHash(fs);
					fs.Close();
					Writer.WriteObjectStart();
					Writer.WriteValue("filename", fi.Name);
					Writer.WriteValue("uniqueFilename", fi.Name);
					Writer.WriteValue("length", fi.Length);
					Writer.WriteValue("URL", fi.Name);
					Writer.WriteValue("hash", BitConverter.ToString(hash).Replace("-", ""));
					Writer.WriteValue("hash256", BitConverter.ToString(hash256).Replace("-", ""));
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();
				Writer.WriteObjectEnd();
			}
		}

		private static bool DoesChunkPakManifestExist(ProjectParams Params, DeploymentContext SC)
		{
			return FileExists_NoExceptions(GetChunkPakManifestListFilename(Params, SC));
		}

		private static string GetChunkPakManifestListFilename(ProjectParams Params, DeploymentContext SC)
		{
			return CombinePaths(GetTmpPackagingPath(Params, SC), "pakchunklist.txt");
		}

		private static string GetChunkPakLayerListName()
		{
			return "pakchunklayers.txt";
		}

		private static string GetTmpPackagingPath(ProjectParams Params, DeploymentContext SC)
		{
			string TmpPackagingPath = CombinePaths(Path.GetDirectoryName(Params.RawProjectPath.FullName), "Saved", "TmpPackaging", SC.CookPlatform);
			if (Params.bUseExtraFlavor)
			{
				TmpPackagingPath = CombinePaths(TmpPackagingPath, "ExtraFlavor");
			}

			return TmpPackagingPath;
		}

		private static bool ShouldCreateIoStoreContainerFiles(ProjectParams Params, DeploymentContext SC)
		{
			if (!ShouldCreatePak(Params, SC))
			{
				return false;
			}

			if (Params.IsProgramTarget)
			{
				return false;
			}

			if (Params.SkipIoStore)
			{
				return false;
			}

			if (Params.IoStore)
			{
				return true;
			}

			ConfigHierarchy PlatformGameConfig = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, DirectoryReference.FromFile(Params.RawProjectPath), SC.StageTargetPlatform.IniPlatformType, SC.CustomConfig);
			bool bUseIoStore = false;
			PlatformGameConfig.GetBool("/Script/UnrealEd.ProjectPackagingSettings", "bUseIoStore", out bUseIoStore);
			return bUseIoStore;
		}

		private static bool ShouldCreatePak(ProjectParams Params, DeploymentContext SC)
		{
			if (Params.CookOnTheFly)
			{
				return false;
			}

			Platform.PakType Pak = SC.StageTargetPlatform.RequiresPak(Params);

			// we may care but we don't want. 
			if (Params.SkipPak)
				return false;

			if (Pak == Platform.PakType.Always)
			{
				return true;
			}
			else if (Pak == Platform.PakType.Never)
			{
				return false;
			}
			else // DontCare
			{
				return (Params.Pak);
			}
		}

		private static bool ShouldCreatePak(ProjectParams Params)
		{
			Platform.PakType Pak = Params.ClientTargetPlatformInstances[0].RequiresPak(Params);

			// we may care but we don't want. 
			if (Params.SkipPak)
				return false;

			if (Pak == Platform.PakType.Always)
			{
				return true;
			}
			else if (Pak == Platform.PakType.Never)
			{
				return false;
			}
			else // DontCare
			{
				return (Params.Pak);
			}
		}

		protected static void DeletePakFiles(string StagingDirectory)
		{
			var StagedFilesDir = new DirectoryInfo(StagingDirectory);
			StagedFilesDir.GetFiles("*.pak", SearchOption.AllDirectories).ToList().ForEach(File => File.Delete());
			StagedFilesDir.GetFiles("*.ucas", SearchOption.AllDirectories).ToList().ForEach(File => File.Delete());
			StagedFilesDir.GetFiles("*.utoc", SearchOption.AllDirectories).ToList().ForEach(File => File.Delete());
		}

		protected static void CleanDirectoryExcludingPakFiles(DirectoryInfo StagingDirectory)
		{
			foreach (DirectoryInfo SubDir in StagingDirectory.EnumerateDirectories())
			{
				if (!SubDir.Name.Equals("Paks", StringComparison.OrdinalIgnoreCase))
				{
					CleanDirectoryExcludingPakFiles(SubDir);
					try { SubDir.Delete(); } catch { }
				}
			}

			foreach (System.IO.FileInfo File in StagingDirectory.EnumerateFiles())
			{
				try { File.Delete(); } catch { }
			}
		}

		public static void CleanStagingDirectory(ProjectParams Params, DeploymentContext SC)
		{
			if (Params.NoCleanStage)
			{
				Logger.LogInformation("Skipping clean of staging directory due to -NoCleanStage argument.");
			}
			else if (SC.Stage && !Params.SkipStage)
			{
				if (Params.SkipPak)
				{
					Logger.LogInformation("Cleaning Stage Directory (exluding PAK files): {Arg0}", SC.StageDirectory.FullName);
					CleanDirectoryExcludingPakFiles(new DirectoryInfo(SC.StageDirectory.FullName));
				}
				else
				{
					Logger.LogInformation("Cleaning Stage Directory: {Arg0}", SC.StageDirectory.FullName);
					try
					{
						DeleteDirectory(SC.StageDirectory.FullName);
					}
					catch (Exception Ex)
					{
						// Delete cooked data (if any) as it may be incomplete / corrupted.
						throw new AutomationException(ExitCode.Error_FailedToDeleteStagingDirectory, Ex, "Stage Failed. Failed to delete staging directory " + SC.StageDirectory.FullName);
					}
					if (SC.OptionalFileStageDirectory != null && DirectoryReference.Exists(SC.OptionalFileStageDirectory))
					{
						Logger.LogInformation("Cleaning Optional Stage Directory: {Arg0}", SC.OptionalFileStageDirectory.FullName);
						try
						{
							DeleteDirectory(SC.OptionalFileStageDirectory.FullName);
						}
						catch (Exception Ex)
						{
							// Delete cooked data (if any) as it may be incomplete / corrupted.
							throw new AutomationException(ExitCode.Error_FailedToDeleteStagingDirectory, Ex, "Stage Failed. Failed to delete staging directory " + SC.OptionalFileStageDirectory.FullName);
						}
					}
				}
			}
			else
			{
				Logger.LogInformation("Cleaning PAK files in stage directory: {Arg0}", SC.StageDirectory.FullName);
				try
				{
					// delete old pak files
					DeletePakFiles(SC.StageDirectory.FullName);
				}
				catch (Exception Ex)
				{
					// Delete cooked data (if any) as it may be incomplete / corrupted.
					throw new AutomationException(ExitCode.Error_FailedToDeleteStagingDirectory, Ex, "Stage Failed. Failed to delete pak files in " + SC.StageDirectory.FullName);
				}
			}
		}

		public static void ApplyStagingManifest(ProjectParams Params, DeploymentContext SC)
		{
			if (ShouldCreatePak(Params, SC))
			{
				if (!ShouldCreateIoStoreContainerFiles(Params, SC))
				{
					FileReference ZenFileManifest = FileReference.Combine(SC.MetadataDir, "zenfs.manifest");
					if (FileReference.Exists(ZenFileManifest))
					{
						throw new AutomationException(String.Format("A Zen file manifest must not exist when staging without IoStore. Did not expected to find {0}. Ensure that legacy cooking with ZenStore disabled (bUseZenStore=false) was successful.", ZenFileManifest.FullName));
					}
				}
				if (SC.CrashReporterUFSFiles.Count > 0)
				{
					CreatePakForCrashReporter(Params, SC);
				}

				if (Params.PrePak)
				{
					CopyPaksFromNetwork(Params, SC);
				}
				else if (SC.PlatformUsesChunkManifests && DoesChunkPakManifestExist(Params, SC))
				{
					CreatePaksUsingChunkManifests(Params, SC);
				}
				else
				{
					CreatePakUsingStagingManifest(Params, SC);
				}
			}

			string BaseManifestFileName = CombinePaths(CmdEnv.LogFolder, "FinalCopy" + (SC.DedicatedServer ? "_Server" : "") + SC.StageTargetPlatform.PlatformType.ToString());
			DumpManifest(SC.FilesToStage.UFSFiles, BaseManifestFileName + "_UFSFiles.txt");

			DumpManifest(SC.FilesToStage.NonUFSFiles, BaseManifestFileName + "_NonUFSFiles.txt");
			DumpManifest(SC.FilesToStage.NonUFSDebugFiles, BaseManifestFileName + "_NonUFSFilesDebug.txt");

			CopyUsingStagingManifest(Params, SC);

			var ThisPlatform = SC.StageTargetPlatform;
			ThisPlatform.PostStagingFileCopy(Params, SC);
		}

		private static DirectoryReference GetIntermediateCommandlineDir(DeploymentContext SC)
		{
			return DirectoryReference.Combine(SC.ProjectRoot, "Intermediate", "UAT", SC.FinalCookPlatform);
		}

		public static void WriteStageCommandline(FileReference IntermediateCmdLineFile, ProjectParams Params, DeploymentContext SC)
		{
			// this file needs to be treated as a UFS file for casing, but NonUFS for being put into the .pak file. 
			// @todo: Maybe there should be a new category - UFSNotForPak
			if (SC.StageTargetPlatform.DeployLowerCaseFilenames(StagedFileType.SystemNonUFS))
			{
				IntermediateCmdLineFile = new FileReference(CombinePaths(Path.GetDirectoryName(IntermediateCmdLineFile.FullName), Path.GetFileName(IntermediateCmdLineFile.FullName).ToLowerInvariant()));
			}
			if (FileReference.Exists(IntermediateCmdLineFile))
			{
				FileReference.Delete(IntermediateCmdLineFile);
			}

			if (!SC.StageTargetPlatform.ShouldStageCommandLine(Params, SC))
			{
				return;
			}

			StringBuilder CommandLineBuilder = new StringBuilder();
			CommandLineBuilder.AppendFormat("{0} ", SC.ProjectArgForCommandLines);

			if (!string.IsNullOrEmpty(Params.StageCommandline))
			{
				CommandLineBuilder.AppendFormat("{0} ", Params.StageCommandline.Trim(new char[] { '\"' }));
			}

			String FileHostCommandline = GetFileHostCommandline(Params, SC);
			if (!string.IsNullOrEmpty(FileHostCommandline))
			{
				CommandLineBuilder.AppendFormat("{0} ", FileHostCommandline);
			}

			if (Params.IterativeDeploy)
			{
				CommandLineBuilder.Append("-iterative ");
			}

			if (!string.IsNullOrEmpty(Params.ReferenceContainerGlobalFileName))
			{
				CommandLineBuilder.AppendFormat("-ReferenceContainerGlobalFileName={0}", Params.ReferenceContainerGlobalFileName);
				if (!string.IsNullOrEmpty(Params.ReferenceContainerCryptoKeys))
				{
					CommandLineBuilder.AppendFormat("-ReferenceContainerCryptoKeys={0}", Params.ReferenceContainerCryptoKeys);
				}
			}

			String CompleteCommandLine = CommandLineBuilder.ToString().Trim();

			if (string.IsNullOrEmpty(CompleteCommandLine))
			{
				return;
			}

			Logger.LogInformation("Creating UECommandLine.txt");
			DirectoryReference.CreateDirectory(GetIntermediateCommandlineDir(SC));
			File.WriteAllText(IntermediateCmdLineFile.FullName, CompleteCommandLine);
		}

		private static void WriteStageCommandline(ProjectParams Params, DeploymentContext SC)
		{
			// always delete the existing commandline text file, so it doesn't reuse an old one
			FileReference IntermediateCmdLineFile = FileReference.Combine(GetIntermediateCommandlineDir(SC), "UECommandLine.txt");
			WriteStageCommandline(IntermediateCmdLineFile, Params, SC);
		}

		private static Dictionary<StagedFileReference, string> ReadDeployedManifest(ProjectParams Params, DeploymentContext SC, List<string> ManifestList)
		{
			Dictionary<StagedFileReference, string> DeployedFiles = new Dictionary<StagedFileReference, string>();
			HashSet<StagedFileReference> CRCFiles = SC.StageTargetPlatform.GetFilesForCRCCheck();

			// read the manifest
			bool bContinueSearch = true;
			foreach (string Manifest in ManifestList)
			{
				int FilesAdded = 0;
				if (bContinueSearch)
				{
					string Data = File.ReadAllText(Manifest);
					string[] Lines = Data.Split('\n');
					foreach (string Line in Lines)
					{
						string[] Pair = Line.Split('\t');
						if (Pair.Length > 1)
						{
							StagedFileReference Filename = new StagedFileReference(Pair[0]);
							string TimeStamp = Pair[1];
							FilesAdded++;
							if (DeployedFiles.ContainsKey(Filename))
							{
								if ((CRCFiles.Contains(Filename) && DeployedFiles[Filename] != TimeStamp) || (!CRCFiles.Contains(Filename) && DateTime.Parse(DeployedFiles[Filename]) > DateTime.Parse(TimeStamp)))
								{
									DeployedFiles[Filename] = TimeStamp;
								}
							}
							else
							{
								DeployedFiles.Add(Filename, TimeStamp);
							}
						}
					}
				}
				File.Delete(Manifest);

				if (FilesAdded == 0 && bContinueSearch)
				{
					// no files have been deployed at all to this, so remove all previously added files and exit the loop as this means we need to deploy everything
					DeployedFiles.Clear();
					bContinueSearch = false;
				}
			}

			return DeployedFiles;
		}

		protected static Dictionary<StagedFileReference, string> ReadStagedManifest(ProjectParams Params, DeploymentContext SC, string Manifest)
		{
			Dictionary<StagedFileReference, string> StagedFiles = new Dictionary<StagedFileReference, string>();
			HashSet<StagedFileReference> CRCFiles = SC.StageTargetPlatform.GetFilesForCRCCheck();

			// get the staged manifest from staged directory
			FileReference ManifestFile = FileReference.Combine(SC.StageDirectory, Manifest);
			if (FileReference.Exists(ManifestFile))
			{
				string[] Lines = FileReference.ReadAllLines(ManifestFile);
				foreach (string Line in Lines)
				{
					string[] Pair = Line.Split('\t');
					if (Pair.Length > 1)
					{
						StagedFileReference Filename = new StagedFileReference(Pair[0]);
						string TimeStamp = Pair[1];
						if (!StagedFiles.ContainsKey(Filename))
						{
							StagedFiles.Add(Filename, TimeStamp);
						}
						else if ((CRCFiles.Contains(Filename) && StagedFiles[Filename] != TimeStamp) || (!CRCFiles.Contains(Filename) && DateTime.Parse(StagedFiles[Filename]) > DateTime.Parse(TimeStamp)))
						{
							StagedFiles[Filename] = TimeStamp;
						}
					}
				}
			}
			return StagedFiles;
		}

		protected static void WriteObsoleteManifest(ProjectParams Params, DeploymentContext SC, Dictionary<StagedFileReference, string> DeployedFiles, Dictionary<StagedFileReference, string> StagedFiles, string ObsoleteManifest)
		{
			List<StagedFileReference> ObsoleteFiles = new List<StagedFileReference>();

			// any file that has been deployed, but is no longer in the staged files is obsolete and should be deleted.
			foreach (KeyValuePair<StagedFileReference, string> File in DeployedFiles)
			{
				if (!StagedFiles.ContainsKey(File.Key))
				{
					ObsoleteFiles.Add(File.Key);
				}
			}

			// write out to the deltamanifest.json
			using (var Writer = File.CreateText(ObsoleteManifest))
			{
				foreach (StagedFileReference ObsoleteFile in ObsoleteFiles)
				{
					Writer.WriteLine(ObsoleteFile);
				}
			}
		}

		protected static void WriteDeltaManifest(ProjectParams Params, DeploymentContext SC, Dictionary<StagedFileReference, string> DeployedFiles, Dictionary<StagedFileReference, string> StagedFiles, string DeltaManifest)
		{
			HashSet<StagedFileReference> CRCFiles = SC.StageTargetPlatform.GetFilesForCRCCheck();
			List<string> DeltaFiles = new List<string>();
			foreach (KeyValuePair<StagedFileReference, string> File in StagedFiles)
			{
				bool bNeedsDeploy = true;
				if (DeployedFiles.ContainsKey(File.Key))
				{
					if (CRCFiles.Contains(File.Key))
					{
						bNeedsDeploy = (File.Value != DeployedFiles[File.Key]);
					}
					else
					{
						DateTime Staged = DateTime.Parse(File.Value);
						DateTime Deployed = DateTime.Parse(DeployedFiles[File.Key]);
						bNeedsDeploy = (Staged > Deployed);
					}
				}

				if (bNeedsDeploy)
				{
					DeltaFiles.Add(File.Key.Name);
				}
			}

			// add the manifest
			if (!DeltaManifest.Contains("NonUFS"))
			{
				DeltaFiles.Add(SC.GetNonUFSDeployedManifestFileName(null));
				DeltaFiles.Add(SC.GetUFSDeployedManifestFileName(null));
			}

			// TODO: determine files which need to be removed

			// write out to the deltamanifest.json
			FileReference ManifestFile = FileReference.Combine(SC.StageDirectory, DeltaManifest);
			FileReference.WriteAllLines(ManifestFile, DeltaFiles);
		}

		//@todo move this
		public static List<DeploymentContext> CreateDeploymentContext(ProjectParams Params, bool InDedicatedServer, bool DoCleanStage = false)
		{
			ParamList<string> ListToProcess;
			if (Params.IsProgramTarget)
			{
				ListToProcess = Params.ProgramTargets;
			}
			else if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
			{
				ListToProcess = Params.ServerCookedTargets;
			}
			else
			{
				ListToProcess = Params.ClientCookedTargets;
			} 
			var ConfigsToProcess = InDedicatedServer && (Params.Cook || Params.CookOnTheFly) ? Params.ServerConfigsToBuild : Params.ClientConfigsToBuild;

			List<Tuple<string, UnrealTargetConfiguration, UnrealArchitectures>> TargetAndConfigPairs = new();

			foreach (var Target in ListToProcess)
			{
				// If we are staging a client and have been asked to include editor targets, we currently only want to
				// include a single Development editor target. Ideally, we should have shipping editor configs and then
				// just include the requested configs for all targets
				if (Params.HasEditorTargets && Params.EditorTargets.Contains(Target))
				{
					TargetAndConfigPairs.Add(new Tuple<string, UnrealTargetConfiguration, UnrealArchitectures>(Target, UnrealTargetConfiguration.Development, Params.EditorArchitecture));
				}
				else if (Params.HasProgramTargets && Params.ProgramTargets.Contains(Target))
				{
					TargetAndConfigPairs.Add(new Tuple<string, UnrealTargetConfiguration, UnrealArchitectures>(Target, UnrealTargetConfiguration.Development, Params.ProgramArchitecture));
				}
				else
				{
					UnrealArchitectures Arches = InDedicatedServer ? Params.ServerArchitecture : Params.ClientArchitecture;
					foreach (var Config in ConfigsToProcess)
					{
						TargetAndConfigPairs.Add(new Tuple<string, UnrealTargetConfiguration, UnrealArchitectures>(Target, Config, Arches));
					}
				}
			}

			List<TargetPlatformDescriptor> PlatformsToStage = Params.ClientTargetPlatforms;
			if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
			{
				PlatformsToStage = Params.ServerTargetPlatforms;
			}

			List<DeploymentContext> DeploymentContexts = new List<DeploymentContext>();
			foreach (var StagePlatform in PlatformsToStage)
			{
				// Get the platform to get cooked data from, may differ from the stage platform
				TargetPlatformDescriptor CookedDataPlatform = Params.GetCookedDataPlatformForClientTarget(StagePlatform);

				if (InDedicatedServer && (Params.Cook || Params.CookOnTheFly))
				{
					CookedDataPlatform = Params.GetCookedDataPlatformForServerTarget(StagePlatform);
				}

				List<string> ExecutablesToStage = new List<string>();

				string PlatformName = StagePlatform.ToString();
				foreach (var TargetAndConfig in TargetAndConfigPairs)
				{
					string Target = TargetAndConfig.Item1;
					UnrealTargetConfiguration Config = TargetAndConfig.Item2;
					string Exe = Target;
					if (Config != UnrealTargetConfiguration.Development)
					{
						Exe = Target + "-" + PlatformName + "-" + Config.ToString();
					}

					// append the architecture part if needed
					UnrealArchitectureConfig ArchConfig = UnrealArchitectureConfig.ForPlatform(StagePlatform.Type);
					UnrealArchitectures PlatformArches = TargetAndConfig.Item3 ?? ArchConfig.ActiveArchitectures(Params.RawProjectPath, Target);
					if (ArchConfig.RequiresArchitectureFilenames(PlatformArches))
					{
						Exe += PlatformArches.ToString();
					}
					ExecutablesToStage.Add(Exe);
				}

				string StageDirectory = ((ShouldCreatePak(Params) || (Params.Stage)) || !String.IsNullOrEmpty(Params.StageDirectoryParam)) ? Params.BaseStageDirectory : "";
				string ArchiveDirectory = (Params.Archive || !String.IsNullOrEmpty(Params.ArchiveDirectoryParam)) ? Params.BaseArchiveDirectory : "";
				DirectoryReference EngineDir = DirectoryReference.Combine(Unreal.RootDirectory, "Engine");
				DirectoryReference ProjectDir = DirectoryReference.FromFile(Params.RawProjectPath);

				List<StageTarget> TargetsToStage = new List<StageTarget>();
				foreach (var TargetAndConfig in TargetAndConfigPairs)
				{
					string Target = TargetAndConfig.Item1;
					UnrealTargetConfiguration Config = TargetAndConfig.Item2;
					UnrealArchitectures Architecture = TargetAndConfig.Item3;

					DirectoryReference ReceiptBaseDir = Params.IsCodeBasedProject ? ProjectDir : EngineDir;
					// handle Engine programs that have a .uproject specially
					// @todo Non-engine programs (unde a game directory) and also have a .uproject are not supported, because the target and executable are in 
					// Game/Binaries, but here the ProjectDir is the program's directory, not the project. Currently not used, but will need work to fix
					if (Params.ProgramTargets.Contains(Target))
					{
						// if we have a uproject file that is under the engine dir, then the target/exe will be in Engine/Binaries
						if (ProjectDir.IsUnderDirectory(EngineDir))
						{
							ReceiptBaseDir = EngineDir;
						}
					}

					Platform PlatformInstance = Platform.Platforms[StagePlatform];
					UnrealTargetPlatform[] SubPlatformsToStage = PlatformInstance.GetStagePlatforms();

					// if we are attempting to gathering multiple platforms, the files aren't required
					bool bJustPackaging = Params.SkipStage && Params.Package;
					bool bIsIterativeSharedCooking = Params.HasIterateSharedCookedBuild;
					bool bRequireStagedFilesToExist = SubPlatformsToStage.Length == 1 && PlatformsToStage.Count == 1 && !bJustPackaging && !bIsIterativeSharedCooking && !Params.HasDLCName;

					foreach (UnrealTargetPlatform ReceiptPlatform in SubPlatformsToStage)
					{
						if (Architecture == null)
						{
							if (PlatformExports.IsPlatformAvailable(ReceiptPlatform))
							{
								Architecture = UnrealArchitectureConfig.ForPlatform(ReceiptPlatform).ActiveArchitectures(Params.RawProjectPath, Target);
							}
						}

						if (Params.IterateSharedBuildUsePrecompiledExe)
						{
							continue;
						}

						FileReference ReceiptFileName = TargetReceipt.GetDefaultPath(ReceiptBaseDir, Target, ReceiptPlatform, Config, Architecture);
						if (!FileReference.Exists(ReceiptFileName))
						{
							if (bRequireStagedFilesToExist)
							{
								// if we aren't collecting multiple platforms, then it is expected to exist
								throw new AutomationException(ExitCode.Error_MissingExecutable, "Stage Failed. Missing receipt '{0}'. Check that this target has been built.", ReceiptFileName);
							}
							else
							{
								// if it's multiple platforms, then allow missing receipts
								continue;
							}

						}

						// Read the receipt for this target
						TargetReceipt Receipt;
						if (!TargetReceipt.TryRead(ReceiptFileName, out Receipt))
						{
							throw new AutomationException("Missing or invalid target receipt ({0})", ReceiptFileName);
						}

						// Convert the paths to absolute
						TargetsToStage.Add(new StageTarget { Receipt = Receipt, RequireFilesExist = bRequireStagedFilesToExist });
					}
				}

				//@todo should pull StageExecutables from somewhere else if not cooked
				var SC = new DeploymentContext(Params.RawProjectPath, Unreal.RootDirectory,
					String.IsNullOrEmpty(StageDirectory) ? null : new DirectoryReference(StageDirectory),
					String.IsNullOrEmpty(Params.OptionalFileStagingDirectory) ? null : new DirectoryReference(Params.OptionalFileStagingDirectory),
					String.IsNullOrEmpty(Params.OptionalFileInputDirectory) ? null : new DirectoryReference(Params.OptionalFileInputDirectory),
					String.IsNullOrEmpty(ArchiveDirectory) ? null : new DirectoryReference(ArchiveDirectory),
					Params.CookerSupportFilesSubdirectory,
					Platform.Platforms[CookedDataPlatform],
					Platform.Platforms[StagePlatform],
					ConfigsToProcess,
					TargetsToStage,
					ExecutablesToStage,
					InDedicatedServer,
					Params.Cook || Params.CookOnTheFly,
					Params.CrashReporter,
					Params.Stage,
					Params.CookOnTheFly,
					Params.Archive,
					Params.IsProgramTarget,
					Params.Client,
					Params.Manifests,
					Params.SeparateDebugInfo,
					Params.HasDLCName ? Params.DLCFile.Directory : null,
					Params.AdditionalPluginDirectories
					);
				LogDeploymentContext(SC);

				if (Params.PreModifyDeploymentContextCallback != null)
				{
					Params.PreModifyDeploymentContextCallback(Params, SC);
				}

				// If we're a derived platform make sure we're at the end, otherwise make sure we're at the front

				if (!CookedDataPlatform.Equals(StagePlatform))
				{
					DeploymentContexts.Add(SC);
				}
				else
				{
					DeploymentContexts.Insert(0, SC);
				}

				SetupCustomStageCopyHandler(Params, SC);
			}

			return DeploymentContexts;
		}
		private static void SetupCustomStageCopyHandler(ProjectParams Params, DeploymentContext SC)
		{
			string CustomStageCopyHandlerName = null;
			ConfigHierarchy GameIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Game, Params.RawProjectPath.Directory, SC.StageTargetPlatform.PlatformType, SC.CustomConfig);
			GameIni.GetString("/Script/UnrealEd.ProjectPackagingSettings", "CustomStageCopyHandler", out CustomStageCopyHandlerName);

			if (!string.IsNullOrEmpty(CustomStageCopyHandlerName))
			{
				Logger.LogInformation("CopyBuildToStagingDirectory using CustomStageCopyHandler {0}", CustomStageCopyHandlerName);
				try
				{
					SC.CustomStageCopyHandler = CustomStageCopyHandler.Create(CustomStageCopyHandlerName);
				}
				catch
				{
					Logger.LogWarning("Failed to create custom stage copy handler '{Arg0}'. No custom stage copy handler will be applied!", CustomStageCopyHandlerName);
					SC.CustomStageCopyHandler = null;
				}
			}
		}

		public static void CopyBuildToStagingDirectory(ProjectParams Params)
		{
			if (ShouldCreatePak(Params) || (Params.Stage && !Params.SkipStage))
			{
				Params.ValidateAndLog();

				Logger.LogInformation("********** STAGE COMMAND STARTED **********");
				var StartTime = DateTime.UtcNow;

				if (!Params.NoClient)
				{
					var DeployContextList = CreateDeploymentContext(Params, false, true);

					// clean the staging directories first
					foreach (var SC in DeployContextList)
					{
						SC.StageTargetPlatform.PreStage(Params, SC);

						// write out the commandline file now so it can go into the manifest
						WriteStageCommandline(Params, SC);
						CreateStagingManifest(Params, SC);
						CleanStagingDirectory(Params, SC);
					}
					foreach (var SC in DeployContextList)
					{
						ApplyStagingManifest(Params, SC);

						if (Params.Deploy && SC.StageTargetPlatform.RequiresManifestFiles)
						{
							List<string> UFSManifests;
							List<string> NonUFSManifests;

							// get the staged file data
							Dictionary<StagedFileReference, string> StagedUFSFiles = ReadStagedManifest(Params, SC, SC.GetUFSDeployedManifestFileName(null));
							Dictionary<StagedFileReference, string> StagedNonUFSFiles = ReadStagedManifest(Params, SC, SC.GetNonUFSDeployedManifestFileName(null));

							foreach (var DeviceName in Params.DeviceNames)
							{
								// get the deployed file data
								Dictionary<StagedFileReference, string> DeployedUFSFiles = new Dictionary<StagedFileReference, string>();
								Dictionary<StagedFileReference, string> DeployedNonUFSFiles = new Dictionary<StagedFileReference, string>();

								if (SC.StageTargetPlatform.RetrieveDeployedManifests(Params, SC, DeviceName, out UFSManifests, out NonUFSManifests))
								{
									DeployedUFSFiles = ReadDeployedManifest(Params, SC, UFSManifests);
									DeployedNonUFSFiles = ReadDeployedManifest(Params, SC, NonUFSManifests);
								}

								WriteObsoleteManifest(Params, SC, DeployedUFSFiles, StagedUFSFiles, SC.GetUFSDeploymentObsoletePath(DeviceName));
								WriteObsoleteManifest(Params, SC, DeployedNonUFSFiles, StagedNonUFSFiles, SC.GetNonUFSDeploymentObsoletePath(DeviceName));

								if (Params.IterativeDeploy)
								{
									// write out the delta file data
									WriteDeltaManifest(Params, SC, DeployedUFSFiles, StagedUFSFiles, SC.GetUFSDeploymentDeltaPath(DeviceName));
									WriteDeltaManifest(Params, SC, DeployedNonUFSFiles, StagedNonUFSFiles, SC.GetNonUFSDeploymentDeltaPath(DeviceName));
								}
							}
						}

						if (Params.bCodeSign && !Params.SkipStage)
						{
							SC.StageTargetPlatform.SignExecutables(SC, Params);
						}
					}
				}

				if (Params.DedicatedServer)
				{
					var DeployContextList = CreateDeploymentContext(Params, true, true);
					foreach (var SC in DeployContextList)
					{
						CreateStagingManifest(Params, SC);
						CleanStagingDirectory(Params, SC);
					}

					foreach (var SC in DeployContextList)
					{
						ApplyStagingManifest(Params, SC);
					}
				}
				Logger.LogInformation("Stage command time: {0:0.00} s", (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000);
				Logger.LogInformation("********** STAGE COMMAND COMPLETED **********");
			}
		}
	}
}
