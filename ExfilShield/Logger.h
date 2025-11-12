#pragma once
#include <string>
#include <mutex>
#include <filesystem>
#include <Windows.h>
class Logger
{
public:
	static Logger& Instance();

	void Init(const std::filesystem::path& baseDir);
	void Info(const std::wstring& message);
	void Warn(const std::wstring& message);
	void Error(const std::wstring& message);
	void Debug(const std::wstring& message);
	void RawUTF8(const std::string& message);

	static std::string WtoUTF8(const std::wstring& w);
	static std::wstring LastErrorMessage(DWORD errorCode);

private:
	Logger() = default;

	void WriteLinePrefixUnlocked(std::string& out) const;
	void RotateLogIfNeededUnlocked();
	void EnsureDirectoryExists() const;

	std::mutex mutex_;
	std::filesystem::path baseDir_;
	std::filesystem::path currentFilePath_;
	SYSTEMTIME lastOpenDay_{ 0 };
	HANDLE hLogFile_{ INVALID_HANDLE_VALUE };

};

