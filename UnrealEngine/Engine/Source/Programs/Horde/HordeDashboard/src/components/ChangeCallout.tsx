// Copyright Epic Games, Inc. All Rights Reserved.
import { Callout, DirectionalHint, FontWeights, mergeStyleSets, Stack, Text } from '@fluentui/react';
import React, { useState } from 'react';
import backend from '../backend';
import { JobData, ChangeSummaryData } from '../backend/Api';
import { observable, action, makeObservable } from 'mobx';
import { observer } from 'mobx-react-lite';
import { Link } from 'react-router-dom';
import { getHordeTheme } from '../styles/theme';


let _styles: any;

const getStyles = () => {

   const theme = getHordeTheme();

   const styles = _styles ?? mergeStyleSets({
      container: {
         overflow: 'auto',
         maxHeight: 600
      },
      callout: {
         maxWidth: 600
      },
      header: {
         padding: '18px 14px 12px'
      },
      title: [
         theme.fonts.mediumPlus,
         {
            margin: 0,
            fontWeight: FontWeights.regular
         }
      ],
      inner: {
         height: '100%',
         padding: '0 14px 10px'
   
      },
      actions: {
         position: 'relative',
         marginTop: 20,
         width: '100%',
         whiteSpace: 'nowrap'
      },
      subtext: [
         theme.fonts.medium,
         {
            margin: 0,
            fontWeight: FontWeights.light
         }
      ]
   });

   _styles = styles;
   return styles;
}


class CommitCache {

   constructor(change: number, streamId: string) {

      makeObservable(this);

      this.change = change;

      backend.getCommit(streamId, change).then((value) => {
         this.setData(value!);
      });
   }

   get commitData(): ChangeSummaryData | undefined {
      // subscribe in any observers
      if (this.commitDataUpdated) { }
      return this._commitData;
   }

   @action
   setData(commitData: ChangeSummaryData) {
      this._commitData = commitData;
      this.commitDataUpdated++;
   }

   @observable
   commitDataUpdated = 0;

   private _commitData?: ChangeSummaryData;

   change: number;

}

const commits = new Map<number, CommitCache>();

export const ChangeCallout: React.FC<{ job: JobData }> = observer(({ job }) => {

   const [visible, setVisible] = useState(false);
   const [searchRef] = useState(React.createRef<HTMLDivElement>());

   const styles = getStyles();

   if (!job.change) {
      return <div style={{ paddingTop: 3 }}>Latest</div>;
   }

   const change = job.change;

   let cache = commits.get(change);
   if (!cache) {
      cache = new CommitCache(change, job.streamId);
      commits.set(change, cache);
   }

   const commit = cache.commitData;


   let key = 0;
   const commitLines = commit?.description.split(/\r\n|\n|\r/);

   if (!commit) {
      return <div style={{ paddingTop: 3 }}>{change}</div>;
   }
   return (
      <Stack>
         <Link to="" className="cl-callout-link" onClick={(ev) => { ev.preventDefault(); setVisible(!visible); }} >
            <div ref={searchRef} style={{ paddingTop: 3 }} >
               {change}
            </div>
            <div className={styles.container} data-is-scrollable={true}>
               <Callout isBeakVisible={false}
                  onDismiss={(ev) => setVisible(false)}
                  hidden={!visible}
                  target={searchRef?.current}
                  className={styles.callout}
                  role="alertdialog"
                  gapSpace={0}
                  setInitialFocus={true}
                  directionalHint={DirectionalHint.bottomLeftEdge}>
                  <div className={styles.inner}>
                     <div className={styles.header}>
                        <p className={styles.title}>
                           {`Change ${commit.number} committed by ${commit.authorInfo?.name}`}
                        </p>
                     </div>
                     <div className={styles.inner}>
                        <p className={styles.subtext}>
                           {commitLines?.map(line => <Text key={`commitline_${job.id}_${key++}`} block>{line}</Text>)}
                        </p>
                     </div>
                  </div>

               </Callout>
            </div>
         </Link>
      </Stack>
   );
});


