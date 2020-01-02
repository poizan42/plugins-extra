/*
 * Process Hacker Window Explorer Extended -
 *   window tree nodes
 *
 * Copyright (C) 2011 wj32
 * Copyright (C) 2016-2018 dmex
 * Copyright (C) 2020 poizan42
 *
 * This file is part of Process Hacker.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "wndtree.h"
#include "wndnode.h"

BOOLEAN WeeBaseNodeHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    )
{
    PWEE_BASE_NODE node1 = *(PWEE_BASE_NODE *)Entry1;
    PWEE_BASE_NODE node2 = *(PWEE_BASE_NODE *)Entry2;

    if (node1->Kind != node2->Kind)
        return FALSE;

    PWEE_WINSTA_NODE winstaNode1, winstaNode2;
    PWEE_DESKTOP_NODE desktopNode1, desktopNode2;
    PWEE_WINDOW_NODE windowNode1, windowNode2;

    switch (node1->Kind)
    {
    case WEENKND_SESSION:
        return ((PWEE_SESSION_NODE)node1)->SessionId == ((PWEE_SESSION_NODE)node2)->SessionId;
    case WEENKND_WINSTA:
        winstaNode1 = (PWEE_WINSTA_NODE)node1;
        winstaNode2 = (PWEE_WINSTA_NODE)node2;
        return PhEqualString(winstaNode1->WinStationName, winstaNode2->WinStationName, FALSE);
    case WEENKND_DESKTOP:
        desktopNode1 = (PWEE_DESKTOP_NODE)node1;
        desktopNode2 = (PWEE_DESKTOP_NODE)node2;
        return desktopNode1->SessionId == desktopNode2->SessionId &&
            PhEqualString(desktopNode1->DesktopName, desktopNode2->DesktopName, FALSE) &&
            PhEqualString(desktopNode1->WinStationName, desktopNode2->WinStationName, FALSE);
    case WEENKND_WINDOW:
        windowNode1 = (PWEE_WINDOW_NODE)node1;
        windowNode2 = (PWEE_WINDOW_NODE)node2;
        return windowNode1->SessionId == windowNode2->SessionId &&
            windowNode1->WindowHandle == windowNode2->WindowHandle &&
            PhEqualString(windowNode1->WinStationName, windowNode2->WinStationName, FALSE);
    default:
        return FALSE;
    }
    return windowNode1->WindowHandle == windowNode2->WindowHandle;
}

ULONG WeeBaseNodeHashtableHashFunction(
    _In_ PVOID Entry
    )
{
    PWEE_WINDOW_NODE wndNode;
    PWEE_WINSTA_NODE winstaNode;
    PWEE_DESKTOP_NODE desktopNode;
    PWEE_SESSION_NODE sessionNode;
    PWEE_BASE_NODE node = *(PWEE_BASE_NODE*)Entry;
    switch (node->Kind)
    {
    case WEENKND_SESSION:
        sessionNode = (PWEE_SESSION_NODE)node;
        return PhHashInt32(sessionNode->SessionId);
    case WEENKND_WINSTA:
        winstaNode = (PWEE_WINSTA_NODE)node;
        return PhHashStringRef(&winstaNode->WinStationName->sr, FALSE);
    case WEENKND_DESKTOP:
        desktopNode = (PWEE_DESKTOP_NODE)node;
        return WeeHashPair(PhHashStringRef(&desktopNode->WinStationName->sr, FALSE),
            PhHashStringRef(&desktopNode->WinStationName->sr, FALSE));
    case WEENKND_WINDOW:
        wndNode = (PWEE_WINDOW_NODE)node;
        return WeeHashPair(PhHashIntPtr((ULONG_PTR)wndNode->WindowHandle),
            PhHashStringRef(&wndNode->WinStationName->sr, FALSE));
    default:
        return 0;
    }
}

PWEE_BASE_NODE WeepCreateBaseNode(_In_ WEE_NODE_KIND NodeKind)
{
    size_t nodeSize;
    switch (NodeKind)
    {
    case WEENKND_SESSION:
        nodeSize = sizeof(WEE_SESSION_NODE);
        break;
    case WEENKND_WINSTA:
        nodeSize = sizeof(WEE_WINSTA_NODE);
        break;
    case WEENKND_DESKTOP:
        nodeSize = sizeof(WEE_DESKTOP_NODE);
        break;
    case WEENKND_WINDOW:
        nodeSize = sizeof(WEE_WINDOW_NODE);
        break;
    case WEENKND_LOADING:
        nodeSize = sizeof(WEE_BASE_NODE);
        break;
    default:
        return NULL;
    }
    PWEE_BASE_NODE node = PhAllocate(nodeSize);
    memset(node, 0, nodeSize);
    PhInitializeTreeNewNode(&node->Node);
    node->Kind = NodeKind;

    memset(node->TextCache, 0, sizeof(PH_STRINGREF) * WEWNTLC_MAXIMUM);
    node->Node.TextCache = node->TextCache;
    node->Node.TextCacheSize = WEWNTLC_MAXIMUM;

    node->Children = PhCreateList(1);

    return node;
}

void WeepAddBaseNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PWEE_BASE_NODE node)
{
    PhAddEntryHashtable(Context->NodeHashtable, &node);
    PhAddItemList(Context->NodeList, node);
}

PWEE_SESSION_NODE WeeCreateSessionNode(
    _In_ DWORD SessionId
)
{
    PWEE_SESSION_NODE sessionNode = (PWEE_SESSION_NODE)WeepCreateBaseNode(WEENKND_SESSION);
    sessionNode->SessionId = SessionId;
    sessionNode->SessionIdString = PhFormatString(L"Session %d", SessionId);
    return sessionNode;
}

PWEE_SESSION_NODE WeeAddSessionNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ DWORD SessionId
)
{
    PWEE_SESSION_NODE sessionNode = WeeCreateSessionNode(SessionId);
    WeepAddBaseNode(Context, &sessionNode->BaseNode);

    return sessionNode;
}

PWEE_WINSTA_NODE WeeAddWinStaNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PPH_STRING WinStationName
)
{
    PWEE_WINSTA_NODE winstaNode = (PWEE_WINSTA_NODE)WeepCreateBaseNode(WEENKND_WINSTA);
    PhReferenceObject(WinStationName);
    winstaNode->WinStationName = WinStationName;

    WeepAddBaseNode(Context, &winstaNode->BaseNode);

    return winstaNode;
}

PWEE_DESKTOP_NODE WeeCreateDesktopNode(
    _In_opt_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
)
{
    PWEE_DESKTOP_NODE deskNode = (PWEE_DESKTOP_NODE)WeepCreateBaseNode(WEENKND_DESKTOP);
    deskNode->SessionId = SessionId;
    PhReferenceObject(WinStaName);
    deskNode->WinStationName = WinStaName;
    PhReferenceObject(DesktopName);
    deskNode->DesktopName = DesktopName;
    deskNode->DisplayString = PhFormatString(
        L"Desktop %ws%ws",
        PhGetString(WinStaName),
        PhGetString(DesktopName));
    return deskNode;
}

PWEE_DESKTOP_NODE WeeAddDesktopNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_opt_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
    )
{
    PWEE_DESKTOP_NODE deskNode = WeeCreateDesktopNode(
        WindowHandle,
        SessionId,
        WinStaName,
        DesktopName
    );
    WeepAddBaseNode(Context, &deskNode->BaseNode);
    return deskNode;
}

PWEE_WINDOW_NODE WeeCreateWindowNode(
    _In_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
)
{
    PWEE_WINDOW_NODE windowNode = (PWEE_WINDOW_NODE)WeepCreateBaseNode(WEENKND_WINDOW);

    windowNode->WindowHandle = WindowHandle;
    windowNode->SessionId = SessionId;
    PhReferenceObject(WinStaName);
    windowNode->WinStationName = WinStaName;
    PhReferenceObject(DesktopName);
    windowNode->DesktopName = DesktopName;

    //if (Context->FilterSupport.FilterList)
    //   windowNode->Node.Visible = PhApplyTreeNewFiltersToNode(&Context->FilterSupport, &windowNode->Node);
    //
    //TreeNew_NodesStructured(Context->TreeNewHandle);

    return windowNode;
}

PWEE_WINDOW_NODE WeeAddWindowNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
    )
{
    PWEE_WINDOW_NODE windowNode = WeeCreateWindowNode(
        WindowHandle, SessionId, WinStaName, DesktopName);
    WeepAddBaseNode(Context, &windowNode->BaseNode);
    return windowNode;
}

PWEE_BASE_NODE WeeCreateLoadingNode()
{
    return WeepCreateBaseNode(WEENKND_LOADING);
}

PWEE_BASE_NODE WeeAddLoadingNode(_Inout_ PWE_WINDOW_TREE_CONTEXT Context)
{
    PWEE_BASE_NODE loadingNode = WeeCreateLoadingNode();
    WeepAddBaseNode(Context, loadingNode);
    return loadingNode;
}

VOID WeeDestroyBaseNode(
    _In_ PWEE_BASE_NODE Node
    )
{
    PhDereferenceObject(Node->Children);
    switch (Node->Kind)
    {
    case WEENKND_LOADING:
        break;
    case WEENKND_SESSION:
        {
            PWEE_SESSION_NODE sessionNode = (PWEE_SESSION_NODE)Node;
            if (sessionNode->SessionIdString) PhDereferenceObject(sessionNode->SessionIdString);
        }
        break;
    case WEENKND_WINSTA:
        {
            PWEE_WINSTA_NODE winstaNode = (PWEE_WINSTA_NODE)Node;
            if (winstaNode->WinStationName) PhDereferenceObject(winstaNode->WinStationName);
        }
        break;
    case WEENKND_DESKTOP:
        {
            PWEE_DESKTOP_NODE deskNode = (PWEE_DESKTOP_NODE)Node;
            if (deskNode->WinStationName) PhDereferenceObject(deskNode->WinStationName);
            if (deskNode->DesktopName) PhDereferenceObject(deskNode->DesktopName);
            if (deskNode->DisplayString) PhDereferenceObject(deskNode->DisplayString);
        }
        break;
    case WEENKND_WINDOW:
        {
            PWEE_WINDOW_NODE windowNode = (PWEE_WINDOW_NODE)Node;
            if (windowNode->WindowText) PhDereferenceObject(windowNode->WindowText);
            if (windowNode->ThreadString) PhDereferenceObject(windowNode->ThreadString);
            if (windowNode->ModuleString) PhDereferenceObject(windowNode->ModuleString);
        }
        break;
    }

    PhFree(Node);
}

VOID WeeRemoveBaseNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PWEE_BASE_NODE Node
    )
{
    ULONG index;

    // Remove from hashtable/list and cleanup.

    PhRemoveEntryHashtable(Context->NodeHashtable, &Node);

    if ((index = PhFindItemList(Context->NodeList, Node)) != ULONG_MAX)
        PhRemoveItemList(Context->NodeList, index);

    WeeDestroyBaseNode(Node);

    TreeNew_NodesStructured(Context->TreeNewHandle);
}
