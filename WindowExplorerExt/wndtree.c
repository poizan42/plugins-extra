/*
 * Process Hacker Window Explorer -
 *   window treelist
 *
 * Copyright (C) 2011 wj32
 * Copyright (C) 2017-2018 dmex
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

#include "wndexp.h"
#include "resource.h"

BOOLEAN WeepBaseNodeHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    );

ULONG WeepBaseNodeHashtableHashFunction(
    _In_ PVOID Entry
    );

VOID WepDestroyBaseNode(
    _In_ PWEE_BASE_NODE WindowNode
    );

BOOLEAN NTAPI WepWindowTreeNewCallback(
    _In_ HWND hwnd,
    _In_ PH_TREENEW_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2,
    _In_opt_ PVOID Context
    );

BOOLEAN WordMatchStringRef(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PPH_STRINGREF Text
    )
{
    PH_STRINGREF part;
    PH_STRINGREF remainingPart;

    remainingPart = PhGetStringRef(Context->SearchboxText);

    while (remainingPart.Length != 0)
    {
        PhSplitStringRefAtChar(&remainingPart, '|', &part, &remainingPart);

        if (part.Length != 0)
        {
            if (PhFindStringInStringRef(Text, &part, TRUE) != -1)
                return TRUE;
        }
    }

    return FALSE;
}

BOOLEAN WordMatchStringZ(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PWSTR Text
    )
{
    PH_STRINGREF text;

    PhInitializeStringRef(&text, Text);

    return WordMatchStringRef(Context, &text);
}

BOOLEAN WeWindowTreeFilterCallback(
    _In_ PPH_TREENEW_NODE Node,
    _In_opt_ PVOID Context
    )
{
    PWE_WINDOW_TREE_CONTEXT context = Context;
    PWE_WINDOW_NODE windowNode = (PWE_WINDOW_NODE)Node;

    if (!context)
        return FALSE;

    if (PhIsNullOrEmptyString(context->SearchboxText))
        return TRUE;

    if (windowNode->WindowClass[0])
    {
        if (WordMatchStringZ(context, windowNode->WindowClass))
            return TRUE;
    }

    if (windowNode->WindowHandleString[0])
    {
        if (WordMatchStringZ(context, windowNode->WindowHandleString))
            return TRUE;
    }

    if (!PhIsNullOrEmptyString(windowNode->WindowText))
    {
        if (WordMatchStringRef(context, &windowNode->WindowText->sr))
            return TRUE;
    }

    if (!PhIsNullOrEmptyString(windowNode->ThreadString))
    {
        if (WordMatchStringRef(context, &windowNode->ThreadString->sr))
            return TRUE;
    }

    if (!PhIsNullOrEmptyString(windowNode->ModuleString))
    {
        if (WordMatchStringRef(context, &windowNode->ModuleString->sr))
            return TRUE;
    }

    return FALSE;
}

VOID WeeInitializeWindowTree(
    _In_ HWND ParentWindowHandle,
    _In_ HWND TreeNewHandle,
    _Out_ PWE_WINDOW_TREE_CONTEXT Context
    )
{
    HWND hwnd;
    PPH_STRING settings;

    memset(Context, 0, sizeof(WE_WINDOW_TREE_CONTEXT));

    Context->NodeHashtable = PhCreateHashtable(
        sizeof(PWE_WINDOW_NODE),
        WeepBaseNodeHashtableEqualFunction,
        WeepBaseNodeHashtableHashFunction,
        100
        );
    Context->NodeList = PhCreateList(100);
    Context->NodeRootList = PhCreateList(30);

    Context->ParentWindowHandle = ParentWindowHandle;
    Context->TreeNewHandle = TreeNewHandle;
    hwnd = TreeNewHandle;
    PhSetControlTheme(hwnd, L"explorer");

    TreeNew_SetCallback(hwnd, WepWindowTreeNewCallback, Context);

    PhAddTreeNewColumn(hwnd, WEWNTLC_CLASS, TRUE, L"Class", 180, PH_ALIGN_LEFT, 0, 0);
    PhAddTreeNewColumn(hwnd, WEWNTLC_HANDLE, TRUE, L"Handle", 70, PH_ALIGN_LEFT, 1, 0);
    PhAddTreeNewColumn(hwnd, WEWNTLC_TEXT, TRUE, L"Text", 220, PH_ALIGN_LEFT, 2, 0);
    PhAddTreeNewColumn(hwnd, WEWNTLC_THREAD, TRUE, L"Thread", 150, PH_ALIGN_LEFT, 3, 0);
    PhAddTreeNewColumn(hwnd, WEWNTLC_MODULE, TRUE, L"Module", 150, PH_ALIGN_LEFT, 4, 0);

    TreeNew_SetTriState(hwnd, TRUE);
    TreeNew_SetSort(hwnd, WEWNTLC_CLASS, NoSortOrder);

    settings = PhGetStringSetting(SETTING_NAME_WINDOW_TREE_LIST_COLUMNS);
    PhCmLoadSettings(hwnd, &settings->sr);
    PhDereferenceObject(settings);

    Context->SearchboxText = PhReferenceEmptyString();

    PhInitializeTreeNewFilterSupport(
        &Context->FilterSupport, 
        Context->TreeNewHandle, 
        Context->NodeList
        );

    Context->TreeFilterEntry = PhAddTreeNewFilter(
        &Context->FilterSupport,
        WeWindowTreeFilterCallback,
        Context
        );
}

VOID WeDeleteWindowTree(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    )
{
    PPH_STRING settings;
    ULONG i;

    PhRemoveTreeNewFilter(&Context->FilterSupport, Context->TreeFilterEntry);
    if (Context->SearchboxText) PhDereferenceObject(Context->SearchboxText);

    PhDeleteTreeNewFilterSupport(&Context->FilterSupport);

    settings = PhCmSaveSettings(Context->TreeNewHandle);
    PhSetStringSetting2(SETTING_NAME_WINDOW_TREE_LIST_COLUMNS, &settings->sr);
    PhDereferenceObject(settings);

    for (i = 0; i < Context->NodeList->Count; i++)
        WepDestroyBaseNode(Context->NodeList->Items[i]);

    PhDereferenceObject(Context->NodeHashtable);
    PhDereferenceObject(Context->NodeList);
    PhDereferenceObject(Context->NodeRootList);
}

BOOLEAN WeepBaseNodeHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    )
{
    PWEE_BASE_NODE node1 = *(PWEE_BASE_NODE *)Entry1;
    PWEE_BASE_NODE node2 = *(PWEE_BASE_NODE *)Entry2;

    if (node1->Kind != node2->Kind)
        return FALSE;

    PWEE_WINSTA_NODE winstaNode1, winstaNode2;
    PWE_WINDOW_NODE windowNode1, windowNode2;

    switch (node1->Kind)
    {
    case WEENKND_SESSION:
        return ((PWEE_SESSION_NODE)node1)->SessionId == ((PWEE_SESSION_NODE)node2)->SessionId;
    case WEENKND_WINSTA:
        winstaNode1 = (PWEE_WINSTA_NODE)node1;
        winstaNode2 = (PWEE_WINSTA_NODE)node2;
        return PhEqualString(winstaNode1->WinStationName, winstaNode2->WinStationName, FALSE);
    case WEENKND_DESKTOP:
    case WEENKND_WINDOW:
        windowNode1 = (PWE_WINDOW_NODE)node1;
        windowNode2 = (PWE_WINDOW_NODE)node2;
        return windowNode1->SessionId == windowNode2->SessionId &&
            windowNode1->WindowHandle == windowNode2->WindowHandle &&
            PhEqualString(windowNode1->WinStationName, windowNode2->WinStationName, FALSE) &&
            PhEqualString(windowNode1->DesktopName, windowNode2->DesktopName, FALSE);
    default:
        return FALSE;
    }
    return windowNode1->WindowHandle == windowNode2->WindowHandle;
}

ULONG WeepBaseNodeHashtableHashFunction(
    _In_ PVOID Entry
    )
{
    PWE_WINDOW_NODE wndNode;
    PWEE_WINSTA_NODE winstaNode;
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
    case WEENKND_WINDOW:
        wndNode = (PWE_WINDOW_NODE)node;
        return PhHashQuadruple(PhHashIntPtr((ULONG_PTR)wndNode->WindowHandle), PhHashStringRef(&wndNode->DesktopName->sr, FALSE),
            PhHashStringRef(&wndNode->WinStationName->sr, FALSE), PhHashInt32(wndNode->SessionId));
    default:
        return 0;
    }
}

PWEE_BASE_NODE WeepAllocBaseNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ WEE_NODE_KIND NodeKind)
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
        nodeSize = sizeof(WE_WINDOW_NODE);
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

PWEE_SESSION_NODE WeeAddSessionNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ DWORD SessionId
)
{
    PWEE_SESSION_NODE sessionNode = (PWEE_SESSION_NODE)WeepAllocBaseNode(Context, WEENKND_SESSION);
    sessionNode->SessionId = SessionId;
    sessionNode->SessionIdString = PhFormatString(L"Session %d", SessionId);

    WeepAddBaseNode(Context, &sessionNode->BaseNode);

    return sessionNode;
}

PWEE_WINSTA_NODE WeeAddWinStaNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PPH_STRING WinStationName
)
{
    PWEE_WINSTA_NODE winstaNode = (PWEE_WINSTA_NODE)WeepAllocBaseNode(Context, WEENKND_WINSTA);
    PhReferenceObject(WinStationName);
    winstaNode->WinStationName = WinStationName;

    WeepAddBaseNode(Context, &winstaNode->BaseNode);

    return winstaNode;
}

PWE_WINDOW_NODE WeepAddWindowOrDesktopNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName,
    _In_ WEE_NODE_KIND nodeKind
)
{
    PWE_WINDOW_NODE windowNode = (PWE_WINDOW_NODE)WeepAllocBaseNode(Context, nodeKind);

    windowNode->WindowHandle = WindowHandle;
    windowNode->SessionId = SessionId;
    PhReferenceObject(WinStaName);
    windowNode->WinStationName = WinStaName;
    PhReferenceObject(DesktopName);
    windowNode->DesktopName = DesktopName;

    WeepAddBaseNode(Context, &windowNode->BaseNode);

    //if (Context->FilterSupport.FilterList)
    //   windowNode->Node.Visible = PhApplyTreeNewFiltersToNode(&Context->FilterSupport, &windowNode->Node);
    //
    //TreeNew_NodesStructured(Context->TreeNewHandle);

    return windowNode;
}


PWEE_DESKTOP_NODE WeeAddDesktopNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
    )
{
    PWEE_DESKTOP_NODE deskNode = (PWEE_DESKTOP_NODE)WeepAddWindowOrDesktopNode(
        Context, WindowHandle, SessionId, WinStaName, DesktopName, WEENKND_DESKTOP);
    deskNode->FirstColumnId = MAXULONG32;
    return deskNode;
}

PWE_WINDOW_NODE WeeAddWindowNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
    )
{
    return WeepAddWindowOrDesktopNode(
        Context, WindowHandle, SessionId, WinStaName, DesktopName, WEENKND_WINDOW);
}

PWE_WINDOW_NODE WeFindWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HWND WindowHandle
    )
{
    WE_WINDOW_NODE lookupWindowNode;
    PWE_WINDOW_NODE lookupWindowNodePtr = &lookupWindowNode;
    PWE_WINDOW_NODE *windowNode;

    lookupWindowNode.WindowHandle = WindowHandle;

    windowNode = (PWE_WINDOW_NODE *)PhFindEntryHashtable(
        Context->NodeHashtable,
        &lookupWindowNodePtr
        );

    if (windowNode)
        return *windowNode;
    else
        return NULL;
}

VOID WeRemoveBaseNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PWEE_BASE_NODE Node
    )
{
    ULONG index;

    // Remove from hashtable/list and cleanup.

    PhRemoveEntryHashtable(Context->NodeHashtable, &Node);

    if ((index = PhFindItemList(Context->NodeList, Node)) != ULONG_MAX)
        PhRemoveItemList(Context->NodeList, index);

    WepDestroyBaseNode(Node);

    TreeNew_NodesStructured(Context->TreeNewHandle);
}

VOID WepDestroyBaseNode(
    _In_ PWEE_BASE_NODE Node
    )
{
    PhDereferenceObject(Node->Children);
    switch (Node->Kind)
    {
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
            if (deskNode->FirstColumnText) PhDereferenceObject(deskNode->FirstColumnText);
        }
    // Fallthrough
    case WEENKND_WINDOW:
        {
            PWE_WINDOW_NODE windowNode = (PWE_WINDOW_NODE)Node;
            if (windowNode->WindowText) PhDereferenceObject(windowNode->WindowText);
            if (windowNode->ThreadString) PhDereferenceObject(windowNode->ThreadString);
            if (windowNode->ModuleString) PhDereferenceObject(windowNode->ModuleString);
        }
        break;
    }

    PhFree(Node);
}

#define SORT_FUNCTION(Column) WepWindowTreeNewCompare##Column

#define BEGIN_SORT_FUNCTION(Column) static int __cdecl WepWindowTreeNewCompare##Column( \
    _In_ void *_context, \
    _In_ const void *_elem1, \
    _In_ const void *_elem2 \
    ) \
{ \
    PWE_WINDOW_NODE node1 = *(PWE_WINDOW_NODE *)_elem1; \
    PWE_WINDOW_NODE node2 = *(PWE_WINDOW_NODE *)_elem2; \
    int sortResult = 0;

#define END_SORT_FUNCTION \
    return PhModifySort(sortResult, ((PWE_WINDOW_TREE_CONTEXT)_context)->TreeNewSortOrder); \
}

BEGIN_SORT_FUNCTION(Class)
{
    sortResult = _wcsicmp(node1->WindowClass, node2->WindowClass);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Handle)
{
    sortResult = uintptrcmp((ULONG_PTR)node1->WindowHandle, (ULONG_PTR)node2->WindowHandle);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Text)
{
    sortResult = PhCompareString(node1->WindowText, node2->WindowText, TRUE);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Thread)
{
    sortResult = uintptrcmp((ULONG_PTR)node1->ClientId.UniqueProcess, (ULONG_PTR)node2->ClientId.UniqueProcess);

    if (sortResult == 0)
        sortResult = uintptrcmp((ULONG_PTR)node1->ClientId.UniqueThread, (ULONG_PTR)node2->ClientId.UniqueThread);
}
END_SORT_FUNCTION

BEGIN_SORT_FUNCTION(Module)
{
    sortResult = PhCompareString(node1->ModuleString, node2->ModuleString, TRUE);
}
END_SORT_FUNCTION

BOOLEAN NTAPI WepWindowTreeNewCallback(
    _In_ HWND hwnd,
    _In_ PH_TREENEW_MESSAGE Message,
    _In_opt_ PVOID Parameter1,
    _In_opt_ PVOID Parameter2,
    _In_opt_ PVOID Context
    )
{
    PWE_WINDOW_TREE_CONTEXT context;
    PWEE_BASE_NODE node;
    PWE_WINDOW_NODE wndNode;

    context = Context;

    if (!context)
        return FALSE;

    switch (Message)
    {
    case TreeNewGetChildren:
        {
            PPH_TREENEW_GET_CHILDREN getChildren = Parameter1;

            if (!getChildren)
                break;

            node = (PWEE_BASE_NODE)getChildren->Node;

            if (context->TreeNewSortOrder == NoSortOrder)
            {
                if (!node)
                {
                    getChildren->Children = (PPH_TREENEW_NODE *)context->NodeRootList->Items;
                    getChildren->NumberOfChildren = context->NodeRootList->Count;
                }
                else
                {
                    getChildren->Children = (PPH_TREENEW_NODE *)node->Children->Items;
                    getChildren->NumberOfChildren = node->Children->Count;
                }
            }
            else
            {
                if (!node)
                {
                    static PVOID sortFunctions[] =
                    {
                        SORT_FUNCTION(Class),
                        SORT_FUNCTION(Handle),
                        SORT_FUNCTION(Text),
                        SORT_FUNCTION(Thread),
                        SORT_FUNCTION(Module)
                    };
                    int (__cdecl *sortFunction)(void *, const void *, const void *);

                    if (context->TreeNewSortColumn < WEWNTLC_MAXIMUM)
                        sortFunction = sortFunctions[context->TreeNewSortColumn];
                    else
                        sortFunction = NULL;

                    if (sortFunction)
                    {
                        qsort_s(context->NodeList->Items, context->NodeList->Count, sizeof(PVOID), sortFunction, context);
                    }

                    getChildren->Children = (PPH_TREENEW_NODE *)context->NodeList->Items;
                    getChildren->NumberOfChildren = context->NodeList->Count;
                }
            }
        }
        return TRUE;
    case TreeNewIsLeaf:
        {
            PPH_TREENEW_IS_LEAF isLeaf = Parameter1;

            if (!isLeaf)
                break;

            node = (PWEE_BASE_NODE)isLeaf->Node;

            if (context->TreeNewSortOrder == NoSortOrder)
                isLeaf->IsLeaf = !(node->Flags & WEENFLG_HAS_CHILDREN);
            else
                isLeaf->IsLeaf = TRUE;
        }
        return TRUE;
    case TreeNewGetCellText:
        {
            PPH_TREENEW_GET_CELL_TEXT getCellText = Parameter1;

            if (!getCellText)
                break;

            node = (PWEE_BASE_NODE)getCellText->Node;
            ULONG firstColumnId = TreeNew_GetFirstColumn(context->TreeNewHandle)->Id;
            BOOL isFirstColumn = getCellText->Id == firstColumnId;
            switch (node->Kind)
            {
            case WEENKND_SESSION:
                if (TreeNew_GetFirstColumn(context->TreeNewHandle)->Id == getCellText->Id)
                {
                    PWEE_SESSION_NODE sessionNode = (PWEE_SESSION_NODE)node;
                    getCellText->Text = PhGetStringRef(sessionNode->SessionIdString);
                }
                break;
            case WEENKND_WINSTA:
                if (TreeNew_GetFirstColumn(context->TreeNewHandle)->Id == getCellText->Id)
                {
                    PWEE_WINSTA_NODE winstaNode = (PWEE_WINSTA_NODE)node;
                    getCellText->Text = PhGetStringRef(winstaNode->WinStationName);
                }
                break;
            case WEENKND_DESKTOP:
            case WEENKND_WINDOW:
                wndNode = (PWE_WINDOW_NODE)node;
                switch (getCellText->Id)
                {
                case WEWNTLC_CLASS:
                    PhInitializeStringRef(&getCellText->Text, wndNode->WindowClass);
                    break;
                case WEWNTLC_HANDLE:
                    PhInitializeStringRef(&getCellText->Text, wndNode->WindowHandleString);
                    break;
                case WEWNTLC_TEXT:
                    getCellText->Text = PhGetStringRef(wndNode->WindowText);
                    break;
                case WEWNTLC_THREAD:
                    getCellText->Text = PhGetStringRef(wndNode->ThreadString);
                    break;
                case WEWNTLC_MODULE:
                    getCellText->Text = PhGetStringRef(wndNode->ModuleString);
                    break;
                default:
                    return FALSE;
                }
                if (node->Kind == WEENKND_DESKTOP && isFirstColumn)
                {
                    PWEE_DESKTOP_NODE deskNode = (PWEE_DESKTOP_NODE)node;
                    if (deskNode->FirstColumnId != firstColumnId)
                    {
                        PhMoveReference(&deskNode->FirstColumnText, PhFormatString(L"%ws (Desktop %ws%ws)",
                            getCellText->Text.Buffer, PhGetString(wndNode->WinStationName), PhGetString(wndNode->DesktopName)));
                    }
                    getCellText->Text = PhGetStringRef(deskNode->FirstColumnText);
                }
                else if (node->Kind == WEENKND_WINDOW)
                {
                    getCellText->Flags = TN_CACHE;
                }

                break;
            default:
                return FALSE;
            }
        }
        return TRUE;
    case TreeNewGetNodeColor:
    {
        PPH_TREENEW_GET_NODE_COLOR getNodeColor = Parameter1;

        if (!getNodeColor)
            break;

        node = (PWEE_BASE_NODE)getNodeColor->Node;

        switch (node->Kind)
        {
        case WEENKND_WINDOW:
            if (!(node->Flags & WEENFLG_WINDOW_VISIBLE))
                getNodeColor->ForeColor = RGB(0x55, 0x55, 0x55);
        }

        getNodeColor->Flags = TN_CACHE;
    }
    return TRUE;
    case TreeNewSortChanged:
    {
        TreeNew_GetSort(hwnd, &context->TreeNewSortColumn, &context->TreeNewSortOrder);
        // Force a rebuild to sort the items.
        TreeNew_NodesStructured(hwnd);
    }
    return TRUE;
    case TreeNewKeyDown:
    {
        PPH_TREENEW_KEY_EVENT keyEvent = Parameter1;

        if (!keyEvent)
            break;

        switch (keyEvent->VirtualKey)
        {
        case 'C':
            if (GetKeyState(VK_CONTROL) < 0)
                SendMessage(context->ParentWindowHandle, WM_COMMAND, ID_WINDOW_COPY, 0);
            break;
        }
    }
    return TRUE;
    case TreeNewLeftDoubleClick:
    {
        SendMessage(context->ParentWindowHandle, WM_COMMAND, ID_WINDOW_PROPERTIES, 0);
    }
    return TRUE;
    case TreeNewContextMenu:
    {
        PPH_TREENEW_CONTEXT_MENU contextMenuEvent = Parameter1;

        SendMessage(context->ParentWindowHandle, WM_COMMAND, ID_SHOWCONTEXTMENU, (LPARAM)contextMenuEvent);
    }
    return TRUE;
    case TreeNewHeaderRightClick:
    {
        PH_TN_COLUMN_MENU_DATA data;

        data.TreeNewHandle = hwnd;
        data.MouseEvent = Parameter1;
        data.DefaultSortColumn = 0;
        data.DefaultSortOrder = AscendingSortOrder;
        PhInitializeTreeNewColumnMenu(&data);

        data.Selection = PhShowEMenu(data.Menu, hwnd, PH_EMENU_SHOW_LEFTRIGHT,
            PH_ALIGN_LEFT | PH_ALIGN_TOP, data.MouseEvent->ScreenLocation.x, data.MouseEvent->ScreenLocation.y);
        PhHandleTreeNewColumnMenu(&data);
        PhDeleteTreeNewColumnMenu(&data);
    }
    return TRUE;
    }

    return FALSE;
}

VOID WeClearWindowTree(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
)
{
    ULONG i;

    for (i = 0; i < Context->NodeList->Count; i++)
        WepDestroyBaseNode(Context->NodeList->Items[i]);

    PhClearHashtable(Context->NodeHashtable);
    PhClearList(Context->NodeList);
    PhClearList(Context->NodeRootList);
}

PWE_WINDOW_NODE WeeGetSelectedWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    )
{
    PWEE_BASE_NODE node = WeeGetSelectedBaseNode(Context);
    return node && (node->Kind == WEENKND_WINDOW || node->Kind == WEENKND_DESKTOP) ?
        (PWE_WINDOW_NODE)node :
        NULL;
}

PWEE_BASE_NODE WeeGetSelectedBaseNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    )
{
    PWEE_BASE_NODE baseNode = NULL;
    ULONG i;

    for (i = 0; i < Context->NodeList->Count; i++)
    {
        baseNode = Context->NodeList->Items[i];

        if (baseNode->Node.Selected)
            return baseNode;
    }

    return NULL;
}


VOID WeeGetSelectedWindowNodes(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _Out_ PWE_WINDOW_NODE **Nodes,
    _Out_ PULONG NumberOfWindows
    )
{
    PPH_LIST list;
    ULONG i;

    list = PhCreateList(2);

    for (i = 0; i < Context->NodeList->Count; i++)
    {
        PWEE_BASE_NODE node = Context->NodeList->Items[i];

        if (node->Node.Selected && node->Kind == WEENKND_WINDOW || node->Kind == WEENKND_DESKTOP)
        {
            PhAddItemList(list, node);
        }
    }

    *Nodes = PhAllocateCopy(list->Items, sizeof(PVOID) * list->Count);
    *NumberOfWindows = list->Count;

    PhDereferenceObject(list);
}

VOID WeeGetSelectedBaseNodes(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _Out_ PWEE_BASE_NODE **Nodes,
    _Out_ PULONG NumberOfNodes
    )
{
    PPH_LIST list;
    ULONG i;

    list = PhCreateList(2);

    for (i = 0; i < Context->NodeList->Count; i++)
    {
        PWEE_BASE_NODE node = Context->NodeList->Items[i];

        if (node->Node.Selected)
        {
            PhAddItemList(list, node);
        }
    }

    *Nodes = PhAllocateCopy(list->Items, sizeof(PVOID) * list->Count);
    *NumberOfNodes = list->Count;

    PhDereferenceObject(list);
}

VOID WeeExpandAllBaseNodes(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ BOOLEAN Expand
    )
{
    ULONG i;
    BOOLEAN needsRestructure = FALSE;

    for (i = 0; i < Context->NodeList->Count; i++)
    {
        PWEE_BASE_NODE node = Context->NodeList->Items[i];

        if (node->Children->Count != 0 && node->Node.Expanded != Expand)
        {
            node->Node.Expanded = Expand;
            needsRestructure = TRUE;
        }
    }

    if (needsRestructure)
        TreeNew_NodesStructured(Context->TreeNewHandle);
}
