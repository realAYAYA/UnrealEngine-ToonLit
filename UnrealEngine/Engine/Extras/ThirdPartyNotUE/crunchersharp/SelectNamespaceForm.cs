using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace CruncherSharp
{
    public partial class SelectNamespaceForm : Form
    {
        public bool SkipAllNamespaces { get; set; }

        public SelectNamespaceForm()
        {
            InitializeComponent();
            SkipAllNamespaces = false;
        }

        public void SetName(String className, ulong count)
        {
            this.labelSelectNamespace.Text = "Select namespace for " + className + " (" + count + " instances)"; 
        }

        public string GetNamespace()
        {
            if (checkedListBoxNamespaces.CheckedItems.Count == 0)
                return "";
            return checkedListBoxNamespaces.CheckedItems[0].ToString();
        }

        public void AddNamespaces(List<String> namespaces)
        {
            foreach (var name in namespaces)
                checkedListBoxNamespaces.Items.Add(name);
        }

        private void checkedListBoxNamespaces_ItemCheck(object sender, ItemCheckEventArgs e)
        {
            Close();
        }

        private void buttonSkip_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void buttonSkipAll_Click(object sender, EventArgs e)
        {
            SkipAllNamespaces = true;
            Close();
        }
    }
}
