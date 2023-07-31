// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Autodesk.Navisworks.Api;
using Autodesk.Navisworks.Api.Data;
using Autodesk.Navisworks.Api.Plugins;
using Autodesk.Navisworks.Api.DocumentParts;
using Autodesk.Navisworks.Api.Interop;
using Autodesk.Navisworks.Api.Interop.ComApi;
using DatasmithNavisworksUtil;

namespace DatasmithNavisworks
{
	// Export parameters effect scene export
	// Adding new parameter: 
	// - default value
	// - Add to gui - tootip(DockPaneControl_Load), DockPaneControl property, UpdateFromControl/UpdateControl
	// - WriteSettings/ReadSetting
	// - Pass to export plugin in OnExportClicked and parse in ParseParameters
	public class ExportParameters
	{
		public int MergeMaxDepth = 3;
		public Vector3d Origin = new Vector3d(0, 0, 0);
		public bool bIncludeMetadata = true;

		public bool USE_COMPONENTS = false;
		public bool CREATE_INSTANCES_FOR_EMPTY_GEOMETRY = false; // Should we bother instantiating empty geometry(might want for debugging or metadata)?
		public bool bUseClipbox = false; // This is disabled for now(can be useful for debugging, though)
	};

	[Plugin("DatasmithNavisworksExporter.Ribbon", "EpicGames", 
		DisplayName = "Datasmith")]
	[RibbonLayout("DatasmithNavisworksExporter.xaml")]
	[Strings("DatasmithNavisworksExporter.name")]
	[RibbonTab("ID_DatasmithTab", DisplayName = "Datasmith")]
	[Command("ID_Button_Export")]
	public class ExporterRibbonPlugin : CommandHandlerPlugin
	{
		public override CommandState CanExecuteCommand(string CommandId)
		{
			CommandState State = new CommandState
			{
				IsVisible = true, 
				IsEnabled = true, 
				IsChecked = false,
			};
			return State;
		}

		public override bool CanExecuteRibbonTab(string RibbonTabId)
		{
			return true;
		}

		// Toggles Exporter UI
		public override int ExecuteCommand(string Name, params string[] Parameters)
		{
			PluginRecord Record = Application.Plugins.FindPlugin("DatasmithNavisworksExporter.DockPane.EpicGames");
			if (Record is DockPanePluginRecord && Record.IsEnabled && Record.LoadPlugin() is ExporterDockPanePlugin DockPanePlugin)
			{
				DockPanePlugin.Visible = !DockPanePlugin.Visible;
			}

			return 0;
		}
	}

	// GUI frontend for the exporter
	[Plugin("DatasmithNavisworksExporter.DockPane", "EpicGames",
		DisplayName = "Datasmith",
		ToolTip = "Datasmith")]
	[DockPanePlugin(244, 268, FixedSize = false, AutoScroll = true, MinimumWidth = 148, MinimumHeight = 248)]
	[Strings("DatasmithNavisworksExporter.name")]
	public class ExporterDockPanePlugin: DockPanePlugin
	{
		private DockPaneControlPresenter DockPanePresenter;

		public class DockPaneControlPresenter : DockPaneControl.IPresenter
		{
			private const int SETTINGS_VERSION = 1;

			public ExportParameters ExportSettings = new ExportParameters();

			public DockPaneControl DockPaneControl;

			public DockPaneControlPresenter(DockPaneControl DockPaneControl)
			{
				this.DockPaneControl = DockPaneControl;
				DockPaneControl.Presenter = this;
				DockPaneControl.CreateControl();

				ReadSetting();
				UpdateControl();
			}

			private void WriteSettings()
			{
				if (!EnsureDatabase(true))
				{
					return;
				}

				try
				{
					DocumentDatabase Database = Application.ActiveDocument.Database;
					using (NavisworksTransaction Transaction = Database.BeginTransaction(DatabaseChangedAction.Edited))
					{
						NavisworksCommand Command = Transaction.Connection.CreateCommand();

						void AddValue(string NameParam, string Name, string ValueParam, string Value)
						{
							Command.Parameters.AddWithValue(NameParam, Name);
							NavisworksParameter CoordValueParam = Command.CreateParameter();
							CoordValueParam.ParameterName = ValueParam;
							CoordValueParam.Value = Value;
							Command.Parameters.Add(CoordValueParam);
						}

						Command.Parameters.AddWithValue("@Version", SETTINGS_VERSION);
						AddValue("@MergeLevelName", "MergeLevel", "@MergeLevelValue", ExportSettings.MergeMaxDepth.ToString(CultureInfo.InvariantCulture));
						AddValue("@XName", "OriginX", "@XValue", ExportSettings.Origin.X.ToString(CultureInfo.InvariantCulture));
						AddValue("@YName", "OriginY", "@YValue", ExportSettings.Origin.Y.ToString(CultureInfo.InvariantCulture));
						AddValue("@ZName", "OriginZ", "@ZValue", ExportSettings.Origin.Z.ToString(CultureInfo.InvariantCulture));
						

						Command.CommandText = "INSERT INTO datasmith_export_settings(version, name, value) VALUES" +
						                      "(@Version, @MergeLevelName, @MergeLevelValue)," +
						                      "(@Version, @XName, @XValue)," +
						                      "(@Version, @YName, @YValue)," +
						                      "(@Version, @ZName, @ZValue);";
						Command.ExecuteNonQuery();
						Transaction.Commit();
					}
				}
				catch (Exception Exception)
				{
					Console.WriteLine(Exception);
				}
			}

			public void ReadSetting()
			{
				if (!EnsureDatabase(false))
				{
					ReadDefaultSettings();
					return;
				}

				DocumentDatabase Database = Application.ActiveDocument.Database;
				NavisworksDataAdapter DataAdapter = new NavisworksDataAdapter($"SELECT version, name, value FROM datasmith_export_settings WHERE version={SETTINGS_VERSION}", Database.Value);
				DataTable Table = new DataTable();

				DataAdapter.Fill(Table);

				foreach (DataRow Row in Table.Rows)
				{
					switch (Row["name"].ToString())
					{
						case "OriginX":
							ExportSettings.Origin.X = Convert.ToDouble(Row["value"]);
							break;
						case "OriginY":
							ExportSettings.Origin.Y = Convert.ToDouble(Row["value"]);
							break;
						case "OriginZ":
							ExportSettings.Origin.Z = Convert.ToDouble(Row["value"]);
							break;
						case "MergeLevel":
							ExportSettings.MergeMaxDepth = Convert.ToInt32(Row["value"]);
							break;
					}
				}
			}

			private void ReadDefaultSettings()
			{
				ExportSettings = new ExportParameters();
			}

			private static bool EnsureDatabase(bool bCreateIfNotExists)
			{
				try
				{
					DocumentDatabase Database = Application.ActiveDocument.Database;

					if (Database.Value.State != ConnectionState.Open)
					{
						return false;
					}

					NavisworksDataAdapter DataAdapter = new NavisworksDataAdapter("SELECT name FROM sqlite_master WHERE type='table' AND name='datasmith_export_settings';", Database.Value);
					DataTable Table = new DataTable();

					DataAdapter.Fill(Table);

					if (Table.Rows.Count != 0)
					{
						// Table already exists
						return true;
					}

					if (!bCreateIfNotExists)
					{
						return false;
					}

					using (NavisworksTransaction Transaction =
						Application.ActiveDocument.Database.BeginTransaction(DatabaseChangedAction.Reset))
					{
						NavisworksCommand Command = Transaction.Connection.CreateCommand();
						Command.CommandText = "CREATE TABLE IF NOT EXISTS datasmith_export_settings(version INTEGER, name TEXT, value TEXT)";
						Command.ExecuteNonQuery();
						Transaction.Commit();
					}
				}
				catch (Exception Exception)
				{
					Console.WriteLine(Exception);
					return false;
				}

				return true;
			}

			public void OnLoaded()
			{
				ReadSetting();
				UpdateControl();
			}

			public void OnExportClicked()
			{
				UpdateFromControl();
				WriteSettings();

				// Execute exporter plugin
				PluginRecord Record = Application.Plugins.FindPlugin("DatasmithNavisworksExporter.EpicGames");
				if (Record is AddInPluginRecord && Record.IsEnabled && Record.LoadPlugin() is AddInPlugin Plugin)
				{
					// Call plugin, passing values formatted by InvariantCulture
					Plugin.Execute("",
						FormattableString.Invariant($"Merge={ExportSettings.MergeMaxDepth}"),
						FormattableString.Invariant($"Origin={ExportSettings.Origin.X}, {ExportSettings.Origin.Y}, {ExportSettings.Origin.Z}"),
						FormattableString.Invariant($"IncludeMetadata={ExportSettings.bIncludeMetadata}")
						);
				}
				else
				{
					System.Windows.Forms.MessageBox.Show("Can't find Datasmith plugin!", "Error",
						System.Windows.Forms.MessageBoxButtons.OK, System.Windows.Forms.MessageBoxIcon.Error);
				}
			}

			public void OnPickOriginClicked()
			{
				// Run picker plugin
				PluginRecord Record = Application.Plugins.FindPlugin("DatasmithNavisworksExporter.PickOrigin.EpicGames");
				if (Record is ToolPluginRecord && Record.IsEnabled && Record.LoadPlugin() is PickOriginPlugin PickOriginPlugin)
				{
					PickOriginPlugin.DockPanePresenter = this;
					Application.MainDocument.Tool.SetCustomToolPlugin(PickOriginPlugin);
				}
			}

			private void UpdateFromControl()
			{
				ExportSettings.MergeMaxDepth = (int) DockPaneControl.MergeValue;

				double.TryParse(DockPaneControl.OriginXText, NumberStyles.Float, CultureInfo.CurrentCulture, out ExportSettings.Origin.X);
				double.TryParse(DockPaneControl.OriginYText, NumberStyles.Float, CultureInfo.CurrentCulture, out ExportSettings.Origin.Y);
				double.TryParse(DockPaneControl.OriginZText, NumberStyles.Float, CultureInfo.CurrentCulture, out ExportSettings.Origin.Z);
			}

			public void UpdateControl()
			{
				DockPaneControl.MergeValue = ExportSettings.MergeMaxDepth;

				DockPaneControl.OriginXText = ((decimal)ExportSettings.Origin.X).ToString(CultureInfo.CurrentCulture);
				DockPaneControl.OriginYText = ((decimal)ExportSettings.Origin.Y).ToString(CultureInfo.CurrentCulture);
				DockPaneControl.OriginZText = ((decimal)ExportSettings.Origin.Z).ToString(CultureInfo.CurrentCulture);
			}
		}

		public override System.Windows.Forms.Control CreateControlPane()
		{
			DockPaneControl DockPaneControl = new DockPaneControl {Dock = System.Windows.Forms.DockStyle.None};
			DockPanePresenter = new DockPaneControlPresenter(DockPaneControl);

			// Bind event to react to file change, to load our settings for it.
			// Autodesk.Navisworks.Api.Application.ActiveDocumentChanged is of no help, see https://forums.autodesk.com/t5/navisworks-api/activedocumentchanged-event-is-not-working-in/td-p/7956840
			Application.ActiveDocument.FileNameChanged += ActiveDocument_FileNameChanged;
			return DockPaneControl;
		}

		private void ActiveDocument_FileNameChanged(object Sender, EventArgs Event)
		{
			DockPanePresenter.ReadSetting();
			DockPanePresenter.UpdateControl();
		}

		public override void DestroyControlPane(System.Windows.Forms.Control Pane)
		{
			Pane?.Dispose();
		}
	}

	[Plugin("DatasmithNavisworksExporter", "EpicGames", 
		DisplayName = "Datasmith")]
	[AddInPlugin(AddInLocation.None,
		LoadForCanExecute = true)]
	[Strings("DatasmithNavisworksExporter.name")]
	public class ExporterPlugin : AddInPlugin
	{
		private const string DIALOG_CAPTION = "Export 3D View to Unreal Datasmith"; // TODO: localize

		private const string HOST_NAME = "Navisworks";
		private const string VENDOR_NAME = "Autodesk Inc.";

		private StreamWriter LogStream;

		public override CommandState CanExecute()
		{
			return new CommandState(true);
		}

		protected override void OnLoaded()
		{
			// Preload DatasmithFacadeCSharp.dll using default locale that has Decimal Separator set to '.'(dot)
			// Otherwise Unreal FConsoleManager::Test() asserts during static initialization
			UnrealLocale Locale = new UnrealLocale();
			FDatasmithFacadeElement.SetCoordinateSystemType(FDatasmithFacadeElement.ECoordinateSystemType.RightHandedZup);
			Locale.RestoreLocale(); // Restore what was set
		}

		class ItemFilterParameters
		{
			public bool bHasClipBox;
			public Box ClipBox;
		}

		// Geometry created for Item's Fragment
		class FragmentGeometry
		{
			private readonly Geometry Geometry;
			public readonly SceneItem SceneItem;

			public bool bOptimized = false;
			private int UserCount = 1; // Reference count, so we can deallocate native memory as soon as it's not used anymore

			public FragmentGeometry()
			{
			}

			public FragmentGeometry(Geometry Geometry, SceneItem SceneItem)
			{
				this.Geometry = Geometry;
				this.SceneItem = SceneItem;
			}

			public uint TriangleCount => Geometry?.TriangleCount ?? 0;

			public bool GetGeometry(out Geometry OutGeometry)
			{
				Debug.Assert(UserCount > 0);
				if (TriangleCount > 0)
				{
					OutGeometry = Geometry;
					return true;
				}

				OutGeometry = null;
				return false;
			}

			public void AddUser()
			{
				UserCount++;
			}

			public void ReleaseUser()
			{
				if (UserCount == 0)
				{
					return;
				}

				UserCount--;
				if (UserCount == 0)
				{
					Geometry?.Dispose();
				}
			}

			public int GetGeometryHashCode()
			{
				return Geometry?.GetHashCode() ?? 0;
			}
		}

		class FragmentGeometryWithAppearance: IEquatable<FragmentGeometryWithAppearance>
		{
			public readonly FragmentGeometry FragmentGeometry;
			public readonly Appearance Appearance;

			private readonly int Hash;

			public FragmentGeometryWithAppearance(FragmentGeometry FragmentGeometry, Appearance Appearance)
			{
				this.FragmentGeometry = FragmentGeometry;
				this.Appearance = Appearance;

				Hash = 0;

				Hash ^= FragmentGeometry.GetGeometryHashCode();
				Hash ^= Appearance.GetHashCode();
			}

			public override bool Equals(object Other) 
			{
				return Equals(Other as FragmentGeometryWithAppearance);
			}

			public bool Equals(FragmentGeometryWithAppearance Other)
			{
				FragmentGeometry.GetGeometry(out Geometry Geometry);
				Other.FragmentGeometry.GetGeometry(out Geometry OtherGeometry);

				bool bGeometriesEqual = Equals(Geometry, OtherGeometry);
				bool bAppearancesEqual = Equals(Appearance, Other.Appearance);
				return bGeometriesEqual && bAppearancesEqual;
			}

			public override int GetHashCode()
			{
				return Hash;
			}
		}

		// Made from SceneItem geometry, reused to instantiate identical SceneItems
		class SceneItemInstancedGeometry: IEquatable<SceneItemInstancedGeometry>
		{
			private readonly List<FragmentGeometry> Meshes;
			public readonly SceneItem Item;

			private readonly int Hash;

			public SceneItemInstancedGeometry(SceneItem Item, List<FragmentGeometry> Geometries)
			{
				this.Item = Item;
				Meshes = Geometries;

				Hash = 0;
				foreach (FragmentGeometry Geom in Meshes)
				{
					Hash ^= Geom.GetGeometryHashCode();
				}
			}

			public int FragmentGeometryCount => Meshes.Count;
			public int NonEmptyGeometryCount => Meshes.Count(Mesh => Mesh.TriangleCount > 0);

			public FragmentGeometry GetFragmentGeometry(int Index)
			{
				return Meshes[Index];
			}

			public IEnumerable<FragmentGeometry> FragmentGeometries => Meshes;

			public override bool Equals(object Other) 
			{
				return Equals(Other as SceneItemInstancedGeometry);
			}

			public bool Equals(SceneItemInstancedGeometry Other)
			{
				return Meshes.Select(A => (A.GetGeometry(out Geometry Geometry), Geometry)).SequenceEqual(Other.Meshes.Select(A => (A.GetGeometry(out Geometry Geometry), Geometry)));
			}

			public override int GetHashCode()
			{
				return Hash;
			}

			public bool HasTriangles()
			{
				foreach (FragmentGeometry FragmentGeometry in Meshes)
				{
					if (FragmentGeometry.TriangleCount > 0)
					{
						return true;
					}
				}

				return false;
			}

			public long TriangleCount()
			{
				return Meshes.Select( F => (long)F.TriangleCount).Sum();
			}

			public void Dispose()
			{
				foreach (FragmentGeometry FragmentGeometry in Meshes)
				{
					FragmentGeometry.ReleaseUser();
				}
			}
		}

		class GeometryInstance
		{
			// Instance is bound to a scene item(a place in original Navisworks Selection Tree)
			public SceneItem SceneItem;

			// Shared Instanced - geometry and list of instances that are due to export.
			// Referenced from all Instances which use its geometry.
			// Holds list of all instances where each instance is added when its decided it need to be exported.
			// Note - instances are not always added into that list. In case their geometry is merged with others then instance is dropped
			public InstancedWithInstancesToExport SharedInstancedForInstancesRef;

			// Transforms and Appearances for geometry elements in SharedInstancedForInstancesRef
			public List<TransformMatrix> Transforms;
			public List<Appearance> Appearances;
			public GuiProperties Metadata;
			public FDatasmithFacadeActorMesh MeshActor;
			public GeometryUtil.BoundingBox BoundingBox;

			public GeometryInstance(InstancedWithInstancesToExport Instanced, SceneItem Item, int MeshesCountHint)
			{
				SharedInstancedForInstancesRef = Instanced;
				SceneItem = Item;
				Transforms = new List<TransformMatrix>(MeshesCountHint);
				Appearances = new List<Appearance>(MeshesCountHint);
			}

			public void AddTransform(TransformMatrix Transform)
			{
				Transforms.Add(Transform);
			}

			public TransformMatrix GetTransform(int FragmentIndex)
			{
				return Transforms[FragmentIndex];
			}

