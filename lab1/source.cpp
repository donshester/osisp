#include "imports.h";
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
#define IDM_REGISTRY_ACCESS 1999
#define IDC_READ_BUTTON 2999
#define IDC_RESULT_EDIT 3999
#define IDC_CHAT_LIST 3991
#define OPEN_CHAT 4999
#define IDC_LISTVIEW 5999
#define IDC_SEND_FILE_BUTTON 5991
#define IDM_SEND_FILE 5191

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SearchWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
static int wordsCount = 0; 
static HWND hEdit;
static HWND hSearchEdit = nullptr;
static int searchStartPosition = 0;
static HWND hSearchWindow = nullptr;
HWND g_hwnd;
std::vector<char> g_content;
HANDLE g_hFileMapping;
LPVOID g_pFileData;
DWORD g_fileSize;
bool g_bIsWordCountThreadRunning = true;
HANDLE hThread = nullptr;
UINT_PTR timerId;
HANDLE g_hFileMutex;
HANDLE g_hRegistryMutex;
CRITICAL_SECTION g_csWordCount;
WSADATA wsaData;
sockaddr_in serverAddr;
HWND hwndChatWindow;
HWND hChatsListBox;
HANDLE chatThread;
HWND g_hWndSettings;

SOCKET g_socket = 0;


LRESULT CALLBACK RegistryAccessWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        CreateWindow(L"BUTTON", L"Чтение", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 10, 80, 30, hWnd, (HMENU)IDC_READ_BUTTON, GetModuleHandle(NULL), NULL);

        CreateWindow(L"EDIT", L"", WS_VISIBLE | WS_CHILD | ES_READONLY | WS_BORDER,
            100, 10, 200, 30, hWnd, (HMENU)IDC_RESULT_EDIT, GetModuleHandle(NULL), NULL);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_READ_BUTTON:
        {
            HKEY hKey; 
            LONG result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\YourRegistryKey", 0, KEY_READ, &hKey);

            if (result == ERROR_SUCCESS)
            {
                WCHAR valueData[256];
                DWORD valueSize = sizeof(valueData);

                result = RegQueryValueEx(hKey, L"YourRegistryValue", 0, NULL, (LPBYTE)valueData, &valueSize);

                if (result == ERROR_SUCCESS)
                {
                    SetDlgItemText(hWnd, IDC_RESULT_EDIT, valueData);
                }
                else
                {
                    SetDlgItemText(hWnd, IDC_RESULT_EDIT, L"Ошибка чтения значения из реестра");
                }

                RegCloseKey(hKey);
            }
            else
            {
                SetDlgItemText(hWnd, IDC_RESULT_EDIT, L"Ошибка открытия ключа в реестре");
            }
            break;
        }
        }

    case WM_CLOSE:
        DestroyWindow(hWnd);
        break;


    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}
