; NextSpaceInterception.nsh - GC3-S3 controlled Interception wrapper
!ifndef NEXTSPACE_INTERCEPTION_NSH
!define NEXTSPACE_INTERCEPTION_NSH
!include "LogicLib.nsh"
!include "FileFunc.nsh"
!include "Sections.nsh"
!include "nsDialogs.nsh"
!include "WinVer.nsh"
!include "x64.nsh"

!define NEXTSPACE_INTERCEPTION_MARKER_KEY "Software\EduLearn Technologies Inc\NextSpace\Components\Interception"
!define NEXTSPACE_INTERCEPTION_VERIFICATION_KEY "Software\EduLearn Technologies Inc\NextSpace\Verification"
!define NEXTSPACE_INTERCEPTION_HELPER_SHA256 "98B4B384BC2C84A9E9F9275352B265DD1B351C4780598D7D1AA7D32B06F5BA86"
!define NEXTSPACE_INTERCEPTION_INSTALL_BAT_SHA256 "77048165377633E8CB5FDBD62C313493BD19E4CDCB0F9CF5BDAEDB88D7BCD105"
!define NEXTSPACE_INTERCEPTION_UNINSTALL_BAT_SHA256 "3CA834971CE2364F53FEF24582C368BD14ECC5F4D8B620FFB04332C2BFC11FA8"
!define NEXTSPACE_INTERCEPTION_VERSION "1.0.0"
!define NEXTSPACE_KEYBOARD_CLASS_GUID "{4D36E96B-E325-11CE-BFC1-08002BE10318}"
!define NEXTSPACE_MOUSE_CLASS_GUID "{4D36E96F-E325-11CE-BFC1-08002BE10318}"
!define NEXTSPACE_KEYBOARD_SERVICE_DISPLAY "Keyboard Upper Filter Driver"
!define NEXTSPACE_MOUSE_SERVICE_DISPLAY "Mouse Upper Filter Driver"
!define NEXTSPACE_EXPECTED_DRIVER_SIZE 18536
!define NEXTSPACE_KEYBOARD_SYS_SHA256 "2CB5EC142CFAC879BCE4A2F9549258DB972AEBBD24F4551B6B748B464EB7DBA9"
!define NEXTSPACE_MOUSE_SYS_SHA256 "0F12D47D01864CA5E1EB663A52B3D2C060521E57B68FF99D70E7F01506E400F9"

Var NextSpaceProfile
Var NextSpaceProfileExplicit
Var NextSpaceNoInterceptionForced
Var NextSpaceTeacherRadio
Var NextSpaceStudentRadio
Var NextSpaceDriverState
Var NextSpaceHelperResult
Var NextSpaceUninstallResult
Var NextSpaceHelperInstalledThisSession
Var NextSpaceRollbackReady
Var NextSpaceRollbackPerformed
Var NextSpaceRollbackResult
Var NextSpaceRollbackDir
Var NextSpaceRecoveryLog
Var NextSpacePriorVersion
Var NextSpacePriorServiceRunning
Var NextSpacePriorServiceStart
Var NextSpaceCleanInstallDirWasEmpty
Var NextSpaceDriverProbeFailed
Var NextSpaceCleanConfigBackupAvailable
Var NextSpacePriorForceGuestPresent
Var NextSpacePriorForceGuestValue

!macro NEXTSPACE_DEFINE_STR_CONTAINS prefix
Function ${prefix}NextSpaceStrContains
  Exch $R1
  Exch
  Exch $R2
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  ${prefix}nextspace_str_loop:
    StrCpy $R5 $R2 $R3 $R4
    StrCmp $R5 $R1 ${prefix}nextspace_str_found
    StrCmp $R5 "" ${prefix}nextspace_str_not_found
    IntOp $R4 $R4 + 1
    Goto ${prefix}nextspace_str_loop
  ${prefix}nextspace_str_found:
    StrCpy $R1 1
    Goto ${prefix}nextspace_str_done
  ${prefix}nextspace_str_not_found:
    StrCpy $R1 0
  ${prefix}nextspace_str_done:
    Pop $R5
    Pop $R4
    Pop $R3
    Pop $R2
    Exch $R1
FunctionEnd
!macroend
!insertmacro NEXTSPACE_DEFINE_STR_CONTAINS ""
!insertmacro NEXTSPACE_DEFINE_STR_CONTAINS "un."

Function NextSpaceCountSubstring
  Exch $R1
  Exch
  Exch $R2
  Push $R0
  Push $R3
  Push $R4
  Push $R5
  StrLen $R3 $R1
  StrCpy $R4 0
  StrCpy $R5 0
  nextspace_count_loop:
    StrCpy $R0 $R2 $R3 $R5
    StrCmp $R0 $R1 nextspace_count_found
    StrCmp $R0 "" nextspace_count_done
    IntOp $R5 $R5 + 1
    Goto nextspace_count_loop
  nextspace_count_found:
    IntOp $R4 $R4 + 1
    IntOp $R5 $R5 + $R3
    Goto nextspace_count_loop
  nextspace_count_done:
    StrCpy $R1 $R4
    Pop $R5
    Pop $R4
    Pop $R3
    Pop $R0
    Pop $R2
    Exch $R1
FunctionEnd

!macro NEXTSPACE_DEFINE_HASH_MATCH prefix
Function ${prefix}NextSpaceHashFileMatches
  Exch $R1
  Exch
  Exch $R2
  Push $R3
  Push $R4
  Push $R5
  nsExec::ExecToStack '"$SYSDIR\certutil.exe" -hashfile "$R2" SHA256'
  Pop $R3
  Pop $R4
  StrCpy $R5 0
  ${If} $R3 == 0
    Push "$R4"
    Push "$\r$\n$R1$\r$\nCertUtil:"
    Call ${prefix}NextSpaceStrContains
    Pop $R5
  ${EndIf}
  StrCpy $R1 $R5
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Exch $R1
FunctionEnd
!macroend
!insertmacro NEXTSPACE_DEFINE_HASH_MATCH ""
!insertmacro NEXTSPACE_DEFINE_HASH_MATCH "un."

Function NextSpacePreflightArchitecture
  ReadEnvStr $R0 "PROCESSOR_ARCHITEW6432"
  ${If} $R0 == ""
    ReadEnvStr $R0 "PROCESSOR_ARCHITECTURE"
  ${EndIf}
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "NextSpace Classroom Management requires 64-bit Windows (x64)." /SD IDOK
    SetErrorLevel 1
    Quit
  ${EndIf}
  ${If} $R0 != "AMD64"
    MessageBox MB_OK|MB_ICONSTOP "NextSpace Classroom Management supports AMD64 Windows only; ARM64 and other architectures are not supported by this payload." /SD IDOK
    SetErrorLevel 1
    Quit
  ${EndIf}
  ${IfNot} ${AtLeastWin10}
    MessageBox MB_OK|MB_ICONSTOP "NextSpace Classroom Management requires a supported 64-bit Windows 10 or Windows 11 release." /SD IDOK
    SetErrorLevel 1
    Quit
  ${EndIf}
FunctionEnd