			public void AddToExportSet()
			{
				SharedInstancedForInstancesRef.AddInstanceToExportSet(this);
			}

			public void AddAppearance(Appearance Appearance)
			{
				Appearances.Add(Appearance);
			}

			public Appearance GetAppearance(int FragmentIndex)
			{
				return Appearances[FragmentIndex];
			}

			public void SetMetadata(GuiProperties Metadata)
			{
				this.Metadata = Metadata;
			}
		}

		// Used to create Datasmith meshes for the same geometry in bulk, releasing source geometry early
		class MultiFragmentsGeometryInstances
		{
			public class AppearanceList: IEquatable<AppearanceList>
			{
				public readonly List<Appearance> Appearances;

				private readonly int Hash;

				public AppearanceList(List<Appearance> Appearances)
				{
					this.Appearances = Appearances;

					Hash = 0;
					foreach (Appearance Appearance in Appearances)
					{
						Hash ^= Appearance.GetHashCode();
					}
				}

				public bool Equals(AppearanceList other)
				{
					return Appearances.SequenceEqual(other.Appearances);
				}

				public override bool Equals(object obj)
				{
					return Equals(obj as AppearanceList);
				}

				public override int GetHashCode()
				{
					return Hash;
				}
			}

			public Dictionary<AppearanceList, List<GeometryInstance>>  WithSameTransform;
			public List<GeometryInstance> WithDifferentTransform;

			public MultiFragmentsGeometryInstances()
			{
				WithSameTransform = new Dictionary<AppearanceList, List<GeometryInstance>>();
				WithDifferentTransform = new List<GeometryInstance>();
			}

			public int Count => WithSameTransform.Count + WithDifferentTransform.Count;
		}

		// Holds all instances that will be exported for an Instanced geometry
		class InstancedWithInstancesToExport
		{
			public SceneItem PrototypeItem;
			public SceneItemInstancedGeometry InstancedGeometry;

			public List<GeometryInstance> InstanceList;

			public InstancedWithInstancesToExport()
			{
				InstanceList = new List<GeometryInstance>();
			}

			public GeometryInstance CreateInstance(SceneItem Item)
			{
				GeometryInstance Instance = new GeometryInstance(this, Item, InstancedGeometry.FragmentGeometryCount);
				// TODO: remove comment - instances added to Instance's List when they are collected for export(if not merged)
				// InstanceList.Add(Instance); 
				return Instance;
			}

			public void AddInstanceToExportSet(GeometryInstance Instance)
			{
				InstanceList.Add(Instance);
			}
		};

		public override int Execute(params string[] Parameters)
		{
			// TODO: 
			// Application.ActiveDocument.ActiveView.RequestDelayedRedraw(ViewRedrawRequests.OverlayRender);
			// ... Application.ActiveDocument.ActiveView.RequestDelayedRedraw(ViewRedrawRequests.All);

			try
			{
				bool IsAutomated = Application.IsAutomated;
				Info(IsAutomated ? "Accessed using Automation" : "Exporting from GUI");

				Document ActiveDocument = Application.ActiveDocument;

				if (Parameters.Length == 0)
				{
					DisplayWarning("Datasmith Export command requires target path parameter string");
					return -1;
				}

				if (ActiveDocument.IsClear)
				{
					DisplayWarning("Opened document is required for Datasmith Export");
					return -1;
				}

				ExportParameters ExportParameters = ParseParameters(Parameters, out string FilePath);

				// todo: just check for FilePath existence/null/empty?
				if (!IsAutomated) // Request filename from user
				{
					string InitialDirectory;
					if (File.Exists(ActiveDocument.FileName))
					{
						InitialDirectory = Path.GetDirectoryName(ActiveDocument.FileName);
					}
					else
					{
						InitialDirectory = Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments);
					}

					System.Windows.Forms.SaveFileDialog Dialog = new System.Windows.Forms.SaveFileDialog
					{
						Title = DIALOG_CAPTION,
						InitialDirectory = InitialDirectory,
						FileName = Path.GetFileNameWithoutExtension(ActiveDocument.FileName) + ".udatasmith",
						DefaultExt = "udatasmith",
						Filter = "Unreal Datasmith|*.udatasmith",
						CheckFileExists = false,
						CheckPathExists = true,
						AddExtension = true,
						OverwritePrompt = true
					};

					if (Dialog.ShowDialog() != System.Windows.Forms.DialogResult.OK)
					{
						return -1; // this is not used by Navisworks by docs, but can be used for tests/automation
					}

					FilePath = Dialog.FileName;
				}
				else
				{
				}

				if (string.IsNullOrWhiteSpace(FilePath))
				{
					DisplayWarning("The udatasmith file name is not valid. Aborting the export....");
					return -1;
				}

				string LogFilePath = FilePath + "." + DateTime.Now.ToString("yyyy-M-dd-HH-mm-ss") + ".log";
				EventStopwatch = new Stopwatch();
				EventStopwatch.Start();
				LogStream = new StreamWriter(LogFilePath);
				try
				{
					EventInfo("Collecting garbage");
					// Collect garbage for better measurement
					System.GC.Collect();
					System.GC.WaitForPendingFinalizers();

					EventInfo("Log warmup");
					EventInfo("Start export: " + FilePath);
					InwOpState10 State = Autodesk.Navisworks.Api.ComApi.ComApiBridge.State;

					ItemFilterParameters ItemFilterParams = new ItemFilterParameters();

					if (ExportParameters.bUseClipbox)
					{
						ExtractClipBox(ActiveDocument, ItemFilterParams);
					}

					double CentimetersPerUnit = GetCentimetersPerUnit(ActiveDocument.Units);
					Info($"CentimetersPerUnit: {CentimetersPerUnit}");
					FDatasmithFacadeElement.SetWorldUnitScale((float)CentimetersPerUnit);

					DocumentModels Models = ActiveDocument.Models;

					Progress ProgressBar = Autodesk.Navisworks.Api.Application.BeginProgress("Export", "Exporting scene");
					try
					{

						string ProductVersion = Autodesk.Navisworks.Api.Application.Version.ApiMajor + "." + Autodesk.Navisworks.Api.Application.Version.ApiMinor;
						FDatasmithFacadeScene DatasmithScene =
							new FDatasmithFacadeScene(HOST_NAME, VENDOR_NAME, Autodesk.Navisworks.Api.Application.Version.Runtime, ProductVersion);
						DatasmithScene.PreExport();
						DatasmithScene.SetOutputPath(System.IO.Path.GetDirectoryName(FilePath));
						DatasmithScene.SetName(System.IO.Path.GetFileNameWithoutExtension(FilePath));

						SceneContext SceneContext = new SceneContext
						{
							CentimetersPerUnit = CentimetersPerUnit,
							DatasmithScene = DatasmithScene,
							ExportParameters =  ExportParameters,
							ItemFilterParams = ItemFilterParams,
							State = State,
							ModelItemCount = Models.RootItemDescendantsAndSelf.Count() // An estimate for total scene item count
						};

						SceneContext.NavisworksToUnrealTransform = CreateNavisworksToUnrealTransform(SceneContext);

						string HashedMaterialName = FDatasmithFacadeElement.GetStringHash("DefaultMaterial");
						FDatasmithFacadeUEPbrMaterial Material = new FDatasmithFacadeUEPbrMaterial(HashedMaterialName);
						Material.SetLabel("DefaultMaterial");
						Material.SetTwoSided(true); // Navisworks doesn't seem to be consistent on vertices winding order

						FDatasmithFacadeMaterialExpressionColor ColorExpression = Material.AddMaterialExpressionColor();
						ColorExpression.SetName("Base Color");
						ColorExpression.SetColor(0.5f, 0.5f, 0.5f, 1.0f);
						ColorExpression.ConnectExpression(Material.GetBaseColor());

						FDatasmithFacadeMaterialExpressionScalar MetallicExpression = Material.AddMaterialExpressionScalar();
						MetallicExpression.SetName("Metallic");
						MetallicExpression.SetScalar(0);
						MetallicExpression.ConnectExpression(Material.GetMetallic());

						FDatasmithFacadeMaterialExpressionScalar SpecularExpression = Material.AddMaterialExpressionScalar();
						SpecularExpression.SetName("Specular");
						SpecularExpression.SetScalar(1.0f);
						SpecularExpression.ConnectExpression(Material.GetSpecular());

						// Consider Shininess inverse of Roughness
						FDatasmithFacadeMaterialExpressionScalar RoughnessExpression = Material.AddMaterialExpressionScalar();
						RoughnessExpression.SetName("Roughness");
						RoughnessExpression.SetScalar(0.5f);
						RoughnessExpression.ConnectExpression(Material.GetRoughness());

						SceneContext.DatasmithMaterials.Add(Material);
						SceneContext.DatasmithScene.AddMaterial(Material);

						SceneContext.DefaultMaterial = Material;

						ExportCameras(SceneContext);

						// Collect Navisworks scene nodes 
						{
							EventInfo("Retrieving Model Items");
							Info($"ModelItemCount={SceneContext.ModelItemCount}");
							ProgressBar.BeginSubOperation(0.1, "Retrieving Model Items");
							try
							{
								NodeContext Context = new NodeContext(SceneContext);
								SceneItem SceneItem = new SceneItem(null, State.CurrentPartition);
								CollectNodes(Context, SceneItem, out SceneContext.RootNode, out bool _, ProgressBar);
								if (ProgressBar.IsCanceled)
								{
									return 0;
								}

								// Output some early scene stats
								Info($"GeometryInstanceTotalCount={Context.SceneContext.GeometryInstanceTotalCount}");
								Info($"GroupInstanceTotalCount={Context.SceneContext.GroupInstanceTotalCount}");
								Info($"HiddenNodeCount={Context.SceneContext.HiddenNodeCount}");
								Info($"GeometryNodeCount={Context.SceneContext.GeometryNodeCount}");
								Info($"GroupNodeCount={Context.SceneContext.GroupNodeCount}");
								Info($"InsertNodeCount={Context.SceneContext.InsertNodeCount}");
								Info($"LayerNodeCount={Context.SceneContext.LayerNodeCount}");

								Info($"Collected Nodes count={SceneContext.SceneItemCount}");

								EventInfo("Done - Retrieving Model Items");
								LogStream?.Flush(); // let all scene info be written to file before starting long geometry export operation
							}
							finally
							{
								ProgressBar.EndSubOperation();
							}
						}

						{
							EventInfo("Extracting Geometry");
							ProgressBar.BeginSubOperation(0.3, "Extracting Geometry");

							try
							{
								ExtractGeometry(SceneContext, ProgressBar);
								if (ProgressBar.IsCanceled)
								{
									return 0;
								}
							}
							finally
							{
								ProgressBar.EndSubOperation();
							}

							// Output stats
							Info($"SceneContext.Appearances.Count={SceneContext.Appearances.Count}");
							foreach (Appearance SceneContextAppearance in SceneContext.Appearances.Keys)
							{
								List<string> L = SceneContextAppearance.AppearanceValues.Select(Value => Value.ToString()).ToList();
								Info($"    Appearance: {SceneContextAppearance.Name} {string.Join(", ", L)}");
							}
							Info(
								$"MultipleFragmentGeometryWithDistinctTransform={SceneContext.MultipleFragmentGeometryWithDistinctTransform}");
							Info(
								$"MultipleFragmentGeometryWithDistinctTransformFragmentCount={SceneContext.MultipleFragmentGeometryWithDistinctTransformFragmentCount}");
							Info(
								$"MultipleFragmentGeometryWithSameTransform={SceneContext.MultipleFragmentGeometryWithSameTransform}");
							Info(
								$"MultipleFragmentGeometryWithSameTransformFragmentCount={SceneContext.MultipleFragmentGeometryWithSameTransformFragmentCount}");

							Info(
								$"MultipleFragmentGeometryWithDistinctAppearance={SceneContext.MultipleFragmentGeometryWithDistinctAppearance}");
							Info(
								$"MultipleFragmentGeometryWithDistinctAppearanceFragmentCount={SceneContext.MultipleFragmentGeometryWithDistinctAppearanceFragmentCount}");
							Info(
								$"MultipleFragmentGeometryWithSameAppearance={SceneContext.MultipleFragmentGeometryWithSameAppearance}");
							Info(
								$"MultipleFragmentGeometryWithSameAppearanceFragmentCount={SceneContext.MultipleFragmentGeometryWithSameAppearanceFragmentCount}");

							Info($"IdenticalItemReused={SceneContext.IdenticalItemReused}");

							EventInfo("Done - Extracting Geometry");
							LogStream?.Flush();
						}

						PrintAllocationStats(SceneContext);

						{
							EventInfo("Preparing Merged Meshes");
							MergeMeshesForNodeTree(SceneContext, ProgressBar);
							if (ProgressBar.IsCanceled)
							{
								return 0;
							}
							EventInfo("Done - Preparing Merged Meshes");
						}

						PrintAllocationStats(SceneContext);

						{
							EventInfo("Optimizing Intermediate scene");
							ProgressBar.BeginSubOperation(0.05, "Optimizing Intermediate scene");
							OptimizeIntermediateScene(SceneContext, ProgressBar);
							if (ProgressBar.IsCanceled)
							{
								return 0;
							}
							ProgressBar.EndSubOperation();
							EventInfo("Done - Optimizing Intermediate scene");
						}

						{
							EventInfo("Creating Meshes");

							CollectGeometryInstancesForNodeTree(SceneContext, ProgressBar);
							if (ProgressBar.IsCanceled)
							{
								return 0;
							}
							EventInfo("Done - CollectGeometryInstancesForNodeTree");

							if (!CreateDatasmithMeshes(SceneContext, ProgressBar))
							{
								return 0;
							}

							EventInfo("Done - Creating Meshes");
						}
						PrintAllocationStats(SceneContext);

						{
							EventInfo("Creating Actors");
							ProgressBar.BeginSubOperation(0.1, "Creating Actors");
							try
							{
								CreateActors(SceneContext, ProgressBar);
								if (ProgressBar.IsCanceled)
								{
									return 0;
								}
							}
							finally{
								ProgressBar.EndSubOperation();
							}
							EventInfo("Done - Creating Actors");
						}

						{
							EventInfo("Set Actor Transforms");
							ProgressBar.BeginSubOperation(0.1, "Set Actor Transforms");
							try
							{
								SetActorTransforms(SceneContext, ProgressBar);
								if (ProgressBar.IsCanceled)
								{
									return 0;
								}
							}
							finally
							{
								ProgressBar.EndSubOperation();
							}
							EventInfo("Done - Set Actor Transforms");
						}

						{
							EventInfo("Building Scene Hierarchy");
							ProgressBar.BeginSubOperation(0.1, "Building Scene Hierarchy");
							try
							{
								BuildSceneHierarchy(SceneContext, ProgressBar);
								if (ProgressBar.IsCanceled)
								{
									return 0;
								}
								EventInfo("Done - Building Scene Hierarchy");
							}
							finally
							{
								ProgressBar.EndSubOperation();
							}
						}

						{
							EventInfo("Reading Metadata");
							ProgressBar.BeginSubOperation(0.2, "Reading Metadata");
							try
							{
								ReadMetadata(SceneContext, ProgressBar);
								if (ProgressBar.IsCanceled)
								{
									return 0;
								}
								EventInfo("Done - Reading Metadata");
							}
							finally
							{
								ProgressBar.EndSubOperation();
							}
						}

						Info($"DuplicateMeshCount={SceneContext.DuplicateMeshCount}");
						Info($"Created Datasmith Scene with {SceneContext.DatasmithActors.Count} Actors, {SceneContext.DatasmithActorMeshes.Count} Meshes, Unique {SceneContext.DatasmithMeshElements.Count} Meshes, {SceneContext.TotalTriangleCount} TotalTriangleCount (VertexCount {SceneContext.TotalVertexCount}, Materials {SceneContext.DatasmithMaterials.Count})");

						{
							// Build and export the Datasmith scene instance and its scene element assets.
							EventInfo("Saving Datasmith scene");
							ProgressBar.BeginSubOperation(1, "Saving Datasmith scene");
							SaveDatasmithScene(DatasmithScene, FilePath);
							ProgressBar.EndSubOperation();
							EventInfo("Done - Saving Datasmith scene");
						}
					} 
					finally
					{
						Autodesk.Navisworks.Api.Application.EndProgress();
					}

				}
				finally
				{
					LogStream.Close();
					LogStream = null;
				}
			}
			catch (System.Exception Ex)
			{
				DisplayWarning(Ex.ToString());
			}

			return 0;
		}

		private void PrintAllocationStats(SceneContext SceneContext)
		{
			TriangleReader TriangleReader = SceneContext.TriangleReader;
			EventInfo("Geometry Allocation Stats:");

			Info($"  GeometryCount={TriangleReader.GetGeometryCount()}");
			Info($"  TriangleCount={TriangleReader.GetTriangleCount()}");
			Info($"  VertexCount={TriangleReader.GetVertexCount()}");

			Info($"  CoordBytesUsed={TriangleReader.GetCoordBytesUsed()}");
			Info($"  NormalBytesUsed={TriangleReader.GetNormalBytesUsed()}");
			Info($"  UvBytesUsed={TriangleReader.GetUvBytesUsed()}");
			Info($"  IndexBytesUsed={TriangleReader.GetIndexBytesUsed()}");

			Info($"  CoordBytesReserved={TriangleReader.GetCoordBytesReserved()}");
			Info($"  NormalBytesReserved={TriangleReader.GetNormalBytesReserved()}");
			Info($"  UvBytesReserved={TriangleReader.GetUvBytesReserved()}");
			Info($"  IndexBytesReserved={TriangleReader.GetIndexBytesReserved()}");

			ulong TotalMemoryReserved = TriangleReader.GetCoordBytesReserved() +
			                                  TriangleReader.GetNormalBytesReserved() +
			                                  TriangleReader.GetUvBytesReserved() +
			                                  TriangleReader.GetIndexBytesReserved();
			Info($"  *TotalMemoryReserved={TotalMemoryReserved}");

			ulong TotalMemoryUsed = TriangleReader.GetCoordBytesUsed() +
			                                  TriangleReader.GetNormalBytesUsed() +
			                                  TriangleReader.GetUvBytesUsed() +
			                                  TriangleReader.GetIndexBytesUsed();
			Info($"  *TotalMemoryUsed={TotalMemoryUsed}");

			ulong ExpectedMemoryUsed = TriangleReader.GetTriangleCount() * 4 * 3 +
			                                      TriangleReader.GetVertexCount() * (8 * 3 + 4 * 5);
			Info($"  *ExpectedMemoryUsed={ExpectedMemoryUsed}");
		}

