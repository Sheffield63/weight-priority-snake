; SnakeGame Installer Script for Inno Setup 7
; Build: ISCC.exe installer\SnakeGame.iss

#define MyAppName "权重优先自动化贪吃蛇"
#define MyAppNameEn "SnakeGame"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "SnakeGame Project"
#define MyAppExeName "SnakeGame.exe"

[Setup]
AppId={{7F3E9A2C-4B5D-4C6E-8A9B-2D1E0F3C5A7B}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppNameEn}
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableWelcomePage=no
LicenseFile=
OutputDir=..\installer\output
OutputBaseFilename=SnakeGame-{#MyAppVersion}-setup
SetupIconFile=..\resources\app.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; 中文字体在安装界面上的显示
WizardImageAlphaFormat=premultiplied
PrivilegesRequired=lowest
; 版本信息（资源管理器属性页）
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription={#MyAppName} Installer
VersionInfoProductName={#MyAppName}

[Languages]
; 中文语言文件随仓库分发（相对路径，CI 的 choco Inno Setup 不含中文 .isl）
Name: "chinesesimplified"; MessagesFile: "Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; 主程序与 Qt/编译器运行库（dist 全量内容）
Source: "..\dist\SnakeGame.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Qt5Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Qt5Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Qt5Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Qt5Multimedia.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\Qt5Network.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\libgcc_s_seh-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
; Qt 插件目录
Source: "..\dist\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\dist\audio\*"; DestDir: "{app}\audio"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; 清理可能残留的用户配置（QSettings 在注册表，无需删除文件）
