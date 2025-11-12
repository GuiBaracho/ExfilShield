#include "Logger.h"

Logger& Logger::Instance()
{
	static Logger instance;
	return instance;
}

void Logger::Init(const std::filesystem::path& baseDir)
{
	std::scoped_lock lk(mutex_);
	baseDir_ = baseDir;
	// Ensure the log file exists
	EnsureDirectoryExists();
	GetLocalTime(&lastOpenDay_);
	currentFilePath_ = baseDir_ / "exfilshield.log";
}

void Logger::Info(const std::wstring& message)
{
	RawUTF8(WtoUTF8(L"[INFO] " + message));
}

void Logger::Warn(const std::wstring& message)
{
	RawUTF8(WtoUTF8(L"[WARN] " + message));
}

void Logger::Error(const std::wstring& message)
{
	RawUTF8(WtoUTF8(L"[ERROR] " + message));
}

void Logger::Debug(const std::wstring& message)
{
	RawUTF8(WtoUTF8(L"[DEBUG] " + message));
}

void Logger::RotateLogIfNeededUnlocked()
{
	SYSTEMTIME now{};
	GetLocalTime(&now);

	if (now.wYear == lastOpenDay_.wYear &&
		now.wMonth == lastOpenDay_.wMonth &&
		now.wDay == lastOpenDay_.wDay &&
		hLogFile_ != INVALID_HANDLE_VALUE)
	{
		return; // No rotation needed
	}

	// Close existing log file if open
	if (hLogFile_ != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hLogFile_);
		hLogFile_ = INVALID_HANDLE_VALUE;
	}

	// Update last open day
	lastOpenDay_ = now;

	// Open new log file
	std::wstring newFile = std::format(L"exfilShield_{:04d}{:02d}{:02d}.log",
		now.wYear,
		now.wMonth,
		now.wDay);
	currentFilePath_ = baseDir_ / newFile;

	hLogFile_ = CreateFileW(
		currentFilePath_.c_str(),
		FILE_APPEND_DATA,
		FILE_SHARE_READ,
		nullptr,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		nullptr
	);
}

void Logger::WriteLinePrefixUnlocked(std::string& out) const
{
	SYSTEMTIME now;
	GetLocalTime(&now);
	std::string buf = 
		std::format("[{:04}/{:02}/{:02} {:02}:{:02}:{:02}.{:03}] ",
			now.wYear,
			now.wMonth,
			now.wDay,
			now.wHour,
			now.wMinute,
			now.wSecond,
			now.wMilliseconds);
	out.append(buf);
}

void Logger::EnsureDirectoryExists() const
{
	std::error_code ec;
	std::filesystem::create_directories(baseDir_, ec);
}

void Logger::RawUTF8(const std::string& message)
{
	std::scoped_lock lk(mutex_);
	RotateLogIfNeededUnlocked();
	if (hLogFile_ == INVALID_HANDLE_VALUE)
	{
		return; // Log file not available
	}

	std::string logMessage;
	WriteLinePrefixUnlocked(logMessage);
	logMessage.append(message);
	logMessage.append("\r\n");

	DWORD bytesWritten = 0;
	WriteFile(
		hLogFile_,
		logMessage.data(),
		(DWORD)logMessage.size(),
		&bytesWritten,
		nullptr
	);
}

std::string Logger::WtoUTF8(const std::wstring& w)
{
	if (w.empty()) return std::string();
	int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
	std::string strTo(sizeNeeded, '\0');
	WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), strTo.data(), sizeNeeded, nullptr, nullptr);
	return strTo;
}

std::wstring Logger::LastErrorMessage(DWORD errorCode)
{
	LPWSTR buf = nullptr;
	DWORD n = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		errorCode,
		0,
		(LPWSTR)&buf,
		0,
		nullptr
	);

	std::wstring msg = (n && buf) ? std::wstring(buf, n) : L"(unknown error)";

	if (buf) LocalFree(buf);

	return msg;
}

