#include <windows.h>
#include <time.h>
#include <conio.h>
#include <stdio.h>
#include <tchar.h>
#include <iostream>

#define PIPE_TIMEOUT 60000
#define BUFSIZE 2048

int WindowsCopyData(){

    TCHAR szDlgTitle[] = TEXT("Pipe Received Data");
    /*HWND hSendWindow = GetConsoleWindow();

    if (hSendWindow == NULL)
        return -1;*/

    // HWND hRecvWindow = FindWindow(NULL, szDlgTitle);
    HWND hRecvWindow = FindWindowEx(0, 0, NULL, szDlgTitle);

    if (hRecvWindow == NULL)
        return -1;

    TCHAR szSendBuf[100];
    COPYDATASTRUCT CopyData;

    for (size_t i = 0; i < 10; i++)
    {
        _stprintf_s(szSendBuf, _T("Hello CopyData [%d] !"), i);

        CopyData.dwData = 1;
        CopyData.cbData = sizeof(szSendBuf);
        CopyData.lpData = szSendBuf;

        SendMessage(hRecvWindow, WM_COPYDATA, NULL, (LPARAM)&CopyData);
        std::wcout << szSendBuf << std::endl;
        Sleep(1000);
    }
}

void PipeCopyData(){
    HANDLE hPipe;
    BOOL   fSuccess = FALSE;
    DWORD  cbToWrite, cbWritten;
    LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\RecPipeData");

    // Try to open a named pipe; wait for it, if necessary. 

    while (1)
    {
        hPipe = CreateFile(
            lpszPipename,   // pipe name 
            GENERIC_WRITE,  // read and write access 
            0,              // no sharing 
            NULL,           // default security attributes
            OPEN_EXISTING,  // opens existing pipe 
            0,              // default attributes 
            NULL);          // no template file 

        // Break if the pipe handle is valid. 

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            break;
        }

        // All pipe instances are busy, so wait for 2s . 

        if (!WaitNamedPipe(lpszPipename, 2000))
        {
            TCHAR str[78];
            _stprintf_s(str, _T("Could not open pipe: 2s wait timed out."));
            MessageBox(NULL, str, _T("Pipe Send Data"), MB_OK | MB_ICONERROR);
        }
    }

    // Send a message to the pipe server. 

    for (size_t i = 0; i < 10; i++)
    {
        TCHAR szMsg[2048];
        _stprintf_s(szMsg, _T("Hello CopyData [%d] !"), i);
        std::wcout << szMsg << std::endl;

        cbToWrite = sizeof(szMsg) + 1;

        fSuccess = WriteFile(
            hPipe,                  // pipe handle 
            szMsg,             // message 
            cbToWrite,              // message length 
            &cbWritten,             // bytes written 
            NULL);                  // not overlapped 

        if (!fSuccess)
        {
            TCHAR str[78];
            _stprintf_s(str, _T("riteFile to pipe failed. GLE=%d."), GetLastError());
            MessageBox(NULL, str, _T("Pipe Send Data"), MB_OK | MB_ICONERROR);
            break;
        }
        Sleep(1000);
    }

    CloseHandle(hPipe);
}

int main()
{
    //WindowsCopyData();

    PipeCopyData();

    system("pause");
    return 0;
}