/** @file
  Graphics Output Protocol functions for the QEMU video controller.

  Copyright (c) 2007 - 2010, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution. The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Qemu.h"
#include <IndustryStandard/Acpi.h>
#include <Library/BltLib.h>

STATIC
VOID
QemuVideoCompleteModeInfo (
  IN  QEMU_VIDEO_MODE_DATA           *ModeData,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info
  )
{
  Info->Version = 0;
  if (ModeData->ColorDepth == 8) {
    Info->PixelFormat = PixelBitMask;
    Info->PixelInformation.RedMask = PIXEL_RED_MASK;
    Info->PixelInformation.GreenMask = PIXEL_GREEN_MASK;
    Info->PixelInformation.BlueMask = PIXEL_BLUE_MASK;
    Info->PixelInformation.ReservedMask = 0;
  } else if (ModeData->ColorDepth == 24) {
    Info->PixelFormat = PixelBitMask;
    Info->PixelInformation.RedMask = PIXEL24_RED_MASK;
    Info->PixelInformation.GreenMask = PIXEL24_GREEN_MASK;
    Info->PixelInformation.BlueMask = PIXEL24_BLUE_MASK;
    Info->PixelInformation.ReservedMask = 0;
  } else if (ModeData->ColorDepth == 32) {
    DEBUG ((EFI_D_INFO, "PixelBlueGreenRedReserved8BitPerColor\n"));
    Info->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  }
  Info->PixelsPerScanLine = Info->HorizontalResolution;
}


STATIC
EFI_STATUS
QemuVideoCompleteModeData (
  IN  QEMU_VIDEO_PRIVATE_DATA           *Private,
  OUT EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode
  )
{
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  *Info;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR     *FrameBufDesc;
  QEMU_VIDEO_MODE_DATA           *ModeData;

  ModeData = &Private->ModeData[Mode->Mode];
  Info = Mode->Info;
  QemuVideoCompleteModeInfo (ModeData, Info);

  Private->PciIo->GetBarAttributes (
                        Private->PciIo,
                        0,
                        NULL,
                        (VOID**) &FrameBufDesc
                        );

  Mode->FrameBufferBase = FrameBufDesc->AddrRangeMin;
  Mode->FrameBufferSize = Info->HorizontalResolution * Info->VerticalResolution;
  Mode->FrameBufferSize = Mode->FrameBufferSize * ((ModeData->ColorDepth + 7) / 8);
  DEBUG ((EFI_D_INFO, "FrameBufferBase: 0x%x, FrameBufferSize: 0x%x\n", Mode->FrameBufferBase, Mode->FrameBufferSize));

  FreePool (FrameBufDesc);
  return EFI_SUCCESS;
}


//
// Graphics Output Protocol Member Functions
//
EFI_STATUS
EFIAPI
QemuVideoGraphicsOutputQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
/*++

Routine Description:

  Graphics Output protocol interface to query video mode

  Arguments:
    This                  - Protocol instance pointer.
    ModeNumber            - The mode number to return information on.
    Info                  - Caller allocated buffer that returns information about ModeNumber.
    SizeOfInfo            - A pointer to the size, in bytes, of the Info buffer.

  Returns:
    EFI_SUCCESS           - Mode information returned.
    EFI_BUFFER_TOO_SMALL  - The Info buffer was too small.
    EFI_DEVICE_ERROR      - A hardware error occurred trying to retrieve the video mode.
    EFI_NOT_STARTED       - Video display is not initialized. Call SetMode ()
    EFI_INVALID_PARAMETER - One of the input args was NULL.

--*/
{
  QEMU_VIDEO_PRIVATE_DATA  *Private;
  QEMU_VIDEO_MODE_DATA     *ModeData;

  Private = QEMU_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (This);

  if (Private->HardwareNeedsStarting) {
    return EFI_NOT_STARTED;
  }

  if (Info == NULL || SizeOfInfo == NULL || ModeNumber >= This->Mode->MaxMode) {
    return EFI_INVALID_PARAMETER;
  }

  *Info = AllocatePool (sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  ModeData = &Private->ModeData[ModeNumber];
  (*Info)->HorizontalResolution = ModeData->HorizontalResolution;
  (*Info)->VerticalResolution   = ModeData->VerticalResolution;
  QemuVideoCompleteModeInfo (ModeData, *Info);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
QemuVideoGraphicsOutputSetMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
  IN  UINT32                       ModeNumber
  )
/*++

Routine Description:

  Graphics Output protocol interface to set video mode

  Arguments:
    This             - Protocol instance pointer.
    ModeNumber       - The mode number to be set.

  Returns:
    EFI_SUCCESS      - Graphics mode was changed.
    EFI_DEVICE_ERROR - The device had an error and could not complete the request.
    EFI_UNSUPPORTED  - ModeNumber is not supported by this device.

--*/
{
  QEMU_VIDEO_PRIVATE_DATA    *Private;
  QEMU_VIDEO_MODE_DATA       *ModeData;
//  UINTN                             Count;

  Private = QEMU_VIDEO_PRIVATE_DATA_FROM_GRAPHICS_OUTPUT_THIS (This);

  if (ModeNumber >= This->Mode->MaxMode) {
    return EFI_UNSUPPORTED;
  }

  ModeData = &Private->ModeData[ModeNumber];

  if (Private->LineBuffer) {
    gBS->FreePool (Private->LineBuffer);
  }

  Private->LineBuffer = NULL;
  Private->LineBuffer = AllocatePool (4 * ModeData->HorizontalResolution);
  if (Private->LineBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  switch (Private->Variant) {
  case QEMU_VIDEO_CIRRUS_5430:
  case QEMU_VIDEO_CIRRUS_5446:
    InitializeCirrusGraphicsMode (Private, &QemuVideoCirrusModes[ModeData->ModeNumber]);
    break;
  case QEMU_VIDEO_BOCHS_MMIO:
  case QEMU_VIDEO_BOCHS:
    InitializeBochsGraphicsMode (Private, &QemuVideoBochsModes[ModeData->ModeNumber]);
    break;
  default:
    ASSERT (FALSE);
    gBS->FreePool (Private->LineBuffer);
    Private->LineBuffer = NULL;
    return EFI_DEVICE_ERROR;
  }

  This->Mode->Mode = ModeNumber;
  This->Mode->Info->HorizontalResolution = ModeData->HorizontalResolution;
  This->Mode->Info->VerticalResolution = ModeData->VerticalResolution;
  This->Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);

  QemuVideoCompleteModeData (Private, This->Mode);

  BltLibConfigure (
    (VOID*)(UINTN) This->Mode->FrameBufferBase,
    This->Mode->Info
    );

  Private->HardwareNeedsStarting  = FALSE;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
QemuVideoGraphicsOutputBlt (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  EFI_GRAPHICS_OUTPUT_BLT_PIXEL         *BltBuffer, OPTIONAL
  IN  EFI_GRAPHICS_OUTPUT_BLT_OPERATION     BltOperation,
  IN  UINTN                                 SourceX,
  IN  UINTN                                 SourceY,
  IN  UINTN                                 DestinationX,
  IN  UINTN                                 DestinationY,
  IN  UINTN                                 Width,
  IN  UINTN                                 Height,
  IN  UINTN                                 Delta
  )
/*++

Routine Description:

  Graphics Output protocol instance to block transfer for CirrusLogic device

Arguments:

  This          - Pointer to Graphics Output protocol instance
  BltBuffer     - The data to transfer to screen
  BltOperation  - The operation to perform
  SourceX       - The X coordinate of the source for BltOperation
  SourceY       - The Y coordinate of the source for BltOperation
  DestinationX  - The X coordinate of the destination for BltOperation
  DestinationY  - The Y coordinate of the destination for BltOperation
  Width         - The width of a rectangle in the blt rectangle in pixels
  Height        - The height of a rectangle in the blt rectangle in pixels
  Delta         - Not used for EfiBltVideoFill and EfiBltVideoToVideo operation.
                  If a Delta of 0 is used, the entire BltBuffer will be operated on.
                  If a subrectangle of the BltBuffer is used, then Delta represents
                  the number of bytes in a row of the BltBuffer.

Returns:

  EFI_INVALID_PARAMETER - Invalid parameter passed in
  EFI_SUCCESS - Blt operation success

--*/
{
  EFI_STATUS                      Status;
  EFI_TPL                         OriginalTPL;

  //
  // We have to raise to TPL Notify, so we make an atomic write the frame buffer.
  // We would not want a timer based event (Cursor, ...) to come in while we are
  // doing this operation.
  //
  OriginalTPL = gBS->RaiseTPL (TPL_NOTIFY);

  switch (BltOperation) {
  case EfiBltVideoToBltBuffer:
  case EfiBltBufferToVideo:
  case EfiBltVideoFill:
  case EfiBltVideoToVideo:
    Status = BltLibGopBlt (
      BltBuffer,
      BltOperation,
      SourceX,
      SourceY,
      DestinationX,
      DestinationY,
      Width,
      Height,
      Delta
      );
    break;

  default:
    Status = EFI_INVALID_PARAMETER;
    ASSERT (FALSE);
  }

  gBS->RestoreTPL (OriginalTPL);

  return Status;
}

EFI_STATUS
QemuVideoGraphicsOutputConstructor (
  QEMU_VIDEO_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS                   Status;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *GraphicsOutput;


  GraphicsOutput            = &Private->GraphicsOutput;
  GraphicsOutput->QueryMode = QemuVideoGraphicsOutputQueryMode;
  GraphicsOutput->SetMode   = QemuVideoGraphicsOutputSetMode;
  GraphicsOutput->Blt       = QemuVideoGraphicsOutputBlt;

  //
  // Initialize the private data
  //
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE),
                  (VOID **) &Private->GraphicsOutput.Mode
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Status = gBS->AllocatePool (
                  EfiBootServicesData,
                  sizeof (EFI_GRAPHICS_OUTPUT_MODE_INFORMATION),
                  (VOID **) &Private->GraphicsOutput.Mode->Info
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  Private->GraphicsOutput.Mode->MaxMode = (UINT32) Private->MaxMode;
  Private->GraphicsOutput.Mode->Mode    = GRAPHICS_OUTPUT_INVALIDE_MODE_NUMBER;
  Private->HardwareNeedsStarting        = TRUE;
  Private->LineBuffer                   = NULL;

  //
  // Initialize the hardware
  //
  GraphicsOutput->SetMode (GraphicsOutput, 0);
  DrawLogo (
    Private,
    Private->ModeData[Private->GraphicsOutput.Mode->Mode].HorizontalResolution,
    Private->ModeData[Private->GraphicsOutput.Mode->Mode].VerticalResolution
    );

  return EFI_SUCCESS;
}

EFI_STATUS
QemuVideoGraphicsOutputDestructor (
  QEMU_VIDEO_PRIVATE_DATA  *Private
  )
/*++

Routine Description:

Arguments:

Returns:

  None

--*/
{
  if (Private->GraphicsOutput.Mode != NULL) {
    if (Private->GraphicsOutput.Mode->Info != NULL) {
      gBS->FreePool (Private->GraphicsOutput.Mode->Info);
    }
    gBS->FreePool (Private->GraphicsOutput.Mode);
  }

  return EFI_SUCCESS;
}


