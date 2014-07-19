#include "StdAfx.h"
#include "RTSPMessage.h"


string trim(string s)
{
    size_t pos = s.find_first_not_of(" \t\r\n");
    if (pos != string::npos)
    {
        s = s.substr(pos);
    }
    pos = s.find_last_not_of(" \t\r\n");
    if (pos != string::npos)
    {
        s.erase(pos + 1);
    }
    return s;
}

bool caselesscompare(string s1, string s2)
{
    if (s1.length() != s2.length())
    {
        return false;
    }
    for (string::iterator i1 = s1.begin(), i2 = s2.begin(); i1 != s1.end(); i1++, i2++)
    {
        if (tolower(*i1) != tolower(*i2))
        {
            return false;
        }
    }
    return true;
}

vector<string> divideBy(string source, string divider)
{
    vector<string> components;
    size_t pos = 0;
    for(;;)
    {
        size_t posNext = source.find(divider, pos);
        string s = source.substr(pos, posNext == string::npos ? posNext : posNext - pos);
        components.push_back(s);
        if (posNext == string::npos)
        {
            break;
        }
        pos = posNext + divider.length();
    }
    return components;
}

static const char *Base64Mapping = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

string encodeLong(unsigned long val, int nPad)
{
    char ch[4];
    int cch = 4 - nPad;
    for (int i = 0; i < cch; i++)
    {
        int shift = 6 * (cch - (i + 1));
        int bits = (val >> shift) & 0x3f;
        ch[i] = Base64Mapping[bits];
    }
    for (int i = 0; i < nPad; i++)
    {
        ch[cch + i] = '=';
    }
    string s(ch, 4);
    return s;
}

string encodeToBase64(const BYTE *p, int cBytes)
{
    string s;
    while (cBytes >= 3)
    {
        unsigned long val = (p[0] << 16) + (p[1] << 8) + p[2];
        p += 3;
        cBytes -= 3;

        s = s.append(encodeLong(val, 0));
    }
    if (cBytes > 0)
    {
        int nPad;
        unsigned long val;
        if (cBytes == 1)
        {
            // pad 8 bits to 2 x 6 and add 2 ==
            nPad = 2;
            val = p[0] << 4;
        }
        else
        {
            // must be two bytes -- pad 16 bits to 3 x 6 and add one =
            nPad = 1;
            val = (p[0] << 8) + p[1];
            val = val << 2;
        }
        s = s.append(encodeLong(val, nPad));
    }
    return s;
}

RTSPMessage::RTSPMessage(string s) :
    m_target(0)
{
    m_lines = divideBy(s, "\r\n");
    if (m_lines.size() > 1)
    {
        vector<string> comps = divideBy(m_lines[0], " ");
        m_request = comps[0];
        string url = comps[1];
        if (caselesscompare(url, "*"))
        {
            m_target = 0;
        }
        else
        {
            url = url.substr(url.find_last_of("/") + 1);
            if ((url.length() == 0) || !caselesscompare(url.substr(0, 9), "streamid="))
            {
                m_target = 0;
            }
            else
            {
                m_target = stoi(url.substr(9));
            }
        }
    }
    string sSeq = ValueForOption(string("CSeq"));
    if (sSeq.length() > 0)
    {
        m_sequence = stoi(sSeq);
    }
}

string RTSPMessage::ValueForOption(string option)
{
    for (size_t i = 1; i < m_lines.size(); i++)
    {
        string line = m_lines[i];
        size_t pos = line.find(":");
        if (pos != string::npos)
        {
            string param = line.substr(0, pos);
            if (caselesscompare(param, option))
            {
                return trim(line.substr(pos + 1));
            }
        }
    }
    return string();
}

string RTSPMessage::CreateResponse(int code, string desc)
{
    ostringstream strm;
    strm << "RTSP/1.0 " << code << " " << desc << "\r\nCSeq: " << Sequence() << "\r\n";
    return strm.str();
}
