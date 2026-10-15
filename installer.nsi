; SpacePenguin Windows installer (NSIS 3).
;
; build-windows.ps1 builds the app and then this installer. By hand, after
; `cmake --install <build> --config Release --prefix <absolute dist dir>`:
;
;   makensis /DVERSION=0.1.0 /DSOURCE_DIR=<dist dir> [/DOUTPUT_DIR=<dir>] installer.nsi
;
; SOURCE_DIR is the install prefix: SpacePenguin.exe with the Qt DLLs, plugins,
; QtWebEngineProcess.exe, its resources and the MSVC runtime beside it.
;
; The installer offers a per-user install (no administrator rights, into
; %LOCALAPPDATA%\Programs) or an install for everyone (into Program Files). It upgrades an
; existing installation in place and keeps browsing data unless the uninstaller is
; told to remove it. Command line:
;   setup.exe /S                     silent install (per-user unless run elevated)
;   setup.exe /S /AllUsers           silent install for everyone (needs elevation)
;   setup.exe /S /CurrentUser        silent per-user install
;   setup.exe /S /D=C:\Path          choose the folder (must be last)

Unicode true
ManifestDPIAware true

!ifndef VERSION
  !define VERSION "0.1.0"
!endif
!ifndef VERSION_QUAD
  !define VERSION_QUAD "${VERSION}.0"
!endif
!ifndef SOURCE_DIR
  !define SOURCE_DIR "build-windows\dist"
!endif
!ifndef OUTPUT_DIR
  !define OUTPUT_DIR "."
!endif

!define PRODUCT_NAME "SpacePenguin"
!define PRODUCT_PUBLISHER "SpacePenguin"
!define PRODUCT_WEB_SITE "https://github.com/developer1-vd/SpacePenguin"
!define PRODUCT_EXE "SpacePenguin.exe"
!define PRODUCT_DIR_REGKEY "Software\Microsoft\Windows\CurrentVersion\App Paths\${PRODUCT_EXE}"
!define PRODUCT_UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"
!define PRODUCT_CLIENT_KEY "Software\Clients\StartMenuInternet\${PRODUCT_NAME}"
!define PRODUCT_CAPABILITIES "${PRODUCT_CLIENT_KEY}\Capabilities"
!define PRODUCT_PROGID "SpacePenguinHTML"

; ---- Per-user or all-users install ----------------------------------------------

!define MULTIUSER_EXECUTIONLEVEL Highest
!define MULTIUSER_MUI
!define MULTIUSER_INSTALLMODE_COMMANDLINE
!define MULTIUSER_USE_PROGRAMFILES64
!define MULTIUSER_INSTALLMODE_INSTDIR "${PRODUCT_NAME}"
; Remember where and how an earlier version was installed.
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MULTIUSER_INSTALLMODE_DEFAULT_REGISTRY_VALUENAME "UninstallString"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_KEY "${PRODUCT_UNINST_KEY}"
!define MULTIUSER_INSTALLMODE_INSTDIR_REGISTRY_VALUENAME "InstallLocation"

!include "MultiUser.nsh"
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

Name "${PRODUCT_NAME} ${VERSION}"
OutFile "${OUTPUT_DIR}\SpacePenguin-${VERSION}-windows-x64-setup.exe"
BrandingText "${PRODUCT_NAME} ${VERSION}"
SetCompressor /SOLID lzma
SetCompressorDictSize 64
ShowInstDetails show
ShowUninstDetails show

!define MUI_ICON "packaging\windows\spacepenguin.ico"
!define MUI_UNICON "packaging\windows\spacepenguin.ico"
!define MUI_ABORTWARNING
!define MUI_COMPONENTSPAGE_SMALLDESC
!define MUI_FINISHPAGE_RUN "$INSTDIR\${PRODUCT_EXE}"
!define MUI_FINISHPAGE_RUN_TEXT "Start ${PRODUCT_NAME}"
!define MUI_FINISHPAGE_LINK "${PRODUCT_NAME} on GitHub"
!define MUI_FINISHPAGE_LINK_LOCATION "${PRODUCT_WEB_SITE}"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MULTIUSER_PAGE_INSTALLMODE
!insertmacro MUI_PAGE_COMPONENTS
!define MUI_PAGE_CUSTOMFUNCTION_LEAVE DirectoryLeave
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${VERSION_QUAD}"
VIFileVersion "${VERSION_QUAD}"
VIAddVersionKey "ProductName" "${PRODUCT_NAME}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "LegalCopyright" "GNU General Public License v3"
VIAddVersionKey "FileDescription" "${PRODUCT_NAME} installer"
VIAddVersionKey "FileVersion" "${VERSION}"
VIAddVersionKey "ProductVersion" "${VERSION}"

