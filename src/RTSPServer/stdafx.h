// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <WinSock2.h>

#include <streams.h>

#include <string>
#include <vector>
#include <list>
#include <sstream>
using namespace std;

#include <comdef.h>
#define SMARTPTR(x)	_COM_SMARTPTR_TYPEDEF(x, __uuidof(x))
#define QI_IMPL(T)	if (iid == __uuidof(T)) { return GetInterface((T*)this, ppv); }

// to use classes that define operator& (such as COM smart pointers) inside an STL collection,
// we need to wrap them in a class like CAdapt. Including this here removes our dependency on ATL.
template <class T>
class CAdapts
{
public:
	CAdapts()
	{
	}
	CAdapts(__in const T& rSrc) :
		m_T( rSrc )
	{
	}

	CAdapts(__in const CAdapts& rSrCA) :
		m_T( rSrCA.m_T )
	{
	}

	CAdapts& operator=(__in const T& rSrc)
	{
		m_T = rSrc;
		return *this;
	}
	bool operator<(__in const T& rSrc) const
	{
		return m_T < rSrc;
	}
	bool operator==(__in const T& rSrc) const
	{
		return m_T == rSrc;
	}
	operator T&()
	{
		return m_T;
	}

	operator const T&() const
	{
		return m_T;
	}

	T m_T;
};

SMARTPTR(IMediaSample);

typedef std::list<CAdapts<IMediaSamplePtr> > sample_list_t;

#include "logging.h"