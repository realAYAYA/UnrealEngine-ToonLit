// Copyright Epic Games, Inc. All Rights Reserved.

import 'dart:collection';

import 'package:flutter/material.dart';

/// A list of items shown in a collapsible, nested tree structure.
class TreeView extends StatefulWidget {
  const TreeView({
    Key? key,
    required this.treeController,
    required this.nodeBuilder,
    this.scrollController,
  }) : super(key: key);

  /// Controller which provides the node data and state management for the tree.
  final TreeViewController treeController;

  /// Function which will create a node widget for the given node data.
  final Widget Function(TreeViewNode node, TreeViewController controller) nodeBuilder;

  /// Controller for scrolling the tree view.
  final ScrollController? scrollController;

  @override
  State<TreeView> createState() => _TreeViewState();
}

class _TreeViewState extends State<TreeView> {
  @override
  void initState() {
    super.initState();

    widget.treeController.addListener(_onControllerChanged);
  }

  @override
  void didUpdateWidget(covariant TreeView oldWidget) {
    super.didUpdateWidget(oldWidget);

    if (oldWidget.treeController != widget.treeController) {
      oldWidget.treeController.removeListener(_onControllerChanged);
      widget.treeController.addListener(_onControllerChanged);
      _onControllerChanged();
    }
  }

  @override
  void dispose() {
    widget.treeController.removeListener(_onControllerChanged);

    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListView.builder(
      itemCount: widget.treeController.getVisibleNodeCount(),
      itemBuilder: _buildNode,
      controller: widget.scrollController,
      clipBehavior: Clip.hardEdge,
    );
  }

  Widget _buildNode(BuildContext context, int index) {
    return widget.nodeBuilder(
      widget.treeController.getVisibleNodeWithIndex(index),
      widget.treeController,
    );
  }

  void _onControllerChanged() {
    setState(() {});
  }
}

/// A controller for a [TreeView] which holds data about the tree's nodes and manages their states.
class TreeViewController extends ChangeNotifier {
  /// List of nodes at the root level of the tree.
  final List<TreeViewNode> _children = [];

  /// Map from unique keys to nodes in the tree.
  final Map<String, TreeViewNode> _nodesByKey = {};

  /// Set of keys for nodes which are currently collapsed.
  final Set<String> _collapsedNodes = {};

  /// Cached list of currently visible in nodes in the order they appear. This may not contain all currently visible
  /// nodes.
  final List<TreeViewNode> _cachedVisibleNodes = [];

  /// Cached number of currently visible nodes.
  int? _cachedVisibleNodeCount = null;

  /// Get a list of all node keys currently in the tree.
  Iterable<String> get allKeys => UnmodifiableListView(_nodesByKey.keys);

  /// Add a node to the tree.
  /// If provided, the node will be added as a child of the node specified by [parentKey].
  void addNode(TreeViewNode node, {String? parentKey}) {
    if (_nodesByKey.containsKey(node.key)) {
      throw ArgumentError('Tried to add a node with key ${node.key}, but that key is already in the tree');
    }

    TreeViewNode? parent = null;
    if (parentKey != null) {
      parent = _nodesByKey[parentKey];
      if (parent == null) {
        throw ArgumentError('Tried to add a node with parent key ${node.key}, but that key is not in the tree');
      }
    }

    _nodesByKey[node.key] = node;

    if (parent != null) {
      parent.children.add(node);
      node.indentation = parent.indentation + 1;
      node.parent = parent;
    } else {
      _children.add(node);
    }

    _invalidateVisibleNodeCache();
    notifyListeners();
  }

  /// Remove a node from the tree if it exists.
  bool removeNode(String key) {
    if (!_nodesByKey.containsKey(key)) {
      return false;
    }

    final node = _nodesByKey.remove(key);
    _children.remove(node);

    node!.parent?.children.remove(node);

    _invalidateVisibleNodeCache();
    notifyListeners();
    return true;
  }

  /// Expand the node with the given [key].
  void expandNode(String key) {
    if (_collapsedNodes.remove(key)) {
      _invalidateVisibleNodeCache();
      notifyListeners();
    }
  }

  /// Collapse the node with the given [key].
  void collapseNode(String key) {
    if (_collapsedNodes.add(key)) {
      _invalidateVisibleNodeCache();
      notifyListeners();
    }
  }

  /// Toggle the collapsed/expanded state of the node with the given [key].
  void toggleNode(String key) {
    if (isNodeExpanded(key)) {
      collapseNode(key);
    } else {
      expandNode(key);
    }
  }

  /// Get the node with the given key if it exists.
  TreeViewNode? getNode(String key) => _nodesByKey[key];

  /// Get the number of visible nodes.
  int getVisibleNodeCount() {
    if (_cachedVisibleNodeCount == null) {
      _cachedVisibleNodeCount = _children.fold<int>(0, (sum, child) => sum + _getVisibleNodeCount(child));
    }

    return _cachedVisibleNodeCount!;
  }

  /// Get the node with the given index in the flattened list of visible nodes.
  TreeViewNode getVisibleNodeWithIndex(int index) {
    if (index < 0 || index >= getVisibleNodeCount()) {
      throw ArgumentError('Visible node index out of range');
    }

    if (index >= _cachedVisibleNodes.length) {
      _cacheVisibleNodeIndices(_children, 0);
    }

    return _cachedVisibleNodes[index];
  }

  /// Check whether the node with the given [key] is expanded.
  bool isNodeExpanded(String key) {
    return !_collapsedNodes.contains(key);
  }

  /// Invalidate the visible node cache.
  void _invalidateVisibleNodeCache() {
    _cachedVisibleNodeCount = null;
    _cachedVisibleNodes.clear();
  }

  /// Given a list of [nodes], traverse them and fill [_cachedVisibleNodes] with the nodes at each index.
  /// Treat the first encountered node as index [index].
  int _cacheVisibleNodeIndices(List<TreeViewNode> nodes, int index) {
    assert(index <= _cachedVisibleNodes.length);

    for (final TreeViewNode node in nodes) {
      if (_cachedVisibleNodes.length <= index) {
        _cachedVisibleNodes.add(node);
      }
      index += 1;

      if (isNodeExpanded(node.key)) {
        index = _cacheVisibleNodeIndices(node.children, index);
      }
    }

    return index;
  }

  /// Count how many visible nodes are contained within a [node] and its descendants (including itself).
  int _getVisibleNodeCount(TreeViewNode node) {
    if (!isNodeExpanded(node.key)) {
      return 1;
    }

    return node.children.fold<int>(1, (sum, child) => sum + _getVisibleNodeCount(child));
  }
}

/// Data about a node in a [TreeView].
class TreeViewNode<T> {
  TreeViewNode({
    required this.key,
    required this.data,
    List<TreeViewNode>? children,
  }) : this.children = children?.toList() ?? [];

  /// A string used to refer to this node uniquely within the tree.
  final String key;

  /// List of nodes descending from this one.
  final List<TreeViewNode> children;

  /// Custom data associated with the node.
  final T data;

  /// How many layers deep this node is indented.
  int indentation = 0;

  /// The parent of this node in its current tree structure, or null if it's root-level.
  TreeViewNode? parent;
}
