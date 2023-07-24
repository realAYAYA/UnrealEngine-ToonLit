// Copyright Epic Games, Inc. All Rights Reserved.

using SolidWorks.Interop.sldworks;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using DatasmithSolidworks.Names;
using static DatasmithSolidworks.FAssemblyDocumentTracker;

namespace DatasmithSolidworks
{
	// Checks for material updates periodically, using a separate thread to execute checks
	// todo: Solidworks API is not concurrent-friendly - https://forum.solidworks.com/thread/43129
	//   need to test but it's probably faster to check material updates simply in Sync. Maybe 
	//   with the same period as currently if needed so
	public class FMaterialUpdateChecker
	{
		public Thread MaterialCheckerThread = null;
		public bool bExitMaterialUpdateThread = false;
		public ManualResetEvent MaterialCheckerEvent = null;

		public FDocumentTracker DocumentTracker;

		public FMaterialUpdateChecker(FDocumentTracker InDocumentTracker)
		{
			DocumentTracker = InDocumentTracker;

			MaterialCheckerThread = new Thread(CheckForMaterialUpdatesProc);
			MaterialCheckerEvent = new ManualResetEvent(false);
			MaterialCheckerThread.Start();
			MaterialCheckerEvent.Set();
		}

		public void Destroy()
		{
			bExitMaterialUpdateThread = true;

			if (MaterialCheckerThread != null && !MaterialCheckerThread.Join(500))
			{
				MaterialCheckerThread.Abort();
			}
		}

		public void Resume()
		{
			MaterialCheckerEvent.Set();
		}

		public void Pause()
		{
			MaterialCheckerEvent.Reset();
		}

		private void CheckForMaterialUpdatesProc()
		{
			while (!bExitMaterialUpdateThread)
			{
				MaterialCheckerEvent.WaitOne();

				// Only check for material updates when we are not current exporting
				if (!DocumentTracker.IsUpdateInProgress())
				{
					bool bSketchMode = false;
					try
					{
						bSketchMode = (DocumentTracker.SwDoc.SketchManager?.ActiveSketch != null);
					}
					catch { }
				
					if (!bSketchMode && DocumentTracker.HasMaterialUpdates())
					{
						DocumentTracker.SetDirty(true);
					}
				}
				Thread.Sleep(600);
			}
		}
	}

	// Build DatasmithScene from Solidworks Document, supporting scene changes/modifications
	// Doesn't incorporate change notifications callbacks/handlers(i.e. notification handlers call into this class to tell what was changed and it updates Datasmith scene accordingly)
	public abstract class FDocumentTracker
	{
		public readonly FDocument Doc;  // Top Document that is tracked/exported
		public ModelDoc2 SwDoc => Doc.SwDoc;

		public ConcurrentDictionary<int, FMaterial> ExportedMaterialsMap = new ConcurrentDictionary<int, FMaterial>();
		public bool bHasConfigurations = false;

		public FDatasmithFacadeScene DatasmithScene = null;
		public FDatasmithExporter Exporter = null;

		public bool bDocumentIsDirty = true;
		public string DatasmithFileExportPath = "";

		public uint FaceCounter = 1; // Face Id generator

		protected FDocumentTracker(FDocument InDoc, FDatasmithExporter InExporter)
		{
			Doc = InDoc;

			DatasmithScene = new FDatasmithFacadeScene("StdMaterial", "Solidworks", "Solidworks", "");
			DatasmithScene.SetName(InDoc.SwDoc.GetTitle());

			Exporter = InExporter ?? new FDatasmithExporter(DatasmithScene);
		}

		public string GetPathName()
		{
			return SwDoc.GetPathName();
		}

		public bool GetDirty()
		{
			return bDocumentIsDirty;
		}

		public virtual void SetDirty(bool bInDirty)
		{
			bDocumentIsDirty = bInDirty;
		}

		public abstract bool HasMaterialUpdates();

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

		public static bool IsValidFaceId(uint InFaceId)
		{
			return (InFaceId >> 24 == 0xAA);
		}

		public bool IsUpdateInProgress()
		{
			return Doc.bDirectLinkSyncInProgress || Doc.bFileExportInProgress;
		}

