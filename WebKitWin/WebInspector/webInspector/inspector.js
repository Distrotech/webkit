/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

var Inspector;
var ignoreWhitespace = true;
var showUserAgentStyles = true;

// Property values to omit in the computed style list.
// If a property has this value, it will be omitted.
// Note we do not provide a value for "display", "height", or "width", for example,
// since we always want to display those.
var typicalStylePropertyValue = {
    "-webkit-appearance": "none",
    "-webkit-background-clip": "border",
    "-webkit-background-composite": "source-over",
    "-webkit-background-origin": "padding",
    "-webkit-background-size": "auto auto",
    "-webkit-border-fit": "border",
    "-webkit-border-horizontal-spacing": "0px",
    "-webkit-border-vertical-spacing": "0px",
    "-webkit-box-align": "stretch",
    "-webkit-box-direction": "normal",
    "-webkit-box-flex": "0",
    "-webkit-box-flex-group": "1",
    "-webkit-box-lines": "single",
    "-webkit-box-ordinal-group": "1",
    "-webkit-box-orient": "horizontal",
    "-webkit-box-pack": "start",
    "-webkit-box-shadow": "none",
    "-webkit-column-break-after" : "auto",
    "-webkit-column-break-before" : "auto",
    "-webkit-column-break-inside" : "auto",
    "-webkit-column-count" : "auto",
    "-webkit-column-gap" : "normal",
    "-webkit-column-rule-color" : "rgb(0, 0, 0)",
    "-webkit-column-rule-style" : "none",
    "-webkit-column-rule-width" : "0px",
    "-webkit-column-width" : "auto",
    "-webkit-highlight": "none",
    "-webkit-line-break": "normal",
    "-webkit-line-clamp": "none",
    "-webkit-margin-bottom-collapse": "collapse",
    "-webkit-margin-top-collapse": "collapse",
    "-webkit-marquee-direction": "auto",
    "-webkit-marquee-increment": "6px",
    "-webkit-marquee-repetition": "infinite",
    "-webkit-marquee-style": "scroll",
    "-webkit-nbsp-mode": "normal",
    "-webkit-rtl-ordering": "logical",
    "-webkit-text-decorations-in-effect": "none",
    "-webkit-text-fill-color": "rgb(0, 0, 0)",
    "-webkit-text-security": "none",
    "-webkit-text-stroke-color": "rgb(0, 0, 0)",
    "-webkit-text-stroke-width": "0",
    "-webkit-user-drag": "auto",
    "-webkit-user-modify": "read-only",
    "-webkit-user-select": "auto",
    "background-attachment": "scroll",
    "background-color": "rgba(0, 0, 0, 0)",
    "background-image": "none",
    "background-position-x": "auto",
    "background-position-y": "auto",
    "background-repeat": "repeat",
    "border-bottom-color": "rgb(0, 0, 0)",
    "border-bottom-style": "none",
    "border-bottom-width": "0px",
    "border-collapse": "separate",
    "border-left-color": "rgb(0, 0, 0)",
    "border-left-style": "none",
    "border-left-width": "0px",
    "border-right-color": "rgb(0, 0, 0)",
    "border-right-style": "none",
    "border-right-width": "0px",
    "border-top-color": "rgb(0, 0, 0)",
    "border-top-style": "none",
    "border-top-width": "0px",
    "bottom": "auto",
    "box-sizing": "content-box",
    "caption-side": "top",
    "clear": "none",
    "color": "rgb(0, 0, 0)",
    "cursor": "auto",
    "direction": "ltr",
    "empty-cells": "show",
    "float": "none",
    "font-style": "normal",
    "font-variant": "normal",
    "font-weight": "normal",
    "left": "auto",
    "letter-spacing": "normal",
    "line-height": "normal",
    "list-style-image": "none",
    "list-style-position": "outside",
    "list-style-type": "disc",
    "margin-bottom": "0px",
    "margin-left": "0px",
    "margin-right": "0px",
    "margin-top": "0px",
    "max-height": "none",
    "max-width": "none",
    "min-height": "0px",
    "min-width": "0px",
    "opacity": "1",
    "orphans": "2",
    "outline-color": "rgb(0, 0, 0)",
    "outline-style": "none",
    "outline-width": "0px",
    "overflow": "visible",
    "overflow-x": "visible",
    "overflow-y": "visible",
    "padding-bottom": "0px",
    "padding-left": "0px",
    "padding-right": "0px",
    "padding-top": "0px",
    "page-break-after": "auto",
    "page-break-before": "auto",
    "page-break-inside": "auto",
    "position": "static",
    "resize": "none",
    "right": "auto",
    "table-layout": "auto",
    "text-align": "auto",
    "text-decoration": "none",
    "text-indent": "0px",
    "text-shadow": "none",
    "text-transform": "none",
    "top": "auto",
    "unicode-bidi": "normal",
    "vertical-align": "baseline",
    "visibility": "visible",
    "white-space": "normal",
    "widows": "2",
    "word-spacing": "0px",
    "word-wrap": "normal",
    "z-index": "normal",
};

// "Nicknames" for some common values that are easier to read.
var valueNickname = {
    "rgb(0, 0, 0)": "black",
    "rgb(255, 255, 255)": "white",
    "rgba(0, 0, 0, 0)": "transparent",
};

// Display types for which margin is ignored.
var noMarginDisplayType = {
    "table-cell": "no",
    "table-column": "no",
    "table-column-group": "no",
    "table-footer-group": "no",
    "table-header-group": "no",
    "table-row": "no",
    "table-row-group": "no",
};

// Display types for which padding is ignored.
var noPaddingDisplayType = {
    "table-column": "no",
    "table-column-group": "no",
    "table-footer-group": "no",
    "table-header-group": "no",
    "table-row": "no",
    "table-row-group": "no",
};

Element.prototype.removeStyleClass = function(className) 
{
    if (this.hasStyleClass(className))
        this.className = this.className.replace(className, "");
}

Element.prototype.addStyleClass = function(className) 
{
    if (!this.hasStyleClass(className))
        this.className += (this.className.length ? " " + className : className);
}

Element.prototype.hasStyleClass = function(className) 
{
    return this.className.indexOf(className) != -1;
}

Node.prototype.firstParentWithClass = function(className) 
{
    var node = this.parentNode;
    while (node) {
        if (node.isSameNode(document)) 
            return null;
        if (node.nodeType == Node.ELEMENT_NODE && node.hasStyleClass(className))
            return node;
        node = node.parentNode;
    }

    return null;
}

