#pragma once
#include <string>
#include <vector>
#include <filesystem>

#include "DeviceIdentity.h"
#include <unordered_set>
#include <unordered_map>
#include <shared_mutex>

enum class PolicyAction
{
	Allow,
	Block,
	Audit
};

struct PolicyEntry
{
	std::wstring vid;      // optional
	std::wstring pid;      // optional
	std::wstring serial;   // optional
};

struct GuidHash {
	size_t operator()(const GUID& guid) const noexcept {
		const auto* data = reinterpret_cast<const uint8_t*>(&guid);
		size_t hash = 0;
		for (size_t i = 0; i < sizeof(GUID); ++i) {
			hash = (hash * 31) + data[i];
		}
		return hash;
	}
};

class PolicyManager
{
public:
	static PolicyManager& Instance();

	bool LoadPolicies(const std::filesystem::path& policyFilePath);
	PolicyAction EvaluateDevice(const DeviceIdentity& device) noexcept;

	PolicyAction GetDefaultAction() const noexcept { return defaultAction_; }
	void SetDefaultAction(PolicyAction action) noexcept { defaultAction_ = action; }

	void OnArrival(const DeviceIdentity& device, bool allowed) noexcept;
	void OnRemoval(const std::wstring& path) noexcept;

private:
	PolicyManager() = default;
	static PolicyAction ParseAction(const std::string& actionStr);
	static std::wstring ToLower(const std::wstring& str);
	static std::wstring ToLower(const std::string& str);
	static std::wstring Utf8ToWstring(const std::string& input);
	bool IsAllowed(const GUID& cid) const noexcept;
	bool IsBlocked(const GUID& cid) const noexcept;

	std::vector<PolicyEntry> whitelist_;
	std::vector<PolicyEntry> blacklist_;
	PolicyAction defaultAction_ = PolicyAction::Block;
	PolicyAction blacklistAction_ = PolicyAction::Block;

	mutable std::shared_mutex mutex_;
	std::unordered_set<GUID, GuidHash> allowed_;
	std::unordered_set<GUID, GuidHash> blocked_;
	std::unordered_map<std::wstring, GUID> pathToContainer_;
	std::unordered_map<GUID, uint32_t, GuidHash> refCount_;
};

