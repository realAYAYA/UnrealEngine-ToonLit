// Copyright Epic Games, Inc. All Rights Reserved.

import { Dialog, PrimaryButton, DialogFooter, DefaultButton, DialogType, TextField } from '@fluentui/react';
import React, { useState } from 'react';

export const ConfirmationDialog: React.FC<{ isOpen: boolean; title: string; cancelText: string; confirmText: string; subText?: string; isTextBoxSpawned?: boolean; textBoxLabel?: string; onConfirm: (...args: any[]) => any; onCancel: (...args: any[]) => any }> = (({ isOpen, title, cancelText, confirmText, subText, isTextBoxSpawned, textBoxLabel, onConfirm, onCancel }) => {
  const [textFieldText, setTextFieldText] = useState<string|undefined>(undefined);
  
  return (
    <Dialog
      hidden={!isOpen}
      onDismiss={onCancel}
      dialogContentProps={{
        type: DialogType.normal,
        title: title,
        subText: subText
      }}
      modalProps={{
        isBlocking: true,
        styles: { main: { maxWidth: 450, minHeight: 160 } },
      }}
    >
      {isTextBoxSpawned && <TextField label={textBoxLabel} value={textFieldText} onChange={(event, value) => setTextFieldText(value)}></TextField>}
      <DialogFooter>
        <PrimaryButton onClick={() => { onConfirm(textFieldText); setTextFieldText(undefined); }} text={confirmText} />
        <DefaultButton onClick={() => { onCancel(textFieldText); setTextFieldText(undefined); }} text={cancelText} />
      </DialogFooter>
    </Dialog>
	);
});