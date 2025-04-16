
// MFC1111Dlg.cpp: файл реализации
//

#include "pch.h"
#include "framework.h"
#include "MFC1111.h"
#include "MFC1111Dlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include <vector>
#include <string>
#include <winternl.h>
#include <memory>


// Диалоговое окно CAboutDlg используется для описания сведений о приложении

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Данные диалогового окна
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // поддержка DDX/DDV

// Реализация
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// Диалоговое окно CMFC1111Dlg



CMFC1111Dlg::CMFC1111Dlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_MFC1111_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFC1111Dlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMFC1111Dlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_CBN_SELCHANGE(IDC_COMBO_KEYS, &CMFC1111Dlg::OnCbnSelchangeComboKeys)
END_MESSAGE_MAP()


// Обработчики сообщений CMFC1111Dlg

BOOL CMFC1111Dlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Добавление пункта "О программе..." в системное меню
	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Устанавливаем значок окна
	SetIcon(m_hIcon, TRUE); // Крупный значок
	SetIcon(m_hIcon, FALSE); // Мелкий значок

	// Обновление списка процессов
	UpdateProcessList();

	return TRUE;  // Возвращаем TRUE, чтобы фокус не передавался другим элементам управления
}

void CMFC1111Dlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// При добавлении кнопки свертывания в диалоговое окно нужно воспользоваться приведенным ниже кодом,
//  чтобы нарисовать значок.  Для приложений MFC, использующих модель документов или представлений,
//  это автоматически выполняется рабочей областью.

void CMFC1111Dlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // контекст устройства для рисования

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Выравнивание значка по центру клиентского прямоугольника
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Нарисуйте значок
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// Система вызывает эту функцию для получения отображения курсора при перемещении
//  свернутого окна.
HCURSOR CMFC1111Dlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

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

void CMFC1111Dlg::UpdateProcessList() {
	// Получаем список процессов
	std::vector<std::pair<DWORD, std::wstring>> processList = GetProcessesUsingNtQuery();

	// Получаем указатель на ComboBox
	CComboBox* pComboBox = (CComboBox*)GetDlgItem(IDC_COMBO_PROCESS);

	// Очистить текущие данные в ComboBox
	pComboBox->ResetContent();

	// Добавляем процессы в ComboBox
	for (const auto& process : processList) {
		std::wstring itemText = process.second + L" - PID: " + std::to_wstring(process.first);
		pComboBox->AddString(itemText.c_str());
	}
}



void CMFC1111Dlg::OnCbnSelchangeComboKeys()
{
	// TODO: добавьте свой код обработчика уведомлений
}
