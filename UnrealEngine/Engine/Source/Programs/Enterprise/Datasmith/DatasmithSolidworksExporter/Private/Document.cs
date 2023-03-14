// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading;

namespace DatasmithSolidworks
{
	/**
	 * Base document class.
	 */
	public abstract class FDocument
	{
		public ModelDoc2 SwDoc = null;
		public bool bDirectLinkAutoSync { get; set; } = false;
		public int DirectLinkSyncCount { get; private set; } = 0;
		public ConcurrentDictionary<int, FMaterial> ExportedMaterialsMap = new ConcurrentDictionary<int, FMaterial>();
		public bool bHasConfigurations = false;
		
		protected FDatasmithFacadeScene DatasmithScene = null;
		protected FDatasmithExporter Exporter = null;
		protected int DocId = -1;
		protected bool bDirectLinkSyncInProgress = false;
		protected bool bFileExportInProgress = false;

		private string DirectLinkPath = "";
		private FDatasmithFacadeDirectLink DatasmithDirectLink = null;
		private bool bDocumentIsDirty = true;
		private string DatasmithFileExportPath = "";
		private Thread MaterialCheckerThread = null;
		private ManualResetEvent MaterialCheckerEvent = null;
		private bool bExitMaterialUpdateThread = false;
		private uint FaceCounter = 1; // Face Id generator


		public FDocument(int InDocId, ModelDoc2 InSwDoc, FDatasmithExporter InExporter)
		{
			DocId = InDocId;
			SwDoc = InSwDoc;
			DatasmithScene = new FDatasmithFacadeScene("StdMaterial", "Solidworks", "Solidworks", "");
			DatasmithScene.SetName(InSwDoc.GetTitle());

			DirectLinkPath = Path.Combine(Path.GetTempPath(), "sw_dl_" + Guid.NewGuid().ToString());

			if (!Directory.Exists(DirectLinkPath))
			{
				Directory.CreateDirectory(DirectLinkPath);
			}

			if (InExporter != null)
			{
				Exporter = InExporter;
			}
			else
			{
				Exporter = new FDatasmithExporter(DatasmithScene);
			}
		}

		public static bool IsValidFaceId(uint InFaceId)
		{
			return (InFaceId >> 24 == 0xAA);
		}

		public uint GetFaceId(IFace2 InFace)
		{
			uint FaceId = unchecked((uint)InFace.GetFaceId());
			if (!IsValidFaceId(FaceId))
			{
				uint Count = FaceCounter++;
				FaceId = unchecked((uint)(0xAA << 24)) | Count;
				InFace.SetFaceId((int)FaceId);
			}
			return FaceId;
		}

		protected bool GetDirty()
		{
			return bDocumentIsDirty;
		}

		protected virtual void SetDirty(bool bInDirty)
		{
			bDocumentIsDirty = bInDirty;
		}

		protected void SetExportStatus(string InMessage)
		{
			if (bDirectLinkSyncInProgress)
			{
				Addin.Instance.LogStatusBarMessage($"DirectLink sync...{InMessage}");
			}
			else if (bFileExportInProgress)
			{
				string FileName = Path.GetFileName(DatasmithFileExportPath);
				Addin.Instance.LogStatusBarMessage($"Exporting file {FileName}...{InMessage}");
			}
		}

		private void ExportConfigurations(List<FConfigurationData> Configs)
		{
			// Remove any existing Datasmith LevelVariantSets as we'll re-add data
			while (DatasmithScene.GetLevelVariantSetsCount() > 0)
			{
				for (int Index = 0; Index < DatasmithScene.GetLevelVariantSetsCount(); Index++)
				{
					DatasmithScene.RemoveLevelVariantSets(DatasmithScene.GetLevelVariantSets(Index));
				}
			}

			Exporter.ExportMaterials(ExportedMaterialsMap);

			if (Configs != null)
			{
				Exporter.ExportLevelVariantSets(Configs);
			}
		}

		private void CheckForMaterialUpdatesProc()
		{
			while (!bExitMaterialUpdateThread)
			{
				MaterialCheckerEvent.WaitOne();

				// Only check for material updates when we are not current exporting
				if (!bDirectLinkSyncInProgress && !bFileExportInProgress)
				{
					bool bSketchMode = false;
					try
					{
						bSketchMode = (SwDoc.SketchManager?.ActiveSketch != null);
					}
					catch { }
				
					if (!bSketchMode && HasMaterialUpdates())
					{
						SetDirty(true);
					}
				}
				Thread.Sleep(600);
			}
		}

