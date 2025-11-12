#include "PolicyManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cwctype>
#include <nlohmann/json.hpp>
#include <shared_mutex>
using json = nlohmann::json;

PolicyManager& PolicyManager::Instance()
{
	static PolicyManager instance; // NOSONAR: local static is intentional to ensure safe initialization
	return instance;
}

bool PolicyManager::LoadPolicies(const std::filesystem::path& policyFilePath)
{
    std::ifstream f(policyFilePath);
    if (!f.is_open()) {
        Logger::Instance().Error(L"Cannot open config file");
        return false;
    }

    json j;  f >> j;

    // clear old
    whitelist_.clear();
    blacklist_.clear();

    if (auto it = j.find("actions"); it != j.end()) {
        if (it->contains("default"))
            defaultAction_ = ParseAction((*it)["default"].get<std::string>());
        if (it->contains("blacklist"))
            blacklistAction_ = ParseAction((*it)["blacklist"].get<std::string>());
    }

    for (auto& w : j["whitelist"])
        whitelist_.emplace_back(ToLower(w.value("vid","")), ToLower(w.value("pid","")), ToLower(w.value("serial","")));

    for (auto& b : j["blacklist"])
        blacklist_.emplace_back(ToLower(b.value("vid","")), ToLower(b.value("pid","")), ToLower(b.value("serial","")));

    Logger::Instance().Info(L"Policy file loaded via nlohmann/json");
    return true;
}

PolicyAction PolicyManager::EvaluateDevice(const DeviceIdentity& device) noexcept
{
    if (IsAllowed(device.containerId))
		return PolicyAction::Allow;
	else if (IsBlocked(device.containerId))
		return PolicyAction::Block;

    const auto vid = ToLower(device.vid);
    const auto pid = ToLower(device.pid);
    const auto serial = ToLower(device.serial);

    auto Matches = [&](const PolicyEntry& e){
        if (!e.vid.empty() && e.vid != vid) return false;
        if (!e.pid.empty() && e.pid != pid) return false;
        if (!e.serial.empty() && e.serial != serial) return false;
        return true;
        };

    for (const auto& e : blacklist_)
        if (Matches(e)) {
			OnArrival(device, false);
			return blacklistAction_;
        }

    for (const auto& e : whitelist_)
        if (Matches(e)) {
			OnArrival(device, true);
            return PolicyAction::Allow;
        }

    return defaultAction_;
}

void PolicyManager::OnArrival(const DeviceIdentity& device, bool allowed) noexcept
{
	std::unique_lock lk(mutex_);
	pathToContainer_[device.devicePath] = device.containerId;
    refCount_[device.containerId]++;
    if (allowed) {
        allowed_.insert(device.containerId);
        blocked_.erase(device.containerId);
    }
    else {
        blocked_.insert(device.containerId);
        allowed_.erase(device.containerId);
    }
}

void PolicyManager::OnRemoval(const std::wstring& path) noexcept
{
    std::unique_lock lk(mutex_);
    auto it = pathToContainer_.find(path);
    if (it == pathToContainer_.end()) return;
    GUID cid = it->second;
    pathToContainer_.erase(it);
    auto rcIt = refCount_.find(cid);
    if (rcIt != refCount_.end()) {
        --(rcIt->second);
        if (rcIt->second == 0) {
            refCount_.erase(rcIt);
            allowed_.erase(cid);
            blocked_.erase(cid);
        }
    }
}

PolicyAction PolicyManager::ParseAction(const std::string& actionStr)
{
	auto lower = ToLower(actionStr);
	if (lower == L"allow")  return PolicyAction::Allow;
	if (lower == L"block")  return PolicyAction::Block;
	return PolicyAction::Audit;
}

std::wstring PolicyManager::ToLower(const std::wstring& str)
{
    std::wstring result = str;
    for (auto& ch : result)
        ch = static_cast<wchar_t>(std::towlower(ch));
    return result;
}

std::wstring PolicyManager::ToLower(const std::string& str)
{
	std::wstring result = Utf8ToWstring(str);
	for (auto& ch : result)
		ch = static_cast<wchar_t>(std::towlower(ch));
	return result;
}

std::wstring PolicyManager::Utf8ToWstring(const std::string& input)
{
    if (input.empty())
        return L"";

    int size_needed = MultiByteToWideChar(CP_UTF8, 0,
        input.c_str(),
        static_cast<int>(input.size()),
        nullptr, 0);

    std::wstring result(size_needed, L'\0');

    MultiByteToWideChar(CP_UTF8, 0,
        input.c_str(),
        static_cast<int>(input.size()),
        result.data(),
        size_needed);

    return result;
}

bool PolicyManager::IsAllowed(const GUID& cid)const noexcept
{
    std::shared_lock lk(mutex_);
    return allowed_.contains(cid);
}

bool PolicyManager::IsBlocked(const GUID& cid) const noexcept
{
    std::shared_lock lk(mutex_);
    return blocked_.contains(cid);
}
