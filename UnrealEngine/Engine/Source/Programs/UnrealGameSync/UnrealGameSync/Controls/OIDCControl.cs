// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Windows.Forms;
using EpicGames.OIDC;

namespace UnrealGameSync.Controls
{
	public partial class OidcControl : UserControl
	{
		private Font _badgeFont;
		private readonly OidcTokenManager _oidcManager;
		private readonly string _providerIdentifier;

		public OidcControl(OidcTokenManager inOidcManager, string inProviderIdentifier, string serviceName)
		{
			InitializeComponent();

			OIDCControlGroupBox.Text = serviceName;
			_oidcManager = inOidcManager;
			_providerIdentifier = inProviderIdentifier;

			_badgeFont = new Font(this.Font.FontFamily, this.Font.SizeInPoints - 2, FontStyle.Bold);

			OidcStatus serviceStatus = _oidcManager.GetStatusForProvider(_providerIdentifier);
			LoginButton.Enabled = serviceStatus == OidcStatus.NotLoggedIn;
		}

		private void DrawBadge(Graphics graphics, Rectangle badgeRect, string badgeText, Color badgeColor, bool mergeLeft, bool mergeRight)
		{
			if (badgeColor.A != 0)
			{
				using (GraphicsPath path = new GraphicsPath())
				{
					path.StartFigure();
					path.AddLine(badgeRect.Left + (mergeLeft ? 1 : 0), badgeRect.Top, badgeRect.Left - (mergeLeft ? 1 : 0), badgeRect.Bottom);
					path.AddLine(badgeRect.Left - (mergeLeft ? 1 : 0), badgeRect.Bottom, badgeRect.Right - 1 - (mergeRight ? 1 : 0), badgeRect.Bottom);
					path.AddLine(badgeRect.Right - 1 - (mergeRight ? 1 : 0), badgeRect.Bottom, badgeRect.Right - 1 + (mergeRight ? 1 : 0), badgeRect.Top);
					path.AddLine(badgeRect.Right - 1 + (mergeRight ? 1 : 0), badgeRect.Top, badgeRect.Left + (mergeLeft ? 1 : 0), badgeRect.Top);
					path.CloseFigure();

					using (SolidBrush brush = new SolidBrush(badgeColor))
					{
						graphics.FillPath(brush, path);
					}
				}

				TextRenderer.DrawText(graphics, badgeText, _badgeFont, badgeRect, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix | TextFormatFlags.PreserveGraphicsClipping);
			}
		}

		private void StatusPanel_Paint(object sender, PaintEventArgs e)
		{
			OidcStatus serviceStatus = _oidcManager.GetStatusForProvider(_providerIdentifier);
			DrawBadge(e.Graphics, e.ClipRectangle, ToServiceDisplayText(serviceStatus), ToServiceDisplayColor(serviceStatus), false, false);
		}

		private Color ToServiceDisplayColor(OidcStatus oidcStatus)
		{
			switch (oidcStatus)
			{
				case OidcStatus.Connected:
				case OidcStatus.TokenRefreshRequired:
					return Color.Green;
				case OidcStatus.NotLoggedIn:
					return Color.Red;
				default:
					throw new ArgumentOutOfRangeException(nameof(oidcStatus), oidcStatus, null);
			}
		}

		private string ToServiceDisplayText(OidcStatus oidcStatus)
		{
			switch (oidcStatus)
			{
				case OidcStatus.Connected:
					return "Connected";
				case OidcStatus.NotLoggedIn:
					return "Not Logged In";
				case OidcStatus.TokenRefreshRequired:
					return "Refresh Pending";
				default:
					throw new ArgumentOutOfRangeException(nameof(oidcStatus), oidcStatus, null);
			}
		}

		private async void LoginButton_Click(object sender, EventArgs e)
		{
			try
			{
				await _oidcManager.Login(_providerIdentifier);

				Focus();

				OidcStatus serviceStatus = _oidcManager.GetStatusForProvider(_providerIdentifier);
				LoginButton.Enabled = serviceStatus == OidcStatus.NotLoggedIn;

				Refresh();
			}
			catch (LoginFailedException exception)
			{
				MessageBox.Show(exception.Message, "Login Failed", MessageBoxButtons.OK);
			}
		}
	}
}
