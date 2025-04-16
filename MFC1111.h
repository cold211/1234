
// MFC1111.h: главный файл заголовка для приложения PROJECT_NAME
//

#pragma once

#ifndef __AFXWIN_H__
	#error "включить pch.h до включения этого файла в PCH"
#endif

#include "resource.h"		// основные символы


// CMFC1111App:
// Сведения о реализации этого класса: MFC1111.cpp
//

class CMFC1111App : public CWinApp
{
public:
	CMFC1111App();

// Переопределение
public:
	virtual BOOL InitInstance();

// Реализация

	DECLARE_MESSAGE_MAP()
};

extern CMFC1111App theApp;
