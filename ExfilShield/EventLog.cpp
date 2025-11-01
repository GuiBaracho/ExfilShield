#include "EventLog.h"

EventWriter& EventWriter::Instance() {
	static EventWriter w;
	return w;
}

void EventWriter::Init(const wchar_t* sourceName)
{
	h_ = RegisterEventSourceW(nullptr, sourceName);
}

static void WriteEvent(HANDLE h, WORD type, WORD id, const std::wstring& msg) {
	if (!h) return;
	LPCWSTR strs[1] = { msg.c_str() };
	ReportEvent(h, type, 0, id, nullptr, 1, 0, strs, nullptr);
}

void EventWriter::Info(WORD eventId, const std::wstring msg)
{
	WriteEvent(h_, EVENTLOG_INFORMATION_TYPE, eventId, msg);
}

void EventWriter::Warn(WORD eventId, const std::wstring msg)
{
	WriteEvent(h_, EVENTLOG_WARNING_TYPE, eventId, msg);
}

void EventWriter::Error(WORD eventId, const std::wstring msg)
{
	WriteEvent(h_, EVENTLOG_ERROR_TYPE, eventId, msg);
}

