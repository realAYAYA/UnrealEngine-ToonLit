// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.DirectLink;
using DatasmithRhino.Properties.Localization;

using Eto.Drawing;
using Eto.Forms;
using Rhino;
using Rhino.Commands;
using System;
using System.Collections.Generic;

namespace DatasmithRhino.Commands
{
	/**
	 * Command used to open the direct link connection management window.
	 */
	public class DatasmithDirectLinkConnectionsCommand : Rhino.Commands.Command
	{
		private DatasmithConnectionForm ConnectionForm;

		public DatasmithDirectLinkConnectionsCommand()
		{
			// Rhino only creates one instance of each command class defined in a
			// plug-in, so it is safe to store a reference in a static property.
			Instance = this;
		}

		/**
		 * The only instance of this command.
		 */
		public static DatasmithDirectLinkConnectionsCommand Instance {
			get; private set;
		}

		/**
		 * The command name as it appears on the Rhino command line.
		 */
		public override string EnglishName {
			get { return "DatasmithDirectLinkConnections"; }
		}

		public override string LocalName {
			get { return Resources.DatasmithDirectLinkConnectionsCommand; }
		}

		private class DatasmithConnectionForm : Eto.Forms.Form
		{
			public bool bIsClosed { get; private set; } = false;
			private FDatasmithFacadeEndpointObserver EndPointObserver = null;
			private FilePicker CacheDirectoryPicker = null;
			private GridView ConnectionGrid = null;
			private FilterCollection<List<string>> ConnectionGridRowContent = null;
			private Dictionary<uint, int> StreamIdIndexMap = new Dictionary<uint, int>();

			private struct ConnectionInfo
			{
				public FDatasmithFacadeRawInfo.FDatasmithFacadeStreamInfo StreamInfo;
				public FDatasmithFacadeRawInfo.FDatasmithFacadeDataPointInfo SourceDataPointInfo;
				public FDatasmithFacadeRawInfo.FDatasmithFacadeDataPointInfo DestinationDataPointInfo;
				public FDatasmithFacadeRawInfo.FDatasmithFacadeEndpointInfo DestinationEndpointInfo;

				public bool IsConnectedToThisEndpoint()
				{
					return StreamInfo.IsActive() && SourceDataPointInfo != null && SourceDataPointInfo.IsOnThisEndpoint();
				}
			}

			private struct ConnectionDisplayInfo
			{
				public uint StreamId;
				public string SourceText;
				public string DestinationText;
			}

			public DatasmithConnectionForm()
			{
				DatasmithRhinoDirectLinkManager DirectLinkManager = DatasmithRhinoPlugin.Instance?.DirectLinkManager;
				if (DirectLinkManager != null)
				{
					EndPointObserver = new FDatasmithFacadeEndpointObserver();
					EndPointObserver.OnStateChangedEvent += OnStateChange;
					DirectLinkManager.DirectLink.AddEndpointObserver(EndPointObserver);
				}

				Init();
			}

