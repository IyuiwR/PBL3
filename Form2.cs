using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Text.RegularExpressions;
using System.Drawing.Imaging;
using Microsoft.Office.Interop.Excel;
using Excel = Microsoft.Office.Interop.Excel;
using DataTable = System.Data.DataTable;
using static System.Windows.Forms.VisualStyles.VisualStyleElement;
using System.IO;
using System.Runtime.InteropServices;

namespace PBL3_final
{
    public partial class Form2 : Form
    {
        SerialPort serialPort;
        int lastSavedRowIndex = 0; 
        int warningCount = 0;  
        public Form2()
        {
            InitializeComponent();
            string[] ports = SerialPort.GetPortNames();
            comboBoxCOM.Items.AddRange(ports);
            if (!dataGridView1.Columns.Contains("ThoiGian"))
            {
                dataGridView1.Columns.Add("ThoiGian", "Thời gian");
            }
        }
        private Timer autoSaveTimer;
        private void Form2_Load(object sender, EventArgs e)
        {
            autoSaveTimer = new Timer();
            autoSaveTimer.Interval = 10000; 
            autoSaveTimer.Tick += AutoSaveTimer_Tick;
            autoSaveTimer.Start();
        }

        private void AutoSaveTimer_Tick(object sender, EventArgs e)
        {
            string filePath = @"D:\Dulieu.xlsx";
            Excel.Application excelApp = new Excel.Application();
            Excel.Workbook workbook = null;
            Excel.Worksheet worksheet = null;

            try
            {
                if (System.IO.File.Exists(filePath))
                {
                    workbook = excelApp.Workbooks.Open(filePath);
                }
                else
                {
                    workbook = excelApp.Workbooks.Add();
                    workbook.SaveAs(filePath);
                }

    
                bool sheetFound = false;
                foreach (Excel.Worksheet sheet in workbook.Sheets)
                {
                    if (sheet.Name == "AutoData")
                    {
                        worksheet = sheet;
                        sheetFound = true;
                        break;
                    }
                }

                if (!sheetFound)
                {
                    worksheet = workbook.Sheets.Add();
                    worksheet.Name = "AutoData";
                    worksheet.Cells[1, 1] = "ID";
                    worksheet.Cells[1, 2] = "Họ tên";
                    worksheet.Cells[1, 3] = "Địa chỉ";
                    worksheet.Cells[1, 4] = "SĐT";
                    worksheet.Cells[1, 5] = "CCCD";
                    worksheet.Cells[1, 6] = "Nhịp tim";
                    worksheet.Cells[1, 7] = "Nồng độ cồn";
                    worksheet.Cells[1, 8] = "Khoảng cách";
                    worksheet.Cells[1, 9] = "Thời gian";
                }

                int startRow = worksheet.Cells[worksheet.Rows.Count, 1].End[Excel.XlDirection.xlUp].Row + 1;

          
                for (int i = lastSavedRowIndex; i < dataGridView1.Rows.Count; i++)
                {
                    if (dataGridView1.Rows[i].IsNewRow) continue; 

                    worksheet.Cells[startRow, 1] = dataGridView1.Rows[i].Cells[0].Value?.ToString();
                worksheet.Cells[startRow, 2] = dataGridView1.Rows[i].Cells[1].Value?.ToString();
                worksheet.Cells[startRow, 3] = dataGridView1.Rows[i].Cells[2].Value?.ToString();
                worksheet.Cells[startRow, 4] = dataGridView1.Rows[i].Cells[3].Value?.ToString();
                worksheet.Cells[startRow, 5] = dataGridView1.Rows[i].Cells[4].Value?.ToString();
                worksheet.Cells[startRow, 6] = dataGridView1.Rows[i].Cells[5].Value?.ToString();
                worksheet.Cells[startRow, 7] = dataGridView1.Rows[i].Cells[6].Value?.ToString();
                worksheet.Cells[startRow, 8] = dataGridView1.Rows[i].Cells[7].Value?.ToString();
                worksheet.Cells[startRow, 9] = DateTime.Now.ToString("HH:mm:ss dd/MM/yyyy");

                    startRow++;
                }

                lastSavedRowIndex = dataGridView1.Rows.Count;

                workbook.Save();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Lỗi ghi Excel: " + ex.Message);
            }
            finally
            {
                if (workbook != null)
                {
                    workbook.Close(false);
                    System.Runtime.InteropServices.Marshal.ReleaseComObject(workbook);
                }

                if (excelApp != null)
                {
                    excelApp.Quit();
                    System.Runtime.InteropServices.Marshal.ReleaseComObject(excelApp);
                }

                worksheet = null;
                workbook = null;
                excelApp = null;

                GC.Collect();
                GC.WaitForPendingFinalizers();
            }
        }

