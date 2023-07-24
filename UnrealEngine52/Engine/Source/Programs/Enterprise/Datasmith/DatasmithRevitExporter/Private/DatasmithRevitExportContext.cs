// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Autodesk.Revit.ApplicationServices;
using Autodesk.Revit.DB;
using Autodesk.Revit.DB.Events;
using Autodesk.Revit.DB.Visual;

namespace DatasmithRevitExporter
{
	// Custom export context for command Export to Unreal Datasmith.
	public class FDatasmithRevitExportContext : IPhotoRenderContext
	{
		// Revit application information for Datasmith.
		public const string HOST_NAME    = "Revit";
		public const string VENDOR_NAME  = "Autodesk Inc.";
		public const string PRODUCT_NAME = "Revit";

		// Revit to Datasmith unit convertion factor.
		private const float CENTIMETERS_PER_FOOT = 30.48F;

		// Running Revit version.
		private string ProductVersion = "";

		// Active Revit document being exported.
		private Document RevitDocument = null;

		// Datasmith file paths for each 3D view to be exported.
		private Dictionary<ElementId, string> DatasmithFilePaths = null;

		// Multi-line debug log.
		private FDatasmithFacadeLog DebugLog = null;

		// Level of detail when tessellating faces (between -1 and 15).
		public int LevelOfTessellation { get; set; } = 8;

		// Stack of world Transforms for the Revit instances being processed.
		private Stack<Transform> WorldTransformStack = new Stack<Transform>();

		// Datasmith scene being built.
		private FDatasmithFacadeScene DatasmithScene = null;

		// Stack of Revit document data being processed.
		private Stack<FDocumentData> DocumentDataStack = new Stack<FDocumentData>();

		// Cache exported document data for future reference (including the parenting (linked documents))
		private Dictionary<string, FDocumentData> ExportedDocuments = null;

		// List of extra search paths for Revit texture files.
		private IList<string> ExtraTexturePaths = new List<string>();

		// HashSet of exported texture names, used to make sure we only export each texture once.
		private HashSet<string> UniqueTextureNameSet = new HashSet<string>();

		// List of messages generated during the export process.
		private List<string> MessageList = new List<string>();

		// The file path for the view that is currently being exported.
		private string CurrentDatasmithFilePath = null;

		private FDirectLink DirectLink;

		private FSettings DocumentSettings;

		private FDatasmithRevitExportContext() {}

		private bool CurrentElementSkipped = false;

		// Progress bar callback.
		public void HandleProgressChanged(
			object                   InSender,
			ProgressChangedEventArgs InArgs
		)
		{
			// DebugLog.AddLine($"HandleProgressChanged: {InArgs.Stage} {InArgs.Position} {InArgs.UpperRange} {InArgs.Caption}");
		}

		public IList<string> GetMessages()
		{
			return MessageList;
		}

		public FDatasmithRevitExportContext(
			Application						InApplication,        // running Revit application
			Document						InDocument,           // active Revit document
			FSettings						InSettings,
			Dictionary<ElementId, string>	InDatasmithFilePaths, // Datasmith output file path
			DatasmithRevitExportOptions		InExportOptions,       // Unreal Datasmith export options
			FDirectLink						InDirectLink
		)
		{
			ProductVersion = InApplication.VersionNumber;
			DocumentSettings = InSettings;
			DatasmithFilePaths = InDatasmithFilePaths;
			RevitDocument = InDocument;
			DirectLink = InDirectLink;

			// Get the Unreal Datasmith export options.
			DebugLog = InExportOptions.GetWriteLogFile() ? new FDatasmithFacadeLog() : null;
			LevelOfTessellation = InSettings.LevelOfTesselation;
		}

		//========================================================================================================================
		// Implement IPhotoRenderContext interface methods called at times of drawing geometry as if executing the Render command.
		// Only actual geometry suitable to appear in a rendered view will be processed and output.

		// Start is called at the very start of the export process, still before the first entity of the model was send out.
		public bool Start()
		{
			// Retrieve the list of extra search paths for Revit texture files.
			RetrieveExtraTexturePaths();

			// Set the coordinate system type of the world geometries and transforms.
			// Revit uses a right-handed Z-up coordinate system.
			FDatasmithFacadeElement.SetCoordinateSystemType(FDatasmithFacadeElement.ECoordinateSystemType.RightHandedZup);

			// Set the scale factor from Revit world units to Datasmith centimeters.
			// Revit uses foot as internal system unit for all 3D coordinates.
			FDatasmithFacadeElement.SetWorldUnitScale(CENTIMETERS_PER_FOOT);

			// We are ready to proceed with the export.
			return true;
		}