; ---- Install --------------------------------------------------------------------

Section "-Remove the previous version"
    ; Upgrading in place: run the old uninstaller silently. It keeps browsing data,
    ; because its "remove browsing data" option is off by default.
    ReadRegStr $0 SHCTX "${PRODUCT_UNINST_KEY}" "InstallLocation"
    ${If} $0 != ""
    ${AndIf} ${FileExists} "$0\Uninstall.exe"
        DetailPrint "Removing the installed version from $0"
        ExecWait '"$0\Uninstall.exe" /S _?=$0'
        Delete "$0\Uninstall.exe"
        RMDir "$0"
    ${EndIf}
SectionEnd

Section "SpacePenguin (required)" SEC_MAIN
    SectionIn RO
    SetOutPath "$INSTDIR"
    File /r "${SOURCE_DIR}\*.*"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr SHCTX "${PRODUCT_DIR_REGKEY}" "" "$INSTDIR\${PRODUCT_EXE}"
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayIcon" "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "DisplayVersion" "${VERSION}"
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "URLInfoAbout" "${PRODUCT_WEB_SITE}"
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "UninstallString" \
        '"$INSTDIR\Uninstall.exe" /$MultiUser.InstallMode'
    WriteRegStr SHCTX "${PRODUCT_UNINST_KEY}" "QuietUninstallString" \
        '"$INSTDIR\Uninstall.exe" /$MultiUser.InstallMode /S'
    WriteRegDWORD SHCTX "${PRODUCT_UNINST_KEY}" "NoModify" 1
    WriteRegDWORD SHCTX "${PRODUCT_UNINST_KEY}" "NoRepair" 1
    ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
    IntFmt $0 "0x%08X" $0
    WriteRegDWORD SHCTX "${PRODUCT_UNINST_KEY}" "EstimatedSize" "$0"

    CreateDirectory "$SMPROGRAMS\${PRODUCT_NAME}"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_EXE}"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\${PRODUCT_NAME} (Private Window).lnk" \
        "$INSTDIR\${PRODUCT_EXE}" "--private"
    CreateShortcut "$SMPROGRAMS\${PRODUCT_NAME}\Uninstall ${PRODUCT_NAME}.lnk" "$INSTDIR\Uninstall.exe" \
        "/$MultiUser.InstallMode"
SectionEnd

Section "Desktop shortcut" SEC_DESKTOP
    CreateShortcut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\${PRODUCT_EXE}"
SectionEnd

