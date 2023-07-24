// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.ExportContext;
using Rhino;
using System;
using System.IO;

namespace DatasmithRhino.DirectLink
{
	public class DatasmithRhinoDirectLinkManager
	{
		private const string UntitledSceneName = "Untitled";

		public FDatasmithFacadeDirectLink DirectLink { get; private set; } = null;
		public FDatasmithFacadeScene DatasmithScene { get; private set; } = null;
		public DatasmithRhinoExportContext ExportContext { get; private set; } = null;
		public DatasmithRhinoChangeListener ChangeListener { get; private set; } = new DatasmithRhinoChangeListener();
		public bool bInitialized { get; private set; } = false;
		public bool bAutoSyncActive { get; private set; } = false;
		// Slate UI is not available on Mac, we can't use the Connection UI and must provide this feature ourselves.
		private string InternalCacheDirectory = Path.GetTempPath();
		public string CacheDirectory
		{
			get
			{
// Slate UI is not available on Mac, do not try to access it.
#if !MAC_OS
				IDirectLinkUI DirectLinkUI = IDatasmithExporterUIModule.Get()?.GetDirectLinkExporterUI();

				if (DirectLinkUI != null)
				{
					return DirectLinkUI.GetDirectLinkCacheDirectory();
				}
				else
#endif //!MAC_OC
				{
					return InternalCacheDirectory;
				}
			}
			set
			{
				InternalCacheDirectory = value;
			}
		}


		public void Initialize()
		{
			if (bInitialized)
			{
				System.Diagnostics.Debug.Fail("Calling DatasmithRhinoDirectLinkManager::Initialize() when DirectLink is already initialized.");
				return;
			}

			if (Environment.OSVersion.Platform == PlatformID.Win32NT)
			{
				string RhinoEngineDir = GetEngineDirWindows();
				if (Directory.Exists(RhinoEngineDir))
				{
					bInitialized = FDatasmithFacadeDirectLink.Init(true, RhinoEngineDir);
				}
				else
				{
					System.Diagnostics.Debug.Fail("Could not initialize FDatasmithFacadeDirectLink because of missing Engine resources");
				}
			}
			else
			{
				//Mac platform, the DatasmithExporter Slate UI is not supported for now on this platform. Simply initialize DirectLink without it.
				bInitialized = FDatasmithFacadeDirectLink.Init();
			}

			if (bInitialized)
			{
				RhinoDoc.EndOpenDocument += OnEndOpenDocument;
				RhinoDoc.BeginOpenDocument += OnBeginOpenDocument;
				RhinoDoc.NewDocument += OnNewDocument;

				if (RhinoDoc.ActiveDoc is RhinoDoc ActiveDocument && ActiveDocument.IsAvailable)
				{
					// If the document is not null that means the plugin was loaded after startup, most likely on first launch.
					// Since the doc is already available the Document update have already been fired and we need to directly setup the DirectLink source.
					SetupDirectLinkScene(ActiveDocument);
				}
			}

			System.Diagnostics.Debug.Assert(bInitialized);
		}

		public void ShutDown()
		{
			if (bInitialized)
			{
				RhinoDoc.EndOpenDocument -= OnEndOpenDocument;
				RhinoDoc.BeginOpenDocument -= OnBeginOpenDocument;
				RhinoDoc.NewDocument -= OnNewDocument;
				SetLiveLink(false);

				FDatasmithFacadeDirectLink.Shutdown();
				bInitialized = false;
			}
		}

		public Rhino.Commands.Result Synchronize(RhinoDoc RhinoDocument)
		{
			Rhino.Commands.Result ExportResult = Rhino.Commands.Result.Failure;
			bool bIsValidContext = ExportContext != null && RhinoDocument == ExportContext.RhinoDocument;

			if (bIsValidContext || SetupDirectLinkScene(RhinoDocument))
			{
				ExportResult = DatasmithRhinoSceneExporter.ExportScene(DatasmithScene, ExportContext, DirectLink.UpdateScene);
				ChangeListener.StartListening(ExportContext);
			}

			return ExportResult;
		}

