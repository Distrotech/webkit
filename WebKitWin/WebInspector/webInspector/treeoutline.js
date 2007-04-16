/*
 * Copyright (C) 2007 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

function TreeOutline(listNode)
{
    this.children = [];
    this.selectedTreeElement = null;
    this._childrenListNode = listNode;
    this._childrenListNode.removeChildren();
    this._knownTreeElements = [];
    this.root = true;
    this.hasChildren = false;
    this.expanded = true;
    this.selected = false;
    this.treeOutline = this;
}

TreeOutline._knownTreeElementNextIdentifier = 1;

TreeOutline.prototype._rememberTreeElement = function(element)
{
    if (!this._knownTreeElements[element.identifier])
        this._knownTreeElements[element.identifier] = [];

    // check if the element is already known
    var elements = this._knownTreeElements[element.identifier];
    for (var i = 0; i < elements.length; ++i)
        if (elements[i] === element)
            return;

    // add the element
    elements.push(element);
}

TreeOutline.prototype._forgetTreeElement = function(element)
{
    if (!this._knownTreeElements[element.identifier])
        return;

    var elements = this._knownTreeElements[element.identifier];
    for (var i = 0; i < elements.length; ++i) {
        if (elements[i] === element) {
            elements.splice(i, 1);
            break;
        }
    }
}

TreeOutline.prototype.findTreeElement = function(representedObject)
{
    if (!representedObject || !representedObject.__treeElementIdentifier)
        return null;

    var elements = this._knownTreeElements[representedObject.__treeElementIdentifier];
    if (!elements)
        return null;

    for (var i = 0; i < elements.length; ++i)
        if (elements[i].representedObject === representedObject)
            return elements[i];

    return null;
}

TreeOutline.prototype.handleKeyEvent = function(event)
{
    if (!this.selectedTreeElement || event.shiftKey || event.metaKey || event.altKey || event.ctrlKey)
        return;

    var handled = false;
    var nextSelectedElement;
    if (event.keyIdentifier === "Up") {
        nextSelectedElement = this.selectedTreeElement.traversePreviousTreeElement(true);
        handled = true;
    } else if (event.keyIdentifier === "Down") {
        nextSelectedElement = this.selectedTreeElement.traverseNextTreeElement(true);
        handled = true;
    } else if (event.keyIdentifier === "Left") {
        if (this.selectedTreeElement.expanded) {
            this.selectedTreeElement.collapse();
            handled = true;
        } else if (this.selectedTreeElement.parent && !this.selectedTreeElement.parent.root) {
            nextSelectedElement = this.selectedTreeElement.parent;
            handled = true;
        }
    } else if (event.keyIdentifier === "Right") {
        if (this.selectedTreeElement.hasChildren) {
            this.selectedTreeElement.expand();
            handled = true;
        }
    }

    if (handled) {
        event.preventDefault();
        event.stopPropagation();
    }

    if (nextSelectedElement) {
        nextSelectedElement.reveal();
        nextSelectedElement.select();
    }
}

TreeOutline.prototype.expand = function()
{
    // this is the root, do nothing
}

TreeOutline.prototype.collapse = function()
{
    // this is the root, do nothing
}

TreeOutline.prototype.revealed = function()
{
    return true;
}

TreeOutline.prototype.reveal = function()
{
    // this is the root, do nothing
}

TreeOutline.prototype.appendChild = function(child)
{
    if (!child)
        throw("child can't be undefined or null");

    var lastChild = this.children[this.children.length - 1];
    if (lastChild) {
        lastChild.nextSibling = child;
        child.previousSibling = lastChild;
    }

    this._rememberTreeElement(child);

    this.children.push(child);
    this.hasChildren = true;
    child.parent = this;
    child.treeOutline = this;
    child._attach();
}

TreeOutline.prototype.removeChild = function(child)
{
    if (!child)
        throw("child can't be undefined or null");

    for (var i = 0; i < this.children.length; ++i) {
        if (this.children[i] === child) {
            this.children.splice(i, 1);
            break;
        }
    }

    this._forgetTreeElement(child);
    child._detach();
    child.removeChildren();
}

TreeOutline.prototype.removeChildren = function()
{
    for (var i = 0; i < this.children.length; ++i) {
        var child = this.children[i];
        this._forgetTreeElement(child);
        child._detach();
        child.removeChildren();
    }

    this.children = [];
}

function TreeElement(title, representedObject, hasChildren)
{
    this.title = title;
    this.identifier = TreeOutline._knownTreeElementNextIdentifier++;
    this.representedObject = representedObject;
    this.representedObject.__treeElementIdentifier = this.identifier;
    this.expanded = false;
    this.selected = false;
    this.hasChildren = hasChildren;
    this.children = [];
    this.onpopulate = null;
    this.onexpand = null;
    this.oncollapse = null;
    this.onreveal = null;
    this.onselect = null;
    this.ondblclick = null;
    this.treeOutline = null;
    this.parent = null;
    this.previousSibling = null;
    this.nextSibling = null;
    this._listItemNode = null;
}

TreeElement.prototype.updateTitle = function(title)
{
    this.title = title;
    if (this._listItemNode)
        this._listItemNode.innerHTML = title;
}

TreeElement.prototype.appendChild = function(child)
{
    if (!child)
        throw("child can't be undefined or null");

    var lastChild = this.children[this.children.length - 1];
    if (lastChild) {
        lastChild.nextSibling = child;
        child.previousSibling = lastChild;
    }

    this.treeOutline._rememberTreeElement(child);

    this.children.push(child);
    this.hasChildren = true;
    child.parent = this;
    child.treeOutline = this.treeOutline;
    if (this.expanded)
        child._attach();
}

TreeElement.prototype.removeChild = function(child)
{
    if (!child)
        throw("child can't be undefined or null");

    for (var i = 0; i < this.children.length; ++i) {
        if (this.children[i] === child) {
            this.children.splice(i, 1);
            break;
        }
    }

    this.treeOutline._forgetTreeElement(child);
    child._detach();
    child.removeChildren();
}

TreeElement.prototype._attach = function()
{
    if (!this._listItemNode || this.parent.refreshChildren) {
        if (this._listItemNode && this._listItemNode.parentNode)
            this._listItemNode.parentNode.removeChild(this._listItemNode);

        this._listItemNode = this.treeOutline._childrenListNode.document.createElement("li");
        this._listItemNode.treeElement = this;
        this._listItemNode.innerHTML = this.title;

        if (this.hasChildren)
            this._listItemNode.addStyleClass("parent");
        if (this.expanded)
            this._listItemNode.addStyleClass("expanded");
        if (this.selected)
            this._listItemNode.addStyleClass("selected");

        this._listItemNode.addEventListener("mousedown", TreeElement.treeElementSelected, false);
        this._listItemNode.addEventListener("click", TreeElement.treeElementToggled, false);
        this._listItemNode.addEventListener("dblclick", TreeElement.treeElementDoubleClicked, false);
    }

    this.parent._childrenListNode.appendChild(this._listItemNode);
    if (this._childrenListNode)
        this.parent._childrenListNode.appendChild(this._childrenListNode);
}

TreeElement.prototype._detach = function()
{
    if (this._listItemNode && this._listItemNode.parentNode)
        this._listItemNode.parentNode.removeChild(this._listItemNode);
    if (this._childrenListNode && this._childrenListNode.parentNode)
        this._childrenListNode.parentNode.removeChild(this._childrenListNode);
    delete this._listItemNode;
    delete this._childrenListNode;
}

TreeElement.prototype.removeChildren = function()
{
    for (var i = 0; i < this.children.length; ++i) {
        var child = this.children[i];
        this.treeOutline._forgetTreeElement(child);
        child._detach();
        child.removeChildren();
    }

    this.children = [];
}

TreeElement.treeElementSelected = function(event)
{
    var element = event.currentTarget;
    if (!element || !element.treeElement)
        return;

    if (event.offsetX > 20 || !element.treeElement.hasChildren)
        element.treeElement.select();
}

TreeElement.treeElementToggled = function(event)
{
    var element = event.currentTarget;
    if (!element || !element.treeElement)
        return;

    if (event.offsetX <= 20 && element.treeElement.hasChildren) {
        if (element.treeElement.expanded) {
            element.treeElement.collapse();
        } else {
            element.treeElement.expand();
        }
    }
}

TreeElement.treeElementDoubleClicked = function(event)
{
    var element = event.currentTarget;
    if (!element || !element.treeElement)
        return;

    if (element.treeElement.hasChildren && !element.treeElement.expanded)
        element.treeElement.expand();

    if (element.treeElement.ondblclick)
        element.treeElement.ondblclick(element.treeElement);
}

TreeElement.prototype.collapse = function()
{
    if (this._listItemNode)
        this._listItemNode.removeStyleClass("expanded");
    if (this._childrenListNode)
        this._childrenListNode.removeStyleClass("expanded");
    this.expanded = false;

    if (this.oncollapse)
        this.oncollapse(this);
}

TreeElement.prototype.expand = function()
{
    if (!this.hasChildren || (this.expanded && !this.refreshChildren))
        return;

    if (!this._childrenListNode || this.refreshChildren) {
        if (this._childrenListNode && this._childrenListNode.parentNode)
            this._childrenListNode.parentNode.removeChild(this._childrenListNode);

        if (this.refreshChildren)
            this.children = [];

        this._childrenListNode = this.treeOutline._childrenListNode.document.createElement("ol");
        this._childrenListNode.parentTreeElement = this;
        this._childrenListNode.addStyleClass("children");

        if (this.onpopulate)
            this.onpopulate(this);

        for (var i = 0; i < this.children.length; ++i) {
            var child = this.children[i];
            child._attach();
        }

        delete this.refreshChildren;

        if (this._listItemNode)
            this._listItemNode.parentNode.insertBefore(this._childrenListNode, this._listItemNode.nextSibling);
    }

    if (this._listItemNode)
        this._listItemNode.addStyleClass("expanded");
    if (this._childrenListNode)
        this._childrenListNode.addStyleClass("expanded");
    this.expanded = true;

    if (this.onexpand)
        this.onexpand(this);
}

TreeElement.prototype.reveal = function()
{
    var currentAncestor = this.parent;
    while (currentAncestor && !currentAncestor.root) {
        if (!currentAncestor.expanded)
            currentAncestor.expand();
        currentAncestor = currentAncestor.parent;
    }

    if (this.onreveal)
        this.onreveal(this);
}

TreeElement.prototype.revealed = function()
{
    var currentAncestor = this.parent;
    while (currentAncestor && !currentAncestor.root) {
        if (!currentAncestor.expanded)
            return false;
        currentAncestor = currentAncestor.parent;
    }

    return true;
}

TreeElement.prototype.select = function()
{
    if (this.treeOutline.selectedTreeElement) {
        this.treeOutline.selectedTreeElement.selected = false;
        if (this.treeOutline.selectedTreeElement._listItemNode)
            this.treeOutline.selectedTreeElement._listItemNode.removeStyleClass("selected");
    }

    this.selected = true;
    this.treeOutline.selectedTreeElement = this;
    if (this._listItemNode)
        this._listItemNode.addStyleClass("selected");

    if (this.onselect)
        this.onselect(this);
}

TreeElement.prototype.traverseNextTreeElement = function(skipHidden, stayWithin)
{
    if (this.hasChildren && this.onpopulate)
        this.onpopulate(this);

    var element = skipHidden ? (this.revealed() ? this.children[0] : null) : this.children[0];
    if (element && (!skipHidden || (skipHidden && this.expanded)))
        return element;

    if (this === stayWithin)
        return null;

    element = skipHidden ? (this.revealed() ? this.nextSibling : null) : this.nextSibling;
    if (element)
        return element;

    element = this;
    while (element && !element.root && !(skipHidden ? (element.revealed() ? element.nextSibling : null) : element.nextSibling) && element.parent !== stayWithin)
        element = element.parent;

    if (!element)
        return null;

    return (skipHidden ? (element.revealed() ? element.nextSibling : null) : element.nextSibling);
}

TreeElement.prototype.traversePreviousTreeElement = function(skipHidden)
{
    var element = skipHidden ? (this.revealed() ? this.previousSibling : null) : this.previousSibling;
    if (element && element.hasChildren && element.onpopulate)
        element.onpopulate(element);

    while (element && (skipHidden ? (element.revealed() && element.expanded ? element.children[element.children.length - 1] : null) : element.children[element.children.length - 1])) {
        if (element.hasChildren && element.onpopulate)
            element.onpopulate(element);
        element = (skipHidden ? (element.revealed() && element.expanded ? element.children[element.children.length - 1] : null) : element.children[element.children.length - 1]);
    }

    if (element)
        return element;

    if (this.parent.root)
        return null;

    return this.parent;
}
