// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

#nullable enable

namespace UnrealGameSync
{
	public partial class SdkInfoWindow : Form
	{
		class SdkItem
		{
			public string Category { get; }
			public string Description { get; }
			public string? Install;
			public string? Browse;

			public SdkItem(string category, string description)
			{
				this.Category = category;
				this.Description = description;
			}
		}

		class BadgeInfo
		{
			public string UniqueId { get; }
			public string Label { get; }
			public Rectangle Rectangle;
			public Action? OnClick;

			public BadgeInfo(string uniqueId, string label)
			{
				this.UniqueId = uniqueId;
				this.Label = label;
			}
		}

		Font _badgeFont;
		string? _hoverBadgeUniqueId;

		public SdkInfoWindow(string[] sdkInfoEntries, Dictionary<string, string> variables, Font badgeFont)
		{
			InitializeComponent();

			this._badgeFont = badgeFont;

			Dictionary<string, ConfigObject> uniqueIdToObject = new Dictionary<string, ConfigObject>(StringComparer.InvariantCultureIgnoreCase);
			foreach(string sdkInfoEntry in sdkInfoEntries)
			{
				ConfigObject obj = new ConfigObject(sdkInfoEntry);

				string uniqueId = obj.GetValue("UniqueId", Guid.NewGuid().ToString());

				ConfigObject? existingObject;
				if(uniqueIdToObject.TryGetValue(uniqueId, out existingObject))
				{
					existingObject.AddOverrides(obj, null);
				}
				else
				{
					uniqueIdToObject.Add(uniqueId, obj);
				}
			}

			List<SdkItem> items = new List<SdkItem>();
			foreach(ConfigObject obj in uniqueIdToObject.Values)
			{
				string category = obj.GetValue("Category", "Other");
				string description = obj.GetValue("Description", "");
				SdkItem item = new SdkItem(category, description);

				item.Install = Utility.ExpandVariables(obj.GetValue("Install", ""), variables);
				if(item.Install.Contains("$("))
				{
					item.Install = null;
				}

				item.Browse = Utility.ExpandVariables(obj.GetValue("Browse", ""), variables);
				if(item.Browse.Contains("$("))
				{
					item.Browse = null;
				}

				if(!String.IsNullOrEmpty(item.Install) && String.IsNullOrEmpty(item.Browse))
				{
					try
					{
						item.Browse = Path.GetDirectoryName(item.Install);
					}
					catch
					{
						item.Browse = null;
					}
				}

				items.Add(item);
			}

			foreach(IGrouping<string, SdkItem> itemGroup in items.GroupBy(x => x.Category).OrderBy(x => x.Key))
			{
				ListViewGroup group = new ListViewGroup(itemGroup.Key);
				SdkListView.Groups.Add(group);

				foreach(SdkItem item in itemGroup)
				{
					ListViewItem newItem = new ListViewItem(group);
					newItem.SubItems.Add(item.Description);
					newItem.SubItems.Add(new ListViewItem.ListViewSubItem(){ Tag = item });
					SdkListView.Items.Add(newItem);
				}
			}

			System.Reflection.PropertyInfo doubleBufferedProperty = typeof(Control).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!;
			doubleBufferedProperty.SetValue(SdkListView, true, null);

			if(SdkListView.Items.Count > 0)
			{
				int itemsHeight = SdkListView.Items[SdkListView.Items.Count - 1].Bounds.Bottom + 20;
				Height = SdkListView.Top + itemsHeight + (Height - SdkListView.Bottom);
			}
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.OK;
			Close();
		}

		private void SdkListView_DrawItem(object sender, DrawListViewItemEventArgs e)
		{
			SdkListView.DrawBackground(e.Graphics, e.Item);
		}

