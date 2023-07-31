// Copyright Epic Games, Inc. All Rights Reserved.

import { Image, Stack, Text } from '@fluentui/react';
import React from 'react';

export const UnderConstruction: React.FC = () => {

    return (<div style={{ position: 'absolute', left: '50%', top: '50%', transform: 'translate(-50%, -50%)' }}>
        <Stack horizontalAlign="center" styles={{ root: { padding: 20, minWidth: 200, minHeight: 100 } }}>
            <Stack horizontal>
                <Stack styles={{ root: { paddingTop: 2, paddingRight: 6 } }}>
                    <Image shouldFadeIn={false} shouldStartVisible={true} width={48} src="/images/horde.svg" />
                </Stack>
                <Stack styles={{ root: { paddingTop: 12 } }}>
                    <Text styles={{ root: { fontFamily: "Horde Raleway Bold", fontSize: 24 } }}>HORDE</Text>
                </Stack>
            </Stack>
            <Text variant="xLarge" styles={{ root: { paddingLeft: 16, paddingTop:10, fontFamily:"Horde Open Sans SemiBold" } }} >Under Construction</Text>
        </Stack>
    </div>);
};