		// Finish is called at the very end of the export process, after all entities were processed (or after the process was cancelled).
		public void Finish()
		{
			if (DebugLog != null && CurrentDatasmithFilePath != null)
			{
				DebugLog.WriteFile(CurrentDatasmithFilePath.Replace(".udatasmith", ".log"));
			}
		}

		// IsCanceled is queried at the beginning of every element.
		public bool IsCanceled()
		{
			// Return whether or not the export process should be canceled.
			return false;
		}

		// OnViewBegin marks the beginning of a 3D view to be exported.
		public RenderNodeAction OnViewBegin(
			ViewNode InViewNode // render node associated with the 3D view
		)
		{
			// Set the level of detail when tessellating faces (between -1 and 15).
			InViewNode.LevelOfDetail = LevelOfTessellation;

			// Initialize the world transform for the 3D view being processed.
			WorldTransformStack.Push(Transform.Identity);

			// Create an empty Datasmith scene.
			if (DirectLink != null)
			{
				DirectLink.OnBeginExport();
				DatasmithScene = DirectLink.DatasmithScene;
				ExportedDocuments = DirectLink.ExportedDocuments;
			}
			else
			{
				DatasmithScene = new FDatasmithFacadeScene(HOST_NAME, VENDOR_NAME, PRODUCT_NAME, ProductVersion);
				ExportedDocuments = new Dictionary<string, FDocumentData>();
			}

			DatasmithScene.PreExport();

			View3D ViewToExport = RevitDocument.GetElement(InViewNode.ViewId) as View3D;
			if (DatasmithFilePaths != null && !DatasmithFilePaths.TryGetValue(ViewToExport.Id, out CurrentDatasmithFilePath))
			{
				return RenderNodeAction.Skip; // TODO log error?
			}
			else if (DirectLink == null)
			{
				string OutputPath = Path.GetDirectoryName(CurrentDatasmithFilePath);
				string SceneName = Path.GetFileNameWithoutExtension(CurrentDatasmithFilePath);

				DatasmithScene.SetOutputPath(OutputPath);
				DatasmithScene.SetName(SceneName);
			}

			// Keep track of the active Revit document being exported.
			PushDocument(RevitDocument, null);

			// Add a new camera actor to the Datasmith scene for the 3D view camera.
			AddCameraActor(ViewToExport, InViewNode.GetCameraInfo());

			// We want to export the 3D view.
			return RenderNodeAction.Proceed;
		}

		// OnViewEnd marks the end of a 3D view being exported.
		// This method is invoked even for 3D views that were skipped.
		public void OnViewEnd(
			ElementId InElementId // exported 3D view ID
		)
		{
			// Forget the active Revit document being exported.
			FDocumentData DocumentData = PopDocument();

			// Check if this is regular file export.
			if (DirectLink == null)
			{
				DatasmithScene.CleanUp();
				DocumentData.OptimizeActorHierarchy(DatasmithScene);

				// Build and export the Datasmith scene instance and its scene element assets.
				DatasmithScene.ExportScene();

				// Dispose of the Datasmith scene.
				DatasmithScene = null;
			}
			else
			{
				DirectLink.OnEndExport();
			}

			// Forget the 3D view world transform.
			WorldTransformStack.Pop();
		}

		private int DebugIndent = 0;

		/// <summary>
		/// Log debug for plugin developer - no runtime toggle(enabled by adding DatasmithRevitDebugOutput conditional compilation symbol)
		/// </summary>
		[Conditional("DatasmithRevitDebugOutput")]
		public void LogDebug(string Message)  
		{
			string OutputMessage = String.Concat(Enumerable.Repeat("    ", DebugIndent)) + Message;
			MessageList.Add(OutputMessage);
			DatasmithRevitApplication.Instance.LogDebug(OutputMessage);
		}

