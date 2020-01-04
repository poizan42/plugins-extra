// Private header shared between wndenum.c and wnddlg.c
#pragma once

typedef struct _WINDOWS_CONTEXT
{
    HWND TreeNewHandle;
    HWND SearchBoxHandle;
    WE_WINDOW_TREE_CONTEXT TreeContext;
    WE_WINDOW_SELECTOR Selector;

    PH_LAYOUT_MANAGER LayoutManager;

    HWND HighlightingWindow;
    ULONG HighlightingWindowCount;
} WINDOWS_CONTEXT, *PWINDOWS_CONTEXT;

typedef struct _WINDOWS_ENUM_CONTEXT
{
    PWINDOWS_CONTEXT WindowsContext;
    DWORD SessionId;
    PPH_STRING WinStaName;
    PPH_STRING DesktopName;
    PWEE_BASE_NODE ParentNode;
} WINDOWS_ENUM_CONTEXT, *PWINDOWS_ENUM_CONTEXT;


VOID WeepAddDesktopsForCurrentWinSta(
    _In_ PWINDOWS_CONTEXT Context,
    _In_ PWEE_BASE_NODE ParentNode
);
