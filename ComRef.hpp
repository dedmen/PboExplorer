#pragma once
#include <atomic>
#include <utility>

template<class T>
class ComRef
{
	T* _ref = nullptr;

	struct NoAssignAddRef{};
	
	ComRef(T* ref, NoAssignAddRef) : _ref(ref) {}

public:
	ComRef() : _ref(nullptr) {}
	
	ComRef (T* ref) : _ref(ref)
	{
		if (ref)
		    ref->AddRef();
	}

	~ComRef()
	{
		if (_ref)
			_ref->Release();
	}

    // Use when initializing ComRef from pointer returned by QueryInterface
	static ComRef FromQuery(T* ref)
	{
		ComRef ret(ref, NoAssignAddRef{});
		return ret;
	}

	// Use when retrieving instance via QueryInterface
	template <class TargetType = void>
	TargetType** AsQueryInterfaceTarget()
	{
		if (_ref)
			_ref->Release();
		_ref = nullptr;
		
		return reinterpret_cast<TargetType**>(&_ref);
	}

	template<typename ...Args>
	static ComRef Create(Args&& ... args)
	{
		return ComRef(new T(std::forward<Args>(args)...));
	}

	template <typename RetType, typename ...Args>
	static T* CreateForReturn(void** result, Args&& ... args) {
		auto newObj = new T(std::forward<Args>(args)...);
		newObj->AddRef();
		*result = static_cast<RetType*>(newObj);
		return newObj;
	}

	template <typename RetType, typename ...Args>
	static T* CreateForReturn(RetType** result, Args&& ... args) {
		auto newObj = new T(std::forward<Args>(args)...);
		newObj->AddRef();
		*result = static_cast<RetType*>(newObj);
		return newObj;
	}

	template<typename ...Args>
	static [[nodiscard]] T* CreateForReturn(Args&& ... args) {
		auto newObj = new T(std::forward<Args>(args)...);
		newObj->AddRef();
		return newObj;
	}
	
	T& operator*()
	{
		return *_ref;
	}

	T* operator->()
	{
		return _ref;
	}

	const T* operator->() const
	{
		return _ref;
	}

	operator T* () {
		return _ref;
	}

	T** operator&() {
		return &_ref;
	}

	ComRef& operator=(T* ref)
	{
		if (ref)
			ref->AddRef();
		
		if (_ref)
			_ref->Release();
		_ref = ref;
		
		return *this;
	}

	operator bool()
	{
		return _ref != nullptr;
	}

	T* GetRef() {
		return _ref;
	}

	const T* GetRef() const {
		return _ref;
	}
};

template<class T>
class CoTaskMemRef
{
	class DataKeeper
	{
		T* _ref;
		std::atomic<uint32_t> refC;
	};
	
	DataKeeper* _ref = nullptr;

	void AddRef()
	{
		if (!_ref) return;
		++_ref->refC;
	}

	void Release()
	{
		if (!_ref) return;
		--_ref->refC;
		if (!_ref->refC)
		{
			CoTaskMemFree(_ref->_ref);
			delete _ref;
			_ref = nullptr;
		}
			
	}

public:
	CoTaskMemRef() : _ref(nullptr) {}
	
	CoTaskMemRef(const CoTaskMemRef& ref) : _ref(ref->_ref)
	{
		AddRef();
	}
	
	~CoTaskMemRef()
	{
		Release();
	}

	T* operator*() {
		return _ref->_ref;
	}

	T* operator->() {
		return _ref->_ref;
	}

	operator T* () {
		return _ref;
	}

	CoTaskMemRef& operator=(const CoTaskMemRef& ref)
	{
		Release();
		_ref = ref._ref;
		AddRef();

		return *this;
	}
};


template<class T>
class CoTaskMemRefS
{
	T* _ref = nullptr;

public:
	CoTaskMemRefS() noexcept : _ref(nullptr) {}
	CoTaskMemRefS(const CoTaskMemRefS& ref) = delete;
	CoTaskMemRefS(CoTaskMemRefS&& ref) noexcept : _ref(ref._ref)
	{
		ref._ref = nullptr;
	}
	CoTaskMemRefS(T* ref) : _ref(ref) {}

	~CoTaskMemRefS()
	{
		if (_ref)
			CoTaskMemFree(_ref);
	}

	T* operator*()
	{
		return _ref;
	}

	T* operator->()
	{
		return _ref;
	}

	operator T* () {
		return _ref;
	}

	operator const T* () const {
		return _ref;
	}


	T** operator&() {
		return &_ref;
	}

	T* GetRef() {
		return _ref;
	}

	const T* GetRef() const {
		return _ref;
	}

	

	CoTaskMemRefS& operator=(const CoTaskMemRefS& ref) = delete;
	CoTaskMemRefS& operator=(CoTaskMemRefS&& ref) noexcept
	{
		_ref = ref._ref;
		ref._ref = nullptr;
		return *this;
	}
	CoTaskMemRefS& operator=(T* ref)
	{
		if (_ref == ref) return *this;
		if (_ref) CoTaskMemFree(_ref);
		_ref = ref;
		return *this;
	}
};

template<class ...T>
class COMJoiner : public T... {


	template<class ST>
	__forceinline bool CheckSingle(const IID& riid, void** ppv) {

		if (IsEqualIID(riid, __uuidof(ST))) {
			*ppv = static_cast<ST*>(this);
			return true;
		}

		return false;
	}

	template<class ST>
	__forceinline bool CheckSingleIUnknown(void** ppv) {
		if (std::is_convertible_v<ST*, IUnknown*>) {
			*ppv = static_cast<IUnknown*>(static_cast<ST*>(this));
			return true;
		}
		return false;
	}



public:

	bool QueryInterfaceJoiner(const IID& riid, void** ppv) {
		if ((CheckSingle<T>(riid, ppv) || ...))
			return true;

		if (IsEqualIID(riid, __uuidof(IUnknown))) {
			// use IUnknown of first COM base that implements it
			return (CheckSingleIUnknown<T>(ppv) || ...);
		}

		return false;
	}

};



template<class Base, class ...T>
class RefCountedCOM : public COMJoiner<T...> {
	std::atomic_uint32_t refCount;

public:

	virtual ~RefCountedCOM() = default;

	ULONG STDMETHODCALLTYPE AddRef(void) override
	{
		return ++refCount;
	}
	ULONG STDMETHODCALLTYPE Release(void) override
	{
		if (--refCount == 0)
		{
			delete static_cast<Base*>(this);
			return 0;
		}

		return refCount;
	}
};

template<class Base>
class RefCounted {
	std::atomic_uint32_t refCount;
public:

	virtual ~RefCounted() = default;

	uint32_t AddRef()
	{
		return ++refCount;
	}
	uint32_t Release()
	{
		if (--refCount == 0)
		{
			delete static_cast<Base*>(this);
			return 0;
		}

		return refCount;
	}
};


inline std::atomic_uint32_t g_DllRefCount;
class GlobalRefCounted {
public:
	GlobalRefCounted() {
		++g_DllRefCount;
	}
	virtual ~GlobalRefCounted() {
		--g_DllRefCount;
	}
};
