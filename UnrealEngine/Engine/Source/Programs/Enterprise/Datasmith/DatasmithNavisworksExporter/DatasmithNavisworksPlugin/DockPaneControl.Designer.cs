// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows.Forms;

namespace DatasmithNavisworks
{
	class DecimalInput : System.Windows.Forms.TextBox
	{
		public void SetValue(string TextValue)
		{
			Text = TextValue;
			ValidateValue();
		}

		protected override void OnLostFocus(System.EventArgs e)
		{
			ValidateValue();
			base.OnLostFocus(e);
		}

		// Validate value by keeping max substring that converts to double, formatting it
		private void ValidateValue()
		{
			double Value = 0.0;
			while ((Text.Length > 0) && !double.TryParse(Text, out Value))
			{
				Text = Text.Substring(0, Text.Length - 1);
			}

			Text = Value.ToString("0.000############");
		}
	};

	partial class DockPaneControl
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

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
            this.components = new System.ComponentModel.Container();
            this.MergeInput = new System.Windows.Forms.NumericUpDown();
            this.MergeGroupBox = new System.Windows.Forms.GroupBox();
            this.MergePanel = new System.Windows.Forms.Panel();
            this.MergeTableLayout = new System.Windows.Forms.TableLayoutPanel();
            this.MergeLabel = new System.Windows.Forms.Label();
            this.OriginPanel = new System.Windows.Forms.Panel();
            this.CoordsTableLayout = new System.Windows.Forms.TableLayoutPanel();
            this.YLabel = new System.Windows.Forms.Label();
            this.ZLabel = new System.Windows.Forms.Label();
            this.PickButton = new System.Windows.Forms.Button();
            this.XLabel = new System.Windows.Forms.Label();
            this.OriginBox = new System.Windows.Forms.GroupBox();
            this.Divisor = new System.Windows.Forms.Label();
            this.ExportButton = new System.Windows.Forms.Button();
            this.toolTip1 = new System.Windows.Forms.ToolTip(this.components);
            this.MetadataGroupBox = new System.Windows.Forms.GroupBox();
            this.MetadataCheckBox = new System.Windows.Forms.CheckBox();
            this.tableLayoutPanel1 = new System.Windows.Forms.TableLayoutPanel();
            this.XInput = new DatasmithNavisworks.DecimalInput();
            this.YInput = new DatasmithNavisworks.DecimalInput();
            this.ZInput = new DatasmithNavisworks.DecimalInput();
            ((System.ComponentModel.ISupportInitialize)(this.MergeInput)).BeginInit();
            this.MergeGroupBox.SuspendLayout();
            this.MergePanel.SuspendLayout();
            this.MergeTableLayout.SuspendLayout();
            this.OriginPanel.SuspendLayout();
            this.CoordsTableLayout.SuspendLayout();
            this.OriginBox.SuspendLayout();
            this.MetadataGroupBox.SuspendLayout();
            this.tableLayoutPanel1.SuspendLayout();
            this.SuspendLayout();
            // 
            // MergeInput
            // 
            this.MergeInput.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.MergeInput.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.MergeInput.Location = new System.Drawing.Point(184, 5);
            this.MergeInput.Maximum = new decimal(new int[] {
            9999,
            0,
            0,
            0});
            this.MergeInput.Name = "MergeInput";
            this.MergeInput.Size = new System.Drawing.Size(45, 20);
            this.MergeInput.TabIndex = 1;
            this.MergeInput.Value = new decimal(new int[] {
            99,
            0,
            0,
            0});
            // 
            // MergeGroupBox
            // 
            this.MergeGroupBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.MergeGroupBox.Controls.Add(this.MergePanel);
            this.MergeGroupBox.Location = new System.Drawing.Point(3, 3);
            this.MergeGroupBox.Name = "MergeGroupBox";
            this.MergeGroupBox.Size = new System.Drawing.Size(238, 57);
            this.MergeGroupBox.TabIndex = 2;
            this.MergeGroupBox.TabStop = false;
            this.MergeGroupBox.Text = "Merge";
            // 
            // MergePanel
            // 
            this.MergePanel.Controls.Add(this.MergeTableLayout);
            this.MergePanel.Dock = System.Windows.Forms.DockStyle.Top;
            this.MergePanel.Location = new System.Drawing.Point(3, 16);
            this.MergePanel.Name = "MergePanel";
            this.MergePanel.Size = new System.Drawing.Size(232, 30);
            this.MergePanel.TabIndex = 3;
            // 
            // MergeTableLayout
            // 
            this.MergeTableLayout.ColumnCount = 2;
            this.MergeTableLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.MergeTableLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
            this.MergeTableLayout.Controls.Add(this.MergeInput, 1, 0);
            this.MergeTableLayout.Controls.Add(this.MergeLabel, 0, 0);
            this.MergeTableLayout.Dock = System.Windows.Forms.DockStyle.Fill;
            this.MergeTableLayout.Location = new System.Drawing.Point(0, 0);
            this.MergeTableLayout.Name = "MergeTableLayout";
            this.MergeTableLayout.RowCount = 1;
            this.MergeTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.MergeTableLayout.Size = new System.Drawing.Size(232, 30);
            this.MergeTableLayout.TabIndex = 0;
            // 
            // MergeLabel
            // 
            this.MergeLabel.AutoSize = true;
            this.MergeLabel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.MergeLabel.Location = new System.Drawing.Point(3, 0);
            this.MergeLabel.Name = "MergeLabel";
            this.MergeLabel.Size = new System.Drawing.Size(175, 30);
            this.MergeLabel.TabIndex = 1;
            this.MergeLabel.Text = "Merge last Nth objects in hierarchy:";
            this.MergeLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // OriginPanel
            // 
            this.OriginPanel.Controls.Add(this.CoordsTableLayout);
            this.OriginPanel.Dock = System.Windows.Forms.DockStyle.Fill;
            this.OriginPanel.Location = new System.Drawing.Point(3, 16);
            this.OriginPanel.Name = "OriginPanel";
            this.OriginPanel.Size = new System.Drawing.Size(232, 123);
            this.OriginPanel.TabIndex = 2;
            // 
            // CoordsTableLayout
            // 
            this.CoordsTableLayout.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.CoordsTableLayout.ColumnCount = 2;
            this.CoordsTableLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Absolute, 40F));
            this.CoordsTableLayout.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.CoordsTableLayout.Controls.Add(this.YLabel, 0, 1);
            this.CoordsTableLayout.Controls.Add(this.ZLabel, 0, 2);
            this.CoordsTableLayout.Controls.Add(this.XInput, 1, 0);
            this.CoordsTableLayout.Controls.Add(this.YInput, 1, 1);
            this.CoordsTableLayout.Controls.Add(this.ZInput, 1, 2);
            this.CoordsTableLayout.Controls.Add(this.PickButton, 1, 3);
            this.CoordsTableLayout.Controls.Add(this.XLabel, 0, 0);
            this.CoordsTableLayout.GrowStyle = System.Windows.Forms.TableLayoutPanelGrowStyle.FixedSize;
            this.CoordsTableLayout.Location = new System.Drawing.Point(0, 0);
            this.CoordsTableLayout.Name = "CoordsTableLayout";
            this.CoordsTableLayout.RowCount = 4;
            this.CoordsTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
            this.CoordsTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
            this.CoordsTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
            this.CoordsTableLayout.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 25F));
            this.CoordsTableLayout.Size = new System.Drawing.Size(232, 110);
            this.CoordsTableLayout.TabIndex = 0;
            // 
            // YLabel
            // 
            this.YLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.YLabel.AutoSize = true;
            this.YLabel.Location = new System.Drawing.Point(3, 27);
            this.YLabel.Name = "YLabel";
            this.YLabel.Size = new System.Drawing.Size(34, 27);
            this.YLabel.TabIndex = 1;
            this.YLabel.Text = "Y";
            this.YLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // ZLabel
            // 
            this.ZLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.ZLabel.AutoSize = true;
            this.ZLabel.Location = new System.Drawing.Point(3, 54);
            this.ZLabel.Name = "ZLabel";
            this.ZLabel.Size = new System.Drawing.Size(34, 27);
            this.ZLabel.TabIndex = 2;
            this.ZLabel.Text = "Z";
            this.ZLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // PickButton
            // 
            this.PickButton.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left)));
            this.PickButton.Location = new System.Drawing.Point(43, 84);
            this.PickButton.Name = "PickButton";
            this.PickButton.Size = new System.Drawing.Size(60, 23);
            this.PickButton.TabIndex = 2;
            this.PickButton.Text = "Pick...";
            this.PickButton.UseVisualStyleBackColor = true;
            this.PickButton.Click += new System.EventHandler(this.PickButton_Click);
            // 
            // XLabel
            // 
            this.XLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.XLabel.AutoSize = true;
            this.XLabel.Location = new System.Drawing.Point(3, 0);
            this.XLabel.Name = "XLabel";
            this.XLabel.Size = new System.Drawing.Size(34, 27);
            this.XLabel.TabIndex = 0;
            this.XLabel.Text = "X";
            this.XLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // OriginBox
            // 
            this.OriginBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.OriginBox.Controls.Add(this.OriginPanel);
            this.OriginBox.Location = new System.Drawing.Point(3, 66);
            this.OriginBox.Name = "OriginBox";
            this.OriginBox.Size = new System.Drawing.Size(238, 142);
            this.OriginBox.TabIndex = 4;
            this.OriginBox.TabStop = false;
            this.OriginBox.Text = "Origin";
            // 
            // Divisor
            // 
            this.Divisor.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.Divisor.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.Divisor.Location = new System.Drawing.Point(3, 240);
            this.Divisor.Name = "Divisor";
            this.Divisor.Size = new System.Drawing.Size(238, 2);
            this.Divisor.TabIndex = 6;
            // 
            // ExportButton
            // 
            this.ExportButton.Anchor = System.Windows.Forms.AnchorStyles.None;
            this.ExportButton.Location = new System.Drawing.Point(36, 214);
            this.ExportButton.Name = "ExportButton";
            this.ExportButton.Size = new System.Drawing.Size(171, 23);
            this.ExportButton.TabIndex = 0;
            this.ExportButton.Text = "Export...";
            this.ExportButton.UseVisualStyleBackColor = true;
            this.ExportButton.Click += new System.EventHandler(this.ExportButton_Click);
            // 
            // MetadataGroupBox
            // 
            this.MetadataGroupBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.MetadataGroupBox.Controls.Add(this.MetadataCheckBox);
            this.MetadataGroupBox.Location = new System.Drawing.Point(3, 197);
            this.MetadataGroupBox.Name = "MetadataGroupBox";
            this.MetadataGroupBox.Size = new System.Drawing.Size(238, 42);
            this.MetadataGroupBox.TabIndex = 7;
            this.MetadataGroupBox.TabStop = false;
            this.MetadataGroupBox.Text = "Metadata";
            this.MetadataGroupBox.Visible = false;
            // 
            // MetadataCheckBox
            // 
            this.MetadataCheckBox.Anchor = System.Windows.Forms.AnchorStyles.Left;
            this.MetadataCheckBox.AutoSize = true;
            this.MetadataCheckBox.Checked = true;
            this.MetadataCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
            this.MetadataCheckBox.Location = new System.Drawing.Point(9, 19);
            this.MetadataCheckBox.Name = "MetadataCheckBox";
            this.MetadataCheckBox.Size = new System.Drawing.Size(104, 17);
            this.MetadataCheckBox.TabIndex = 0;
            this.MetadataCheckBox.Text = "Export Metadata";
            this.MetadataCheckBox.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
            this.MetadataCheckBox.UseVisualStyleBackColor = true;
            // 
            // tableLayoutPanel1
            // 
            this.tableLayoutPanel1.ColumnCount = 1;
            this.tableLayoutPanel1.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle(System.Windows.Forms.SizeType.Percent, 100F));
            this.tableLayoutPanel1.Controls.Add(this.Divisor, 0, 3);
            this.tableLayoutPanel1.Controls.Add(this.OriginBox, 0, 1);
            this.tableLayoutPanel1.Controls.Add(this.MergeGroupBox, 0, 0);
            this.tableLayoutPanel1.Controls.Add(this.ExportButton, 0, 2);
            this.tableLayoutPanel1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.tableLayoutPanel1.Location = new System.Drawing.Point(0, 0);
            this.tableLayoutPanel1.Name = "tableLayoutPanel1";
            this.tableLayoutPanel1.RowCount = 4;
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.RowStyles.Add(new System.Windows.Forms.RowStyle());
            this.tableLayoutPanel1.Size = new System.Drawing.Size(244, 244);
            this.tableLayoutPanel1.TabIndex = 8;
            // 
            // XInput
            // 
            this.XInput.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.XInput.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.XInput.Location = new System.Drawing.Point(43, 3);
            this.XInput.Name = "XInput";
            this.XInput.Size = new System.Drawing.Size(186, 20);
            this.XInput.TabIndex = 3;
            this.XInput.Text = "0.12345";
            // 
            // YInput
            // 
            this.YInput.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.YInput.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.YInput.Location = new System.Drawing.Point(43, 30);
            this.YInput.Name = "YInput";
            this.YInput.Size = new System.Drawing.Size(186, 20);
            this.YInput.TabIndex = 4;
            this.YInput.Text = "3.142";
            // 
            // ZInput
            // 
            this.ZInput.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Left | System.Windows.Forms.AnchorStyles.Right)));
            this.ZInput.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.ZInput.Location = new System.Drawing.Point(43, 57);
            this.ZInput.Name = "ZInput";
            this.ZInput.Size = new System.Drawing.Size(186, 20);
            this.ZInput.TabIndex = 5;
            this.ZInput.Text = "30000000.0";
            // 
            // DockPaneControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.Controls.Add(this.tableLayoutPanel1);
            this.Controls.Add(this.MetadataGroupBox);
            this.MinimumSize = new System.Drawing.Size(148, 244);
            this.Name = "DockPaneControl";
            this.Size = new System.Drawing.Size(244, 244);
            this.Load += new System.EventHandler(this.DockPaneControl_Load);
            ((System.ComponentModel.ISupportInitialize)(this.MergeInput)).EndInit();
            this.MergeGroupBox.ResumeLayout(false);
            this.MergePanel.ResumeLayout(false);
            this.MergeTableLayout.ResumeLayout(false);
            this.MergeTableLayout.PerformLayout();
            this.OriginPanel.ResumeLayout(false);
            this.CoordsTableLayout.ResumeLayout(false);
            this.CoordsTableLayout.PerformLayout();
            this.OriginBox.ResumeLayout(false);
            this.MetadataGroupBox.ResumeLayout(false);
            this.MetadataGroupBox.PerformLayout();
            this.tableLayoutPanel1.ResumeLayout(false);
            this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.NumericUpDown MergeInput;
		private System.Windows.Forms.Label MergeLabel;
		private System.Windows.Forms.GroupBox MergeGroupBox;
		private System.Windows.Forms.Panel OriginPanel;
		private System.Windows.Forms.Panel MergePanel;
		private System.Windows.Forms.TableLayoutPanel MergeTableLayout;
		private System.Windows.Forms.Button PickButton;
		private System.Windows.Forms.TableLayoutPanel CoordsTableLayout;
		private System.Windows.Forms.Label ZLabel;
		private System.Windows.Forms.Label YLabel;
		private System.Windows.Forms.Label XLabel;
		private System.Windows.Forms.GroupBox OriginBox;
		private System.Windows.Forms.Label Divisor;
		private System.Windows.Forms.Button ExportButton;
		private DecimalInput XInput;
		private DecimalInput ZInput;
		private DecimalInput YInput;
		private ToolTip toolTip1;
		private GroupBox MetadataGroupBox;
		private CheckBox MetadataCheckBox;
		private TableLayoutPanel tableLayoutPanel1;
	}
}