void LoadFontSettings()
{
    WaitForSingleObject(g_hRegistryMutex, INFINITE);

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\322EDITOR322\\FontSettings", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        LOGFONT lf = { 0 };

        DWORD dataSize;
        DWORD fontUnderline, fontStrikeOut, fontItalic, fontBold;

        dataSize = sizeof(DWORD);
        RegQueryValueEx(hKey, L"FontUnderline", 0, NULL, (LPBYTE)&fontUnderline, &dataSize);

        dataSize = sizeof(DWORD);
        RegQueryValueEx(hKey, L"FontStrikeOut", 0, NULL, (LPBYTE)&fontStrikeOut, &dataSize);

        dataSize = sizeof(DWORD);
        RegQueryValueEx(hKey, L"FontItalic", 0, NULL, (LPBYTE)&fontItalic, &dataSize);

        dataSize = sizeof(DWORD);
        RegQueryValueEx(hKey, L"FontBold", 0, NULL, (LPBYTE)&fontBold, &dataSize);

        dataSize = sizeof(wchar_t) * MAX_PATH;
        RegQueryValueEx(hKey, L"FontName", 0, NULL, (LPBYTE)lf.lfFaceName, &dataSize);

        dataSize = sizeof(DWORD);
        RegQueryValueEx(hKey, L"FontSize", 0, NULL, (LPBYTE)&lf.lfHeight, &dataSize);

        RegCloseKey(hKey);

        lf.lfUnderline = fontUnderline;
        lf.lfStrikeOut = fontStrikeOut;
        lf.lfItalic = fontItalic;
        lf.lfWeight = fontBold;

        HFONT hFont = CreateFontIndirect(&lf);
        SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    ReleaseMutex(g_hRegistryMutex);
}
void LoadColorSettings()
{
    WaitForSingleObject(g_hRegistryMutex, INFINITE);

    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, L"Software\\322EDITOR322\\ColorSettings", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD dataSize;
        DWORD backgroundColor;

        dataSize = sizeof(DWORD);
        RegQueryValueEx(hKey, L"BackgroundColor", 0, NULL, (LPBYTE)&backgroundColor, &dataSize);

        RegCloseKey(hKey);

        SendMessage(hEdit, EM_SETBKGNDCOLOR, FALSE, backgroundColor);
    }

    ReleaseMutex(g_hRegistryMutex);
}
struct ThreadParams {
    HWND hWnd;
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

            EnterCriticalSection(&g_csWordCount);

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

