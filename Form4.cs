using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using OfficeOpenXml;
using Microsoft.Office.Interop.Excel;
using Excel = Microsoft.Office.Interop.Excel;
using DataTable = System.Data.DataTable;

namespace PBL3_final
{
    public partial class Form4 : Form
    {
        public Form4()
        {
            InitializeComponent();
            ExcelPackage.License.SetNonCommercialPersonal("Minh");
            this.Load += Form4_Load;
        }
        private void Form4_Load(object sender, EventArgs e)
        {
            string excelFilePath = @"D:\Code\ex.xlsx"; // đường dẫn đến file Excel
            LoadExcelToGrid(excelFilePath);
        }

        private void LoadExcelToGrid(string filePath)
        {
            var excelApp = new Excel.Application();
            var workbook = excelApp.Workbooks.Open(filePath);
            var worksheet = (Excel.Worksheet)workbook.Sheets[1];
            var range = worksheet.UsedRange;

            int rowCount = range.Rows.Count;
            int colCount = range.Columns.Count;

            dataGridView1.ColumnCount = colCount;

            for (int i = 1; i <= rowCount; i++)
            {
                object[] row = new object[colCount];
                for (int j = 1; j <= colCount; j++)
                {
                    row[j - 1] = (range.Cells[i, j] as Excel.Range).Value2;
                }
                dataGridView1.Rows.Add(row);
            }

            workbook.Close(false);
            excelApp.Quit();
        }
        private void button1_Click(object sender, EventArgs e)
        {
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Title = "Import Excel";
            openFileDialog.Filter = "Excel (*.xlsx)|*.xlsx|Excel 2003 (*.xls)|*.xls";

            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                try
                {
                    ImportExcel(openFileDialog.FileName);
                    MessageBox.Show("Nhập file thành công!");
                }
                catch (Exception ex)
                {
                    MessageBox.Show("Nhập file không thành công!\n" + ex.Message);
                }
            }
        }
        private void ImportExcel(string path)
        {
            using (ExcelPackage excelPackage = new ExcelPackage(new FileInfo(path)))
            {
                ExcelWorksheet excelWorksheet = excelPackage.Workbook.Worksheets[0];
                DataTable dataTable = new DataTable();

                for (int i = excelWorksheet.Dimension.Start.Column; i <= excelWorksheet.Dimension.End.Column; i++)
                {
                    dataTable.Columns.Add(excelWorksheet.Cells[1, i].Value.ToString());
                }

                for (int i = excelWorksheet.Dimension.Start.Row + 1; i <= excelWorksheet.Dimension.End.Row; i++)
                {
                    List<string> listRows = new List<string>();
                    for (int j = excelWorksheet.Dimension.Start.Column; j <= excelWorksheet.Dimension.End.Column; j++)
                    {
                        listRows.Add(excelWorksheet.Cells[i, j].Value.ToString());
                    }
                    dataTable.Rows.Add(listRows.ToArray());
                }
                dataGridView1.DataSource = dataTable;
            }
        }
    }
}
