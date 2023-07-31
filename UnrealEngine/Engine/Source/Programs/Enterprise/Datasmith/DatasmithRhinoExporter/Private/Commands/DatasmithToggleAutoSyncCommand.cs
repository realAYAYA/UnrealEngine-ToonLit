// Copyright Epic Games, Inc. All Rights Reserved.

using DatasmithRhino.DirectLink;
using DatasmithRhino.Properties.Localization;
using Rhino;
using Rhino.Commands;

namespace DatasmithRhino.Commands
{
	/**
	 * Command used to toggle a direct link auto sync on and off.
	 */
	public class DatasmithToggleAutoSyncCommand : Command
	{
		public DatasmithToggleAutoSyncCommand()
		{
			// Rhino only creates one instance of each command class defined in a
			// plug-in, so it is safe to store a reference in a static property.
			Instance = this;
		}

		/**
		 * The only instance of this command.
		 */
		public static DatasmithToggleAutoSyncCommand Instance {
			get; private set;
		}

		/**
		 * The command name as it appears on the Rhino command line.
		 */
		public override string EnglishName {
			get { return "DatasmithToggleAutoSync"; }
		}

		///TODO: This needs to be localized.
		public override string LocalName {
			get { return Resources.DatasmithToggleAutoSyncCommand; }
		}

		protected override Result RunCommand(RhinoDoc RhinoDocument, RunMode Mode)
		{
			DatasmithRhinoDirectLinkManager DirectLinkManager = DatasmithRhinoPlugin.Instance?.DirectLinkManager;
			Result CommandResult = Result.Failure;

			if (DirectLinkManager != null)
			{
				bool bAutoSyncToggledValue = !DirectLinkManager.bAutoSyncActive;
				CommandResult = DirectLinkManager.SetLiveLink(bAutoSyncToggledValue);

				if (CommandResult == Result.Success)
				{
					RhinoApp.WriteLine(string.Format(Resources.DatasmithToggleAutoSyncMessageStatus, bAutoSyncToggledValue ? Resources.DatasmithAutoSyncStatusEnabled : Resources.DatasmithAutoSyncStatusDisabled));
				}
			}

			return CommandResult;
		}
	}
}