Function NextSpaceResolveProfile
  StrCpy $NextSpaceProfile 0
  StrCpy $NextSpaceProfileExplicit 0
  StrCpy $NextSpaceNoInterceptionForced 0
  StrCpy $NextSpaceHelperInstalledThisSession 0
  StrCpy $NextSpaceHelperResult -1
  StrCpy $NextSpaceRollbackReady 0
  StrCpy $NextSpaceRollbackPerformed 0
  StrCpy $NextSpaceRollbackResult -1
  StrCpy $NextSpacePriorServiceRunning 0
  StrCpy $NextSpacePriorServiceStart ""
  StrCpy $NextSpaceCleanInstallDirWasEmpty 0
  StrCpy $NextSpaceProductMutationStarted 0
  StrCpy $NextSpaceServiceRegisteredThisSession 0
  StrCpy $NextSpaceConfigMutatedThisSession 0
  StrCpy $NextSpaceDriverProbeFailed 0
  StrCpy $NextSpacePriorServiceRemoved 0
  StrCpy $NextSpaceCleanConfigBackupAvailable 0
  StrCpy $NextSpaceForceGuestMutated 0
  StrCpy $NextSpacePriorForceGuestPresent 0
  StrCpy $NextSpacePriorForceGuestValue 0
  ${GetParameters} $R0
  Push "$R0"
  Push "/Profile="
  Call NextSpaceCountSubstring
  Pop $R5
  ${If} $R5 > 1
    MessageBox MB_OK|MB_ICONSTOP "Specify /Profile exactly once." /SD IDOK
    SetErrorLevel 1
    Quit
  ${EndIf}
  Push "$R0"
  Push "/Profile"
  Call NextSpaceStrContains
  Pop $R4
  ClearErrors
  ${GetOptions} $R0 "/Profile=" $R1
  ${IfNot} ${Errors}
    StrCpy $NextSpaceProfileExplicit 1
    ${If} $R1 == "Teacher"
    ${OrIf} $R1 == "teacher"
      ClearErrors
      ${GetOptions} $R0 "/NoMaster" $R2
      ${IfNot} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "Conflict: /Profile=Teacher cannot be combined with /NoMaster.$\r$\nUsage: /Profile=Teacher|Student." /SD IDOK
        SetErrorLevel 1
        Quit
      ${EndIf}
      ClearErrors
      ${GetOptions} $R0 "/NoInterception" $R2
      ${IfNot} ${Errors}
        StrCpy $NextSpaceNoInterceptionForced 1
      ${EndIf}
    ${ElseIf} $R1 == "Student"
    ${OrIf} $R1 == "student"
      ClearErrors
      ${GetOptions} $R0 "/NoInterception" $R2
      ${IfNot} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "Conflict: /Profile=Student cannot be combined with /NoInterception.$\r$\nUsage: /Profile=Teacher|Student." /SD IDOK
        SetErrorLevel 1
        Quit
      ${EndIf}
      StrCpy $NextSpaceProfile 1
    ${Else}
      MessageBox MB_OK|MB_ICONSTOP "Unknown profile '$R1'.$\r$\nUsage: /Profile=Teacher or /Profile=Student." /SD IDOK
      SetErrorLevel 1
      Quit
    ${EndIf}
  ${Else}
    ${If} $R4 == 1
      MessageBox MB_OK|MB_ICONSTOP "Malformed /Profile option. Use exactly /Profile=Teacher or /Profile=Student." /SD IDOK
      SetErrorLevel 1
      Quit
    ${EndIf}
    ClearErrors
    ${GetOptions} $R0 "/NoMaster" $R2
    ${IfNot} ${Errors}
      StrCpy $NextSpaceProfileExplicit 1
      ClearErrors
      ${GetOptions} $R0 "/NoInterception" $R3
      ${IfNot} ${Errors}
        MessageBox MB_OK|MB_ICONSTOP "Conflict: /NoMaster cannot be combined with /NoInterception.$\r$\nUsage: /Profile=Teacher|Student." /SD IDOK
        SetErrorLevel 1
        Quit
      ${EndIf}
      StrCpy $NextSpaceProfile 1
    ${Else}
      ClearErrors
      ${GetOptions} $R0 "/NoInterception" $R3
      ${IfNot} ${Errors}
        StrCpy $NextSpaceProfileExplicit 1
        StrCpy $NextSpaceNoInterceptionForced 1
      ${Else}
        IfSilent 0 +2
          StrCpy $NextSpaceProfileExplicit 1
      ${EndIf}
    ${EndIf}
  ${EndIf}
  !insertmacro UnselectSection ${SEC_INTERCEPTION}
  Call NextSpaceApplyProfileSections
FunctionEnd

Function NextSpaceApplyProfileSections
  ${If} $NextSpaceProfile == 1
    SectionSetFlags ${SEC_MASTER} ${SF_RO}
    IntOp $R0 ${SF_SELECTED} | ${SF_RO}
    SectionSetFlags ${SEC_INTERCEPTION} $R0
  ${Else}
    IntOp $R0 ${SF_SELECTED} | ${SF_RO}
    SectionSetFlags ${SEC_MASTER} $R0
    ${If} $NextSpaceNoInterceptionForced == 1
      SectionSetFlags ${SEC_INTERCEPTION} ${SF_RO}
    ${ElseIf} ${SectionIsSelected} ${SEC_INTERCEPTION}
      SectionSetFlags ${SEC_INTERCEPTION} ${SF_SELECTED}
    ${Else}
      SectionSetFlags ${SEC_INTERCEPTION} 0
    ${EndIf}
  ${EndIf}
FunctionEnd

Function NextSpaceProfilePageCreate
  ${If} $NextSpaceProfileExplicit == 1
    Abort
  ${EndIf}
  nsDialogs::Create 1018
  Pop $R0
  ${If} $R0 == error
    Abort
  ${EndIf}
  ${NSD_CreateLabel} 0 0 100% 28u "Choose exactly one workstation profile. The shared Student Agent runtime is installed for both profiles."
  Pop $R0
  ${NSD_CreateRadioButton} 0 38u 100% 18u "Teacher workstation - Teacher Console required; Interception optional and off by default"
  Pop $NextSpaceTeacherRadio
  ${NSD_CreateRadioButton} 0 64u 100% 18u "Student workstation - no Teacher Console; Interception required"
  Pop $NextSpaceStudentRadio
  ${NSD_Check} $NextSpaceTeacherRadio
  nsDialogs::Show
FunctionEnd

Function NextSpaceProfilePageLeave
  ${NSD_GetState} $NextSpaceTeacherRadio $R0
  ${NSD_GetState} $NextSpaceStudentRadio $R1
  ${If} $R0 == ${BST_CHECKED}
  ${AndIf} $R1 != ${BST_CHECKED}
    StrCpy $NextSpaceProfile 0
  ${ElseIf} $R1 == ${BST_CHECKED}
  ${AndIf} $R0 != ${BST_CHECKED}
    StrCpy $NextSpaceProfile 1
  ${Else}
    MessageBox MB_OK|MB_ICONSTOP "Select exactly one workstation profile before continuing." /SD IDOK
    Abort
  ${EndIf}
  Call NextSpaceApplyProfileSections
FunctionEnd

