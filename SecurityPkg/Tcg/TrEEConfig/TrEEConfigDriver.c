/** @file
  The module entry point for TrEE configuration module.

Copyright (c) 2013, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials 
are licensed and made available under the terms and conditions of the BSD License 
which accompanies this distribution.  The full text of the license may be found at 
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS, 
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "TrEEConfigImpl.h"

extern TPM_INSTANCE_ID  mTpmInstanceId[TPM_DEVICE_MAX + 1];

/**
  The entry point for TrEE configuration driver.

  @param[in]  ImageHandle        The image handle of the driver.
  @param[in]  SystemTable        The system table.

  @retval EFI_ALREADY_STARTED    The driver already exists in system.
  @retval EFI_OUT_OF_RESOURCES   Fail to execute entry point due to lack of resources.
  @retval EFI_SUCCES             All the related protocols are installed on the driver.
  @retval Others                 Fail to install protocols as indicated.

**/
EFI_STATUS
EFIAPI
TrEEConfigDriverEntryPoint (
  IN EFI_HANDLE          ImageHandle,
  IN EFI_SYSTEM_TABLE    *SystemTable
  )
{
  EFI_STATUS                    Status;
  TREE_CONFIG_PRIVATE_DATA      *PrivateData;
  TREE_CONFIGURATION            TrEEConfiguration;
  UINTN                         Index;
  UINTN                         DataSize;

  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiCallerIdGuid,
                  NULL,
                  ImageHandle,
                  ImageHandle,
                  EFI_OPEN_PROTOCOL_TEST_PROTOCOL
                  );
  if (!EFI_ERROR (Status)) {
    return EFI_ALREADY_STARTED;
  }
  
  //
  // Create a private data structure.
  //
  PrivateData = AllocateCopyPool (sizeof (TREE_CONFIG_PRIVATE_DATA), &mTrEEConfigPrivateDateTemplate);
  ASSERT (PrivateData != NULL);

  //
  // Install private GUID.
  //    
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiCallerIdGuid,
                  PrivateData,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  DataSize = sizeof(TrEEConfiguration);
  Status = gRT->GetVariable (
                  TREE_STORAGE_NAME,
                  &gTrEEConfigFormSetGuid,
                  NULL,
                  &DataSize,
                  &TrEEConfiguration
                  );
  if (EFI_ERROR (Status)) {
  }
  //
  // We should always reinit PP request.
  //
  TrEEConfiguration.Tpm2Operation = TREE_PHYSICAL_PRESENCE_NO_ACTION;

  //
  // Sync data from PCD to variable, so that we do not need detect again in S3 phase.
  //

  //
  // Get data from PCD to make sure data consistant - platform driver is suppose to construct this PCD accroding to Variable
  //
  for (Index = 0; Index < sizeof(mTpmInstanceId)/sizeof(mTpmInstanceId[0]); Index++) {
    if (CompareGuid (PcdGetPtr(PcdTpmInstanceGuid), &mTpmInstanceId[Index].TpmInstanceGuid)) {
      TrEEConfiguration.TpmDevice = mTpmInstanceId[Index].TpmDevice;
      break;
    }
  }

  //
  // Save to variable so platform driver can get it.
  //
  Status = gRT->SetVariable (
                  TREE_STORAGE_NAME,
                  &gTrEEConfigFormSetGuid,
                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                  sizeof(TrEEConfiguration),
                  &TrEEConfiguration
                  );
  ASSERT_EFI_ERROR (Status);
  
  //
  // Install TrEE configuration form
  //
  Status = InstallTrEEConfigForm (PrivateData);
  if (EFI_ERROR (Status)) {
    goto ErrorExit;
  }

  return EFI_SUCCESS;

ErrorExit:
  if (PrivateData != NULL) {
    UninstallTrEEConfigForm (PrivateData);
  }  
  
  return Status;
}

/**
  Unload the TrEE configuration form.

  @param[in]  ImageHandle         The driver's image handle.

  @retval     EFI_SUCCESS         The TrEE configuration form is unloaded.
  @retval     Others              Failed to unload the form.

**/
EFI_STATUS
EFIAPI
TrEEConfigDriverUnload (
  IN EFI_HANDLE  ImageHandle
  )
{
  EFI_STATUS                  Status;
  TREE_CONFIG_PRIVATE_DATA    *PrivateData;

  Status = gBS->HandleProtocol (
                  ImageHandle,
                  &gEfiCallerIdGuid,
                  (VOID **) &PrivateData
                  );  
  if (EFI_ERROR (Status)) {
    return Status;  
  }
  
  ASSERT (PrivateData->Signature == TREE_CONFIG_PRIVATE_DATA_SIGNATURE);

  gBS->UninstallMultipleProtocolInterfaces (
         &ImageHandle,
         &gEfiCallerIdGuid,
         PrivateData,
         NULL
         );
  
  UninstallTrEEConfigForm (PrivateData);

  return EFI_SUCCESS;
}