; Lists SpacePenguin under Settings > Apps > Default apps. Windows asks the user
; before it becomes the default; nothing is taken over silently.
Section "Offer as a default browser" SEC_BROWSER
    WriteRegStr SHCTX "Software\Classes\${PRODUCT_PROGID}" "" "SpacePenguin HTML Document"
    WriteRegStr SHCTX "Software\Classes\${PRODUCT_PROGID}\DefaultIcon" "" "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr SHCTX "Software\Classes\${PRODUCT_PROGID}\shell\open\command" "" \
        '"$INSTDIR\${PRODUCT_EXE}" "%1"'

    WriteRegStr SHCTX "${PRODUCT_CLIENT_KEY}" "" "${PRODUCT_NAME}"
    WriteRegStr SHCTX "${PRODUCT_CLIENT_KEY}\DefaultIcon" "" "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr SHCTX "${PRODUCT_CLIENT_KEY}\shell\open\command" "" '"$INSTDIR\${PRODUCT_EXE}"'
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}" "ApplicationName" "${PRODUCT_NAME}"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}" "ApplicationIcon" "$INSTDIR\${PRODUCT_EXE},0"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}" "ApplicationDescription" \
        "A secure, lightweight, and usable browser written in Qt"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}\FileAssociations" ".htm" "${PRODUCT_PROGID}"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}\FileAssociations" ".html" "${PRODUCT_PROGID}"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}\FileAssociations" ".xhtml" "${PRODUCT_PROGID}"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}\URLAssociations" "http" "${PRODUCT_PROGID}"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}\URLAssociations" "https" "${PRODUCT_PROGID}"
    WriteRegStr SHCTX "${PRODUCT_CAPABILITIES}\StartMenu" "StartMenuInternet" "${PRODUCT_NAME}"
    WriteRegStr SHCTX "Software\RegisteredApplications" "${PRODUCT_NAME}" "${PRODUCT_CAPABILITIES}"
    ; Tell Explorer the associations changed (SHCNE_ASSOCCHANGED).
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_MAIN} "The browser itself."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_DESKTOP} "Put a SpacePenguin shortcut on the desktop."
    !insertmacro MUI_DESCRIPTION_TEXT ${SEC_BROWSER} \
        "List SpacePenguin in Windows' Default apps so you can choose it as your browser."
!insertmacro MUI_FUNCTION_DESCRIPTION_END

; ---- Uninstall ------------------------------------------------------------------

Section "un.SpacePenguin" UNSEC_MAIN
    SectionIn RO
    ; The install folder is always one of our own (see DirectoryLeave), so the
    ; recursive delete cannot reach anything else.
    ${If} ${FileExists} "$INSTDIR\${PRODUCT_EXE}"
        RMDir /r "$INSTDIR"
    ${EndIf}

    DeleteRegKey SHCTX "${PRODUCT_DIR_REGKEY}"
    DeleteRegKey SHCTX "${PRODUCT_UNINST_KEY}"
    DeleteRegKey SHCTX "Software\Classes\${PRODUCT_PROGID}"
    DeleteRegKey SHCTX "${PRODUCT_CLIENT_KEY}"
    DeleteRegValue SHCTX "Software\RegisteredApplications" "${PRODUCT_NAME}"
    System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'

    RMDir /r "$SMPROGRAMS\${PRODUCT_NAME}"
    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
SectionEnd

Section /o "un.Remove browsing data" UNSEC_DATA
    ; History, bookmarks, settings (QSettings lives in the registry on Windows),
    ; cookies, cache, filter lists and user scripts of the current user.
    RMDir /r "$APPDATA\${PRODUCT_NAME}"
    RMDir /r "$LOCALAPPDATA\${PRODUCT_NAME}"
    DeleteRegKey HKCU "Software\${PRODUCT_NAME}"
SectionEnd

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
    !insertmacro MUI_DESCRIPTION_TEXT ${UNSEC_MAIN} "The program, its shortcuts and its registration."
    !insertmacro MUI_DESCRIPTION_TEXT ${UNSEC_DATA} \
        "Also delete your history, bookmarks, settings, cookies and cache. Off by default."
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

; ---- Functions ------------------------------------------------------------------

Function .onInit
    ${IfNot} ${RunningX64}
        MessageBox MB_OK|MB_ICONSTOP "${PRODUCT_NAME} needs 64-bit Windows."
        Abort
    ${EndIf}
    SetRegView 64
    ; MULTIUSER_INIT picks the default folder for the install mode and would
    ; overwrite a folder given with /D=, so keep that one.
    StrCpy $0 $INSTDIR
    !insertmacro MULTIUSER_INIT
    ${If} $0 != ""
        StrCpy $INSTDIR $0
    ${EndIf}
FunctionEnd

Function un.onInit
    SetRegView 64
    !insertmacro MULTIUSER_UNINIT
FunctionEnd

; Always install into a folder of our own, so the uninstaller's recursive delete can
; never reach anything else (for example if "C:\Program Files" itself is chosen).
Function DirectoryLeave
    ${GetFileName} "$INSTDIR" $0
    ${If} $0 != "${PRODUCT_NAME}"
        StrCpy $INSTDIR "$INSTDIR\${PRODUCT_NAME}"
    ${EndIf}
FunctionEnd
