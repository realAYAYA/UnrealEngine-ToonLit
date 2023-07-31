import * as React from 'react';
import { Link } from 'react-router-dom';
import { ILinkProps } from '@fluentui/react/lib/Link';
import { removeAnchorLink } from '../../utilities/index';

export const MarkdownLink: React.StatelessComponent<ILinkProps> = props => {
   let href = props.href;
   if (href && href[0] === '#' && href.indexOf('/') === -1) {
      // This is an anchor link within this page. We need to prepend the current route.
      href = removeAnchorLink(window.location.hash) + href;
   }

   if (href?.startsWith("file://")) {
      return <a children={props.children} href={href!} />;
   }


   if (href) {

      try {
         const url = new URL(href);
         if (url.hostname === window.location.hostname) {
            href = url.pathname + url.search
         } else {
            return <a children={props.children} href={href} target="_blank" rel="noreferrer"/>;
         }
      } catch {

      }
   }

   return <Link children={props.children} to={href!} />;
};
