// Copyright Epic Games, Inc. All Rights Reserved.

import { CommandBar, CommandBarButton, ContextualMenu, ContextualMenuItem, ContextualMenuItemType, DefaultButton, Dialog, DialogFooter, DialogType, IButton, IButtonProps, ICommandBarItemProps, IContextualMenuItem, IContextualMenuItemProps, IContextualMenuItemStyles, IContextualMenuStyles, Persona, PersonaSize, PrimaryButton, Stack, Text } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useRef, useState } from 'react';
import { Link, useHistory } from 'react-router-dom';
import backend, { useBackend } from '../backend';
import { ProjectData } from "../backend/Api";
import dashboard from '../backend/Dashboard';
import { ProjectStore } from '../backend/ProjectStore';
import { modeColors } from '../styles/Styles';
import { VersionModal } from './VersionModal';

const logoutURL = "/account";

// Styles for both command bar and overflow/menu items
const itemStyles: Partial<IContextualMenuItemStyles> = {
   label: { fontSize: 12 },
   root: {
      paddingLeft: 6,
      selectors: {
         'a:link,a:visited': {
            color: modeColors.text
         }
      }
   }
};
// For passing the styles through to the context menus
const menuStyles: Partial<IContextualMenuStyles> = {
   root: {
      fontSize: 12, selectors: {
         'a:link,a:visited': {
            color: modeColors.text
         }
      }
   },
   header: { fontFamily: "Horde Open Sans Bold" },
   subComponentStyles: { menuItem: itemStyles, callout: {} }
};

type ButtonData = {
   project: ProjectData;
}

// Top level project button component
const ProjectButton: React.FunctionComponent<IButtonProps> = (props) => {

   const history = useHistory();
   const { project } = props.data as ButtonData;

   const buttonRef = React.createRef<IButton>();
   return (
      <Stack disableShrink={true}
         onMouseEnter={() => {
            buttonRef.current?.openMenu(true);
         }}
         onMouseLeave={() => {
            buttonRef.current?.dismissMenu();
         }} >
         <CommandBarButton
            componentRef={buttonRef}
            menuProps={{ subMenuHoverDelay: 0, items: [] }}
            href={`/project/${project?.id}`}

            onMenuClick={(ev) => {

               if (ev?.metaKey || ev?.ctrlKey) {
                  window.open(`/project/${project?.id}`);
               } else {
                  history.push(`/project/${project?.id}`);
               }

            }}
            {...props}
            styles={{
               ...props.styles,
               ...itemStyles,
               root: {
                  backgroundColor: modeColors.header,
                  color: modeColors.text,
                  height: 30,
                  paddingLeft: 12,
                  paddingRight: 12,
                  margin: 0,
                  fontFamily: "Horde Raleway Regular"
               },
               menuIcon: {
                  display: "none"
               }
            }}
         />
      </Stack>
   );
};

type IProjectContextualMenuItem = IContextualMenuItem & {
   link?: string;
}

// individual menu items
const ProjectMenuItem: React.FunctionComponent<IContextualMenuItemProps> = props => {

   const item = props.item as IProjectContextualMenuItem;

   if (!item.link) {
      return <ContextualMenuItem {...props} />;
   }

   // Due to ContextualMenu implementation quirks, passing styles here doesn't work
   return <Link to={item.link} onClick={(ev: any) => { ev.stopPropagation(); return true; }}> <ContextualMenuItem onClick={(ev: any) => { ev.preventDefault(); return true; }} {...props} /></Link>;
};

const generateProjectMenu = (history: any, store: ProjectStore) => {

   const projects = store.projects;

   if (!projects || !projects.length) {
      return [];
   }

   const cbProps: ICommandBarItemProps[] = [];

   projects.sort((a: ProjectData, b: ProjectData) => {
      return a.order - b.order;
   }).forEach(p => {

      // setup streams as sub items
      const subItems: IProjectContextualMenuItem[] = [];


      if (p.categories) {

         const cats = p.categories.filter(c => c.showOnNavMenu);
         const track = new Set<string>();

         cats.forEach(c => {

            if (track.has(c.name)) {
               return;
            }

            const streams = p.streams?.filter(s => c.streams.indexOf(s.id) !== -1);

            if (!streams || !streams.length) {
               return;
            }

            track.add(c.name);

            const catItem: IContextualMenuItem = {
               itemType: ContextualMenuItemType.Section,
               key: `stream_category_${c.name}`,
               sectionProps: {
                  title: c.name,
                  items: [],
                  bottomDivider: true
               }
            }


            streams.forEach(stream => {


               catItem.sectionProps!.items.push(
                  {

                     style: { color: modeColors.text },
                     key: stream.id, text: stream.name, data: { project: p, stream: stream }, link: `/stream/${stream.id}`
                  }
               );

            })

            subItems.push(catItem);


         })

         if (subItems.length) {
            subItems.push(
               {
                  style: { color: modeColors.text },
                  key: `show_all_${p.id}`, text: "Show All", data: { project: p, stream: undefined }, link: `/project/${p.id}`
               }
            );
         }
      }

      if (!subItems.length) {

         p.streams?.forEach(stream => {
            subItems.push(
               {
                  style: { color: modeColors.text },
                  key: stream.id, text: stream.name, data: { project: p, stream: stream }, link: `/stream/${stream.id}`
               }
            );
         });

      }

      if (!subItems.length) {
         return;
      }


      const style = { ...menuStyles } as Partial<IContextualMenuStyles>;

      // project button        
      const cbItem: ICommandBarItemProps = {
         key: p.id,
         text: p.name.toUpperCase(),
         data: {
            project: p

         },
         subMenuProps: {
            contextualMenuItemAs: ProjectMenuItem,
            styles: style,
            items: subItems
         }
      };

      cbProps.push(cbItem);
   });

   return cbProps;
};


