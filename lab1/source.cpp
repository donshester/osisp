#include <windows.h>
#include <richedit.h> 
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <tchar.h>
#include "Richedit.h"
#include "commctrl.h"
#include <iostream>
#define RICHEDIT_CLASS L"RICHEDIT50W"
#define IDM_OPEN  1001
#define IDM_SAVE  1002
#define IDM_EXIT  1003
#define IDM_NEW 1004
#define IDC_CLOSE_BUTTON 1005
#define IDM_SEARCH 1006
#define IDM_SEARCH_IN_SEARCH_WINDOW 1008
#define IDM_CHANGE_THEME 1009
#define IDM_CHANGE_FONT 1010
#define IDM_OPEN_BINARY 1011
#define IDM_SAVE_BINARY 1012
#define IDC_START_COUNT 2
#define IDC_WORD_LIST 3
#define WM_UPDATE_WORD_COUNT 6
#define IDC_STOP_COUNT 5
#define HOTKEY_ID 1

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SearchWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static int wordsCount = 0; 
static HWND hEdit;
static HWND hSearchEdit = nullptr;
static int searchStartPosition = 0;
static HWND hSearchWindow = nullptr;
HWND g_hwnd;
HANDLE g_hFileMapping;
LPVOID g_pFileData;
DWORD g_fileSize;
bool g_bIsWordCountThreadRunning = true;
HANDLE hThread = nullptr;
UINT_PTR timerId;
struct ThreadParams {
    HWND hWnd;
    // Другие параметры, если необходимо
};
void OpenSearchWindow(HWND hWnd)
{
    hSearchWindow = CreateWindow(
        L"SearchWindowClass",
        L"Search Window",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
    );

    if (hSearchWindow != nullptr)
    {
        hSearchEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            10, 10, 150, 30,
            hSearchWindow,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        HWND hSearchButton = CreateWindow(
            L"BUTTON",
            L"Search",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            170, 10, 80, 30,
            hSearchWindow,
            reinterpret_cast<HMENU>(IDM_SEARCH_IN_SEARCH_WINDOW),
            GetModuleHandle(nullptr),
            nullptr
        );

        if (hSearchEdit == nullptr || hSearchButton == nullptr)
        {
            MessageBox(hWnd, L"Не удалось создать элементы для поиска.", L"Ошибка", MB_OK | MB_ICONERROR);
            DestroyWindow(hSearchWindow);
        }

        ShowWindow(hSearchWindow, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(hWnd, L"Не удалось создать окно поиска.", L"Ошибка", MB_OK | MB_ICONERROR);
    }
}

DWORD WINAPI WordCountThread(LPVOID lpParam) {
    ThreadParams* params = reinterpret_cast<ThreadParams*>(lpParam);
    HWND hWnd = params->hWnd;

    while (g_bIsWordCountThreadRunning) {
        int textLength = GetWindowTextLength(hEdit);

        if (textLength > 0) {
            std::wstring text;
            text.resize(textLength + 1);
            GetWindowText(hEdit, &text[0], textLength + 1);

            int wordCount = 0;
            bool inWord = false;

            for (wchar_t c : text) {
                if (iswspace(c)) {
                    inWord = false;
                }
                else {
                    if (!inWord) {
                        wordCount++;
                        inWord = true;
                    }
                }
            }
            wordsCount = wordCount;
            if (hWnd) {
                SendMessage(hWnd, WM_UPDATE_WORD_COUNT, 0, 0);
            }
        }

        Sleep(1000);
    }

    return 0;
}

void OnHotkey(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
    if (wParam == HOTKEY_ID)
    {
        OpenSearchWindow(hWnd);
    }
}

void OpenColorDialog()
{
    CHOOSECOLOR cc = { sizeof(CHOOSECOLOR) };
    static COLORREF custColors[16] = { 0 };
    cc.rgbResult = RGB(255, 255, 255);
    cc.lpCustColors = custColors;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColor(&cc))
    {
        SendMessage(hEdit, EM_SETBKGNDCOLOR, FALSE, cc.rgbResult);
    }
}



