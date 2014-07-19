#pragma once


#include <strsafe.h>
#include <ShlObj.h>


// writes log output to file if present.
class Logger
{
public:
    Logger(const TCHAR *pFile) :
        m_hFile(NULL)
    {
        TCHAR szPath[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, szPath)))
        {
            StringCbCat(szPath, sizeof(szPath), TEXT("\\"));
            StringCbCat(szPath, sizeof(szPath), pFile);
            if (GetFileAttributes(szPath) != INVALID_FILE_ATTRIBUTES)
            {
                m_hFile = CreateFile(szPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, 0, NULL);
                if (m_hFile == INVALID_HANDLE_VALUE)
                {
                    m_hFile = NULL;
                }
                else
                {
                    SetFilePointer(m_hFile, 0, NULL, FILE_END);
                }
                m_msBase = timeGetTime();

                SYSTEMTIME st;
                GetLocalTime(&st);
                Log("Started %04d-%02d-%02d %02d:%02d:%02d",
                    st.wYear,
                    st.wMonth,
                    st.wDay,
                    st.wHour,
                    st.wMinute,
                    st.wSecond);
            }
        }
    }
    ~Logger()
    {
        if (m_hFile != NULL)
        {
            CloseHandle(m_hFile);
        }
    }

    void Log(const char *pFormat, ...)
    {
#ifndef _DEBUG
        if (m_hFile != NULL)
#endif
        {
            va_list va;
            va_start(va, pFormat);
            char ach[4096];
            StringCbPrintfA(ach, sizeof(ach), "%d:\t", timeGetTime() - m_msBase);
            size_t cchTime;
            StringCchLengthA(ach, sizeof(ach), &cchTime);
            StringCbVPrintfA(ach + cchTime, sizeof(ach)  - (cchTime * sizeof(char)), pFormat, va);
            size_t cch;
            StringCchLengthA(ach, sizeof(ach), &cch);

            va_end(va);

            // debug output without newline and without time (added by existing debug code)
            _bstr_t str = ach + cchTime;
            DbgLog((LOG_TRACE, 0, "%s", (char *)str));

#ifdef _DEBUG
            if (m_hFile != NULL)
#endif
            {
                // add time at start and newline at end for file output
                ach[cch++] = '\r';
                ach[cch++] = '\n';

                CAutoLock lock(&m_csLog);
                DWORD cActual;
                WriteFile(m_hFile, ach, cch * sizeof(char), &cActual, NULL);
            }
        }
    }


private:
    CCritSec m_csLog;
    DWORD m_msBase;
    HANDLE m_hFile;
};

extern Logger theLogger;
#define LOG(x)	theLogger.Log x
