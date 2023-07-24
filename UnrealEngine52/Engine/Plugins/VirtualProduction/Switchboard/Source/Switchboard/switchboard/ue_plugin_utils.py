# Copyright Epic Games, Inc. All Rights Reserved.

import glob
import pathlib
import typing

from switchboard.switchboard_logging import LOGGER


class UnrealPlugin(object):
    '''
    A simple object that encapsulates properties of an Unreal Engine plugin
    based on the file system path and name of the plugin's .uplugin file.
    '''

    @classmethod
    def from_plugin_path(cls, plugin_path: typing.Union[pathlib.Path, str]):
        '''
        Get an UnrealPlugin object based on the given file system path.

        The provided path can be either a file path to an Unreal Engine
        ".uplugin" file or a path to the plugin directory that contains
        a ".uplugin" file. In the latter case, the directory will be
        searched for the ".uplugin" file.
        '''
        if not isinstance(plugin_path, pathlib.Path):
            plugin_path = pathlib.Path(plugin_path)

        if not plugin_path.exists():
            LOGGER.warning(
                'Cannot find uplugin file or plugin directory at '
                f'path: {plugin_path}')
            return None

        if (plugin_path.is_file() and
                plugin_path.suffix.casefold() == '.uplugin'):
            return cls(plugin_path)

        if not plugin_path.is_dir():
            LOGGER.warning(f'Plugin path is not a directory: {plugin_path}')
            return None

        uplugin_file_paths = list(plugin_path.glob('*.uplugin'))
        if not uplugin_file_paths:
            LOGGER.warning(
                f'No .uplugin files found in plugin directory: {plugin_path}')
            return None

        if len(uplugin_file_paths) > 1:
            msg = (
                'Multiple .uplugin files found in plugin directory: '
                f'{plugin_path}\n    ')
            msg += '\n    '.join([str(p) for p in uplugin_file_paths])
            LOGGER.warning(msg)
            return None

        uplugin_file_path = pathlib.Path(uplugin_file_paths[0])

        return cls(uplugin_file_path)

    @classmethod
    def from_path_filters(
            cls,
            ue_project_path: typing.Union[pathlib.Path, str],
            filter_patterns: typing.List[str]):
        '''
        Get a list of Unreal Engine plugins matching the given path-based
        filter patterns.

        The provided path patterns can be absolute or relative. For relative
        path patterns, patterns resembling a directory name (i.e. no path
        separators) will be assumed to refer to plugin directories inside the
        Unreal Engine project (in its "Plugins" subdirectory). Otherwise,
        relative path patterns are assumed to be relative project directory
        (the directory containing the .uproject file).
        '''
        if not isinstance(ue_project_path, pathlib.Path):
            ue_project_path = pathlib.Path(ue_project_path)

        plugin_paths = set()

        for filter_pattern in filter_patterns:
            path_pattern = pathlib.Path(filter_pattern)

            if path_pattern.is_absolute():
                plugin_filter = path_pattern
            else:
                path_pattern_parts = path_pattern.parts
                if len(path_pattern_parts) < 1:
                    continue

                if len(path_pattern_parts) == 1:
                    # When the path pattern is relative and it looks like a
                    # directory name (i.e. no path separators), assume we're
                    # matching against plugin directories inside the project.
                    plugin_filter = ue_project_path / 'Plugins' / path_pattern
                else:
                    # Otherwise, assume that the path pattern is relative to
                    # the project directory.
                    plugin_filter = ue_project_path / path_pattern

            for plugin_path in glob.glob(str(plugin_filter)):
                plugin_path = pathlib.Path(plugin_path).resolve()
                plugin_paths.add(plugin_path)

        unreal_plugins = []
        for plugin_path in sorted(list(plugin_paths)):
            unreal_plugin = cls.from_plugin_path(plugin_path)
            if unreal_plugin:
                unreal_plugins.append(unreal_plugin)

        return unreal_plugins

    def __init__(self, uplugin_file_path: typing.Union[pathlib.Path, str]):
        if not isinstance(uplugin_file_path, pathlib.Path):
            uplugin_file_path = pathlib.Path(uplugin_file_path)

        self._uplugin_file_path = uplugin_file_path

    def __repr__(self) -> str:
        return f'{self.__class__.__name__}("{self._uplugin_file_path}")'

    @property
    def uplugin_file_path(self) -> pathlib.Path:
        '''
        The file system path to the plugin's .uplugin file.
        '''
        return self._uplugin_file_path

    @property
    def name(self) -> str:
        '''
        The name of the plugin.
        '''
        return self._uplugin_file_path.stem

    @property
    def plugin_path(self) -> pathlib.Path:
        '''
        The file system path to the root directory of the plugin.
        '''
        return self._uplugin_file_path.parents[0]

    @property
    def plugin_content_path(self) -> pathlib.Path:
        '''
        The file system path to the plugin's "Content" directory.

        This is the directory that gets mounted in Unreal Engine.
        '''
        return self.plugin_path / 'Content'

    @property
    def mounted_path(self) -> pathlib.PurePosixPath:
        '''
        The root content path of the plugin when its "Content" directory is
        mounted in Unreal Engine.
        '''
        # Note that UE uses the file name of the uplugin file to produce the
        # mounted path in engine, and not the name of the plugin directory or
        # any data inside the uplugin file.
        return pathlib.PurePosixPath(f'/{self.name}')
