/*
 * Process Hacker Window Explorer Extended -
 *   utility functions
 *
 * Copyright (C) 2011 wj32
 * Copyright (C) 2017-2019 dmex
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

// WARNING: No functions from ProcessHacker.exe should be used in this file!

PVOID WeGetProcedureAddress(
    _In_ PSTR Name
    )
{
    return PhGetProcedureAddress(NtCurrentPeb()->ImageBaseAddress, Name, 0);
}

VOID WeFormatLocalObjectName(
    _In_ PWSTR OriginalName,
    _Inout_updates_(256) PWCHAR Buffer,
    _Out_ PUNICODE_STRING ObjectName
    )
{
    SIZE_T length;
    SIZE_T originalNameLength;

    // Sessions other than session 0 require SeCreateGlobalPrivilege.
    if (NtCurrentPeb()->SessionId != 0)
    {
        memcpy(Buffer, L"\\Sessions\\", 10 * sizeof(WCHAR));
        _ultow(NtCurrentPeb()->SessionId, Buffer + 10, 10);
        length = wcslen(Buffer);
        originalNameLength = wcslen(OriginalName);
        memcpy(Buffer + length, OriginalName, (originalNameLength + 1) * sizeof(WCHAR));
        length += originalNameLength;

        ObjectName->Buffer = Buffer;
        ObjectName->MaximumLength = (ObjectName->Length = (USHORT)(length * sizeof(WCHAR))) + sizeof(WCHAR);
    }
    else
    {
        RtlInitUnicodeString(ObjectName, OriginalName);
    }
}

/*PPH_STRING WeeGetUserObjectName(_In_ HANDLE hObj)
{
    DWORD nameLen;
    if (!GetUserObjectInformation(hObj, UOI_NAME, NULL, 0, &nameLen) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return NULL;
    PPH_STRING name;
    name = PhCreateStringEx(NULL, nameLen - sizeof(UNICODE_NULL));
    if (!GetUserObjectInformation(hObj, UOI_NAME, PhGetString(name), nameLen, &nameLen) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        PhDereferenceObject(name);
        return NULL;
    }
    return name;
}*/

void WeeRefEnsureObjectName(_In_ HANDLE Object, _Inout_ PPH_STRING* ObjectName)
{
    if (*ObjectName)
    {
        PhReferenceObject(*ObjectName);
        return;
    }
    if (!NT_SUCCESS(WeeGetObjectName(Object, ObjectName)))
        *ObjectName = PhCreateString(L"(Unknown)");
}

void WeeRefEnsureDesktopName(_In_opt_ HDESK Desktop, _Inout_ PPH_STRING* DesktopName)
{
    if (*DesktopName && PhGetString(*DesktopName)[0] != '\\')
    {
        *DesktopName = PhConcatStrings2(L"\\", PhGetString(*DesktopName));
    }
    else if (!Desktop && *DesktopName)
    {
        PhReferenceObject(*DesktopName);
    }
    else if (Desktop)
    {
        WeeRefEnsureObjectName(Desktop, DesktopName);
    }
}

NTSTATUS WeeGetObjectName(_In_ HANDLE hObj, _Out_ PPH_STRING* ObjectName)
{
    POBJECT_NAME_INFORMATION buffer;
    ULONG bufferSize;
    bufferSize = 0x200;
    buffer = PhAllocate(bufferSize);
    NTSTATUS status = NtQueryObject(
        hObj,
        ObjectNameInformation,
        buffer,
        bufferSize,
        &bufferSize
        );

    if (status == STATUS_BUFFER_OVERFLOW || status == STATUS_INFO_LENGTH_MISMATCH ||
        status == STATUS_BUFFER_TOO_SMALL)
    {
        PhFree(buffer);
        buffer = PhAllocate(bufferSize);
        status = NtQueryObject(
            hObj,
            ObjectNameInformation,
            buffer,
            bufferSize,
            &bufferSize
        );
    }

    if (NT_SUCCESS(status))
    {
        *ObjectName = PhCreateStringFromUnicodeString(&buffer->Name);
    }

    PhFree(buffer);

    return status;
}

PH_STRINGREF WeeGetBareDesktopName(_In_ PPH_STRING DesktopName)
{
    PH_STRINGREF desktopNameRef = PhGetStringRef(DesktopName);
    if (desktopNameRef.Buffer[0] == '\\')
    {
        PhSkipStringRef(&desktopNameRef, sizeof(wchar_t));
    }
    return desktopNameRef;
}

PWSTR WeeGetBareDesktopNameZ(_In_ PWSTR DesktopName)
{
    if (DesktopName[0] == '\\')
    {
        DesktopName++;
    }
    return DesktopName;
}