Function NextSpaceVerifyEmbeddedPayload
  InitPluginsDir
  SetOutPath "$PLUGINSDIR\NextSpaceInterception"
  File /oname=install-interception.exe "${NEXTSPACE_STAGE_DIR}\interception\install-interception.exe"
  File /oname=install.bat "${NEXTSPACE_STAGE_DIR}\interception\install.bat"
  File /oname=uninstall.bat "${NEXTSPACE_STAGE_DIR}\interception\uninstall.bat"
  Push "$PLUGINSDIR\NextSpaceInterception\install-interception.exe"
  Push 470528
  Push "${NEXTSPACE_INTERCEPTION_HELPER_SHA256}"
  Call NextSpaceVerifyOnePayloadFile
  Pop $R0
  Push "$PLUGINSDIR\NextSpaceInterception\install.bat"
  Push 150
  Push "${NEXTSPACE_INTERCEPTION_INSTALL_BAT_SHA256}"
  Call NextSpaceVerifyOnePayloadFile
  Pop $R1
  Push "$PLUGINSDIR\NextSpaceInterception\uninstall.bat"
  Push 53
  Push "${NEXTSPACE_INTERCEPTION_UNINSTALL_BAT_SHA256}"
  Call NextSpaceVerifyOnePayloadFile
  Pop $R2
  ${If} $R0 != 1
  ${OrIf} $R1 != 1
  ${OrIf} $R2 != 1
    MessageBox MB_OK|MB_ICONSTOP "The embedded canonical Interception payload failed its exact size/SHA-256 preflight. No product files were changed." /SD IDOK
    SetErrorLevel 1
    Quit
  ${EndIf}
  DetailPrint "Embedded Interception payload verified: ${NEXTSPACE_INTERCEPTION_HELPER_SHA256}."
FunctionEnd

Function NextSpaceVerifyOnePayloadFile
  Exch $R2
  Exch 1
  Exch $R1
  Exch 2
  Exch $R0
  Push $R3
  Push $R4
  Push $R5
  StrCpy $R4 0
  ClearErrors
  FileOpen $R3 "$R0" r
  ${IfNot} ${Errors}
    FileSeek $R3 0 END $R5
    FileClose $R3
    ${If} $R5 == $R1
      Push "$R0"
      Push "$R2"
      Call NextSpaceHashFileMatches
      Pop $R4
    ${EndIf}
  ${EndIf}
  StrCpy $R1 $R4
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R0
  Pop $R2
  Exch $R1
FunctionEnd

Function NextSpaceClassifyProduct
  System::Call 'advapi32::RegOpenKeyExW(p 0x80000002, w "${UNINSTALL_PATH}", i 0, i 0x20119, *p .R3) i .R0'
  ${If} $R0 == 0
    System::Call 'advapi32::RegCloseKey(p $R3)'
  ${ElseIf} $R0 != 2
    Goto nextspace_product_unreadable
  ${EndIf}
  System::Call 'advapi32::RegOpenKeyExW(p 0x80000002, w "${UNINSTALL_PATH}", i 0, i 0x20219, *p .R3) i .R1'
  ${If} $R1 == 0
    System::Call 'advapi32::RegCloseKey(p $R3)'
  ${ElseIf} $R1 != 2
    Goto nextspace_product_unreadable
  ${EndIf}
  ${If} $R0 == 2
  ${AndIf} $R1 == 2
    StrCpy $NextSpaceProductState 0
    Return
  ${EndIf}
  ${If} $R0 == 0
    SetRegView 64
    ClearErrors
    ReadRegStr $R2 HKLM "${UNINSTALL_PATH}" "DisplayName"
    ${If} ${Errors}
      Goto nextspace_product_unknown
    ${ElseIf} $R2 != "${DISPLAY_NAME}"
      Goto nextspace_product_unknown
    ${EndIf}
  ${EndIf}
  ${If} $R1 == 0
    SetRegView 32
    ClearErrors
    ReadRegStr $R2 HKLM "${UNINSTALL_PATH}" "DisplayName"
    StrCpy $R4 0
    ${If} ${Errors}
      StrCpy $R4 1
    ${ElseIf} $R2 != "${DISPLAY_NAME}"
      StrCpy $R4 1
    ${EndIf}
    SetRegView 64
    ${If} $R4 == 1
      Goto nextspace_product_unknown
    ${EndIf}
  ${EndIf}
  StrCpy $NextSpaceProductState 1
  Return
  nextspace_product_unreadable:
  SetRegView 64
  StrCpy $NextSpaceProductState 2
  DetailPrint "Existing product registration could not be read safely; automatic overwrite rejected."
  MessageBox MB_OK|MB_ICONSTOP "The existing product registration could not be read safely. Installation stopped without treating it as absent." /SD IDOK
  SetErrorLevel 1
  Quit
  nextspace_product_unknown:
  SetRegView 64
  StrCpy $NextSpaceProductState 2
  DetailPrint "Unknown or stock Veyon registration detected; automatic overwrite rejected."
  MessageBox MB_OK|MB_ICONSTOP "An existing stock Veyon or unknown product registration was detected.$\r$\nExport its configuration, uninstall it, perform a clean NextSpace install, then import and validate the configuration." /SD IDOK
  SetErrorLevel 1
  Quit
FunctionEnd

