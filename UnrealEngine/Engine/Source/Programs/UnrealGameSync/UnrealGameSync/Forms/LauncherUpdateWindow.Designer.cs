namespace UnrealGameSync.Forms
{
	partial class LauncherUpdateWindow
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
            this.label1 = new System.Windows.Forms.Label();
            this.UpdateNowBtn = new System.Windows.Forms.Button();
            this.LaterBtn = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.label1.Location = new System.Drawing.Point(18, 17);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(531, 38);
            this.label1.TabIndex = 0;
            this.label1.Text = "A newer version of the UnrealGameSync Launcher is available. Installation will ta" +
    "ke less than a minute. Would you like to update now?";
            // 
            // UpdateNowBtn
            // 
            this.UpdateNowBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.UpdateNowBtn.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.UpdateNowBtn.Location = new System.Drawing.Point(324, 66);
            this.UpdateNowBtn.Name = "UpdateNowBtn";
            this.UpdateNowBtn.Size = new System.Drawing.Size(110, 29);
            this.UpdateNowBtn.TabIndex = 1;
            this.UpdateNowBtn.Text = "Update Now";
            this.UpdateNowBtn.UseVisualStyleBackColor = true;
            // 
            // LaterBtn
            // 
            this.LaterBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.LaterBtn.DialogResult = System.Windows.Forms.DialogResult.Ignore;
            this.LaterBtn.Location = new System.Drawing.Point(439, 66);
            this.LaterBtn.Name = "LaterBtn";
            this.LaterBtn.Size = new System.Drawing.Size(110, 29);
            this.LaterBtn.TabIndex = 2;
            this.LaterBtn.Text = "Later";
            this.LaterBtn.UseVisualStyleBackColor = true;
            // 
            // LauncherUpdateWindow
            // 
            this.AcceptButton = this.UpdateNowBtn;
            this.AutoScaleDimensions = new System.Drawing.SizeF(96F, 96F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Dpi;
            this.CancelButton = this.LaterBtn;
            this.ClientSize = new System.Drawing.Size(561, 104);
            this.ControlBox = false;
            this.Controls.Add(this.LaterBtn);
            this.Controls.Add(this.UpdateNowBtn);
            this.Controls.Add(this.label1);
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "LauncherUpdateWindow";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Update Available";
            this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Button UpdateNowBtn;
		private System.Windows.Forms.Button LaterBtn;
	}
}