int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    if (!RegisterHotKey(NULL, HOTKEY_ID, MOD_CONTROL, 'F'))
    {
        MessageBox(NULL, L"Не удалось зарегистрировать горячую клавишу!", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    HWND hWnd{};
    MSG msg{};
    WNDCLASS wc{ sizeof(WNDCLASS) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hInstance = hInstance;

    wc.lpszClassName = L"LAB1!";
    wc.lpszMenuName = nullptr;
    wc.style = CS_VREDRAW | CS_HREDRAW;
    WNDCLASS wcSearchWindow{ sizeof(WNDCLASS) };

    wcSearchWindow.lpfnWndProc = SearchWindowProc;
    wcSearchWindow.hInstance = hInstance;
    wcSearchWindow.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH));
    wcSearchWindow.lpszClassName = L"SearchWindowClass";


    if (!RegisterClass(&wcSearchWindow) || !RegisterClassW(&wc)) {
        return EXIT_FAILURE;
    }

    if (hWnd = CreateWindow(wc.lpszClassName, L"LAB1", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, wc.hInstance, nullptr)) {
        HMENU hMenu = CreateMenu();
        HMENU hFileMenu = CreateMenu();
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"Файл");
        AppendMenu(hFileMenu, MF_STRING, IDM_OPEN, L"Открыть");
        AppendMenu(hFileMenu, MF_STRING, IDM_SAVE, L"Сохранить");
        AppendMenu(hFileMenu, MF_STRING, IDM_OPEN_BINARY, L"Открыть бинарный файл");
        AppendMenu(hFileMenu, MF_STRING, IDM_SAVE_BINARY, L"Сохранить бинарный файл");
        AppendMenu(hFileMenu, MF_STRING, IDC_START_COUNT, L"Начать счетчик");
        AppendMenu(hFileMenu, MF_STRING, IDC_STOP_COUNT, L"Закончить счетчик");
        AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hFileMenu, MF_STRING, IDM_EXIT, L"Выход");
        AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hFileMenu, MF_STRING, IDM_NEW, L"Новый");
        AppendMenu(hFileMenu, MF_SEPARATOR, 0, NULL);
        AppendMenu(hFileMenu, MF_STRING, IDM_CHANGE_THEME, L"Поменять тему");
        AppendMenu(hFileMenu, MF_STRING, IDM_CHANGE_FONT, L"Изменить шрифт");
        AppendMenu(hMenu, MF_STRING, IDM_SEARCH, L"Поиск");
        SetMenu(hWnd, hMenu);
        ShowWindow(hWnd, nCmdShow);
        UpdateWindow(hWnd);

        while (GetMessage(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_HOTKEY)
            {
                OnHotkey(msg.hwnd, msg.wParam, msg.lParam);
            }
        }
    }
    return EXIT_SUCCESS;
}

LRESULT CALLBACK SearchWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_SEARCH_IN_SEARCH_WINDOW:
        {
            int searchQueryLength = GetWindowTextLength(hSearchEdit);
            if (searchQueryLength == 0)
            {
                MessageBox(hWnd, L"Введите текст.", L"Error", MB_OK | MB_ICONERROR);
                return 0;
            }

            wchar_t* searchQuery = new wchar_t[searchQueryLength + 1];
            GetWindowText(hSearchEdit, searchQuery, searchQueryLength + 1);

            int mainEditLength = GetWindowTextLength(hEdit);
            wchar_t* mainEditText = new wchar_t[mainEditLength + 1];
            GetWindowText(hEdit, mainEditText, mainEditLength + 1);

            wchar_t* found = wcsstr(mainEditText + searchStartPosition, searchQuery);

            if (found != nullptr)
            {
                int foundPos = found - mainEditText;

                SendMessage(hEdit, EM_SETSEL, foundPos, foundPos + searchQueryLength);

                CHARFORMAT2 cf;
                cf.cbSize = sizeof(cf);
                cf.dwMask = CFM_BACKCOLOR;
                cf.crBackColor = RGB(255, 255, 0);

                SendMessage(hEdit, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);


                searchStartPosition = foundPos + searchQueryLength;
            }
            else
            {
                searchStartPosition = 0;
                MessageBox(hWnd, L"No more matches found.", L"Search Complete", MB_OK | MB_ICONINFORMATION);
            }

            delete[] searchQuery;
            delete[] mainEditText;
        }
        break;

        case WM_CLOSE:
            if (hWnd == hSearchWindow)
                DestroyWindow(hWnd);
            break;
        }
        break;

    case WM_DESTROY:
        if (hWnd == hSearchWindow)
            hSearchWindow = nullptr;
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}


