#pragma once

#include <Windows.h>
#include <string>

class EventWriter
{
public:
	static EventWriter& Instance();
	void Init(const wchar_t* sourceName = L"ExfilShield");
	void Info(WORD eventId, const std::wstring msg);
	void Warn(WORD eventId, const std::wstring msg);
	void Error(WORD eventId, const std::wstring msg);

private:
	EventWriter() = default;
	HANDLE h_ = nullptr;
};

