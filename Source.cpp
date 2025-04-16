#include "pch.h"
#include <windows.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <commctrl.h>
#include <tchar.h>
#include <AclAPI.h>
#include <memory>
#include <winternl.h>  // Для работы с NtQuerySystemInformation и UNICODE_STRING
#include <algorithm>    // Для std::transform

#pragma comment(lib, "ntdll.lib")  // Подключаем ntdll.lib для работы с NtQuerySystemInformation

// Функция для включения привилегии SeDebugPrivilege
bool EnableDebugPrivilege() {
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    LUID luid;

    // Получаем токен текущего процесса
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        return false;
    }

    // Получаем LUID для привилегии SeDebugPrivilege
    if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    // Устанавливаем привилегию
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Применяем изменения привилегий
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL)) {
        CloseHandle(hToken);
        return false;
    }

    // Проверяем, была ли привилегия успешно установлена
    if (GetLastError() == ERROR_NOT_ALL_ASSIGNED) {
        CloseHandle(hToken);
        return false;
    }

    CloseHandle(hToken);
    return true;
}

// Используем уже существующее определение структуры из winternl.h
extern "C" NTSTATUS NTAPI NtQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

// Функция для получения информации о процессах
std::vector<std::pair<DWORD, std::wstring>> GetProcessesUsingNtQuery() {
    std::vector<std::pair<DWORD, std::wstring>> processes;
    ULONG len = 0;
    ULONG bytesNeeded = 0;

    // Запрашиваем размер данных
    NTSTATUS status = NtQuerySystemInformation(SystemProcessInformation, NULL, 0, &bytesNeeded);
    if (status != 0xC0000004) { // STATUS_INFO_LENGTH_MISMATCH
        return processes; // Возвращаем пустой список
    }

    // Выделяем память для информации о процессах
    std::unique_ptr<char[]> buffer(new char[bytesNeeded]);
    status = NtQuerySystemInformation(SystemProcessInformation, buffer.get(), bytesNeeded, &len);
    if (status != 0) {
        return processes; // Возвращаем пустой список, если произошла ошибка
    }

    SYSTEM_PROCESS_INFORMATION* pProcess = (SYSTEM_PROCESS_INFORMATION*)buffer.get();

    // Перебираем все процессы
    while (pProcess) {
        if (pProcess->UniqueProcessId != 0) {  // Пропускаем процессы с ID = 0
            std::wstring processName(pProcess->ImageName.Buffer, pProcess->ImageName.Length / sizeof(wchar_t));
            processes.push_back({ (DWORD)pProcess->UniqueProcessId, processName });
        }
        if (pProcess->NextEntryOffset == 0) {
            break;
        }
        pProcess = (SYSTEM_PROCESS_INFORMATION*)((char*)pProcess + pProcess->NextEntryOffset);
    }

    return processes;
}

// Функция для обновления ComboBox с процессами
void UpdateProcessList(HWND hComboBox) {
    // Очистить текущие данные
    SendMessage(hComboBox, CB_RESETCONTENT, 0, 0);

    // Получаем список процессов с их именами и PID
    std::vector<std::pair<DWORD, std::wstring>> processList = GetProcessesUsingNtQuery();
    for (const auto& process : processList) {
        std::wstring itemText = process.second + L" - PID: " + std::to_wstring(process.first);
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)itemText.c_str());
    }
}

// Функция для обновления ComboBox с клавишами
void UpdateKeyList(HWND hComboBox) {
    // Очистить текущие данные
    SendMessage(hComboBox, CB_RESETCONTENT, 0, 0);

    // Список клавиш для выбора
    std::vector<std::wstring> keys = {
        L"Esc", L"F1", L"F2", L"F3", L"F4", L"F5",
        L"F6", L"F7", L"F8", L"F9", L"F10", L"F11", L"F12"
    };

    // Добавляем клавиши в ComboBox
    for (const auto& key : keys) {
        SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)key.c_str());
    }
}

// Переменная для хранения выбранной клавиши
int selectedKey = VK_ESCAPE;  // По умолчанию, клавиша Escape

// Устанавливаем хук на выбранную клавишу
HHOOK hKeyboardHook = NULL;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        if (wParam == selectedKey) {
            MessageBox(NULL, L"Выбранная клавиша нажата!", L"Hook", MB_OK);
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);
}

// Функция для установки хука
bool InstallHook() {
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, GetCurrentThreadId());
    if (hKeyboardHook == NULL) {
        return false;
    }
    return true;
}

