// Copyright Epic Games, Inc. All Rights Reserved.

using System.Drawing;
using System.Windows.Forms;

namespace Horde.Agent.TrayApp.Forms
{
	partial class IdleForm
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			nameColumnHeader = new ColumnHeader();
			statsListView = new ListView();
			valueColumnHeader = new ColumnHeader();
			minValueColumnHeader = new ColumnHeader();
			resultColumnHeader = new ColumnHeader();
			stateTextBox = new TextBox();
			tableLayoutPanel1 = new TableLayoutPanel();
			tableLayoutPanel1.SuspendLayout();
			SuspendLayout();
			// 
			// nameColumnHeader
			// 
			nameColumnHeader.Text = "Name";
			nameColumnHeader.Width = 140;
			// 
			// statsListView
			// 
			statsListView.Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right;
			statsListView.Columns.AddRange(new ColumnHeader[] { nameColumnHeader, valueColumnHeader, minValueColumnHeader, resultColumnHeader });
			statsListView.FullRowSelect = true;
			statsListView.GridLines = true;
			statsListView.Location = new Point(20, 79);
			statsListView.Margin = new Padding(20, 0, 20, 20);
			statsListView.Name = "statsListView";
			statsListView.Size = new Size(806, 283);
			statsListView.TabIndex = 0;
			statsListView.UseCompatibleStateImageBehavior = false;
			statsListView.View = View.Details;
			// 
			// valueColumnHeader
			// 
			valueColumnHeader.Text = "Value";
			valueColumnHeader.TextAlign = HorizontalAlignment.Center;
			valueColumnHeader.Width = 80;
			// 
			// minValueColumnHeader
			// 
			minValueColumnHeader.Text = "MinValue";
			minValueColumnHeader.TextAlign = HorizontalAlignment.Center;
			minValueColumnHeader.Width = 80;
			// 
			// resultColumnHeader
			// 
			resultColumnHeader.Text = "Result";
			resultColumnHeader.TextAlign = HorizontalAlignment.Center;
			resultColumnHeader.Width = 80;
			// 
			// stateTextBox
			// 
			stateTextBox.BorderStyle = BorderStyle.FixedSingle;
			stateTextBox.Dock = DockStyle.Fill;
			stateTextBox.Location = new Point(20, 20);
			stateTextBox.Margin = new Padding(20);
			stateTextBox.Name = "stateTextBox";
			stateTextBox.ReadOnly = true;
			stateTextBox.Size = new Size(806, 39);
			stateTextBox.TabIndex = 1;
			// 
			// tableLayoutPanel1
			// 
			tableLayoutPanel1.ColumnCount = 1;
			tableLayoutPanel1.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100F));
			tableLayoutPanel1.Controls.Add(statsListView, 0, 1);
			tableLayoutPanel1.Controls.Add(stateTextBox, 0, 0);
			tableLayoutPanel1.Dock = DockStyle.Fill;
			tableLayoutPanel1.Location = new Point(0, 0);
			tableLayoutPanel1.Name = "tableLayoutPanel1";
			tableLayoutPanel1.RowCount = 2;
			tableLayoutPanel1.RowStyles.Add(new RowStyle());
			tableLayoutPanel1.RowStyles.Add(new RowStyle(SizeType.Percent, 100F));
			tableLayoutPanel1.Size = new Size(846, 382);
			tableLayoutPanel1.TabIndex = 2;
			// 
			// IdleForm
			// 
			AutoScaleDimensions = new SizeF(192F, 192F);
			AutoScaleMode = AutoScaleMode.Dpi;
			ClientSize = new Size(846, 382);
			Controls.Add(tableLayoutPanel1);
			FormBorderStyle = FormBorderStyle.FixedDialog;
			Margin = new Padding(4, 2, 4, 2);
			Name = "IdleForm";
			ShowIcon = false;
			Text = "Idle Stats";
			TopMost = true;
			tableLayoutPanel1.ResumeLayout(false);
			tableLayoutPanel1.PerformLayout();
			ResumeLayout(false);
		}

		#endregion

		private ListView statsListView;
		private ColumnHeader valueColumnHeader;
		private ColumnHeader minValueColumnHeader;
		private ColumnHeader resultColumnHeader;
		private ColumnHeader nameColumnHeader;
		private TextBox stateTextBox;
		private TableLayoutPanel tableLayoutPanel1;
	}
}