!macro NEXTSPACE_CLASSIFY_INTERCEPTION_BODY prefix
  Push $R0
  Push $R1
  Push $R2
  Push $R3
  Push $R4
  Push $R5
  Push $R6
  Push $R7
  Push $R8
  Push $R9
  StrCpy $R1 0
  StrCpy $R2 1
  StrCpy $NextSpaceDriverProbeFailed 0

  nsExec::ExecToStack '"$SYSDIR\reg.exe" query "HKLM\SYSTEM\CurrentControlSet\Control\Class\${NEXTSPACE_KEYBOARD_CLASS_GUID}" /v UpperFilters /reg:64'
  Pop $R8
  Pop $R9
  ${If} $R8 == 0
    StrLen $R0 $R9
    ${If} $R0 >= 500
      StrCpy $NextSpaceDriverProbeFailed 1
      StrCpy $R2 0
    ${EndIf}
    Push "$R9"
    Push "keyboard"
    Call ${prefix}NextSpaceStrContains
    Pop $R3
    Push "$R9"
    Push "REG_MULTI_SZ"
    Call ${prefix}NextSpaceStrContains
    Pop $R4
    Push "$R9"
    Push "keyboard\0kbdclass"
    Call ${prefix}NextSpaceStrContains
    Pop $R5
    Push "$R9"
    Push "keyboard\0kbdclass\0"
    Call ${prefix}NextSpaceStrContains
    Pop $R6
    Push "$R9"
    Push "    UpperFilters    REG_MULTI_SZ    keyboard\0kbdclass$\r$\n"
    Call ${prefix}NextSpaceStrContains
    Pop $R7
    ${If} $R3 == 1
      StrCpy $R1 1
    ${EndIf}
    ${If} $R3 != 1
    ${OrIf} $R4 != 1
    ${OrIf} $R5 != 1
    ${OrIf} $R6 == 1
    ${OrIf} $R7 != 1
      StrCpy $R2 0
    ${EndIf}
  ${Else}
    StrCpy $R2 0
    StrCpy $NextSpaceDriverProbeFailed 1
  ${EndIf}

  nsExec::ExecToStack '"$SYSDIR\reg.exe" query "HKLM\SYSTEM\CurrentControlSet\Control\Class\${NEXTSPACE_MOUSE_CLASS_GUID}" /v UpperFilters /reg:64'
  Pop $R8
  Pop $R9
  ${If} $R8 == 0
    StrLen $R0 $R9
    ${If} $R0 >= 500
      StrCpy $NextSpaceDriverProbeFailed 1
      StrCpy $R2 0
    ${EndIf}
    Push "$R9"
    Push "mouse"
    Call ${prefix}NextSpaceStrContains
    Pop $R3
    Push "$R9"
    Push "REG_MULTI_SZ"
    Call ${prefix}NextSpaceStrContains
    Pop $R4
    Push "$R9"
    Push "mouse\0mouclass"
    Call ${prefix}NextSpaceStrContains
    Pop $R5
    Push "$R9"
    Push "mouse\0mouclass\0"
    Call ${prefix}NextSpaceStrContains
    Pop $R6
    Push "$R9"
    Push "    UpperFilters    REG_MULTI_SZ    mouse\0mouclass$\r$\n"
    Call ${prefix}NextSpaceStrContains
    Pop $R7
    ${If} $R3 == 1
      StrCpy $R1 1
    ${EndIf}
    ${If} $R3 != 1
    ${OrIf} $R4 != 1
    ${OrIf} $R5 != 1
    ${OrIf} $R6 == 1
    ${OrIf} $R7 != 1
      StrCpy $R2 0
    ${EndIf}
  ${Else}
    StrCpy $R2 0
    StrCpy $NextSpaceDriverProbeFailed 1
  ${EndIf}

  System::Call 'advapi32::RegOpenKeyExW(p 0x80000002, w "SYSTEM\CurrentControlSet\Services\keyboard", i 0, i 0x20119, *p .R9) i .R8'
  ${If} $R8 == 0
    System::Call 'advapi32::RegCloseKey(p $R9)'
    StrCpy $R1 1
    ClearErrors
    ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Services\keyboard" "DisplayName"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R0 != "${NEXTSPACE_KEYBOARD_SERVICE_DISPLAY}"
      StrCpy $R2 0
    ${EndIf}
    ClearErrors
    ReadRegDWORD $R3 HKLM "SYSTEM\CurrentControlSet\Services\keyboard" "ErrorControl"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R3 != 1
      StrCpy $R2 0
    ${EndIf}
    ClearErrors
    ReadRegDWORD $R4 HKLM "SYSTEM\CurrentControlSet\Services\keyboard" "Start"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R4 != 3
      StrCpy $R2 0
    ${EndIf}
    ClearErrors
    ReadRegDWORD $R5 HKLM "SYSTEM\CurrentControlSet\Services\keyboard" "Type"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R5 != 1
      StrCpy $R2 0
    ${EndIf}
  ${ElseIf} $R8 == 2
    StrCpy $R2 0
  ${Else}
    StrCpy $R2 0
    StrCpy $NextSpaceDriverProbeFailed 1
  ${EndIf}

  System::Call 'advapi32::RegOpenKeyExW(p 0x80000002, w "SYSTEM\CurrentControlSet\Services\mouse", i 0, i 0x20119, *p .R9) i .R8'
  ${If} $R8 == 0
    System::Call 'advapi32::RegCloseKey(p $R9)'
    StrCpy $R1 1
    ClearErrors
    ReadRegStr $R0 HKLM "SYSTEM\CurrentControlSet\Services\mouse" "DisplayName"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R0 != "${NEXTSPACE_MOUSE_SERVICE_DISPLAY}"
      StrCpy $R2 0
    ${EndIf}
    ClearErrors
    ReadRegDWORD $R3 HKLM "SYSTEM\CurrentControlSet\Services\mouse" "ErrorControl"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R3 != 1
      StrCpy $R2 0
    ${EndIf}
    ClearErrors
    ReadRegDWORD $R4 HKLM "SYSTEM\CurrentControlSet\Services\mouse" "Start"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R4 != 3
      StrCpy $R2 0
    ${EndIf}
    ClearErrors
    ReadRegDWORD $R5 HKLM "SYSTEM\CurrentControlSet\Services\mouse" "Type"
    ${If} ${Errors}
      StrCpy $R2 0
    ${ElseIf} $R5 != 1
      StrCpy $R2 0
    ${EndIf}
  ${ElseIf} $R8 == 2
    StrCpy $R2 0
  ${Else}
    StrCpy $R2 0
    StrCpy $NextSpaceDriverProbeFailed 1
  ${EndIf}

  ${DisableX64FSRedirection}
  IfFileExists "$WINDIR\System32\drivers\keyboard.sys" 0 ${prefix}nextspace_kb_missing
    StrCpy $R1 1
    FileOpen $R0 "$WINDIR\System32\drivers\keyboard.sys" r
    ${If} ${Errors}
      StrCpy $R2 0
    ${Else}
      FileSeek $R0 0 END $R6
      FileClose $R0
      ${If} $R6 != ${NEXTSPACE_EXPECTED_DRIVER_SIZE}
        StrCpy $R2 0
      ${EndIf}
    ${EndIf}
    Push "$WINDIR\System32\drivers\keyboard.sys"
    Push "${NEXTSPACE_KEYBOARD_SYS_SHA256}"
    Call ${prefix}NextSpaceHashFileMatches
    Pop $R7
    ${If} $R7 != 1
      StrCpy $R2 0
    ${EndIf}
    Goto ${prefix}nextspace_kb_done
  ${prefix}nextspace_kb_missing:
    StrCpy $R2 0
    System::Call 'kernel32::GetFileAttributesW(w "$WINDIR\System32\drivers\keyboard.sys") i .R0'
    ${If} $R0 == -1
      System::Call 'kernel32::GetLastError() i .R3'
      ${If} $R3 != 2
      ${AndIf} $R3 != 3
        StrCpy $NextSpaceDriverProbeFailed 1
      ${EndIf}
    ${Else}
      StrCpy $NextSpaceDriverProbeFailed 1
    ${EndIf}
  ${prefix}nextspace_kb_done:

  IfFileExists "$WINDIR\System32\drivers\mouse.sys" 0 ${prefix}nextspace_mouse_missing
    StrCpy $R1 1
    FileOpen $R0 "$WINDIR\System32\drivers\mouse.sys" r
    ${If} ${Errors}
      StrCpy $R2 0
    ${Else}
      FileSeek $R0 0 END $R6
      FileClose $R0
      ${If} $R6 != ${NEXTSPACE_EXPECTED_DRIVER_SIZE}
        StrCpy $R2 0
      ${EndIf}
    ${EndIf}
    Push "$WINDIR\System32\drivers\mouse.sys"
    Push "${NEXTSPACE_MOUSE_SYS_SHA256}"
    Call ${prefix}NextSpaceHashFileMatches
    Pop $R7
    ${If} $R7 != 1
      StrCpy $R2 0
    ${EndIf}
    Goto ${prefix}nextspace_mouse_done
  ${prefix}nextspace_mouse_missing:
    StrCpy $R2 0
    System::Call 'kernel32::GetFileAttributesW(w "$WINDIR\System32\drivers\mouse.sys") i .R0'
    ${If} $R0 == -1
      System::Call 'kernel32::GetLastError() i .R3'
      ${If} $R3 != 2
      ${AndIf} $R3 != 3
        StrCpy $NextSpaceDriverProbeFailed 1
      ${EndIf}
    ${Else}
      StrCpy $NextSpaceDriverProbeFailed 1
    ${EndIf}
  ${prefix}nextspace_mouse_done:
  ${EnableX64FSRedirection}

  System::Call 'advapi32::RegOpenKeyExW(p 0x80000002, w "${NEXTSPACE_INTERCEPTION_MARKER_KEY}", i 0, i 0x20119, *p .R9) i .R8'
  ${If} $R8 == 0
    System::Call 'advapi32::RegCloseKey(p $R9)'
    StrCpy $R7 1
    ClearErrors
    ReadRegDWORD $R0 HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "Managed"
    ${If} ${Errors}
      StrCpy $R7 0
    ${ElseIf} $R0 != 1
      StrCpy $R7 0
    ${EndIf}
    ClearErrors
    ReadRegStr $R3 HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "PayloadSha256"
    ${If} ${Errors}
      StrCpy $R7 0
    ${ElseIf} $R3 != "${NEXTSPACE_INTERCEPTION_HELPER_SHA256}"
      StrCpy $R7 0
    ${EndIf}
    ClearErrors
    ReadRegStr $R4 HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "InstalledByVersion"
    ${If} ${Errors}
      StrCpy $R7 0
    ${ElseIf} $R4 != "${NEXTSPACE_INTERCEPTION_VERSION}"
      StrCpy $R7 0
    ${EndIf}
    ClearErrors
    ReadRegDWORD $R5 HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "PendingVerification"
    ${If} ${Errors}
      StrCpy $R7 0
    ${ElseIf} $R5 != 0
    ${AndIf} $R5 != 1
      StrCpy $R7 0
    ${EndIf}
    ${If} $R7 == 1
    ${AndIf} $R2 == 1
      StrCpy $NextSpaceDriverState 1
    ${Else}
      StrCpy $NextSpaceDriverState 3
    ${EndIf}
  ${ElseIf} $R8 != 2
    StrCpy $NextSpaceDriverState 3
  ${ElseIf} $NextSpaceDriverProbeFailed == 1
    StrCpy $NextSpaceDriverState 2
  ${ElseIf} $R1 == 0
    StrCpy $NextSpaceDriverState 0
  ${Else}
    StrCpy $NextSpaceDriverState 2
  ${EndIf}
  Pop $R9
  Pop $R8
  Pop $R7
  Pop $R6
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Pop $R1
  Pop $R0
