#ifndef WNDTREE_H
#define WNDTREE_H

#define WEWNTLC_CLASS 0
#define WEWNTLC_HANDLE 1
#define WEWNTLC_TEXT 2
#define WEWNTLC_THREAD 3
#define WEWNTLC_MODULE 4
#define WEWNTLC_MAXIMUM 5

#define WEENFLG_HAS_CHILDREN 1

#define WEENFLG_WINDOW_VISIBLE 2

typedef enum _WEE_NODE_KIND
{
    WEENKND_SESSION,
    WEENKND_WINSTA,
    WEENKND_DESKTOP,
    WEENKND_WINDOW
} WEE_NODE_KIND;

typedef struct _WEE_BASE_NODE
{
    PH_TREENEW_NODE Node;

    struct _WEE_BASE_NODE *Parent;
    PPH_LIST Children;
    WEE_NODE_KIND Kind;
    ULONG Flags;
    PH_STRINGREF TextCache[WEWNTLC_MAXIMUM];
} WEE_BASE_NODE, *PWEE_BASE_NODE;

typedef struct _WEE_SESSION_NODE
{
    WEE_BASE_NODE BaseNode;
    DWORD SessionId;
    PPH_STRING SessionIdString;
} WEE_SESSION_NODE, *PWEE_SESSION_NODE;

typedef struct _WEE_WINSTA_NODE
{
    WEE_BASE_NODE BaseNode;
    PPH_STRING WinStationName;
} WEE_WINSTA_NODE, *PWEE_WINSTA_NODE;

struct _WEE_DESKTOP_NODE;
typedef struct _WEE_DESKTOP_NODE WEE_DESKTOP_NODE, * PWEE_DESKTOP_NODE;

typedef struct _WEE_WINDOW_NODE
{
    WEE_BASE_NODE BaseNode;
    ULONG SessionId;
    PPH_STRING DesktopName;
    PPH_STRING WinStationName;

    HWND WindowHandle;
    WCHAR WindowClass[64];
    PPH_STRING WindowText;
    CLIENT_ID ClientId;

    WCHAR WindowHandleString[PH_PTR_STR_LEN_1];
    PPH_STRING ThreadString;
    PPH_STRING ModuleString;
} WEE_WINDOW_NODE, *PWEE_WINDOW_NODE;

struct _WEE_DESKTOP_NODE
{
    WEE_WINDOW_NODE RootWindowNode;
    PPH_STRING FirstColumnText;
    ULONG FirstColumnId;
    //PPH_STRING DesktopName;
};

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

PWEE_SESSION_NODE WeeAddSessionNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ DWORD SessionId
);

PWEE_WINSTA_NODE WeeAddWinStaNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PPH_STRING WinStationName
);

PWEE_DESKTOP_NODE WeeAddDesktopNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_opt_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
);

PWEE_WINDOW_NODE WeeAddWindowNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
);

PWEE_WINDOW_NODE WeFindWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ HWND WindowHandle
    );

VOID WeRemoveBaseNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PWEE_BASE_NODE BaseNode
    );

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