			private void Init()
			{
				Title = "Datasmith Direct Link Connection Status";
				ClientSize = new Size(400, 500);

				Size DefaultSpacing = new Size(5, 5); // space between each cell
				Padding DefaultPadding = new Padding(10, 10, 10, 10); // space around the table's sides

				ListBox ConnectionList = new ListBox();
				ConnectionList.Items.Add("Connection1");
				ConnectionList.Items.Add("Connection2");
				
				CacheDirectoryPicker = new FilePicker();
				CacheDirectoryPicker.FileAction = Eto.FileAction.SelectFolder;
				CacheDirectoryPicker.FilePath = DatasmithRhinoPlugin.Instance.DirectLinkManager.CacheDirectory;
				CacheDirectoryPicker.FilePathChanged += OnCacheDirectoryChanged;

				ConnectionGridRowContent = new FilterCollection<List<string>>();
				ConnectionGrid = new GridView
				{
					ShowHeader = true,
					DataStore = ConnectionGridRowContent,
					AllowMultipleSelection = true,
					GridLines = GridLines.None,
				};

				ConnectionGrid.Columns.Add(new GridColumn() { HeaderText = "Source", Sortable = false, DataCell = new TextBoxCell(0), AutoSize = false });
				ConnectionGrid.Columns.Add(new GridColumn() { HeaderText = "Destination", Sortable = false, DataCell = new TextBoxCell(1), AutoSize = false });
				
				Content = new TableLayout
				{
					Spacing = DefaultSpacing,
					Padding = DefaultPadding,
					Rows =
					{
						new TableRow
						{
							ScaleHeight = true,
							Cells =
							{
								new TableCell(ConnectionGrid, true)
							}
						},
						new TableRow
						{
							Cells =
							{
								new TableLayout
								{
									Rows =
									{
										new TableRow
										{
											Cells =
											{
												new TableCell(new Label { Text = "Cache Directory:" }, false),
												new TableCell(CacheDirectoryPicker, true)
											}
										}
									}
								}

							}
						}
					}
				};
			}

			protected override void OnClosed(EventArgs e)
			{
				base.OnClosed(e);

				bIsClosed = true;
				DatasmithRhinoDirectLinkManager DirectLinkManager = DatasmithRhinoPlugin.Instance?.DirectLinkManager;
				
				if (DirectLinkManager != null)
				{
					DirectLinkManager.DirectLink.RemoveEndpointObserver(EndPointObserver);
					EndPointObserver.OnStateChangedEvent -= OnStateChange;
					EndPointObserver = null;
				}
			}

			protected override void OnSizeChanged(EventArgs e)
			{
				base.OnSizeChanged(e);

				if (ConnectionGrid != null)
				{
					int BorderSize = ConnectionGrid.Border == BorderType.None ? 0 : 2;
					int ColumnWidth = (ConnectionGrid.Width - BorderSize) / ConnectionGrid.Columns.Count;
					foreach(GridColumn CurrentColumn in ConnectionGrid.Columns)
					{
						CurrentColumn.Width = ColumnWidth;
					}
				}
			}
			private void OnCacheDirectoryChanged(object Sender, EventArgs Args)
			{
				DatasmithRhinoDirectLinkManager DirectLinkManager = DatasmithRhinoPlugin.Instance?.DirectLinkManager;
				
				if (DirectLinkManager != null && !String.IsNullOrEmpty(CacheDirectoryPicker.FilePath))
				{
					DirectLinkManager.CacheDirectory = CacheDirectoryPicker.FilePath;
				}
			}

			private void OnStateChange(object sender, FDatasmithFacadeEndpointObserver.FDatasmithFacadeRawInfoEventArgs RawInfoArgs)
			{
				List<ConnectionDisplayInfo> StreamIdSourceDestinationTuples = new List<ConnectionDisplayInfo>();
				FDatasmithFacadeRawInfo RawInfo = RawInfoArgs.RawInfo;

				for (int StreamIndex = 0, StreamNum = RawInfo.GetNumberOfStreamsInfo(); StreamIndex < StreamNum; StreamIndex++)
				{
					FDatasmithFacadeRawInfo.FDatasmithFacadeStreamInfo StreamInfo = RawInfo.GetStreamInfo(StreamIndex);
					FDatasmithFacadeRawInfo.FDatasmithFacadeDataPointInfo SourceDataPointInfo = RawInfo.GetDataPointsInfo(StreamInfo.GetSource());

					bool bConnectedToThisEndpoint = StreamInfo.IsActive() && SourceDataPointInfo != null && SourceDataPointInfo.IsOnThisEndpoint();
					if (bConnectedToThisEndpoint)
					{
						FDatasmithFacadeRawInfo.FDatasmithFacadeDataPointInfo DestinationDataPointInfo = RawInfo.GetDataPointsInfo(StreamInfo.GetDestination());
						if (DestinationDataPointInfo != null)
						{
							ConnectionInfo CurrentConnectionInfo = new ConnectionInfo
							{
								StreamInfo = StreamInfo,
								SourceDataPointInfo = SourceDataPointInfo,
								DestinationDataPointInfo = DestinationDataPointInfo,
								DestinationEndpointInfo = RawInfo.GetEndpointInfo(DestinationDataPointInfo.GetEndpointAddress())
							};

							StreamIdSourceDestinationTuples.Add(GenerateConnectionString(CurrentConnectionInfo));
						}
					}
				}
				RhinoApp.InvokeOnUiThread(new OnUpdateConnectionListDelegate(UpdateConnectionList), StreamIdSourceDestinationTuples);
			}

