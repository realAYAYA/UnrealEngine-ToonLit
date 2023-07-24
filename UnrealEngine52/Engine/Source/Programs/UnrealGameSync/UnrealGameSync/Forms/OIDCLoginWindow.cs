// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Drawing;
using System.Windows.Forms;
using EpicGames.OIDC;
using UnrealGameSync.Controls;

namespace UnrealGameSync.Forms
{
	public partial class OidcLoginWindow : Form
	{
		public OidcLoginWindow(OidcTokenManager oidcManager)
		{
			InitializeComponent();

			int lastYPosition = 10;
			foreach ((string providerIdentifier, ProviderInfo provider) in oidcManager.Providers)
			{
				OidcControl serviceControl = new OidcControl(oidcManager, providerIdentifier, provider.DisplayName)
				{
					Anchor = AnchorStyles.Top | AnchorStyles.Left | AnchorStyles.Right,
					Location = new Point(10, lastYPosition)
				};
				Controls.Add(serviceControl);

				lastYPosition = serviceControl.Size.Height + serviceControl.Location.Y + 10;
			}
		}

		private void DoneButton_Click(object sender, EventArgs e)
		{
			Close();
		}
	}
}
