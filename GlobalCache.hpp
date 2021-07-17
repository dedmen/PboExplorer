#pragma once

#include <any>
#include <functional>
#include <mutex>
#include <type_traits>
#include "Util.hpp"




class GlobalCache
{
	// unique ptr so we can reliably return reference and don't need to worry about map realloc maybe messing us up
	using CacheDirT = Util::unordered_map_stringkey<std::unique_ptr<std::any>>;
	CacheDirT cacheEntries;
	std::recursive_mutex cacheLock;

public:

	template<class Functor>
	const typename std::invoke_result_t<Functor>& GetFromCache(std::string_view cacheKey, Functor generator) {
		std::unique_lock lock(cacheLock);

		auto found = cacheEntries.find(cacheKey);
		if (found == cacheEntries.end()) {
			found = cacheEntries.insert(CacheDirT::value_type{ cacheKey, std::make_unique<std::any>(generator()) }).first;
		}

		return std::any_cast<const typename std::invoke_result_t<Functor>&>(*found->second);
	}


};

inline GlobalCache GCache;