			private ConnectionDisplayInfo GenerateConnectionString(ConnectionInfo InConnectionInfo)			{
				uint StreamId = InConnectionInfo.StreamInfo.GetStreamId();
				string SourceString = InConnectionInfo.SourceDataPointInfo.GetName();
				string DestinationString = String.Format("{0} : {1}", InConnectionInfo.DestinationEndpointInfo.GetName(), InConnectionInfo.DestinationDataPointInfo.GetName());

				return new ConnectionDisplayInfo { StreamId = StreamId, SourceText = SourceString, DestinationText = DestinationString };
			}

			private delegate void OnUpdateConnectionListDelegate(List<ConnectionDisplayInfo> ConnectedSourceDestinations);
			private void UpdateConnectionList(List<ConnectionDisplayInfo> ConnectedSourceDestinations)
			{
				bool[] FoundConnectedDestinations = new bool[ConnectionGridRowContent.Count];
				int NumberOfEntriesAdded = 0;
				
				foreach (ConnectionDisplayInfo CurrentConnectionDisplayInfo in ConnectedSourceDestinations)
				{
					if (StreamIdIndexMap.TryGetValue(CurrentConnectionDisplayInfo.StreamId, out int EntryIndex))
					{
						FoundConnectedDestinations[EntryIndex] = true;
					}
					else
					{
						ConnectionGridRowContent.Add(new List<string> { CurrentConnectionDisplayInfo.SourceText, CurrentConnectionDisplayInfo.DestinationText });
						StreamIdIndexMap.Add(CurrentConnectionDisplayInfo.StreamId, ConnectionGridRowContent.Count - 1);
						++NumberOfEntriesAdded;
					}
				}
				
				// Remove all connections which are no longer active.
				for (int EntryToDeleteIndex = ConnectedSourceDestinations.Count - NumberOfEntriesAdded - 1; EntryToDeleteIndex >= 0; --EntryToDeleteIndex)
				{
					if (!FoundConnectedDestinations[EntryToDeleteIndex])
					{
						ConnectionGridRowContent.RemoveAt(EntryToDeleteIndex);
					}
				}
			}
		}

		private void OpenEtoConnectionForm()
		{
			if (ConnectionForm == null || ConnectionForm.bIsClosed)
			{
				ConnectionForm = new DatasmithConnectionForm();
				ConnectionForm.Owner = Rhino.UI.RhinoEtoApp.MainWindow;
				ConnectionForm.CanFocus = true;
			}

			ConnectionForm.Show();
			ConnectionForm.Focus();
		}

		protected override Result RunCommand(RhinoDoc RhinoDocument, RunMode Mode)
		{
#if MAC_OS
			OpenEtoConnectionForm();
			bool bSuccess = true;
#else
			DatasmithRhinoDirectLinkManager DirectLinkManager = DatasmithRhinoPlugin.Instance?.DirectLinkManager;
			bool bSuccess = false;
			if (DirectLinkManager != null)
			{
				bSuccess = DirectLinkManager.OpenConnectionManangementWindow();
			}
#endif
			return bSuccess ? Result.Success : Result.Failure;
		}
	}
}