!macroend

Function NextSpaceClassifyInterception
  !insertmacro NEXTSPACE_CLASSIFY_INTERCEPTION_BODY ""
FunctionEnd
Function un.NextSpaceClassifyInterception
  !insertmacro NEXTSPACE_CLASSIFY_INTERCEPTION_BODY "un."
FunctionEnd

Function NextSpaceAppendRecoveryLog
  Exch $R0
  Push $R1
  FileOpen $R1 "$NextSpaceRecoveryLog" a
  ${IfNot} ${Errors}
    FileWrite $R1 "$R0$\r$\n"
    FileClose $R1
  ${EndIf}
  Pop $R1
  Pop $R0
FunctionEnd

Function NextSpaceValidateConfigBackup
  Exch $R0
  Push $R1
  Push $R2
  Push $R3
  Push $R4
  Push $R5
  StrCpy $R3 0
  StrCpy $R4 0
  StrCpy $R5 0
  ClearErrors
  FileOpen $R1 "$R0" r
  ${If} ${Errors}
    Goto nextspace_config_validate_done
  ${EndIf}
  nextspace_config_validate_loop:
    ClearErrors
    FileReadByte $R1 $R2
    ${If} ${Errors}
      Goto nextspace_config_validate_close
    ${EndIf}
    ${If} $R2 == 9
    ${OrIf} $R2 == 10
    ${OrIf} $R2 == 13
    ${OrIf} $R2 == 32
      Goto nextspace_config_validate_loop
    ${EndIf}
    ${If} $R3 == 0
      StrCpy $R3 $R2
    ${EndIf}
    StrCpy $R4 $R2
    Goto nextspace_config_validate_loop
  nextspace_config_validate_close:
    FileClose $R1
    ${If} $R3 == 123
    ${AndIf} $R4 == 125
      StrCpy $R5 1
    ${EndIf}
    ${If} $R5 == 1
      System::Call 'kernel32::SetEnvironmentVariableW(w "NEXTSPACE_CONFIG_BACKUP", w "$R0")'
      ClearErrors
      StrCpy $R1 -1
      ExecWait '"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe" -NoLogo -NoProfile -NonInteractive -Command "$$ErrorActionPreference = [System.Management.Automation.ActionPreference]::Stop; Get-Content -Raw -LiteralPath $$env:NEXTSPACE_CONFIG_BACKUP | ConvertFrom-Json | Out-Null"' $R1
      StrCpy $R2 0
      ${If} ${Errors}
        StrCpy $R2 1
      ${EndIf}
      System::Call 'kernel32::SetEnvironmentVariableW(w "NEXTSPACE_CONFIG_BACKUP", p 0)'
      ${If} $R2 == 1
        StrCpy $R5 0
      ${ElseIf} $R1 != 0
        StrCpy $R5 0
      ${EndIf}
    ${EndIf}
  nextspace_config_validate_done:
    StrCpy $R0 $R5
    Pop $R5
    Pop $R4
    Pop $R3
    Pop $R2
    Pop $R1
    Exch $R0
FunctionEnd

Function NextSpaceCaptureCleanConfiguration
  Delete "$NextSpaceRollbackDir\config.json"
  ClearErrors
  StrCpy $R0 -1
  ExecWait '"$INSTDIR\veyon-wcli.exe" config export "$NextSpaceRollbackDir\config.json"' $R0
  ${If} ${Errors}
    Abort "Existing retained configuration could not be captured before clean-install settings changed."
  ${ElseIf} $R0 != 0
    Abort "Existing retained configuration export failed before clean-install settings changed."
  ${EndIf}
  Push "$NextSpaceRollbackDir\config.json"
  Call NextSpaceValidateConfigBackup
  Pop $R1
  ${If} $R1 != 1
    Abort "Existing retained configuration backup is not a complete JSON object."
  ${EndIf}
  StrCpy $NextSpaceCleanConfigBackupAvailable 1
FunctionEnd

