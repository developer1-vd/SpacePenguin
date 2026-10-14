; SpacePenguin Windows installer.
;
; Build the app first; build-windows.ps1 does everything:
;   powershell -ExecutionPolicy Bypass -File build-windows.ps1 -Version 0.1.0
; or by hand, after `cmake --install <build> --config Release --prefix <dist>`:
;   makensis /DVERSION=0.1.0 /DSOURCE_DIR=<dist> installer.nsi
;
; SOURCE_DIR is the install prefix, which windeployqt has already filled with the
; Qt DLLs, plugins, QtWebEngineProcess.exe and its resources.

Unicode true

!ifndef VERSION
  !define VERSION "0.1.0"
!endif
!ifndef SOURCE_DIR
  !define SOURCE_DIR "build-windows\dist"
!endif

!define PRODUCT_NAME "SpacePenguin"
!define PRODUCT_PUBLISHER "SpacePenguin"
!define PRODUCT_WEB_SITE "https://github.com/developer1-vd/SpacePenguin"
!define PRODUCT_EXE "SpacePenguin.exe"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_EXE}"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_CAPABILITIES "Software\Clients\StartMenuInternet\${PRODUCT_NAME}\Capabilities"
!define PRODUCT_PROGID "SpacePenguinHTML"

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

Name "${PRODUCT_NAME} ${VERSION}"
OutFile "SpacePenguin-${VERSION}-windows-x64-setup.exe"
InstallDir "$PROGRAMFILES64\${PRODUCT_NAME}"
InstallDirRegKey HKLM "${PRODUCT_DIR_REGKEY}" ""
RequestExecutionLevel admin
SetCompressor /SOLID lzma
ShowInstDetails show
ShowUninstDetails show

!define MUI_ICON "packaging\windows\spacepenguin.ico"
!define MUI_UNICON "packaging\windows\spacepenguin.ico"
!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\${PRODUCT_EXE}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; VIProductVersion needs four numeric parts.
VIProductVersion "${VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "GNU General Public License v3"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

Section "SpacePenguin (required)" SEC_MAIN
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "${SOURCE_DIR}\*.*"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\${PRODUCT_EXE}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr HKLM "${PRODUCT_UNINST_KEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${PRODUCT_UNINST_KEY}" "NoRepair" 1

    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_EXE}"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} (Private).lnk" \
        "$INSTDIR\${PRODUCT_EXE}" "--private"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall ${PRODUCT_NAME}.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
    CreateShortcut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_EXE}"
SectionEnd

; Lists SpacePenguin under Settings > Apps > Default apps. Windows asks the user
; before it becomes the default; nothing is taken over silently.
Section "Offer as a default browser" SEC_BROWSER
    WriteRegStr HKLM "Software\Classes\${PRODUCT_PROGID}" "" "SpacePenguin HTML Document"
    WriteRegStr HKLM "Software\Classes\${PRODUCT_PROGID}\DefaultIcon" "" "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr HKLM "Software\Classes\${PRODUCT_PROGID}\shell\open\command" "" \
        '"$INSTDIR\${PRODUCT_EXE}" "%1"'

    WriteRegStr HKLM "Software\Clients\StartMenuInternet\${PRODUCT_NAME}" "" "${PRODUCT_NAME}"
    WriteRegStr HKLM "Software\Clients\StartMenuInternet\${PRODUCT_NAME}\DefaultIcon" "" \
        "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr HKLM "Software\Clients\StartMenuInternet\${PRODUCT_NAME}\shell\open\command" "" \
        '"$INSTDIR\${PRODUCT_EXE}"'
    WriteRegStr HKLM "${PRODUCT_CAPABILITIES}" "ApplicationName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${PRODUCT_CAPABILITIES}" "ApplicationIcon" "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr HKLM "${PRODUCT_CAPABILITIES}" "ApplicationDescription" \
        "A secure, lightweight, and usable browser written in Qt"
    WriteRegStr HKLM "${PRODUCT_CAPABILITIES}\FileAssociations" ".htm" "${PRODUCT_PROGID}"
    WriteRegStr HKLM "${PRODUCT_CAPABILITIES}\FileAssociations" ".html" "${PRODUCT_PROGID}"
    WriteRegStr HKLM "${PRODUCT_CAPABILITIES}\URLAssociations" "http" "${PRODUCT_PROGID}"
    WriteRegStr HKLM "${PRODUCT_CAPABILITIES}\URLAssociations" "https" "${PRODUCT_PROGID}"
    WriteRegStr HKLM "Software\RegisteredApplications" "${PRODUCT_NAME}" "${PRODUCT_CAPABILITIES}"
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_MAIN} "The browser itself."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Put a SpacePenguin shortcut on the desktop."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_BROWSER} \
        "List SpacePenguin in Windows' Default apps so you can choose it as your browser."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
    ${If} ${FileExists} "$INSTDIR\${PRODUCT_EXE}"
        RMDir /r "$INSTDIR"
    ${EndIf}

    DeleteRegKey HKLM "${PRODUCT_DIR_REGKEY}"
    DeleteRegKey HKLM "${PRODUCT_UNINST_KEY}"
    DeleteRegKey HKLM "Software\Classes\${PRODUCT_PROGID}"
    DeleteRegKey HKLM "Software\Clients\StartMenuInternet\${PRODUCT_NAME}"
    DeleteRegValue HKLM "Software\RegisteredApplications" "${PRODUCT_NAME}"

    RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
SectionEnd

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "SpacePenguin needs 64-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
FunctionEnd

; Always install into a folder of our own, so the uninstaller's recursive delete
; can never reach anything else (for example if "C:\Program Files" is chosen).
Function DirectoryLeave
    ${GetFileName} "$INSTDIR" $0
    ${If} $0 != "${PRODUCT_NAME}"
        StrCpy $INSTDIR "$INSTDIR\${PRODUCT_NAME}"
    ${EndIf}
FunctionEnd

Function un.onInit
    SetRegView 64
FunctionEnd