		public void SetExportStatus(string InMessage)
		{
			Doc.SetExportStatus(InMessage);
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

		private void ExportLights()
		{
			List<FLight> Lights  = FLightExporter.ExportLights(SwDoc);

			foreach (FLight Light in Lights)
			{
				Exporter.ExportLight(Light);
			}
		}

		public void Export()
		{
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

			ConfigurationManager ConfigManager = SwDoc.ConfigurationManager;
			string[] ConfigurationNames = SwDoc?.GetConfigurationNames();
			FConfigurationExporter ConfigurationExporter = new FConfigurationExporter(Meshes, ConfigurationNames, ConfigManager.ActiveConfiguration.Name);

			List<FConfigurationData> Configs = ConfigurationExporter.ExportConfigurations(this);

			bHasConfigurations = (Configs != null) && (Configs.Count != 0);

			ExportToDatasmithScene(ConfigurationExporter);

			ExportLights();
			ExportConfigurations(Configs);

			DatasmithScene.PreExport();
			DatasmithScene.ExportScene(DatasmithFileExportPath);

			DatasmithScene = OldScene;
			Exporter = OldExporter;
		}

		public void Sync(string InOutputPath)
		{
			DatasmithScene.SetName(SwDoc.GetTitle());
			DatasmithScene.SetOutputPath(InOutputPath);

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
		}

		// todo: make abstract
		public virtual void ExportToDatasmithScene(FConfigurationExporter ConfigurationExporter)
		{
			throw new NotImplementedException();
		}

		public abstract void ExportToDatasmithScene(FMeshes Meshes);
		public abstract void PreExport(FMeshes Meshes, bool bConfigurations);  // Called before configurations are parsed to prepare meshes needed to identify if they create different configurations

		public virtual bool NeedExportComponent(FConfigurationTree.FComponentTreeNode InComponent,
			FConfigurationTree.FComponentConfig ActiveComponentConfig)
		{
			return true;
		}

		public virtual void AddPartDocument(FConfigurationTree.FComponentTreeNode InNode){}

		public virtual void AddExportedComponent(FConfigurationTree.FComponentTreeNode InNode) {}

		public abstract ConcurrentDictionary<FComponentName, FObjectMaterials> LoadDocumentMaterials(HashSet<FComponentName> ComponentNamesToExportSet);
		public abstract void AddComponentMaterials(FComponentName ComponentName, FObjectMaterials Materials);
		public abstract FObjectMaterials GetComponentMaterials(Component2 Comp);

		public abstract void AddMeshForComponent(FComponentName ComponentName, FMeshName MeshName);

		public virtual void Destroy()
		{
		}

		public abstract FMeshData ExtractComponentMeshData(Component2 Comp);

		// Extracts, exports meshes used for the assembly configuration, assigns them to actors
		// todo: separate just Datasmith Mesh export to its own thread, all other code should be in single thread
		// Datasmith actor assignment doesn't need threading(and not supposed to be thread-safe?) and Solidworks multithreading is supposed to be slower(SW does all the work in main thread)
		public void ProcessConfigurationMeshes(FMeshes.FConfiguration MeshesConfiguration, string MeshSuffix)
		{
			
			List<FDatasmithExporter.FMeshExportInfo> MeshExportInfos = new List<FDatasmithExporter.FMeshExportInfo>();

			// Extract meshes data and prepare for parallel datasmith export
			foreach (Component2 Comp in MeshesConfiguration.EnumerateComponents())
			{
				FMeshData MeshData = ExtractComponentMeshData(Comp);
				MeshesConfiguration.AddMesh(Comp, MeshData);

				if (MeshData != null)
				{

					FActorName ComponentActorName = Exporter.GetComponentActorName(Comp);
					FComponentName ComponentName = new FComponentName(Comp);
					MeshExportInfos.Add(new FDatasmithExporter.FMeshExportInfo()
					{
						ComponentName = ComponentName,
						MeshName = FMeshName.FromString(MeshSuffix == null
							? $"{ComponentName}_Mesh"
							: $"{ComponentName}_{MeshSuffix}_Mesh"),
						ActorName = ComponentActorName,
						MeshData = MeshData
					});
				}
			}

			Exporter.ExportMeshes(MeshExportInfos, out List<FDatasmithExporter.FMeshExportInfo> CreatedMeshes);

			foreach (FDatasmithExporter.FMeshExportInfo Info in CreatedMeshes)
			{
				AddMeshForComponent(Info.ComponentName, Info.MeshName);  // Register that this mesh was used for the component
			}
		}
	};


