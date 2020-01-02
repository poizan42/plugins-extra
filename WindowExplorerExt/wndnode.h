#pragma once

#include <phdk.h>

#define WEENFLG_HAS_CHILDREN 1
#define WEENFLG_WINDOW_VISIBLE 2

typedef struct _WE_WINDOW_TREE_CONTEXT WE_WINDOW_TREE_CONTEXT, *PWE_WINDOW_TREE_CONTEXT;

typedef enum _WEE_NODE_KIND
{
    WEENKND_SESSION,
    WEENKND_WINSTA,
    WEENKND_DESKTOP,
    WEENKND_WINDOW,
    // Dummy node to indicate we are in the process of loading, does not have its own struct
    WEENKND_LOADING
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
    WEE_BASE_NODE BaseNode;
    ULONG SessionId;
    PPH_STRING WinStationName;
    PPH_STRING DesktopName;
    PPH_STRING DisplayString;
};

BOOLEAN WeeBaseNodeHashtableEqualFunction(
    _In_ PVOID Entry1,
    _In_ PVOID Entry2
    );

ULONG WeeBaseNodeHashtableHashFunction(
    _In_ PVOID Entry
    );

PWEE_SESSION_NODE WeeCreateSessionNode(
    _In_ DWORD SessionId
);

PWEE_SESSION_NODE WeeAddSessionNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ DWORD SessionId
);

PWEE_WINSTA_NODE WeeAddWinStaNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PPH_STRING WinStationName
);

PWEE_DESKTOP_NODE WeeCreateDesktopNode(
    _In_opt_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
);

PWEE_DESKTOP_NODE WeeAddDesktopNode(
    _Inout_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_opt_ HANDLE WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName
);

PWEE_WINDOW_NODE WeeCreateWindowNode(
    _In_ HANDLE WindowHandle,
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

PWEE_BASE_NODE WeeCreateLoadingNode();

PWEE_BASE_NODE WeeAddLoadingNode(_Inout_ PWE_WINDOW_TREE_CONTEXT Context);

VOID WeeRemoveBaseNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PWEE_BASE_NODE BaseNode
    );


VOID WeeDestroyBaseNode(
    _In_ PWEE_BASE_NODE Node
);