        private string GetDataValue(string input, string keyword, char stopChar)
        {
            if (string.IsNullOrEmpty(input) || string.IsNullOrEmpty(keyword))
                return string.Empty;

            int keywordIndex = input.IndexOf(keyword);

            if (keywordIndex == -1)
                return string.Empty; 

            int startIndex = keywordIndex + keyword.Length;
            if (startIndex >= input.Length)
                return string.Empty;

            int endIndex = input.IndexOf(stopChar, startIndex);

            if (endIndex != -1)
            {
                return input.Substring(startIndex, endIndex - startIndex);
            }
            else
            {
                return input.Substring(startIndex); 
            }
        }
        private string GetDataValueToEnd(string input, string keyword)
        {
            if (string.IsNullOrEmpty(input) || string.IsNullOrEmpty(keyword))
                return string.Empty;

            int keywordIndex = input.IndexOf(keyword);

            if (keywordIndex == -1)
                return string.Empty; 

            int startIndex = keywordIndex + keyword.Length;

            if (startIndex >= input.Length)
                return string.Empty; 

            return input.Substring(startIndex);
        }
        private void button1_Click(object sender, EventArgs e)
        {
            serialPort = new SerialPort(comboBoxCOM.Text, 9600);
            serialPort.DataReceived += SerialPort_DataReceived;
            serialPort.Open();
        }
        private void SerialPort_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            string line = serialPort.ReadLine();

            this.Invoke(new System.Action(() =>
            {
                string nhipTim = GetDataValueToEnd(line, "P:");
                string nongDoCon = GetDataValue(line, "MQ3:", ',');
                string khoangCach = GetDataValue(line, "Dist:", ' ');

                if (line.Contains("Warn:"))
                {
                    warningCount++;
                    MessageBox.Show($"⚠️ Cảnh báo vi phạm! Tổng số lần: {warningCount}", "Cảnh báo", MessageBoxButtons.OK, MessageBoxIcon.Warning);

                    labelWarningCount.Text = $"Số lần vi phạm: {warningCount}";
                }

                if (int.TryParse(nhipTim, out int pulseValue) && pulseValue >= 50)
                {
                    textBox1.Text = nhipTim;
                    textBox2.Text = nongDoCon;
                    textBox3.Text = khoangCach;
                }
                else
                {
                    textBox1.Text = "0";
                    textBox2.Text = nongDoCon;
                    textBox3.Text = khoangCach;
                }
                if (int.TryParse(nhipTim, out pulseValue) && pulseValue >= 50)
                {
                    dataGridView1.Rows.Add(

                             "",
                             "",
                            "",
                            "",
                             "",
                             nhipTim,
                             nongDoCon,
                             khoangCach
                        );
                }
                else
                {
                    dataGridView1.Rows.Add(

                             "",
                             "",
                            "",
                            "",
                             "",
                             "0",
                             nongDoCon,
                             khoangCach
                        );
                }
            }));
        }

        private void btnXoa_Click(object sender, EventArgs e)
        {
            if (dataGridView1.SelectedRows.Count > 0)
            {
                dataGridView1.Rows.RemoveAt(dataGridView1.SelectedRows[0].Index);
            }
        }

