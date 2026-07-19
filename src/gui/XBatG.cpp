// XBatG.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "XBatG.h"
#include <tchar.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;
TCHAR szTitle[MAX_LOADSTRING];
HWND g_hPageGeneral = NULL;
HWND g_hPageResources = NULL;

HWND g_hMainDlg = NULL;

// Forward declarations
BOOL InitInstance(HINSTANCE, int);
INT_PTR CALLBACK MainDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK GeneralPageProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ResourcesPageProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine,
    int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    SetProcessDPIAware();

    LoadString(
        hInstance,
        IDS_APP_TITLE,
        szTitle,
        MAX_LOADSTRING);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!IsDialogMessage(g_hMainDlg, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

BOOL InitInstance(
    HINSTANCE hInstance,
    int nCmdShow)
{
    hInst = hInstance;

    g_hMainDlg =
        CreateDialog(
            hInstance,
            MAKEINTRESOURCE(IDD_MAIN),
            NULL,
            MainDlgProc);

    if (!g_hMainDlg)
        return FALSE;

    ShowWindow(g_hMainDlg, nCmdShow);
    UpdateWindow(g_hMainDlg);

    return TRUE;
}

INT_PTR CALLBACK MainDlgProc(
    HWND hWnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
    {
        HICON hIcon =
            LoadIcon(
                hInst,
                MAKEINTRESOURCE(IDI_XBATG));

        SendMessage(
            hWnd,
            WM_SETICON,
            ICON_BIG,
            (LPARAM)hIcon);

        SendMessage(
            hWnd,
            WM_SETICON,
            ICON_SMALL,
            (LPARAM)hIcon);

        HMENU hMenu =
            LoadMenu(
                hInst,
                MAKEINTRESOURCE(IDC_XBATG));

        SetMenu(
            hWnd,
            hMenu);

        // Tab control initialization
        HWND hTab = GetDlgItem(hWnd, IDC_TAB_MAIN);
        TCITEM tie = { 0 };
        tie.mask = TCIF_TEXT;
        tie.pszText = (LPWSTR)_T("General");
        TabCtrl_InsertItem(hTab, 0, &tie);
        tie.pszText = (LPWSTR)_T("Resources");
        TabCtrl_InsertItem(hTab, 1, &tie);

        g_hPageGeneral = CreateDialog(hInst, MAKEINTRESOURCE(IDD_PAGE_GENERAL), hTab, GeneralPageProc);
        g_hPageResources = CreateDialog(hInst, MAKEINTRESOURCE(IDD_PAGE_RESOURCES), hTab, ResourcesPageProc);

        RECT rc;
        GetClientRect(hTab, &rc);
        TabCtrl_AdjustRect(hTab, FALSE, &rc);
        rc.left-=2;
        MoveWindow(g_hPageGeneral, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        MoveWindow(g_hPageResources, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        ShowWindow(g_hPageGeneral, SW_SHOW);
        ShowWindow(g_hPageResources, SW_HIDE);
        

        return TRUE;
    }
    case WM_NOTIFY:
    {
        LPNMHDR hdr =
            (LPNMHDR)lParam;

        if (hdr->idFrom == IDC_TAB_MAIN &&
            hdr->code == TCN_SELCHANGE)
        {
            int index =
                TabCtrl_GetCurSel(
                    GetDlgItem(hWnd, IDC_TAB_MAIN));

            ShowWindow(
                g_hPageGeneral,
                index == 0 ?
                SW_SHOW :
                SW_HIDE);

            ShowWindow(
                g_hPageResources,
                index == 1 ?
                SW_SHOW :
                SW_HIDE);
        }

        return TRUE;
    }
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_ABOUT:
            DialogBox(
                hInst,
                MAKEINTRESOURCE(IDD_ABOUTBOX),
                hWnd,
                About);
            return TRUE;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            return TRUE;
        }

        break;
    }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        return TRUE;

    case WM_DESTROY:
        PostQuitMessage(0);
        return TRUE;
    }

    return FALSE;
}

INT_PTR CALLBACK GeneralPageProc(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
    {

    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(
                hDlg,
                LOWORD(wParam));
            return TRUE;
        }

        break;
    }



    return FALSE;
}

INT_PTR CALLBACK ResourcesPageProc(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    switch (message)
    {
    case WM_INITDIALOG:
    {

    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(
                hDlg,
                LOWORD(wParam));
            return TRUE;
        }

        break;
    }

    return FALSE;
}




INT_PTR CALLBACK About(
    HWND hDlg,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);

    HWND hStcVer = GetDlgItem(hDlg, IDC_STC_VER);
    HWND hStcCpr = GetDlgItem(hDlg, IDC_STC_CPR);

    switch (message)
    {
    case WM_INITDIALOG:
        // Set text
        TCHAR szVerText[64];
        _stprintf_s(szVerText, _countof(szVerText), _T("XBatG v%s"), _T(FILE_VERSION_STRING));
        SetWindowText(hStcVer, szVerText);
        TCHAR szCprText[64];
        _stprintf_s(szCprText, _countof(szCprText), _T("2026 LeapHeap: LGPL License"));
        SetWindowText(hStcCpr, szCprText);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
            EndDialog(
                hDlg,
                LOWORD(wParam));
            return TRUE;
        }

        break;
    }

    return FALSE;
}