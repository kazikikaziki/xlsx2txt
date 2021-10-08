/// @file
/// Copyright (c) 2020 Heliodor
/// This software is released under the MIT License.
/// http://opensource.org/licenses/mit-license.php

#pragma once
#include <unordered_set>
#include <vector>

#define K_Grab K__GRAB
#define K_Drop K__DROP

#define K__GRAB(a)  ((a) ? ((a)->grab(),     nullptr) : nullptr) // call KRef::grab()
#define K__DROP(a)  ((a) ? ((a)->drop(), (a)=nullptr) : nullptr) // call KRef::drop()

namespace Kamilo {


class KRef {
public:
	KRef();
	void grab() const;
	void drop() const;
	int getReferenceCount() const;
	virtual const char * getReferenceDebugString() const;
	void setReferenceDebugBreak(int cond_refcnt);

protected:
	virtual ~KRef();

private:
	mutable int m_RefCnt;
	int m_DebubBreakRefCnt;
};




template <class _KRef> class KRefSet {
public:
	KRefSet() {
	}
	bool empty() const {
		return m_Items.empty();
	}
	size_t size() const {
		return m_Items.size();
	}
	void clear() {
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			_KRef *ref = *it;
			if (ref) ref->drop();
		}
		m_Items.clear();
	}
	void insert(_KRef *ref) {
		auto pair = m_Items.insert(ref);
		if (pair.second) {
			ref->grab(); // item added
		} else {
			// item alredy exists
		}
	}
	bool contains(_KRef *ref) {
		auto it = m_Items.find(ref);
		return it != m_Items.end();
	}
	void erase(_KRef *ref) {
		auto it = m_Items.find(ref);
		if (it != m_Items.end()) {
			if (ref) ref->drop();
			m_Items.erase(it);
		}
	}
#if 0
	std::unordered_set<_KRef *>::iterator begin() {
		return m_Items.begin();
	}
	std::unordered_set<_KRef *>::iterator end() {
		return m_Items.end();
	}
#else
	std::unordered_set<_KRef *> items() {
		return m_Items;
	}
#endif

private:
	std::unordered_set<_KRef *> m_Items;
};

template <class _KRef> class KRefArray {
public:
	KRefArray() {
	}
	bool empty() const {
		return m_Items.empty();
	}
	size_t size() const {
		return m_Items.size();
	}
	void clear() {
		for (auto it=m_Items.begin(); it!=m_Items.end(); ++it) {
			_KRef *ref = *it;
			if (ref) ref->drop();
		}
		m_Items.clear();
	}
	void push_back(_KRef *ref) {
		if (ref) ref->grab();
		m_Items.push_back(ref);
	}
	bool contains(_KRef *ref) {
		auto it = m_Items.find(ref);
		return it != m_Items.end();
	}
	void erase(_KRef *ref) {
		auto it = m_Items.find(ref);
		if (it != m_Items.end()) {
			if (ref) ref->drop();
			m_Items.erase(it);
		}
	}
	_KRef * operator[] (int index) const {
		return m_Items[index];
	}
#if 0
	std::vector<_KRef *>::iterator begin() {
		return m_Items.begin();
	}
	std::vector<_KRef *>::iterator end() {
		return m_Items.end();
	}
#endif
	std::vector<_KRef *> items() {
		return m_Items;
	}

private:
	std::vector<_KRef *> m_Items;
};

template <class _KRef> class KAutoRef {
public:
	KAutoRef() {
		m_Ref = nullptr;
	}
	KAutoRef(const _KRef *ref) {
		m_Ref = const_cast<_KRef*>(ref);
		K__GRAB(m_Ref);
	}
	KAutoRef(const KAutoRef<_KRef> &autoref) {
		m_Ref = const_cast<_KRef*>(autoref.m_Ref);
		K__GRAB(m_Ref);
	}
	~KAutoRef() {
		K__DROP(m_Ref);
	}
	void make() {
		K__DROP(m_Ref);
		m_Ref = new _KRef();
	}
	_KRef * get() const {
		return m_Ref;
	}
	void set(const _KRef *ref) {
		K__DROP(m_Ref);
		m_Ref = const_cast<_KRef*>(ref);
		K__GRAB(m_Ref);
	}
	KAutoRef<_KRef> operator = (_KRef *ref) {
		set(ref);
		return *this;
	}
	KAutoRef<_KRef> operator = (const KAutoRef<_KRef> &autoref) {
		set(autoref.m_Ref);
		return *this;
	}
#if 1
	operator _KRef * () const {
		return m_Ref;
	}
	_KRef * operator()() const {
		return m_Ref;
	}
#endif
	bool operator == (const _KRef *ref) const {
		return m_Ref == ref;
	}
	bool operator == (const KAutoRef<_KRef> &autoref) const {
		return m_Ref == autoref.m_Ref;
	}
	bool operator != (const _KRef *ref) const {
		return m_Ref != ref;
	}
	bool operator != (const KAutoRef<_KRef> &autoref) const {
		return m_Ref != autoref.m_Ref;
	}
	bool operator < (const _KRef *ref) const {
		return m_Ref < ref;
	}
	bool operator < (const KAutoRef<_KRef> &autoref) const {
		return m_Ref < autoref.m_Ref;
	}
	_KRef * operator->() const {
		if (m_Ref) {
			return m_Ref;
		} else {
			__debugbreak();
			m_Ref = new _KRef();
			return m_Ref;
		}
	}
	bool isValid() const {
		return m_Ref != nullptr;
	}
	bool isNull() const {
		return m_Ref == nullptr;
	}
private:
	mutable _KRef *m_Ref;
};


} // namespace
