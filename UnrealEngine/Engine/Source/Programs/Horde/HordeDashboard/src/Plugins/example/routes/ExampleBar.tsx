// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';
import { MessageBar, MessageBarType } from '@fluentui/react';

export const ExampleBar: React.FC = () => {
    return (
        <MessageBar messageBarType={MessageBarType.info} isMultiline={false} > Hello from Example Plugin </MessageBar>
    );
};