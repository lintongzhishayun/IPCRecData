#include <iostream>
#include <windows.h>
#include <tchar.h>
#include <string>
#include <atlbase.h>
#include <Uxtheme.h>
#include <vsstyle.h>
#include <strsafe.h>
#include "resource.h"

using namespace std;

#define PIPE_TIMEOUT 60000
#define BUFSIZE 2048
#define WM_PIPETHREAD    WM_APP+1001


HANDLE hPipe;
HANDLE hThread[2];
DWORD WINAPI  PipeServerThreadFunctionEX(LPVOID lpParam);
DWORD WINAPI  UIThreadFunction(LPVOID lpParam);

#include <commctrl.h> 
#pragma comment(lib, "comctl32") 
#pragma comment(lib, "UxTheme.lib")
#define MAX_LOADSTRING 100
#define WINDOWS_CLASS L"Pipe Received Data"
#define WINDOWS_TITLE L"Pipe Received Data"

HINSTANCE hInst;
HWND hPipeTarget = NULL;
HWND hWnd = NULL;
static HBRUSH s_hbrBkgnd = NULL;
TCHAR szBuffer[2048];

struct STRUHANDLE
{
    HINSTANCE hInstance;
    int       nCmdShow;
    STRUHANDLE() :
        hInstance(NULL), nCmdShow(0)
    {}
};

ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    BOOL bRet = FALSE;
    /*::CreateMutex(NULL, FALSE, _T("RecPipeData"));
    if (ERROR_ALREADY_EXISTS == GetLastError())
    {
        return -1;
    }*/

    TCHAR szTemp[MAX_PATH];
    memset(szTemp, 0, MAX_PATH);
    TCHAR szExecuteCmd[MAX_PATH];
    memset(szExecuteCmd, 0, MAX_PATH);

    DWORD   dwPipeThreadId, dwUIThreadId;
    hThread[0] = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        PipeServerThreadFunctionEX,       // thread function name
        NULL,          // argument to thread function 
        0,                      // use default creation flags 
        &dwPipeThreadId);   // returns the thread identifier 

    if (hThread[0] == NULL)
    {
        TCHAR str[78];
        _stprintf_s(str, _T("CreateThread Failed %d"), GetLastError());
        OutputDebugString(str);
    }

    STRUHANDLE struHandle;
    struHandle.hInstance = hInstance;
    struHandle.nCmdShow = nCmdShow;
    hThread[1] = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        UIThreadFunction,       // thread function name
        &struHandle,          // argument to thread function 
        0,                      // use default creation flags 
        &dwUIThreadId);   // returns the thread identifier 

    if (hThread[1] == NULL)
    {
        TCHAR str[78];
        _stprintf_s(str, _T("CreateThread Failed %d"), GetLastError());
        OutputDebugString(str);
    }

    switch (WaitForMultipleObjects(2, hThread, FALSE, INFINITE))
    {
    case WAIT_OBJECT_0 + 0:
        CloseHandle(hThread[0]);
        Sleep(2000);
        ::SendMessage(hWnd, WM_DESTROY, 0, 0);
        CloseHandle(hThread[1]);
        break;
    case WAIT_OBJECT_0 + 1:
        CloseHandle(hThread[1]);
        TerminateThread(hThread[0], 1);
        CloseHandle(hThread[0]);
        break;
    case WAIT_TIMEOUT:
        OutputDebugString(_T("Wait timed out.\n"));
        break;
    default:
        break;
    };

    return bRet;
}

/* Use overlapping IO, only when the connection exists, the other end will continue to send */
DWORD WINAPI  PipeServerThreadFunctionEX(LPVOID lpParam)
{
    LPTSTR lpszPipename = _T("\\\\.\\pipe\\RecPipeData");

    hPipe = CreateNamedPipe(
        lpszPipename,             // pipe name 
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,      // read/write access 
        PIPE_TYPE_BYTE,                // blocking mode 
        1, // unlimited instances 
        0,
        0,
        PIPE_TIMEOUT,
        NULL);

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        TCHAR str[78];
        _stprintf_s(str, _T("CreateNamedPipe failed with %d."), GetLastError());
        OutputDebugString(str);
        ExitThread(-1);
    }

    BOOL fConnected = FALSE;

    OVERLAPPED ol = { 0 };
    ol.hEvent = CreateEvent(
        NULL,
        TRUE,
        FALSE,
        NULL);

    fConnected = ConnectNamedPipe(hPipe, &ol);

    if (!fConnected)
    {
        switch (GetLastError())
        {
        case ERROR_PIPE_CONNECTED:
            fConnected = TRUE;
            break;

        case ERROR_IO_PENDING:
            if (WaitForSingleObject(ol.hEvent, PIPE_TIMEOUT) == WAIT_OBJECT_0)
            {
                DWORD dwIgnore;

                fConnected = GetOverlappedResult(
                    hPipe,
                    &ol,
                    &dwIgnore,
                    FALSE);
            }
            break;
        }
    }

    if (!fConnected)
    {
        TCHAR str[78];
        _stprintf_s(str, _T("ConnectNamedPipe failed with %d."), GetLastError());
        MessageBox(NULL, str, _T("Pipe Received Data"), MB_OK | MB_ICONERROR);
        OutputDebugString(str);
        CloseHandle(hPipe);
        ExitThread(-2);
    }

    while (fConnected)
    {
        DWORD numBytesRead = 0;
        BOOL result = ReadFile(
            hPipe,
            szBuffer,
            (BUFSIZE - 1) * sizeof(wchar_t),
            &numBytesRead,
            NULL
            );

        if (result)
        {
            if (_T('\0') != szBuffer[0])
            {
                ::SendMessage(hWnd, WM_PIPETHREAD, 0, 0);
            }
        }
        else{
            TCHAR str[78];
            _stprintf_s(str, _T("ReadFile failed with %d."), GetLastError());
            OutputDebugString(str);
            CloseHandle(hPipe);
            ExitThread(-3);
        }
    }

    CloseHandle(hPipe);
    ExitThread(1);
}

