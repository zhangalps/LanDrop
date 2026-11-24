using System;
using System.Diagnostics;
using System.Drawing;
using System.Threading;
using System.Windows.Forms;

namespace LanDropSetup
{
    internal sealed class InstallerForm : Form
    {
        private readonly TextBox _pathBox;
        private readonly Button _browseBtn;
        private readonly CheckBox _chkDesktop;
        private readonly CheckBox _chkStartMenu;
        private readonly CheckBox _chkLaunch;
        private readonly Label _statusLabel;
        private readonly ProgressBar _progressBar;
        private readonly Button _installBtn;
        private readonly Button _cancelBtn;
        private bool _installing;

        public InstallerForm()
        {
            Text = string.Format("{0} {1} 安装向导", InstallEngine.AppName, InstallEngine.AppVersion);
            ClientSize = new Size(520, 385);
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            StartPosition = FormStartPosition.CenterScreen;
            BackColor = Color.FromArgb(248, 250, 252);
            Font = new Font("Microsoft YaHei UI", 9.5f);

            Icon appIcon = null;
            try
            {
                appIcon = Icon.ExtractAssociatedIcon(Application.ExecutablePath);
                if (appIcon != null) Icon = appIcon;
            }
            catch { }

            Panel header = new Panel();
            header.Dock = DockStyle.Top;
            header.Height = 82;
            header.BackColor = Color.White;
            Controls.Add(header);

            Panel divider = new Panel();
            divider.Dock = DockStyle.Bottom;
            divider.Height = 1;
            divider.BackColor = Color.FromArgb(226, 232, 240);
            header.Controls.Add(divider);

            if (appIcon != null)
            {
                PictureBox iconBox = new PictureBox();
                iconBox.Image = appIcon.ToBitmap();
                iconBox.SizeMode = PictureBoxSizeMode.StretchImage;
                iconBox.Location = new Point(24, 18);
                iconBox.Size = new Size(44, 44);
                header.Controls.Add(iconBox);
            }

            Label titleLabel = new Label();
            titleLabel.Text = "安装 " + InstallEngine.AppName;
            titleLabel.Font = new Font("Microsoft YaHei UI", 14f, FontStyle.Bold);
            titleLabel.ForeColor = Color.FromArgb(15, 23, 42);
            titleLabel.Location = new Point(82, 16);
            titleLabel.AutoSize = true;
            header.Controls.Add(titleLabel);

            Label subLabel = new Label();
            subLabel.Text = string.Format("局域网极速快传与群聊  ·  版本 {0}", InstallEngine.AppVersion);
            subLabel.ForeColor = Color.FromArgb(100, 116, 139);
            subLabel.Location = new Point(84, 47);
            subLabel.AutoSize = true;
            header.Controls.Add(subLabel);

            Label pathLabel = new Label();
            pathLabel.Text = "安装位置";
            pathLabel.ForeColor = Color.FromArgb(51, 65, 85);
            pathLabel.Location = new Point(24, 102);
            pathLabel.AutoSize = true;
            Controls.Add(pathLabel);

            _pathBox = new TextBox();
            _pathBox.Text = InstallEngine.DefaultInstallDir();
            _pathBox.Location = new Point(24, 128);
            _pathBox.Size = new Size(376, 28);
            Controls.Add(_pathBox);

            _browseBtn = new Button();
            _browseBtn.Text = "浏览...";
            _browseBtn.Location = new Point(408, 127);
            _browseBtn.Size = new Size(88, 29);
            _browseBtn.FlatStyle = FlatStyle.System;
            _browseBtn.Click += OnBrowseClicked;
            Controls.Add(_browseBtn);

            _chkDesktop = CreateOptionCheckBox("创建桌面快捷方式", 174);
            _chkStartMenu = CreateOptionCheckBox("创建开始菜单快捷方式", 204);
            _chkLaunch = CreateOptionCheckBox("安装完成后立即启动 " + InstallEngine.AppName, 234);
            Controls.Add(_chkDesktop);
            Controls.Add(_chkStartMenu);
            Controls.Add(_chkLaunch);

            _statusLabel = new Label();
            _statusLabel.Text = "准备就绪，点击「开始安装」继续";
            _statusLabel.ForeColor = Color.FromArgb(100, 116, 139);
            _statusLabel.Location = new Point(24, 276);
            _statusLabel.Size = new Size(472, 22);
            Controls.Add(_statusLabel);

            _progressBar = new ProgressBar();
            _progressBar.Location = new Point(24, 302);
            _progressBar.Size = new Size(472, 16);
            _progressBar.Minimum = 0;
            _progressBar.Maximum = 100;
            Controls.Add(_progressBar);

            _installBtn = new Button();
            _installBtn.Text = "开始安装";
            _installBtn.Location = new Point(296, 334);
            _installBtn.Size = new Size(104, 34);
            _installBtn.BackColor = Color.FromArgb(59, 130, 246);
            _installBtn.ForeColor = Color.White;
            _installBtn.FlatStyle = FlatStyle.Flat;
            _installBtn.FlatAppearance.BorderSize = 0;
            _installBtn.Cursor = Cursors.Hand;
            _installBtn.Click += OnInstallClicked;
            Controls.Add(_installBtn);

            _cancelBtn = new Button();
            _cancelBtn.Text = "取消";
            _cancelBtn.Location = new Point(410, 334);
            _cancelBtn.Size = new Size(86, 34);
            _cancelBtn.FlatStyle = FlatStyle.System;
            _cancelBtn.Click += delegate { if (!_installing) Close(); };
            Controls.Add(_cancelBtn);
        }

