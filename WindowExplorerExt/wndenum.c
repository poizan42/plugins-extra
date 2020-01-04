/*
 * Process Hacker Window Explorer Extended -
 *   window enumeration 
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

#include "wndexp.h"
#include "wnddlg.h"

VOID WeepFillWindowInfo(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_ PWEE_WINDOW_NODE Node
    )
{
    PPH_STRING windowText;
    ULONG threadId;
    ULONG processId;

    PhPrintPointer(Node->WindowHandleString, Node->WindowHandle);
    GetClassName(Node->WindowHandle, Node->WindowClass, sizeof(Node->WindowClass) / sizeof(WCHAR));

    if (PhGetWindowTextEx(Node->WindowHandle, PH_GET_WINDOW_TEXT_INTERNAL, &windowText) > 0)
        PhMoveReference(&Node->WindowText, windowText);

    if (PhIsNullOrEmptyString(Node->WindowText))
        PhMoveReference(&Node->WindowText, PhReferenceEmptyString());

    threadId = GetWindowThreadProcessId(Node->WindowHandle, &processId);
    Node->ClientId.UniqueProcess = UlongToHandle(processId);
    Node->ClientId.UniqueThread = UlongToHandle(threadId);
    Node->ThreadString = PhGetClientIdName(&Node->ClientId);

    BOOLEAN isWindowVisible = IsWindowVisible(Node->WindowHandle);
    BOOLEAN hasChildren = !!FindWindowEx(Node->WindowHandle, NULL, NULL, NULL);
    Node->BaseNode.Flags =
        (isWindowVisible ? WEENFLG_WINDOW_VISIBLE : 0) |
        (hasChildren ? WEENFLG_HAS_CHILDREN : 0);

    if (processId)
    {
        HANDLE processHandle;
        PVOID instanceHandle;
        PPH_STRING fileName;

        if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, UlongToHandle(processId))))
        {
            if (!(instanceHandle = (PVOID)GetWindowLongPtr(Node->WindowHandle, GWLP_HINSTANCE)))
            {
                instanceHandle = (PVOID)GetClassLongPtr(Node->WindowHandle, GCLP_HMODULE);
            }

            if (instanceHandle)
            {
                if (NT_SUCCESS(PhGetProcessMappedFileName(processHandle, instanceHandle, &fileName)))
                {
                    PhMoveReference(&fileName, PhGetFileName(fileName));
                    PhMoveReference(&fileName, PhGetBaseName(fileName));

                    PhMoveReference(&Node->ModuleString, fileName);
                }
            }

            NtClose(processHandle);
        }
    }

    if (Context->FilterSupport.FilterList) // Note: Apply filter after filling window node data. (dmex)
        Node->BaseNode.Node.Visible = PhApplyTreeNewFiltersToNode(&Context->FilterSupport, &Node->BaseNode.Node);
}

PWEE_WINDOW_NODE WeepAddChildWindowNode(
    _In_ PWE_WINDOW_TREE_CONTEXT Context,
    _In_opt_ PWEE_BASE_NODE ParentNode,
    _In_ HWND WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName

    )
{
    PWEE_WINDOW_NODE childNode;

    childNode = WeeAddWindowNode(Context, WindowHandle, SessionId, WinStaName, DesktopName);

    WeepFillWindowInfo(Context, childNode);

    if (ParentNode)
    {
        // This is a child node.
        childNode->BaseNode.Node.Expanded = TRUE;
        childNode->BaseNode.Parent = ParentNode;

        PhAddItemList(ParentNode->Children, childNode);
    }
    else
    {
        // This is a root node.
        childNode->BaseNode.Node.Expanded = TRUE;

        PhAddItemList(Context->NodeRootList, childNode);
    }

    return childNode;
}

VOID WeepAddChildWindows(
    _In_ PWINDOWS_CONTEXT Context,
    _In_opt_ PWEE_WINDOW_NODE ParentNode,
    _In_ HWND WindowHandle,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_ PPH_STRING DesktopName,
    _In_opt_ HANDLE FilterProcessId,
    _In_opt_ HANDLE FilterThreadId
    )
{
    HWND childWindow = NULL;
    ULONG i = 0;

    // We use FindWindowEx because EnumWindows doesn't return Metro app windows.
    // Set a reasonable limit to prevent infinite loops.
    while (i < 0x800 && (childWindow = FindWindowEx(WindowHandle, childWindow, NULL, NULL)))
    {
        ULONG processId;
        ULONG threadId;

        threadId = GetWindowThreadProcessId(childWindow, &processId);

        if (
            (!FilterProcessId || UlongToHandle(processId) == FilterProcessId) &&
            (!FilterThreadId || UlongToHandle(threadId) == FilterThreadId)
            )
        {
            PWEE_WINDOW_NODE childNode = WeepAddChildWindowNode(&Context->TreeContext, (PWEE_BASE_NODE)ParentNode, childWindow, SessionId, WinStaName, DesktopName);

            if (childNode->BaseNode.Flags & WEENFLG_HAS_CHILDREN)
            {
                WeepAddChildWindows(Context, childNode, childWindow, SessionId, WinStaName, DesktopName, NULL, NULL);
            }
        }

        i++;
    }
}

BOOL CALLBACK WepEnumDesktopWindowsProc(
    _In_ HWND hwnd,
    _In_ LPARAM lParam
    )
{
    PWINDOWS_ENUM_CONTEXT context = (PWINDOWS_ENUM_CONTEXT)lParam;

    WeepAddChildWindowNode(&context->WindowsContext->TreeContext, context->ParentNode, hwnd, context->SessionId, context->WinStaName, context->DesktopName);

    return TRUE;
}

VOID WeepAddDesktopWindows(
    _In_ PWINDOWS_CONTEXT Context,
    _In_opt_ PWEE_BASE_NODE ParentNode,
    _In_opt_ HDESK Desktop,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WinStaName,
    _In_opt_ PPH_STRING DesktopName
    )
{
    assert(Desktop || DesktopName);

    BOOL closeDesktop = FALSE;
    BOOL refDesktopName = FALSE;
    if (Desktop && !DesktopName)
    {
        WeeRefEnsureObjectName(Desktop, &DesktopName);
        refDesktopName = TRUE;
    }
    if (!Desktop && DesktopName)
    {
        if (!(Desktop = OpenDesktop(WeeGetBareDesktopName(DesktopName).Buffer, 0, FALSE, DESKTOP_ENUMERATE)))
            return;
        closeDesktop = TRUE;
    }
    

    WINDOWS_ENUM_CONTEXT enumContext = { 0 };
    enumContext.WindowsContext = Context;
    enumContext.SessionId = SessionId;
    enumContext.WinStaName = WinStaName;
    enumContext.DesktopName = DesktopName;
    enumContext.ParentNode = ParentNode;
    EnumDesktopWindows(Desktop, WepEnumDesktopWindowsProc, (LPARAM)&enumContext);
    if (closeDesktop) CloseDesktop(Desktop);
    if (DesktopName && refDesktopName) PhDereferenceObject(DesktopName);
}

WeepAddDesktopWithWindows(
    _In_ PWINDOWS_CONTEXT Context,
    _In_ PWEE_BASE_NODE ParentNode,
    _In_opt_ HDESK Desktop,
    _In_opt_ HWND DesktopWindow,
    _In_ DWORD SessionId,
    _In_ PPH_STRING WindowStationName,
    _In_opt_ PPH_STRING DesktopName
)
{
    WeeRefEnsureDesktopName(Desktop, &DesktopName);
    BOOL isCurrentDesktop = WeeCompareDesktopsOnSameWinSta(Desktop, PhGetString(DesktopName), WeeCurrentDesktop, PhGetString(WeeCurrentDesktopName));
    if (!DesktopWindow && isCurrentDesktop)
    {
        DesktopWindow = GetDesktopWindow();
    }

    PWEE_DESKTOP_NODE desktopNode = (PWEE_DESKTOP_NODE)WeeAddDesktopNode(&Context->TreeContext, DesktopWindow, SessionId, WindowStationName, DesktopName);
    if (DesktopWindow)
    {
        PWEE_WINDOW_NODE desktopWndNode = WeepAddChildWindowNode(&Context->TreeContext, &desktopNode->BaseNode, DesktopWindow, SessionId, WindowStationName, DesktopName);
        WeepAddChildWindows(Context, desktopWndNode, DesktopWindow, SessionId, WindowStationName, DesktopName, NULL, NULL);
        HWND messageWnd = WeeGetMessageRootWindow(DesktopWindow, isCurrentDesktop);
        if (messageWnd)
        {
            PWEE_WINDOW_NODE messageWndNode = WeepAddChildWindowNode(&Context->TreeContext, &desktopNode->BaseNode, messageWnd, SessionId, WindowStationName, DesktopName);
            WeepAddChildWindows(Context, messageWndNode, messageWnd, SessionId, WindowStationName, DesktopName, NULL, NULL);
        }
    }
    else
    {
        WeepAddDesktopWindows(Context, &desktopNode->BaseNode, Desktop, SessionId, WindowStationName, DesktopName);
    }

    desktopNode->BaseNode.Flags |= WEENFLG_HAS_CHILDREN;
    if (ParentNode)
    {
        desktopNode->BaseNode.Parent = ParentNode;
        PhAddItemList(ParentNode->Children, desktopNode);
        ParentNode->Node.Expanded = TRUE;
        ParentNode->Flags |= WEENFLG_HAS_CHILDREN;
    }

    if (DesktopName) PhDereferenceObject(DesktopName);
}

static BOOL CALLBACK WeepEnumDesktopsProc(
    _In_ LPWSTR lpszDesktop,
    _In_ LPARAM lParam
    )
{
    PWINDOWS_ENUM_CONTEXT enumContext = (PWINDOWS_ENUM_CONTEXT)lParam;
    PPH_STRING desktopName;
    if (lpszDesktop[0] == '\\')
        desktopName = PhCreateString(lpszDesktop);
    else
        desktopName = PhConcatStrings2(L"\\", lpszDesktop);
    WeepAddDesktopWithWindows(
        enumContext->WindowsContext,
        enumContext->ParentNode,
        NULL,
        NULL,
        enumContext->SessionId,
        enumContext->WinStaName,
        desktopName);
    PhDereferenceObject(desktopName);
    return TRUE;
}

VOID WeepAddDesktopsForCurrentWinSta(
    _In_ PWINDOWS_CONTEXT Context,
    _In_ PWEE_BASE_NODE ParentNode
)
{
    WINDOWS_ENUM_CONTEXT enumContext = { 0 };
    enumContext.WindowsContext = Context;
    enumContext.SessionId = NtCurrentPeb()->SessionId;
    enumContext.WinStaName = WeeCurrentWindowStationName;
    enumContext.ParentNode = ParentNode;
    if (!EnumDesktops(WeeCurrentWindowStation, WeepEnumDesktopsProc, (LPARAM)&enumContext))
    {
        WeepAddDesktopWithWindows(
            Context,
            ParentNode,
            WeeCurrentDesktop,
            GetDesktopWindow(),
            NtCurrentPeb()->SessionId,
            WeeCurrentWindowStationName,
            WeeCurrentDesktopName);
    }
}
