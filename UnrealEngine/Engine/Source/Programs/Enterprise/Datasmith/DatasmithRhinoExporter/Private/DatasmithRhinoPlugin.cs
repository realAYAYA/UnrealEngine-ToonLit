// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.DirectLink;
using DatasmithRhino.Utils;
using Rhino;
using Rhino.PlugIns;
using System;

#if MAC_OS
using RhinoMac;
#endif

namespace DatasmithRhino
{
	///<summary>
	/// <para>Every RhinoCommon .rhp assembly must have one and only one PlugIn-derived
	/// class. DO NOT create instances of this class yourself. It is the
	/// responsibility of Rhino to create an instance of this class.</para>
	/// <para>To complete plug-in information, please also see all PlugInDescription
	/// attributes in AssemblyInfo.cs (you might need to click "Project" ->
	/// "Show All Files" to see it in the "Solution Explorer" window).</para>
	///</summary>
	public abstract class DatasmithRhinoPlugin : FileExportPlugIn
	{
		public override PlugInLoadTime LoadTime { get { return PlugInLoadTime.AtStartup; } }
		public DatasmithRhinoDirectLinkManager DirectLinkManager { get; private set; }
		public DatasmithRhinoLogManager LogManager { get; private set; } = new DatasmithRhinoLogManager();

		public DatasmithRhinoPlugin()
		{
			Instance = this;
			RhinoApp.Closing += OnProcessExit;

			//Initialize DirectLink framework.
			DirectLinkManager = new DatasmithRhinoDirectLinkManager();
			DirectLinkManager.Initialize();

			int LangageIdentifier = Rhino.ApplicationSettings.AppearanceSettings.LanguageIdentifier;
			System.Globalization.CultureInfo Culture = new System.Globalization.CultureInfo(LangageIdentifier);
			System.Globalization.CultureInfo.DefaultThreadCurrentCulture = Culture;
			System.Globalization.CultureInfo.DefaultThreadCurrentUICulture = Culture;
		}

		///<summary>Gets the only instance of the DatasmithRhino plug-in.</summary>
		public static DatasmithRhinoPlugin Instance {
			get; private set;
		}

		/// <summary>Defines file extensions that this export plug-in is designed to write.</summary>
		/// <param name="options">Options that specify how to write files.</param>
		/// <returns>A list of file types that can be exported.</returns>
		protected override FileTypeList AddFileTypes(Rhino.FileIO.FileWriteOptions options)
		{
			var result = new FileTypeList();
			result.AddFileType("Unreal Datasmith (*.udatasmith)", "udatasmith");
			return result;
		}

#if MAC_OS
		protected override LoadReturnCode OnLoad(ref string errorMessage)
		{
			string PluginPath = System.IO.Path.GetDirectoryName(Assembly.Location);
			string ResourcesPath = System.IO.Path.Combine(PluginPath, "Resources");
			string PListPath = System.IO.Path.Combine(ResourcesPath, "DatasmithRhino.plist");
			
			bool bLoaded = RhinoMac.Runtime.MacPlatformService.LoadToolPaletteCollection(PListPath);
			if (!bLoaded)
			{
				System.Diagnostics.Debug.WriteLine("WARNING: Failed to load tool palette.");
			}
					
			return base.OnLoad(ref errorMessage);
		}
#endif

		/// <summary>
		/// Is called when a user requests to export a ".udatasmith" file.
		/// It is actually up to this method to write the file itself.
		/// </summary>
		/// <param name="Filename">The complete path to the new file.</param>
		/// <param name="Index">The index of the file type as it had been specified by the AddFileTypes method.</param>
		/// <param name="RhinoDocument">The document to be written.</param>
		/// <param name="Options">Options that specify how to write file.</param>
		/// <returns>A value that defines success or a specific failure.</returns>
		protected override WriteFileResult WriteFile(string Filename, int Index, RhinoDoc RhinoDocument, Rhino.FileIO.FileWriteOptions Options)
		{
			const bool bSkipHidden = true;
			FDatasmithFacadeScene DatasmithScene = DatasmithRhinoSceneExporter.CreateDatasmithScene(Filename, RhinoDocument);
			DatasmithRhinoExportOptions ExportOptions = new DatasmithRhinoExportOptions(Options, RhinoDocument, DatasmithScene, bSkipHidden);
			return DatasmithRhinoSceneExporter.ExportToFile(ExportOptions);
		}

		public void OnProcessExit(object Sender, EventArgs Args)
		{
			RhinoApp.Closing -= OnProcessExit;

			DirectLinkManager.ShutDown();

			// If we are not on Windows, we need to manually call FDatasmithFacadeScene.Shutdown() when the process ends.
			if (Environment.OSVersion.Platform != PlatformID.Win32NT)
			{
				FDatasmithFacadeScene.Shutdown();
			}
		}
	}
}