        private void btnTimKiem_Click(object sender, EventArgs e)
        {
            string filePath = @"D:\CODE\ex.xlsx";

            if (!File.Exists(filePath))
            {
                MessageBox.Show("Không tìm thấy tệp Excel.");
                return;
            }

            // Mở Excel
            Excel.Application excelApp = new Excel.Application();
            Excel.Workbook workbook = excelApp.Workbooks.Open(filePath);
            Excel.Worksheet worksheet = workbook.Sheets[1];

            Excel.Range range = worksheet.UsedRange;
            int rowCount = range.Rows.Count;

            bool found = false;

            dataGridView1.Rows.Clear();

            for (int i = 2; i <= rowCount; i++) 
            {
                string id = ((Excel.Range)range.Cells[i, 1]).Text;
                string name = ((Excel.Range)range.Cells[i, 2]).Text;

                if (id == comboBoxID.Text || name.ToLower().Contains(txtHoTen.Text.ToLower()))
                {
                    string diaChi = ((Excel.Range)range.Cells[i, 3]).Text;
                    string sdt = ((Excel.Range)range.Cells[i, 4]).Text;
                    string cccd = ((Excel.Range)range.Cells[i, 5]).Text;

                    dataGridView1.Rows.Add(id, name, diaChi, sdt, cccd);

                    found = true;
                }
            }

            // Đóng Excel
            workbook.Close(false);
            excelApp.Quit();
            Marshal.ReleaseComObject(worksheet);
            Marshal.ReleaseComObject(workbook);
            Marshal.ReleaseComObject(excelApp);

            if (!found)
            {
                MessageBox.Show("Không tìm thấy người dùng.");
            }
            else
            {
                AutoSaveTimer_Tick(null, null);
            }
        }
        private void ExportToExcel(DataGridView dgv)
        {
            if (dgv.Rows.Count == 0)
            {
                MessageBox.Show("Không có dữ liệu để xuất!");
                return;
            }

            Excel.Application excelApp = new Excel.Application();
            Excel.Workbook workbook = excelApp.Workbooks.Add(Type.Missing);
            Excel.Worksheet worksheet = workbook.ActiveSheet;
            worksheet.Name = "DriverSupportData";


            for (int i = 1; i < dgv.Columns.Count + 1; i++)
            {
                worksheet.Cells[1, i] = dgv.Columns[i - 1].HeaderText;
            }


            for (int i = 0; i < dgv.Rows.Count; i++)
            {
                for (int j = 0; j < dgv.Columns.Count; j++)
                {
                    worksheet.Cells[i + 2, j + 1] = dgv.Rows[i].Cells[j].Value?.ToString();
                }
            }


            excelApp.Visible = true;
        }

        private void button2_Click(object sender, EventArgs e)
        {
            serialPort.Close();
        }

        private void button3_Click(object sender, EventArgs e)
        {
            if (serialPort.IsOpen)
            {
                serialPort.Write("F");
            }
            else
            {
                MessageBox.Show("Cổng COM chưa mở!");
            }
        }

        private void button4_Click(object sender, EventArgs e)
        {
            if (serialPort.IsOpen)
            {
                serialPort.Write("H");
            }
            else
            {
                MessageBox.Show("Cổng COM chưa mở!");
            }
        }

        private void button5_Click(object sender, EventArgs e)
        {
            if (serialPort.IsOpen)
            {
                serialPort.Write("L");
            }
            else
            {
                MessageBox.Show("Cổng COM chưa mở!");
            }
        }

        private void button6_Click(object sender, EventArgs e)
        {
            if (serialPort.IsOpen)
            {
                serialPort.Write("S");
            }
            else
            {
                MessageBox.Show("Cổng COM chưa mở!");
            }
        }

        private void button7_Click(object sender, EventArgs e)
        {
            ExportToExcel(dataGridView1);

        }
    }
}