Function NextSpacePrepareRollback
  ClearErrors
  ReadRegDWORD $NextSpacePriorForceGuestValue HKLM "SYSTEM\CurrentControlSet\Control\Lsa" "ForceGuest"
  ${IfNot} ${Errors}
    StrCpy $NextSpacePriorForceGuestPresent 1
  ${EndIf}
  ${If} $NextSpaceProductState == 0
    IfFileExists "$INSTDIR\*.*" 0 nextspace_clean_dir_ready
      MessageBox MB_OK|MB_ICONSTOP "The clean-install directory is not empty. Choose an empty directory so rollback removes only files created by this attempt." /SD IDOK
      Abort "Clean-install directory is not empty."
    nextspace_clean_dir_ready:
    StrCpy $NextSpaceCleanInstallDirWasEmpty 1
    GetTempFileName $NextSpaceRollbackDir
    Delete "$NextSpaceRollbackDir"
    CreateDirectory "$NextSpaceRollbackDir"
    ${If} ${Errors}
      Abort "Could not create the clean-install rollback directory."
    ${EndIf}
    StrCpy $NextSpaceRecoveryLog "$NextSpaceRollbackDir\recovery.log"
    FileOpen $R0 "$NextSpaceRecoveryLog" w
    ${If} ${Errors}
      Abort "Could not create the clean-install recovery log."
    ${EndIf}
    FileWrite $R0 "GC3-S3 NextSpace clean-install rollback record$\r$\n"
    FileClose $R0
    StrCpy $NextSpaceRollbackReady 1
    Return
  ${EndIf}
  ${If} $NextSpaceProductState != 1
    Abort "Unsupported product state before rollback preparation."
  ${EndIf}

  GetTempFileName $NextSpaceRollbackDir
  Delete "$NextSpaceRollbackDir"
  CreateDirectory "$NextSpaceRollbackDir"
  ${If} ${Errors}
    Abort "Could not create the explicit rollback directory."
  ${EndIf}
  StrCpy $NextSpaceRecoveryLog "$NextSpaceRollbackDir\recovery.log"
  FileOpen $R0 "$NextSpaceRecoveryLog" w
  ${If} ${Errors}
    Abort "Could not create the required recovery log."
  ${EndIf}
  FileWrite $R0 "GC3-S3 NextSpace upgrade rollback record$\r$\n"
  FileClose $R0
  ClearErrors
  ReadRegStr $NextSpacePriorVersion HKLM "${UNINSTALL_PATH}" "DisplayVersion"
  ${If} ${Errors}
    StrCpy $NextSpacePriorVersion "unknown"
  ${EndIf}
  ClearErrors
  ReadRegDWORD $NextSpacePriorServiceStart HKLM "SYSTEM\CurrentControlSet\Services\VeyonService" "Start"
  ${If} ${Errors}
    Abort "Existing service start mode could not be captured safely."
  ${EndIf}
  nsExec::ExecToStack '"$SYSDIR\sc.exe" query "VeyonService"'
  Pop $R0
  Pop $R1
  StrCpy $NextSpacePriorServiceRunning 0
  ${If} $R0 != 0
    Abort "Existing service state could not be captured safely."
  ${EndIf}
  Push "$R1"
  Push "RUNNING"
  Call NextSpaceStrContains
  Pop $NextSpacePriorServiceRunning
  ${If} $NextSpacePriorServiceRunning != 1
    Push "$R1"
    Push "STOPPED"
    Call NextSpaceStrContains
    Pop $R2
    ${If} $R2 != 1
      Abort "Existing service state was unreadable or transitional."
    ${EndIf}
  ${EndIf}
  Delete "$NextSpaceRollbackDir\config.json"
  ClearErrors
  StrCpy $R0 -1
  ExecWait '"$INSTDIR\veyon-wcli.exe" config export "$NextSpaceRollbackDir\config.json"' $R0
  ${If} ${Errors}
    Abort "Existing configuration exporter could not be launched."
  ${ElseIf} $R0 != 0
    Push "config-export=$R0; preparation stopped before mutation"
    Call NextSpaceAppendRecoveryLog
    Abort "Existing configuration export failed."
  ${EndIf}
  Push "$NextSpaceRollbackDir\config.json"
  Call NextSpaceValidateConfigBackup
  Pop $R1
  ${If} $R1 != 1
    Abort "Existing configuration backup is not a complete JSON object."
  ${EndIf}
  CreateDirectory "$NextSpaceRollbackDir\product"
  ClearErrors
  StrCpy $R0 -1
  ExecWait '"$SYSDIR\robocopy.exe" "$INSTDIR" "$NextSpaceRollbackDir\product" /E /COPY:DAT /DCOPY:DAT /R:0 /W:0 /NFL /NDL /NJH /NJS' $R0
  ${If} ${Errors}
    Abort "Existing product backup tool could not be launched."
  ${ElseIf} $R0 >= 8
    Push "backup-robocopy=$R0; preparation stopped before mutation"
    Call NextSpaceAppendRecoveryLog
    Abort "Existing product backup failed."
  ${EndIf}
  IfFileExists "$NextSpaceRollbackDir\product\veyon-wcli.exe" 0 nextspace_backup_incomplete
  IfFileExists "$NextSpaceRollbackDir\product\veyon-service.exe" 0 nextspace_backup_incomplete
  Goto nextspace_backup_verified
  nextspace_backup_incomplete:
    Abort "Existing product backup is incomplete."
  nextspace_backup_verified:
  Push "prior-version=$NextSpacePriorVersion; prior-service-running=$NextSpacePriorServiceRunning; prior-service-start=$NextSpacePriorServiceStart"
  Call NextSpaceAppendRecoveryLog
  Push "backup-robocopy=$R0; config-export=0"
  Call NextSpaceAppendRecoveryLog
  StrCpy $NextSpaceRollbackReady 1
FunctionEnd

