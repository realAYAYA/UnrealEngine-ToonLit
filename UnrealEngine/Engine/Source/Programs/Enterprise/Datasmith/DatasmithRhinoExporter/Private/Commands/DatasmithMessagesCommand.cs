// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.Properties.Localization;
using DatasmithRhino.Utils;

using Eto.Drawing;
using Eto.Forms;
using Rhino;
using Rhino.Commands;
using System;
using System.Text;

namespace DatasmithRhino.Commands
{
	/**
	 * Command used to open the datasmith message windows.
	 */
	public class DatasmithMessagesCommand : Rhino.Commands.Command
	{
		private DatasmithLogForm LogForm;

		public DatasmithMessagesCommand()
		{
			// Rhino only creates one instance of each command class defined in a
			// plug-in, so it is safe to store a reference in a static property.
			Instance = this;
		}

		/**
		 * The only instance of this command.
		 */
		public static DatasmithMessagesCommand Instance {
			get; private set;
		}

		/**
		 * The command name as it appears on the Rhino command line.
		 */
		public override string EnglishName {
			get { return "DatasmithMessages"; }
		}

		public override string LocalName {
			get { return Resources.DatasmithMessagesCommand; }
		}

		private class DatasmithLogForm : Eto.Forms.Form
		{
			public bool bIsClosed { get; private set; } = false;
			
			private TextArea LogTextArea;

			public DatasmithLogForm()
			{
				DatasmithRhinoLogManager LogManager = DatasmithRhinoPlugin.Instance.LogManager;
				LogManager.OnLogsChanged += OnLogsChanged;

				Init(GenerateLogString(LogManager));
			}

			private void OnLogsChanged(DatasmithRhinoLogManager LogManager)
			{
				if (LogTextArea != null)
				{
					LogTextArea.Text = GenerateLogString(LogManager);
				}
			}

			/// <summary>
			/// Generate the full log string by ordering the messages by log type in the following order: Error > Warning > Info
			/// </summary>
			/// <param name="LogManager"></param>
			/// <returns></returns>
			private string GenerateLogString(DatasmithRhinoLogManager LogManager)
			{
				StringBuilder Logs = new StringBuilder();

				Logs.Append(LogManager.GetLogs(DatasmithRhinoLogType.Error));
				Logs.Append(LogManager.GetLogs(DatasmithRhinoLogType.Warning));
				Logs.Append(LogManager.GetLogs(DatasmithRhinoLogType.Info));

				return Logs.ToString();
			}

			private void Init(string Logs)
			{
				Title = Resources.DatasmithMessagesTitle;
				ClientSize = new Size(400, 500);

				Size DefaultSpacing = new Size(5, 5); // space between each cell
				Padding DefaultPadding = new Padding(10, 10, 10, 10); // space around the table's sides

				//Keeping the reference of the Log TextArea
				LogTextArea = new TextArea { Text = Logs, ReadOnly = true };

				Button ClearLogsButton = new Button { Text = Resources.ClearMessagesButton };
				ClearLogsButton.Click += (object Sender, EventArgs Args) =>
				{
					DatasmithRhinoPlugin.Instance.LogManager.ClearLogs();
				};

				Content = new TableLayout
				{
					Spacing = DefaultSpacing,
					Padding = DefaultPadding,
					Rows =
					{
						new TableRow(
							new TableCell(new Label { Text = Resources.DatasmithMessagesTitle + ":" }, true)
						),
						new TableRow
						{ 
							ScaleHeight = true,
							Cells =
							{
								new TableCell(LogTextArea, true)

							}
						},
						new TableRow(
							new TableLayout
							{
								Spacing = DefaultSpacing,
								Padding = DefaultPadding,
								Rows =
								{
									new TableRow(
										new TableCell(null, true),
										ClearLogsButton
									)
								}
							}
						),
					}
				};
			}

			protected override void OnClosed(EventArgs e)
			{
				DatasmithRhinoPlugin.Instance.LogManager.OnLogsChanged -= OnLogsChanged;
				bIsClosed = true;
			}
		}

		protected override Result RunCommand(RhinoDoc doc, RunMode mode)
		{
			if (LogForm == null || LogForm.bIsClosed)
			{
				LogForm = new DatasmithLogForm();
				LogForm.Owner = Rhino.UI.RhinoEtoApp.MainWindow;
				LogForm.CanFocus = true;
			}

			LogForm.Show();
			LogForm.Focus();

			return Result.Success;
		}
	}
}