		private void ExportLights()
		{
			List<FLight> Lights  = FLightExporter.ExportLights(SwDoc);

			foreach (FLight Light in Lights)
			{
				Exporter.ExportLight(Light);
			}
		}

		private void RunExport(bool bIsDirectLinkExport)
		{
			if (bIsDirectLinkExport)
			{
				// DirectLink sync

				bDirectLinkSyncInProgress = true;

				DatasmithScene.SetName(SwDoc.GetTitle());
				DatasmithScene.SetOutputPath(DirectLinkPath);

#if DEBUG
				Stopwatch Watch = Stopwatch.StartNew();
#endif
				FMeshes Meshes = new FMeshes();
				
				PreExport(Meshes, false);
				ExportToDatasmithScene(Meshes);

				ExportLights();

#if DEBUG
				Watch.Stop();
				Debug.WriteLine($"EXPORT TIME: {(double)Watch.ElapsedMilliseconds / 1000.0}");
#endif

				DatasmithDirectLink.UpdateScene(DatasmithScene);
			}
			else
			{
				// Export to file

				bFileExportInProgress = true;


				string OutDir = Path.GetDirectoryName(DatasmithFileExportPath);
				string CleanFileName = Path.GetFileNameWithoutExtension(DatasmithFileExportPath);

				// Save/restore scene and exporter in order not to mess up DirectLink state
				FDatasmithFacadeScene OldScene = DatasmithScene;
				FDatasmithExporter OldExporter = Exporter;

				DatasmithScene = new FDatasmithFacadeScene("StdMaterial", "Solidworks", "Solidworks", "");
				DatasmithScene.SetName(CleanFileName);
				DatasmithScene.SetOutputPath(OutDir);

				Exporter = new FDatasmithExporter(DatasmithScene);

				FMeshes Meshes = new FMeshes();

				PreExport(Meshes, true);

				List<FConfigurationData> Configs = new FConfigurationExporter(Meshes).ExportConfigurations(this);
				bHasConfigurations = (Configs != null) && (Configs.Count != 0);

				ExportToDatasmithScene(Meshes);
			
				ExportLights();
				ExportConfigurations(Configs);

				DatasmithScene.PreExport();
				DatasmithScene.ExportScene(DatasmithFileExportPath);

				DatasmithScene = OldScene;
				Exporter = OldExporter;
			}

			SetExportStatus("Done");

			SetDirty(false);

			bDirectLinkSyncInProgress = false;
			bFileExportInProgress = false;

			// Kickoff material checker thread after first export
			if (MaterialCheckerThread == null)
			{
				MaterialCheckerThread = new Thread(CheckForMaterialUpdatesProc);
				MaterialCheckerEvent = new ManualResetEvent(false);
				MaterialCheckerThread.Start();
				MaterialCheckerEvent.Set();
			}
		}

		public abstract bool HasMaterialUpdates();
		public abstract void ExportToDatasmithScene(FMeshes Meshes);
		public abstract void PreExport(FMeshes Meshes, bool bConfigurations);  // Called before configurations are parsed to prepare meshes needed to identify if they create different configurations

		public virtual void Init()
		{
		}

		public virtual void Destroy()
		{
			bExitMaterialUpdateThread = true;

			if (MaterialCheckerThread != null && !MaterialCheckerThread.Join(500))
			{
				MaterialCheckerThread.Abort();
			}
		}

		public string GetPathName()
		{
			return SwDoc.GetPathName();
		}

		public void MakeActive(bool bInActive)
		{
			if (bInActive)
			{
				if (DatasmithDirectLink == null)
				{
					DatasmithDirectLink = new FDatasmithFacadeDirectLink();
					if (!DatasmithDirectLink.InitializeForScene(DatasmithScene))
					{
						throw new Exception("DirectLink: failed to initialize");
					}
				}

				MaterialCheckerEvent?.Set();
			}
			else
			{
				// Suspend material checker
				MaterialCheckerEvent?.Reset();

				DatasmithDirectLink?.Dispose();
				DatasmithDirectLink = null;
			}
		}

		public void OnExportToFile(string InFilePath)
		{
			DatasmithFileExportPath = InFilePath;
			RunExport(false);
		}

		public void OnDirectLinkSync()
		{
			if (SwDoc.SketchManager.ActiveSketch != null)
			{
				// Do not run direct link in sketch mode! 
				// This will falsely detect meshes as deleted.
				return;
			}

			DirectLinkSyncCount++;
			RunExport(true);
		}

		public void OnIdle()
		{
			bool bSceneIsDirty = GetDirty();
			if (bSceneIsDirty && bDirectLinkAutoSync)
			{
				OnDirectLinkSync();
			}
		}
	}
}