		[Conditional("DatasmithRevitDebugOutput")]
		private void LogDebugElementBegin(ElementId InElementId)
		{
			LogDebug($"OnElementBegin({InElementId})");
			Element CurrentElement = GetElement(InElementId);
			if (CurrentElement != null)
			{
				LogDebug($" # '{CurrentElement.Name}'({CurrentElement.GetType()}), UID: {CurrentElement.UniqueId}");
				if (CurrentElement is FamilyInstance Instance)
				{
					LogDebug($" # Instance: '{Instance.Name}'");
					FamilySymbol Symbol = Instance.Symbol;
					LogDebug($" #  FamilySymbol: '{Symbol.Name}',  Id: {Symbol.Id}, UID: {Symbol.UniqueId}");
					Family SymbolFamily = Symbol.Family;
					LogDebug($" #   Family: '{SymbolFamily.Name}',  Id: {SymbolFamily.Id}, UID {SymbolFamily.UniqueId} ");
				}
			}
			else
			{
				LogDebug(" # <Unknown>");
			}

			++DebugIndent;
		}

		[Conditional("DatasmithRevitDebugOutput")]
		private void LogDebugElementEnd(ElementId InElementId)
		{
			--DebugIndent;

			LogDebug($"OnElementEnd({InElementId})");
		}

		[Conditional("DatasmithRevitDebugOutput")]
		private void LogDebugInstanceBegin(InstanceNode InInstanceNode)
		{
			LogDebug($"OnInstanceBegin({InInstanceNode.NodeName})");

#if REVIT_API_2023
			SymbolGeometryId GeometryId = InInstanceNode.GetSymbolGeometryId();
			LogDebug($" #  GeometryUniqueId: '{GeometryId.AsUniqueIdentifier()}'");
			Element CurrentFamilySymbol = GetElement(GeometryId.SymbolId);
#else
			Element CurrentFamilySymbol = GetElement(InInstanceNode.GetSymbolId());
#endif

			if (CurrentFamilySymbol != null)
			{
				LogDebug($" #  Symbol: '{CurrentFamilySymbol.GetType()}', '{CurrentFamilySymbol.Name}',  Id: {CurrentFamilySymbol.Id}, UID: {CurrentFamilySymbol.UniqueId}");

				switch (CurrentFamilySymbol)
				{
					case FamilySymbol Symbol:
					{
						Family SymbolFamily = Symbol.Family;
						LogDebug(
							$" #   Family: '{SymbolFamily.Name}',  Id: {SymbolFamily.Id}, UID {SymbolFamily.UniqueId} ");
						break;
					}
				}
			}

			++DebugIndent;
		}

		[Conditional("DatasmithRevitDebugOutput")]
		private void LogDebugInstanceEnd(InstanceNode InInstanceNode)
		{
			--DebugIndent;

			LogDebug($"OnInstanceEnd({InInstanceNode.NodeName})");
		}

		[Conditional("DatasmithRevitDebugOutput")]
		private void LogDebugLinkBegin(LinkNode InLinkNode)
		{
			LogDebug($"LogDebugLinkBegin({InLinkNode.NodeName})");

			++DebugIndent;
		}

		[Conditional("DatasmithRevitDebugOutput")]
		private void LogDebugLinkEnd(LinkNode InLinkNode)
		{
			--DebugIndent;

			LogDebug($"OnLinkEnd({InLinkNode.NodeName})");
		}

		// OnElementBegin marks the beginning of an element to be exported.
		public RenderNodeAction OnElementBegin(
			ElementId InElementId // exported element ID
		)
		{
			LogDebugElementBegin(InElementId);

			CurrentElementSkipped = true;

			Element CurrentElement = GetElement(InElementId);

			if (CurrentElement != null && !CurrentElement.IsTransient)
			{
				// Keep track of the element being processed.
				if (!PushElement(CurrentElement, WorldTransformStack.Peek(), "Element Begin"))
				{
					return RenderNodeAction.Skip; // Cached element.
				}

				// We want to export the element.
				CurrentElementSkipped = false;
				return RenderNodeAction.Proceed;
			}

            return RenderNodeAction.Skip;
		}

		// OnElementEnd marks the end of an element being exported.
		// This method is invoked even for elements that were skipped.
		public void OnElementEnd(
			ElementId InElementId // exported element ID
		)
		{
			LogDebugElementEnd(InElementId);

			if (GetElement(InElementId) != null)
			{
				Debug.Assert(!GetElement(InElementId).IsTransient); // OnElementBegin checks for IsTransient
				// Forget the current element being exported.
				PopElement("Element End");
			}
		}