Function NextSpaceRollbackInstall
  ${If} $NextSpaceRollbackPerformed == 1
    Return
  ${EndIf}
  StrCpy $NextSpaceRollbackPerformed 1
  StrCpy $NextSpaceRollbackResult 0
  ${If} $NextSpaceRecoveryLog == ""
    StrCpy $NextSpaceRecoveryLog "$TEMP\NextSpace-Interception-Recovery.log"
  ${EndIf}
  Push "rollback-start profile=$NextSpaceProfile product=$NextSpaceProductState driver=$NextSpaceDriverState helper=$NextSpaceHelperResult"
  Call NextSpaceAppendRecoveryLog
  ${If} $NextSpaceHelperInstalledThisSession == 1
    ClearErrors
    StrCpy $NextSpaceUninstallResult -1
    ExecWait '"$PLUGINSDIR\NextSpaceInterception\install-interception.exe" /uninstall' $NextSpaceUninstallResult
    StrCpy $R4 0
    ${If} ${Errors}
      StrCpy $R4 1
    ${EndIf}
    Push "helper-uninstall=$NextSpaceUninstallResult"
    Call NextSpaceAppendRecoveryLog
    ${If} $R4 == 0
    ${AndIf} $NextSpaceUninstallResult == 0
      DeleteRegKey HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}"
      SetRebootFlag true
    ${Else}
      StrCpy $NextSpaceRollbackResult 1
    ${EndIf}
  ${EndIf}

  ${If} $NextSpaceProductState == 1
    ${If} $NextSpaceRollbackReady != 1
      Push "rollback preparation incomplete; product mutation did not start"
      Call NextSpaceAppendRecoveryLog
      Goto nextspace_rollback_finalize
    ${EndIf}
    ${If} $NextSpaceServiceRegisteredThisSession == 1
      ClearErrors
      StrCpy $R0 -1
      ExecWait '"$INSTDIR\veyon-wcli.exe" service stop' $R0
      Push "replacement-service-stop=$R0"
      Call NextSpaceAppendRecoveryLog
      ClearErrors
      StrCpy $R0 -1
      ExecWait '"$INSTDIR\veyon-wcli.exe" service unregister' $R0
      StrCpy $R4 0
      ${If} ${Errors}
        StrCpy $R4 1
      ${EndIf}
      Push "replacement-service-unregister=$R0"
      Call NextSpaceAppendRecoveryLog
      ${If} $R4 == 1
        StrCpy $NextSpaceRollbackResult 1
        Goto nextspace_rollback_finalize
      ${ElseIf} $R0 != 0
        StrCpy $NextSpaceRollbackResult 1
        Goto nextspace_rollback_finalize
      ${EndIf}
      StrCpy $NextSpaceServiceRegisteredThisSession 0
    ${EndIf}
    IfFileExists "$NextSpaceRollbackDir\product\veyon-wcli.exe" 0 nextspace_restore_invalid
    IfFileExists "$NextSpaceRollbackDir\product\veyon-service.exe" 0 nextspace_restore_invalid
    Goto nextspace_restore_valid
    nextspace_restore_invalid:
      StrCpy $NextSpaceRollbackResult 1
      Push "restore backup failed validation"
      Call NextSpaceAppendRecoveryLog
      Goto nextspace_rollback_finalize
    nextspace_restore_valid:
    ClearErrors
    StrCpy $R0 -1
    ExecWait '"$SYSDIR\robocopy.exe" "$NextSpaceRollbackDir\product" "$INSTDIR" /MIR /COPY:DAT /DCOPY:DAT /R:0 /W:0 /NFL /NDL /NJH /NJS' $R0
    StrCpy $R4 0
    ${If} ${Errors}
      StrCpy $R4 1
    ${EndIf}
    Push "restore-robocopy=$R0"
    Call NextSpaceAppendRecoveryLog
    StrCpy $R6 1
    ${If} $R4 == 1
      StrCpy $NextSpaceRollbackResult 1
      StrCpy $R6 0
    ${ElseIf} $R0 >= 8
      StrCpy $NextSpaceRollbackResult 1
      StrCpy $R6 0
    ${EndIf}
    ${If} $R6 == 1
    ${AndIf} $NextSpacePriorServiceRemoved == 1
      ClearErrors
      StrCpy $R2 -1
      ExecWait '"$INSTDIR\veyon-wcli.exe" service register' $R2
      StrCpy $R4 0
      ${If} ${Errors}
        StrCpy $R4 1
      ${EndIf}
      Push "service-register=$R2"
      Call NextSpaceAppendRecoveryLog
      ${If} $R4 == 1
        StrCpy $NextSpaceRollbackResult 1
        StrCpy $R6 0
      ${ElseIf} $R2 != 0
        StrCpy $NextSpaceRollbackResult 1
        StrCpy $R6 0
      ${Else}
        nsExec::ExecToStack '"$SYSDIR\sc.exe" query "VeyonService"'
        Pop $R3
        Pop $R5
        ${If} $R3 != 0
          StrCpy $NextSpaceRollbackResult 1
          StrCpy $R6 0
        ${EndIf}
      ${EndIf}
    ${EndIf}
    Push "$NextSpaceRollbackDir\config.json"
    Call NextSpaceValidateConfigBackup
    Pop $R5
    ${If} $R5 != 1
      StrCpy $NextSpaceRollbackResult 1
      StrCpy $R6 0
    ${EndIf}
    ${If} $R6 == 1
      ClearErrors
      StrCpy $R1 -1
      ExecWait '"$INSTDIR\veyon-wcli.exe" config clear' $R1
      StrCpy $R4 0
      ${If} ${Errors}
        StrCpy $R4 1
      ${EndIf}
      Push "config-clear=$R1"
      Call NextSpaceAppendRecoveryLog
      ${If} $R4 == 1
        StrCpy $NextSpaceRollbackResult 1
        StrCpy $R6 0
      ${ElseIf} $R1 != 0
        StrCpy $NextSpaceRollbackResult 1
        StrCpy $R6 0
      ${EndIf}
    ${EndIf}
    ${If} $R6 == 1
      ClearErrors
      StrCpy $R1 -1
      ExecWait '"$INSTDIR\veyon-wcli.exe" config import "$NextSpaceRollbackDir\config.json"' $R1
      StrCpy $R4 0
      ${If} ${Errors}
        StrCpy $R4 1
      ${EndIf}
      Push "config-import=$R1"
      Call NextSpaceAppendRecoveryLog
      ${If} $R4 == 1
        StrCpy $NextSpaceRollbackResult 1
        StrCpy $R6 0
      ${ElseIf} $R1 != 0
        StrCpy $NextSpaceRollbackResult 1
        StrCpy $R6 0
      ${EndIf}
    ${EndIf}
    ${If} $NextSpacePriorServiceStart != ""
      ClearErrors
      WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Services\VeyonService" "Start" $NextSpacePriorServiceStart
      ${If} ${Errors}
        StrCpy $NextSpaceRollbackResult 1
      ${EndIf}
    ${EndIf}
    ${If} $NextSpacePriorServiceRunning == 1
      ClearErrors
      StrCpy $R3 -1
      ExecWait '"$INSTDIR\veyon-wcli.exe" service start' $R3
      StrCpy $R4 0
      ${If} ${Errors}
        StrCpy $R4 1
      ${EndIf}
      Push "service-start=$R3"
      Call NextSpaceAppendRecoveryLog
      ${If} $R4 == 1
        StrCpy $NextSpaceRollbackResult 1
      ${ElseIf} $R3 != 0
        StrCpy $NextSpaceRollbackResult 1
      ${Else}
        nsExec::ExecToStack '"$SYSDIR\sc.exe" query "VeyonService"'
        Pop $R4
        Pop $R5
        Push "$R5"
        Push "RUNNING"
        Call NextSpaceStrContains
        Pop $R5
        ${If} $R4 != 0
        ${OrIf} $R5 != 1
          StrCpy $NextSpaceRollbackResult 1
        ${EndIf}
      ${EndIf}
    ${EndIf}
  ${ElseIf} $NextSpaceCleanInstallDirWasEmpty == 1
  ${AndIf} $NextSpaceProductMutationStarted == 1
    ${If} $NextSpaceConfigMutatedThisSession == 1
      ${If} $NextSpaceCleanConfigBackupAvailable != 1
        StrCpy $NextSpaceRollbackResult 1
      ${Else}
        Push "$NextSpaceRollbackDir\config.json"
        Call NextSpaceValidateConfigBackup
        Pop $R6
        ${If} $R6 != 1
          StrCpy $NextSpaceRollbackResult 1
        ${Else}
          ClearErrors
          StrCpy $R0 -1
          ExecWait '"$INSTDIR\veyon-wcli.exe" config clear' $R0
          ${If} ${Errors}
          ${OrIf} $R0 != 0
            StrCpy $NextSpaceRollbackResult 1
            StrCpy $R6 0
          ${EndIf}
          ${If} $R6 == 1
            ClearErrors
            StrCpy $R0 -1
            ExecWait '"$INSTDIR\veyon-wcli.exe" config import "$NextSpaceRollbackDir\config.json"' $R0
            ${If} ${Errors}
            ${OrIf} $R0 != 0
              StrCpy $NextSpaceRollbackResult 1
            ${EndIf}
          ${EndIf}
        ${EndIf}
      ${EndIf}
    ${EndIf}
    ${If} $NextSpaceServiceRegisteredThisSession == 1
      ClearErrors
      StrCpy $R0 -1
      ExecWait '"$INSTDIR\veyon-wcli.exe" service unregister' $R0
      ${If} ${Errors}
      ${OrIf} $R0 != 0
        StrCpy $NextSpaceRollbackResult 1
      ${EndIf}
    ${EndIf}
    RmDir /r "$INSTDIR"
  ${ElseIf} $NextSpaceCleanInstallDirWasEmpty == 1
    DetailPrint "Rollback skipped: no product mutation had started."
  ${Else}
    StrCpy $NextSpaceRollbackResult 1
  ${EndIf}
  nextspace_rollback_finalize:
  ${If} $NextSpaceForceGuestMutated == 1
    ClearErrors
    ${If} $NextSpacePriorForceGuestPresent == 1
      WriteRegDWORD HKLM "SYSTEM\CurrentControlSet\Control\Lsa" "ForceGuest" $NextSpacePriorForceGuestValue
    ${Else}
      DeleteRegValue HKLM "SYSTEM\CurrentControlSet\Control\Lsa" "ForceGuest"
    ${EndIf}
    ${If} ${Errors}
      StrCpy $NextSpaceRollbackResult 1
    ${EndIf}
  ${EndIf}
  Push "rollback-result=$NextSpaceRollbackResult; ownership marker retained if helper uninstall failed"
  Call NextSpaceAppendRecoveryLog
  ${If} $NextSpaceRollbackResult == 0
    DetailPrint "Rollback completed; recovery log: $NextSpaceRecoveryLog"
  ${Else}
    DetailPrint "Rollback INCOMPLETE; technician recovery required: $NextSpaceRecoveryLog"
  ${EndIf}