void ResizeEditControl(HWND hWnd, HWND hEdit)
{
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    SetWindowPos(hEdit, nullptr, 0, 0, clientRect.right, clientRect.bottom, SWP_NOZORDER);
}

void OpenBinaryFile(LPCTSTR filePath) {
    if (g_pFileData != NULL) {
        UnmapViewOfFile(g_pFileData);
        g_pFileData = NULL;
    }
    if (g_hFileMapping != NULL) {
        CloseHandle(g_hFileMapping);
        g_hFileMapping = NULL;
    }

    HANDLE hFile = CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        MessageBox(g_hwnd, _T("Failed to open the file"), _T("Error"), MB_ICONERROR);
        return;
    }

    g_fileSize = GetFileSize(hFile, NULL);

    g_hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, g_fileSize, NULL);
    if (g_hFileMapping == NULL) {
        MessageBox(g_hwnd, _T("Failed to create file mapping"), _T("Error"), MB_ICONERROR);
        CloseHandle(hFile);
        return;
    }

    g_pFileData = MapViewOfFile(g_hFileMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, g_fileSize);
    if (g_pFileData == NULL) {
        MessageBox(g_hwnd, _T("Failed to map the file into memory"), _T("Error"), MB_ICONERROR);
        CloseHandle(g_hFileMapping);
        g_hFileMapping = NULL;
        CloseHandle(hFile);
        return;
    }
}
LPCWSTR AsciiToHex(const WCHAR* asciiString) {
    std::wstringstream resultStream;

    for (size_t i = 0; i < wcslen(asciiString); ++i) {
        int asciiValue = static_cast<int>(asciiString[i]);
        resultStream << std::hex << std::setw(2) << std::setfill(L'0') << asciiValue;
        if (i < wcslen(asciiString) - 1) {
            resultStream << L' ';
        }
    }

    std::wstring result = resultStream.str();
    LPCWSTR resultStr = _wcsdup(result.c_str());

    return resultStr;
}
void StopWordCountThread() {
    g_bIsWordCountThreadRunning = false;
    if (hThread != nullptr) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = nullptr;
    }
}

LPCWSTR HexToAsciiString(const WCHAR* hexString) {
    std::wstringstream resultStream;

    std::wistringstream hexStream(hexString);
    std::wstring hexByte;
    while (hexStream >> hexByte) {
        int number = 0;
        std::wstringstream hexConverter;
        hexConverter << std::hex << hexByte;
        hexConverter >> number;

        WCHAR asciiChar = static_cast<WCHAR>(number);
        resultStream << asciiChar;
    }

    std::wstring result = resultStream.str();
    LPCWSTR resultStr = _wcsdup(result.c_str());

    return resultStr;
}