		// OnInstanceBegin marks the beginning of a family instance to be exported.
		public RenderNodeAction OnInstanceBegin(
			InstanceNode InInstanceNode // family instance output node
		)
		{
			LogDebugInstanceBegin(InInstanceNode);

#if REVIT_API_2023
			Element CurrentFamilySymbol = GetElement(InInstanceNode.GetSymbolGeometryId().SymbolId);
#else
			Element CurrentFamilySymbol = GetElement(InInstanceNode.GetSymbolId());
#endif

			if (CurrentFamilySymbol != null)
			{
				// Keep track of the world transform for the instance being processed.
				WorldTransformStack.Push(WorldTransformStack.Peek().Multiply(InInstanceNode.GetTransform()));

				ElementType CurrentInstanceType = CurrentFamilySymbol as ElementType;

				// Keep track of the instance being processed.
				if (CurrentInstanceType != null)
				{
					PushInstance(CurrentInstanceType, WorldTransformStack.Peek(), "Instance Begin");
				}
				else
				{
					CurrentElementSkipped = !PushElement(CurrentFamilySymbol, WorldTransformStack.Peek(), "Symbol Begin");
				}
			}

			// We always wanna proceed, because in certain cases where InInstanceNode is valid but CurrentInstance is not,
			// what follows is valid geometry related to the instance previously exported.
            return CurrentElementSkipped ? RenderNodeAction.Skip : RenderNodeAction.Proceed;
		}

		// OnInstanceEnd marks the end of a family instance being exported.
		// This method is invoked even for family instances that were skipped.
		public void OnInstanceEnd(
			InstanceNode InInstanceNode // family instance output node
		)
		{
			LogDebugInstanceEnd(InInstanceNode);

#if REVIT_API_2023
			Element CurrentFamilySymbol = GetElement(InInstanceNode.GetSymbolGeometryId().SymbolId);
#else
			Element CurrentFamilySymbol = GetElement(InInstanceNode.GetSymbolId());
#endif

			if (CurrentFamilySymbol != null)
			{
				// Forget the current instance being exported.
				if (CurrentFamilySymbol as ElementType != null)
				{
					PopInstance("Instance End");
				}
				else
				{
					PopElement("Symbol End");
				}

				// Forget the current world transform.
				WorldTransformStack.Pop();
			}
		}

		// OnLinkBegin marks the beginning of a link instance to be exported.
		public RenderNodeAction OnLinkBegin(
			LinkNode InLinkNode // linked Revit document output node
		)
		{
			LogDebugLinkBegin(InLinkNode);
#if REVIT_API_2023
			ElementType CurrentInstanceType = GetElement(InLinkNode.SymbolId) as ElementType;
#else
			ElementType CurrentInstanceType = GetElement(InLinkNode.GetSymbolId()) as ElementType;
#endif

			if (CurrentInstanceType != null)
			{
				// Keep track of the world transform for the instance being processed.
				WorldTransformStack.Push(WorldTransformStack.Peek().Multiply(InLinkNode.GetTransform()));

				// Keep track of the instance being processed.
				PushInstance(CurrentInstanceType, WorldTransformStack.Peek(), "Link Begin");
			}

			Document LinkedDocument = InLinkNode.GetDocument();

			if (LinkedDocument != null)
			{
				// Keep track of the linked document being processed.
				PushDocument(LinkedDocument, CurrentInstanceType.UniqueId);
			}

			return (CurrentInstanceType != null && LinkedDocument != null) ? RenderNodeAction.Proceed : RenderNodeAction.Skip;
		}

		// OnLinkEnd marks the end of a link instance being exported.
		// This method is invoked even for link instances that were skipped.
		public void OnLinkEnd(
			LinkNode InLinkNode // linked Revit document output node
		)
		{
			LogDebugLinkEnd(InLinkNode);
			if (InLinkNode.GetDocument() != null)
			{
				// Forget the current linked document being exported.
				PopDocument();
			}

#if REVIT_API_2023
			ElementType CurrentInstanceType = GetElement(InLinkNode.SymbolId) as ElementType;
#else
			ElementType CurrentInstanceType = GetElement(InLinkNode.GetSymbolId()) as ElementType;
#endif

			if (CurrentInstanceType != null)
			{
				// Forget the current instance being exported.
				PopInstance("Link End");

				// Forget the current world transform.
				WorldTransformStack.Pop();
			}
		}

