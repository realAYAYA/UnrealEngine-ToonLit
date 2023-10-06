namespace CruncherSharp
{
    partial class SelectNamespace
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
            this.buttonOK = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // checkedListBoxNamespaces
            // 
            this.checkedListBoxNamespaces.FormattingEnabled = true;
            this.checkedListBoxNamespaces.Location = new System.Drawing.Point(-1, 0);
            this.checkedListBoxNamespaces.Name = "checkedListBoxNamespaces";
            this.checkedListBoxNamespaces.Size = new System.Drawing.Size(344, 274);
            this.checkedListBoxNamespaces.TabIndex = 0;
            // 
            // buttonOK
            // 
            this.buttonOK.Location = new System.Drawing.Point(230, 292);
            this.buttonOK.Name = "buttonOK";
            this.buttonOK.Size = new System.Drawing.Size(75, 23);
            this.buttonOK.TabIndex = 1;
            this.buttonOK.Text = "OK";
            this.buttonOK.UseVisualStyleBackColor = true;
            // 
            // SelectNamespace
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(346, 327);
            this.Controls.Add(this.buttonOK);
            this.Controls.Add(this.checkedListBoxNamespaces);
            this.Name = "SelectNamespace";
            this.Text = "Select namespace";
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.CheckedListBox checkedListBoxNamespaces;
        private System.Windows.Forms.Button buttonOK;
    }
}