namespace CruncherSharp
{
    partial class SelectNamespaceForm
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
            this.checkedListBoxNamespaces = new System.Windows.Forms.CheckedListBox();
            this.buttonSkip = new System.Windows.Forms.Button();
            this.buttonSkipAll = new System.Windows.Forms.Button();
            this.labelSelectNamespace = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // checkedListBoxNamespaces
            // 
            this.checkedListBoxNamespaces.FormattingEnabled = true;
            this.checkedListBoxNamespaces.Location = new System.Drawing.Point(11, 44);
            this.checkedListBoxNamespaces.Name = "checkedListBoxNamespaces";
            this.checkedListBoxNamespaces.Size = new System.Drawing.Size(430, 229);
            this.checkedListBoxNamespaces.TabIndex = 0;
            this.checkedListBoxNamespaces.ItemCheck += new System.Windows.Forms.ItemCheckEventHandler(this.checkedListBoxNamespaces_ItemCheck);
            // 
            // buttonSkip
            // 
            this.buttonSkip.Location = new System.Drawing.Point(285, 300);
            this.buttonSkip.Name = "buttonSkip";
            this.buttonSkip.Size = new System.Drawing.Size(75, 23);
            this.buttonSkip.TabIndex = 1;
            this.buttonSkip.Text = "Skip";
            this.buttonSkip.UseVisualStyleBackColor = true;
            this.buttonSkip.Click += new System.EventHandler(this.buttonSkip_Click);
            // 
            // buttonSkipAll
            // 
            this.buttonSkipAll.Location = new System.Drawing.Point(366, 300);
            this.buttonSkipAll.Name = "buttonSkipAll";
            this.buttonSkipAll.Size = new System.Drawing.Size(75, 23);
            this.buttonSkipAll.TabIndex = 2;
            this.buttonSkipAll.Text = "Skip all";
            this.buttonSkipAll.UseVisualStyleBackColor = true;
            this.buttonSkipAll.Click += new System.EventHandler(this.buttonSkipAll_Click);
            // 
            // labelSelectNamespace
            // 
            this.labelSelectNamespace.AutoSize = true;
            this.labelSelectNamespace.Location = new System.Drawing.Point(13, 13);
            this.labelSelectNamespace.Name = "labelSelectNamespace";
            this.labelSelectNamespace.Size = new System.Drawing.Size(95, 13);
            this.labelSelectNamespace.TabIndex = 3;
            this.labelSelectNamespace.Text = "Select namespace";
            // 
            // SelectNamespaceForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoSize = true;
            this.ClientSize = new System.Drawing.Size(453, 334);
            this.Controls.Add(this.labelSelectNamespace);
            this.Controls.Add(this.buttonSkipAll);
            this.Controls.Add(this.buttonSkip);
            this.Controls.Add(this.checkedListBoxNamespaces);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Name = "SelectNamespaceForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Resolve namespace";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.CheckedListBox checkedListBoxNamespaces;
        private System.Windows.Forms.Button buttonSkip;
        private System.Windows.Forms.Button buttonSkipAll;
        private System.Windows.Forms.Label labelSelectNamespace;
    }
}