            LeaveCriticalSection(&g_csWordCount);

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
void SaveColorSettings(COLORREF color)
{
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\322EDITOR322\\ColorSettings", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
    {
        RegSetValueEx(hKey, L"BackgroundColor", 0, REG_DWORD, (LPBYTE)&color, sizeof(DWORD));
        RegCloseKey(hKey);
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
        SaveColorSettings(cc.rgbResult);
    }
}






LRESULT CALLBACK ChatProc(HWND hWindow, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case WM_CLOSE:
            if (hWindow == g_hWndSettings) {
                Package p = {Operation::Close, 0, 0 };
                send(g_socket, reinterpret_cast<char*>(&p), sizeof(Package), NULL);
                closesocket(g_socket);
                DestroyWindow(hWindow);
            }
            break;
        }
        break;
    case WM_DESTROY:
        if (hWindow == g_hWndSettings) {
            g_hWndSettings = nullptr;
        }
        break;
    case WM_NOTIFY:
    {
        NMHDR* nmh = reinterpret_cast<NMHDR*>(lParam);
        if (nmh->code == NM_DBLCLK) {
            NMLISTVIEW* pNmlv = reinterpret_cast<NMLISTVIEW*>(nmh);
            int selectedIndex = pNmlv->iItem;

            WCHAR buffer[MAX_PATH];
            LVITEM lvItem;
            lvItem.iItem = selectedIndex;
            lvItem.iSubItem = 0;
            lvItem.pszText = buffer;
            lvItem.cchTextMax = MAX_PATH;

            SendMessage(pNmlv->hdr.hwndFrom, LVM_GETITEMTEXT, selectedIndex, reinterpret_cast<LPARAM>(&lvItem));

            int itemId = std::stoi(buffer);
            if (itemId != -1) {
                OPENFILENAME ofn;
                wchar_t szFileName[1024] = L"";

                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = NULL;
                ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
                ofn.lpstrFile = szFileName;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                if (GetOpenFileName(&ofn) == TRUE)
                {
                    HANDLE hFile = CreateFileW(szFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                    DWORD fSize = GetFileSize(hFile, NULL);
                    if (hFile != INVALID_HANDLE_VALUE) {
                        if (fSize == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
                            CloseHandle(hFile);
                            return false;
                        }
                        g_content.resize(fSize + 1);
                        DWORD bytesRead;
                        if (ReadFile(hFile, g_content.data(), fSize, &bytesRead, NULL)) {
                            g_content[bytesRead] = '\0';
                        }
                        else {
                            return false;
                        }
                        CloseHandle(hFile);
                    }
                    else {
                        return false;
                    }

                    std::wstring name = szFileName;

                    name = name.substr(name.find_last_of('\\') + 1);
                    LPWSTR fName = const_cast<LPWSTR>(name.data());
                    name = fName;
                    Package pack{ Operation::Send, name.size(), itemId };
                    send(g_socket, reinterpret_cast<char*>(&pack), sizeof(Package), NULL);
                    send(g_socket, reinterpret_cast<char*>(const_cast<wchar_t*>(name.c_str())), name.size() * sizeof(wchar_t), NULL);
                    send(g_socket, reinterpret_cast<char*>(&fSize), sizeof(int), NULL);
                    send(g_socket, g_content.data(), fSize * sizeof(char), NULL);
                }
            }
        }
        else if (nmh->code == NM_RCLICK) {
            NMLISTVIEW* pNmlv = reinterpret_cast<NMLISTVIEW*>(nmh);
            int selectedItemIndex = pNmlv->iItem;

            if (selectedItemIndex >= 0) {
                POINT pt;
                GetCursorPos(&pt);

                HMENU hPopupMenu = CreatePopupMenu();
                AppendMenu(hPopupMenu, MF_STRING, IDM_SEND_FILE, L"Send File");

                TrackPopupMenu(hPopupMenu, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, hwndChatWindow, NULL);

                DestroyMenu(hPopupMenu);
            }
        }
        break;
    }
    default:
        return DefWindowProc(hWindow, msg, wParam, lParam);
    }
    return 0;
}
void ProcessChatThread()
{
    int op;
    int sz;
    while (true)
    {
        Package p;
        int bytes = recv(g_socket, reinterpret_cast<char*>(&p), sizeof(Package), NULL);
        switch (p.operation)
        {
        case Operation::Send:
        {
            std::wstring fName;
            fName.resize(p.data * sizeof(wchar_t));
            recv(g_socket, reinterpret_cast<char*>(const_cast<wchar_t*>(fName.data())), p.data * sizeof(wchar_t), NULL);
            std::wstring msg = L"File received: " + fName; 
            int result = MessageBox(NULL, msg.c_str(), L"File received", MB_OKCANCEL | MB_ICONQUESTION);

            if (result == IDOK) {
                BROWSEINFO bi = { 0 };
                bi.hwndOwner = hwndChatWindow;
                bi.lpszTitle = L"Choose folder:";
                bi.ulFlags = BIF_NEWDIALOGSTYLE | BIF_RETURNONLYFSDIRS;
                LPITEMIDLIST pidl = SHBrowseForFolder(&bi);

                if (pidl != NULL)
                {
                    wchar_t folderPath[MAX_PATH];
                    SHGetPathFromIDList(pidl, folderPath);
                    int size;
                    std::string data;
                    recv(g_socket, reinterpret_cast<char*>(&size), sizeof(int), NULL);
                    data.resize(size);
                    recv(g_socket, const_cast<char*>(data.data()), size * sizeof(char), NULL);
                    std::wstring path = folderPath;
                    path += L"\\" + fName;
                    std::vector<char> cont(data.begin(), data.end());
                    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

                    if (hFile == INVALID_HANDLE_VALUE)
                    {
                        OutputDebugString(L"Unable to open file");
                    }
                    LPDWORD byteRead = 0;
                    if (!WriteFile(hFile, cont.data(), size, byteRead, NULL))
                    {
                        OutputDebugString(L"Unable to async write");
                        CloseHandle(hFile);
                        break;
                    }
                    CloseHandle(hFile);
                }
            }
        }
        break;
        case Operation::Update:
        {
            if (p.data > 0)
            {
                std::vector<int> receivedData;
                int currentId;
                recv(g_socket, reinterpret_cast<char*>(&currentId), sizeof(int), 0);
                std::vector<char> buffer(p.data * sizeof(int));
                int bytesRead = recv(g_socket, buffer.data(), buffer.size(), 0);

                if (bytesRead > 0) {
                    receivedData.resize(bytesRead / sizeof(int));
                    memcpy(receivedData.data(), buffer.data(), bytesRead);
                }
                else {
                    break;
                }

                if (hChatsListBox != NULL) {
                    SendMessage(hChatsListBox, LB_RESETCONTENT, 0, 0);
                }

                for (size_t i = 0; i < receivedData.size(); i++)
                {
                    if (hChatsListBox != NULL && currentId != i) {
                        LVITEM lvi;
                        lvi.mask = LVIF_TEXT;

                        lvi.iItem = i;
                        lvi.iSubItem = 0;
                        std::wstring text = std::to_wstring(receivedData[i]);
                        lvi.pszText = const_cast<LPWSTR>(text.c_str());
                        ListView_InsertItem(hChatsListBox, &lvi);
                    }
                }
            }
        }
        break;
        default:
            break;
        }
    }
}
void InitiateConnection(HWND windowHandle)
{
    if (WSAStartup(MAKEWORD(2, 1), &wsaData) != 0) {
        OutputDebugString(L"WSAStartup failed.");
        return;
    }

    sockaddr_in serverAddr;
    int serverAddrSize = sizeof(serverAddr);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1111);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    g_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket == INVALID_SOCKET) {
        OutputDebugString(L"Failed to create socket.");
        return;
    }

    if (connect(g_socket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        OutputDebugString(L"Connection failed.");
        closesocket(g_socket);
        WSACleanup();
        return;
    }

    hwndChatWindow = CreateWindow(L"ChatWindowClass", L"Chat Window", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 600, nullptr, nullptr, nullptr, nullptr);

    hChatsListBox = CreateWindowEx(0, WC_LISTVIEW, NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_EDITLABELS,
        0, 0, 600, 600, hwndChatWindow, reinterpret_cast<HMENU>(IDC_CHAT_LIST), NULL, NULL);

    LVCOLUMN lvColumn;
    lvColumn.mask = LVCF_WIDTH | LVCF_TEXT;
    lvColumn.cx = 600;
    lvColumn.pszText = const_cast<LPWSTR>(L"Номер клиента");
    ListView_InsertColumn(hChatsListBox, 0, &lvColumn);

    chatThread = CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(ProcessChatThread), NULL, NULL, NULL);

    ShowWindow(hwndChatWindow, SW_SHOWNORMAL);
}




