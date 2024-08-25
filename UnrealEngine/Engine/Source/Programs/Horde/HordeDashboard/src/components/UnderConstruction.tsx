// Copyright Epic Games, Inc. All Rights Reserved.

import { Stack, Text } from '@fluentui/react';
import React from 'react';
import { useWindowSize } from '../base/utilities/hooks';
import { getHordeStyling } from '../styles/Styles';
import { Breadcrumbs } from './Breadcrumbs';
import { TopNav } from './TopNav';

export const UnderConstruction: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
   const centerAlign = vw / 2 - 720;

   const { hordeClasses, modeColors } = getHordeStyling();

   const key = `windowsize_view_${windowSize.width}_${windowSize.height}`;

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Analytics' }]} />
      <Stack styles={{ root: { width: "100%", backgroundColor: modeColors.background } }}>
         <Stack style={{ width: "100%", backgroundColor: modeColors.background }}>
            <Stack style={{ position: "relative", width: "100%", height: 'calc(100vh - 148px)' }}>
               <div style={{ overflowX: "auto", overflowY: "visible" }}>
                  <Stack horizontal style={{ paddingTop: 30, paddingBottom: 48 }}>
                     <Stack key={`${key}`} style={{ paddingLeft: centerAlign }} />
                     <Stack style={{ width: 1440 }}>
                        <Stack style={{ paddingBottom: 12 }}>
                           <Stack horizontalAlign="center">
                                 <Text variant="mediumPlus">Under Construction</Text>
                              </Stack>
                           </Stack>                        
                     </Stack>
                  </Stack>
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};