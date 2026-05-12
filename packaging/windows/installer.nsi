!define PRODUCT_NAME "Saw"
!define PRODUCT_VERSION "0.1.0"
!define PUBLISHER "LorenzoMazzeo"
!define VST3_SRC "Saw.vst3"
!define VST3_DEST "$COMMONPROGRAMFILES64\VST3"
!define UNINSTALL_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_NAME}"

Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
OutFile "Saw-${PRODUCT_VERSION}-Windows.exe"
InstallDir "${VST3_DEST}"
RequestExecutionLevel admin
SetCompressor /SOLID lzma

Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "VST3 Plugin" SecVST3
    SetOutPath "${VST3_DEST}"
    File /r "${VST3_SRC}"

    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "Publisher" "${PUBLISHER}"
    WriteRegStr HKLM "${UNINSTALL_KEY}" "UninstallString" "$INSTDIR\uninstall-saw.exe"
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINSTALL_KEY}" "NoRepair" 1

    WriteUninstaller "$INSTDIR\uninstall-saw.exe"
SectionEnd

Section "Uninstall"
    RMDir /r "${VST3_DEST}\Saw.vst3"
    Delete "${VST3_DEST}\uninstall-saw.exe"
    DeleteRegKey HKLM "${UNINSTALL_KEY}"
SectionEnd
