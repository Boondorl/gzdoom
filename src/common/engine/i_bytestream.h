/*
** i_bytestream.h
** Simple byte stream readers and writers for handling demo data.
**
**---------------------------------------------------------------------------
** Copyright 1998-2006 Randy Heit
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifndef __I_BYTESTREAM_H__
#define __I_BYTESTREAM_H__

#include "tarray.h"
#include "zstring.h"

// Byte streams.

class ByteWriter
{
	const TArrayView<uint8_t> _buffer = { nullptr, 0u };
	size_t _curPos = 0u;
public:
	ByteWriter(const TArrayView<uint8_t> buffer) : _buffer(buffer) {}

	inline bool WouldWritePastEnd(size_t bytes) const { return _curPos + bytes >= Size(); }
	inline size_t GetWrittenBytes() const { return _curPos; }
	inline TArrayView<uint8_t> GetWrittenData() const { return { Data(), _curPos }; }
	inline TArrayView<uint8_t> GetRemainingData() const { return { &_buffer[_curPos], Size() - _curPos }; }
	inline uint8_t* Data() const { return _buffer.Data(); }
	inline size_t Size() const { return _buffer.Size(); }

	void Reset()
	{
		_curPos = 0u;
	}
	void SkipBytes(size_t bytes)
	{
		assert(_curPos + bytes < Size());
		_curPos += bytes;
	}
	void WriteBytes(const TArrayView<const uint8_t> bytes)
	{
		memcpy(&_buffer[_curPos], bytes.Data(), bytes.Size());
		SkipBytes(bytes.Size());
	}
	template<typename T>
	void WriteValue(T value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		*(T*)&buffer[_curPos] = T;
		SkipBytes(sizeof(T));
	}
	template<typename T>
	void WriteType(const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		*(T*)&buffer[_curPos] = T;
		SkipBytes(sizeof(T));
	}
};

class ByteReader
{
	const TArrayView<const uint8_t> _buffer = { nullptr, 0u };
	size_t _curPos = 0u;

	TArrayView<const uint8_t> InternalReadBytes(size_t bytes, bool skip)
	{
		const TArrayView<const uint8_t> view = { &_buffer[_curPos], bytes };
		if (skip)
			SkipBytes(bytes);
		return view;
	}
public:
	ByteReader(const TArrayView<const uint8_t> buffer) : _buffer(buffer) {}

	inline bool WouldReadPastEnd(size_t bytes) const { return _curPos + bytes >= Size(); }
	inline size_t GetReadBytes() const { return _curPos; }
	inline TArrayView<const uint8_t> GetReadData() const { return { Data(), _curPos }; }
	inline TArrayView<const uint8_t> GetRemainingData() const { return { &_buffer[_curPos], Size() - _curPos }; }
	inline const uint8_t* Data() const { return _buffer.Data(); }
	inline size_t Size() const { return _buffer.Size(); }

	void Reset()
	{
		_curPos = 0u;
	}
	void SkipBytes(size_t bytes)
	{
		assert(_curPos + bytes < Size());
		_curPos += bytes;
	}
	TArrayView<const uint8_t> ReadBytes(size_t bytes)
	{
		return InternalReadBytes(bytes, true);
	}
	template<typename T>
	T ReadValue()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return *(T*)ReadBytes(sizeof(T)).Data();
	}
	template<typename T>
	void ReadType(T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		value = *(T*)ReadBytes(sizeof(T)).Data();
	}
	TArrayView<const uint8_t> PeekBytes(size_t bytes)
	{
		return InternalReadBytes(bytes, false);
	}
	template<typename T>
	T PeekValue()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return *(T*)PeekBytes(sizeof(T)).Data();
	}
	template<typename T>
	void PeekType(T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		value = *(T*)PeekBytes(sizeof(T)).Data();
	}
};

// Byte stream handlers.
// NOTE: Strings are intentionally NOT serialized with their null terminators as this is
// a completely unreliable way of determining their length (malformed strings will mess with buffers in
// unpredictable ways). Instead they're serialized as arrays of chars. This is not an oversight, please do
// not "fix" this as FString itself also automatically adds a null terminator when constructing from a
// char array.

class WriteStream
{
	ByteWriter _writer;
public:
	enum { IsWriting = 1 };
	enum { IsReading = 0 };

	WriteStream(const TArrayView<uint8_t> buffer) : _writer(buffer) {}

	inline void Reset() { _writer.Reset(); }
	inline size_t GetWrittenBytes() const { return _writer.GetWrittenBytes(); }
	inline TArrayView<uint8_t> GetWrittenData() const { return _writer.GetWrittenData(); }
	inline TArrayView<uint8_t> GetRemainingData() const { return _writer.GetRemainingData(); }
	inline size_t Size() const { return _writer.Size(); }
	inline uint8_t* Data() const { return _writer.Data(); }

	bool SerializeInt8(int8_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(int8_t)))
			return false;
		_writer.WriteValue<int8_t>(value);
		return true;
	}
	bool SerializeUInt8(uint8_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(uint8_t)))
			return false;
		_writer.WriteValue<uint8_t>(value);
		return true;
	}
	bool SerializeInt16(int16_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(int16_t)))
			return false;
		_writer.WriteValue<int16_t>(value);
		return true;
	}
	bool SerializeUInt16(uint16_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(uint16_t)))
			return false;
		_writer.WriteValue<uint16_t>(value);
		return true;
	}
	bool SerializeInt32(int32_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(int32_t)))
			return false;
		_writer.WriteValue<int32_t>(value);
		return true;
	}
	bool SerializeUInt32(uint32_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(uint32_t)))
			return false;
		_writer.WriteValue<uint32_t>(value);
		return true;
	}
	bool SerializeInt64(int64_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(int64_t)))
			return false;
		_writer.WriteValue<int64_t>(value);
		return true;
	}
	bool SerializeUInt64(uint64_t value)
	{
		if (_writer.WouldWritePastEnd(sizeof(uint64_t)))
			return false;
		_writer.WriteValue<uint64_t>(value);
		return true;
	}
	// Wrappers for easier writing.
	bool SerializeBool(bool value)
	{
		return SerializeUInt8((uint8_t)value);
	}
	bool SerializeFloat(float value)
	{
		union {
			float f;
			uint32_t u;
		} temp;
		temp.f = value;
		return SerializeUInt32(temp.u);
	}
	bool SerializeDouble(double value)
	{
		union {
			double d;
			uint64_t u;
		} temp;
		temp.d = value;
		return SerializeUInt64(temp.u);
	}
	bool SerializeString(const FString& str)
	{
		return SerializeArray<char>({ str.GetChars(), str.Len() }, 0u, false);
	}
	template<typename T>
	bool SerializeType(const T& value)
	{
		if (_writer.WouldWritePastEnd(sizeof(T)))
			return false;
		_writer.WriteType<T>(value);
		return true;
	}
	template<typename T>
	bool SerializeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const size_t len = values.Size();
		if (expected && ((!exact && len > expected) || (exact && len != expected)))
			return false;
		const size_t size = len * sizeof(T);
		if (_writer.WouldWritePastEnd((sizeof(uint16_t) + size)))
			return false;
		const uint16_t l = (uint16_t)len;
		_writer.WriteValue<uint16_t>(l);
		if (size)
		{
			const TArrayView<const uint8_t> bytes = { (const uint8_t*)values.Data(), size };
			_writer.WriteBytes(bytes);
		}
		return true;
	}
	template<typename T>
	bool SerializeChunk(const TArrayView<const T> values, size_t expected, bool exact)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const size_t len = values.Size();
		if (expected && ((!exact && len > expected) || (exact && len != expected)))
			return false;
		const size_t size = len * sizeof(T);
		if (_writer.WouldWritePastEnd((sizeof(uint32_t) + size)))
			return false;
		const uint32_t l = (uint32_t)len;
		_writer.WriteValue<uint32_t>(l);
		if (size)
		{
			const TArrayView<const uint8_t> bytes = { (const uint8_t*)values.Data(), size };
			_writer.WriteBytes(bytes);
		}
		return true;
	}
};

class ReadStream
{
	ByteReader _reader;
public:
	enum { IsWriting = 0 };
	enum { IsReading = 1 };

	ReadStream(const TArrayView<const uint8_t> buffer) : _reader(buffer) {}

	inline void Reset() { _reader.Reset(); }
	inline size_t GetReadBytes() const { return _reader.GetReadBytes(); }
	inline TArrayView<const uint8_t> GetReadData() const { return _reader.GetReadData(); }
	inline TArrayView<const uint8_t> GetRemainingData() const { return _reader.GetRemainingData(); }
	inline size_t Size() const { return _reader.Size(); }
	inline const uint8_t* Data() const { return _reader.Data(); }

	// Direct read API. This has no safety checking so use with extreme caution.
	FString ReadString()
	{
		const auto data = ReadArray<char>();
		return FString(data.Data(), data.Size());
	}
	template<typename T>
	T ReadValue()
	{
		return _reader.ReadValue<T>();
	}
	template<typename T>
	void ReadType(T& value)
	{
		_reader.ReadType<T>(value);
	}
	template<typename T>
	TArrayView<const T> ReadArray()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint16_t len = _reader.ReadValue<uint16_t>();
		return { (T*)_reader.ReadBytes(len * sizeof(T)).Data(), len };
	}
	template<typename T>
	TArrayView<const T> ReadChunk()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint32_t len = _reader.ReadValue<uint32_t>();
		return { (T*)_reader.ReadBytes(len * sizeof(T)).Data(), len };
	}

	// Peeking API.
	FString PeekString()
	{
		const auto data = PeekArray<char>();
		return FString(data.Data(), data.Size());
	}
	template<typename T>
	T PeekValue()
	{
		return _reader.PeekValue<T>();
	}
	template<typename T>
	void PeekType(T& value)
	{
		_reader.PeekType<T>(value);
	}
	template<typename T>
	TArrayView<const T> PeekArray()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint16_t len = _reader.PeekValue<uint16_t>();
		return { (T*)(_reader.PeekBytes(sizeof(uint16_t) + len * sizeof(T)).Data() + sizeof(uint16_t)), len };
	}
	template<typename T>
	TArrayView<const T> PeekChunk()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint32_t len = _reader.PeekValue<uint32_t>();
		return { (T*)(_reader.PeekBytes(sizeof(uint32_t) + len * sizeof(T)).Data() + sizeof(uint32_t)), len };
	}

	// Networking API.
	bool SerializeInt8(int8_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(int8_t)))
			return false;
		value = _reader.ReadValue<int8_t>();
		return true;
	}
	bool SerializeUInt8(uint8_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(uint8_t)))
			return false;
		value = _reader.ReadValue<uint8_t>();
		return true;
	}
	bool SerializeInt16(int16_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(int16_t)))
			return false;
		value = _reader.ReadValue<int16_t>();
		return true;
	}
	bool SerializeUInt16(uint16_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(uint16_t)))
			return false;
		value = _reader.ReadValue<uint16_t>();
		return true;
	}
	bool SerializeInt32(int32_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(int32_t)))
			return false;
		value = _reader.ReadValue<int32_t>();
		return true;
	}
	bool SerializeUInt32(uint32_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(uint32_t)))
			return false;
		value = _reader.ReadValue<uint32_t>();
		return true;
	}
	bool SerializeInt64(int64_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(int64_t)))
			return false;
		value = _reader.ReadValue<int64_t>();
		return true;
	}
	bool SerializeUInt64(uint64_t& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(uint64_t)))
			return false;
		value = _reader.ReadValue<uint64_t>();
		return true;
	}
	// Wrappers for easier reading.
	bool SerializeBool(bool& value)
	{
		uint8_t val;
		if (!SerializeUInt8(val))
			return false;
		value = (bool)val;
		return true;
	}
	bool SerializeFloat(float& value)
	{
		union {
			float f;
			uint32_t u;
		} temp;
		if (!SerializeUInt32(temp.u))
			return false;
		value = temp.f;
		return true;
	}
	bool SerializeDouble(double& value)
	{
		union {
			double d;
			uint64_t u;
		} temp;
		if (!SerializeUInt64(temp.u))
			return false;
		value = temp.d;
		return true;
	}
	bool SerializeString(FString& str)
	{
		TArrayView<const char> data;
		if (!SerializeArray<char>(data, 0u, false))
			return false;
		str = FString(data.Data(), data.Size());
		return true;
	}
	template<typename T>
	bool SerializeType(T& value)
	{
		if (_reader.WouldReadPastEnd(sizeof(T)))
			return false;
		_reader.ReadType<T>(value);
		return true;
	}
	template<typename T>
	bool SerializeArray(TArrayView<const T>& values, size_t expected, bool exact)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		if (_reader.WouldReadPastEnd(sizeof(uint16_t)))
			return false;
		const uint16_t len = _reader.ReadValue<uint16_t>();
		if (expected && ((!exact && len > expected) || (exact && len != expected)))
			return false;
		const size_t size = len * sizeof(T);
		if (size)
		{
			if (_reader.WouldReadPastEnd(size))
				return false;
			values = { (const T*)_reader.ReadBytes(size).Data(), len };
		}
		else
		{
			values = { nullptr, 0u };
		}
		return true;
	}
	template<typename T>
	bool SerializeChunk(TArrayView<const T>& values, size_t expected, bool exact)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		if (_reader.WouldReadPastEnd(sizeof(uint32_t)))
			return false;
		const uint32_t len = _reader.ReadValue<uint32_t>();
		if (expected && ((!exact && len > expected) || (exact && len != expected)))
			return false;
		const size_t size = len * sizeof(T);
		if (size)
		{
			if (_reader.WouldReadPastEnd(size))
				return false;
			values = { (const T*)_reader.ReadBytes(size).Data(), len };
		}
		else
		{
			values = { nullptr, 0u };
		}
		return true;
	}
};

// Useful for getting the size of a packet.
class MeasureStream
{
	ByteReader _reader;
public:
	enum { IsWriting = 0 };
	enum { IsReading = 0 };

	MeasureStream(const TArrayView<const uint8_t> stream) : _reader(stream) {}

	inline void Reset() { _reader.Reset(); }
	inline size_t GetSkippedBytes() const { return _reader.GetReadBytes(); }
	inline size_t Size() const { return _reader.Size(); }

	// Direct skipping API.
	void SkipString()
	{
		SkipArray<char>();
	}
	template<typename T>
	void SkipValue()
	{
		_reader.SkipBytes(sizeof(T));
	}
	template<typename T>
	void SkipArray()
	{
		_reader.SkipBytes(_reader.ReadValue<uint16_t>() * sizeof(T));
	}
	template<typename T>
	void SkipChunk()
	{
		_reader.SkipBytes(_reader.ReadValue<uint32_t>() * sizeof(T));
	}

	// Networking API.
	bool SerializeInt8(int8_t value)
	{
		_reader.SkipBytes(sizeof(int8_t));
		return true;
	}
	bool SerializeUInt8(uint8_t value)
	{
		_reader.SkipBytes(sizeof(uint8_t));
		return true;
	}
	bool SerializeInt16(int16_t value)
	{
		_reader.SkipBytes(sizeof(int16_t));
		return true;
	}
	bool SerializeUInt16(uint16_t value)
	{
		_reader.SkipBytes(sizeof(uint16_t));
		return true;
	}
	bool SerializeInt32(int32_t value)
	{
		_reader.SkipBytes(sizeof(int32_t));
		return true;
	}
	bool SerializeUInt32(uint32_t value)
	{
		_reader.SkipBytes(sizeof(uint32_t));
		return true;
	}
	bool SerializeInt64(int64_t value)
	{
		_reader.SkipBytes(sizeof(int64_t));
		return true;
	}
	bool SerializeUInt64(uint64_t value)
	{
		_reader.SkipBytes(sizeof(uint64_t));
		return true;
	}
	bool SerializeBool(bool value)
	{
		return SerializeUInt8(0u);
	}
	bool SerializeString(const FString& str)
	{
		return SerializeArray<char>({ nullptr, 0u }, 0u, false);
	}
	template<typename T>
	bool SerializeType(const T& value)
	{
		_reader.SkipBytes(sizeof(T));
		return true;
	}
	template<typename T>
	bool SerializeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		_reader.SkipBytes(_reader.ReadValue<uint16_t>() * sizeof(T));
		return true;
	}
	template<typename T>
	bool SerializeChunk(const TArrayView<const T> values, size_t expected, bool exact)
	{
		_reader.SkipBytes(_reader.ReadValue<uint32_t>() * sizeof(T));
		return true;
	}
};

// For quickly writing chunks of data off into dynamic buffers.
class DynamicWriteStream
{
	TArray<uint8_t> _array = {};
public:
	inline void Clear() { _array.Clear(); }
	inline void Reset() { _array.Reset(); }
	inline TArrayView<const uint8_t> GetView() const { return { Data(), Size() }; }
	inline size_t Size() const { return _array.Size(); }
	inline const uint8_t* Data() const { return _array.Data(); }

	void WriteBytes(const TArrayView<const uint8_t> bytes)
	{
		for (auto byte : bytes)
			_array.Push(byte);
	}
	void WriteString(const FString& value)
	{
		WriteArray<char>({ value.GetChars(), value.Len() });
	}
	template<typename T>
	void WriteValue(T value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		WriteBytes({ (uint8_t*)&T, sizeof(T) });
	}
	template<typename T>
	void WriteType(const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		WriteBytes({ (uint8_t*)&T, sizeof(T) });
	}
	template<typename T>
	void WriteArray(const TArrayView<const T> data)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint16_t size = data.Size();
		WriteValue<uint16_t>(size);
		WriteBytes({ (uint8_t*)data.Data(), size * sizeof(T) });
	}
	template<typename T>
	void WriteChunk(const TArrayView<const T> data)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint32_t size = data.Size();
		WriteValue<uint32_t>(size);
		WriteBytes({ (uint8_t*)data.Data(), size * sizeof(T) });
	}
};

// Storage to a static buffer of any size that the object itself has
// ownership of. Makes TArrayViews into dynamic arrays significantly safer.
class StaticReadBuffer
{
	size_t _curPos = 0u;
	TArray<uint8_t> _array = {};

	TArrayView<const uint8_t> InternalReadBytes(size_t bytes, bool skip)
	{
		if (_curPos + bytes >= Size())
			return { nullptr, 0u };

		const TArrayView<const uint8_t> data = { &_array[_curPos], bytes };
		if (skip)
			SkipBytes(bytes);
		return data;
	}
public:
	StaticReadBuffer() = default;
	StaticReadBuffer(const TArray<uint8_t>& array) : _array(array) {}
	StaticReadBuffer(const TArrayView<const uint8_t> data)
	{
		for (size_t i = 0u; i < data.Size(); ++i)
			_array.Push(data[i]);
	}

	inline void Reset() { _curPos = 0u; }
	inline size_t GetReadBytes() const { return _curPos; }
	inline TArrayView<const uint8_t> GetView() const { return { Data(), Size() }; }
	inline TArrayView<const uint8_t> GetReadData() const { return { Data(), _curPos }; }
	inline TArrayView<const uint8_t> GetRemainingData() const { return { &_array[_curPos], Size() - _curPos }; }
	inline size_t Size() const { return _array.Size(); }
	inline const uint8_t* Data() const { return _array.Data(); }

	void SkipBytes(size_t bytes)
	{
		assert(_curPos + bytes < Size());
		_curPos += bytes;
	}
	TArrayView<const uint8_t> ReadBytes(size_t bytes)
	{
		return InternalReadBytes(bytes, true);
	}
	void ReadString(FString& value)
	{
		const auto data = ReadArray<char>();
		value = FString(data.Data(), data.Size());
	}
	template<typename T>
	T ReadValue()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return *(T*)ReadBytes(sizeof(T)).Data();
	}
	template<typename T>
	void ReadType(T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		value = *(T*)ReadBytes(sizeof(T)).Data();
	}
	template<typename T>
	TArrayView<const T> ReadArray()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint16_t size = ReadValue<uint16_t>();
		return { (const T*)ReadBytes(size * sizeof(T)).Data(), size };
	}
	template<typename T>
	TArrayView<const T> ReadChunk()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint32_t size = ReadValue<uint32_t>();
		return { (const T*)ReadBytes(size * sizeof(T)).Data(), size };
	}
	TArrayView<const uint8_t> PeekBytes(size_t bytes)
	{
		return InternalReadBytes(bytes, false);
	}
	void PeekString(FString& value)
	{
		const auto data = PeekArray<char>();
		value = FString(data.Data(), data.Size());
	}
	template<typename T>
	T PeekValue()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return *(T*)PeekBytes(sizeof(T)).Data();
	}
	template<typename T>
	void PeekType(T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		value = *(T*)PeekBytes(sizeof(T)).Data();
	}
	template<typename T>
	TArrayView<const T> PeekArray()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint16_t size = PeekValue<uint16_t>();
		return { (const T*)(PeekBytes(sizeof(uint16_t) + size * sizeof(T)).Data() + sizeof(uint16_t)), size };
	}
	template<typename T>
	TArrayView<const T> PeekChunk()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const uint32_t size = PeekValue<uint32_t>();
		return { (const T*)(PeekBytes(sizeof(uint32_t) + size * sizeof(T)).Data() + sizeof(uint32_t)), size };
	}
};

#endif //__I_BYTESTREAM_H__
