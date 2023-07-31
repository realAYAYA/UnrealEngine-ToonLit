// Copyright Epic Games, Inc. All Rights Reserved.

import React from 'react';

export const PrintException: React.FC<{ message: string }> = ({ message }) => {
    return (
        <div style={{whiteSpace:"pre"}}> {message} </div>
    );
};