		// OnLight marks the beginning of export of a light which is enabled for rendering.
		// This method is only called for interface IPhotoRenderContext.
		public void OnLight(
			LightNode InLightNode // light output node
		)
		{
			// Keep track of the world transform for the light being processed.
			WorldTransformStack.Push(WorldTransformStack.Peek().Multiply(InLightNode.GetTransform()));

			// Add a light actor in the hierarchy of Datasmith actors being processed.
			AddLightActor(WorldTransformStack.Peek(), InLightNode.GetAsset());

			// Forget the current light world transform.
			WorldTransformStack.Pop();
		}

		// OnRPC marks the beginning of export of an RPC object.
		// This method is only called for interface IPhotoRenderContext.
		public void OnRPC(
			RPCNode InRPCNode // RPC content output node
		)
		{
			// We ignore the RPC node local transform since the RPC location point will be used later.

			// Add an RPC mesh actor in the hierarchy of Datasmith actors being processed.
			AddRPCActor(WorldTransformStack.Peek(), InRPCNode.GetAsset());
		}

		// OnFaceBegin marks the beginning of a Face to be exported.
		// This method is invoked only when the custom exporter was set up to include geometric objects in the output stream (IncludeGeometricObjects).
		public RenderNodeAction OnFaceBegin(
			FaceNode InFaceNode // face output node
		)
		{
			// We want to receive geometry (polymesh) for this face.
			return RenderNodeAction.Proceed;
		}

		// OnFaceEnd marks the end of the current face being exported.
		// This method is invoked only when the custom exporter was set up to include geometric objects in the output stream (IncludeGeometricObjects).
		// This method is invoked even for faces that were skipped.
		public void OnFaceEnd(
			FaceNode InFaceNode // face output node
		)
		{
			// Nothing to do here.
		}

		// OnMaterial marks a change of the material.
		// This method can be invoked for every single out-coming mesh even when the material has not actually changed.
		public void OnMaterial(
			MaterialNode InMaterialNode // current material output node
		)
		{
			SetMaterial(InMaterialNode, ExtraTexturePaths);
		}

		// OnPolymesh is called when a tessellated polymesh of a 3D face is being output.
		// The current material is applied to the polymesh.
		public void OnPolymesh(
			PolymeshTopology InPolymeshNode // tessellated polymesh output node
		)
		{
			if (IgnoreElementGeometry())
			{
				return;
			}

			int CurrentMaterialIndex = DocumentDataStack.Peek().GetCurrentMaterialIndex();

			FDocumentData.FDatasmithPolymesh CurrentMesh = GetCurrentPolymesh();

			// Retrieve the Datasmith mesh being processed.
			Transform MeshPointsTransform = GetCurrentMeshPointsTransform();

			int initialVertexCount = CurrentMesh.Vertices.Count;

			// Add the vertex points (in right-handed Z-up coordinates) to the Datasmith mesh.
			foreach (XYZ Point in InPolymeshNode.GetPoints())
			{
				XYZ FinalPoint = MeshPointsTransform != null ? MeshPointsTransform.OfPoint(Point) : Point;
				CurrentMesh.Vertices.Add(FinalPoint);
			}

			// Add the vertex UV texture coordinates to the Datasmith mesh.
			foreach (UV uv in InPolymeshNode.GetUVs())
			{
				CurrentMesh.UVs.Add(new UV(uv.U, -uv.V));
			}

			IList<PolymeshFacet> Facets = InPolymeshNode.GetFacets();
			// Add the triangle vertex indexes to the Datasmith mesh.
			foreach (PolymeshFacet facet in Facets)
			{
				CurrentMesh.Faces.Add(new FDocumentData.FPolymeshFace(initialVertexCount + facet.V1, initialVertexCount + facet.V2, initialVertexCount + facet.V3, CurrentMaterialIndex));
			}

			// Add the triangle vertex normals (in right-handed Z-up coordinates) to the Datasmith mesh.
			// Normals can be associated with either points or facets of the polymesh.
			switch (InPolymeshNode.DistributionOfNormals)
			{
				case DistributionOfNormals.AtEachPoint:
				{
					IList<XYZ> normals = InPolymeshNode.GetNormals();
					if (MeshPointsTransform != null)
					{
						foreach (PolymeshFacet facet in Facets)
						{
							XYZ normal1 = MeshPointsTransform.OfVector(normals[facet.V1]);
							XYZ normal2 = MeshPointsTransform.OfVector(normals[facet.V2]);
							XYZ normal3 = MeshPointsTransform.OfVector(normals[facet.V3]);

							CurrentMesh.Normals.Add(normal1);
							CurrentMesh.Normals.Add(normal2);
							CurrentMesh.Normals.Add(normal3);
						}
					}
					else
					{
						foreach (PolymeshFacet facet in Facets)
						{
							XYZ normal1 = normals[facet.V1];
							XYZ normal2 = normals[facet.V2];
							XYZ normal3 = normals[facet.V3];

							CurrentMesh.Normals.Add(normal1);
							CurrentMesh.Normals.Add(normal2);
							CurrentMesh.Normals.Add(normal3);
						}
					}

					break;
				}
				case DistributionOfNormals.OnePerFace:
				{
					XYZ normal = InPolymeshNode.GetNormals()[0];

					if (MeshPointsTransform != null)
					{
						normal = MeshPointsTransform.OfVector(normal);
					}

					for (int i = 0; i < 3 * InPolymeshNode.NumberOfFacets; i++)
					{
						CurrentMesh.Normals.Add(normal);
					}
					break;
				}
				case DistributionOfNormals.OnEachFacet:
				{
					if (MeshPointsTransform != null)
					{
						foreach (XYZ normal in InPolymeshNode.GetNormals())
						{
							XYZ FinalNormal = MeshPointsTransform.OfVector(normal);
							CurrentMesh.Normals.Add(FinalNormal);
							CurrentMesh.Normals.Add(FinalNormal);
							CurrentMesh.Normals.Add(FinalNormal);
						}
					}
					else
					{
						foreach (XYZ normal in InPolymeshNode.GetNormals())
						{
							CurrentMesh.Normals.Add(normal);
							CurrentMesh.Normals.Add(normal);
							CurrentMesh.Normals.Add(normal);
						}
					}
					break;
				}
			}
		}

