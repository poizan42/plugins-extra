/*
 * Process Hacker Window Explorer Extended -
 *   window treelist
 *
 * Copyright (C) 2011 wj32
 * Copyright (C) 2017-2018 dmex
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

#include "wndexp.h"
#include "resource.h"

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
    PWEE_WINDOW_NODE windowNode = (PWEE_WINDOW_NODE)Node;

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
        sizeof(PWEE_WINDOW_NODE),
        WeeBaseNodeHashtableEqualFunction,
        WeeBaseNodeHashtableHashFunction,
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
        WeeDestroyBaseNode(Context->NodeList->Items[i]);

    PhDereferenceObject(Context->NodeHashtable);
    PhDereferenceObject(Context->NodeList);
    PhDereferenceObject(Context->NodeRootList);
}

/*PWEE_WINDOW_NODE WeFindWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HWND WindowHandle
    )
{
    WEE_WINDOW_NODE lookupWindowNode;
    PWEE_WINDOW_NODE lookupWindowNodePtr = &lookupWindowNode;
    PWEE_WINDOW_NODE *windowNode;

    lookupWindowNode.WindowHandle = WindowHandle;

    windowNode = (PWEE_WINDOW_NODE *)PhFindEntryHashtable(
        Context->NodeHashtable,
        &lookupWindowNodePtr
        );

    if (windowNode)
        return *windowNode;
    else
        return NULL;
}*/

#define SORT_FUNCTION(Column) WepWindowTreeNewCompare##Column

#define BEGIN_SORT_FUNCTION(Column) static int __cdecl WepWindowTreeNewCompare##Column( \
    _In_ void *_context, \
    _In_ const void *_elem1, \
    _In_ const void *_elem2 \
    ) \
{ \
    PWEE_WINDOW_NODE node1 = *(PWEE_WINDOW_NODE *)_elem1; \
    PWEE_WINDOW_NODE node2 = *(PWEE_WINDOW_NODE *)_elem2; \
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
    PWEE_WINDOW_NODE wndNode;

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
                if (isFirstColumn)
                {
                    PWEE_SESSION_NODE sessionNode = (PWEE_SESSION_NODE)node;
                    getCellText->Text = PhGetStringRef(sessionNode->SessionIdString);
                }
                break;
            case WEENKND_WINSTA:
                if (isFirstColumn)
                {
                    PWEE_WINSTA_NODE winstaNode = (PWEE_WINSTA_NODE)node;
                    getCellText->Text = PhGetStringRef(winstaNode->WinStationName);
                }
                break;
            case WEENKND_DESKTOP:
                if (isFirstColumn)
                {
                    PWEE_DESKTOP_NODE deskNode = (PWEE_DESKTOP_NODE)node;
                    getCellText->Text = PhGetStringRef(deskNode->DisplayString);
                }
                break;
            case WEENKND_WINDOW:
                wndNode = (PWEE_WINDOW_NODE)node;
                switch (getCellText->Id)
                {
                case WEWNTLC_CLASS:
                    PhInitializeStringRef(&getCellText->Text, wndNode->WindowClass);
                    break;
                case WEWNTLC_HANDLE:
                    PhInitializeStringRef(&getCellText->Text, wndNode->WindowHandleString);
                    break;
                case WEWNTLC_TEXT:
                    if (wndNode->WindowText)
                        getCellText->Text = PhGetStringRef(wndNode->WindowText);
                    break;
                case WEWNTLC_THREAD:
                    if (wndNode->ThreadString)
                        getCellText->Text = PhGetStringRef(wndNode->ThreadString);
                    break;
                case WEWNTLC_MODULE:
                    if (wndNode->ModuleString)
                        getCellText->Text = PhGetStringRef(wndNode->ModuleString);
                    break;
                default:
                    return FALSE;
                }
                getCellText->Flags = TN_CACHE;
                break;
            case WEENKND_LOADING:
                if (isFirstColumn)
                {
                    PhInitializeStringRef(&getCellText->Text, L"Loading ...");
                }
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
        WeeDestroyBaseNode(Context->NodeList->Items[i]);

    PhClearHashtable(Context->NodeHashtable);
    PhClearList(Context->NodeList);
    PhClearList(Context->NodeRootList);
}

PWEE_WINDOW_NODE WeeGetSelectedWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    )
{
    PWEE_BASE_NODE node = WeeGetSelectedBaseNode(Context);
    if (!node || (node->Kind != WEENKND_WINDOW))
        return NULL;
    PWEE_WINDOW_NODE windowNode = (PWEE_WINDOW_NODE)node;
    return windowNode->WindowHandle ? windowNode : NULL;
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
    _Out_ PWEE_WINDOW_NODE **Nodes,
    _Out_ PULONG NumberOfWindows
    )
{
    PPH_LIST list;
    ULONG i;

    list = PhCreateList(2);

    for (i = 0; i < Context->NodeList->Count; i++)
    {
        PWEE_BASE_NODE node = Context->NodeList->Items[i];

        if (node->Node.Selected && node->Kind == WEENKND_WINDOW)
        {
            PWEE_WINDOW_NODE windowNode = (PWEE_WINDOW_NODE)node;
            if (windowNode->WindowHandle)
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