int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    g_hFileMutex = CreateMutex(NULL, FALSE, L"FileMutex");
    if (g_hFileMutex == NULL) {
        MessageBox(NULL, L"Не удалось создать мьютекс для ресурсов!", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    g_hRegistryMutex = CreateMutex(NULL, FALSE, L"RegistryMutex");
    if (g_hRegistryMutex == NULL) {
        MessageBox(NULL, L"Не удалось создать мьютекс для реестра!", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    InitializeCriticalSection(&g_csWordCount);

   
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
    WNDCLASSEXW wcChatWindow{ sizeof(WNDCLASSEX) };

    wcChatWindow.lpfnWndProc = ChatProc;
    wcChatWindow.hInstance = hInstance;
    wcChatWindow.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH));
    wcChatWindow.lpszClassName = L"ChatWindowClass";
    RegisterClassExW(&wcChatWindow);


    if (!RegisterClass(&wcSearchWindow) || !RegisterClassW(&wc)) {
        CloseHandle(g_hFileMutex);
        return EXIT_FAILURE;
    }
    LoadFontSettings();
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
        AppendMenu(hFileMenu, MF_STRING, IDM_REGISTRY_ACCESS, L"Открыть реестр");
        AppendMenu(hMenu, MF_STRING, IDM_SEARCH, L"Поиск");
        AppendMenu(hMenu, MF_STRING, OPEN_CHAT, L"Открыть чат");

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
    closesocket(g_socket);

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
    WaitForSingleObject(g_hFileMutex, INFINITE);

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
        ReleaseMutex(g_hFileMutex);
    }

    g_fileSize = GetFileSize(hFile, NULL);

    g_hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, g_fileSize, NULL);
    if (g_hFileMapping == NULL) {
        MessageBox(g_hwnd, _T("Failed to create file mapping"), _T("Error"), MB_ICONERROR);
        CloseHandle(hFile);
        ReleaseMutex(g_hFileMutex);
        return;
    }

    g_pFileData = MapViewOfFile(g_hFileMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, g_fileSize);
    if (g_pFileData == NULL) {
        MessageBox(g_hwnd, _T("Failed to map the file into memory"), _T("Error"), MB_ICONERROR);
        CloseHandle(g_hFileMapping);
        g_hFileMapping = NULL;
        CloseHandle(hFile);
        ReleaseMutex(g_hFileMutex);
        return;
    }
    ReleaseMutex(g_hFileMutex);
}
void StopWordCountThread() {
    g_bIsWordCountThreadRunning = false;
    if (hThread != nullptr) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
        hThread = nullptr;
    }
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
void OpenRegistryAccessWindow(HWND hWnd)
{
    WCHAR szClassName[] = L"RegistryAccessWindowClass";

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = RegistryAccessWndProc; 
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = szClassName;

    if (!RegisterClass(&wc))
    {
        DWORD dwError = GetLastError();
        wchar_t errorMessage[256];
        swprintf(errorMessage, 256, L"Ошибка при регистрации класса окна. Код ошибки: %lu", dwError);
        MessageBox(NULL, errorMessage, L"Ошибка", MB_OK | MB_ICONERROR);
    }
    else
    {
        HWND hRegistryAccessWindow = CreateWindow(
            szClassName, 
            L"Registry Access",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 400, 200,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        if (hRegistryAccessWindow != nullptr)
        {
            ShowWindow(hRegistryAccessWindow, SW_SHOWNORMAL);
        }
        else
        {
            DWORD dwError = GetLastError();
            wchar_t errorMessage[256];
            swprintf(errorMessage, 256, L"Произошла ошибка при создании окна. Код ошибки: %lu", dwError);
            MessageBox(NULL, errorMessage, L"Ошибка", MB_OK | MB_ICONERROR);
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
        LoadFontSettings();
        LoadColorSettings();
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
    case WM_SIZE:
        ResizeEditControl(hWnd, hEdit);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case OPEN_CHAT:
            InitiateConnection(hWnd);
            break;
        case IDM_REGISTRY_ACCESS:
            OpenRegistryAccessWindow(hWnd);
            break;
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

                HKEY hKey;
                if (RegCreateKeyEx(HKEY_CURRENT_USER, L"Software\\322EDITOR322\\FontSettings", 0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
                {
                    RegSetValueEx(hKey, L"FontName", 0, REG_SZ, (LPBYTE)lf.lfFaceName, sizeof(wchar_t) * (wcslen(lf.lfFaceName) + 1));
                    RegSetValueEx(hKey, L"FontSize", 0, REG_DWORD, (LPBYTE)&lf.lfHeight, sizeof(DWORD));
                    RegSetValueEx(hKey, L"FontUnderline", 0, REG_DWORD, (LPBYTE)&lf.lfUnderline, sizeof(DWORD));
                    RegSetValueEx(hKey, L"FontStrikeOut", 0, REG_DWORD, (LPBYTE)&lf.lfStrikeOut, sizeof(DWORD));
                    RegSetValueEx(hKey, L"FontItalic", 0, REG_DWORD, (LPBYTE)&lf.lfItalic, sizeof(DWORD));
                    RegSetValueEx(hKey, L"FontBold", 0, REG_DWORD, (LPBYTE)&lf.lfWeight, sizeof(DWORD));
                    RegCloseKey(hKey);
                }
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
                    LPCWSTR text = utf16Text;

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
                    std::wstring hexString = text.c_str();
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
                DeleteCriticalSection(&g_csWordCount);
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
