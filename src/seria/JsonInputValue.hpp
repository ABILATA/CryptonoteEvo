// Copyright (c) 2012-2018, The CryptoNote developers, The Bytecoin developers, [ ] developers
//
// This file is part of Bytecoin.
//
// Bytecoin is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Bytecoin is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Bytecoin.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include "common/JsonValue.hpp"
#include "ISeria.hpp"

namespace seria {

class JsonInputValue : public ISeria {
public:
	JsonInputValue(const common::JsonValue &value);
	JsonInputValue(common::JsonValue &&value);

	virtual bool isInput() const override { return true; }

	virtual void beginObject() override;
	virtual void objectKey(common::StringView name) override;
	virtual void endObject() override;

	virtual void beginMap(size_t &size) override;
	virtual void nextMapKey(std::string &name) override;
	virtual void endMap() override { endObject(); }

	virtual void beginArray(size_t &size, bool fixed_size = false) override;
	virtual void endArray() override;

	virtual void seria_v(uint8_t &value) override;
	virtual void seria_v(int16_t &value) override;
	virtual void seria_v(uint16_t &value) override;
	virtual void seria_v(int32_t &value) override;
	virtual void seria_v(uint32_t &value) override;
	virtual void seria_v(int64_t &value) override;
	virtual void seria_v(uint64_t &value) override;
	virtual void seria_v(double &value) override;
	virtual void seria_v(bool &value) override;
	virtual void seria_v(std::string &value) override;
	virtual void seria_v(common::BinaryArray &value) override;
	virtual void binary(void *value, size_t size) override;
private:
	common::JsonValue value;
	const common::JsonValue *objectKeyValue = nullptr;
	std::vector<const common::JsonValue *> chain;
	std::vector<size_t> idxs;
	std::vector<common::JsonValue::Object::const_iterator> itrs;

	const common::JsonValue *getValue();

	template<typename T>
	void getInteger(T &v) {
		const common::JsonValue *val = getValue();
		if( val )
			v = static_cast<T>(val->get_integer());
	}
	template<typename T>
	void getUnsigned(T &v) {
		const common::JsonValue *val = getValue();
		if( val )
			v = static_cast<T>(val->get_unsigned());
	}
};

template<typename T>
void fromJsonValue(T &v, const common::JsonValue &js) {
	JsonInputValue s(js);
	s(v);
}

}
