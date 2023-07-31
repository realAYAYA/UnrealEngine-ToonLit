// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';
import { MessageBar, MessageBarType } from '@fluentui/react';


export const ErrorBar: React.FC<{ message: string }> = ({ message }) => {
    return (
        <MessageBar messageBarType={MessageBarType.error} isMultiline={false} > {message} </MessageBar>
    );
};