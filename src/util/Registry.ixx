module;
#include <Windows.h>
export module Registry;

import std;
import std.compat;

export
class RegistryEntry {
	// Delete is handled by someone else
	bool ignoreDelete = false;
	// directory == key or value
	bool isDirectory = false;
	HKEY rootKey;
	std::wstring subKeyFormat;
	std::wstring valueNameFormat;
	std::variant<std::wstring, DWORD> data;

public:

	RegistryEntry(HKEY rootKey,
		std::wstring_view subKeyFormat,
		std::wstring_view valueNameFormat,
		std::wstring_view dataFormat,
		bool ignoreDelete = true
	) : ignoreDelete(ignoreDelete), isDirectory(false), rootKey(rootKey), subKeyFormat(subKeyFormat), valueNameFormat(valueNameFormat), data(static_cast<std::wstring>(dataFormat)) {}

	RegistryEntry(HKEY rootKey,
		std::wstring_view subKeyFormat,
		std::wstring_view valueNameFormat,
		DWORD data,
		bool ignoreDelete = true
	) : ignoreDelete(ignoreDelete), isDirectory(false), rootKey(rootKey), subKeyFormat(subKeyFormat), valueNameFormat(valueNameFormat), data(data) {}

	RegistryEntry(HKEY rootKey,
		std::wstring_view subKeyFormat,
		bool ignoreDelete = false
	) : ignoreDelete(ignoreDelete), isDirectory(true), rootKey(rootKey), subKeyFormat(subKeyFormat) {}



	// Context for formatting, used as arguments for key/value/data formats
	struct RegistryContext {
		// {0} = "{710E6680-2905-48CC-A6F5-FFFA6FB6EB41}" - our registry CLSID
		std::wstring pboExplorerCLSID;
		// {1} = "O:\dev\pbofolder\PboExplorer\x64\Debug\PboExplorer.dll" - Full path to dll module path
		std::wstring modulePath;
	};


	HRESULT DoCreate(const RegistryContext& context) {
        const std::wstring fullPath = std::vformat(subKeyFormat, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));

		HKEY hKey;
		DWORD dwDisp;

		auto lResult = RegCreateKeyEx(rootKey,
			fullPath.c_str(),
			0,
			nullptr,
			REG_OPTION_NON_VOLATILE,
			KEY_WRITE,
			nullptr,
			&hKey,
			&dwDisp);


		//E_ACCESSDENIED non admin
		if (lResult == NOERROR)
		{
			// directory has no values
			if (isDirectory) return S_OK;


            const std::wstring fullValueName = std::vformat(valueNameFormat, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));

			std::visit([&](auto&& arg) {
				using T = std::decay_t<decltype(arg)>;
				if constexpr (std::is_same_v<T, std::wstring>) {
					std::wstring fullData = std::vformat(arg, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));

					lResult = RegSetValueExW(hKey,
						fullValueName.empty() ? nullptr : fullValueName.c_str(),
						0,
						REG_SZ,
						reinterpret_cast<const BYTE*>(fullData.c_str()),
						static_cast<DWORD>(fullData.length()) * 2 + 2);
				}
				else if constexpr (std::is_same_v<T, DWORD>) {
					lResult = RegSetValueExW(hKey,
						fullValueName.empty() ? nullptr : fullValueName.c_str(),
						0,
						REG_DWORD,
						reinterpret_cast<const BYTE*>(&arg),
						sizeof(DWORD));
				}
				else
					static_assert(sizeof(T) < 0, "non-exhaustive visitor!");
				}, data);

			RegCloseKey(hKey);
		}
		else
			return lResult;

		return S_OK;
	}


	HRESULT DoDelete(const RegistryContext& context) {
		if (ignoreDelete) return S_OK;
        const std::wstring fullPath = std::vformat(subKeyFormat, std::make_wformat_args(context.pboExplorerCLSID, context.modulePath));

		// if we own this directory, delete its whole tree
		if (isDirectory) {
			return RegDeleteTreeW(rootKey, fullPath.c_str());
		} else {
			// delete single key
			return RegDeleteKeyW(rootKey, fullPath.c_str());
		}
	}

	bool operator==(const RegistryEntry& other) const {
		return
			isDirectory == other.isDirectory &&
			ignoreDelete == other.ignoreDelete &&
			subKeyFormat == other.subKeyFormat &&
			valueNameFormat == other.valueNameFormat &&
			rootKey == other.rootKey;
	}
};

export std::filesystem::path ReadRegistryFilePathKey(HKEY hkey, std::wstring path, std::wstring key) {

	HKEY hKey;
	auto lRes = RegOpenKeyExW(hkey, path.data(), 0, KEY_READ, &hKey);
	if (!SUCCEEDED(lRes))
		return {};

	WCHAR szBuffer[MAX_PATH];
	DWORD dwBufferSize = sizeof(szBuffer);
	DWORD type = 0;
	lRes = RegQueryValueExW(hKey, key.data(), 0, &type, (LPBYTE)szBuffer, &dwBufferSize);
	if (!SUCCEEDED(lRes))
		return {};

	if (type == REG_SZ && std::filesystem::exists(szBuffer))
		return szBuffer;
	if (type == REG_EXPAND_SZ) {
		WCHAR szBufferEx[512];
		ExpandEnvironmentStringsW(szBuffer, szBufferEx, sizeof(szBufferEx));
		if (std::filesystem::exists(szBufferEx))
			return szBufferEx;
	}

	return {};
}
