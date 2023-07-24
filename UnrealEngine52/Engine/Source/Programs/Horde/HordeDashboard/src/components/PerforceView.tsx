// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, IColumn, SelectionMode, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect } from "react";
import backend from "../backend";
import { GetPerforceServerStatusResponse } from "../backend/Api";
import { PollBase } from "../backend/PollBase";
import { useWindowSize } from "../base/utilities/hooks";
import { hordeClasses, modeColors } from "../styles/Styles";
import { Breadcrumbs } from "./Breadcrumbs";
import { TopNav } from "./TopNav";

class PerforceServerHandler extends PollBase {

   constructor(pollTime = 5000) {

      super(pollTime);

   }

   clear() {
      super.stop();
   }

   async poll(): Promise<void> {

      try {

         this.status = await backend.getPerforceServerStatus();

         this.setUpdated();

      } catch (err) {

      }

   }

   status: GetPerforceServerStatusResponse[] = [];
}

const handler = new PerforceServerHandler();

const ServerPanel: React.FC = observer(() => {

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   // subscribe
   if (handler.updated) { };

   const columns = [
      { key: 'column_cluster', name: 'Cluster', fieldName: 'cluster', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column_numleases', name: 'Leases', fieldName: 'numLeases', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column_status', name: 'Status', fieldName: 'status', minWidth: 120, maxWidth: 120, isResizable: false },
      { key: 'column_server', name: 'Server', fieldName: 'serverAndPort', minWidth: 300, maxWidth: 300, isResizable: false },
      { key: 'column_detail', name: 'Detail', fieldName: 'detail', minWidth: 200, maxWidth: 200, isResizable: false },
   ];

   let status = [...handler.status];

   status = status.sort((a, b) => {

      if (a.cluster === b.cluster) {
         return b.numLeases - a.numLeases;
      }

      if (a.cluster === "Default") return -1;
      if (b.cluster === "Default") return 1;

      return a.cluster.toLowerCase() < b.cluster.toLowerCase() ? -1 : 1;
   });

   const renderItem = (item: any, index?: number, column?: IColumn) => {
      if (!column?.fieldName) {
         return null;
      }
      return <Text style={{color: modeColors.text}}>{item[column?.fieldName] }</Text>
   };

   return (<Stack>
      <Stack styles={{ root: { paddingTop: 18, paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack className={hordeClasses.raised}>
            <Stack tokens={{ childrenGap: 12 }}>
               <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Servers</Text>
               <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "calc(100vh - 312px)" }} data-is-scrollable={true}>
                  <Stack>
                     <DetailsList
                        items={status}
                        columns={columns}
                        selectionMode={SelectionMode.none}
                        layoutMode={DetailsListLayoutMode.justified}
                        compact={true}
                        onRenderItemColumn={renderItem}
                     />
                  </Stack>
               </div>
            </Stack>
         </Stack>
      </Stack>
   </Stack>);
});


export const PerforceServerView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Perforce' }]} />
      <Stack horizontal>
         <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 896, flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1778, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%", height: 'calc(100vh - 228px)' }} tokens={{ childrenGap: 18 }}>
                     <ServerPanel />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};

