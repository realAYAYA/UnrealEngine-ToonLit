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
    public partial class SelectNamespace : Form
    {
        public SelectNamespace()
        {
            InitializeComponent();
        }

        public void SetName(String className)
        {
            Name = "Select namespace for " + className; 
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
    }
}
