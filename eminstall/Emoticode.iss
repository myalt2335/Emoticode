[Setup]
AppName=Emoticode
AppVersion=1.B004.09.250904.GRENUS@0d9b3
DefaultDirName={localappdata}\Emoticode
DisableDirPage=yes
DisableProgramGroupPage=yes
OutputDir=.
OutputBaseFilename=EmoticodeInstaller
Compression=lzma
SolidCompression=yes
SetupIconFile=emoticode.ico
UninstallDisplayIcon={app}\wrapper\emo.exe
PrivilegesRequired=admin
ChangesAssociations=yes

[Files]
Source: "bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "wrapper\*"; DestDir: "{app}\wrapper"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "mingw\*"; DestDir: "{app}\mingw"; Flags: ignoreversion recursesubdirs createallsubdirs; Check: NeedBundledGCC
Source: "samples\*"; DestDir: "{code:GetSamplesDir}"; Flags: ignoreversion recursesubdirs createallsubdirs; Tasks: installSamples

[Tasks]
Name: installSamples; Description: "Install sample scripts"; GroupDescription: "Optional Components:"; Flags: unchecked

; This does not do SHIT. It does *NOT* register .O_O with the friendly name "Emoticode Source File" its more waste than anything. I don't wanna bother deleting it incase I try and fix it in the future tho
[Registry]
Root: HKLM; Subkey: "Software\Classes\.O_O"; ValueType: string; ValueData: "Emoticode.SourceFile"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "Software\Classes\Emoticode.SourceFile"; ValueType: string; ValueData: "Emoticode Source File"; Flags: uninsdeletekey
Root: HKLM; Subkey: "Software\Classes\Emoticode.SourceFile\DefaultIcon"; ValueType: string; ValueData: "{app}\wrapper\emotinterpret.exe,0"
Root: HKLM; Subkey: "Software\Classes\Emoticode.SourceFile\shell\open\command"; ValueType: string; ValueData: """{app}\wrapper\emotinterpret.exe"" ""%1"""

[Code]
var
  SamplesDirPage: TInputDirWizardPage;

function GetSamplesDir(Param: string): string;
begin
  Result := SamplesDirPage.Values[0];
end;

procedure InitializeWizard;
begin
  SamplesDirPage := CreateInputDirPage(wpSelectTasks,
    'Sample Scripts Location',
    'Where should the sample scripts be installed?',
    'Select a folder to install the optional sample scripts:',
    False, '');
  SamplesDirPage.Add('');
  SamplesDirPage.Values[0] := ExpandConstant('{userdocs}\Emoticode Samples');
end;

function ShouldSkipPage(PageID: Integer): Boolean;
begin
  if PageID = SamplesDirPage.ID then
    Result := not IsTaskSelected('installSamples')
  else
    Result := False;
end;

function DirInPath(Dir: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER,
    'Environment', 'Path', OrigPath) then
    OrigPath := '';
  Result := Pos(Dir, OrigPath) > 0;
end;

procedure AddDirToPath(Dir: string);
var
  OrigPath, NewPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER,
    'Environment', 'Path', OrigPath) then
    OrigPath := '';
  if OrigPath = '' then
    NewPath := Dir
  else
    NewPath := OrigPath + ';' + Dir;
  RegWriteStringValue(HKEY_CURRENT_USER,
    'Environment', 'Path', NewPath);
end;

function ToolExists(Tool: string): Boolean;
var
  ResultCode: Integer;
begin
  if Exec('cmd.exe', '/c where ' + Tool, '', SW_HIDE,
    ewWaitUntilTerminated, ResultCode) then
    Result := (ResultCode = 0)
  else
    Result := False;
end;

function NeedBundledGCC: Boolean;
begin
  Result := (not ToolExists('gcc')) or (not ToolExists('g++'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    if not DirInPath(ExpandConstant('{app}\wrapper')) then
      AddDirToPath(ExpandConstant('{app}\wrapper'));
    if NeedBundledGCC then
      if not DirInPath(ExpandConstant('{app}\mingw')) then
        AddDirToPath(ExpandConstant('{app}\mingw'));
  end;
end;