void SaveHexToFile(LPCTSTR filePath, const std::wstring& hexString) {
    if (g_hFileMapping != NULL) {
        CloseHandle(g_hFileMapping);
        g_hFileMapping = NULL;

        HANDLE hFile = CreateFile(filePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD bytesWritten;
            WriteFile(hFile, hexString.c_str(), static_cast<DWORD>(hexString.size() * sizeof(WCHAR)), &bytesWritten, NULL);
            CloseHandle(hFile);
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{   
    static wchar_t currentFileName[MAX_PATH] = L"";

    HWND hButtonContainer = nullptr;
    
    HWND hStartWordCountButton = nullptr;
    switch (message)
    {
    case WM_CREATE:
    {
        LoadLibrary(TEXT("Msftedit.dll"));
        hEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            RICHEDIT_CLASS,
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
            0, 0, 800, 500, 
            hWnd,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );
        if (hEdit == nullptr)
        {
            MessageBox(hWnd, L"Не удалось создать элемент управления Edit.", L"Ошибка", MB_OK | MB_ICONERROR);
        }
        break;
    }   
    case WM_UPDATE_WORD_COUNT: {
        std::wstring labelText = L"Количество слов: " + std::to_wstring(wordsCount);
        SetWindowText(hWnd, labelText.c_str());

        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_CHANGE_THEME:
            OpenColorDialog();
            break;
        case IDM_CHANGE_FONT:
        {
            CHOOSEFONT cf = { sizeof(CHOOSEFONT) };
            LOGFONT lf = { 0 };

            cf.hwndOwner = hWnd;
            cf.lpLogFont = &lf;
            cf.Flags = CF_SCREENFONTS | CF_EFFECTS | CF_INITTOLOGFONTSTRUCT;
            if (ChooseFont(&cf))
            {
                HFONT hFont = CreateFontIndirect(&lf);
                SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
            }
            break;
        }
        case IDM_OPEN:
        {
            OPENFILENAME ofn;
            wchar_t szFile[MAX_PATH] = L"";
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFilter = L"C++ файлы (*.cpp;*.txt)\0*.cpp;*.txt\0Все файлы (*.*)\0*.*\0";
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

            if (GetOpenFileName(&ofn))
            {
                wcscpy_s(currentFileName, MAX_PATH, ofn.lpstrFile);

                HANDLE hFile = CreateFile(ofn.lpstrFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    DWORD dwFileSize = GetFileSize(hFile, NULL);

                    wchar_t* fileContent = (wchar_t*)malloc((dwFileSize + 1) * sizeof(wchar_t));
                    if (fileContent)
                    {
                        DWORD bytesRead;
                        ReadFile(hFile, fileContent, dwFileSize * sizeof(wchar_t), &bytesRead, NULL);
                        fileContent[dwFileSize / sizeof(wchar_t)] = L'\0';

                        SetWindowText(hEdit, fileContent);

                        free(fileContent);
                    }
                    else
                    {
                        MessageBox(hWnd, L"Не удалось выделить память для содержимого файла.", L"Ошибка", MB_OK | MB_ICONERROR);
                    }

                    CloseHandle(hFile);
                }
                else
                {
                    MessageBox(hWnd, L"Не удалось открыть выбранный файл.", L"Ошибка", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }

        case IDM_SAVE:
        {
            HANDLE hFile = INVALID_HANDLE_VALUE;

            if (currentFileName[0] != L'\0')
            {
                hFile = CreateFile(currentFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            }

            if (hFile == INVALID_HANDLE_VALUE)
            {
                OPENFILENAME ofn;
                wchar_t szFile[MAX_PATH] = L"";
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = L"C++ files (*.cpp;*.txt)\0*.cpp;*.txt\0All files (*.*)\0*.*\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;

                if (GetSaveFileName(&ofn))
                {
                    hFile = CreateFile(ofn.lpstrFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                    if (hFile != INVALID_HANDLE_VALUE)
                    {
                        wcscpy_s(currentFileName, MAX_PATH, ofn.lpstrFile);
                    }
                    else
                    {
                        MessageBox(hWnd, L"Не удалось создать выбранный файл.", L"Ошибка", MB_OK | MB_ICONERROR);
                    }
                }
            }

            if (hFile != INVALID_HANDLE_VALUE)
            {
                int textLength = GetWindowTextLength(hEdit);
                wchar_t* textBuffer = (wchar_t*)malloc((textLength + 1) * sizeof(wchar_t));
                if (textBuffer)
                {
                    GetWindowText(hEdit, textBuffer, textLength + 1);

                    DWORD bytesWritten;
                    WriteFile(hFile, textBuffer, textLength * sizeof(wchar_t), &bytesWritten, NULL);

                    free(textBuffer);
                    CloseHandle(hFile);
                }
                else
                {
                    MessageBox(hWnd, L"Не удалось выделить память для текста.", L"Ошибка", MB_OK | MB_ICONERROR);
                    CloseHandle(hFile);
                }
            }

            break;
        }
        case IDM_NEW:
        {
            if (MessageBox(hWnd, L"Вы уверены, что хотите создать новый документ? Все несохраненные изменения будут потеряны.", L"Подтвердите действие", MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                SetWindowText(hEdit, L"");
                currentFileName[0] = L'\0';
            }
            break;
        }
       
        case IDM_OPEN_BINARY: {
            OPENFILENAME ofn = { 0 };
            TCHAR filePath[MAX_PATH] = { 0 };

            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = filePath;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            ofn.lpstrFilter = L"Binary Files\0*.bin\0All Files\0*.*\0";
            ofn.lpstrDefExt = L"bin";
            if (GetOpenFileName(&ofn)) {
                OpenBinaryFile(filePath);
                int requiredBufferSize = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)g_pFileData, -1, NULL, 0);
                if (requiredBufferSize > 0) {
                    WCHAR* utf16Text = new WCHAR[requiredBufferSize];
                    MultiByteToWideChar(CP_UTF8, 0, (LPCCH)g_pFileData, -1, utf16Text, requiredBufferSize);
                    LPCWSTR text = HexToAsciiString(utf16Text);

                    SetWindowText(hEdit, text);

                    delete[] utf16Text;
                }
                lstrcpy(currentFileName, filePath);

            }
            break;
        }
        case IDM_SAVE_BINARY: {
            if (g_hFileMapping != NULL) {
                OPENFILENAME ofn = { 0 };
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFile = currentFileName;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;

                ofn.lpstrFilter = L"Binary Files\0*.bin\0All Files\0*.*\0";
                ofn.lpstrDefExt = L"bin";

                if (GetSaveFileName(&ofn)) {
                    int textLength = GetWindowTextLength(hEdit);
                    std::wstring text;
                    if (textLength > 0) {
                        text.resize(textLength);
                        GetWindowText(hEdit, &text[0], textLength + 1);
                    }
                    std::wstring hexString = AsciiToHex(text.c_str());
                    SaveHexToFile(currentFileName, hexString);
                }
            }
            break;
        }
        case IDM_SEARCH:
            OpenSearchWindow(hWnd);
            break;
       
        case IDC_START_COUNT: {
            ThreadParams threadParams;
            threadParams.hWnd = hWnd;

            hThread = CreateThread(nullptr, 0, WordCountThread, &threadParams, CREATE_SUSPENDED, nullptr);
            if (hThread == nullptr) {
                MessageBox(hWnd, L"Не удалось запустить поток.", L"Ошибка", MB_OK | MB_ICONERROR);
            }
            if (SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL) == 0) {
                MessageBox(hWnd, L"Не удалось назначить приоритет потоку.", L"Ошибка", MB_OK | MB_ICONERROR);
            }
            ResumeThread(hThread);

            break;
        }

        case IDC_STOP_COUNT: {
            StopWordCountThread();
            break;
        }

        case WM_CLOSE: {
            if (hThread != nullptr) {
                TerminateThread(hThread, 0);
                CloseHandle(hThread);
            }
            DestroyWindow(hWnd);
            break;
        }


        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;

       
    case WM_DESTROY:

        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
