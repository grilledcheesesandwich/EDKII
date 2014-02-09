/*
    Terence has to do:
    1. mark goto WAIT: segment.
    2. added SystemTable->BootServices->Stall(1000000);
    The result is non-error.
*/
#include "EfiShellLib.h"
#include "CMOSRTC.h"
#include "IoAccess.h"
#include "CpuIo.h"
#include "XtuOemCmosDefine.h"


#include STRING_DEFINES_FILE
extern UINT8 STRING_ARRAY_NAME[];

extern EFI_GUID gEfiCpuIoProtocolGuid;

#define CMOS_INDEX              0x70
#define CMOS_DATA               0x71
#define CMOS_EXINDEX            0x72
#define CMOS_EXDATA             0x73
#define CMOS_ADDRESS_MAXIMUM    16
#define CMOS_ROW_MAXIMUM        16

volatile UINT8 	dataSource[CMOS_ADDRESS_MAXIMUM];
volatile UINT8   readData = 0, i = 0, j = 0;
volatile UINT8    oldData = 0;

EFI_GUID                CMOSRTCGuid = CMOSRTC_STRING_PACK_GUID;  

// VOID CMOSRTCIoAccess(VOID);

#ifdef EFI_BOOTSHELL
EFI_APPLICATION_ENTRY_POINT (CMOSRTC_Main)
#endif
// #pragma optimize("", off)
EFI_STATUS 
EFIAPI 
CMOSRTC_Main (
IN EFI_HANDLE ImageHandle, 
IN EFI_SYSTEM_TABLE *SystemTable
)
{
    EFI_HII_HANDLE          HiiHandle;
    EFI_CPU_IO_PROTOCOL     *mCpuIo;
    EFI_STATUS              CpuStatus;
    
	EFI_EVENT               KeyEvent;
	EFI_INPUT_KEY           Key;
    
     // UINT8   j=0;
    // UINT8 i=0;
    // CHAR8 prev;
    
    EFI_SHELL_APP_INIT(ImageHandle ,SystemTable);
    EFI_SHELL_STR_INIT(HiiHandle, STRING_ARRAY_NAME, CMOSRTCGuid);

    
    CpuStatus = SystemTable->BootServices->LocateProtocol(&gEfiCpuIoProtocolGuid, NULL, (VOID**) &mCpuIo);
    if (EFI_ERROR(CpuStatus)) {
        Print(L"Unable to locate CPU I/O protocol.\n");
    }


    do {
        for( i = 0; i < CMOS_ADDRESS_MAXIMUM ; i++ )
		{
			IoWrite8(CMOS_INDEX,i);
            
			readData=IoRead8(CMOS_DATA);
			dataSource[i] = readData;
            
			if (i == 0) {
				if (readData == oldData) {
                //Print(L"");
                   // goto WAIT;
				}
				else {
                    oldData = readData;
				}
			}
		}
        for( j = 0 ; j < CMOS_ROW_MAXIMUM; j++ )
        {
            Print(L"%02x  ",dataSource[j]);
            
            // 1425
            //dataSource[k*CMOS_ROW_MAXIMUM+j]=0;
            // 1425
        }
        Print(L"\n");
        
        SystemTable->BootServices->Stall(1*1000*1000);
// WAIT:        
        KeyEvent = SystemTable->ConIn->WaitForKey;  
        SystemTable->ConIn->ReadKeyStroke(SystemTable->ConIn, &Key); 
        if (Key.ScanCode != 0x00){
            Print(L"     ============SCAN CODE: 0x%02x================\n",Key.ScanCode);
        }
    } while(Key.ScanCode != SCAN_ESC);
     // }
    //you can get EFI_CPU_STATUS if mCpuIo cast to EFI_CPU_IO_PROTOCOL
    // CpuIoStatus = ((EFI_CPU_IO_PROTOCOL *)mCpuIo)->Io.Read(
                                                    // ((EFI_CPU_IO_PROTOCOL *)mCpuIo),
                                                    // EfiCpuIoWidthUint8,
                                                    // CMOS_DATA,
                                                    // 1,
                                                    // &readData
                                                    // );
    
   
    LibUnInitializeStrings();
    return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
InitializeCMOSRTCGetLineHelp (
  OUT CHAR16              **Str
  )
{
  return LibCmdGetStringByToken (STRING_ARRAY_NAME, &CMOSRTCGuid, STRING_TOKEN (STR_CMOSRTC_LINE_HELP), Str);
}