/* Register the window class, draw the window, register the window callback function */
DWORD WINAPI  UIThreadFunction(LPVOID lpParam)
{
    STRUHANDLE *pStruHandle = static_cast<STRUHANDLE*>(lpParam);

    HINSTANCE hInstance = pStruHandle->hInstance;
    int nCmdShow = pStruHandle->nCmdShow;
    MSG msg;
    HACCEL hAccelTable;

    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        TCHAR str[78];
        _stprintf_s(str, _T("InitInstance Windows Failed %d"), GetLastError());
        MessageBox(NULL, (LPCWSTR)(str), _T("Windows Received Data"), MB_OK | MB_ICONERROR);
        return FALSE;
    }

    hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDS_STRING_PIPE));

    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_PIPE));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDS_STRING_PIPE);
    wcex.lpszClassName = WINDOWS_CLASS;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON_PIPE));

    return RegisterClassEx(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    POINT pos;
    GetCursorPos(&pos);
    GetDesktopWindow();

    hWnd = CreateWindow(WINDOWS_CLASS, WINDOWS_TITLE, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU /*| WS_THICKFRAME*/ | WS_MINIMIZEBOX /*| WS_MAXIMIZEBOX*/,
        GetSystemMetrics(SM_CXFULLSCREEN) - 450, GetSystemMetrics(SM_CYFULLSCREEN) / 50 /*dwHeight - 100*/, 450, 70, NULL, NULL, hInstance, NULL);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
    case  WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(124, 252, 0));
        SetBkColor(hdcStatic, RGB(0, 0, 0));
        SetBkMode(hdcStatic, TRANSPARENT);

        if (s_hbrBkgnd == NULL)
        {
            s_hbrBkgnd = CreateSolidBrush(RGB(51, 161, 201));
        }
        return (INT_PTR)s_hbrBkgnd;
    }
    break;
    case  WM_CREATE:
    {
        RECT rc;
        GetClientRect(hWnd, &rc);

        HFONT hFont = CreateFont(15,
            0,
            0,
            0,
            FALSE,
            FALSE,
            FALSE,
            FALSE,
            DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY,
            VARIABLE_PITCH,
            _T("FF_ROMAN"));

        int nX = rc.left + (rc.right - rc.left) / 100;
        int nY = rc.top + (rc.bottom - rc.top) / 100;
        int nWidth = rc.right - (rc.right - rc.left) / 50;
        int nHeight = (rc.bottom - (rc.bottom - rc.top) / 80) / 2;

        hPipeTarget = CreateWindowEx(0,
            _T("STATIC"),
            NULL,
            WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
            nX, nY, nWidth, nHeight,
            hWnd,
            NULL,
            ((LPCREATESTRUCT)lParam)->hInstance,
            NULL);

        SendMessage(hPipeTarget, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    break;
    case WM_PAINT:
        hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    case WM_DESTROY:
        DeleteObject(s_hbrBkgnd);
        PostQuitMessage(0);
        break;
    case WM_PIPETHREAD:
    {
        // split string
        /*TCHAR *ptr;
        TCHAR delim[] = TEXT("\n");
        TCHAR* token = wcstok_s(szBuffer, delim, &ptr);*/

        if (!SetWindowText(hPipeTarget, szBuffer))
        {
            TCHAR str[78];
            _stprintf_s(str, TEXT("SetWindowText Failed %d"), GetLastError());
            MessageBox(NULL, (LPCWSTR)(str), _T("Pipe Received Data"), MB_OK | MB_ICONERROR);
        }
    }
    break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}