	// Controls Syncing of a tracked Document, handling all events
	public interface IDocumentSyncer
	{
		// Make Datasmith Scene up to date with the Solidworks Document
		void Sync(string InOutputPath);

		// Indicate that change handling should start(after first Sync)
		void Start();

		// Resume all change tracking(after document was made active, foreground in Solidworks)
		void Resume();

		// Pause all change tracking(when document deactivated, background)
		void Pause();

		// Is any changes pending(simple test for AutoSync)
		bool GetDirty();

		// After Sync
		// todo: Probably not needed at all(i.e. all SetDirty(false) is only deep inside in notifiers, and with false can be moved  into Sync?)
		void SetDirty(bool bInDirty);  

		FDatasmithFacadeScene GetDatasmithScene();

		// Explicit immediate release of resources(line event handlers)
		void Destroy();
	}

	/**
	 * Base document class.
	 */
	public abstract class FDocument
	{
		public int DocId = -1;
		public ModelDoc2 SwDoc = null;

		// Datasmith scene synced with the Solidworks document
		public IDocumentSyncer DocumentSyncer;

		// DirectLink
		public FDatasmithFacadeDirectLink DatasmithDirectLink = null;
		public string DirectLinkPath;
		public bool bDirectLinkAutoSync { get; set; } = false;
		public int DirectLinkSyncCount { get; private set; } = 0;

		// Sync/Export 
		public bool bDirectLinkSyncInProgress = false;
		public bool bFileExportInProgress = false;
		public string DatasmithFileExportPath = null;


		public FDocument(int InDocId, ModelDoc2 InSwDoc)
		{
			DocId = InDocId;
			SwDoc = InSwDoc;

			DirectLinkPath = Path.Combine(Path.GetTempPath(), "sw_dl_" + Guid.NewGuid().ToString());

			if (!Directory.Exists(DirectLinkPath))
			{
				Directory.CreateDirectory(DirectLinkPath);
			}
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
			
			bDirectLinkSyncInProgress = true;

			DocumentSyncer.Sync(DirectLinkPath);
			DatasmithDirectLink.UpdateScene(DocumentSyncer.GetDatasmithScene());
			DocumentSyncer.SetDirty(false);

			bDirectLinkSyncInProgress = false;

			SetExportStatus("Done");

			DocumentSyncer.Start();
		}

		public void ToggleDirectLinkAutoSync()
		{
			bDirectLinkAutoSync = !bDirectLinkAutoSync;

			if (bDirectLinkAutoSync && DirectLinkSyncCount == 0)
			{
				// Run first sync
				OnDirectLinkSync();
			}
		}

		public void OnIdle()
		{
			if (bDirectLinkAutoSync && DocumentSyncer.GetDirty())
			{
				OnDirectLinkSync();
			}
		}

		public void OnExportToFile(string InFilePath)
		{
			bFileExportInProgress = true;
			DatasmithFileExportPath = InFilePath;  // For status
			Export(InFilePath);
			bFileExportInProgress = false;
		}

		// Toggle if this document is synced with DirectLink
		public void MakeActive(bool bInActive)
		{
			if (bInActive)
			{
				if (DatasmithDirectLink == null)
				{
					DatasmithDirectLink = new FDatasmithFacadeDirectLink();
					if (!DatasmithDirectLink.InitializeForScene(DocumentSyncer.GetDatasmithScene()))
					{
						throw new Exception("DirectLink: failed to initialize");
					}
				}

				DocumentSyncer.Resume();
			}
			else
			{
				// Suspend material checker(while it's a heavy threaded procedure)
				// other event handling is not suspended to register changes in the model
				DocumentSyncer?.Pause();

				DatasmithDirectLink?.CloseCurrentSource();
				DatasmithDirectLink?.Dispose();  // aka 'Close DirectLink Connection'
				DatasmithDirectLink = null;
			}
		}

		public abstract void Export(string InFilePath);

		public string GetPathName()
		{
			return SwDoc.GetPathName();
		}

		// Dispose all resources explicitly
		// todo: DatasmithDirectLink should be disposed here too?
		public virtual void Destroy()
		{
			DocumentSyncer?.Destroy();
		}

		public void SetExportStatus(string InMessage)
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

	}
}