        private static CheckBox CreateOptionCheckBox(string text, int top)
        {
            CheckBox cb = new CheckBox();
            cb.Text = text;
            cb.Checked = true;
            cb.ForeColor = Color.FromArgb(30, 41, 59);
            cb.Location = new Point(24, top);
            cb.AutoSize = true;
            return cb;
        }

        private void OnBrowseClicked(object sender, EventArgs e)
        {
            using (FolderBrowserDialog dlg = new FolderBrowserDialog())
            {
                dlg.Description = "选择 " + InstallEngine.AppName + " 安装目录";
                dlg.SelectedPath = _pathBox.Text;
                if (dlg.ShowDialog(this) == DialogResult.OK)
                    _pathBox.Text = dlg.SelectedPath;
            }
        }

        private void OnInstallClicked(object sender, EventArgs e)
        {
            if (_installing) return;
            _installing = true;
            SetControlsEnabled(false);

            InstallOptions options = new InstallOptions
            {
                InstallDir = _pathBox.Text,
                CreateDesktopShortcut = _chkDesktop.Checked,
                CreateStartMenuShortcut = _chkStartMenu.Checked
            };
            bool launchAfter = _chkLaunch.Checked;

            Thread worker = new Thread(delegate()
            {
                try
                {
                    string exePath = InstallEngine.Install(options, UpdateProgress);
                    BeginInvoke((Action)delegate()
                    {
                        _installing = false;
                        MessageBox.Show(
                            this,
                            string.Format("{0} 已成功安装至：\n{1}", InstallEngine.AppName, options.InstallDir),
                            "安装完成",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Information);

                        if (launchAfter)
                        {
                            ProcessStartInfo psi = new ProcessStartInfo(exePath);
                            psi.WorkingDirectory = System.IO.Path.GetDirectoryName(exePath);
                            Process.Start(psi);
                        }
                        Close();
                    });
                }
                catch (Exception ex)
                {
                    BeginInvoke((Action)delegate()
                    {
                        _installing = false;
                        SetControlsEnabled(true);
                        _statusLabel.Text = "安装失败";
                        MessageBox.Show(
                            this,
                            "安装过程中发生错误：\n" + ex.Message,
                            "安装失败",
                            MessageBoxButtons.OK,
                            MessageBoxIcon.Error);
                    });
                }
            });
            worker.IsBackground = true;
            worker.Start();
        }

        private void UpdateProgress(int percent, string status)
        {
            if (IsDisposed) return;
            BeginInvoke((Action)delegate()
            {
                _progressBar.Value = Math.Max(0, Math.Min(100, percent));
                _statusLabel.Text = status;
            });
        }

        private void SetControlsEnabled(bool enabled)
        {
            _pathBox.Enabled = enabled;
            _browseBtn.Enabled = enabled;
            _chkDesktop.Enabled = enabled;
            _chkStartMenu.Enabled = enabled;
            _chkLaunch.Enabled = enabled;
            _installBtn.Enabled = enabled;
            _cancelBtn.Enabled = enabled;
        }
    }
}
