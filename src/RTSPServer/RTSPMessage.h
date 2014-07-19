#pragma once


string trim(string s);
bool caselesscompare(string s1, string s2);
vector<string> divideBy(string source, string divider);
string encodeToBase64(const BYTE *p, int cBytes);

class RTSPMessage
{
public:
    RTSPMessage(string s);

    string Request()
    {
        return m_request;
    }

    int Sequence()
    {
        return m_sequence;
    }

    string CreateResponse(int code, string desc);
    string ValueForOption(string option);

    // 0 for aggregate or stream id (1..n)
    int URLTarget()
    {
        return m_target;
    }

private:
    vector<string> m_lines;
    string m_request;
    int m_sequence;
    int m_target;
};