		private void SdkListView_DrawSubItem(object sender, DrawListViewSubItemEventArgs e)
		{
			if(e.ColumnIndex != columnHeader3.Index)
			{
				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, SdkListView.Font, e.Bounds, SdkListView.ForeColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else
			{
				e.Graphics.SmoothingMode = SmoothingMode.AntiAlias;

				List<BadgeInfo> badges = GetBadges(e.Item, e.SubItem);
				for(int idx = 0; idx < badges.Count; idx++)
				{
					Color badgeColor = (_hoverBadgeUniqueId == badges[idx].UniqueId)? Color.FromArgb(140, 180, 230) : Color.FromArgb(112, 146, 190);
					if(badges[idx].OnClick != null)
					{
						DrawBadge(e.Graphics, badges[idx].Label, badges[idx].Rectangle, (idx > 0), (idx < badges.Count - 1), badgeColor);
					}
				}
			}
		}

		private void SdkListView_MouseMove(object sender, MouseEventArgs e)
		{
			string? newHoverUniqueId = null;

			ListViewHitTestInfo hitTest = SdkListView.HitTest(e.Location);
			if(hitTest.Item != null && hitTest.SubItem == hitTest.Item.SubItems[2])
			{
				List<BadgeInfo> badges = GetBadges(hitTest.Item, hitTest.SubItem);
				foreach(BadgeInfo badge in badges)
				{
					if(badge.Rectangle.Contains(e.Location))
					{
						newHoverUniqueId = badge.UniqueId;
					}
				}
			}

			if(newHoverUniqueId != _hoverBadgeUniqueId)
			{
				_hoverBadgeUniqueId = newHoverUniqueId;
				SdkListView.Invalidate();
			}
		}

		private void SdkListView_MouseLeave(object sender, EventArgs e)
		{
			_hoverBadgeUniqueId = null;
		}

		private void SdkListView_MouseDown(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo hitTest = SdkListView.HitTest(e.Location);
			if(hitTest.Item != null && hitTest.SubItem == hitTest.Item.SubItems[2])
			{
				List<BadgeInfo> badges = GetBadges(hitTest.Item, hitTest.SubItem);
				foreach(BadgeInfo badge in badges)
				{
					if(badge.Rectangle.Contains(e.Location) && badge.OnClick != null)
					{
						badge.OnClick();
					}
				}
			}
		}

		private List<BadgeInfo> GetBadges(ListViewItem item, ListViewItem.ListViewSubItem subItem)
		{
			string uniqueIdPrefix = String.Format("{0}_", item.Index);

			List<BadgeInfo> badges = new List<BadgeInfo>();

			SdkItem sdk = (SdkItem)subItem.Tag;

			Action? installAction = null;
			if(!String.IsNullOrEmpty(sdk.Install))
			{
				installAction = () => { Install(sdk.Install); };
			}
			badges.Add(new BadgeInfo(uniqueIdPrefix + "_Install", "Install") { OnClick = installAction });

			Action? browseAction = null;
			if(!String.IsNullOrEmpty(sdk.Browse))
			{
				browseAction = () => { Browse(sdk.Browse); };
			}
			badges.Add(new BadgeInfo(uniqueIdPrefix + "_Browse", "Browse"){ OnClick = browseAction });

			int right = subItem.Bounds.Right - 10;
			for(int idx = badges.Count - 1; idx >= 0; idx--)
			{
				Size badgeSize = GetBadgeSize(badges[idx].Label);
				right -= badgeSize.Width;
				badges[idx].Rectangle = new Rectangle(right, subItem.Bounds.Y + (subItem.Bounds.Height - badgeSize.Height) / 2, badgeSize.Width, badgeSize.Height);
			}

			return badges;
		}

		private void Browse(string directoryName)
		{
			try
			{
				Process.Start("explorer.exe", String.Format("\"{0}\"", directoryName));
			}
			catch(Exception ex)
			{
				MessageBox.Show(String.Format("Unable to open explorer to {0}: {1}", directoryName, ex.Message));
			}
		}

		private void Install(string fileName)
		{
			try
			{
				ProcessStartInfo startInfo = new ProcessStartInfo();
				startInfo.FileName = fileName;
				startInfo.UseShellExecute = true;
				Process.Start(startInfo);
			}
			catch(Exception ex)
			{
				MessageBox.Show(String.Format("Unable to run {0}: {1}", fileName, ex.Message));
			}
		}

		private Size GetBadgeSize(string badgeText)
		{
			Size labelSize = TextRenderer.MeasureText(badgeText, _badgeFont);
			int badgeHeight = _badgeFont.Height + 1;

			return new Size(labelSize.Width + badgeHeight - 4, badgeHeight);
		}

		private void DrawBadge(Graphics graphics, string badgeText, Rectangle badgeRect, bool mergeLeft, bool mergeRight, Color badgeColor)
		{
			using (GraphicsPath path = new GraphicsPath())
			{
				path.StartFigure();
				path.AddLine(badgeRect.Left + (mergeLeft? 1 : 0), badgeRect.Top, badgeRect.Left - (mergeLeft? 1 : 0), badgeRect.Bottom);
				path.AddLine(badgeRect.Left - (mergeLeft? 1 : 0), badgeRect.Bottom, badgeRect.Right - 1 - (mergeRight? 1 : 0), badgeRect.Bottom);
				path.AddLine(badgeRect.Right - 1 - (mergeRight? 1 : 0), badgeRect.Bottom, badgeRect.Right - 1 + (mergeRight? 1 : 0), badgeRect.Top);
				path.AddLine(badgeRect.Right - 1 + (mergeRight? 1 : 0), badgeRect.Top, badgeRect.Left + (mergeLeft? 1 : 0), badgeRect.Top);
				path.CloseFigure();

				using(SolidBrush brush = new SolidBrush(badgeColor))
				{
					graphics.FillPath(brush, path);
				}
			}

			TextRenderer.DrawText(graphics, badgeText, _badgeFont, badgeRect, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix | TextFormatFlags.PreserveGraphicsClipping);
		}
	}
}
