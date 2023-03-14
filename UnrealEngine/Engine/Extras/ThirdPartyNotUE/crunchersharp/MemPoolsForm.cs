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
    public partial class AddMemPoolsForm : Form
    {

        public AddMemPoolsForm()
        {
            InitializeComponent();
        }

		private void buttonOK_Click(object sender, EventArgs e)
		{
			Close();
		}
		public void SetMemPools(List<uint> memPools)
		{
			foreach (var memPool in memPools)
			{
				memPoolsTextBox.Text += memPool;
				memPoolsTextBox.Text += ", ";
			}
		}

		public List<uint> GetMemPool()
		{
			string[] memPools = memPoolsTextBox.Text.Split(',');
			List<uint> memPoolsList = new List<uint>();
			foreach (var memPool in memPools)
			{
				try
				{
					memPoolsList.Add(UInt32.Parse(memPool));
				}

				catch (OverflowException)
				{
				}

				catch (FormatException)
				{
					if (memPool.Contains('-'))
					{
						string[] difference = memPool.Split('-');
						uint value = UInt32.Parse(difference[0]) - UInt32.Parse(difference[1]);
						memPoolsList.Add(value);
					}
					if (memPool.Contains('+'))
					{
						string[] difference = memPool.Split('+');
						uint value = UInt32.Parse(difference[0]) + UInt32.Parse(difference[1]);
						memPoolsList.Add(value);
					}
				}
			}
			return memPoolsList;
		}
	}
}
