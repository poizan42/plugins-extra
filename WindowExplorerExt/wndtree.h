#ifndef WNDTREE_H
#define WNDTREE_H

#define WEWNTLC_CLASS 0
#define WEWNTLC_HANDLE 1
#define WEWNTLC_TEXT 2
#define WEWNTLC_THREAD 3
#define WEWNTLC_MODULE 4
#define WEWNTLC_MAXIMUM 5

#include "wndexp.h"
#include "wndnode.h"

typedef struct _WE_WINDOW_TREE_CONTEXT
{
    HWND ParentWindowHandle;
    HWND TreeNewHandle;
    ULONG TreeNewSortColumn;
    PH_SORT_ORDER TreeNewSortOrder;

    PPH_STRING SearchboxText;
    PH_TN_FILTER_SUPPORT FilterSupport;
    PPH_TN_FILTER_ENTRY TreeFilterEntry;

    PPH_HASHTABLE NodeHashtable;
    PPH_LIST NodeList;
    PPH_LIST NodeRootList;
} WE_WINDOW_TREE_CONTEXT, *PWE_WINDOW_TREE_CONTEXT;

VOID WeeInitializeWindowTree(
    _In_ HWND ParentWindowHandle,
    _In_ HWND TreeNewHandle,
    _Out_ PWE_WINDOW_TREE_CONTEXT Context
    );

VOID WeDeleteWindowTree(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    );

/*PWEE_WINDOW_NODE WeFindWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HWND WindowHandle
    );*/

VOID WeClearWindowTree(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    );

PWEE_BASE_NODE WeeGetSelectedBaseNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    );

PWEE_WINDOW_NODE WeeGetSelectedWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context
    );

VOID WeeGetSelectedBaseNodes(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _Out_ PWEE_BASE_NODE **Nodes,
    _Out_ PULONG NumberOfNodes
    );

VOID WeeGetSelectedWindowNodes(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _Out_ PWEE_WINDOW_NODE **Nodes,
    _Out_ PULONG NumberOfWindows
    );

VOID WeeExpandAllBaseNodes(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ BOOLEAN Expand
    );

#endif
