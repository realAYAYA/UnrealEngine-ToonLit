// Copyright Epic Games, Inc. All Rights Reserved.

import { Point } from '@fluentui/react';
import { ContextualMenu, ContextualMenuItemType, IContextualMenuItem, Stack, Text } from '@fluentui/react';
import React, { MutableRefObject, useState } from 'react';
import { GetChangeSummaryResponse, GetJobStepRefResponse, JobData } from '../backend/Api';
import dashboard, { StatusColor } from "../backend/Dashboard";
import { projectStore } from '../backend/ProjectStore';

export type ChangeContextMenuTarget = {

   ref?: MutableRefObject<null>;
   point?: Point;

}

export const ChangeContextMenu: React.FC<{ target: ChangeContextMenuTarget, onDismiss?: () => void, job?: JobData, stepRef?: GetJobStepRefResponse, commit?: GetChangeSummaryResponse, rangeCL?: number }> = ({ target, onDismiss, job, stepRef, commit, rangeCL }) => {

   if (!job) {
      return null;
   }

   let jobChange = commit?.number;

   if (jobChange === undefined) {
      jobChange = stepRef?.change ?? job.change!;
   }
      
   let change = jobChange.toString();
   if (!stepRef && job?.preflightChange) {
      change = `PF ${job.preflightChange}`;
   }

   const copyToClipboard = () => {
      const el = document.createElement('textarea');
      el.value = change.replace("PF", "");
      document.body.appendChild(el);
      el.select();
      document.execCommand('copy');
      document.body.removeChild(el);
   }

   const menuItems: IContextualMenuItem[] = [];

   if (commit) {

      menuItems.push({
         key: "commit", onRender: () => {
            return <Stack style={{ padding: 18, paddingTop: 18, paddingBottom: 18, maxWidth: 500 }} tokens={{ childrenGap: 12 }}>
               <Stack horizontal tokens={{ childrenGap: 36 }}>
                  <Text variant="small" style={{ fontFamily: "Horde Open Sans Bold" }}>Author:</Text>
                  <Text variant="small" >{commit.authorInfo?.name}</Text>
               </Stack>
               <Stack horizontal tokens={{ childrenGap: 35 }}>
                  <Text variant="small" style={{ fontFamily: "Horde Open Sans Bold" }}>Change:</Text>
                  <Text variant="small" >{commit.number}</Text>
               </Stack>

               <Stack horizontal tokens={{ childrenGap: 12 }}>
                  <Text variant="small" style={{ fontFamily: "Horde Open Sans Bold" }}>Description:</Text>
                  <Text variant="small" >{commit.description}</Text>
               </Stack>
            </Stack>
         }
      });

      menuItems.push({
         key: "commit_divider",
         itemType: ContextualMenuItemType.Divider
      });


   }

   menuItems.push({
      key: 'copy_to_clipboard',
      text: 'Copy CL to Clipboard',
      onClick: () => copyToClipboard(),
   }
   );

   menuItems.push({ key: 'open_in_swarm', text: 'Open CL in Swarm', onClick: (ev) => { window.open(`${dashboard.swarmUrl}/changes/${change.replace("PF ", "")}`) } })

   // range
   const stream = projectStore.streamById(job.streamId)!;
   const project = projectStore.byId(stream!.projectId)!;
   const name = project.name === "Engine" ? "UE4" : project.name;

   let url = "";
   url = `${dashboard.swarmUrl}/files/${name}/${stream.name}?range=@${jobChange}#commits`;


   let highCL = jobChange;
   let lowCL = jobChange;

   if (rangeCL !== undefined) {
      lowCL = Math.min(jobChange, rangeCL);
      highCL = Math.max(jobChange, rangeCL);
      url = `${dashboard.swarmUrl}/files/${name}/${stream.name}?range=@${lowCL},@${highCL}#commits`;
   }

   let rangeText = 'Open CL Range in Swarm';

   if (rangeCL) {
      rangeText = `Open CL Range ${lowCL} - ${highCL}`;
   }


   if (url) {
      menuItems.push({ key: 'open_in_swarm_range', text: rangeText, onClick: (ev) => { window.open(url) } })
   }

   return (<ContextualMenu
      styles={{ list: { selectors: { '.ms-ContextualMenu-itemText': { fontSize: "10px", paddingLeft: 8 } } } }}
      items={menuItems}
      hidden={false}
      target={target.ref ?? target.point}
      onItemClick={() => { if (onDismiss) onDismiss() }}
      onDismiss={() => { if (onDismiss) onDismiss() }}
   />);

}

export const ChangeButton: React.FC<{ job?: JobData, stepRef?: GetJobStepRefResponse, commit?: GetChangeSummaryResponse, hideAborted?: boolean, rangeCL?: number }> = ({ job, stepRef, commit, hideAborted, rangeCL }) => {

   const [menuShown, setMenuShown] = useState(false);

   const spanRef = React.useRef(null);

   if (!job) {
      return null;
   }

   if (!job.abortedByUserInfo) {
      hideAborted = true;
   }


   let change = job?.change?.toString() ?? "Latest";
   if (job?.preflightChange) {
      change = `PF ${job.preflightChange}`;
   }

   if (stepRef) {
      change = stepRef.change.toString();
   }

   const colors = dashboard.getStatusColors();

   return (<Stack verticalAlign="center" verticalFill={true} horizontalAlign="start"> <div style={{ paddingBottom: "1px" }}>
      <Stack tokens={{ childrenGap: 4 }}>
         <span ref={spanRef} style={{ padding: "2px 6px 2px 6px", height: "15px", cursor: "pointer" }} className={job.startedByUserInfo ? "cl-callout-button-user" : "cl-callout-button"} onClick={(ev) => { ev.stopPropagation(); ev.preventDefault(); setMenuShown(!menuShown) }} >{change}</span>
         {!hideAborted && <span ref={spanRef} style={{ padding: "2px 6px 2px 6px", height: "16px", cursor: "pointer", userSelect: "none", fontFamily: "Horde Open Sans SemiBold", fontSize: "10px", backgroundColor: colors.get(StatusColor.Failure), color: "rgb(255, 255, 255)" }} onClick={(ev) => { ev.preventDefault(); setMenuShown(!menuShown) }}>Aborted</span>}
      </Stack>
      {menuShown && <ChangeContextMenu target={{ ref: spanRef }} job={job} commit={commit} stepRef={stepRef} rangeCL={rangeCL}  onDismiss={() => setMenuShown(false)} />}
   </div></Stack>);
};

/*

text-decoration: none;
  white-space: nowrap;
  background:#0288ee;
  color: rgb(255, 255, 255);
  user-select: none;
  font-family: "Horde Open Sans SemiBold";
  font-size: 10px;
  border-width: 0;
  */