Element.prototype.query = function(query) 
{
    return document.evaluate(query, this, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
}

Element.prototype.removeChildren = function()
{
    while (this.firstChild) 
        this.removeChild(this.firstChild);        
}

Element.prototype.firstChildSkippingWhitespace = firstChildSkippingWhitespace;
Element.prototype.lastChildSkippingWhitespace = lastChildSkippingWhitespace;

Node.prototype.isWhitespace = isNodeWhitespace;
Node.prototype.displayName = nodeDisplayName;
Node.prototype.contentPreview = nodeContentPreview;
Node.prototype.isAncestor = isAncestorNode;
Node.prototype.isDescendant = isDescendantNode;
Node.prototype.firstCommonAncestor = firstCommonNodeAncestor;
Node.prototype.nextSiblingSkippingWhitespace = nextSiblingSkippingWhitespace;
Node.prototype.previousSiblingSkippingWhitespace = previousSiblingSkippingWhitespace;
Node.prototype.traverseNextNode = traverseNextNode;
Node.prototype.traversePreviousNode = traversePreviousNode;

String.prototype.hasSubstring = function(string, caseInsensitive)
{
    if (!caseInsensitive)
        return (this.indexOf(string) != -1 ? true : false);
    return this.match(new RegExp(string.escapeForRegExp(), "i"));
}

String.prototype.escapeCharacters = function(chars)
{
    var foundChar = false;
    for (var i = 0; i < chars.length; ++i) {
        if (this.indexOf(chars.charAt(i)) != -1) {
            foundChar = true;
            break;
        }
    }

    if (!foundChar)
        return this;

    var result = "";
    for (var i = 0; i < this.length; ++i) {
        if (chars.indexOf(this.charAt(i)) != -1)
            result += "\\";
        result += this.charAt(i);
    }

    return result;
}

String.prototype.escapeForRegExp = function()
{
    return this.escapeCharacters("^[]{}()\\.$*+?|");
}

String.prototype.escapeHTML = function()
{
    return this.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

String.prototype.collapseWhitespace = function()
{
    return this.replace(/[\s\xA0]+/g, " ");
}

String.prototype.trimWhitespace = function()
{
    return this.replace(/^[\s\xA0]+|[\s\xA0]+$/g, "");
}

function isNodeWhitespace()
{
    if (!this || this.nodeType != Node.TEXT_NODE)
        return false;
    if (!this.nodeValue.length)
        return true;
    return this.nodeValue.match(/^[\s\xA0]+$/);
}

function nodeDisplayName()
{
    if (!this)
        return "";

    switch (this.nodeType) {
        case Node.DOCUMENT_NODE:
            return "Document";

        case Node.ELEMENT_NODE:
            var name = "<" + this.nodeName.toLowerCase();

            if (this.hasAttributes()) {
                var value = this.getAttribute("id");
                if (value)
                    name += " id=\"" + value + "" + value + "\"";
                value = this.getAttribute("class");
                if (value)
                    name += " class=\"" + value + "\"";
                if (this.nodeName.toLowerCase() == "a") {
                    value = this.getAttribute("name");
                    if (value)
                        name += " name=\"" + value + "\"";
                    value = this.getAttribute("href");
                    if (value)
                        name += " href=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() == "img") {
                    value = this.getAttribute("src");
                    if (value)
                        name += " src=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() == "iframe") {
                    value = this.getAttribute("src");
                    if (value)
                        name += " src=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() == "input") {
                    value = this.getAttribute("name");
                    if (value)
                        name += " name=\"" + value + "\"";
                    value = this.getAttribute("type");
                    if (value)
                        name += " type=\"" + value + "\"";
                } else if (this.nodeName.toLowerCase() == "form") {
                    value = this.getAttribute("action");
                    if (value)
                        name += " action=\"" + value + "\"";
                }
            }

            return name + ">";

        case Node.TEXT_NODE:
            if (isNodeWhitespace.call(this))
                return "(whitespace)";
            return "\"" + this.nodeValue + "\"";

        case Node.COMMENT_NODE:
            return "<!--" + this.nodeValue + "-->";
    }

    return this.nodeName.toLowerCase().collapseWhitespace();
}

function nodeContentPreview()
{
    if (!this || !this.hasChildNodes || !this.hasChildNodes())
        return "";

    var limit = 0;
    var preview = "";

    // always skip whitespace here
    var currentNode = traverseNextNode.call(this, true, this);
    while (currentNode) {
        if (currentNode.nodeType == Node.TEXT_NODE)
            preview += currentNode.nodeValue.escapeHTML();
        else
            preview += nodeDisplayName.call(currentNode).escapeHTML();

        currentNode = traverseNextNode.call(currentNode, true, this);

        if (++limit > 4) {
            preview += "&#x2026;"; // ellipsis
            break;
        }
    }

    return preview.collapseWhitespace();
}

function isAncestorNode(ancestor)
{
    if (!this || !ancestor)
        return false;

    var currentNode = ancestor.parentNode;
    while (currentNode) {
        if (this.isSameNode(currentNode))
            return true;
        currentNode = currentNode.parentNode;
    }

    return false;
}

function isDescendantNode(descendant)
{
    return isAncestorNode.call(descendant, this);
}

function firstCommonNodeAncestor(node)
{
    if (!this || !node)
        return;

    var node1 = this.parentNode;
    var node2 = node.parentNode;

    if ((!node1 || !node2) || !node1.isSameNode(node2))
        return null;

    while (node1 && node2) {
        if (!node1.parentNode || !node2.parentNode)
            break;
        if (!node1.isSameNode(node2))
            break;

        node1 = node1.parentNode;
        node2 = node2.parentNode;
    }

    return node1;
}

function nextSiblingSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.nextSibling;
    while (node && node.nodeType == Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = node.nextSibling;
    return node;
}

function previousSiblingSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.previousSibling;
    while (node && node.nodeType == Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = node.previousSibling;
    return node;
}

function firstChildSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.firstChild;
    while (node && node.nodeType == Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = nextSiblingSkippingWhitespace.call(node);
    return node;
}

function lastChildSkippingWhitespace()
{
    if (!this)
        return;
    var node = this.lastChild;
    while (node && node.nodeType == Node.TEXT_NODE && isNodeWhitespace.call(node))
        node = previousSiblingSkippingWhitespace.call(node);
    return node;
}

function traverseNextNode(skipWhitespace, stayWithin)
{
    if (!this)
        return;

    var node = skipWhitespace ? firstChildSkippingWhitespace.call(this) : this.firstChild;
    if (node)
        return node;

    if (stayWithin && this.isSameNode(stayWithin))
        return null;

    node = skipWhitespace ? nextSiblingSkippingWhitespace.call(this) : this.nextSibling;
    if (node)
        return node;

    node = this;
    while (node && !(skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling) && (!stayWithin || !node.parentNode || !node.parentNode.isSameNode(stayWithin)))
        node = node.parentNode;
    if (!node)
        return null;

    return skipWhitespace ? nextSiblingSkippingWhitespace.call(node) : node.nextSibling;
}

function traversePreviousNode(skipWhitespace)
{
    if (!this)
        return;
    var node = skipWhitespace ? previousSiblingSkippingWhitespace.call(this) : this.previousSibling;
    while (node && (skipWhitespace ? lastChildSkippingWhitespace.call(node) : node.lastChild) )
        node = skipWhitespace ? lastChildSkippingWhitespace.call(node) : node.lastChild;
    if (node)
        return node;
    return this.parentNode;
}

function setUpScrollbar(id)
{
    var bar = new AppleVerticalScrollbar(document.getElementById(id));

    bar.setTrackStart("Images/scrollTrackTop.png", 18);
    bar.setTrackMiddle("Images/scrollTrackMiddle.png");
    bar.setTrackEnd("Images/scrollTrackBottom.png", 18);
    bar.setThumbStart("Images/scrollThumbTop.png", 9);
    bar.setThumbMiddle("Images/scrollThumbMiddle.png");
    bar.setThumbEnd("Images/scrollThumbBottom.png", 9);

    return bar;
}

function loaded()
{
    treeOutlineScrollArea = new AppleScrollArea(document.getElementById("treeOutlineScrollView"),
        setUpScrollbar("treeOutlineScrollbar"));

    nodeContentsScrollArea = new AppleScrollArea(document.getElementById("nodeContentsScrollview"),
        setUpScrollbar("nodeContentsScrollbar"));
    elementAttributesScrollArea = new AppleScrollArea(document.getElementById("elementAttributesScrollview"),
        setUpScrollbar("elementAttributesScrollbar"));
    
    styleRulesScrollArea = new AppleScrollArea(document.getElementById("styleRulesScrollview"),
        setUpScrollbar("styleRulesScrollbar"));
    stylePropertiesScrollArea = new AppleScrollArea(document.getElementById("stylePropertiesScrollview"),
        setUpScrollbar("stylePropertiesScrollbar"));

    jsPropertiesScrollArea = new AppleScrollArea(document.getElementById("jsPropertiesScrollview"),
        setUpScrollbar("jsPropertiesScrollbar"));

    window.addEventListener("resize", refreshScrollbars, true);
    document.addEventListener("click", changeFocus, true);
    document.addEventListener("focus", changeFocus, true);
    document.addEventListener("keypress", documentKeypress, true);
    document.getElementById("splitter").addEventListener("mousedown", topAreaResizeDragStart, true);

    currentFocusElement = document.getElementById("tree");

    toggleNoSelection(false);
    switchPane("node");
}

var currentFocusElement;

function changeFocus(event)
{
    var nextFocusElement;

    var current = event.target;
    while(current) {
        if (current.nodeName == "INPUT")
            nextFocusElement = current;
        current = current.parentNode;
    }

    if (!nextFocusElement)
        nextFocusElement = event.target.firstParentWithClass("focusable");

    if (!nextFocusElement || (currentFocusElement && currentFocusElement.isSameNode(nextFocusElement)))
        return;

    if (currentFocusElement) {
        currentFocusElement.removeStyleClass("focused");
        currentFocusElement.addStyleClass("blured");
    }

    currentFocusElement = nextFocusElement;

    if (currentFocusElement) {
        currentFocusElement.addStyleClass("focused");
        currentFocusElement.removeStyleClass("blured");
    }
}

function documentKeypress(event)
{
    if (!currentFocusElement || !currentFocusElement.id || !currentFocusElement.id.length)
        return;
    if (window[currentFocusElement.id + "Keypress"])
        eval(currentFocusElement.id + "Keypress(event)");
}

function refreshScrollbars()
{
    treeOutlineScrollArea.refresh();
    elementAttributesScrollArea.refresh();
    jsPropertiesScrollArea.refresh();
    nodeContentsScrollArea.refresh();
    stylePropertiesScrollArea.refresh();
    styleRulesScrollArea.refresh();
}

var searchActive = false;
var searchQuery = "";
var searchResults = [];

function performSearch(query)
{
    var treePopup = document.getElementById("treePopup");
    var searchField = document.getElementById("search");
    var searchCount = document.getElementById("searchCount");

    if (query.length && !searchActive) {
        treePopup.style.display = "none";
        searchCount.style.display = "block";
        searchField.style.width = "150px";
        searchActive = true;
    } else if (!query.length && searchActive) {
        treePopup.style.removeProperty("display");
        searchCount.style.removeProperty("display");
        searchField.style.removeProperty("width");
        searchActive = false;
    }

    searchQuery = query;
    refreshSearch();
}

function refreshSearch()
{
    searchResults = [];

    if (searchActive) {
        // perform the search
        var treeOutline = document.getElementById("treeOutline");

        treeOutline.innerText = ""; // clear the existing tree

        treeOutlineScrollArea.refresh();
        toggleNoSelection(true);

        var count = 0;
        if (searchQuery.indexOf("/") == 0) {
            // search document by Xpath query
            try {
                var rootNodeDocument = Inspector.rootDOMNode().ownerDocument;
                var nodeList = rootNodeDocument.evaluate(searchQuery, rootNodeDocument, null, XPathResult.ORDERED_NODE_SNAPSHOT_TYPE);
                for (var i = 0; i < nodeList.snapshotLength; ++i) {
                    searchResults.push(nodeList.snapshotItem(i));
                    count++;
                }
            } catch(err) {
                // ignore any exceptions. the query might be malformed, but we allow that 
            }
        } else {
            // search document nodes by node name, node value, id and class name
            var node = Inspector.rootDOMNode().ownerDocument;
            while ((node = traverseNextNode.call(node, true))) {
                var matched = false;
                if (node.nodeName.hasSubstring(searchQuery, true))
                    matched = true;
                else if (node.nodeType == Node.TEXT_NODE && node.nodeValue.hasSubstring(searchQuery, true))
                    matched = true;
                else if (node.nodeType == Node.ELEMENT_NODE && node.id.hasSubstring(searchQuery, true))
                    matched = true;
                else if (node.nodeType == Node.ELEMENT_NODE && node.className.hasSubstring(searchQuery, true))
                    matched = true;
                if (matched) {
                    searchResults.push(node);
                    count++;
                }
            }
        }

        for (var i = 0; i < searchResults.length; ++i) {
            var outlineElement = outlineElementForNode(searchResults[i]);
            appendOutlineElement(treeOutline, outlineElement);
            if (outlineElement.expandable())
                outlineElement.collapse();
            if (i == 0)
                outlineElement.select();
        }

        var searchCountElement = document.getElementById("searchCount");
        if (count == 1)
            searchCountElement.textContent = "1 node";
        else
            searchCountElement.textContent = count + " nodes";
    } else {
        // switch back to the DOM tree and reveal the focused node
        updateTreeOutline();
    }
}

var tabNames = ["node","metrics","style","properties"];
var currentPane = "node";
var paneUpdateState = [];
var noSelection = false;

function toggleNoSelection(state)
{
    noSelection = state;
    if (noSelection) {
        for (var i = 0; i < tabNames.length; ++i)
            document.getElementById(tabNames[i] + "Pane").style.display = "none";
        document.getElementById("noSelection").style.removeProperty("display");
    } else {
        document.getElementById("noSelection").style.display = "none";
        switchPane(currentPane);
    }
}

function switchPane(pane)
{
    currentPane = pane;
    for (var i = 0; i < tabNames.length; ++i) {
        var paneElement = document.getElementById(tabNames[i] + "Pane");
        var button = document.getElementById(tabNames[i] + "Button");
        if (!button.originalClassName)
            button.originalClassName = button.className;
        if (pane == tabNames[i]) {
            if (!noSelection)
                paneElement.style.removeProperty("display");
            button.className = button.originalClassName + " selected";
        } else {
            paneElement.style.display = "none";
            button.className = button.originalClassName;
        }
    }

    if (noSelection)
        return;

    if (!paneUpdateState[pane]) {
        eval("update" + pane.charAt(0).toUpperCase() + pane.substr(1) + "Pane()");
        paneUpdateState[pane] = true;
    } else {
        refreshScrollbars();
    }
}

function nodeTypeName(node)
{
    switch (node.nodeType) {
        case Node.ELEMENT_NODE: return "Element";
        case Node.ATTRIBUTE_NODE: return "Attribute";
        case Node.TEXT_NODE: return "Text";
        case Node.CDATA_SECTION_NODE: return "Character Data";
        case Node.ENTITY_REFERENCE_NODE: return "Entity Reference";
        case Node.ENTITY_NODE: return "Entity";
        case Node.PROCESSING_INSTRUCTION_NODE: return "Processing Instruction";
        case Node.COMMENT_NODE: return "Comment";
        case Node.DOCUMENT_NODE: return "Document";
        case Node.DOCUMENT_TYPE_NODE: return "Document Type";
        case Node.DOCUMENT_FRAGMENT_NODE: return "Document Fragment";
        case Node.NOTATION_NODE: return "Notation";
    }
    return "(unknown)";
}

function treeOutlineNodeClicked(event)
{
    var element = event.currentTarget;
    if (event.offsetX > 20) {
        element.select();
    } else if (element.expandable()) {
        if (element.expanded()) {
            element.collapse();
        } else {
            element.expand();
        }
    }
}

function treeOutlineNodeDoubleClicked(event)
{
    if (searchActive)
        return;

    var element = event.currentTarget;
    if (element.representedElement && element.expandable()) {
        element.expand();
        Inspector.setRootDOMNode(element.representedElement);
    }
}

function revealedOutlineItem()
{
    if (this.parentNode && this.parentNode.treeRoot)
        return true;

    if (!this.parentNode)
        return false;

    var currentListItem = this.parentNode.parentListElement;
    while (currentListItem) {
        if (!currentListItem.expanded())
            return false;
        if (!currentListItem.parentNode)
            return false;
        if (currentListItem.parentNode.treeRoot)
            return true;
        currentListItem = currentListItem.parentNode.parentListElement;
    }

    return true;
}

function expandedOutlineItem()
{
    return this.hasStyleClass("expanded");
}

function collapseOutlineItem()
{
    this.removeStyleClass("expanded");
    if (this.childrenListElement)
        this.childrenListElement.removeStyleClass("expanded");
    treeOutlineScrollArea.refresh();
}

function expandableOutlineItem()
{
    return this.hasStyleClass("parent");
}

function expandOutlineItem()
{
    if (!this.childrenListElement || (this.childrenListElement && this.childrenListElement.ignoredWhitespace != ignoreWhitespace)) {
        var ol = document.createElement("ol");
        ol.className = "children";

        ol.ignoredWhitespace = ignoreWhitespace;
        ol.parentListElement = this;
        this.childrenListElement = ol;

        var child = this.representedElement.firstChild;
        while (child) {
            if (!ignoreWhitespace || (!isNodeWhitespace.call(child) && ignoreWhitespace))
                appendOutlineElement(ol, outlineElementForNode(child));
            child = child.nextSibling;
        }

        this.parentNode.insertBefore(ol, this.nextSibling);
    }

    this.addStyleClass("expanded");
    this.childrenListElement.addStyleClass("expanded");

    treeOutlineScrollArea.refresh();
}

function revealOutlineItem()
{
    if (this.parentNode && this.parentNode.treeRoot) {
        treeOutlineScrollArea.reveal(this);
        return;
    }

    var foundRoot = false;
    var ancestors = [];
    var currentNode = this.representedElement.parentNode;
    while (currentNode) {
        ancestors.unshift(currentNode);

        var outlineElement = outlineElementForNode(currentNode, true);
        if (outlineElement && outlineElement.parentNode && outlineElement.parentNode.treeRoot) {
            foundRoot = true;
            break;
        }

        currentNode = currentNode.parentNode;
    }

    if (!foundRoot)
        return;

    for (var i = 0; i < ancestors.length; ++i)
        outlineElementForNode(ancestors[i]).expand();

    treeOutlineScrollArea.reveal(this);
}

var currentSelectedOutlineItem;

function selectOutlineItem()
{
    if (currentSelectedOutlineItem)
        currentSelectedOutlineItem.removeStyleClass("selected");
    this.addStyleClass("selected");
    currentSelectedOutlineItem = this;
    Inspector.setFocusedDOMNode(this.representedElement);
}

function outlineElementForNode(node, dontCreate)
{
    if (node.__webInspectorTreeListItem || dontCreate)
        return node.__webInspectorTreeListItem;

    var content = nodeDisplayName.call(node).escapeHTML();
    var li = document.createElement("li");

    if (node.hasChildNodes && node.hasChildNodes()) {
        li.className = "parent";
        content += " <span class=\"content\">" + nodeContentPreview.call(node) + "</span>";
    }

    li.innerHTML = content;
    li.addEventListener("click", treeOutlineNodeClicked, false);
    li.addEventListener("dblclick", treeOutlineNodeDoubleClicked, false);
    li.representedElement = node;
    node.__webInspectorTreeListItem = li;

    li.expanded = expandedOutlineItem;
    li.expandable = expandableOutlineItem;
    li.collapse = collapseOutlineItem;
    li.expand = expandOutlineItem;
    li.reveal = revealOutlineItem;
    li.revealed = revealedOutlineItem;
    li.select = selectOutlineItem;

    return li;
}

function appendOutlineElement(list, item)
{
    if (item.parentNode && item.parentNode.isSameNode(list))
        return;

    if (item.parentNode && !item.parentNode.isSameNode(list) && item.parentNode.parentListElement) {
        var parentListItem = item.parentNode.parentListElement;

        if (item.parentNode.parentNode && !item.parentNode.parentNode.treeRoot)
            item.parentNode.parentNode.removeChild(item.parentNode);
        item.parentNode.removeChildren();

        delete parentListItem.childrenListElement;
        parentListItem.collapse();
    }

    list.appendChild(item);
    if (item.childrenListElement)
        list.insertBefore(item.childrenListElement, item.nextSibling);
}

function updateTreeOutline()
{
    if (searchActive)
        return;

    var rootNode = Inspector.rootDOMNode();
    var treeOutline = document.getElementById("treeOutline");

    treeOutline.treeRoot = true;
    treeOutline.innerText = ""; // clear the existing tree

    treeOutlineScrollArea.refresh();

    if (!rootNode)
        return;

    appendOutlineElement(treeOutline, outlineElementForNode(rootNode));

    var focusedNode = Inspector.focusedDOMNode();
    var outlineElement = outlineElementForNode(focusedNode);
    outlineElement.reveal();
    outlineElement.select();

    var rootPopup = document.getElementById("treePopup");
    
    var resetPopup = true;
    for (var i = 0; i < rootPopup.options.length; ++i) {
        if (rootPopup.options[i].representedNode == rootNode) {
            rootPopup.options[i].selected = true;
            resetPopup = false;
            break;
        }
    }
    if (!resetPopup)
        return;
    
    rootPopup.innerHTML = ""; // reset the popup

    var currentNode = rootNode;
    while (currentNode) {
        var option = document.createElement("option");
        option.representedNode = currentNode;
        option.textContent = nodeDisplayName.call(currentNode);
        if (currentNode.isSameNode(rootNode))
            option.selected = true;
        rootPopup.insertBefore(option, rootPopup.firstChild);
        currentNode = currentNode.parentNode;
    }
}

function selectNewRoot(event)
{
    var rootPopup = document.getElementById("treePopup");
    var option = rootPopup.options[rootPopup.selectedIndex];
    Inspector.setRootDOMNode(option.representedNode);
}

function treeKeypress(event)
{
    if (event.metaKey)
        return;

    event.preventDefault();
    event.stopPropagation();

    var nextFocusedNode;
    var focusedNode = Inspector.focusedDOMNode();

    if (event.keyCode == 63232) {
        // up arrow key
        nextFocusedNode = traversePreviousNode.call(focusedNode, ignoreWhitespace);
        while (nextFocusedNode) {
            var outlineElement = outlineElementForNode(nextFocusedNode, true);
            if (outlineElement && outlineElement.revealed())
                break;
            nextFocusedNode = traversePreviousNode.call(nextFocusedNode, ignoreWhitespace);
        }
    } else if (event.keyCode == 63233) {
        // down arrow key
        nextFocusedNode = traverseNextNode.call(focusedNode, ignoreWhitespace);
        while (nextFocusedNode) {
            var outlineElement = outlineElementForNode(nextFocusedNode, true);
            if (outlineElement && outlineElement.revealed())
                break;
            nextFocusedNode = traverseNextNode.call(nextFocusedNode, ignoreWhitespace);
        }
    } else if (event.keyCode == 63234) {
        // left arrow key
        var focusedOutlineElement = outlineElementForNode(focusedNode, true);
        if (focusedOutlineElement) {
            if (focusedOutlineElement.expanded())
                focusedOutlineElement.collapse();
            else if (focusedOutlineElement.parentNode && !focusedOutlineElement.parentNode.treeRoot)
                nextFocusedNode = focusedOutlineElement.parentNode.parentListElement.representedElement;
        }
    } else if (event.keyCode == 63235) {
        // right arrow key
        var focusedOutlineElement = outlineElementForNode(focusedNode, true);
        if (focusedOutlineElement && focusedOutlineElement.expandable())
            focusedOutlineElement.expand();
    }

    if (nextFocusedNode) {
        var outlineElement = outlineElementForNode(nextFocusedNode, true);
        if (outlineElement && outlineElement.revealed()) {
            outlineElement.reveal();
            outlineElement.select();
        }
    }
}

function traverseTreeBackward(event)
{
    var node;
    var focusedNode = Inspector.focusedDOMNode();

    // traverse backward, holding the opton key will traverse only to the previous sibling
    if (event.altKey)
        node = ignoreWhitespace ? previousSiblingSkippingWhitespace.call(focusedNode) : focusedNode.previousSibling;
    else
        node = traversePreviousNode.call(focusedNode, ignoreWhitespace);

    if (node) {
        var root = Inspector.rootDOMNode();
        if (!root.isSameNode(node) && !isAncestorNode.call(root, node))
            Inspector.setRootDOMNode(firstCommonNodeAncestor.call(Inspector.focusedDOMNode(), node));
        var outlineElement = outlineElementForNode(node);
        outlineElement.reveal();
        outlineElement.select();
    }
}

function traverseTreeForward(event)
{
    var node;
    var focusedNode = Inspector.focusedDOMNode();

    // traverse forward, holding the opton key will traverse only to the next sibling
    if (event.altKey)
        node = ignoreWhitespace ? nextSiblingSkippingWhitespace.call(focusedNode) : focusedNode.nextSibling;
    else
        node = traverseNextNode.call(focusedNode, ignoreWhitespace);

    if (node) {
        var root = Inspector.rootDOMNode();
        if (!root.isSameNode(node) && !isAncestorNode.call(root, node))
            Inspector.setRootDOMNode(firstCommonNodeAncestor.call(Inspector.focusedDOMNode(), node));
        var outlineElement = outlineElementForNode(node);
        outlineElement.reveal();
        outlineElement.select();
    }
}

function updatePanes()
{
    for (var i = 0; i < tabNames.length; ++i)
        paneUpdateState[tabNames[i]] = false;
    toggleNoSelection((Inspector.focusedDOMNode() ? false : true));
    if (noSelection)
        return;
    eval("update" + currentPane.charAt(0).toUpperCase() + currentPane.substr(1) + "Pane()");    
    paneUpdateState[currentPane] = true;
}

function updateElementAttributes()
{
    var focusedNode = Inspector.focusedDOMNode();
    var attributesList = document.getElementById("elementAttributesList")

    attributesList.innerText = "";

    if (!focusedNode.attributes.length)
        attributesList.innerHTML = "<span class=\"disabled\">(none)</span>";

    for (var i = 0; i < focusedNode.attributes.length; ++i) {
        var attr = focusedNode.attributes[i];
        var li = document.createElement("li");

        var span = document.createElement("span");
        span.className = "property";
        if (attr.namespaceURI)
            span.title = attr.namespaceURI;
        span.textContent = attr.name;
        li.appendChild(span);
        
        span = document.createElement("span");
        span.className = "relation";
        span.textContent = "=";
        li.appendChild(span);

        span = document.createElement("span");
        span.className = "value";
        span.textContent = "\"" + attr.value + "\"";
        span.title = attr.value;
        li.appendChild(span);

        if (attr.style) {
            span = document.createElement("span");
            span.className = "mapped";
            span.innerHTML = "(<a href=\"javascript:selectMappedStyleRule('" + attr.name + "')\">mapped style</a>)";
            li.appendChild(span);
        }

        attributesList.appendChild(li);
    }

    elementAttributesScrollArea.refresh();
}

function updateNodePane()
{
    if (!Inspector)
        return;
    var focusedNode = Inspector.focusedDOMNode();

    if (focusedNode.nodeType == Node.TEXT_NODE || focusedNode.nodeType == Node.COMMENT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.removeProperty("display");

        document.getElementById("nodeContentsScrollview").textContent = focusedNode.nodeValue;
        nodeContentsScrollArea.refresh();
    } else if (focusedNode.nodeType == Node.ELEMENT_NODE) {
        document.getElementById("elementAttributes").style.removeProperty("display");
        document.getElementById("nodeContents").style.display = "none";

        updateElementAttributes();
        
        if (focusedNode.namespaceURI.length > 0) {
            document.getElementById("nodeNamespace").textContent = focusedNode.namespaceURI;
            document.getElementById("nodeNamespace").title = focusedNode.namespaceURI;
            document.getElementById("nodeNamespaceRow").style.removeProperty("display");
        } else {
            document.getElementById("nodeNamespaceRow").style.display = "none";
        }
    } else if (focusedNode.nodeType == Node.DOCUMENT_NODE) {
        document.getElementById("nodeNamespaceRow").style.display = "none";
        document.getElementById("elementAttributes").style.display = "none";
        document.getElementById("nodeContents").style.display = "none";
    }

    document.getElementById("nodeType").textContent = nodeTypeName(focusedNode);
    document.getElementById("nodeName").textContent = focusedNode.nodeName;

    refreshScrollbars();
}

var styleRules = [];
var selectedStyleRuleIndex = 0;
var styleProperties = [];
var expandedStyleShorthands = [];

function updateStylePane()
{
    var focusedNode = Inspector.focusedDOMNode();
    if (focusedNode.nodeType == Node.TEXT_NODE && focusedNode.parentNode && focusedNode.parentNode.nodeType == Node.ELEMENT_NODE)
        focusedNode = focusedNode.parentNode;
    var rulesArea = document.getElementById("styleRulesScrollview");
    var propertiesArea = document.getElementById("stylePropertiesTree");

    rulesArea.innerText = "";
    propertiesArea.innerText = "";
    styleRules = [];
    styleProperties = [];

    if (focusedNode.nodeType == Node.ELEMENT_NODE) {
        document.getElementById("styleRules").style.removeProperty("display");
        document.getElementById("styleProperties").style.removeProperty("display");
        document.getElementById("noStyle").style.display = "none";

        var propertyCount = [];

        var computedStyle = focusedNode.ownerDocument.defaultView.getComputedStyle(focusedNode);
        if (computedStyle && computedStyle.length) {
            var computedObj = {
                isComputedStyle: true,
                selectorText: "Computed Style",
                style: computedStyle,
                subtitle: "",
            };
            styleRules.push(computedObj);
        }

        var focusedNodeName = focusedNode.nodeName.toLowerCase();
        for (var i = 0; i < focusedNode.attributes.length; ++i) {
            var attr = focusedNode.attributes[i];
            if (attr.style) {
                var attrStyle = {
                    attrName: attr.name,
                    style: attr.style,
                    subtitle: "element\u2019s \u201C" + attr.name + "\u201D attribute",
                };
                attrStyle.selectorText = focusedNodeName + "[" + attr.name;
                if (attr.value.length)
                    attrStyle.selectorText += "=" + attr.value;
                attrStyle.selectorText += "]";
                styleRules.push(attrStyle);
            }
        }

        var matchedStyleRules = focusedNode.ownerDocument.defaultView.getMatchedCSSRules(focusedNode, "", !showUserAgentStyles);
        if (matchedStyleRules) {
            for (var i = 0; i < matchedStyleRules.length; ++i) {
                styleRules.push(matchedStyleRules[i]);
            }
        }

        if (focusedNode.style.length) {
            var inlineStyle = {
                selectorText: "Inline Style Attribute",
                style: focusedNode.style,
                subtitle: "element\u2019s \u201Cstyle\u201D attribute",
            };
            styleRules.push(inlineStyle);
        }

        if (styleRules.length && selectedStyleRuleIndex >= styleRules.length)
            selectedStyleRuleIndex = (styleRules.length - 1);

        var priorityUsed = false;
        for (var i = (styleRules.length - 1); i >= 0; --i) {
            styleProperties[i] = [];

            var row = document.createElement("div");
            row.className = "row";
            if (i == selectedStyleRuleIndex)
                row.className += " selected";
            if (styleRules[i].isComputedStyle)
                row.className += " computedStyle";

            var cell = document.createElement("div");
            cell.className = "cell selector";
            var text = styleRules[i].selectorText;
            cell.textContent = text;
            cell.title = text;
            row.appendChild(cell);

            cell = document.createElement("div");
            cell.className = "cell stylesheet";
            var sheet;
            if (styleRules[i].subtitle)
                sheet = styleRules[i].subtitle;
            else if (styleRules[i].parentStyleSheet && styleRules[i].parentStyleSheet.href)
                sheet = styleRules[i].parentStyleSheet.href;
            else if (styleRules[i].parentStyleSheet && !styleRules[i].parentStyleSheet.ownerNode)
                sheet = "user agent stylesheet";
            else
                sheet = "inline stylesheet";
            cell.textContent = sheet;
            cell.title = sheet;
            row.appendChild(cell);

            row.styleRuleIndex = i;
            row.addEventListener("click", styleRuleSelect, false);

            var style = styleRules[i].style;
            var styleShorthandLookup = [];
            for (var j = 0; j < style.length; ++j) {
                var prop = null;
                var name = style[j];
                var shorthand = style.getPropertyShorthand(name);
                if (shorthand)
                    prop = styleShorthandLookup[shorthand];

                if (!priorityUsed && style.getPropertyPriority(name).length)
                    priorityUsed = true;

                if (prop) {
                    prop.subProperties.push(name);
                } else {
                    prop = {
                        style: style,
                        subProperties: [name],
                        unusedProperties: [],
                        name: (shorthand ? shorthand : name),
                    };
                    styleProperties[i].push(prop);
                    if (shorthand) {
                        styleShorthandLookup[shorthand] = prop;
                        if (!propertyCount[shorthand]) {
                            propertyCount[shorthand] = 1;
                        } else {
                            prop.unusedProperties[shorthand] = true;
                            propertyCount[shorthand]++;
                        }
                    }
                }

                if (styleRules[i].isComputedStyle)
                    continue;

                if (!propertyCount[name]) {
                    propertyCount[name] = 1;
                } else {
                    prop.unusedProperties[name] = true;
                    propertyCount[name]++;
                }
            }

            if (styleRules[i].isComputedStyle && styleRules.length > 1) {
                var divider = document.createElement("hr");
                divider.className = "divider";
                rulesArea.insertBefore(divider, rulesArea.firstChild);
            }

            if (rulesArea.firstChild)
                rulesArea.insertBefore(row, rulesArea.firstChild);
            else
                rulesArea.appendChild(row);
        }

        if (priorityUsed) {
            // walk the properties again and account for !important
            var priorityCount = [];
            for (var i = 0; i < styleRules.length; ++i) {
                if (styleRules[i].isComputedStyle)
                    continue;
                var style = styleRules[i].style;
                for (var j = 0; j < styleProperties[i].length; ++j) {
                    var prop = styleProperties[i][j];
                    for (var k = 0; k < prop.subProperties.length; ++k) {
                        var name = prop.subProperties[k];
                        if (style.getPropertyPriority(name).length) {
                            if (!priorityCount[name]) {
                                if (prop.unusedProperties[name])
                                    prop.unusedProperties[name] = false;
                                priorityCount[name] = 1;
                            } else {
                                priorityCount[name]++;
                            }
                        } else if (priorityCount[name]) {
                            prop.unusedProperties[name] = true;
                        }
                    }
                }
            }
        }

        updateStyleProperties();
    } else {
        var noStyle = document.getElementById("noStyle");
        noStyle.textContent = "Can't style " + nodeTypeName(focusedNode) + " nodes.";
        document.getElementById("styleRules").style.display = "none";
        document.getElementById("styleProperties").style.display = "none";
        noStyle.style.removeProperty("display");
    }

    styleRulesScrollArea.refresh();
}

function styleRuleSelect(event)
{
    var row = document.getElementById("styleRulesScrollview").firstChild;
    while (row) {
        if (row.nodeName == "DIV")
            row.className = "row";
        row = row.nextSibling;
    }

    row = event.currentTarget;
    row.className = "row selected";

    selectedStyleRuleIndex = row.styleRuleIndex;
    updateStyleProperties();
}

function populateStyleListItem(li, prop, name)
{
    var span = document.createElement("span");
    span.className = "property";
    span.textContent = name + ": ";
    li.appendChild(span);

    var value = prop.style.getPropertyValue(name);
    if (!value)
        return;

    span = document.createElement("span");
    span.className = "value";
    var textValue = valueNickname[value] ? valueNickname[value] : value;
    var priority = prop.style.getPropertyPriority(name);
    if (priority.length)
        textValue += " !" + priority;
    span.textContent = textValue + ";";
    span.title = textValue;
    li.appendChild(span);

    var colors = value.match(/(rgb\([0-9]+, [0-9]+, [0-9]+\))|(rgba\([0-9]+, [0-9]+, [0-9]+, [0-9]+\))/g);
    if (colors) {
        for (var k = 0; k < colors.length; ++k) {
            var swatch = document.createElement("span");
            swatch.className = "colorSwatch";
            swatch.style.backgroundColor = colors[k];
            li.appendChild(swatch);
        }
    }
}

function updateStyleProperties()
{
    var focusedNode = Inspector.focusedDOMNode();
    var propertiesTree = document.getElementById("stylePropertiesTree");
    propertiesTree.innerText = "";

    if (selectedStyleRuleIndex >= styleProperties.length) {
        stylePropertiesScrollArea.refresh();
        return;
    }

    var properties = styleProperties[selectedStyleRuleIndex];
    var omitTypicalValues = styleRules[selectedStyleRuleIndex].isComputedStyle;
    for (var i = 0; i < properties.length; ++i) {
        var prop = properties[i];
        var name = prop.name;
        if (omitTypicalValues && typicalStylePropertyValue[name] == prop.style.getPropertyValue(name))
            continue;

        var mainli = document.createElement("li");
        if (prop.subProperties.length > 1) {
            mainli.className = "hasChildren";
            if (expandedStyleShorthands[name])
                mainli.className += " expanded";
            mainli.shorthand = name;
            var button = document.createElement("button");
            button.addEventListener("click", toggleStyleShorthand, false);
            mainli.appendChild(button);
        }

        populateStyleListItem(mainli, prop, name);
        propertiesTree.appendChild(mainli);

        var overloadCount = 0;
        if (prop.subProperties && prop.subProperties.length > 1) {
            var subTree = document.createElement("ul");
            if (!expandedStyleShorthands[name])
                subTree.style.display = "none";

            for (var j = 0; j < prop.subProperties.length; ++j) {
                var name = prop.subProperties[j];
                var li = document.createElement("li");
                if (prop.style.isPropertyImplicit(name) || prop.style.getPropertyValue(name) == "initial")
                    li.className = "implicit";

                if (prop.unusedProperties[name] || prop.unusedProperties[name]) {
                    li.className += " overloaded";
                    overloadCount++;
                }

                populateStyleListItem(li, prop, name);
                subTree.appendChild(li);
            }

            propertiesTree.appendChild(subTree);
        }

        if (prop.unusedProperties[name] || overloadCount == prop.subProperties.length)
            mainli.className += " overloaded";
    }

    stylePropertiesScrollArea.refresh();
}

function toggleStyleShorthand(event)
{
    var li = event.currentTarget.parentNode;
    if (li.hasStyleClass("expanded")) {
        li.removeStyleClass("expanded");
        li.nextSibling.style.display = "none";
        expandedStyleShorthands[li.shorthand] = false;
    } else {
        li.addStyleClass("expanded");
        li.nextSibling.style.removeProperty("display");
        expandedStyleShorthands[li.shorthand] = true;
    }

    stylePropertiesScrollArea.refresh();
}

function toggleIgnoreWhitespace()
{
    ignoreWhitespace = !ignoreWhitespace;
    updateTreeOutline();
}

function toggleShowUserAgentStyles()
{
    showUserAgentStyles = !showUserAgentStyles;
    updateStylePane();
}

function selectMappedStyleRule(attrName)
{
    if (!paneUpdateState["style"])
        updateStylePane();

    for (var i = 0; i < styleRules.length; ++i)
        if (styleRules[i].attrName == attrName)
            break;

    selectedStyleRuleIndex = i;

    var row = document.getElementById("styleRulesScrollview").firstChild;
    while (row) {
        if (row.nodeName == "DIV") {
            if (row.styleRuleIndex == selectedStyleRuleIndex)
                row.className = "row selected";
            else
                row.className = "row";
        }
        row = row.nextSibling;
    }

    styleRulesScrollArea.refresh();

    updateStyleProperties();
    switchPane("style");
}

function setMetric(style, name, suffix)
{
    var value = style.getPropertyValue(name + suffix);
    if (value == "" || value == "0px")
        value = "\u2012";
    else
        value = value.replace(/px$/, "");
    document.getElementById(name).textContent = value;
}

function setBoxMetrics(style, box, suffix)
{
    setMetric(style, box + "-left", suffix);
    setMetric(style, box + "-right", suffix);
    setMetric(style, box + "-top", suffix);
    setMetric(style, box + "-bottom", suffix);
}

function updateMetricsPane()
{
    var style;
    var focusedNode = Inspector.focusedDOMNode();
    if (focusedNode.nodeType == Node.ELEMENT_NODE)
        style = focusedNode.ownerDocument.defaultView.getComputedStyle(focusedNode);
    if (!style || style.length == 0) {
        document.getElementById("noMetrics").style.removeProperty("display");
        document.getElementById("marginBoxTable").style.display = "none";
        return;
    }

    document.getElementById("noMetrics").style.display = "none";
    document.getElementById("marginBoxTable").style.removeProperty("display");

    setBoxMetrics(style, "margin", "");
    setBoxMetrics(style, "border", "-width");
    setBoxMetrics(style, "padding", "");

    var size = style.getPropertyValue("width").replace(/px$/, "")
        + " \u00D7 "
        + style.getPropertyValue("height").replace(/px$/, "");
    document.getElementById("content").textContent = size;

    if (noMarginDisplayType[style.display] == "no")
        document.getElementById("marginBoxTable").setAttribute("hide", "yes");
    else
        document.getElementById("marginBoxTable").removeAttribute("hide");

    if (noPaddingDisplayType[style.display] == "no")
        document.getElementById("paddingBoxTable").setAttribute("hide", "yes");
    else
        document.getElementById("paddingBoxTable").removeAttribute("hide");
}

function updatePropertiesPane()
{
    // FIXME: Like the style pane, this should have a top item that's "all properties"
    // and separate items for each item in the prototype chain. For now, we implement
    // only the "all properties" part, and only for enumerable properties.

    var focusedNode = Inspector.focusedDOMNode();
    var list = document.getElementById("jsPropertiesList");
    list.innerText = "";

    for (var name in focusedNode) {
        var li = document.createElement("li");

        var span = document.createElement("span");
        span.className = "property";
        span.textContent = name + ": ";
        li.appendChild(span);

        var value = focusedNode[name];

        span = document.createElement("span");
        span.className = "value";
        span.textContent = value;
        span.title = value;
        li.appendChild(span);

        list.appendChild(li);
    }

    jsPropertiesScrollArea.refresh();
}

function dividerDragStart(element, dividerDrag, dividerDragEnd, event, cursor) 
{
    element.dragging = true;
    element.dragLastY = event.clientY + window.scrollY;
    element.dragLastX = event.clientX + window.scrollX;
    document.addEventListener("mousemove", dividerDrag, true);
    document.addEventListener("mouseup", dividerDragEnd, true);
    document.body.style.cursor = cursor;
    event.preventDefault();
}

function dividerDragEnd(element, dividerDrag, dividerDragEnd, event) 
{
    element.dragging = false;
    document.removeEventListener("mousemove", dividerDrag, true);
    document.removeEventListener("mouseup", dividerDragEnd, true);
    document.body.style.removeProperty("cursor");
}

function topAreaResizeDragStart(event) 
{
    dividerDragStart(document.getElementById("splitter"), topAreaResizeDrag, topAreaResizeDragEnd, event, "row-resize");
}

function topAreaResizeDrag(event) 
{
    var element = document.getElementById("splitter");
    if (element.dragging == true) {
        var y = event.clientY + window.scrollY;
        var delta = element.dragLastY - y;
        element.dragLastY = y;

        var top = document.getElementById("top");
        top.style.height = (top.clientHeight - delta) + "px";

        var splitter = document.getElementById("splitter");
        splitter.style.top = (splitter.offsetTop - delta) + "px";

        var bottom = document.getElementById("bottom");
        bottom.style.top = (bottom.offsetTop - delta) + "px";

        window.resizeBy(0, (delta * -1));

        treeOutlineScrollArea.refresh();

        event.preventDefault();
    }
}

function topAreaResizeDragEnd(event) 
{
    dividerDragEnd(document.getElementById("splitter"), topAreaResizeDrag, topAreaResizeDragEnd, event);
}