BOOL WeeCompareDesktopsOnSameWinSta(
    _In_opt_ HDESK Desktop1,
    _In_opt_ PWSTR Desktop1Name,
    _In_opt_ HDESK Desktop2,
    _In_opt_ PWSTR Desktop2Name)
{
    static BOOL(*CompareObjectHandles)(HANDLE hFirstObjectHandle, HANDLE hSecondObjectHandle) = NULL;
    static BOOL compareObjectHandlesLoaded = FALSE;

    assert(Desktop1 || Desktop1Name);
    assert(Desktop2 || Desktop2Name);

    if (Desktop1 && Desktop2)
    {
        if (!compareObjectHandlesLoaded)
        {
            PVOID kernelbaseHandle = PhGetDllHandle(L"Kernelbase.dll");
            if (kernelbaseHandle)
            {
                CompareObjectHandles = PhGetProcedureAddress(kernelbaseHandle, "CompareObjectHandles", 0);
            }
            compareObjectHandlesLoaded = TRUE;
        }
        if (CompareObjectHandles)
        {
            return CompareObjectHandles(Desktop1, Desktop2);
        }
    }
    PWSTR desktop1BareName = NULL, desktop2BareName = NULL;
    if (Desktop1Name)
        desktop1BareName = WeeGetBareDesktopNameZ(Desktop1Name);
    if (Desktop2Name)
        desktop2BareName = WeeGetBareDesktopNameZ(Desktop2Name);
    PPH_STRING desktop1Name = NULL, desktop2Name = NULL;

    BOOL ret = FALSE;
    if (!desktop1BareName)
    {
        if (!Desktop1) goto clean;
        if (!NT_SUCCESS(WeeGetObjectName(Desktop1, &desktop1Name)))
            goto clean;
        desktop1BareName = PhGetString(desktop1Name);
    }
    if (!desktop2BareName)
    {
        if (!Desktop2) goto clean;
        if (!NT_SUCCESS(WeeGetObjectName(Desktop2, &desktop2Name)))
            goto clean;
        desktop2BareName = PhGetString(desktop2Name);
    }
    ret = wcscmp(desktop1BareName, desktop2BareName) == 0;
clean:
    if (desktop1Name) PhDereferenceObject(desktop1Name);
    if (desktop2Name) PhDereferenceObject(desktop2Name);
    return ret;
}

HWND WeeGetMessageRootWindow(
    _In_opt_ HWND DesktopWindow,
    _In_ BOOL IsCurrentDesktop)
{
    if (IsCurrentDesktop)
    {
        HWND firstMsgWnd, msgRoot;
        firstMsgWnd = FindWindowEx(HWND_MESSAGE, NULL, NULL, NULL);
        if (firstMsgWnd)
        {
            msgRoot = GetAncestor(firstMsgWnd, GA_PARENT);
            if (msgRoot)
                return msgRoot;
        }
    }
    if (IsCurrentDesktop && !DesktopWindow)
        DesktopWindow = GetDesktopWindow();
    if (!DesktopWindow)
        return NULL;
    /* We can't search directly for the Message-only root window, but we can try some heuristic to find it
     * from the desktop window since csrss allocates them right after each other,
     * Try 8 windows each direction. */
    WCHAR className[8];
    ULONG_PTR ulDskWnd = (ULONG_PTR)DesktopWindow;
    HWND minWnd = ulDskWnd >= 18 ? (HWND)(ulDskWnd - 16) : (HWND)2;
    HWND maxWnd = ulDskWnd <= ULONG_MAX - 18 ? (HWND)(ulDskWnd + 16) : (HWND)2;
    for (HWND tryWnd = minWnd; tryWnd <= maxWnd; tryWnd = PTR_ADD_OFFSET(tryWnd, 2))
    {
        if (tryWnd != DesktopWindow && GetClassName(tryWnd, className, 8) == 7 && memcmp(className, L"Message", 7*sizeof(WCHAR)) == 0)
            return tryWnd;
    }
    return NULL;
}

VOID WeInvertWindowBorder(
    _In_ HWND hWnd
    )
{
    RECT rect;
    HDC hdc;

    GetWindowRect(hWnd, &rect);

    if (hdc = GetWindowDC(hWnd))
    {
        ULONG penWidth = GetSystemMetrics(SM_CXBORDER) * 3;
        INT oldDc;
        HPEN pen;
        HBRUSH brush;

        oldDc = SaveDC(hdc);

        // Get an inversion effect.
        SetROP2(hdc, R2_NOT);

        pen = CreatePen(PS_INSIDEFRAME, penWidth, RGB(0x00, 0x00, 0x00));
        SelectPen(hdc, pen);

        brush = GetStockObject(NULL_BRUSH);
        SelectBrush(hdc, brush);

        // Draw the rectangle.
        Rectangle(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top);

        // Cleanup.
        DeletePen(pen);
        RestoreDC(hdc, oldDc);
        ReleaseDC(hWnd, hdc);
    }
}