// Top level admin button component
const AdminButton: React.FunctionComponent<IButtonProps> = (props) => {

   const history = useHistory();

   const buttonRef = React.createRef<IButton>();
   return (
      <Stack disableShrink={true}
         onMouseEnter={() => {
            buttonRef.current?.openMenu();
         }}
         onMouseLeave={() => {
            buttonRef.current?.dismissMenu();
         }} >
         <CommandBarButton
            componentRef={buttonRef}
            menuProps={{ subMenuHoverDelay: 0, items: [] }}

            onMouseUp={(ev => {
               if (ev?.button === 1) {
                  window.open(`/agents`);
               }
            })}

            onMenuClick={(ev) => {

               if (ev?.metaKey || ev?.ctrlKey) {
                  window.open(`/agents`);
               } else {
                  history.push(`/agents`);
               }
            }}
            {...props}
            styles={{
               ...props.styles,
               ...itemStyles,
               root: {
                  backgroundColor: modeColors.header,
                  height: 30,
                  paddingLeft: 12,
                  paddingRight: 12,
                  margin: 0,
                  fontFamily: "Horde Raleway Regular"
               },
               menuIcon: {
                  display: "none"
               }
            }}
         />
      </Stack>
   );
};


// this module global is needed to enforce showing dialog ony once, in case component is recreated
let globalHasShown = true; // true, disabled
const RequestLogout: React.FC = observer(() => {

   const [hasShown, setHasShown] = useState(false);

   // subscribe
   if (dashboard.updated) { }

   if (!dashboard.requestLogout || hasShown || globalHasShown) {
      return null
   }

   const dialogContentProps = {
      type: DialogType.normal,
      title: 'New Features',
      subText: 'New features require logging out of Okta',
   };

   const modalProps = {
      isBlocking: true
   };

   const logout = async () => {
      try {
         console.log('Logging out of server');
         await backend.serverLogout(logoutURL);

      } catch (reason) {
         console.error(reason);
      }
   }

   return <Dialog
      hidden={false}
      onDismiss={() => { globalHasShown = true; setHasShown(true) }}
      modalProps={modalProps}
      dialogContentProps={dialogContentProps}

   >
      <Stack style={{ paddingTop: 8 }}>
         <DialogFooter>
            <PrimaryButton onClick={() => logout()} text="Logout" />
            <DefaultButton onClick={() => { globalHasShown = true; setHasShown(true); }} text="Cancel" />
         </DialogFooter>
      </Stack>
   </Dialog>

});