FunctionEnd

Function NextSpaceInstallInterception
  ${IfNot} ${SectionIsSelected} ${SEC_INTERCEPTION}
    Return
  ${EndIf}
  ${If} $NextSpaceDriverState == 3
    Abort "Conflicting NextSpace Interception ownership state."
  ${ElseIf} $NextSpaceDriverState == 1
    DetailPrint "Exact NextSpace-managed Interception footprint found; verification remains pending."
    ClearErrors
    WriteRegDWORD HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "PendingVerification" 1
    WriteRegDWORD HKLM "${NEXTSPACE_INTERCEPTION_VERIFICATION_KEY}" "InterceptionPendingVerification" 1
    ${If} ${Errors}
      Abort "Interception verification state could not be persisted."
    ${EndIf}
    SetRebootFlag true
  ${ElseIf} $NextSpaceDriverState == 2
    DetailPrint "External or unknown Interception footprint preserved; no ownership marker written."
    ClearErrors
    WriteRegDWORD HKLM "${NEXTSPACE_INTERCEPTION_VERIFICATION_KEY}" "ExternalInterceptionCandidate" 1
    WriteRegDWORD HKLM "${NEXTSPACE_INTERCEPTION_VERIFICATION_KEY}" "InterceptionPendingVerification" 1
    ${If} ${Errors}
      Abort "External Interception verification state could not be persisted."
    ${EndIf}
    SetRebootFlag true
  ${ElseIf} $NextSpaceDriverState == 0
    ClearErrors
    StrCpy $NextSpaceHelperResult -1
    ExecWait '"$PLUGINSDIR\NextSpaceInterception\install-interception.exe" /install' $NextSpaceHelperResult
    DetailPrint "Interception helper /install exit code: $NextSpaceHelperResult"
    ${IfNot} ${Errors}
    ${AndIf} $NextSpaceHelperResult == 0
      StrCpy $NextSpaceHelperInstalledThisSession 1
      ClearErrors
      WriteRegDWORD HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "Managed" 1
      WriteRegStr HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "PayloadSha256" "${NEXTSPACE_INTERCEPTION_HELPER_SHA256}"
      WriteRegStr HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "InstalledByVersion" "${NEXTSPACE_INTERCEPTION_VERSION}"
      WriteRegDWORD HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}" "PendingVerification" 1
      WriteRegDWORD HKLM "${NEXTSPACE_INTERCEPTION_VERIFICATION_KEY}" "InterceptionPendingVerification" 1
      ${If} ${Errors}
        Call NextSpaceRollbackInstall
        Abort "Interception ownership marker write failed."
      ${EndIf}
      SetRebootFlag true
      DetailPrint "Pending reboot and verification; not classroom-ready."
    ${ElseIf} $NextSpaceProfile == 0
      DetailPrint "Teacher workstation continuing without optional Interception."
      !insertmacro UnselectSection ${SEC_INTERCEPTION}
    ${Else}
      Call NextSpaceRollbackInstall
      Abort "Required Student Interception helper failed; rollback result=$NextSpaceRollbackResult."
    ${EndIf}
  ${EndIf}
  DetailPrint "GC3-S3 state: profile=$NextSpaceProfile product=$NextSpaceProductState driver=$NextSpaceDriverState canonical=${NEXTSPACE_INTERCEPTION_HELPER_SHA256} helper=$NextSpaceHelperResult rollback=$NextSpaceRollbackResult pending-verification=required-if-selected."
FunctionEnd

Function un.NextSpaceGuardedUninstallInterception
  Call un.NextSpaceClassifyInterception
  ${If} $NextSpaceDriverState == 1
    IfFileExists "$INSTDIR\interception\install-interception.exe" 0 nextspace_uninstall_helper_missing
    ClearErrors
    FileOpen $R0 "$INSTDIR\interception\install-interception.exe" r
    ${If} ${Errors}
      Abort "Managed Interception helper could not be read; product files and marker preserved."
    ${EndIf}
    FileSeek $R0 0 END $R1
    FileClose $R0
    ${If} $R1 != 470528
      Abort "Managed Interception helper size mismatch; product files and marker preserved."
    ${EndIf}
    Push "$INSTDIR\interception\install-interception.exe"
    Push "${NEXTSPACE_INTERCEPTION_HELPER_SHA256}"
    Call un.NextSpaceHashFileMatches
    Pop $R2
    ${If} $R2 != 1
      Abort "Managed Interception helper hash mismatch; product files and marker preserved."
    ${EndIf}
    ClearErrors
    StrCpy $NextSpaceUninstallResult -1
    ExecWait '"$INSTDIR\interception\install-interception.exe" /uninstall' $NextSpaceUninstallResult
    DetailPrint "Managed Interception helper /uninstall exit code: $NextSpaceUninstallResult"
    ${IfNot} ${Errors}
    ${AndIf} $NextSpaceUninstallResult == 0
      ClearErrors
      DeleteRegKey HKLM "${NEXTSPACE_INTERCEPTION_MARKER_KEY}"
      ${If} ${Errors}
        Abort "Driver removed but ownership marker cleanup failed; technician inspection required."
      ${EndIf}
      SetRebootFlag true
      Return
    ${EndIf}
    Abort "Managed Interception uninstall failed; product files and marker preserved."
    nextspace_uninstall_helper_missing:
      Abort "Managed Interception helper missing; product files and marker preserved."
  ${ElseIf} $NextSpaceDriverState == 0
    DetailPrint "No Interception footprint or ownership marker found."
  ${ElseIf} $NextSpaceDriverState == 2
    DetailPrint "External or unknown Interception footprint preserved; /uninstall not invoked."
  ${Else}
    DetailPrint "Conflicting NextSpace marker preserved; /uninstall not invoked."
  ${EndIf}
FunctionEnd

!endif