		private void ExportCameras(SceneContext SceneContext)
		{
			InwSavedViewsColl SavedViews = SceneContext.State.SavedViews();

			ExportCamerasFromSavedViews(SceneContext, SavedViews);
		}

		private void ExportCamerasFromSavedViews(SceneContext SceneContext, InwSavedViewsColl SavedViews, string NamePrefix="")
		{
			foreach (InwOpSavedView SavedView in SavedViews)
			{
				switch (SavedView.Type)
				{
					case nwESavedViewType.eSavedViewType_View:
					{
						if (SavedView is InwOpView View)
						{
							Info($"View.name: {View.name}");
							InwNvViewPoint ViewPoint = View.anonview.ViewPoint;
							InwNvCamera Camera = ViewPoint.Camera;

							FDatasmithFacadeActorCamera CameraActor =
								new FDatasmithFacadeActorCamera(
									FDatasmithFacadeElement.GetStringHash("Camera:" + NamePrefix + View.name));
							CameraActor.SetLabel(NamePrefix + View.name);

							Vector3d Position = SceneContext.NavisworksToUnrealTransform.TransformPosition(Converters.ConvertVector3(Camera.Position));

							CameraActor.SetCameraPosition((float) Position.X, (float) Position.Y, (float) Position.Z);

							Vector3d ViewDir = SceneContext.NavisworksToUnrealTransform.TransformVector(
								Converters.ConvertVector3(Camera.GetViewDir()));
							Vector3d UpVector = SceneContext.NavisworksToUnrealTransform.TransformVector(
								Converters.ConvertVector3(Camera.GetUpVector()));

							CameraActor.SetCameraRotation(
								(float) ViewDir.X, (float) ViewDir.Y, (float) ViewDir.Z,
								(float) UpVector.X, (float) UpVector.Y, (float) UpVector.Z);

							if (Camera.Projection == nwEProjection.eProjection_PERSPECTIVE)
							{
								float CameraAspectRatio = (float) Camera.AspectRatio;
								CameraActor.SetAspectRatio(CameraAspectRatio);
								CameraActor.SetFocalLength(CameraActor.GetSensorWidth() / CameraAspectRatio);
								try
								{
									CameraActor.SetFocusDistance((float) ViewPoint.FocalDistance);
								}
								catch(COMException)
								{
									// FocalDistance can be missing, ignore
								}
							}

							SceneContext.DatasmithScene.AddActor(CameraActor);
						}

						// CameraActor.SetFocalLength
						break;
					}					
					case nwESavedViewType.eSavedViewType_Folder:
					{
						if (SavedView is InwOpFolderView FolderView)
						{
							ExportCamerasFromSavedViews(SceneContext, FolderView.SavedViews(),
								NamePrefix + FolderView.name + "_");
						}
						break;
					}
				}
			}
		}

		private static ExportParameters ParseParameters(string[] Parameters, out string FilePath)
		{
			ExportParameters ExportParameters = new ExportParameters();

			FilePath = Parameters[0];

			foreach (string Parameter in Parameters.Skip(1))
			{
				string[] ParameterTokens = Parameter.Split(new char[] {'='});
				if (ParameterTokens.Length != 2)
				{
					DisplayWarning($"Error in parameter: '{Parameter}', expected 'Name=Value' format");
					continue;
				}

				string ParameterName = ParameterTokens[0];
				string ParameterValueString = ParameterTokens[1];

				switch (ParameterName)
				{
					case "Merge":
					{
						if (!int.TryParse(ParameterValueString, NumberStyles.Integer, CultureInfo.InvariantCulture,
							out int Value))
						{
							DisplayWarning($"Error in parameter: '{Parameter}', can't convert '{ParameterValueString}' to int");
							continue;
						}

						ExportParameters.MergeMaxDepth = Value;
						break;
					}
					case "IncludeMetadata":
					{
						if (!bool.TryParse(ParameterValueString, out bool Value))
						{
							DisplayWarning(
								$"Error in parameter: '{Parameter}', can't convert '{ParameterValueString}' to bool");
							continue;
						}

						ExportParameters.bIncludeMetadata = Value;
						break;
					}
					case "Origin":
					{
						string[] OriginTokens = ParameterValueString.Split(new char[] {','});

						bool bIsValid = false;
						try
						{
							if (OriginTokens.Length != 3)
							{
								continue;
							}

							List<double> Doubles = new List<double>(3);
							foreach (string OriginToken in OriginTokens)
							{
								if (!double.TryParse(OriginToken, NumberStyles.Float, CultureInfo.InvariantCulture,
									out double FloatValue))
								{
									continue;
								}

								Doubles.Add(FloatValue);
							}

							ExportParameters.Origin.X = Doubles[0];
							ExportParameters.Origin.Y = Doubles[1];
							ExportParameters.Origin.Z = Doubles[2];
							bIsValid = true;
						}
						finally
						{
							if (!bIsValid)
							{
								DisplayWarning(
									$"Error in parameter: '{Parameter}', can't convert '{ParameterValueString}' - expected comma-separated list of 3 floating-point values in invariant culture, e.g. 'Origin=1.0, 2.0, 3.0'");
							}
						}

						break;
					}
					default:
					{
						DisplayWarning(
							$"Error in parameter: '{Parameter}', '{ParameterName}' parameter name is not recognized!");
						continue;
					}
				}
			}

			return ExportParameters;
		}

		private static IEnumerable<Node> EnumerateNodes(Node RootNode)
		{
			Stack<Node> Stack = new Stack<Node>();
			Stack.Push(RootNode);
			while (Stack.Count != 0)
			{
				Node CurrentNode = Stack.Pop();
				foreach (Node Child in CurrentNode.Children)
				{
					Stack.Push(Child);
				}
				yield return CurrentNode;
			}
		}

