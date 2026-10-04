; SpacePenguin NSIS Installer Script
; Build with: makensis installer.nsi

!define PRODUCT_NAME "SpacePenguin"
!define PRODUCT_VERSION "0.1.0"
!define PRODUCT_PUBLISHER "SpacePenguin Team"
!define PRODUCT_WEB_SITE "https://github.com/yourusername/SpacePenguin"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\SpacePenguin.exe"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_UNINST_ROOT_KEY "HKLM"

; Modern UI
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

; General settings
Name "${PRODUCT_NAME}"
OutFile "SpacePenguin-${PRODUCT_VERSION}-windows-x64.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
ShowInstDetails show
ShowUninstDetails show

; Request admin rights
RequestExecutionLevel admin

; Icon
!define MUI_ICON "assets/icon.ico"
!define MUI_UNICON "assets/icon.ico"

; Modern UI pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Language
!insertmacro MUI_LANGUAGE "English"

; Version info
VIProductVersion "${PRODUCT_VERSION}"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2026 SpacePenguin Team"
VIAddVersionKey "FileDescription" "A secure, lightweight browser"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"

; Sections
Section "Main" SEC_MAIN
    SetOutPath "$INSTDIR"

    ; Main executable
    File "build\SpacePenguin.exe"

    ; Qt libraries
    File /r "build\*.dll"
    File /r "build\*.exe"

    ; Qt plugins
    File /r "build\plugins\*.*"

    ; Qt translations
    File /r "build\translations\*.qm"

    ; WebEngine resources
    File /r "build\resources\*.*"

    ; WebEngine process
    File "build\QtWebEngineProcess.exe"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"

    ; Registry entries
    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\SpacePenguin.exe"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKLM "${PRODUCT_UNST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoRepair" 1

    ; Start menu shortcuts
    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\SpacePenguin.exe"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

    ; Desktop shortcut
    CreateShortcut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\SpacePenguin.exe"

    ; File associations
    WriteRegStr HKCR ".html" "" "SpacePenguinHTML"
    WriteRegStr HKCR "SpacePenguinHTML" "" "HTML Document"
    WriteRegStr HKCR "SpacePenguinHTML\DefaultIcon" "" "$INSTDIR\SpacePenguin.exe,0"
    WriteRegStr HKCR "SpacePenguinHTML\shell\open\command" "" '"$INSTDIR\SpacePenguin.exe" "%1"'

    WriteRegStr HKCR ".htm" "" "SpacePenguinHTML"
    WriteRegStr HKCR ".url" "" "SpacePenguinURL"
    WriteRegStr HKCR "SpacePenguinURL" "" "URL Shortcut"
    WriteRegStr HKCR "SpacePenguinURL\DefaultIcon" "" "$INSTDIR\SpacePenguin.exe,0"
    WriteRegStr HKCR "SpacePenguinURL\shell\open\command" "" '"$INSTDIR\SpacePenguin.exe" "%1"'

SectionEnd

; Optional: Additional languages section
Section "Additional Languages" SEC_LANGUAGES
    SetOutPath "$INSTDIR\translations"
    File /r "build\translations\*.qm"
SectionEnd

; Uninstaller
Section "Uninstall"
    ; Remove files
    Delete "$INSTDIR\*.*"
    RMDir /r "$INSTDIR\plugins"
    RMDir /r "$INSTDIR\translations"
    RMDir /r "$INSTDIR\resources"

    ; Remove registry
    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
    DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"

    ; Remove file associations
    DeleteRegKey HKCR "SpacePenguinHTML"
    DeleteRegKey HKCR "SpacePenguinURL"

    ; Remove shortcuts
    Delete "$SMPROGRAMS\${PRODUCT_NAME}\*.*"
    RMDir "$SMPROGRAMS\${PRODUCT_NAME}"
    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"

    ; Remove install directory
    RMDir "$INSTDIR"
SectionEnd

; Functions
Function .onInit
    ; Check if already installed
    StrCpy $0 0
    ReadRegStr $0 HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion"
    ${If} $0 != ""
        MessageBox MB_YESNO|MB_ICONQUESTION \
            "SpacePenguin is already installed (version $0).$\n$\nDo you want to reinstall?" \
            IDNO +2
        Abort
    ${EndIf}
FunctionEnd

Function un.onUninstallSuccess
    HideWindow
    MessageBox MB_OK|MB_ICONINFORMATION "SpacePenguin has been successfully uninstalled."
FunctionEnd