		// End of IPhotoRenderContext interface method implementation.
		//========================================================================================================================

		// Retrieve the list of extra search paths for Revit texture files.
		// This is done by reading user's Revit.ini file and searching for field AdditionalRenderAppearancePaths
		// which contains search paths that Revit will use to locate texture files.
		// Note that the behavior in Revit is to search in the directory itself and not in child sub-directories.
		private void RetrieveExtraTexturePaths()
		{
			string UserSpecificDirectoryPath = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);

			if (!string.IsNullOrEmpty(UserSpecificDirectoryPath) && Path.IsPathRooted(UserSpecificDirectoryPath) && Directory.Exists(UserSpecificDirectoryPath))
			{
				string FullRevitIniPath = $"{UserSpecificDirectoryPath}\\AppData\\Roaming\\Autodesk\\Revit\\Autodesk Revit {ProductVersion}\\Revit.ini";

				if (File.Exists(FullRevitIniPath))
				{
					FileStream RevitIniStream = new FileStream(FullRevitIniPath, FileMode.Open, FileAccess.Read, FileShare.Read);

					using (StreamReader RevitIniReader = new StreamReader(RevitIniStream))
					{
						string ConfigLine;

						while ((ConfigLine = RevitIniReader.ReadLine()) != null)
						{
							if (ConfigLine.Contains("AdditionalRenderAppearancePaths"))
							{
								string[] SplitLineArray = ConfigLine.Split('=');

								if (SplitLineArray.Length > 1)
								{
									string[] TexturePaths = SplitLineArray[1].Split('|');

									foreach (string TexturePath in TexturePaths)
									{
										ExtraTexturePaths.Add(Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, TexturePath)));
									}

									break;
								}
							}
						}
					}
				}
			}
		}

		private Element GetElement(
			ElementId InElementId
		)
		{
			return DocumentDataStack.Peek().GetElement(InElementId);
		}

		private void PushDocument(
			Document InRevitDocument,
			string InLinkedDocumentId
		)
		{
			if (DocumentDataStack.Count > 0 && DirectLink != null)
			{
				// This is a linked document, store it's root element
				Element LinkedDocElement = DocumentDataStack.Peek().GetCurrentElement();
				DirectLink.OnBeginLinkedDocument(LinkedDocElement);
			}

			bool bIsTopDocument = InLinkedDocumentId == null;
			string DocumentId = InLinkedDocumentId ?? "";

			// Check if we have cache for this document so we don't recreate it each Sync
			// Also ElementData objects are referencing their owning FDocumentData 
			if (!ExportedDocuments.TryGetValue(DocumentId, out var DocumentData))  
			{
				DocumentData = new FDocumentData(InRevitDocument, DocumentSettings, ref MessageList,
					DirectLink, DocumentId);
				ExportedDocuments.Add(DocumentId, DocumentData);
			}

			// Reset DocumentData for new update
			// todo: Current implementation expects DocumentData to be clean but what is possible is to track created assets and to reuse them when their element changes
			DocumentData.Reset(this);

			DocumentDataStack.Push(DocumentData);

			if (bIsTopDocument)
			{
				// Top level document
				DocumentDataStack.Peek().AddLocationActors(WorldTransformStack.Peek());
			}

		}

		private FDocumentData PopDocument()
		{
			FDocumentData DocumentData = DocumentDataStack.Peek();

			if (DocumentDataStack.Count == 1)
			{
				DocumentData.WrapupScene(DatasmithScene, UniqueTextureNameSet);
			}
			else
			{
				// Get element data which is the parent of linked elements - need document under the current Linked document on the stack
				// Stack.ToArray returns array with Stack top at [0] array element, so second to top is [1]
				// todo: may replace Stack with List
				FDocumentData[] DocumentDataArray = DocumentDataStack.ToArray();
				FDocumentData.FBaseElementData ParentElementData = DocumentDataArray[1].GetCurrentActor();
			
				DocumentData.WrapupLink(DatasmithScene, ParentElementData, UniqueTextureNameSet);
				DirectLink?.OnEndLinkedDocument();
			}
			// Pop document only when all elements are collected for it (WrapupScene may also make actors for host elements like Level which needs to have proper document stack to build unique element name-path)
			DocumentDataStack.Pop(); 
			return DocumentData;
		}

		private bool PushElement(
			Element   InElement,
			Transform InWorldTransform,
			string    InLogLinePrefix
		)
		{
			FDocumentData DocumentData = DocumentDataStack.Peek();
			if (DocumentData.PushElement(InElement, InWorldTransform))
			{
				DocumentData.LogElement(DebugLog, InLogLinePrefix, +1);
				return true;
			}
			return false;
		}

		private void PopElement(
			string InLogLinePrefix
		)
		{
			if (CurrentElementSkipped)
			{
				CurrentElementSkipped = false;
				return;
			}

			FDocumentData DocumentData = DocumentDataStack.Peek();

			DocumentData.LogElement(DebugLog, InLogLinePrefix, -1);
			DocumentData.PopElement(DatasmithScene);
		}

		private void PushInstance(
			ElementType InInstanceType,
			Transform   InWorldTransform,
			string      InLogLinePrefix
		)
		{
			FDocumentData DocumentData = DocumentDataStack.Peek();

			DocumentData.PushInstance(InInstanceType, InWorldTransform);
			DocumentData.LogElement(DebugLog, InLogLinePrefix, +1);
		}

		private void PopInstance(
			string InLogLinePrefix
		)
		{
			FDocumentData DocumentData = DocumentDataStack.Peek();

			DocumentData.LogElement(DebugLog, InLogLinePrefix, -1);
			DocumentData.PopInstance(DatasmithScene);
		}

		private void AddLightActor(
			Transform InWorldTransform,
			Asset     InLightAsset
		)
		{
			DocumentDataStack.Peek().AddLightActor(InWorldTransform, InLightAsset);
		}

		private void AddRPCActor(
			Transform InWorldTransform,
			Asset     InRPCAsset
		)
		{
			DocumentDataStack.Peek().AddRPCActor(InWorldTransform, InRPCAsset, DatasmithScene);
		}

		private void SetMaterial(
			MaterialNode  InMaterialNode,
			IList<string> InExtraTexturePaths
		)
		{
			FDocumentData DocumentData = DocumentDataStack.Peek();

			if (DocumentData.SetMaterial(InMaterialNode, InExtraTexturePaths))
			{
				DocumentData.LogMaterial(InMaterialNode, DebugLog, "Add Material");
			}
		}

		private bool IgnoreElementGeometry()
		{
			return DocumentDataStack.Peek().IgnoreElementGeometry();
		}

		private FDocumentData.FDatasmithPolymesh GetCurrentPolymesh()
		{
			return DocumentDataStack.Peek().GetCurrentPolymesh();
		}

		public FDatasmithFacadeMeshElement GetCurrentMeshElement()
		{
			return DocumentDataStack.Peek().GetCurrentMeshElement();
		}

		private Transform GetCurrentMeshPointsTransform()
		{
			return DocumentDataStack.Peek().GetCurrentMeshPointsTransform();
		}

		private void AddCameraActor(
			View3D     InView3D,
			CameraInfo InViewCamera
		)
		{
			FDatasmithFacadeActorCamera CameraActor = null;

			if (DirectLink != null)
			{
				DirectLink.MarkForExport(InView3D);

				if (DirectLink.IsElementCached(InView3D))
				{
					FDocumentData.FBaseElementData CameraElementData = DirectLink.GetCachedElement(InView3D);
					CameraActor = CameraElementData.ElementActor as FDatasmithFacadeActorCamera;
				}
			}

			if (CameraActor == null)
			{
				// Create a new Datasmith camera actor.
				// Hash the Datasmith camera actor name to shorten it.
				string HashedName = FDatasmithFacadeElement.GetStringHash(InView3D.UniqueId);
				CameraActor = new FDatasmithFacadeActorCamera(HashedName);
			}

			CameraActor.SetLabel(InView3D.Name);

			if (InView3D.Category != null)
			{
				// Set the Datasmith camera actor layer to be the 3D view category name.
				CameraActor.SetLayer(InView3D.Category.Name);
			}

			// Gets the current non-saved orientation of the 3D view.
			ViewOrientation3D ViewOrientation = InView3D.GetOrientation();

			// Set the world position (in right-handed Z-up coordinates) of the Datasmith camera actor.
			XYZ CameraPosition = ViewOrientation.EyePosition;
			CameraActor.SetCameraPosition((float)CameraPosition.X, (float)CameraPosition.Y, (float)CameraPosition.Z);

			// Set the world rotation of the Datasmith camera actor with
			// the camera world forward and up vectors (in right-handed Z-up coordinates).
			XYZ CameraForward = ViewOrientation.ForwardDirection;
			XYZ CameraUp = ViewOrientation.UpDirection;
			CameraActor.SetCameraRotation((float)CameraForward.X, (float)CameraForward.Y, (float)CameraForward.Z, (float)CameraUp.X, (float)CameraUp.Y, (float)CameraUp.Z);

			// When the 3D view camera is not available, an orthographic view should be assumed.
			if (InViewCamera != null)
			{
				// Compute the aspect ratio (width/height) of the Revit 3D view camera, where
				// HorizontalExtent is the distance between left and right planes on the target plane,
				// VerticalExtent is the distance between top and bottom planes on the target plane.
				float AspectRatio = (float)(InViewCamera.HorizontalExtent / InViewCamera.VerticalExtent);

				// Set the aspect ratio of the Datasmith camera.
				CameraActor.SetAspectRatio(AspectRatio);

				if (InView3D.IsPerspective)
				{
					// Set the sensor width of the Datasmith camera.
					CameraActor.SetSensorWidth((float)(InViewCamera.HorizontalExtent * /* millimeters per foot */ 304.8));

					// Get the distance from eye point along view direction to target plane.
					// This value is appropriate for perspective views only.
					float TargetDistance = (float)InViewCamera.TargetDistance;

					// Set the Datasmith camera focus distance.
					CameraActor.SetFocusDistance(TargetDistance);

					// Set the Datasmith camera focal length.
					CameraActor.SetFocalLength(TargetDistance * /* millimeters per foot */ 304.8F);
				}
			}

			if (!DirectLink?.IsElementCached(InView3D) ?? true)
			{
				// Add the camera actor to the Datasmith scene.
				DatasmithScene.AddActor(CameraActor);

				// Cache new camera actor
				DirectLink?.CacheElement(RevitDocument, InView3D, new FDocumentData.FBaseElementData(CameraActor, null, DocumentDataStack.Peek()));
			}
		}

		public string GetActorName(FDocumentData InDocumentData)
		{
			// Expecting this to be called only for current document on the stack
			Debug.Assert(DocumentDataStack.Peek() == InDocumentData);

			// Use Element "path" in the whole linked documents hierarchy to build unique name for any kind of instance element(e.g. instance of the same family of the same linked file)
			string ActorName = string.Join(":", 
				                   DocumentDataStack.Select(DocumentData => $"{DocumentData.DocumentId}({DocumentData.GetElementStackName()})"));
			LogDebug($"GetActorName:'{ActorName}'");
			return ActorName;
		}
	}
}