export const TopNav: React.FC<{ suppressServer?: boolean }> = observer(({ suppressServer }) => {

   const [showMenu, setShowMenu] = useState(false);
   const [showVersion, setShowVersion] = useState(false);
   const history = useHistory();
   const { projectStore } = useBackend();

   const divRef = useRef(null);

   // subscribe
   if (dashboard.updated) { }

   const generateAdminMenu = (history: any) => {


      const cbProps: ICommandBarItemProps[] = [];

      const subItems: IContextualMenuItem[] = [];

      // Resources
      const resourceItems: IContextualMenuItem[] = [];

      resourceItems.push({
         key: "admin_agents",
         text: "Agents",
         link: `/agents`
      });

      resourceItems.push({
         key: "admin_devices",
         text: "Devices",
         link: `/devices`
      });

      resourceItems.push({
         key: "admin_perforce_servers",
         text: "Perforce Servers",
         link: `/perforce/servers`
      });

      subItems.push({
         itemType: ContextualMenuItemType.Section,
         key: `admin_resources`,
         sectionProps: {
            title: "Resources",
            items: resourceItems,
            bottomDivider: true
         }
      });

      // Monitoring
      const monitoringItems: IContextualMenuItem[] = [];
      monitoringItems.push({
         key: "admin_utilization",
         text: "Utilization",
         link: `/reports/utilization`
      });

      if (dashboard.hordeAdmin) {
         monitoringItems.push({
            key: "admin_notices",
            text: "Notices",
            link: `/notices`
         });
      }

      subItems.push({
         itemType: ContextualMenuItemType.Section,
         key: `admin_monitoring`,
         sectionProps: {
            title: "Monitoring",
            items: monitoringItems,
            bottomDivider: true
         }
      });

      // Configuration
      const versionItems: IContextualMenuItem[] = [];
      versionItems.push({
         key: "server_versions",
         text: "Version",
         onClick: () => { setShowVersion(true) }
      });

      subItems.push({
         itemType: ContextualMenuItemType.Section,
         key: `admin_config`,
         sectionProps: {
            title: "Version",
            items: versionItems,
            bottomDivider: true
         }
      });

      const style = { ...menuStyles } as Partial<IContextualMenuStyles>;

      const cbItem: ICommandBarItemProps = {
         key: "admin_button",
         text: "SERVER",
         subMenuProps: {
            contextualMenuItemAs: ProjectMenuItem,
            styles: style,
            items: subItems
         }
      };

      cbProps.push(cbItem);

      return cbProps;
   };



   let initials = "?.?";
   try {
      const username = dashboard.username ?? "?.?";
      if (username.indexOf(".") !== -1) {
         const firstlast = username.split(".");
         initials = firstlast[0][0].toUpperCase() + firstlast[1][0].toUpperCase();
      } else {
         initials = username.toUpperCase().slice(0, 2);
      }
   } catch (reason) {
      console.log(reason);
   }

   const menuItems: IContextualMenuItem[] = [
      {
         itemType: ContextualMenuItemType.Section,
         key: `user_section`,
         sectionProps: {
            items: [
               {
                  key: 'settingsItem',
                  text: 'Preferences',
                  onClick: () => {
                     history.push("/dashboard");
                  }
               }],
            bottomDivider: true
         }
      },
      {
         key: 'logoutItem',
         text: 'Sign Out',
         onClick: () => {

            const logout = async () => {
               console.log('Logging out of server');
               await backend.serverLogout(logoutURL);
            }
            logout();
         }
      }
   ];

   let showServer = true;//dashboard.hordeAdmin || dashboard.internalEmployee;

   // @todo: better no auth server config detection: https://jira.it.epicgames.com/browse/UE-119421
   if (dashboard.username === "Anonymous") {
      showServer = true;
   }

   if (suppressServer) {
      showServer = false;
   }

   return (
      <div style={{ backgroundColor: modeColors.header }}>
         {showVersion && <VersionModal show={true} onClose={() => { setShowVersion(false) }} />}
         <Stack tokens={{ maxWidth: 1800, childrenGap: 0 }} disableShrink={true} styles={{ root: { backgroundColor: modeColors.header, margin: "auto", width: "100%" } }}>
            <Stack horizontal verticalAlign='center' styles={{ root: { height: "60px" } }} >

               <Link to="/index"><Stack horizontal styles={{ root: { paddingLeft: 8, cursor: 'pointer' } }}>
                  <Stack styles={{ root: { paddingTop: 2, paddingRight: 6 } }}>
                     <img style={{ width: 32 }} src="/images/horde.svg" alt="" />
                  </Stack>
                  <Stack styles={{ root: { paddingTop: 11 } }}>
                     <Text styles={{ root: { fontFamily: "Horde Raleway Bold", color: dashboard.darktheme ? "#FFFFFFFF" : modeColors.text } }}>HORDE</Text>
                  </Stack>
               </Stack>
               </Link>

               <RequestLogout />

               <Stack grow>
                  <CommandBar styles={{ root: { paddingTop: 17, backgroundColor: modeColors.header } }}
                     buttonAs={ProjectButton}
                     onReduceData={() => undefined}
                     items={generateProjectMenu(history, projectStore)}
                  />
               </Stack>

               <Stack grow />
               <Stack horizontal verticalAlign="center" tokens={{ childrenGap: 0 }}>
                  <Stack>
                     {showServer && <CommandBar styles={{ root: { paddingTop: 17, paddingLeft: 0, paddingRight: 32, backgroundColor: modeColors.header } }}
                        buttonAs={AdminButton}
                        onReduceData={() => undefined}
                        items={generateAdminMenu(history)}
                     />}
                  </Stack>

                  <Stack styles={{ root: {} }} onMouseEnter={() => setShowMenu(true)}
                     onMouseLeave={() => { setShowMenu(false) }} >
                     <div ref={divRef}>
                        <a href="/" onClick={(ev) => { ev.preventDefault(); setShowMenu(true); }}>
                           <Persona styles={{ root: { selectors: { ".ms-Persona-initials": { fontWeight: "unset", fontFamily: "Horde Open Sans SemiBold" } } } }} imageShouldFadeIn={false} imageInitials={initials} imageUrl={dashboard.userImage32} size={PersonaSize.size32}
                              onClick={() => { history.push("/index") }} />
                        </a>
                     </div>
                     <ContextualMenu
                        items={menuItems}
                        hidden={!showMenu}
                        target={divRef}
                        isBeakVisible={true}
                        gapSpace={0}
                        beakWidth={0}
                        onItemClick={() => { setShowMenu(false); }}
                        onDismiss={() => { setShowMenu(false); }}
                     />
                  </Stack>
               </Stack>


            </Stack>
         </Stack></div>);
});
