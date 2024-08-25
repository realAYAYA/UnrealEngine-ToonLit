// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;

namespace UnrealGameSync
{
	public partial class ArgumentsWindow : Form
	{
		const int LvmFirst = 0x1000;
		const int LvmEditlabelw = (LvmFirst + 118);

		const int EmSetsel = 0xb1;

		[DllImport("user32.dll", CharSet = CharSet.Auto)]
		static extern IntPtr SendMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);

		[DllImport("user32.dll")]
		static extern bool SetWindowText(IntPtr hWnd, string text);

		public ArgumentsWindow(List<Tuple<string, bool>> arguments, bool promptBeforeLaunch)
		{
			InitializeComponent();
			Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));

			ActiveControl = ArgumentsList;

			ArgumentsList.Font = SystemFonts.IconTitleFont;

			ArgumentsList.Items.Clear();

			foreach (Tuple<string, bool> argument in arguments)
			{
				ListViewItem item = new ListViewItem(argument.Item1);
				item.Checked = argument.Item2;
				ArgumentsList.Items.Add(item);
			}

			ListViewItem addAnotherItem = new ListViewItem("Click to add an item...", 0);
			ArgumentsList.Items.Add(addAnotherItem);

			PromptBeforeLaunchCheckBox.Checked = promptBeforeLaunch;
		}

		public bool PromptBeforeLaunch => PromptBeforeLaunchCheckBox.Checked;

		public List<Tuple<string, bool>> GetItems()
		{
			List<Tuple<string, bool>> items = new List<Tuple<string, bool>>();
			for (int idx = 0; idx < ArgumentsList.Items.Count - 1; idx++)
			{
				ListViewItem item = ArgumentsList.Items[idx];
				items.Add(new Tuple<string, bool>(item.Text, item.Checked));
			}
			return items;
		}

		private void ClearButton_Click(object sender, EventArgs e)
		{
			//			ArgumentsTextBox.Text = "";
			DialogResult = System.Windows.Forms.DialogResult.OK;
			Close();
		}

		private void ArgumentsList_AfterLabelEdit(object sender, LabelEditEventArgs e)
		{
			if ((e.Label == null && ArgumentsList.Items[e.Item].Text.Length == 0) || (e.Label != null && e.Label.Trim().Length == 0))
			{
				e.CancelEdit = true;
				ArgumentsList.Items.RemoveAt(e.Item);
			}
		}

		private void ArgumentsList_BeforeLabelEdit(object sender, LabelEditEventArgs e)
		{
			if (e.Item == ArgumentsList.Items.Count - 1)
			{
				e.CancelEdit = true;
			}
		}

		private void ArgumentsList_MouseClick(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo info = ArgumentsList.HitTest(e.Location);
			if (info.Item.Index == ArgumentsList.Items.Count - 1)
			{
				ListViewItem newItem = new ListViewItem();
				newItem.Checked = true;
				newItem = ArgumentsList.Items.Insert(ArgumentsList.Items.Count - 1, newItem);
				newItem.BeginEdit();
			}
			else
			{
				using (Graphics graphics = ArgumentsList.CreateGraphics())
				{
					int labelOffset = e.X - CheckBoxPadding - CheckBoxRenderer.GetGlyphSize(graphics, CheckBoxState.CheckedNormal).Width - CheckBoxPadding;
					if (labelOffset >= 0 && labelOffset < TextRenderer.MeasureText(info.Item.Text, ArgumentsList.Font).Width)
					{
						info.Item.BeginEdit();
					}
				}
			}
		}

		const int CheckBoxPadding = 5;

		private void ArgumentsList_DrawItem(object sender, DrawListViewItemEventArgs e)
		{
			e.DrawBackground();
			if (e.ItemIndex < ArgumentsList.Items.Count - 1)
			{
				CheckBoxState state = e.Item.Checked ? CheckBoxState.CheckedNormal : CheckBoxState.UncheckedNormal;
				Size checkSize = CheckBoxRenderer.GetGlyphSize(e.Graphics, state);
				CheckBoxRenderer.DrawCheckBox(e.Graphics, new Point(e.Bounds.Left + 4, e.Bounds.Top + (e.Bounds.Height - checkSize.Height) / 2), state);
				DrawItemLabel(e.Graphics, SystemColors.WindowText, e.Item);
			}
			else
			{
				DrawItemLabel(e.Graphics, SystemColors.GrayText, e.Item);
			}
		}

		private void DrawItemLabel(Graphics graphics, Color normalColor, ListViewItem item)
		{
			Rectangle labelRect = GetLabelRectangle(graphics, item);
			if (item.Selected)
			{
				graphics.FillRectangle(SystemBrushes.Highlight, labelRect);
				TextRenderer.DrawText(graphics, item.Text, ArgumentsList.Font, labelRect, SystemColors.HighlightText, SystemColors.Highlight, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine);
			}
			else
			{
				TextRenderer.DrawText(graphics, item.Text, ArgumentsList.Font, labelRect, normalColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine);
			}
		}

		private Rectangle GetLabelRectangle(Graphics graphics, ListViewItem item)
		{
			CheckBoxState state = item.Checked ? CheckBoxState.CheckedNormal : CheckBoxState.UncheckedNormal;
			Size checkSize = CheckBoxRenderer.GetGlyphSize(graphics, state);
			Size labelSize = TextRenderer.MeasureText(item.Text, ArgumentsList.Font);

			int labelIndent = CheckBoxPadding + checkSize.Width + CheckBoxPadding;
			return new Rectangle(item.Bounds.Left + labelIndent, item.Bounds.Top, labelSize.Width, item.Bounds.Height);
		}

		private void ArgumentsList_KeyUp(object sender, KeyEventArgs e)
		{
			if (e.KeyCode == Keys.Delete && ArgumentsList.SelectedIndices.Count == 1)
			{
				int index = ArgumentsList.SelectedIndices[0];
				ArgumentsList.Items.RemoveAt(index);
				if (index < ArgumentsList.Items.Count - 1)
				{
					ArgumentsList.Items[index].Selected = true;
				}
			}
		}

		private void ArgumentsList_KeyPress(object sender, KeyPressEventArgs e)
		{
			if (ArgumentsList.SelectedItems.Count == 1 && !Char.IsControl(e.KeyChar))
			{
				ListViewItem item = ArgumentsList.SelectedItems[0];
				if (item.Index == ArgumentsList.Items.Count - 1)
				{
					item = new ListViewItem();
					item.Checked = true;
					item = ArgumentsList.Items.Insert(ArgumentsList.Items.Count - 1, item);
				}

				IntPtr newHandle = SendMessage(ArgumentsList.Handle, LvmEditlabelw, new IntPtr(item.Index), IntPtr.Zero);
				SetWindowText(newHandle, e.KeyChar.ToString());
				SendMessage(newHandle, EmSetsel, new IntPtr(1), new IntPtr(1));

				e.Handled = true;
			}
		}

		private void MoveUpButton_Click(object sender, EventArgs e)
		{
			if (ArgumentsList.SelectedIndices.Count == 1)
			{
				int index = ArgumentsList.SelectedIndices[0];
				if (index > 0)
				{
					ListViewItem item = ArgumentsList.Items[index];
					ArgumentsList.Items.RemoveAt(index);
					ArgumentsList.Items.Insert(index - 1, item);
				}
			}
		}

		private void MoveDownButton_Click(object sender, EventArgs e)
		{
			if (ArgumentsList.SelectedIndices.Count == 1)
			{
				int index = ArgumentsList.SelectedIndices[0];
				if (index < ArgumentsList.Items.Count - 2)
				{
					ListViewItem item = ArgumentsList.Items[index];
					ArgumentsList.Items.RemoveAt(index);
					ArgumentsList.Items.Insert(index + 1, item);
				}
			}
		}

		private void ArgumentsList_SelectedIndexChanged(object sender, EventArgs e)
		{
			UpdateMoveButtons();
		}

		private void UpdateMoveButtons()
		{
			MoveUpButton.Enabled = (ArgumentsList.SelectedIndices.Count == 1 && ArgumentsList.SelectedIndices[0] > 0);
			MoveDownButton.Enabled = (ArgumentsList.SelectedIndices.Count == 1 && ArgumentsList.SelectedIndices[0] < ArgumentsList.Items.Count - 2);
		}
	}
}
