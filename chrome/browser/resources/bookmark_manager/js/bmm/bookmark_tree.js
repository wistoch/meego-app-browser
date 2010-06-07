// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


cr.define('bmm', function() {
  const Tree = cr.ui.Tree;
  const TreeItem = cr.ui.TreeItem;

  var treeLookup = {};

  /**
   * Creates a new tree item for a bookmark node.
   * @param {!Object} bookmarkNode The bookmark node.
   * @constructor
   * @extends {TreeItem}
   */
  function BookmarkTreeItem(bookmarkNode) {
    var ti = new TreeItem({
      label: bookmarkNode.title,
      bookmarkNode: bookmarkNode,
      // Bookmark toolbar and Other bookmarks are not draggable.
      draggable: bookmarkNode.parentId != ROOT_ID
    });
    ti.__proto__ = BookmarkTreeItem.prototype;
    return ti;
  }

  BookmarkTreeItem.prototype = {
    __proto__: TreeItem.prototype,

    /** @inheritDoc */
    addAt: function(child, index) {
      TreeItem.prototype.addAt.call(this, child, index);
      if (child.bookmarkNode)
        treeLookup[child.bookmarkNode.id] = child;
    },

    /** @inheritDoc */
    remove: function(child) {
      TreeItem.prototype.remove.call(this, child);
      if (child.bookmarkNode)
        delete treeLookup[child.bookmarkNode.id];
    },

    /**
     * The ID of the bookmark this tree item represents.
     * @type {string}
     */
    get bookmarkId() {
      return this.bookmarkNode.id;
    }
  };

  /**
   * Asynchronousy adds a tree item at the correct index based on the bookmark
   * backend.
   *
   * Since the bookmark tree only contains folders the index we get from certain
   * callbacks is not very useful so we therefore have this async call which
   * gets the children of the parent and adds the tree item at the desired
   * index.
   *
   * This also exoands the parent so that newly added children are revealed.
   *
   * @param {!cr.ui.TreeItem} parent The parent tree item.
   * @param {!cr.ui.TreeItem} treeItem The tree item to add.
   * @param {Function=} f A function which gets called after the item has been
   *     added at the right index.
   */
  function addTreeItem(parent, treeItem, opt_f) {
    chrome.bookmarks.getChildren(parent.bookmarkNode.id, function(children) {
      var index = children.filter(bmm.isFolder).map(function(item) {
        return item.id;
      }).indexOf(treeItem.bookmarkNode.id);
      parent.addAt(treeItem, index);
      parent.expanded = true;
      if (opt_f)
        opt_f();
    });
  }


  /**
   * Creates a new bookmark list.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {HTMLButtonElement}
   */
  var BookmarkTree = cr.ui.define('tree');

  BookmarkTree.prototype = {
    __proto__: Tree.prototype,

    handleBookmarkChanged: function(id, changeInfo) {
      var treeItem = treeLookup[id];
      if (treeItem)
        treeItem.label = treeItem.bookmarkNode.title = changeInfo.title;
    },

    handleChildrenReordered: function(id, reorderInfo) {
      var parentItem = treeLookup[id];
      // The tree only contains folders.
      var dirIds = reorderInfo.childIds.filter(function(id) {
        return id in treeLookup;
      }).forEach(function(id, i) {
        parentItem.addAt(treeLookup[id], i);
      });
    },

    handleCreated: function(id, bookmarkNode) {
      if (bmm.isFolder(bookmarkNode)) {
        var parentItem = treeLookup[bookmarkNode.parentId];
        var newItem = new BookmarkTreeItem(bookmarkNode);
        addTreeItem(parentItem, newItem);
      }
    },

    handleMoved: function(id, moveInfo) {
      var treeItem = treeLookup[id];
      if (treeItem) {
        var oldParentItem = treeLookup[moveInfo.oldParentId];
        oldParentItem.remove(treeItem);
        var newParentItem = treeLookup[moveInfo.parentId];
        // The tree only shows folders so the index is not the index we want. We
        // therefore get the children need to adjust the index.
        addTreeItem(newParentItem, treeItem);
      }
    },

    handleRemoved: function(id, removeInfo) {
      var parentItem = treeLookup[removeInfo.parentId];
      var itemToRemove = treeLookup[id];
      if (parentItem && itemToRemove)
        parentItem.remove(itemToRemove);
    },

    insertSubtree:function(folder) {
      if (!bmm.isFolder(folder))
        return;
      var children = folder.children;
      this.handleCreated(folder.id, folder);
      for(var i = 0; i < children.length; i++) {
        var child = children[i];
        this.insertSubtree(child);
      }
    },

    /**
     * Returns the bookmark node with the given ID. The tree only maintains
     * folder nodes.
     * @param {string} id The ID of the node to find.
     * @return {BookmarkTreeNode} The bookmark tree node or null if not found.
     */
    getBookmarkNodeById: function(id) {
      var treeItem = treeLookup[id];
      if (treeItem)
        return treeItem.bookmarkNode;
      return null;
    },

    /**
     * Fetches the bookmark items and builds the tree control.
     */
    reload: function() {
      /**
       * Recursive helper function that adds all the directories to the
       * parentTreeItem.
       * @param {!cr.ui.Tree|!cr.ui.TreeItem} parentTreeItem The parent tree
       *     element to append to.
       * @param {!Array.<BookmarkTreeNode>} bookmarkNodes
       * @return {boolean} Whether any directories where added.
       */
      function buildTreeItems(parentTreeItem, bookmarkNodes) {
        var hasDirectories = false;
        for (var i = 0, bookmarkNode; bookmarkNode = bookmarkNodes[i]; i++) {
          if (bmm.isFolder(bookmarkNode)) {
            hasDirectories = true;
            var item = new BookmarkTreeItem(bookmarkNode);
            parentTreeItem.add(item);
            var anyChildren = buildTreeItems(item, bookmarkNode.children);
            item.expanded = anyChildren;
          }
        }
        return hasDirectories;
      }

      var self = this;
      chrome.experimental.bookmarkManager.getSubtree('', true, function(root) {
        self.clear();
        buildTreeItems(self, root[0].children);
        cr.dispatchSimpleEvent(self, 'load');
      });
    },

    /**
     * Clears the tree.
     */
    clear: function() {
      // Remove all fields without recreating the object since other code
      // references it.
      for (var id in treeLookup){
        delete treeLookup[id];
      }
      this.textContent = '';
    },

    /** @inheritDoc */
    addAt: function(child, index) {
      Tree.prototype.addAt.call(this, child, index);
      if (child.bookmarkNode)
        treeLookup[child.bookmarkNode.id] = child;
    },

    /** @inheritDoc */
    remove: function(child) {
      Tree.prototype.remove.call(this, child);
      if (child.bookmarkNode)
        delete treeLookup[child.bookmarkNode.id];
    }
  };

  return {
    BookmarkTree: BookmarkTree,
    BookmarkTreeItem: BookmarkTreeItem,
    treeLookup: treeLookup
  };
});
