// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Linq;
using EpicGames.Core;
using UnrealBuildBase;
using System.Runtime.Versioning;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	///  Base class to handle deploy of a target for a given platform
	/// </summary>
	[SupportedOSPlatform("windows")]
	class HoloLensDeploy : UEBuildDeploy
	{
		private FileReference? MakeAppXPath;
		private FileReference? SignToolPath;
		private string Extension = ".appx";

		public HoloLensDeploy(ILogger InLogger)
			: base(InLogger)
		{
		}

		/// <summary>
		/// Utility function to delete a file
		/// </summary>
		void DeployHelper_DeleteFile(string InFileToDelete)
		{
			Logger.LogInformation("HoloLensDeploy.DeployHelper_DeleteFile({File})", InFileToDelete);
			if (File.Exists(InFileToDelete) == true)
			{
				FileAttributes attributes = File.GetAttributes(InFileToDelete);
				if ((attributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
				{
					attributes &= ~FileAttributes.ReadOnly;
					File.SetAttributes(InFileToDelete, attributes);
				}
				File.Delete(InFileToDelete);
			}
		}

		/// <summary>
		/// Copy the contents of the given source directory to the given destination directory
		/// </summary>
		bool CopySourceToDestDir(string InSourceDirectory, string InDestinationDirectory, string InWildCard,
			bool bInIncludeSubDirectories, bool bInRemoveDestinationOrphans)
		{
			Logger.LogInformation("HoloLensDeploy.CopySourceToDestDir({SourceDir}, {DestDir}, {Wildcard},...)", InSourceDirectory, InDestinationDirectory, InWildCard);
			if (Directory.Exists(InSourceDirectory) == false)
			{
				Logger.LogInformation("Warning: CopySourceToDestDir - SourceDirectory does not exist: {SourceDir}", InSourceDirectory);
				return false;
			}

			// Make sure the destination directory exists!
			Directory.CreateDirectory(InDestinationDirectory);

			SearchOption OptionToSearch = bInIncludeSubDirectories ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;

			var SourceDirs = new List<string>(Directory.GetDirectories(InSourceDirectory, "*.*", OptionToSearch));
			foreach (string SourceDir in SourceDirs)
			{
				string SubDir = SourceDir.Replace(InSourceDirectory, "");
				string DestDir = InDestinationDirectory + SubDir;
				Directory.CreateDirectory(DestDir);
			}

			var SourceFiles = new List<string>(Directory.GetFiles(InSourceDirectory, InWildCard, OptionToSearch));
			var DestFiles = new List<string>(Directory.GetFiles(InDestinationDirectory, InWildCard, OptionToSearch));

			// Keep a list of the files in the source directory... without the source path
			List<string> FilesInSource = new List<string>();

			// Copy all the source files that are newer...
			foreach (string SourceFile in SourceFiles)
			{
				string Filename = SourceFile.Replace(InSourceDirectory, "");
				FilesInSource.Add(Filename.ToUpperInvariant());
				string DestFile = InDestinationDirectory + Filename;

				System.DateTime SourceTime = File.GetLastWriteTime(SourceFile);
				System.DateTime DestTime = File.GetLastWriteTime(DestFile);

				if (SourceTime > DestTime)
				{
					try
					{
						DeployHelper_DeleteFile(DestFile);
						File.Copy(SourceFile, DestFile, true);
					}
					catch (Exception exceptionMessage)
					{
						Logger.LogInformation("Failed to copy {Source} to deployment: {Ex}", SourceFile, exceptionMessage);
					}
				}
			}

			if (bInRemoveDestinationOrphans == true)
			{
				// If requested, delete any destination files that do not have a corresponding
				// file in the source directory
				foreach (string DestFile in DestFiles)
				{
					string DestFilename = DestFile.Replace(InDestinationDirectory, "");
					if (FilesInSource.Contains(DestFilename.ToUpperInvariant()) == false)
					{
						Logger.LogInformation("Destination file does not exist in Source - DELETING: {File}", DestFile);
						//FileAttributes attributes = File.GetAttributes(DestFile);
						try
						{
							DeployHelper_DeleteFile(DestFile);
						}
						catch (Exception exceptionMessage)
						{
							Logger.LogInformation("Failed to delete {DestFile} from deployment: {Ex}", DestFile, exceptionMessage);
						}
					}
				}
			}

			return true;
		}


		/// <summary>
		/// Helper function for copying files
		/// </summary>
		void CopyFile(string InSource, string InDest, bool bForce)
		{
			if (File.Exists(InSource) == true)
			{
				if (File.Exists(InDest) == true)
				{
					if (File.GetLastWriteTime(InSource).CompareTo(File.GetLastWriteTime(InDest)) == 0)
					{
						//If the source and dest have the file and they have the same write times they are assumed to be equal and we don't need to copy.
						return;
					}
					if (bForce == true)
					{
						DeployHelper_DeleteFile(InDest);
					}
				}
				Logger.LogInformation("HoloLensDeploy.CopyFile({Source}, {Dest}, {Force})", InSource, InDest, bForce);
				File.Copy(InSource, InDest, true);
				File.SetAttributes(InDest, File.GetAttributes(InDest) & ~FileAttributes.ReadOnly);
			}
			else
			{
				Logger.LogInformation("HoloLensDeploy: File didn't exist - {Source}", InSource);
			}
		}

		/// <summary>
		/// Helper function for copying a tree files
		/// </summary>
		void CopyDirectory(string InSource, string InDest, bool bForce, bool bRecurse)
		{
			if (Directory.Exists(InSource))
			{
				if (!Directory.Exists(InDest))
				{
					Directory.CreateDirectory(InDest);
				}

				// Copy all files
				string[] FilesInDir = Directory.GetFiles(InSource);
				foreach (string FileSourcePath in FilesInDir)
				{
					string FileDestPath = Path.Combine(InDest, Path.GetFileName(FileSourcePath));
					CopyFile(FileSourcePath, FileDestPath, true);
				}

				// Recurse sub directories
				string[] DirsInDir = Directory.GetDirectories(InSource);
				foreach (string DirSourcePath in DirsInDir)
				{
					string DirName = Path.GetFileName(DirSourcePath);
					string DirDestPath = Path.Combine(InDest, DirName);
					CopyDirectory(DirSourcePath, DirDestPath, bForce, bRecurse);
				}
			}
		}

		public bool PrepForUATPackageOrDeploy(FileReference? ProjectFile, string ProjectName, string ProjectDirectory, WindowsArchitecture Architecture, List<UnrealTargetConfiguration> TargetConfigurations, List<string> ExecutablePaths, string EngineDirectory, bool bForDistribution, string CookFlavor, bool bIsDataDeploy)
		{
			//@todo need to support dlc and other targets
			//string LocalizedContentDirectory = Path.Combine(ProjectDirectory, "Content", "Localization", "Game");
			string AbsoluteExeDirectory = Path.GetDirectoryName(ExecutablePaths[0])!;
			//bool IsGameSpecificExe = ProjectFile != null && AbsoluteExeDirectory.StartsWith(ProjectDirectory);
			//string RelativeExeFilePath = Path.Combine(IsGameSpecificExe ? ProjectName : "Engine", "Binaries", "HoloLens", Path.GetFileName(ExecutablePaths[0]));

			//string TargetDirectory = Path.Combine(ProjectDirectory, "Saved", "HoloLens");


			// If using a secure networking manifest, copy it to the output directory.
			string NetworkManifest = Path.Combine(ProjectDirectory, "Config", "HoloLens", "NetworkManifest.xml");
			if (File.Exists(NetworkManifest))
			{
				CopyFile(NetworkManifest, Path.Combine(AbsoluteExeDirectory, "NetworkManifest.xml"), false);
			}

			// If using Xbox Live generate the json config file expected by the SDK
			DirectoryReference? ConfigDirRef = DirectoryReference.FromFile(ProjectFile);
			if (ConfigDirRef == null && !string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()))
			{
				ConfigDirRef = new DirectoryReference(UnrealBuildTool.GetRemoteIniPath()!);
			}

			MakeAppXPath = HoloLensExports.GetWindowsSdkToolPath("makeappx.exe");
			SignToolPath = HoloLensExports.GetWindowsSdkToolPath("signtool.exe");

			return true;
		}

		private void MakePackage(TargetReceipt Receipt, TargetReceipt NewReceipt, WindowsArchitecture Architecture, List<string> UpdatedFiles)
		{
			string OutputName = String.Format("{0}_{1}_{2}_{3}", Receipt.TargetName, Receipt.Platform, Receipt.Configuration, WindowsExports.GetArchitectureSubpath(Architecture));
			string IntermediateDirectory = Path.Combine(Receipt.ProjectDir != null ? Receipt.ProjectDir.FullName : Unreal.EngineDirectory.FullName, "Intermediate", "Deploy", WindowsExports.GetArchitectureSubpath(Architecture));
			string OutputDirectory = Receipt.Launch!.Directory.FullName;
			string OutputAppX = Path.Combine(OutputDirectory, OutputName + Extension);
			string SigningCertificate = @"Build\HoloLens\SigningCertificate.pfx";
			string SigningCertificatePath = Path.Combine(Receipt.ProjectDir != null ? Receipt.ProjectDir.FullName : Unreal.EngineDirectory.FullName, SigningCertificate);
			if (!File.Exists(SigningCertificatePath))
			{
				SigningCertificate = @"Platforms\HoloLens\Build\SigningCertificate.pfx";
				SigningCertificatePath = Path.Combine(Receipt.ProjectDir != null ? Receipt.ProjectDir.FullName : Unreal.EngineDirectory.FullName, SigningCertificate);
			}

			string MapFilename = Path.Combine(IntermediateDirectory, OutputName + ".pkgmap");
			var LocalRoot = Receipt.ProjectDir;
			var EngineRoot = Unreal.RootDirectory;
			var AddedFiles = new Dictionary<string, string>();
			bool PackageFileNeedToBeUpdated = !File.Exists(OutputAppX);

			DateTime AppXTime = DateTime.Now;

			PackageFileNeedToBeUpdated = true;

			if(!PackageFileNeedToBeUpdated)
			{
				AppXTime = File.GetLastWriteTimeUtc(OutputAppX);
			}

			{
				foreach (var Product in Receipt.BuildProducts)
				{
					if (Product.Type == BuildProductType.Executable || Product.Type == BuildProductType.DynamicLibrary || Product.Type == BuildProductType.RequiredResource)
					{
						string Filename;
						if(AddedFiles.ContainsKey(Product.Path.FullName))
						{
							continue;
						}

						if (LocalRoot != null && Product.Path.IsUnderDirectory(LocalRoot))
						{
							Filename = Product.Path.MakeRelativeTo(LocalRoot.ParentDirectory!);
						}
						else if(Product.Path.IsUnderDirectory(EngineRoot))
						{
							Filename = Product.Path.MakeRelativeTo(EngineRoot);
						}
						else
						{
							throw new BuildException("Failed to parse target receipt file.  See log for details.");
						}

						AddedFiles.Add(Product.Path.FullName, Filename);
					}
				}

				foreach(var Dep in Receipt.RuntimeDependencies)
				{
					if(Dep.Type == StagedFileType.NonUFS)
					{
						if(AddedFiles.ContainsKey(Dep.Path.FullName))
						{
							continue;
						}

						string Filename;
						if (LocalRoot != null && Dep.Path.IsUnderDirectory(LocalRoot))
						{
							Filename = Dep.Path.MakeRelativeTo(LocalRoot.ParentDirectory!);
						}
						else if (Dep.Path.IsUnderDirectory(EngineRoot))
						{
							Filename = Dep.Path.MakeRelativeTo(EngineRoot);
						}
						else
						{
							throw new BuildException("Failed to parse target receipt file.  See log for details.");
						}

						AddedFiles.Add(Dep.Path.FullName, Filename);
					}
				}
			}

			string ManifestName = String.Format("AppxManifest_{0}.xml", WindowsExports.GetArchitectureSubpath(Architecture));
			AddedFiles.Add(Path.Combine(OutputDirectory, ManifestName), "AppxManifest.xml");

			//manually add resources
			string PriFileName = String.Format("resources_{0}.pri", WindowsExports.GetArchitectureSubpath(Architecture));
			AddedFiles.Add(Path.Combine(OutputDirectory, PriFileName), "resources.pri");
			{
				DirectoryReference ResourceFolder = DirectoryReference.Combine(Receipt.Launch.Directory, WindowsExports.GetArchitectureSubpath(Architecture));
				foreach (var ResourcePath in UpdatedFiles)
				{
					var ResourceFile = new FileReference(ResourcePath);

					if (ResourceFile.IsUnderDirectory(ResourceFolder))
					{
						AddedFiles.Add(ResourceFile.FullName, ResourceFile.MakeRelativeTo(ResourceFolder));
					}
					else
					{
						Logger.LogError("Wrong path to resource \'{Resource}\', the resource should be in \'{Folder}\'", ResourceFile.FullName, ResourceFolder.FullName);
						throw new BuildException("Failed to generate AppX file.  See log for details.");
					}
				}
			}


			FileReference SourceNetworkManifestPath = new FileReference(Path.Combine(OutputDirectory, "NetworkManifest.xml"));
			if (FileReference.Exists(SourceNetworkManifestPath))
			{
				AddedFiles.Add(SourceNetworkManifestPath.FullName, "NetworkManifest.xml");
			}
			FileReference SourceXboxConfigPath = new FileReference(Path.Combine(OutputDirectory, "xboxservices.config"));
			if (FileReference.Exists(SourceXboxConfigPath))
			{
				AddedFiles.Add(SourceXboxConfigPath.FullName, "xboxservices.config");
			}

			do
			{
				if (PackageFileNeedToBeUpdated)
				{
					break;
				}

				if (!File.Exists(MapFilename))
				{
					PackageFileNeedToBeUpdated = true;
					break;
				}
				string[] lines = File.ReadAllLines(MapFilename, Encoding.UTF8);
				int filesCount = 0;

				foreach(var line in lines)
				{
					if (line[0] == '[')
					{
						continue;
					}

					string[] files = line.Split('\t');

					if(files.Length != 2)
					{
						PackageFileNeedToBeUpdated = true;
						break;
					}

					files[0] = files[0].Trim('\"');
					files[1] = files[1].Trim('\"');

					if (!AddedFiles.ContainsKey(files[0]))
					{
						PackageFileNeedToBeUpdated = true;
						break;
					}

					if(AddedFiles[files[0]] != files[1])
					{
						PackageFileNeedToBeUpdated = true;
						break;
					}

					if(File.GetLastWriteTimeUtc(files[0]).CompareTo(AppXTime) >= 0)
					{
						PackageFileNeedToBeUpdated = true;
						break;
					}

					++filesCount;
				}

				if(PackageFileNeedToBeUpdated)
				{
					break;
				}

				if(filesCount != AddedFiles.Count)
				{
					PackageFileNeedToBeUpdated = true;
					break;
				}

				if (File.Exists(SigningCertificatePath) && File.GetLastWriteTimeUtc(SigningCertificatePath).CompareTo(AppXTime) >= 0)
				{
					PackageFileNeedToBeUpdated = true;
					break;
				}
			}
			while(false);

			if(!PackageFileNeedToBeUpdated)
			{
				NewReceipt.BuildProducts.Add(new BuildProduct(new FileReference(OutputAppX), BuildProductType.Package));
				return;
			}

			try
			{
				DeployHelper_DeleteFile(OutputAppX);
			}
			catch (Exception exceptionMessage)
			{
				Logger.LogError("Failed to delete {Output} from deployment: {Ex}", OutputAppX, exceptionMessage);
				throw new BuildException("Failed to generate AppX file.  See log for details.");
			}

			var AppXRecipeBuiltFiles = new StringBuilder();
			AppXRecipeBuiltFiles.AppendLine(@"[Files]");
			foreach (var f in AddedFiles)
			{
				AppXRecipeBuiltFiles.AppendLine(String.Format("\"{0}\"\t\"{1}\"", f.Key, f.Value));
			}
			File.WriteAllText(MapFilename, AppXRecipeBuiltFiles.ToString(), Encoding.UTF8);

			string MakeAppXCommandLine = String.Format("pack /o /f \"{0}\" /p \"{1}\"", MapFilename, OutputAppX);

			var StartInfo = new ProcessStartInfo(MakeAppXPath!.FullName, MakeAppXCommandLine);
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			var ExitCode = Utils.RunLocalProcessAndPrintfOutput(StartInfo, Logger);
			if (ExitCode < 0)
			{
				throw new BuildException("Failed to generate AppX file.  See log for details.");
			}

			if (File.Exists(SigningCertificatePath))
			{
				string SignToolCommandLine = String.Format("sign /a /f \"{0}\" /fd SHA256 \"{1}\"", SigningCertificatePath, OutputAppX);
				StartInfo = new ProcessStartInfo(SignToolPath!.FullName, SignToolCommandLine);
				StartInfo.UseShellExecute = false;
				StartInfo.CreateNoWindow = true;
				ExitCode = Utils.RunLocalProcessAndPrintfOutput(StartInfo, Logger);
				if (ExitCode < 0)
				{
					throw new BuildException("Failed to generate AppX file.  See log for details.");
				}
			}

			Logger.LogInformation("AppX successfully packaged to \'{Output}{Ext}\'", OutputName, Extension);
			NewReceipt.BuildProducts.Add(new BuildProduct(new FileReference(OutputAppX), BuildProductType.Package));
		}


		private void CopyDataAndSymbolsBetweenReceipts(TargetReceipt Receipt, TargetReceipt NewReceipt, WindowsArchitecture Architecture)
		{
			NewReceipt.AdditionalProperties.AddRange(Receipt.AdditionalProperties);
			NewReceipt.AdditionalProperties = NewReceipt.AdditionalProperties.GroupBy(e => e.Name).Select(g => g.First()).ToList();

			NewReceipt.BuildProducts.AddRange(Receipt.BuildProducts.FindAll(x => x.Type == BuildProductType.SymbolFile));
			NewReceipt.BuildProducts = NewReceipt.BuildProducts.GroupBy(e => e.Path).Select(g => g.First()).ToList();

			NewReceipt.RuntimeDependencies.AddRange(Receipt.RuntimeDependencies.FindAll(x => x.Type != StagedFileType.NonUFS));
			NewReceipt.RuntimeDependencies = new RuntimeDependencyList(NewReceipt.RuntimeDependencies.GroupBy(e => e.Path).Select(g => g.First()).ToList());
		}


		public override bool PrepTargetForDeployment(TargetReceipt Receipt)
		{
			// Use the project name if possible - InTarget.AppName changes for 'Client'/'Server' builds
			string ProjectName = Receipt.ProjectFile != null ? Receipt.ProjectFile.GetFileNameWithoutAnyExtensions() : Receipt.Launch!.GetFileNameWithoutExtension();
			Logger.LogInformation("Prepping {Project} for deployment to {Platform}", ProjectName, Receipt.Platform.ToString());
			System.DateTime PrepDeployStartTime = DateTime.UtcNow;

			// Note: TargetReceipt.Read now expands path variables internally.
			TargetReceipt? NewReceipt = null;
			FileReference ReceiptFileName = TargetReceipt.GetDefaultPath(Receipt.ProjectDir != null ? Receipt.ProjectDir : Unreal.EngineDirectory, Receipt.TargetName, Receipt.Platform, Receipt.Configuration, "Multi");
			if (!TargetReceipt.TryRead(ReceiptFileName, Unreal.EngineDirectory, out NewReceipt))
			{
				NewReceipt = new TargetReceipt(Receipt.ProjectFile, Receipt.TargetName, Receipt.TargetType, Receipt.Platform, Receipt.Configuration, Receipt.Version, "Multi", Receipt.IsTestTarget);
			}

			string SDK = "";
			var Results = Receipt.AdditionalProperties.Where(x => x.Name == "SDK");
			if (Results.Any())
			{
				SDK = Results.First().Value;
			}
			HoloLensExports.InitWindowsSdkToolPath(SDK, Logger);

			AddWinMDReferencesFromReceipt(Receipt, Receipt.ProjectDir != null ? Receipt.ProjectDir : Unreal.EngineDirectory, Unreal.EngineDirectory.ParentDirectory!.FullName, SDK);

			//PrepForUATPackageOrDeploy(InTarget.ProjectFile, InAppName, InTarget.ProjectDirectory.FullName, InTarget.OutputPath.FullName, TargetBuildEnvironment.RelativeEnginePath, false, "", false);
			List<UnrealTargetConfiguration> TargetConfigs = new List<UnrealTargetConfiguration> { Receipt.Configuration };
			List<string> ExePaths = new List<string> { Receipt.Launch!.FullName };
			string RelativeEnginePath = Unreal.EngineDirectory.MakeRelativeTo(DirectoryReference.GetCurrentDirectory());

			WindowsArchitecture Arch = WindowsArchitecture.ARM64;
			if (Receipt.Architecture.ToLower() == "x64")
			{
				Arch = WindowsArchitecture.x64;
			}


			string AbsoluteExeDirectory = Path.GetDirectoryName(ExePaths[0])!;
			UnrealTargetPlatform Platform = UnrealTargetPlatform.HoloLens;
			string IntermediateDirectory = Path.Combine(Receipt.ProjectDir != null ? Receipt.ProjectDir.FullName : Unreal.EngineDirectory.FullName, "Intermediate", "Deploy", WindowsExports.GetArchitectureSubpath(Arch));
			List<string> UpdatedFiles = new HoloLensManifestGenerator(Logger).CreateManifest(Platform, Arch, AbsoluteExeDirectory, IntermediateDirectory, Receipt.ProjectFile, Receipt.ProjectDir != null ? Receipt.ProjectDir.FullName : Unreal.EngineDirectory.FullName, TargetConfigs, ExePaths, WinMDReferences)!;

			PrepForUATPackageOrDeploy(Receipt.ProjectFile, ProjectName, Receipt.ProjectDir != null ? Receipt.ProjectDir.FullName : Unreal.EngineDirectory.FullName, Arch, TargetConfigs, ExePaths, RelativeEnginePath, false, "", false);
			MakePackage(Receipt, NewReceipt, Arch, UpdatedFiles);
			CopyDataAndSymbolsBetweenReceipts(Receipt, NewReceipt, Arch);

			NewReceipt.Write(ReceiptFileName, Unreal.EngineDirectory);

			// Log out the time taken to deploy...
			double PrepDeployDuration = (DateTime.UtcNow - PrepDeployStartTime).TotalSeconds;
			Logger.LogInformation("HoloLens deployment preparation took {Time:0.00} seconds", PrepDeployDuration);

			return true;
		}

		public void AddWinMDReferencesFromReceipt(TargetReceipt Receipt, DirectoryReference SourceProjectDir, string DestRelativeTo, string SDKVersion)
		{
			// Dependency paths in receipt are already expanded at this point
			foreach (var Dep in Receipt.RuntimeDependencies)
			{
				if (Dep.Path.GetExtension() == ".dll")
				{
					string SourcePath = Dep.Path.FullName;
					string WinMDFile = Path.ChangeExtension(SourcePath, "winmd");
					if (File.Exists(WinMDFile))
					{
						string DestPath = Dep.Path.FullName;
						DestPath = Dep.Path.FullName.Replace(Unreal.EngineDirectory.FullName, Path.Combine(DestRelativeTo, "Engine"));
						DestPath = DestPath.Replace(SourceProjectDir.FullName, Path.Combine(DestRelativeTo, SourceProjectDir.GetDirectoryName()));
						DestPath = Utils.MakePathRelativeTo(DestPath, DestRelativeTo);
						WinMDReferences.Add(new WinMDRegistrationInfo(new FileReference(WinMDFile), DestPath, SDKVersion, Logger));
					}
				}
			}
		}

		private List<WinMDRegistrationInfo> WinMDReferences = new List<WinMDRegistrationInfo>();
	}
}
