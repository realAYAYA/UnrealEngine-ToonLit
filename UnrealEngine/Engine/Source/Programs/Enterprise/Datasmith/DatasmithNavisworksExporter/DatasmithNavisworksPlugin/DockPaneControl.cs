// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Windows.Forms;

namespace DatasmithNavisworks
{
	public partial class DockPaneControl : UserControl
	{
		public interface IPresenter
		{
			void OnLoaded();
			void OnExportClicked();
			void OnPickOriginClicked();
		}

		public IPresenter Presenter;

		public string OriginXText
		{
			get => XInput.Text;
			set => XInput.SetValue(value);
		}

		public string OriginYText
		{
			get => YInput.Text;
			set => YInput.SetValue(value);
		}

		public string OriginZText
		{
			get => ZInput.Text;
			set => ZInput.SetValue(value);
		}

		public decimal MergeValue
		{
			get => MergeInput.Value;
			set => MergeInput.Value = value;

		}

		public bool IncludeMetadata
		{
			get => MetadataCheckBox.Checked;
			set => MetadataCheckBox.Checked = value;

		}

		public DockPaneControl()
		{
			InitializeComponent();
		}

		private void DockPaneControl_Load(object Sender, EventArgs EventArgs)
		{
			const string MergeTooltip = "Will merge scene subtree into single mesh if it's not deeper than this value";
			toolTip1.SetToolTip(MergeGroupBox, MergeTooltip);
			toolTip1.SetToolTip(MergeLabel, MergeTooltip);
			toolTip1.SetToolTip(MergeInput, MergeTooltip);

			const string OriginTooltip = "Origin will be center of coordinates(0, 0, 0) in the exported scene";
			toolTip1.SetToolTip(OriginBox, OriginTooltip);
			toolTip1.SetToolTip(XLabel, OriginTooltip);
			toolTip1.SetToolTip(YLabel, OriginTooltip);
			toolTip1.SetToolTip(ZLabel, OriginTooltip);
			toolTip1.SetToolTip(XInput, OriginTooltip);
			toolTip1.SetToolTip(YInput, OriginTooltip);
			toolTip1.SetToolTip(ZInput, OriginTooltip);

			const string PickOriginTooltip = "Pick Origin from scene object vertex";
			toolTip1.SetToolTip(PickButton, PickOriginTooltip);

			const string MetadataTooltip = "Include metadata export(might affect export time)";
			toolTip1.SetToolTip(MetadataCheckBox, MetadataTooltip);

			Presenter.OnLoaded();
		}

		private void ExportButton_Click(object Sender, EventArgs EventArgs)
		{
			Presenter.OnExportClicked();
		}

		private void PickButton_Click(object Sender, EventArgs EventArgs)
		{
			Presenter.OnPickOriginClicked();
		}

	}
}
