// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Linq;
using System.Xml;
using System.Xml.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{

	/// <summary>
	/// Visual Studio for Mac project file generator implementation
	/// </summary>
	class VCMacProjectFileGenerator : VCProjectFileGenerator
	{

		/// <summary>
		/// Default constructor
		/// </summary>
		/// <param name="InOnlyGameProject">The single project to generate project files for, or null</param>
		/// <param name="InArguments">Additional command line arguments</param>
		public VCMacProjectFileGenerator(FileReference? InOnlyGameProject, CommandLineArguments InArguments)
			: base(InOnlyGameProject, VCProjectFileFormat.Default, InArguments)
		{
			// no suo file, requires ole32
			Settings.bWriteSolutionOptionFile = false;
		}

		/// True if we should include IntelliSense data in the generated project files when possible
		public override bool ShouldGenerateIntelliSenseData()
		{
			return false;
		}

		protected bool IsValidProject(ProjectFile ProjectFile)
		{
			XmlDocument Doc = new XmlDocument();
			Doc.Load(ProjectFile.ProjectFilePath.FullName);

			XmlNodeList Elements = Doc.GetElementsByTagName("TargetFramework");
			foreach (XmlElement? Element in Elements)
			{
				if (Element == null)
				{
					continue;
				}

				// some projects can have TargetFramework's nested in conditionals for IsWindows, etc. Rather than try to handle that,
				// we look if _any_ TargetFramework doesn't contain Windows and assume that means the conditionals are set up as expected
				if (!Element.InnerText.Contains("windows"))
				{
					return true;
				}
			}

			// if there was no framework detected without windows, then this is not valid
			return false;
		}

		/// <summary>
		/// Writes the project files to disk
		/// </summary>
		/// <returns>True if successful</returns>
		protected override bool WriteProjectFiles(PlatformProjectGeneratorCollection PlatformProjectGenerators, ILogger Logger)
		{
			// This can be reset by higher level code when it detects that we don't have
			// VS2022 installed (TODO - add custom format for Mac?)
			Settings.ProjectFileFormat = VCProjectFileFormat.VisualStudio2022;

			// we can't generate native projects so clear them here, we will just
			// write out OtherProjectFiles and AutomationProjectFiles
			GeneratedProjectFiles.Clear();

			// remove C# projects that require windows (see BuildAllScriptPlugins() looking at TargetFramework)
			OtherProjectFiles.RemoveAll(x => !IsValidProject(x));
			AutomationProjectFiles.RemoveAll(x => !IsValidProject(x));

			if (!base.WriteProjectFiles(PlatformProjectGenerators, Logger))
			{
				return false;
			}

			// Write AutomationReferences file
			if (AutomationProjectFiles.Any())
			{
				XNamespace NS = XNamespace.Get("http://schemas.microsoft.com/developer/msbuild/2003");

				DirectoryReference AutomationToolDir = DirectoryReference.Combine(Unreal.EngineSourceDirectory, "Programs", "AutomationTool");
				DirectoryReference AutomationToolBinariesDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Binaries", "DotNET", "AutomationTool");
				new XDocument(
					new XElement(NS + "Project",
						new XAttribute("ToolsVersion", VCProjectFileGenerator.GetProjectFileToolVersionString(Settings.ProjectFileFormat)),
						new XAttribute("DefaultTargets", "Build"),
						new XElement(NS + "ItemGroup",
							from AutomationProject in AutomationProjectFiles
							select new XElement(NS + "ProjectReference",
								new XAttribute("Include", AutomationProject.ProjectFilePath.MakeRelativeTo(AutomationToolDir)),
								new XElement(NS + "Project", (AutomationProject as VCSharpProjectFile)!.ProjectGUID.ToString("B")),
								new XElement(NS + "Name", AutomationProject.ProjectFilePath.GetFileNameWithoutExtension()),
								new XElement(NS + "Private", "false")
							)
						),
						// Delete the private copied dlls in case they were ever next to the .exe - that is a bad place for them
						new XElement(NS + "Target",
							new XAttribute("Name", "CleanUpStaleDlls"),
							new XAttribute("AfterTargets", "Build"),
							AutomationProjectFiles.SelectMany(AutomationProject => {
									string BaseFilename = FileReference.Combine(AutomationToolBinariesDir, AutomationProject.ProjectFilePath.GetFileNameWithoutExtension()).FullName;
									return new List<XElement>() {
										new XElement(NS + "Delete",	new XAttribute("Files", BaseFilename + ".dll")),
										new XElement(NS + "Delete",	new XAttribute("Files", BaseFilename + ".dll.config")),
										new XElement(NS + "Delete",	new XAttribute("Files", BaseFilename + ".pdb"))
									};
								}
							)
						)
					)
				).Save(FileReference.Combine(IntermediateProjectFilesPath, "AutomationTool.csproj.References").FullName);
			}

			return true;
		}
	}
}