		// Enumerate all nodes in intermediate scene tree, updating progress bar
		private static IEnumerable<Node> EnumerateNodesProgress(SceneContext SceneContext, Progress ProgressBar)
		{
			int Index = 0;
			foreach (Node CurrentNode in EnumerateNodes(SceneContext.RootNode))
			{
				ProgressBar.Update(Math.Min(1, (double) Index / SceneContext.SceneItemList.Count));
				yield return CurrentNode;
				Index++;
			}
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static void CreateActors(SceneContext SceneContext, Progress ProgressBar)
		{
			foreach (Node CurrentNode in EnumerateNodesProgress(SceneContext, ProgressBar))
			{
				if (ProgressBar.IsCanceled)
				{
					return;
				}

				SceneItem Item = CurrentNode.SceneItem;

				// Create actors only for nodes with children or with metadata
				// Geometry nodes will be added as MeshActors
				if ((CurrentNode.Children.Count > 0) || CurrentNode.HasMetadata() || (Item.Parent == null))
				{
					Debug.Assert(CurrentNode.SceneItem.DatasmithActor == null);
					CurrentNode.SceneItem.DatasmithActor = CreateDatasmithActorForSceneItem(SceneContext, CurrentNode.SceneItem);
				}
			}
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static void SetActorTransforms(SceneContext SceneContext, Progress ProgressBar)
		{
			SetActorTransforms(SceneContext.RootNode);
		}

		// Set transforms recursively computing subtree bounding boxes and setting translation to box center
		private static GeometryUtil.BoundingBox SetActorTransforms(Node Node)
		{
			GeometryUtil.BoundingBox BoundingBox = new GeometryUtil.BoundingBox();

			if (Node.Instance != null)
			{
				BoundingBox.Extend(Node.Instance.BoundingBox);
			}

			foreach (Node Child in Node.Children)
			{
				BoundingBox.Extend(SetActorTransforms(Child));
			}

			if (BoundingBox.IsSet())
			{
				Node.SceneItem.DatasmithActor?.SetWorldTransform(TransformMatrix.Identity
					.ConcatTranslation(BoundingBox.BottomCenter).Floats);
			}
			return BoundingBox;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private void ExtractGeometry(SceneContext SceneContext, Progress ProgressBar)
		{
			foreach (Node CurrentNode in EnumerateNodesProgress(SceneContext, ProgressBar))
			{
				if (ProgressBar.IsCanceled)
				{
					return;
				}

				SceneItem Item = CurrentNode.SceneItem;
				if (Item.ComNode.IsGeometry) // Is this a a Leaf geometry node
				{
					if (!ConvertGeometryItem(SceneContext, CurrentNode, ProgressBar))
					{
						return;
					}
				}
			}
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private void CollectGeometryInstancesForNodeTree(SceneContext SceneContext, Progress ProgressBar)
		{
			CollectGeometryInstancesForNodeTreeRecursive(SceneContext, ProgressBar, SceneContext.RootNode);
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private bool CollectGeometryInstancesForNodeTreeRecursive(SceneContext SceneContext, Progress ProgressBar,
			Node InNode)
		{
			InNode.Instance?.AddToExportSet();

			foreach (Node ChildNode in InNode.Children)
			{
				if (!CollectGeometryInstancesForNodeTreeRecursive(SceneContext, ProgressBar, ChildNode))
				{
					return false;
				}
			}

			return true;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private void MergeMeshesForNodeTree(SceneContext SceneContext, Progress ProgressBar)
		{
			if (!InspectSceneHierarchy(SceneContext, ProgressBar, SceneContext.RootNode))
			{
				return;
			}

			Info($"Merge max depth: {SceneContext.ExportParameters.MergeMaxDepth}");

			MergeMeshesForNodeTreeRecursive(SceneContext, ProgressBar, SceneContext.RootNode);
		}


		[MethodImpl(MethodImplOptions.NoInlining)]
		private bool InspectSceneHierarchy(SceneContext SceneContext, Progress ProgressBar,
			Node InNode)
		{
			InNode.GeometryDepth = InNode.HasGeometry() ? 0 : -1;
			InNode.GeometryCountInHierarchy = InNode.HasGeometry() ? 1 : 0;
			InNode.TriangleCountInHierarchy = InNode.GetTriangleCount();

			foreach (Node ChildNode in InNode.Children)
			{
				if (!InspectSceneHierarchy(SceneContext, ProgressBar, ChildNode))
				{
					return false;
				}

				// In case child has geometry somewhere in its hierarchy 
				if (ChildNode.GeometryDepth > -1)
				{
					InNode.GeometryDepth = Math.Max(ChildNode.GeometryDepth + 1, InNode.GeometryDepth);
					InNode.GeometryCountInHierarchy += ChildNode.GeometryCountInHierarchy;
					InNode.TriangleCountInHierarchy += ChildNode.TriangleCountInHierarchy;
				}
			}

			return true;
		}


		[MethodImpl(MethodImplOptions.NoInlining)]
		private bool MergeMeshesForNodeTreeRecursive(SceneContext SceneContext, Progress ProgressBar,
			Node InNode)
		{
			// Limit meshes merging
			const int MaxTriangleCount = 1000000; // limit triangle counts heavily - to avoid OOM
			if (InNode.GeometryDepth > SceneContext.ExportParameters.MergeMaxDepth || InNode.GeometryCountInHierarchy < 2 || InNode.TriangleCountInHierarchy > MaxTriangleCount)
			{
				foreach (Node ChildNode in InNode.Children)
				{
					if (!MergeMeshesForNodeTreeRecursive(SceneContext, ProgressBar, ChildNode))
					{
						return false;
					}
				}
			}
			else
			{
				// Merge instances

				Debug.Assert(!InNode.HasGeometry()); // Haven't seen group node that also has geometry

				List<FragmentGeometry> Geometries = new List<FragmentGeometry>();
				List<TransformMatrix> Transforms = new List<TransformMatrix>();
				List<Appearance> Appearances = new List<Appearance>();
				MergeNodeGeometryInstances(InNode, Geometries, Transforms, Appearances);

				SceneItem Item = InNode.SceneItem;
				InstancedWithInstancesToExport Instanced = CreateOrGetInstancedForInstancedGeometry(SceneContext, new SceneItemInstancedGeometry(Item, Geometries), Item);

				if (Instanced == null)
				{
					return true;
				}

				GeometryInstance Instance = Instanced.CreateInstance(InNode.SceneItem);
				foreach (TransformMatrix Transform in Transforms)
				{
					Instance.AddTransform(Transform);
				}
				foreach (Appearance Appearance in Appearances)
				{
					Instance.AddAppearance(Appearance);
				}
				// Replace node geometry and remove children
				InNode.Instance = Instance;
				InNode.Children.Clear();
			}

			return true;
		}

		private static InstancedWithInstancesToExport CreateOrGetInstancedForInstancedGeometry(SceneContext SceneContext,
			SceneItemInstancedGeometry InstancedGeometry, SceneItem Item)
		{
			if (SceneContext.StoredInstancedForGeometry.TryGetValue(InstancedGeometry,
				out InstancedWithInstancesToExport StoredInstanced))
			{
				SceneContext.IdenticalItemReused += 1;
				InstancedGeometry.Dispose();
				return StoredInstanced;
			}

			InstancedWithInstancesToExport Instanced = AddInstancedSceneItem(SceneContext, Item, InstancedGeometry);
			SceneContext.StoredInstancedForGeometry.Add(InstancedGeometry, Instanced);
			return Instanced;
		}

		private bool MergeNodeGeometryInstances(Node InNode, List<FragmentGeometry> Geometries, List<TransformMatrix> Transforms, List<Appearance> Appearances)
		{
			foreach (Node ChildNode in InNode.Children)
			{
				if (ChildNode.HasGeometry())
				{
					GeometryInstance Instance = ChildNode.Instance;
					foreach (FragmentGeometry FragmentGeometry in Instance.SharedInstancedForInstancesRef.InstancedGeometry.FragmentGeometries)
					{
						Geometries.Add(FragmentGeometry);
						FragmentGeometry.AddUser();
					}
					Transforms.AddRange(Instance.Transforms);
					Appearances.AddRange(Instance.Appearances);
				}

				if (!MergeNodeGeometryInstances(ChildNode, Geometries, Transforms, Appearances))
				{
					return false;
				}
			}

			return true;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private bool CreateDatasmithMeshes(SceneContext SceneContext, Progress ProgressBar)
		{
			// For each source geometry - create Datasmith Mesh and immediately deallocate source geometry(to greatly reduce memory footprint)
			Dictionary<SceneItemInstancedGeometry, MultiFragmentsGeometryInstances> MergedGeometryForSceneItemInstancedGeometry = new Dictionary<SceneItemInstancedGeometry, MultiFragmentsGeometryInstances>();

			EventInfo("Creating Datasmith Meshes");
			ProgressBar.BeginSubOperation(0.2, "Creating Datasmith Meshes");
			try
			{
				// Create MeshActor for each instance of FragmentGeometry
				Dictionary<FragmentGeometryWithAppearance, List<Tuple<GeometryInstance, int>> > InstancesForSingleFragment = new Dictionary<FragmentGeometryWithAppearance, List<Tuple<GeometryInstance, int>>>();
				foreach (InstancedWithInstancesToExport Instances in SceneContext.AllInstanceds)
				{
					if (Instances.InstanceList.Count == 0)
					{
						// Instanced geometry might have no instances in case those were merged into others
						Instances.InstancedGeometry.Dispose();
						continue;
					}

					int InstancesToMerge = 0;
					int InstancesToBake = 0;

					foreach (GeometryInstance Instance in Instances.InstanceList)
					{
						// Consider optimizations for multifragment geometry
						// If geometry is to be merged or 'baked' we don't release InstancedGeometry yet - it will be used during mesh spawn
						bool bMergeMultifragment = true;
						if (bMergeMultifragment && Instances.InstancedGeometry.NonEmptyGeometryCount > 1)
						{
							// 1. Can we merge geometry for this instance under same transform?
							bool bCanMerge = (new HashSet<TransformMatrix>(Instance.Transforms)).Count == 1;

							// 2. Bake all multifragment geometry which doesn't have common transform, not only those with Instances.InstanceList.Count == 1;
							// TODO: could try optimizing in case fragments have different transforms but relative positions of fragments 
							// for instances are the same - this way each instance can have single transform for that shared geometry
							bool bCanBake = true; 
							if (bCanMerge || bCanBake)
							{
								if (!MergedGeometryForSceneItemInstancedGeometry.TryGetValue(
									Instances.InstancedGeometry,
									out MultiFragmentsGeometryInstances Mergeds))
								{
									Mergeds = new MultiFragmentsGeometryInstances();
									MergedGeometryForSceneItemInstancedGeometry[Instances.InstancedGeometry] = Mergeds;
								}

								// If possible - merge first! If so happened that fragments have same transform - this coincidence can very well happen on other meshes too. E.g. in case it's an instance of some object that wasn't reported by Navisworks as instance.
								if (bCanMerge)
								{
									MultiFragmentsGeometryInstances.AppearanceList AppearanceList = new MultiFragmentsGeometryInstances.AppearanceList(Instance.Appearances);
									if (!Mergeds.WithSameTransform.TryGetValue(AppearanceList,
										out List<GeometryInstance> InstancesForAppearanceList))
									{
										InstancesForAppearanceList = new List<GeometryInstance>();
										Mergeds.WithSameTransform.Add(AppearanceList, InstancesForAppearanceList);
									}
									InstancesForAppearanceList.Add(Instance);
									InstancesToMerge += 1;
									continue;
								}
								else if (bCanBake)
								{
									Mergeds.WithDifferentTransform.Add(Instance);
									InstancesToBake += 1;
									continue;
								}
							}
						}
						
						// 3. Or just instantiate each Fragment geometry as separate mesh
						foreach (var It in Instances.InstancedGeometry.FragmentGeometries.Select((V, I) => new {FragmentIndex=I, FragmentGeometry=V}))
						{
							FragmentGeometryWithAppearance FragmentGeometryWithAppearance = new FragmentGeometryWithAppearance(It.FragmentGeometry, Instance.GetAppearance(It.FragmentIndex));
							It.FragmentGeometry.AddUser(); // Will be released after datasmith mesh is created
						
							if (!InstancesForSingleFragment.TryGetValue(FragmentGeometryWithAppearance,
								out List<Tuple<GeometryInstance, int>> Actors))
							{
								Actors = new List<Tuple<GeometryInstance, int>>();
								InstancesForSingleFragment.Add(FragmentGeometryWithAppearance, Actors);
							}

							if (ProgressBar.IsCanceled)
							{
								return false;
							}

							Actors.Add(new Tuple<GeometryInstance, int>(Instance, It.FragmentIndex));
						}
					}

					// Release users for fragments that won't be used in baking or merging
					if (InstancesToMerge == 0 && InstancesToBake == 0)
					{
						Instances.InstancedGeometry.Dispose();
					}
				}

				int Count = InstancesForSingleFragment.Count;
				Info($"Meshes for single fragments:{Count}");
				int TotalSingleFragmentsInstanced = 0;
				foreach (var It in InstancesForSingleFragment.Select(
					(Kvp, Index) => new { Index, InstanceAndFragmentIndexList=Kvp.Value, Fragment=Kvp.Key}))
				{
					if (ProgressBar.IsCanceled)
					{
						return false;
					}
					ProgressBar.Update((double) It.Index / Count);

					FragmentGeometry FragmentGeometry = It.Fragment.FragmentGeometry;
					try
					{
						if (FragmentGeometry.GetGeometry(out Geometry Geometry))
						{
							FDatasmithFacadeMeshElement DatasmithMeshElement = CreateDatasmithMeshElementForItem(SceneContext, FragmentGeometry.SceneItem, It.Index);

							using (FDatasmithFacadeMesh DatasmithMesh = CreateDatasmithMeshForItem(SceneContext, FragmentGeometry.SceneItem, It.Index))
							{
								FillDatasmithMeshFromGeometry(Geometry, DatasmithMesh);
								ExportDatasmithMesh(SceneContext, DatasmithMesh, DatasmithMeshElement);
							}

							DatasmithMeshElement.SetMaterial(GetMaterialForFragmentGeometry(SceneContext, It.Fragment.Appearance), 0);

							SceneContext.TotalMeshSectionCount++;

							// Use this mesh for all instances
							foreach ((GeometryInstance Instance, int FragmentIndex) in It.InstanceAndFragmentIndexList)
							{
								TransformMatrix Transform = SceneContext.NavisworksToUnrealTransform.Multiply(Instance.GetTransform(FragmentIndex));

								FDatasmithFacadeActorMesh FragmentMeshActor = CreateDatasmithActorMeshForInstance(SceneContext, Instance, FragmentIndex);
								FragmentMeshActor.SetWorldTransform(Transform.Floats);
								FragmentMeshActor.SetMesh(DatasmithMeshElement.GetName());

								TotalSingleFragmentsInstanced++;
								SceneContext.TotalMeshSectionInstancedCount++;

								Instance.BoundingBox = new GeometryUtil.BoundingBox();
								Instance.BoundingBox.Extend(Geometry, Transform);
							}
						}
					}	
					finally
					{
						FragmentGeometry.ReleaseUser();
					}
				}

				Info($"TotalSingleFragmentsInstanced: {TotalSingleFragmentsInstanced}");
			}
			finally
			{
				ProgressBar.EndSubOperation();
			}
			EventInfo("Done - Creating Datasmith Meshes");

			EventInfo("Creating Merged Datasmith Meshes");
			ProgressBar.BeginSubOperation(0.2, "Creating Merged Datasmith Meshes");
			try
			{
				int Count = MergedGeometryForSceneItemInstancedGeometry.Count;
				Info($"Meshes with multiple fragments:{Count}");
				int TotalMergedMeshes = 0;
				int TotalMergedMeshInstances = 0;
				int TotalBakedMeshInstances = 0;
				int TotalBakedMeshTriangles = 0;
				foreach (var It in MergedGeometryForSceneItemInstancedGeometry.Select(
					(Kvp, Index) => new {Index, Instances = Kvp.Value, Geometry = Kvp.Key}))
				{
					SceneItemInstancedGeometry InstancedGeometry = It.Geometry;
					MultiFragmentsGeometryInstances Instances = It.Instances;

					if (Instances.Count == 0)
					{
						continue;
					}

					ProgressBar.Update((double) It.Index / Count);

					bool bHasNonDegenerateTriangles = false;
					foreach (FragmentGeometry FragmentGeometry in InstancedGeometry.FragmentGeometries)
					{
						if (FragmentGeometry.GetGeometry(out Geometry Geometry))
						{
							bHasNonDegenerateTriangles = true;
							break;
						}
					}
					if (!bHasNonDegenerateTriangles)
					{
						continue;
					}

					// Merge meshes that don't have same transform(transform applied)
					foreach (GeometryInstance Instance in Instances.WithDifferentTransform)
					{
						if (ProgressBar.IsCanceled)
						{
							return false;
						}

						FDatasmithFacadeMeshElement DatasmithMeshElement = CreateDatasmithMeshElementForItem(SceneContext, Instance.SceneItem, 0);
						int SlotCount = AssignAppearancesListToDatasmithMesh(SceneContext, DatasmithMeshElement, Instance.Appearances, out List<int> SlotRemap);
						SceneContext.TotalMeshSectionCount += SlotCount;
						SceneContext.TotalMeshSectionInstancedCount += SlotCount;

						// For each mesh that is composed from fragments with distinct transforms create its own DatasmithMesh 
						using (FDatasmithFacadeMesh DatasmithMesh = CreateDatasmithMeshForBakedFragmentGeometry(SceneContext, Instance, SlotRemap, out Vector3d Translation))
						{
							if (DatasmithMesh == null)
							{
								Info($"Datasmith Mesh wasn't created for {Instance.SceneItem?.ComNode?.UserName}");
								continue;
							}

							ExportDatasmithMesh(SceneContext, DatasmithMesh, DatasmithMeshElement);
							TotalBakedMeshTriangles += DatasmithMesh.GetFacesCount();

							FDatasmithFacadeActorMesh ItemMeshActor =
								CreateDatasmithActorMeshForInstance(SceneContext, Instance, 0);
							// Translation is expected to be in export(Unreal) coordinates already
							TransformMatrix Transform = TransformMatrix.Identity.ConcatTranslation(Translation);
							ItemMeshActor.SetWorldTransform(Transform.Floats);
							ItemMeshActor.SetMesh(DatasmithMeshElement.GetName());
						}

						TotalBakedMeshInstances++;
					}

					// Merge meshes that have same transform
					if (Instances.WithSameTransform.Count > 0)
					{
						foreach (var AppearanceListAndInstances in Instances.WithSameTransform.Select((Value, Index) =>
							new {Index, AppearanceList = Value.Key, GeometryInstances = Value.Value}))
						{
							MultiFragmentsGeometryInstances.AppearanceList AppearanceList =
								AppearanceListAndInstances.AppearanceList;
							List<GeometryInstance> GeometryInstances = AppearanceListAndInstances.GeometryInstances;

							FDatasmithFacadeMeshElement DatasmithMeshElement = CreateDatasmithMeshElementForItem(SceneContext, InstancedGeometry.Item, It.Index);
							int SlotCount = AssignAppearancesListToDatasmithMesh(SceneContext, DatasmithMeshElement, AppearanceList.Appearances, out List<int> SlotRemap);

							// Compute bounding box of consolidated geometry to repivot merged geometry to the bbox center
							// Bounding box is computed in local coordinates(before item transform applied)
							GeometryUtil.BoundingBox BoundingBox = new GeometryUtil.BoundingBox();
							foreach (FragmentGeometry FragmentGeometry in InstancedGeometry.FragmentGeometries)
							{
								OptimizeSourceGeometry(FragmentGeometry);
								if (FragmentGeometry.GetGeometry(out Geometry Geometry))
								{
									BoundingBox.Extend(Geometry);
								}
							}
							Vector3d PivotTranslation = BoundingBox.BottomCenter;
							TransformMatrix CenterTransform = TransformMatrix.Identity.ConcatTranslation(PivotTranslation);

							// TODO: Potential optimization - same mesh might have different set of appearances
							// Right now we are creating separate mesh for each appearance list, might want to make override materials on mesh actor
							using (FDatasmithFacadeMesh MergedDatasmithMesh = CreateDatasmithMeshForItem(SceneContext, InstancedGeometry.Item, AppearanceListAndInstances.Index)) // Instantiate mesh with different name for each appearance
							{
								TotalMergedMeshes++;
								FillDatasmithMeshFromInstancedGeometry(InstancedGeometry, MergedDatasmithMesh, SlotRemap, -PivotTranslation);

								ExportDatasmithMesh(SceneContext, MergedDatasmithMesh, DatasmithMeshElement);

								SceneContext.TotalMeshSectionCount += SlotCount;
							}


							foreach (GeometryInstance Instance in GeometryInstances)
							{
								if (ProgressBar.IsCanceled)
								{
									return false;
								}

								FDatasmithFacadeActorMesh ItemMeshActor = CreateDatasmithActorMeshForInstance(SceneContext, Instance, 0);

								// Repivot to BBox center. Note - CenterTransform applied first(i.e. to local vertex positions)
								TransformMatrix TransformRepivoted = Instance.GetTransform(0).Multiply(CenterTransform);
								TransformMatrix WorldTransform = SceneContext.NavisworksToUnrealTransform.Multiply(TransformRepivoted);
								ItemMeshActor.SetWorldTransform(WorldTransform.Floats);
								ItemMeshActor.SetMesh(DatasmithMeshElement.GetName());
								Instance.BoundingBox = BoundingBox.Transform(WorldTransform);

								TotalMergedMeshInstances++;
								SceneContext.TotalMeshSectionInstancedCount += SlotCount;
							}
						}
					}
					InstancedGeometry.Dispose();
				}
				Info($"TotalBakedMeshTriangles: {TotalBakedMeshTriangles}");
				Info($"TotalBakedMeshInstances: {TotalBakedMeshInstances}");
				Info($"TotalMergedMeshInstances: {TotalMergedMeshInstances}");

				Info($"TotalMeshSectionCount: {SceneContext.TotalMeshSectionCount}");
				Info($"TotalMeshSectionInstancedCount: {SceneContext.TotalMeshSectionInstancedCount}");
			} 
			finally
			{
				ProgressBar.EndSubOperation();
			}


			EventInfo("Done Creating Merged Datasmith Meshes");

			return true;
		}

		private static TransformMatrix CreateNavisworksToUnrealTransform(SceneContext SceneContext)
		{
			InwOaPartition3 Partition = ((InwOaPartition3) SceneContext.State.CurrentPartition);
			Vector3d WorldNorth = Converters.ConvertVector3(Partition.GetNorthVector());
			Vector3d WorldUp = Converters.ConvertVector3(Partition.GetUpVector());

			// Convert to UE/export space by subtracting Origin and rotating to have North to match Unreal X and Up - Unreal Z
			Vector3d Z = WorldUp;
			Vector3d X = WorldNorth;
			Vector3d Y = Z.Cross(X);
			TransformMatrix Orientation = new TransformMatrix(new[]
			{
				X.X, Y.X, Z.X, 0,
				X.Y, Y.Y, Z.Y, 0,
				X.Z, Y.Z, Z.Z, 0,
				0, 0, 0, 1,
			});
			Vector3d Translation = Orientation.TransformPosition(-SceneContext.ExportParameters.Origin);

			return Orientation.ConcatTranslation(Translation);
		}

		private static FDatasmithFacadeMetaData CreateDatasmithMetadata(FDatasmithFacadeActor FragmentMeshActor, GuiProperties GuiProperties)
		{
			FDatasmithFacadeMetaData MeshActorMetaData = new FDatasmithFacadeMetaData(FragmentMeshActor.GetName() + "_DATA");

			foreach ((string AttributeName, GuiProperties.Attribute Attribute) in GuiProperties.EnumerateAttributes())
			{
				foreach ((string PropertyName, string PropertyValue) in Attribute.EnumerateProperties())
				{
					MeshActorMetaData.AddPropertyString($"{AttributeName}*{PropertyName}", PropertyValue);
				}
			}

			if (MeshActorMetaData.GetPropertiesCount() > 0)
			{
				MeshActorMetaData.SetAssociatedElement(FragmentMeshActor);
				return MeshActorMetaData;
		}

			return null;
		}

		private int AssignAppearancesListToDatasmithMesh(SceneContext SceneContext, FDatasmithFacadeMeshElement DatasmithMeshElement,
			List<Appearance> Appearances, out List<int> SlotRemap)
		{
			int SlotCount = 0;
			Dictionary<Appearance, int> SlotForAppearance = new Dictionary<Appearance, int>();
			SlotRemap = new List<int>(Appearances.Count);
			foreach (var AppIt in Appearances.Select((Value, Index) =>
				new {Index, Appearance = Value}))
			{
				Appearance Appearance = AppIt.Appearance;
				if (!SlotForAppearance.TryGetValue(Appearance, out int Slot))
				{
					Slot = SlotCount;
					SlotCount++;
					SlotForAppearance.Add(Appearance, Slot);
					DatasmithMeshElement.SetMaterial(GetMaterialForFragmentGeometry(SceneContext, Appearance), Slot);
				}

				SlotRemap.Add(Slot);
			}

			return SlotCount;
		}

		private static string GetMaterialForFragmentGeometry(SceneContext SceneContext, Appearance Appearance)
		{
			if (Appearance == null)
			{
				return SceneContext.DefaultMaterial.GetName();
			}

			if (!SceneContext.DatasmithMaterialForAppearance.TryGetValue(Appearance,
				out FDatasmithFacadeUEPbrMaterial Material))
			{
				string Name = Appearance.Name.Length != 0 ? Appearance.Name : $"FragmentMaterial_{SceneContext.DatasmithMaterialForAppearance.Count}";
				Material = new FDatasmithFacadeUEPbrMaterial(FDatasmithFacadeElement.GetStringHash(Name));
				Material.SetLabel(Name);

				Material.SetTwoSided(true); // Navisworks doesn't seem to be consistent on vertices winding order

				float[] AmbientColor = {(float)Appearance.AppearanceValues[0], (float)Appearance.AppearanceValues[1], (float)Appearance.AppearanceValues[2]};
				float[] DiffuseColor = {(float)Appearance.AppearanceValues[3], (float)Appearance.AppearanceValues[4], (float)Appearance.AppearanceValues[5]};
				float[] SpecularColor = {(float)Appearance.AppearanceValues[6], (float)Appearance.AppearanceValues[7], (float)Appearance.AppearanceValues[8]};
				float[] EmissiveColor = {(float)Appearance.AppearanceValues[9], (float)Appearance.AppearanceValues[10], (float)Appearance.AppearanceValues[11]};
				float Shininess = (float)Appearance.AppearanceValues[12];
				float Transparency = (float)Appearance.AppearanceValues[13];

				float SpecularIntensity = SpecularColor.Max();

				FDatasmithFacadeMaterialExpressionColor ColorExpression = Material.AddMaterialExpressionColor();
				ColorExpression.SetName("Base Color");
				ColorExpression.SetColor(DiffuseColor[0], DiffuseColor[1], DiffuseColor[2], 1.0f);
				ColorExpression.ConnectExpression(Material.GetBaseColor());

				FDatasmithFacadeMaterialExpressionScalar MetallicExpression = Material.AddMaterialExpressionScalar();
				MetallicExpression.SetName("Metallic");
				MetallicExpression.SetScalar(0);
				MetallicExpression.ConnectExpression(Material.GetMetallic());

				FDatasmithFacadeMaterialExpressionScalar SpecularExpression = Material.AddMaterialExpressionScalar();
				SpecularExpression.SetName("Specular");
				SpecularExpression.SetScalar(SpecularIntensity);
				SpecularExpression.ConnectExpression(Material.GetSpecular());

				// Consider Roughness is high when Shininess is low or Specular intensity is very low
				FDatasmithFacadeMaterialExpressionScalar RoughnessExpression = Material.AddMaterialExpressionScalar();
				RoughnessExpression.SetName("Roughness");
				RoughnessExpression.SetScalar((float)Math.Pow(1.0f - Math.Min(SpecularIntensity/0.1f, 1.0f)*Shininess, 2.0f));
				RoughnessExpression.ConnectExpression(Material.GetRoughness());

				FDatasmithFacadeMaterialExpressionColor EmissiveExpression = Material.AddMaterialExpressionColor();
				EmissiveExpression.SetName("Emissive");
				EmissiveExpression.SetColor(EmissiveColor[0], EmissiveColor[1], EmissiveColor[2], 1.0f);
				EmissiveExpression.ConnectExpression(Material.GetEmissiveColor());

				if (Transparency > float.Epsilon)
				{
					Material.SetBlendMode(/*EBlendMode::BLEND_Translucent*/2);
					FDatasmithFacadeMaterialExpressionScalar Scalar = Material.AddMaterialExpressionScalar();
					Scalar.SetName("Opacity");
					Scalar.SetScalar(1 - Transparency);
					Scalar.ConnectExpression(Material.GetOpacity());
				}

				SceneContext.DatasmithMaterials.Add(Material);

				SceneContext.DatasmithMaterialForAppearance.Add(Appearance, Material);

				SceneContext.DatasmithScene.AddMaterial(Material);
			}
			return Material.GetName();
		}

		struct NodeGeometry
		{
		};

		// Reflects scene hierarchy to export
		private class Node
		{
			public SceneItem SceneItem;

			public GeometryInstance Instance;

			public List<Node> Children = new List<Node>();
			public int GeometryDepth;
			public int GeometryCountInHierarchy;
			public long TriangleCountInHierarchy;

			public bool HasGeometry() 
			{
				return Instance != null;
			}

			public long GetTriangleCount()
			{
				return HasGeometry() ? Instance.SharedInstancedForInstancesRef.InstancedGeometry.TriangleCount() : 0;
			}

			public bool HasMetadata()
			{
				return Instance?.Metadata != null;
			}
		};

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static void BuildSceneHierarchy(SceneContext SceneContext, Progress ProgressBar)
		{

			foreach (Node CurrentNode in EnumerateNodesProgress(SceneContext, ProgressBar))
			{
				if (ProgressBar.IsCanceled)
				{
					return;
				}

				SceneItem Item = CurrentNode.SceneItem;

				// If a node has no DatasmithActor this means it's a leaf geometry node
				if (Item.DatasmithActor == null)
				{
					continue;
				}

				FDatasmithFacadeActorMesh MeshActor = CurrentNode.Instance?.MeshActor;
				if (MeshActor != null)
				{
					Item.DatasmithActor.AddChild(MeshActor);
				}

				foreach (Node Child in CurrentNode.Children)
				{
					Item.DatasmithActor.AddChild(Child.SceneItem.DatasmithActor ?? Child.Instance.MeshActor);
				}

				if (Item.Parent == null)
				{
					// Add root actor to datasmith scene
					SceneContext.DatasmithScene.AddActor(Item.DatasmithActor);
				}
			}
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static void OptimizeIntermediateScene(SceneContext SceneContext, Progress ProgressBar)
		{
			OptimizeIntermediateSceneRecursive(SceneContext, ProgressBar, ref SceneContext.RootNode);
		}

		// Returns false when Node can be optimized out entirely
		private static bool OptimizeIntermediateSceneRecursive(SceneContext SceneContext, Progress ProgressBar,
			ref Node InOutNode)
		{
			for (int ChildIndex = InOutNode.Children.Count - 1; ChildIndex >= 0; ChildIndex--)
			{
				Node Child = InOutNode.Children[ChildIndex];
				if (OptimizeIntermediateSceneRecursive(SceneContext, ProgressBar, ref Child))
				{
					InOutNode.Children[ChildIndex] = Child;
				}
				else
				{
					// Child can be removed
					InOutNode.Children.RemoveAt(ChildIndex);
				}
			}

			// Don't remove actor if it has geometry or metadata
			if (InOutNode.HasGeometry() || InOutNode.HasMetadata())
			{
				return true;
			}

			switch (InOutNode.Children.Count)
			{
				case 0:
				{
					// Remove empty actor with no children
					return false;
				}					
				case 1:
				{
					// Remove empty intermediate actor with single child, keeping the child
					SceneItem NodeSceneItem = InOutNode.SceneItem;
					InOutNode = InOutNode.Children[0];
					// Use ancestor node SceneItem for optimized node
					// This way actor label used will be more meaningful(e.g. leaf geometry nodes are named 'Solid' etc)
					InOutNode.SceneItem = NodeSceneItem;
					return true;
				}
			}
			return true;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static void ReadMetadata(SceneContext SceneContext, Progress ProgressBar)
		{
			foreach (Node Node in EnumerateNodesProgress(SceneContext, ProgressBar))
			{
				if (ProgressBar.IsCanceled)
				{
					return;
				}

				FDatasmithFacadeActor DatasmithActor = Node.SceneItem.DatasmithActor;

				if (DatasmithActor != null)
				{
					AddMetadataForSceneItemToDatasmithActor(SceneContext, Node.SceneItem, DatasmithActor);
				}

				FDatasmithFacadeActor DatasmithMeshActor = Node.Instance?.MeshActor;
				if (DatasmithMeshActor != null)
				{
					// Add metadata to MeshActor, if it was built for different SceneItem(this happens when scene was optimized so that single child geometry instance was moved to parent)
					// or Node's DatasmithActor is not present(it's omitted when there's nothing else in the hierarchy except maybe a single MeshActor)
					SceneItem InstanceSceneItem = Node.Instance.SceneItem;
					if (((InstanceSceneItem != Node.SceneItem) || DatasmithActor == null))
					{
						AddMetadataForSceneItemToDatasmithActor(SceneContext, InstanceSceneItem, DatasmithMeshActor);
					}
				}
			}
		}

		private static void AddMetadataForSceneItemToDatasmithActor(SceneContext SceneContext, SceneItem Item,
			FDatasmithFacadeActor DatasmithActor)
		{
			if (!GetSceneItemComPath(SceneContext, Item, out InwOaPath ComPath))
			{
				return;
			}

			GuiProperties GuiProperties = new GuiProperties();
			ExtractProperties(SceneContext, ComPath, GuiProperties);

			if (DatasmithActor == null)
			{
				return;
			}

			FDatasmithFacadeMetaData DatasmithMetaData = CreateDatasmithMetadata(DatasmithActor, GuiProperties);
			if (DatasmithMetaData != null)
			{
				SceneContext.DatasmithScene.AddMetaData(DatasmithMetaData);
			}
		}

		private static bool GetSceneItemComPath(SceneContext SceneContext, SceneItem Item, out InwOaPath ComPath)
		{
			if (SceneContext.ItemComPathForPath.TryGetValue(Item.Path, out ComPath))
			{
				return true;
			}
			ReadPathsForSceneItemComNodeInstances(SceneContext, Item);
			return SceneContext.ItemComPathForPath.TryGetValue(Item.Path, out ComPath);
		}

		// InwOaPath is needed to read Item "gui properties"(metadata) and retrieve IsHidden value on ModelItem
		// We don't have direct way to get InwOaPath3 for each Item in the scene tree
		// Each Com node(InwOaNode) can have a number on instances withing navisworks scene hierarchy 
		// This instance corresponds to a 'scene item' and identified by 'path'(InwOaPath in Com API)
		private static void ReadPathsForSceneItemComNodeInstances(SceneContext SceneContext, SceneItem SceneItem)
		{
			InwOaPath3 NwOaPath = (InwOaPath3) SceneContext.State.GetFirstInstanceOfNode(SceneItem.ComNode);
			while (NwOaPath != null)
			{
				SceneItemPath SceneItemPath = new SceneItemPath(NwOaPath);
				SceneContext.ItemComPathForPath[SceneItemPath] = NwOaPath;
				NwOaPath = (InwOaPath3) NwOaPath.GetNextInstance();
			}
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static void SaveDatasmithScene(FDatasmithFacadeScene DatasmithScene, string FilePath)
		{
			DatasmithScene.ExportScene(FilePath);
		}

		private IEnumerable<InwOaFragment3> ItemFragments(SceneContext SceneContext, SceneItem Item)
		{
			InwNodeFragsColl Fragments = Item.ComNode.Fragments();
			foreach (InwOaFragment3 Fragment in Fragments)
			{
				yield return Fragment;
			}

			// Without this Navisworks COM api collection can be released by .NET runtime too soon making collection items deallocated too(implementation detail of NW COM api)
			GC.KeepAlive(Fragments);
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private bool ConvertGeometryItem(SceneContext SceneContext, Node Node, Progress ProgressBar)
		{
			SceneItem Item = Node.SceneItem;
			if (!SceneContext.NavisworksInstancesCollection.GetInstancesForItem(Item, out NavisworksNodeInstances NodeInstances))
			{
				return true;
			}

			// Geometry node is split into 'Fragments', each containing mesh and transform(and appearance)
			// SceneItem may be reported as 'instance' by the API - in this case we choose first as 'prototype' and read geometry from its fragments
			// for other instances  read only transforms/appearances from fragments 
			if (NodeInstances.SharedInstancedRef == null)
			{
				if (!CreateInstancedGeometryForItem(SceneContext, Item, NodeInstances, out SceneItemInstancedGeometry InstancedGeometry, ProgressBar))
				{
					return false;
				}
				NodeInstances.SharedInstancedRef = CreateOrGetInstancedForInstancedGeometry(SceneContext, InstancedGeometry, Item);
			}

			if (!SceneContext.ExportParameters.CREATE_INSTANCES_FOR_EMPTY_GEOMETRY 
			    && !NodeInstances.SharedInstancedRef.InstancedGeometry.HasTriangles())
			{
				return true;
			}

			// Create Instance for Instanced geometry, setting up Transform and Appearance for each fragment
			GeometryInstance Instance = NodeInstances.SharedInstancedRef.CreateInstance(Node.SceneItem);
			Node.Instance = Instance;
			foreach (NavisworksFragmentInstance NavisworksFragmentInstance in NodeInstances.GetFragmentInstances(Item.Path))
			{
				Instance.AddTransform(NavisworksFragmentInstance.FragmentTransform);
				Instance.AddAppearance(NavisworksFragmentInstance.Appearance);
			}

			if (SceneContext.ExportParameters.bIncludeMetadata)
			{
				if (NodeInstances.TryGetGuiPropertiesForPath(Item.Path, out GuiProperties GuiProperty))
				{
					Instance.SetMetadata(GuiProperty);
				}
			}
			return true;
		}

		private static Appearance CreateAppearance(SceneContext SceneContext, Array FragmentAppearance,
			InwOaFragment3 Fragment)
		{
			if (FragmentAppearance == null)
			{
				return null;
			}

			Appearance Appearance = new Appearance(FragmentAppearance);

			// Return stored appearance if the same appearance was already there
			if (SceneContext.Appearances.TryGetValue(Appearance, out Appearance StoredAppearance))
			{
				return StoredAppearance;
			}

			SceneContext.Appearances[Appearance] = Appearance;

			string MaterialName = GetFragmentMaterialName(Fragment);

			string NameBase = MaterialName;
			// Avoid duplicate names
			int Index = 0;
			while (SceneContext.AppearanceNames.Contains(MaterialName))
			{
				MaterialName = NameBase + Index;
				Index++;
			}
			SceneContext.AppearanceNames.Add(MaterialName);

			Appearance.Name = MaterialName;

			return Appearance;

		}

		private static string GetFragmentMaterialName(InwOaFragment3 Fragment)
		{
			// Find out material name and attach to the Appearance
			string Name = "";
			if (Fragment.path.Nodes().Last() is InwOaNode Node)
			{
				InwNodeAttributesColl NwNodeAttributesColl = Node.Attributes();
				foreach (InwOaAttribute Attribute in NwNodeAttributesColl)
				{
					if (Attribute is InwOaMaterial Material)
					{
						Name = Material.UserName;
					}
				}

				GC.KeepAlive(NwNodeAttributesColl);
			}

			return Name;
		}

		private static InstancedWithInstancesToExport AddInstancedSceneItem(SceneContext SceneContext, SceneItem Item, SceneItemInstancedGeometry InstancedGeometry)
		{
			InstancedWithInstancesToExport Instanced = new InstancedWithInstancesToExport
			{
				PrototypeItem = Item,
				InstancedGeometry = InstancedGeometry
			};
			SceneContext.AllInstanceds.Add(Instanced);
			return Instanced;
		}

		private bool CreateInstancedGeometryForItem(SceneContext SceneContext, SceneItem Item, NavisworksNodeInstances NavisworksInstances,
			out SceneItemInstancedGeometry InstancedGeometry, Progress ProgressBar)
		{
			InstancedGeometry = null;

			HashSet<TransformMatrix> FragmentTransformsSet = new HashSet<TransformMatrix>();
			HashSet<Appearance> FragmentAppearanceSet = new HashSet<Appearance>();
			List<FragmentGeometry> Geometries = new List<FragmentGeometry>();

			foreach (InwOaFragment3 Fragment in ItemFragments(SceneContext, Item))
			{
				if (ProgressBar.IsCanceled)
				{
					return false;
				}

				SceneItemPath FragmentPath = new SceneItemPath(Fragment);

				// Store item Com API path, so that we can quickly extract Properties(metadata) later from it
				SceneContext.ItemComPathForPath[FragmentPath] = Fragment.path;

				TransformMatrix FragmentTransform = Converters.ConvertMatrix(Fragment.GetLocalToWorldMatrix());
				Appearance Appearance = CreateAppearance(SceneContext, (Array)Fragment.Appearance, Fragment);

				NavisworksInstances.AddFragmentInstance(FragmentPath, FragmentTransform, Appearance);
				if (!Item.Path.Equals(FragmentPath))
				{
					// If this fragment is from another Item Path this means this if fragment of another instance of the node.
					// Skip extracting geometry in that case for this fragment. But keep instance's parameters(above - transform, appearance) so we don't need to parse fragments again(this is slow)
					continue;
				}

				FragmentGeometry FragmentGeometry;
				DatasmithNavisworksUtil.GeometrySettings ReadParams = new GeometrySettings();
				ReadParams.SetTriangleSizeThreshold(SceneContext.TriangleSizeThreshold);
				ReadParams.SetPositionThreshold(SceneContext.PositionThreshold);
				ReadParams.SetNormalThreshold(SceneContext.NormalThreshold);

				// SceneContext.TriangleSizeThreshold, SceneContext.PositionThreshold, SceneContext.NormalThreshold
				Geometry SourceGeometry = SceneContext.TriangleReader.ReadGeometry(Fragment, ReadParams);
				if (!GeometryUtil.HasNonDegenerateTriangles(SourceGeometry, SceneContext.PositionThreshold))
				{
					// Store empty object for convenience(we can't drop fragments - need to preserve count and order of fragments so that other instances match)
					FragmentGeometry = new FragmentGeometry();
				}
				else
				{
					// Test if we already have identical geometry stored and reuse it
					if (SceneContext.StoredFragmentGeometries.TryGetValue(SourceGeometry,
						out FragmentGeometry StoredFragmentGeometry))
					{
						SceneContext.DuplicateMeshCount += 1;
						// deallocate native data early, we are sure we don't need it anymore(we used same stored geometry)
						SourceGeometry.Dispose();
						StoredFragmentGeometry.AddUser();
						FragmentGeometry = StoredFragmentGeometry;
					}
					else
					{
						FragmentGeometry = new FragmentGeometry(SourceGeometry, Item);
						SceneContext.StoredFragmentGeometries.Add(SourceGeometry, FragmentGeometry);
					}
				}

				Geometries.Add(FragmentGeometry);

				FragmentTransformsSet.Add(FragmentTransform);
				FragmentAppearanceSet.Add(Appearance);
			}

			bool bFragmentsHaveSameTransform = FragmentTransformsSet.Count == 1;
			bool bFragmentsHaveSameAppearance = FragmentAppearanceSet.Count == 1;
			InstancedGeometry = new SceneItemInstancedGeometry(Item, Geometries);

			// Collect some stats for multifragment geoms
			if (Geometries.Count > 1)
			{
				if (bFragmentsHaveSameTransform)
				{
					SceneContext.MultipleFragmentGeometryWithSameTransform += 1;
					SceneContext.MultipleFragmentGeometryWithSameTransformFragmentCount += Geometries.Count;
				}
				else
				{
					SceneContext.MultipleFragmentGeometryWithDistinctTransform += 1;
					SceneContext.MultipleFragmentGeometryWithDistinctTransformFragmentCount +=
						Geometries.Count;
				}

				if (bFragmentsHaveSameAppearance)
				{
					SceneContext.MultipleFragmentGeometryWithSameAppearance += 1;
					SceneContext.MultipleFragmentGeometryWithSameAppearanceFragmentCount += Geometries.Count;
				}
				else
				{
					SceneContext.MultipleFragmentGeometryWithDistinctAppearance += 1;
					SceneContext.MultipleFragmentGeometryWithDistinctAppearanceFragmentCount +=
						Geometries.Count;
				}
			}

			return true;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static void ExtractProperties(SceneContext SceneContext, InwOaPath NodePath, GuiProperties GuiProperties)
		{
			InwGUIPropertyNode PropertyNode = SceneContext.State.GetGUIPropertyNode(NodePath, true);
			InwGUIAttributesColl GuiAttributes = PropertyNode.GUIAttributes();
			foreach (InwGUIAttribute2 GuiAttribute in GuiAttributes)
			{
				InwOaPropertyColl Properties = GuiAttribute.Properties();
				GuiProperties.Attribute Attribute = GuiProperties.AddAttribute(GuiAttribute.ClassUserName);
				foreach (InwOaProperty Property in Properties)
				{
					// name - is property identity
					// UserName is what displayed in Navisworks UI - on Properties tabs and in Find Items window. UserName for different properties in one Tab can match 
					// In Navisworks search is done by property identity(name). When two properties have same UseName Find Items will display identical names in the 'Property' list

					string PropertyName = Property.UserName;
					string PropertyValue = null;
					try
					{
						PropertyValue = Property.value?.ToString();
					}
					catch(COMException)
					{
						// 'value' COM field can be missing, raising exception on access
					}
					Attribute.SetProperty(PropertyName, PropertyValue);
				}

				// Without this Navisworks COM api collection can be released by .NET runtime too soon making collection items deallocated too(implementation detail of NW COM api)
				GC.KeepAlive(Properties);
			}
			// Without this Navisworks COM api collection can be released by .NET runtime too soon making collection items deallocated too(implementation detail of NW COM api)
			GC.KeepAlive(GuiAttributes);

			// This holds GuiAttribute instances
			GC.KeepAlive(PropertyNode);
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static FDatasmithFacadeActor CreateDatasmithActorForSceneItem(SceneContext SceneContext, SceneItem Item)
		{
			GetNameAndLabelFromItem(Item, out string Name, out string Label);
			FDatasmithFacadeActor ItemDatasmithActor = new FDatasmithFacadeActor(FDatasmithFacadeElement.GetStringHash(Name));
			ItemDatasmithActor.SetLabel(Label);
			SceneContext.DatasmithActors.Add(ItemDatasmithActor); // hold on to reference
			if (SceneContext.ExportParameters.USE_COMPONENTS)
			{
				ItemDatasmithActor.SetIsComponent(Item.bChildrenOnlySimple);
			}
			return ItemDatasmithActor;
		}

		private static FDatasmithFacadeActorMesh CreateDatasmithActorMeshForInstance(SceneContext SceneContext, GeometryInstance Instance, int MeshIndex)
		{
			GetNameAndLabelFromItem(Instance.SceneItem, out string Name, out string Label);
			string HashedName = FDatasmithFacadeElement.GetStringHash("F" + MeshIndex + ":" + Name);
			FDatasmithFacadeActorMesh ItemMeshActor = new FDatasmithFacadeActorMesh(HashedName);
			ItemMeshActor.SetLabel(Label + "_" + MeshIndex);
			SceneContext.DatasmithActorMeshes.Add(ItemMeshActor); // hold on to reference
			if (SceneContext.ExportParameters.USE_COMPONENTS)
			{
				ItemMeshActor.SetIsComponent(true);
			}

			Instance.MeshActor = ItemMeshActor;
			return ItemMeshActor;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static FDatasmithFacadeMesh CreateDatasmithMeshForItem(SceneContext SceneContext, SceneItem Item, int MeshIndex)
		{
			// Set mesh name/label from the Item that it's instantiated from
			GetNameAndLabelFromItem(Item, out string Name, out string Label);
			// Hash the Datasmith mesh name to shorten it and make it valid
			string HashedName = FDatasmithFacadeElement.GetStringHash("M" + MeshIndex + ":" + Name);
			FDatasmithFacadeMesh DatasmithMesh = new FDatasmithFacadeMesh();
			DatasmithMesh.SetName(HashedName);

			return DatasmithMesh;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]

		private static FDatasmithFacadeMeshElement CreateDatasmithMeshElementForItem(SceneContext SceneContext, SceneItem Item, int MeshIndex)
		{
			// Set mesh name/label from the Item that it's instantiated from
			GetNameAndLabelFromItem(Item, out string Name, out string Label);
			// Hash the Datasmith mesh name to shorten it and make it valid
			string HashedName = FDatasmithFacadeElement.GetStringHash("M" + MeshIndex + ":" + Name);
			FDatasmithFacadeMeshElement MeshElement = new FDatasmithFacadeMeshElement(HashedName);
			MeshElement.SetLabel(Label + "_" + MeshIndex);
			SceneContext.DatasmithMeshElements.Add(MeshElement); // hold on to reference
			SceneContext.DatasmithScene.AddMesh(MeshElement);
			return MeshElement;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private static bool ExportDatasmithMesh(SceneContext SceneContext, FDatasmithFacadeMesh DatasmithMesh, FDatasmithFacadeMeshElement DatasmithMeshElement)
		{
			// Export the DatasmithMesh to a .udsmesh file and assign it to the DatasmithMeshElement.
			if (SceneContext.DatasmithScene.ExportDatasmithMesh(DatasmithMeshElement, DatasmithMesh))
			{
				SceneContext.TotalVertexCount += DatasmithMesh.GetVerticesCount();
				SceneContext.TotalTriangleCount += DatasmithMesh.GetFacesCount();
				
				return true;
			}

			return false;
		}

		private unsafe void FillDatasmithMeshFromGeometry(Geometry Geometry, FDatasmithFacadeMesh DatasmithMesh)
		{
			int VertexCount = (int)Geometry.VertexCount;
			const int UVChannelCount = 1;
			const int DefaultUVChannel = 0;
			DatasmithMesh.SetVerticesCount(VertexCount);
			DatasmithMesh.SetUVChannelsCount(UVChannelCount);
			DatasmithMesh.SetUVCount(DefaultUVChannel, VertexCount);
			for (int VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
			{
				GeometryVertex Vertex = new GeometryVertex(Geometry, VertexIndex);

				Vector3d Position = Vertex.Position;
				DatasmithMesh.SetVertex(VertexIndex, (float)Position.X, (float)Position.Y, (float)Position.Z);

				Vector3d Normal = Vertex.Normal;
				DatasmithMesh.SetNormal(VertexIndex, (float)Normal.X, (float)Normal.Y, (float)Normal.Z);

				Vector2d Uv = Vertex.UV;
				DatasmithMesh.SetUV(DefaultUVChannel, VertexIndex, (float)Uv.X, (float)Uv.Y);
			}

			uint TriangleCount = Geometry.TriangleCount;
			DatasmithMesh.SetFacesCount((int)TriangleCount);
			for (int TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
			{
				int Vertex1 = (int)Geometry.Indices[TriangleIndex * 3];
				int Vertex2 = (int)Geometry.Indices[TriangleIndex * 3 + 1];
				int Vertex3 = (int)Geometry.Indices[TriangleIndex * 3 + 2];

				DatasmithMesh.SetFace(TriangleIndex, Vertex1, Vertex2, Vertex3);
				DatasmithMesh.SetFaceUV(TriangleIndex, DefaultUVChannel, Vertex1, Vertex2, Vertex3);
			}
		}

		private unsafe void FillDatasmithMeshFromInstancedGeometry(SceneItemInstancedGeometry InstancedGeometry,
			FDatasmithFacadeMesh DatasmithMesh, List<int> SlotRemap, Vector3d PositionOffset)
		{
			const int UVChannelCount = 1;
			const int DefaultUVChannel = 0;
			int VerticesCount = 0;
			int FacesCount = 0;
			List<Geometry> GeometryFragments = new List<Geometry>(InstancedGeometry.FragmentGeometryCount);

			for (int FragmentIndex = 0; FragmentIndex < InstancedGeometry.FragmentGeometryCount; ++FragmentIndex)
			{
				if (InstancedGeometry.GetFragmentGeometry(FragmentIndex).GetGeometry(out Geometry Geometry))
				{
					GeometryFragments.Add(Geometry);
					VerticesCount += (int)Geometry.VertexCount;
					FacesCount += (int)Geometry.TriangleCount;
				}
			}

			DatasmithMesh.SetVerticesCount(VerticesCount);
			DatasmithMesh.SetUVChannelsCount(UVChannelCount);
			DatasmithMesh.SetUVCount(DefaultUVChannel, VerticesCount);
			DatasmithMesh.SetFacesCount(FacesCount);

			int VertexIndexOffset = 0;
			int FaceIndexOffset = 0;
			for (int FragmentIndex = 0; FragmentIndex < GeometryFragments.Count; ++FragmentIndex)
			{
				Geometry Geometry = GeometryFragments[FragmentIndex];

				for (int VertexIndex = 0; VertexIndex < Geometry.VertexCount; VertexIndex++)
				{
					GeometryVertex Vertex = new GeometryVertex(Geometry, VertexIndex);

					Vector3d Position = Vertex.Position + PositionOffset;
					DatasmithMesh.SetVertex(VertexIndexOffset + VertexIndex, (float)Position.X, (float)Position.Y, (float)Position.Z);

					Vector3d Normal = Vertex.Normal;
					DatasmithMesh.SetNormal(VertexIndexOffset + VertexIndex, (float)Normal.X, (float)Normal.Y, (float)Normal.Z);

					Vector2d Uv = Vertex.UV;
					DatasmithMesh.SetUV(DefaultUVChannel, VertexIndexOffset + VertexIndex, (float)Uv.X, (float)Uv.Y);
				}

				uint TriangleCount = Geometry.TriangleCount;
				int MaterialId = SlotRemap[FragmentIndex];
				for (int TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
				{
					int FaceIndex = TriangleIndex + FaceIndexOffset;
					int Vertex1 = (int)Geometry.Indices[TriangleIndex * 3] + VertexIndexOffset;
					int Vertex2 = (int)Geometry.Indices[TriangleIndex * 3 + 1] + VertexIndexOffset;
					int Vertex3 = (int)Geometry.Indices[TriangleIndex * 3 + 2] + VertexIndexOffset;

					DatasmithMesh.SetFace(FaceIndex, Vertex1, Vertex2, Vertex3, MaterialId);
					DatasmithMesh.SetFaceUV(FaceIndex, DefaultUVChannel, Vertex1, Vertex2, Vertex3);
				}

				VertexIndexOffset += (int)Geometry.VertexCount;
				FaceIndexOffset += (int)Geometry.TriangleCount;
			}
		}

		private unsafe FDatasmithFacadeMesh  CreateDatasmithMeshForBakedFragmentGeometry(SceneContext SceneContext, GeometryInstance Instance, List<int> SlotRemap, out Vector3d OutTranslation)
		{
			FDatasmithFacadeMesh DatasmithMesh = CreateDatasmithMeshForItem(SceneContext, Instance.SceneItem, 0);
			const int UVChannelCount = 1;
			const int DefaultUVChannel = 0;
			int VerticesCount = 0;
			int FacesCount = 0;
			List<Geometry> GeometryFragments = new List<Geometry>(Instance.SharedInstancedForInstancesRef.InstancedGeometry.FragmentGeometryCount);

			for (int FragmentIndex = 0; FragmentIndex < Instance.SharedInstancedForInstancesRef.InstancedGeometry.FragmentGeometryCount; ++FragmentIndex)
			{
				FragmentGeometry FragmentGeometry = Instance.SharedInstancedForInstancesRef.InstancedGeometry.GetFragmentGeometry(FragmentIndex);
				OptimizeSourceGeometry(FragmentGeometry);

				if (FragmentGeometry.GetGeometry(out Geometry Geometry))
				{
					GeometryFragments.Add(Geometry);
					VerticesCount += (int)Geometry.VertexCount;
					FacesCount += (int)Geometry.TriangleCount;
				}
				else
				{
					// Add an empty entry to make sure the indices match SlotRemap.
					GeometryFragments.Add(null);
				}
			}

			// Compute bounding box of consolidated geometry to repivot merged geometry to the bbox center
			GeometryUtil.BoundingBox BoundingBox = new GeometryUtil.BoundingBox();
			for (int FragmentIndex = 0; FragmentIndex < Instance.SharedInstancedForInstancesRef.InstancedGeometry.FragmentGeometryCount; FragmentIndex++)
			{
				FragmentGeometry FragmentGeometry = Instance.SharedInstancedForInstancesRef.InstancedGeometry.GetFragmentGeometry(FragmentIndex);
				OptimizeSourceGeometry(FragmentGeometry);
				if (FragmentGeometry.GetGeometry(out Geometry Geometry))
				{
					BoundingBox.Extend(Geometry, SceneContext.NavisworksToUnrealTransform.Multiply(Instance.GetTransform(FragmentIndex)));
				}
			}
			// Center of the bounding box of vertices already transformed to export(Unreal) coordinates
			Vector3d PivotTranslation = BoundingBox.BottomCenter;
			OutTranslation = PivotTranslation; 

			DatasmithMesh.SetVerticesCount(VerticesCount);
			DatasmithMesh.SetUVChannelsCount(UVChannelCount);
			DatasmithMesh.SetUVCount(DefaultUVChannel, VerticesCount);
			DatasmithMesh.SetFacesCount(FacesCount);

			// For Fragments that don't all share single transform - bake Fragment transform into its vertices
			int VertexIndexOffset = 0;
			int FaceIndexOffset = 0;
			for (int FragmentIndex = 0; FragmentIndex < GeometryFragments.Count; ++FragmentIndex)
			{
				Geometry Geometry = GeometryFragments[FragmentIndex];

				if (Geometry != null)
				{
					TransformMatrix Transform = SceneContext.NavisworksToUnrealTransform.Multiply(Instance.GetTransform(FragmentIndex));

					for (int VertexIndex = 0; VertexIndex < Geometry.VertexCount; VertexIndex++)
					{
						GeometryVertex Vertex = new GeometryVertex(Geometry, VertexIndex);

						Vector3d CoordTransformed = Transform.TransformPosition(Vertex.Position) - PivotTranslation;
						DatasmithMesh.SetVertex(VertexIndexOffset + VertexIndex, (float)CoordTransformed.X, (float)CoordTransformed.Y, (float)CoordTransformed.Z);

						Vector3 NormalTransformed = new Vector3(Transform.TransformNormal(Vertex.Normal));
						DatasmithMesh.SetNormal(VertexIndexOffset + VertexIndex, (float)NormalTransformed.X, (float)NormalTransformed.Y, (float)NormalTransformed.Z);

						Vector2d Uv = Vertex.UV;
						DatasmithMesh.SetUV(DefaultUVChannel, VertexIndexOffset + VertexIndex, (float)Uv.X, (float)Uv.Y);
					}

					uint TriangleCount = Geometry.TriangleCount;
					int MaterialId = SlotRemap[FragmentIndex];
					for (int TriangleIndex = 0; TriangleIndex < TriangleCount; TriangleIndex++)
					{
						int FaceIndex = TriangleIndex + FaceIndexOffset;
						int Vertex1 = (int)Geometry.Indices[TriangleIndex * 3] + VertexIndexOffset;
						int Vertex2 = (int)Geometry.Indices[TriangleIndex * 3 + 1] + VertexIndexOffset;
						int Vertex3 = (int)Geometry.Indices[TriangleIndex * 3 + 2] + VertexIndexOffset;

						DatasmithMesh.SetFace(FaceIndex, Vertex1, Vertex2, Vertex3, MaterialId);
						DatasmithMesh.SetFaceUV(FaceIndex, DefaultUVChannel, Vertex1, Vertex2, Vertex3);
					}
					VertexIndexOffset += (int)Geometry.VertexCount;
					FaceIndexOffset += (int)Geometry.TriangleCount;
				}
			}

			Instance.BoundingBox = BoundingBox;
			return DatasmithMesh;
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private void OptimizeSourceGeometry(Geometry ReadGeometry)
		{
			// TODO: Skip optimize for now - check why DatasmithExport not duplicating normals for all wedges
			// ReadGeometry?.Optimize();
		}

		[MethodImpl(MethodImplOptions.NoInlining)]
		private void OptimizeSourceGeometry(FragmentGeometry FragmentGeometry)
		{
			if (FragmentGeometry.bOptimized)
			{
				return;
			}			
			FragmentGeometry.bOptimized = true;
			if (FragmentGeometry.GetGeometry(out Geometry Geometry))
			{
				OptimizeSourceGeometry(Geometry);
			}
		}

		private class SceneItemPath: IEquatable<SceneItemPath>
		{
			private readonly int[] Path;
			private readonly int Hash;

			public SceneItemPath()
			{
				Path = new int[0];
			}

			[MethodImpl(MethodImplOptions.NoInlining)]
			public SceneItemPath(SceneItemPath ParentPath, int Index)
			{
				// Child path is parent and index appended
				Path = new int[ParentPath.Path.Length + 1];
				ParentPath.Path.CopyTo(Path, 0);
				Path[Path.Length - 1] = Index+1; // This is used to compare to Fragment path from Navisworks API where indices are 1-based

				Hash = ComputeHash();
			}

			[MethodImpl(MethodImplOptions.NoInlining)]
			public SceneItemPath(InwOaFragment3 Fragment): this(Fragment.path)
			{
			}

			[MethodImpl(MethodImplOptions.NoInlining)]
			public SceneItemPath(InwOaPath NwOaPath)
			{
				Array PathArray = (Array) (object) NwOaPath.ArrayData;
				Path = PathArray.OfType<int>().ToArray();

				Hash = ComputeHash();
			}

			private int ComputeHash()
			{
				int Hash = Path.Length;
				foreach (int I in Path)
				{
					Hash = unchecked(Hash*31 + I);
				}

				return Hash;
			}

			public override string ToString()
			{
				return "FragmentPath(" + string.Join(", ", Path) + ")";
			}

			public bool Equals(SceneItemPath Other)
			{
				return Path.SequenceEqual(Other.Path);
			}

			public override bool Equals(object Other)
			{
				return Equals(Other as SceneItemPath);
			}

			public override int GetHashCode()
			{
				return Hash;
			}
		};

		[MethodImpl(MethodImplOptions.NoInlining)]
		private void ExtractClipBox(Document ActiveDocument, ItemFilterParameters ItemFilterParams)
		{
			ItemFilterParams.bHasClipBox = false;

			LcOaClipPlaneSet ClipPlaneSet = ActiveDocument.ActiveView.Viewer.GetClipPlaneSet();
			LcOaClipPlaneSetMode ClipPlaneSetMode = ClipPlaneSet.GetMode();

			if (ClipPlaneSetMode == LcOaClipPlaneSetMode.eMODE_BOX)
			{
				BoundingBox3D Box3D = ClipPlaneSet.GetBox();

				ItemFilterParams.bHasClipBox = true;
				Box ClipBox = new Box
				{
					Min = {X = (float) Box3D.Min.X, Y = (float) Box3D.Min.Y, Z = (float) Box3D.Min.Z},
					Max = {X = (float) Box3D.Max.X, Y = (float) Box3D.Max.Y, Z = (float) Box3D.Max.Z}
				};

				ItemFilterParams.ClipBox = ClipBox;
			}

			Info(ItemFilterParams.bHasClipBox ? "ClipBox: " + ItemFilterParams.ClipBox : "No clip box");
		}

		private static double GetCentimetersPerUnit(Units Units)
		{
			const double CENTIMETERS_PER_YARD = 91.44;
			const double CENTIMETERS_PER_INCH = 2.54;
			switch (Units)
			{
				case Units.Meters:
					return 100;
				case Units.Centimeters:
					return 1;
				case Units.Millimeters:
					return 0.1;
				case Units.Feet:
					return 30.48;
				case Units.Inches:
					return CENTIMETERS_PER_INCH;
				case Units.Yards:
					return CENTIMETERS_PER_YARD;
				case Units.Kilometers:
					return 100000;
				case Units.Miles:
					return CENTIMETERS_PER_YARD * 1760;
				case Units.Micrometers:
					return 0.0001;
				case Units.Mils:
					return CENTIMETERS_PER_INCH * 0.001;
				case Units.Microinches:
					return CENTIMETERS_PER_INCH * 0.000001;
				default:
					return 1;
			}
		}

		private static void GetNameAndLabelFromItem(SceneItem Item, out string Name, out string Label)
		{
			Label = Item.ComNode.UserName;
			const int LabelMaxLength = 50;

			// Reduce too long label(or asset won't save if path is >255)
			if (Label.Length > LabelMaxLength)
			{
				// Leave right(end) part of the label - looks like in contains most useful information(and most varying per object)
				Label = Label.Substring(Label.Length - LabelMaxLength, LabelMaxLength);
			}

			// Path uniquely identifies scene Item, so build Name from it
			List<int> PathArray = new List<int>();
			while (Item != null)
			{
				PathArray.Add(Item.IndexInParentChildren);
				Item = Item.Parent;
			}

			Name = string.Join("_", (from object O in PathArray select O.ToString()));
		}

		private void Info(string Message, int Level = 0)
		{
			string Indent = new string(' ', Level*2);
			LogStream?.WriteLine($"{Indent}{Message}");
		}

		private Stopwatch EventStopwatch;

		private void EventInfo(string Message)
		{
			LogStream?.WriteLine($"{EventStopwatch.Elapsed.TotalSeconds:0.00}:{Message}");
			LogStream?.Flush(); // let all scene info be written for critical message(event)
			Console.WriteLine($"{EventStopwatch.Elapsed.TotalSeconds:0.00}:{Message}");
			Debug.WriteLine($"{Process.GetCurrentProcess().TotalProcessorTime.TotalSeconds:0.00}:{Message}");
		}

		private static void DisplayWarning(string Message)
		{
			if (Autodesk.Navisworks.Api.Application.IsAutomated)
			{
				Console.WriteLine(Message);
			}
			else
			{
				System.Windows.Forms.MessageBox.Show(Message, DIALOG_CAPTION, System.Windows.Forms.MessageBoxButtons.OK, System.Windows.Forms.MessageBoxIcon.Warning);
			}
		}

		class SceneItem
		{
			public SceneItem(SceneItem Parent, InwOaNode ComNode, int IndexInParentChildren = 0)
			{
				this.Parent = Parent;
				this.ComNode = ComNode;
				this.IndexInParentChildren = IndexInParentChildren;

				Path = Parent == null ? new SceneItemPath() : new SceneItemPath(Parent.Path, IndexInParentChildren);

				bChildrenOnlySimple = true;
				InwLBox3f Box3F = ComNode.GetBoundingBox(true, true);

				BoundingBox.Min.X = (float)Box3F.min_pos.data1;
				BoundingBox.Min.Y = (float)Box3F.min_pos.data2;
				BoundingBox.Min.Z = (float)Box3F.min_pos.data3;
				BoundingBox.Max.X = (float)Box3F.max_pos.data1;
				BoundingBox.Max.Y = (float)Box3F.max_pos.data2;
				BoundingBox.Max.Z = (float)Box3F.max_pos.data3;
			}

			public int Index;
			public SceneItem Parent;
			public InwOaNode ComNode;
			public int IndexInParentChildren;
			public SceneItemPath Path;
			public Box BoundingBox;
			public bool bIsSimpleGeometry;
			public bool bChildrenOnlySimple;
			public FDatasmithFacadeActor DatasmithActor;
		}

		class Appearance : IEquatable<Appearance>
		{
			public readonly List<float> AppearanceValues;
			private readonly int Hash;
			public string Name;

			public Appearance(Array Values)
			{
				AppearanceValues = Values.OfType<float>().ToList();

				Hash = 0;
				foreach (double F in AppearanceValues)
				{
					Hash ^= F.GetHashCode();
				}
			}

			public override bool Equals(object Other)
			{
				return Equals(Other as Appearance);
			}

			public bool Equals(Appearance Other)
			{
				return AppearanceValues.SequenceEqual(Other.AppearanceValues);
			}

			public override int GetHashCode()
			{
				return Hash;
			}
		};

		class NavisworksFragmentInstance
		{
			public readonly TransformMatrix FragmentTransform;
			public readonly Appearance Appearance;

			public NavisworksFragmentInstance(TransformMatrix FragmentTransform, Appearance Appearance)
			{
				this.FragmentTransform = FragmentTransform;
				this.Appearance = Appearance;
			}
		}


		// Holds all instanced for Navisworks Com API Node
		// Each 'instance' in Navisworks Com API is distinguished by item Path
		class NavisworksNodeInstances
		{
			public readonly SceneItem SceneItem; // Represents one item of the instances
			private readonly Dictionary<SceneItemPath, List<NavisworksFragmentInstance>> FragmentsForPath = new Dictionary<SceneItemPath, List<NavisworksFragmentInstance>>();
			private readonly Dictionary<SceneItemPath, GuiProperties> GuiPropertyForPath = new Dictionary<SceneItemPath, GuiProperties>();
			public InstancedWithInstancesToExport SharedInstancedRef;

			public NavisworksNodeInstances(SceneItem Item)
			{
				this.SceneItem = Item;
			}

			public void AddFragmentInstance(SceneItemPath FragmentPath, TransformMatrix FragmentTransform, Appearance Appearance)
			{
				if (!FragmentsForPath.TryGetValue(FragmentPath,
					out List<NavisworksFragmentInstance> NavisworksFragmentInstances))
				{
					NavisworksFragmentInstances = new List<NavisworksFragmentInstance>();
					FragmentsForPath.Add(FragmentPath, NavisworksFragmentInstances);
				}

				NavisworksFragmentInstance NavisworksFragmentInstance = new NavisworksFragmentInstance(FragmentTransform, Appearance);
				NavisworksFragmentInstances.Add(NavisworksFragmentInstance);
			}

			// Get all fragments that match item path(i.e. for specific instance)
			public IEnumerable<NavisworksFragmentInstance> GetFragmentInstances(SceneItemPath FragmentPath)
			{
				if (FragmentsForPath.TryGetValue(FragmentPath, out List<NavisworksFragmentInstance> FragmentInstances))
				{
					foreach (NavisworksFragmentInstance Instance in FragmentInstances)
					{
						yield return Instance;
					}
				}
			}

			public bool HasGuiPropertyForPath(SceneItemPath SceneItemPath)
			{
				return GuiPropertyForPath.ContainsKey(SceneItemPath);
			}

			public GuiProperties AddGuiPropertiesForPath(SceneItemPath SceneItemPath)
			{
				GuiProperties GuiProperties = new GuiProperties();
				GuiPropertyForPath.Add(SceneItemPath, GuiProperties);
				return GuiProperties;
			}

			public bool TryGetGuiPropertiesForPath(SceneItemPath SceneItemPath, out GuiProperties GuiProperties)
			{
				return GuiPropertyForPath.TryGetValue(SceneItemPath, out GuiProperties);
			}
		}

		class NavisworksInstancesCollection
		{
			private readonly SceneContext SceneContext;

			// instances for every Com API node. 'object' is the actual Com pointer, that identifies Com node(rather than InwOaNode which is a wrapper that can be different for the same actual node)
			public readonly Dictionary<object, NavisworksNodeInstances> NavisworksInstancesForComNode = new Dictionary<object, NavisworksNodeInstances>();

			public NavisworksInstancesCollection(SceneContext SceneContext)
			{
				this.SceneContext = SceneContext;
			}

			public bool GetInstancesForItem(SceneItem Item, out NavisworksNodeInstances NodeInstances)
			{
				return NavisworksInstancesForComNode.TryGetValue(GetRawPointerForNode(Item), out NodeInstances);
			}

			public bool GetOrCreateInstances(SceneItem Item, out NavisworksNodeInstances NodeInstances)
			{
				object Pointer = GetRawPointerForNode(Item);
				if (NavisworksInstancesForComNode.TryGetValue(Pointer, out NodeInstances))
				{
					return true;
				}
				NavisworksInstancesForComNode.Add(Pointer, new NavisworksNodeInstances(Item));
				return false;
			}

			private object GetRawPointerForNode(SceneItem Item)
			{
				SceneContext.State.X64PtrVar(Item.ComNode, out object Pointer); // Get actual 64-bit pointer to COM object
				return Pointer;
			}
		}

		class SceneContext
		{
			public InwOpState10 State;
			public DatasmithNavisworksUtil.TriangleReader TriangleReader = new TriangleReader();

			public int ModelItemCount; // Total item count, as read using Navisworks api
			public List<SceneItem> SceneItemList = new List<SceneItem>(); // All scene items acquired from the Navisworks scene

			public Node RootNode; // Scene hierarchy that will me exported

			public ItemFilterParameters ItemFilterParams;
			public ExportParameters ExportParameters;

			public TransformMatrix NavisworksToUnrealTransform; // Transform to what is saved to Datasmith

			// Each ComNode(InwOaNode) can be present in multiple parts of hierarchy(in different node paths): E.g. 
			// Root
			//     - Trunk
			//         - BranchA
			//             - GreenApple
			//             - RedApple
			//         - BranchB
			//             - RedApple
			// And this node contains ALL the fragments for all its instances.
			// E.g. RedApple will contain 2x Fragments - those that are visible in path within BranchA and within BranchB.
			// Fragments are distinguished by checking their 'path' field - it corresponds(equals) to item's Path.
			// Com node is identified by raw Com pointer.
			// NavisworksInstancesforComNode stores fragments for each ComNode(key is raw com pointer), sorting them by item Path.
			public readonly NavisworksInstancesCollection NavisworksInstancesCollection;

			// Intermediate export data
			public List<InstancedWithInstancesToExport> AllInstanceds = new List<InstancedWithInstancesToExport>(); // Every SharedInstanced geometry with its instances, added for export
			public Dictionary<SceneItemInstancedGeometry, InstancedWithInstancesToExport> StoredInstancedForGeometry = new Dictionary<SceneItemInstancedGeometry, InstancedWithInstancesToExport>();
			public Dictionary<Geometry, FragmentGeometry> StoredFragmentGeometries = new Dictionary<Geometry, FragmentGeometry>();
			public HashSet<string> AppearanceNames = new HashSet<string>();
			public Dictionary<Appearance, Appearance> Appearances = new Dictionary<Appearance, Appearance>();

			// Datasmith 
			public FDatasmithFacadeScene DatasmithScene;
			public Dictionary<SceneItemPath, InwOaPath> ItemComPathForPath = new Dictionary<SceneItemPath, InwOaPath>();
			public Dictionary<Appearance, FDatasmithFacadeUEPbrMaterial> DatasmithMaterialForAppearance = new Dictionary<Appearance, FDatasmithFacadeUEPbrMaterial>();
			public List<FDatasmithFacadeMeshElement> DatasmithMeshElements = new List<FDatasmithFacadeMeshElement>();
			public List<FDatasmithFacadeActor> DatasmithActors = new List<FDatasmithFacadeActor>();
			public List<FDatasmithFacadeActor> DatasmithActorMeshes = new List<FDatasmithFacadeActor>();
			public List<FDatasmithFacadeBaseMaterial> DatasmithMaterials = new List<FDatasmithFacadeBaseMaterial>();
			public FDatasmithFacadeBaseMaterial DefaultMaterial;

			// Stats
			public int GroupInstanceTotalCount = 0;
			public int GeometryInstanceTotalCount = 0;
			public int HiddenNodeCount = 0;
			public int GeometryNodeCount = 0;
			public int GroupNodeCount = 0;
			public int InsertNodeCount = 0;
			public int LayerNodeCount = 0;

			public int TotalVertexCount = 0;
			public int TotalTriangleCount = 0;

			public int SceneItemCount => SceneItemList.Count;
			public double CentimetersPerUnit;

			public double TriangleSizeThreshold => 1e-8 / (CentimetersPerUnit * CentimetersPerUnit);
			public double PositionThreshold => 0.00002 / CentimetersPerUnit;
			public double NormalThreshold => 1e-4;

			public int MultipleFragmentGeometryWithDistinctTransform;
			public int MultipleFragmentGeometryWithDistinctTransformFragmentCount;
			public int MultipleFragmentGeometryWithSameTransform;
			public int MultipleFragmentGeometryWithSameTransformFragmentCount;
			public int MultipleFragmentGeometryWithDistinctAppearance;
			public int MultipleFragmentGeometryWithDistinctAppearanceFragmentCount;
			public int MultipleFragmentGeometryWithSameAppearance;
			public int MultipleFragmentGeometryWithSameAppearanceFragmentCount;

			public int DuplicateMeshCount;
			public int IdenticalItemReused;
			public int TotalMeshSectionCount;
			public int TotalMeshSectionInstancedCount;

			public SceneContext()
			{
				NavisworksInstancesCollection = new NavisworksInstancesCollection(this);
			}
		}

		class NodeContext
		{
			public SceneContext SceneContext;

			public int Level;
			public NodeContext Parent;

			public NodeContext(SceneContext SceneContext)
			{
				Level = 0;
				Parent = null;
				this.SceneContext = SceneContext;
			}

			public NodeContext(NodeContext Parent)
			{
				this.Level = Parent.Level + 1;
				this.Parent = Parent;
				this.SceneContext = Parent.SceneContext;
			}
		};

		[MethodImpl(MethodImplOptions.NoInlining)]
		private bool CollectNodes(NodeContext Context, SceneItem Item, out Node Node, out bool bIsVisible, Progress ProgressBar)
		{
			Info($"Node {Item.IndexInParentChildren}: {Item.ComNode.UserName}", Context.Level);
			Node = new Node();
			bIsVisible = false;

			if (ProgressBar.IsCanceled)
			{
				return false;
			}

			Node.SceneItem = Item;

			if (Item.ComNode.IsOverrideHide) // Test Hidden on  COM node
			{
				Info($"!IsOverrideHide", Context.Level);
				// skipping entire hierarchy for hidden nodes
				Context.SceneContext.HiddenNodeCount++;
				return true;
			}

			if (!GetSceneItemComPath(Context.SceneContext, Item, out InwOaPath ComPath))
			{
				Debug.Assert(false); // This seems like impossible situation(that com node instance won't have instance path)
				return true;
			}

			ModelItem ModelItem = Autodesk.Navisworks.Api.ComApi.ComApiBridge.ToModelItem(ComPath);
			if (ModelItem.IsHidden) // Test Hidden on COM node Instance
			{
				Info($"!IsHidden", Context.Level);
				// skipping entire hierarchy for hidden nodes
				Context.SceneContext.HiddenNodeCount++;
				return true;
			}

			bIsVisible = true;

			if (Context.SceneContext.ItemFilterParams.bHasClipBox && !Item.BoundingBox.Intersects(Context.SceneContext.ItemFilterParams.ClipBox))
			{
				return true;
			}

			int ItemIndex = Context.SceneContext.SceneItemList.Count;
			Item.Index = ItemIndex;
			Context.SceneContext.SceneItemList.Add(Item);

			Item.bIsSimpleGeometry = Item.ComNode.IsGeometry &&
				!Item.ComNode.IsGroup && !Item.ComNode.IsInsert && !Item.ComNode.IsLayer && !Item.ComNode.IsPartition;

			if (Item.Parent != null && !Item.bIsSimpleGeometry)
			{
				Item.Parent.bChildrenOnlySimple = false;
			}

			InwOaNode NwOaNode = Item.ComNode;
			// Test if this node was already used within the hierarchy("instance")
			bool bDoesNodeHaveAnotherInstance = Context.SceneContext.NavisworksInstancesCollection.GetOrCreateInstances(Item, out NavisworksNodeInstances NavisworksInstance);
			if (bDoesNodeHaveAnotherInstance)
			{
				Info($"!instance", Context.Level);

				if (Item.ComNode.IsGeometry)
				{
					Context.SceneContext.GeometryInstanceTotalCount++;
				}
				if (Item.ComNode.IsGroup)
				{
					Context.SceneContext.GroupInstanceTotalCount++;
				}
			}
			if (NwOaNode.IsGeometry)
			{
				Context.SceneContext.GeometryNodeCount++;
				// Commented fragments loading - sometimes Navisowrks packs lots of extra fragments into
				// one node and loading them all takes too much time - UE-93658 
				// InwNodeFragsColl Fragments = Node.Fragments();
				// Info($"# IsGeometry (Fragments: {Fragments.Count})", Context.Level);
				Info($"# IsGeometry", Context.Level);
			}

			if (NwOaNode.IsInsert)
			{
				Context.SceneContext.InsertNodeCount++;
				Info($"# IsInsert ", Context.Level);
			}
			
			if (NwOaNode.IsLayer)
			{
				Context.SceneContext.LayerNodeCount++;
				Info($"# IsLayer ", Context.Level);
			}

			if (NwOaNode.IsGroup)
			{
				Context.SceneContext.GroupNodeCount ++;
				Info($"# IsGroup ", Context.Level);
				if (NwOaNode is InwOaGroup Group)
				{
					Info($"# Children ({Group.Children().Count})", Context.Level);
					int ChildIndex = 0; // Index of child to identify Item path correctly
					foreach (InwOaNode Child in Group.Children())
					{
						NodeContext ChildContext = new NodeContext(Context);
						SceneItem ChildItem = new SceneItem(Item, Child, ChildIndex);

						if (!CollectNodes(ChildContext, ChildItem, out Node ChildNode, out bool bIsChildVisible, ProgressBar))
						{
							return false;
						}

						if (bIsChildVisible)
						{
							Node.Children.Add(ChildNode);
						}

						ChildIndex++;
					}
				}
			}

			// Clamped because ModelItemCount sometimes lower than processed count as .NET api count doesn't include root node for multifile nwf as Com api does
			ProgressBar.Update(Math.Min(1, (double) Context.SceneContext.SceneItemCount / Context.SceneContext.ModelItemCount));
			return true;
		}

		// Encapsulates data, displayed in Navisworks Properties window for an item
		public class GuiProperties
		{
			// An Attribute corresponds to a Tab in Properties windows of Navisworks
			public class Attribute
			{
				private readonly Dictionary<string, string> Properties = new Dictionary<string, string>();

				public IEnumerable<Tuple<string, string>> EnumerateProperties()
				{
					foreach (KeyValuePair<string, string> KeyValuePair in Properties)
					{
						yield return new Tuple<string, string>(KeyValuePair.Key, KeyValuePair.Value);
					}
				}

				public void SetProperty(string Name, string Value)
				{
					string NameBase = Name;

					// Avoid  duplicate names
					int Index = 0;
					while (Properties.ContainsKey(Name))
					{
						Name = NameBase + Index;
						Index++;
					}

					Properties.Add(Name, Value);
				}
			}

			private readonly Dictionary<string, Attribute> Attributes = new Dictionary<string, Attribute>();

			public IEnumerable<Tuple<string, Attribute>> EnumerateAttributes()
			{
				foreach (KeyValuePair<string, Attribute> KeyValuePair in Attributes)
				{
					yield return new Tuple<string, Attribute>(KeyValuePair.Key, KeyValuePair.Value);
				}
			}

			public Attribute AddAttribute(string Name)
			{
				string NameBase = Name;

				// Avoid  duplicate names
				int Index = 0;
				while (Attributes.ContainsKey(Name))
				{
					Name = NameBase + Index;
					Index++;
				}

				Attribute Attribute = new Attribute();
				Attributes.Add(Name, Attribute);
				return Attribute;
			}
		};
	}

	// Tool to pick point in the scene to use as Origin to rebase exported geometry to
	[Plugin("DatasmithNavisworksExporter.PickOrigin", "EpicGames")]
	public class PickOriginPlugin : ToolPlugin
	{
		private ModelItem PickedModelItem;
		private Vector3d PickedPoint;
		public ExporterDockPanePlugin.DockPaneControlPresenter DockPanePresenter;

		private TriangleReader LocalTriangleReader = new TriangleReader();

		public override bool MouseDown(View View,
			KeyModifiers Modifiers,
			ushort Button,
			int X,
			int Y,
			double TimeOffset)
		{
			const ushort LEFT_MOUSE_BUTTON = 1;

			// Save picked point of LMB press
			if (Button == LEFT_MOUSE_BUTTON)
			{
				DockPanePresenter.ExportSettings.Origin = PickedPoint;
				DockPanePresenter.UpdateControl();

				Application.MainDocument.Tool.Value = Tool.Select;  
				Application.ActiveDocument.ActiveView.RequestDelayedRedraw(ViewRedrawRequests.Render);
			}
			
			return false;
		}

		public override bool MouseUp(View View, KeyModifiers Modifiers, ushort Button, int X, int Y, double TimeOffset)
		{
			const ushort RIGHT_MOUSE_BUTTON = 3;
			// Cancel tool on RMB click
			if (Button == RIGHT_MOUSE_BUTTON) 
			{
				// TODO: revert
				Application.MainDocument.Tool.Value = Tool.Select;  
				Application.ActiveDocument.ActiveView.RequestDelayedRedraw(ViewRedrawRequests.Render);
			}

			// Prevent context menu appear
			return true;
		}

		public override bool MouseMove(View View, KeyModifiers Modifiers, int X, int Y, double TimeOffset)
		{
			PickItemResult PickItemResult = View.PickItemFromPoint(X, Y);

			if (PickItemResult != null)
			{
				Vector3d IntersectionPoint = new Vector3d()
				{
					X = PickItemResult.Point.X,
					Y = PickItemResult.Point.Y,
					Z = PickItemResult.Point.Z
				};

				ModelItem Item = PickItemResult.ModelItem;

				PickedPoint = SnapIntersectionPointToGeometryVertex(View, Item, X, Y, IntersectionPoint);
				PickedModelItem = Item;
				Application.ActiveDocument.ActiveView.RequestDelayedRedraw(ViewRedrawRequests.Render);
			}

			return false;
		}

		private Vector3d SnapIntersectionPointToGeometryVertex(View View, ModelItem Item, int X, int Y, Vector3d Point)
		{
			InwOaPath Path = Autodesk.Navisworks.Api.ComApi.ComApiBridge.ToInwOaPath(Item);
			InwNodeFragsColl Fragments = Path.Fragments();

			// Compute distance to intersection point, for each fragment's box
			List<Tuple<InwOaFragment3, double>> FragmentsWithDistanceSquared = new List<Tuple<InwOaFragment3, double>>();
			foreach (InwOaFragment3 Fragment in Fragments)
			{
				InwLBox3f Box3F = Fragment.GetWorldBox();

				Vector3d BoxMin = new Vector3d()
				{
					X = Box3F.min_pos.data1,
					Y = Box3F.min_pos.data2,
					Z = Box3F.min_pos.data3
				};

				Vector3d BoxMax = new Vector3d()
				{
					X = Box3F.max_pos.data1,
					Y = Box3F.max_pos.data2,
					Z = Box3F.max_pos.data3
				};

				Vector3d BoxSize = (BoxMax - BoxMin) * 0.5;
				Vector3d BoxCenter = (BoxMin + BoxMax) * 0.5;

				// Compute distance to the bounding box
				//                .(Point)
				//               /
				//              / <- Result
				//             /         
				// ------------------
				//        ^  /       |
				// Size[0]  /        |
				//        v/         |
				//        *(Center)  |
				// Abs - don't case which side point is located at
				// Max(0) - in case point coordinate contained withing a box's dimension
				double DistanceSquaredToBox = ((BoxCenter - Point).Abs() - BoxSize).Max(0).LengthSquared();

				FragmentsWithDistanceSquared.Add(new Tuple<InwOaFragment3, double>(Fragment, DistanceSquaredToBox));
			}

			Vector3d PositionWorldSpaceBest = Point;
			double DistanceSquaredBest = double.MaxValue;
			// Look for closest vertex(to intersection point)
			// Iterate fragments ordered by (increasing)distance to intersection point. This will let drop computation for all far away fragments altogether
			foreach ((InwOaFragment3 Fragment, double DistanceSquaredToBoxMin) in FragmentsWithDistanceSquared.OrderBy(It => It.Item2).ToList())
			{
				// Skip checking fragments - they are definitely farther from currently found closest vertex
				if (DistanceSquaredBest < DistanceSquaredToBoxMin)
				{
					break;
				}

				Geometry Geometry = LocalTriangleReader.ReadGeometry(Fragment, new GeometrySettings());
				TransformMatrix LocalToWorldTransform = Converters.ConvertMatrix(Fragment.GetLocalToWorldMatrix());
				foreach (GeometryVertex Vertex in GeometryUtil.EnumerateVertices(Geometry))
				{
					Vector3d PositionWorldSpace = LocalToWorldTransform.TransformPosition(Vertex.Position);

					double DistanceSquared = (PositionWorldSpace - Point).LengthSquared();
					if (DistanceSquaredBest > DistanceSquared)
					{
						DistanceSquaredBest = DistanceSquared;
						PositionWorldSpaceBest = PositionWorldSpace;
					}
				}
			}

			// Without this Navisworks COM api collection can be released by .NET runtime too soon making collection items deallocated too(implementation detail of NW COM api)
			GC.KeepAlive(Fragments);
			return PositionWorldSpaceBest;
		}

		public override Cursor GetCursor(View View, KeyModifiers Modifier)
		{
			return Cursor.Focus;
		}

		public override bool ContextMenu(View View, int X, int Y)
		{
			return false;
		}

		// Render seems to work, but not OverlayRender
		public override void Render(View View, Graphics Graphics)
		{
			if (PickedModelItem != null && !PickedModelItem.IsDisposed)
			{
				// Render gizmo to highlight picked point
				BoundingBox3D Box = PickedModelItem.BoundingBox();
				Vector3D BoxSize = (Box.Max - Box.Min);
				double Size = Math.Max(Math.Max(BoxSize.X, BoxSize.Y), BoxSize.Z)*0.5;
				Point3D Origin = new Point3D(PickedPoint.X, PickedPoint.Y, PickedPoint.Z);

				Vector3D X = new Vector3D(Size, 0, 0);
				Vector3D Y = new Vector3D(0, Size, 0);
				Vector3D Z = new Vector3D(0, 0, Size);
				Graphics.Color(new Color(1, 0, 0), 1);
				Graphics.Line(Origin - X, Origin + X);
				Graphics.Color(new Color(0, 1, 0), 1);
				Graphics.Line(Origin - Y, Origin + Y);
				Graphics.Color(new Color(0, 0, 1), 1);
				Graphics.Line(Origin - Z, Origin + Z);
			}
		}
	}

	public struct Vector2d
	{
		public double X;
		public double Y;

		public Vector2d(double X, double Y)
		{
			this.X = X;
			this.Y = Y;
		}
	}

	public struct Vector3d
	{
		public double X;
		public double Y;
		public double Z;

		public Vector3d(double X, double Y, double Z)
		{
			this.X = X;
			this.Y = Y;
			this.Z = Z;
		}

		public static Vector3d operator -(Vector3d V) => new Vector3d(){ X = -V.X, Y = -V.Y,  Z = -V.Z};
		public static Vector3d operator -(Vector3d A, Vector3d B) => new Vector3d(){ X = A.X - B.X, Y = A.Y - B.Y,  Z = A.Z - B.Z};
		public static Vector3d operator +(Vector3d A, Vector3d B) => new Vector3d(){ X = A.X + B.X, Y = A.Y + B.Y,  Z = A.Z + B.Z};
		public static Vector3d operator *(Vector3d A, Vector3d B) => new Vector3d(){ X = A.X * B.X, Y = A.Y * B.Y,  Z = A.Z * B.Z};
		public static Vector3d operator *(Vector3d A, double S) => new Vector3d(){ X =  A.X * S, Y = A.Y * S,  Z = A.Z * S};

		public double LengthSquared() => X*X + Y*Y + Z*Z;

		public Vector3d Abs() => new Vector3d(){ X =  Math.Abs(X), Y = Math.Abs(Y),  Z = Math.Abs(Z)};
		public Vector3d Min(double S) => new Vector3d(){ X =  Math.Min(X, S), Y = Math.Min(Y, S),  Z = Math.Min(Z, S)};
		public Vector3d Max(double S) => new Vector3d(){ X =  Math.Max(X, S), Y = Math.Max(Y, S),  Z = Math.Max(Z, S)};
 
		public Vector3d Min(Vector3d Other) => new Vector3d(){ X =  Math.Min(X, Other.X), Y = Math.Min(Y, Other.Y),  Z = Math.Min(Z, Other.Z)};
		public Vector3d Max(Vector3d Other) => new Vector3d(){ X =  Math.Max(X, Other.X), Y = Math.Max(Y, Other.Y),  Z = Math.Max(Z, Other.Z)};

		public bool AlmostEqual(Vector3d Other, double Threshold)
		{
			return (Math.Abs(X - Other.X) <= Threshold) 
			       && (Math.Abs(Y - Other.Y) <= Threshold) 
			       && (Math.Abs(Z - Other.Z) <= Threshold);
		}

		public Vector3d Cross(Vector3d V)
		{
			return new Vector3d(
				Y * V.Z - Z * V.Y,
				Z * V.X - X * V.Z,
				X * V.Y - Y * V.X);
		}
	}

	public struct Vector3
	{
		public float X;
		public float Y;
		public float Z;

		public Vector3(Vector3d V)
		{
			X = (float) V.X;
			Y = (float) V.Y;
			Z = (float) V.Z;
		}

		public static Vector3 operator +(Vector3 A, Vector3 B) => new Vector3(){ X = A.X + B.X, Y = A.Y + B.Y,  Z = A.Z + B.Z};
		public static Vector3 operator -(Vector3 A, Vector3 B) => new Vector3(){ X = A.X - B.X, Y = A.Y - B.Y,  Z = A.Z - B.Z};
		public static Vector3 operator *(Vector3 A, Vector3 B) => new Vector3(){ X = A.X * B.X, Y = A.Y * B.Y,  Z = A.Z * B.Z};

		public void Normalize()
		{
			float LengthSquared = X * X + Y * Y + Z * Z;

			if (!(LengthSquared > 1e-8f))
			{
				return;
			}

			float Scale = (float)(1.0 / Math.Sqrt(LengthSquared));
			X *= Scale;
			Y *= Scale;
			Z *= Scale;
		}


	}

	struct Box
	{
		public Vector3 Min;
		public Vector3 Max;

		public bool Intersects(Box Other)
		{
			Box A = this;
			Box B = Other;
			return A.MinLessThanMaxOf(B) && B.MinLessThanMaxOf(A);
		}

		private bool MinLessThanMaxOf(Box Other)
		{
			return (Min.X < Other.Max.X) 
			       && (Min.Y < Other.Max.Y)
			       && (Min.Z < Other.Max.Z);
		}
	}

	public class TransformMatrix : IEquatable<TransformMatrix>
	{
		public readonly double[] Matrix4x4;

		public float[] Floats => Array.ConvertAll(Matrix4x4, X => (float) X);

		public TransformMatrix(double[] Matrix4x4)
		{
			this.Matrix4x4 = Matrix4x4;
		}

		public override bool Equals(object Other) 
		{
			return Equals(Other as TransformMatrix);
		}

		public bool Equals(TransformMatrix Other)
		{
			return Matrix4x4.SequenceEqual(Other.Matrix4x4);
		}

		public override int GetHashCode()
		{
			int Hash = 0;

			foreach (double F in Matrix4x4)
			{
				Hash ^= F.GetHashCode();
			}

			return Hash;
		}

		public ref double M(int X, int Y)
		{
			return ref Matrix4x4[Y * 4 + X];
		}

		public TransformMatrix ConcatTranslation(Vector3d Translation) 
		{
			TransformMatrix Result = new TransformMatrix(Matrix4x4.ToArray());

			Result.M(0, 3) += Translation.X;
			Result.M(1, 3) += Translation.Y;
			Result.M(2, 3) += Translation.Z;

			return Result;
		}


		public Vector3d TransformPosition(Vector3d V)
		{
			Vector3d R;

			R.X = V.X * M(0, 0) + V.Y * M(0, 1) + V.Z * M(0, 2) + 1 * M(0, 3);
			R.Y = V.X * M(1, 0) + V.Y * M(1, 1) + V.Z * M(1, 2) + 1 * M(1, 3);
			R.Z = V.X * M(2, 0) + V.Y * M(2, 1) + V.Z * M(2, 2) + 1 * M(2, 3);

			return R;
		}

		public Vector3d TransformVector(Vector3d V)
		{
			Vector3d R;

			R.X = V.X * M(0, 0) + V.Y * M(0, 1) + V.Z * M(0, 2) + 0 * M(0, 3);
			R.Y = V.X * M(1, 0) + V.Y * M(1, 1) + V.Z * M(1, 2) + 0 * M(1, 3);
			R.Z = V.X * M(2, 0) + V.Y * M(2, 1) + V.Z * M(2, 2) + 0 * M(2, 3);

			return R;
		}

		// Assumes that there's no non-uniform scaling
		public Vector3d TransformNormal(Vector3d V)
		{
			return TransformVector(V);
		}

		public TransformMatrix Multiply(TransformMatrix Other) 
		{
			TransformMatrix Result = new TransformMatrix(new double[16]);

			for (int i = 0; i < 4; i++)
			{
				for (int j = 0; j < 4; j++)
				{
					double V = 0;
					for (int k = 0; k < 4; k++)
					{
						V += M(i, k) * Other.M(k, j);
					}

					Result.M(i, j) = V;
				}
			}

			return Result;
		}

		public static TransformMatrix Identity => new TransformMatrix(new double[]
			{
				1, 0, 0, 0,
				0, 1, 0, 0,
				0, 0, 1, 0,
				0, 0, 0, 1,
			});
	};

	class Converters
	{
		public static TransformMatrix ConvertMatrix(InwLTransform3f LocalToWorldMatrix)
		{
			Array Matrix = (Array) LocalToWorldMatrix.Matrix;

			double[] MatrixFloat = new double[16];

			foreach (int Index in Enumerable.Range(0, 16))
			{
				MatrixFloat[Index] = (double) Matrix.GetValue(Index + Matrix.GetLowerBound(0));
			}

			return new TransformMatrix(MatrixFloat);
		}

		public static Vector3d ConvertVector3(InwLPos3f V)
		{
			return new Vector3d(V.data1, V.data2, V.data3);
		}

		public static Vector3d ConvertVector3(InwLVec3f V)
		{
			return new Vector3d(V.data1, V.data2, V.data3);
		}

		public static Vector3d ConvertVector3(InwLUnitVec3f V)
		{
			return new Vector3d(V.data1, V.data2, V.data3);
		}
	}

	unsafe struct GeometryVertex
	{
		private readonly Geometry Geometry;
		private int VertexIndex;

		public GeometryVertex(Geometry Geometry, int VertexIndex)
		{
			this.Geometry = Geometry;
			this.VertexIndex = VertexIndex;
		}

		public Vector3d Position =>
			new Vector3d(Geometry.Coords[VertexIndex * 3 + 0], Geometry.Coords[VertexIndex * 3 + 1], Geometry.Coords[VertexIndex * 3 + 2]);

		public Vector3d Normal =>
			new Vector3d(Geometry.Normals[VertexIndex * 3 + 0], Geometry.Normals[VertexIndex * 3 + 1], Geometry.Normals[VertexIndex * 3 + 2]);

		public Vector2d UV =>
			new Vector2d(Geometry.UVs[VertexIndex * 2 + 0], Geometry.UVs[VertexIndex * 2 + 1]);

	}

	class GeometryUtil
	{
		public class BoundingBox
		{
			private Vector3d BoundMin;
			private Vector3d BoundMax;
			private bool bIsSet;

			public BoundingBox()
			{
				bIsSet = false;
			}

			public bool IsSet()
			{
				return bIsSet;
			}

			public Vector3d Center => (BoundMin + BoundMax) * 0.5;
			public Vector3d BottomCenter => new Vector3d((BoundMin.X + BoundMax.X) * 0.5, (BoundMin.Y + BoundMax.Y) * 0.5, BoundMin.Z);

			public void Extend(Geometry Geometry, TransformMatrix Transform=null)
			{
				foreach (GeometryVertex Vertex in EnumerateVertices(Geometry))
				{
					Vector3d CoordTransformed = Transform?.TransformPosition(Vertex.Position) ?? Vertex.Position;

					Extend(CoordTransformed);
				}
			}

			public void Extend(BoundingBox Other)
			{
				if (!Other.bIsSet)
				{
					return;
				}

				foreach (Vector3d V in Other.EnumerateCorners())
				{
					Extend(V);
				}
			}

			private void Extend(Vector3d V)
			{
				if (bIsSet)
				{
					BoundMin = BoundMin.Min(V);
					BoundMax = BoundMax.Max(V);
				}
				else
				{
					BoundMin = V;
					BoundMax = V;
					bIsSet = true;
				}
			}

			public IEnumerable<Vector3d> EnumerateCorners()
			{
				Vector3d[] Bounds = { BoundMin, BoundMax };

				for (int i = 0; i < 8; ++i)
				{
					yield return new Vector3d(Bounds[i & 1].X, Bounds[(i & 2) >> 1].Y, Bounds[(i & 4) >> 2].Z);
				}
			}

			public BoundingBox Transform(TransformMatrix TransformMatrix)
			{
				BoundingBox Result = new BoundingBox();
				if (!bIsSet)
				{
					return Result;
				}

				foreach (Vector3d V in EnumerateCorners())
				{
					Result.Extend(TransformMatrix.TransformPosition(V));
				}

				return Result;
			}

		};



		public static IEnumerable<GeometryVertex> EnumerateVertices(Geometry Geometry)
		{
			for (int VertexIndex = 0; VertexIndex < Geometry.VertexCount; VertexIndex++)
			{
				yield return new GeometryVertex(Geometry, VertexIndex);
			}
		}

		public static unsafe Vector3d GetPosition(Geometry Geometry, uint Index)
		{
			return new Vector3d(
				Geometry.Coords[Index * 3 + 0], 
				Geometry.Coords[Index * 3 + 1],
				Geometry.Coords[Index * 3 + 2]);
		}

		public static unsafe bool HasNonDegenerateTriangles(Geometry Geometry, double Threshold)
		{
			for (int TriangleIndex = 0; TriangleIndex < Geometry.TriangleCount; TriangleIndex++)
			{
				Vector3d P0 = GetPosition(Geometry, Geometry.Indices[TriangleIndex * 3 + 0]);
				Vector3d P1 = GetPosition(Geometry, Geometry.Indices[TriangleIndex * 3 + 1]);
				Vector3d P2 = GetPosition(Geometry, Geometry.Indices[TriangleIndex * 3 + 2]);
				if (!(P0.AlmostEqual(P1, Threshold)
				    || P0.AlmostEqual(P2, Threshold)
				    || P1.AlmostEqual(P2, Threshold)))
				{
					return true;
				}
			}
			return false;
		}
	}
}