// Процедура обработки сообщений окна
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        // Включаем привилегию SeDebugPrivilege
        if (!EnableDebugPrivilege()) {
            MessageBox(hwnd, L"Не удалось включить привилегию SeDebugPrivilege", L"Ошибка", MB_OK | MB_ICONERROR);
            PostQuitMessage(0);
            return 0;
        }

        // Создаем ComboBox для списка процессов
        HWND hComboBox = CreateWindowEx(0, WC_COMBOBOX, NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL,
            10, 10, 200, 200, hwnd, (HMENU)1, GetModuleHandle(NULL), NULL);

        // Получаем список процессов с их именами и PID
        UpdateProcessList(hComboBox);

        // Создаем ComboBox для выбора клавиши (сдвигаем его правее и уменьшаем ширину)
        HWND hKeyComboBox = CreateWindowEx(0, WC_COMBOBOX, NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_HASSTRINGS | WS_VSCROLL,
            220, 10, 100, 200, hwnd, (HMENU)4, GetModuleHandle(NULL), NULL);

        // Обновляем ComboBox клавиш
        UpdateKeyList(hKeyComboBox);

        // Создаем кнопку для копирования PID (сдвигаем вниз и уменьшаем ширину)
        CreateWindow(L"BUTTON", L"Коп. PID", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10, 220, 60, 30, hwnd, (HMENU)2, GetModuleHandle(NULL), NULL);

        // Создаем кнопку для установки хука (сдвигаем вниз и уменьшаем ширину)
        CreateWindow(L"BUTTON", L"Уст. хук", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            80, 220, 60, 30, hwnd, (HMENU)3, GetModuleHandle(NULL), NULL);
    } break;

    case WM_COMMAND: {
        // Обработчик для кнопки "Копировать PID"
        if (LOWORD(wParam) == 2) {
            HWND hComboBox = GetDlgItem(hwnd, 1);
            int selectedProcessIndex = SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
            if (selectedProcessIndex != CB_ERR) {
                // Получаем текст выбранного процесса
                wchar_t processInfo[256];
                SendMessage(hComboBox, CB_GETLBTEXT, selectedProcessIndex, (LPARAM)processInfo);

                // Извлекаем PID из строки
                std::wstring processInfoStr(processInfo);
                size_t pidPos = processInfoStr.find(L"PID: ");
                std::wstring pidStr = processInfoStr.substr(pidPos + 5);  // Извлекаем PID как строку
                DWORD pid = std::stoi(pidStr);  // Преобразуем PID в число

                // Копируем PID в буфер обмена
                if (OpenClipboard(hwnd)) {
                    EmptyClipboard();
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (pidStr.size() + 1) * sizeof(wchar_t));
                    if (hMem) {
                        memcpy(GlobalLock(hMem), pidStr.c_str(), (pidStr.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(hMem);
                        SetClipboardData(CF_UNICODETEXT, hMem);
                    }
                    CloseClipboard();
                }
                MessageBox(hwnd, L"PID скопирован в буфер обмена", L"Успех", MB_OK | MB_ICONINFORMATION);
            }
        }

        // Обработчик для кнопки "Установить хук"
        else if (LOWORD(wParam) == 3) {
            HWND hComboBox = GetDlgItem(hwnd, 1);
            int selectedProcessIndex = SendMessage(hComboBox, CB_GETCURSEL, 0, 0);
            if (selectedProcessIndex != CB_ERR) {
                // Получаем текст выбранного процесса
                wchar_t processInfo[256];
                SendMessage(hComboBox, CB_GETLBTEXT, selectedProcessIndex, (LPARAM)processInfo);

                // Извлекаем PID из строки
                std::wstring processInfoStr(processInfo);
                size_t pidPos = processInfoStr.find(L"PID: ");
                DWORD pid = 0;
                if (pidPos != std::wstring::npos) {
                    std::wstring pidStr = processInfoStr.substr(pidPos + 5);
                    pid = std::stoi(pidStr);  // Преобразуем PID в число
                }

                // Получаем выбранную клавишу для хука
                HWND hKeyComboBox = GetDlgItem(hwnd, 4);
                int selectedKeyIndex = SendMessage(hKeyComboBox, CB_GETCURSEL, 0, 0);
                if (selectedKeyIndex != CB_ERR) {
                    wchar_t keyText[256];
                    SendMessage(hKeyComboBox, CB_GETLBTEXT, selectedKeyIndex, (LPARAM)keyText);

                    // Сопоставляем выбранную клавишу с виртуальным кодом
                    if (wcscmp(keyText, L"Esc") == 0) selectedKey = VK_ESCAPE;
                    else if (wcscmp(keyText, L"F1") == 0) selectedKey = VK_F1;
                    else if (wcscmp(keyText, L"F2") == 0) selectedKey = VK_F2;
                    else if (wcscmp(keyText, L"F3") == 0) selectedKey = VK_F3;
                    else if (wcscmp(keyText, L"F4") == 0) selectedKey = VK_F4;
                    else if (wcscmp(keyText, L"F5") == 0) selectedKey = VK_F5;
                    // Можно добавить другие клавиши по аналогии

                    if (InstallHook()) {
                        MessageBox(hwnd, L"Хук установлен успешно!", L"Успех", MB_OK | MB_ICONINFORMATION);
                    }
                    else {
                        MessageBox(hwnd, L"Не удалось установить хук", L"Ошибка", MB_OK | MB_ICONERROR);
                    }
                }
            }
        }
    } break;

    case WM_CLOSE:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int main() {
    // Регистрация окна
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = L"ProcessNameChanger";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);

    // Создание окна
    HWND hwnd = CreateWindowEx(0, L"ProcessNameChanger", L"Выберите процесс и клавишу для хука",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 400, 350, NULL, NULL, GetModuleHandle(NULL), NULL);

    // Показать и обновить окно
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Главный цикл обработки сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
