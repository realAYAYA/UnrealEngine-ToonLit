namespace UnrealGameSync.Controls
{
	partial class OidcControl
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
			this.OIDCControlGroupBox = new System.Windows.Forms.GroupBox();
			this.StatusPanel = new System.Windows.Forms.Panel();
			this.LoginButton = new System.Windows.Forms.Button();
			this.OIDCControlGroupBox.SuspendLayout();
			this.SuspendLayout();
			// 
			// OIDCControlGroupBox
			// 
			this.OIDCControlGroupBox.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.OIDCControlGroupBox.Controls.Add(this.StatusPanel);
			this.OIDCControlGroupBox.Controls.Add(this.LoginButton);
			this.OIDCControlGroupBox.Location = new System.Drawing.Point(0, 0);
			this.OIDCControlGroupBox.Name = "OIDCControlGroupBox";
			this.OIDCControlGroupBox.Size = new System.Drawing.Size(345, 86);
			this.OIDCControlGroupBox.TabIndex = 0;
			this.OIDCControlGroupBox.TabStop = false;
			// 
			// StatusPanel
			// 
			this.StatusPanel.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.StatusPanel.Location = new System.Drawing.Point(20, 34);
			this.StatusPanel.Name = "StatusPanel";
			this.StatusPanel.Size = new System.Drawing.Size(144, 29);
			this.StatusPanel.TabIndex = 2;
			this.StatusPanel.Paint += new System.Windows.Forms.PaintEventHandler(this.StatusPanel_Paint);
			// 
			// LoginButton
			// 
			this.LoginButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.LoginButton.Location = new System.Drawing.Point(227, 34);
			this.LoginButton.Name = "LoginButton";
			this.LoginButton.Size = new System.Drawing.Size(112, 34);
			this.LoginButton.TabIndex = 0;
			this.LoginButton.Text = "Login...";
			this.LoginButton.UseVisualStyleBackColor = true;
			this.LoginButton.Click += new System.EventHandler(this.LoginButton_Click);
			// 
			// OIDCControl
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(10F, 25F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.OIDCControlGroupBox);
			this.Name = "OidcControl";
			this.Size = new System.Drawing.Size(358, 101);
			this.OIDCControlGroupBox.ResumeLayout(false);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.GroupBox OIDCControlGroupBox;
		private System.Windows.Forms.Button LoginButton;
		private System.Windows.Forms.Panel StatusPanel;
	}
}