		public bool OpenConnectionManangementWindow()
		{
			if (bInitialized && DirectLink != null)
			{
				IDirectLinkUI DirectLinkUI = IDatasmithExporterUIModule.Get()?.GetDirectLinkExporterUI();

				if (DirectLinkUI != null)
				{
					DirectLinkUI.OpenDirectLinkStreamWindow();
					return true;
				}
			}

			return false;
		}

		public Rhino.Commands.Result SetLiveLink(bool bActive)
		{
			if (bActive && !bAutoSyncActive)
			{
				if (ExportContext.RhinoDocument != null)
				{
					RhinoApp.Idle += OnRhinoIdle;
					bAutoSyncActive = true;

					if (!ExportContext.bExportedOnce)
					{
						// Make sure the first (longest) update is done on the spot.
						// Not needed if the scene has already been synced.
						return Synchronize(ExportContext.RhinoDocument);
					}
				}
				else
				{
					return Rhino.Commands.Result.Failure;
				}
			}
			else if(!bActive && bAutoSyncActive)
			{
				RhinoApp.Idle -= OnRhinoIdle;
				bAutoSyncActive = false;
			}

			return Rhino.Commands.Result.Success;
		}

		private void OnRhinoIdle(object Sender, EventArgs e)
		{
			if (ExportContext.RhinoDocument != null && ExportContext.bIsDirty)
			{
				Synchronize(ExportContext.RhinoDocument);
			}
		}

		private string GetEngineDirWindows()
		{
			string RhinoEngineDir = null;

			try
			{
				using (Microsoft.Win32.RegistryKey Key = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(@"Software\Wow6432Node\EpicGames\Unreal Engine"))
				{
					RhinoEngineDir = Key?.GetValue("RhinoEngineDir") as string;
				}
			}
			finally
			{
				if (RhinoEngineDir == null)
				{
					// If we could not read the registry, fallback to hardcoded engine dir
					RhinoEngineDir = @"C:\ProgramData\Epic\Exporter\RhinoEngine\";
				}
			}

			return RhinoEngineDir;
		}

		private void OnBeginOpenDocument(object Sender, DocumentOpenEventArgs Args)
		{
			if (!Args.Merge && !Args.Reference)
			{
				// Before opening a new document we need to stop listening to changes in the scene.
				// Otherwise we'll be updating cache of the old scene with the new document.
				ChangeListener.StopListening();
				SetLiveLink(false);
			}
		}

		private void OnEndOpenDocument(object Sender, DocumentOpenEventArgs Args)
		{
			if (!Args.Merge && !Args.Reference)
			{
				SetupDirectLinkScene(Args.Document, Args.FileName);
			}
		}

		private void OnNewDocument(object Sender, DocumentEventArgs Args)
		{
			if (Args.Document != null)
			{
				SetupDirectLinkScene(Args.Document);
				ChangeListener.StopListening();
				SetLiveLink(false);
			}
		}

		private bool SetupDirectLinkScene(RhinoDoc RhinoDocument, string FilePath = null)
		{
			//Override all of the existing scene export data, we are exporting a new document.
			try
			{
				string SceneFileName = GetSceneExportFilePath(RhinoDocument, FilePath);
				DatasmithScene = DatasmithRhinoSceneExporter.CreateDatasmithScene(SceneFileName, RhinoDocument);

				if (DirectLink == null)
				{
					DirectLink = new FDatasmithFacadeDirectLink();
				}
				DirectLink.InitializeForScene(DatasmithScene);

				const bool bSkipHidden = false;
				DatasmithRhinoExportOptions ExportOptions = new DatasmithRhinoExportOptions(RhinoDocument, DatasmithScene, bSkipHidden);
				ExportContext = new DatasmithRhinoExportContext(ExportOptions);
			}
			catch (Exception)
			{
				return false;
			}

			return true;
		}

		private string GetSceneExportFilePath(RhinoDoc RhinoDocument, string OptionalFilePath = null)
		{
			string SceneName;
			if (!string.IsNullOrEmpty(RhinoDocument.Name))
			{
				SceneName = RhinoDocument.Name;
			}
			else if (!string.IsNullOrEmpty(OptionalFilePath))
			{
				SceneName = Path.GetFileNameWithoutExtension(OptionalFilePath);
			}
			else
			{
				SceneName = UntitledSceneName;
			}

			return Path.Combine(CacheDirectory, SceneName);
		}
	}
}