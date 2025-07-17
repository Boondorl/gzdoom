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
#include <limits>

// Byte streams.

class ByteWriter
{
	const TArrayView<uint8_t> _buffer = { nullptr, 0u };
	size_t _curPos = 0u;
	size_t _unwindPos = ~0u;
public:
	ByteWriter(const TArrayView<uint8_t> buffer) : _buffer(buffer) {}

	inline bool WouldWritePastEnd(size_t bytes) const { return _curPos + bytes >= Size(); }
	inline size_t GetWrittenBytes() const { return _curPos; }
	inline TArrayView<uint8_t> GetWrittenData() const { return { Data(), _curPos }; }
	inline TArrayView<uint8_t> GetRemainingData() const { return { &_buffer[_curPos], Size() - _curPos }; }
	inline TArrayView<uint8_t> GetUnwindData() const { return _unwindPos < _curPos ? TArrayView{ &_buffer[_unwindPos], _curPos - _unwindPos } : TArrayView<uint8_t>{ nullptr, 0u }; }
	inline uint8_t* Data() const { return _buffer.Data(); }
	inline size_t Size() const { return _buffer.Size(); }

	void Reset()
	{
		_curPos = 0u;
		_unwindPos = ~0u;
	}
	void SetUnwindPos()
	{
		_unwindPos = _curPos;
	}
	void Unwind()
	{
		if (_unwindPos < _curPos)
			_curPos = _unwindPos;
	}
	void ClearUnwindPos()
	{
		_unwindPos = ~0u;
	}
	void SkipBytes(size_t bytes)
	{
		assert(_curPos + bytes < Size());
		_curPos += bytes;
	}
	void WriteBytes(const TArrayView<const uint8_t> bytes)
	{
		if (bytes.Size())
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
	size_t _measurePos = 0u;

	TArrayView<const uint8_t> InternalReadBytes(size_t bytes) const
	{
		return bytes ? TArrayView{ &_buffer[_curPos], bytes } : TArrayView<const uint8_t>{ nullptr, 0u };
	}
public:
	ByteReader(const TArrayView<const uint8_t> buffer) : _buffer(buffer) {}

	inline bool WouldReadPastEnd(size_t bytes) const { return _curPos + bytes >= Size(); }
	inline size_t GetReadBytes() const { return _curPos; }
	inline TArrayView<const uint8_t> GetReadData() const { return { Data(), _curPos }; }
	inline TArrayView<const uint8_t> GetRemainingData() const { return { &_buffer[_curPos], Size() - _curPos }; }
	inline const uint8_t* Data() const { return _buffer.Data(); }
	inline size_t Size() const { return _buffer.Size(); }

	void StartMeasuring()
	{
		_measurePos = _curPos;
	}
	void ClearMeasurement()
	{
		_measurePos = 0u;
	}
	TArrayView<const uint8_t> GetMeasuredData() const
	{
		return InternalReadBytes(_curPos - _measurePos);
	}

	void Reset()
	{
		_curPos = _measurePos = 0u;
	}
	void SkipBytes(size_t bytes)
	{
		assert(_curPos + bytes < Size());
		_curPos += bytes;
	}
	TArrayView<const uint8_t> ReadBytes(size_t bytes)
	{
		const auto data = InternalReadBytes(bytes);
		SkipBytes(bytes);
		return data;
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
	TArrayView<const uint8_t> PeekBytes(size_t bytes) const
	{
		return InternalReadBytes(bytes);
	}
	template<typename T>
	T PeekValue() const
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return *(T*)PeekBytes(sizeof(T)).Data();
	}
	template<typename T>
	void PeekType(T& value) const
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

	template<typename T, typename Size>
	bool SerializeRange(const TArrayView<const T> values, size_t expected, bool exact)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const size_t len = values.Size();
		if (len > std::numeric_limits<Size>::max() || (expected && ((!exact && len > expected) || (exact && len != expected))))
			return false;
		const size_t size = len * sizeof(T);
		if (_writer.WouldWritePastEnd(sizeof(Size) + size))
			return false;
		const Size l = (Size)len;
		_writer.WriteValue<Size>(l);
		if (size)
		{
			const TArrayView<const uint8_t> bytes = { (const uint8_t*)values.Data(), size };
			_writer.WriteBytes(bytes);
		}
		return true;
	}
public:
	enum { IsWriting = 1 };
	enum { IsReading = 0 };

	WriteStream(const TArrayView<uint8_t> buffer) : _writer(buffer) {}

	inline void Reset() { _writer.Reset(); }
	inline size_t GetWrittenBytes() const { return _writer.GetWrittenBytes(); }
	inline TArrayView<uint8_t> GetWrittenData() const { return _writer.GetWrittenData(); }
	inline TArrayView<uint8_t> GetRemainingData() const { return _writer.GetRemainingData(); }
	inline TArrayView<uint8_t> GetUnwindData() const { return _writer.GetUnwindData(); }
	inline size_t Size() const { return _writer.Size(); }
	inline uint8_t* Data() const { return _writer.Data(); }

	void SetUnwindPos()
	{
		_writer.SetUnwindPos();
	}
	void Unwind()
	{
		_writer.Unwind();
	}
	void ClearUnwindPos()
	{
		_writer.ClearUnwindPos();
	}

	// This is mainly here since the manual readers also assume 64-bit unsigned string sizes.
	// This shouldn't be used for actual networking since strings at this size couldn't
	// be sent out anyway.
	bool WriteString(const FString& value)
	{
		return SerializeChunk<char>({ value.GetChars(), value.Len() }, 0u, false);
	}

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
	bool SerializeSmallArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint8_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint16_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeLargeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint32_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeChunk(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint64_t>(values, expected, exact);
	}
};

class ReadStream
{
	ByteReader _reader;

	template<typename T, typename Size>
	TArrayView<const T> ReadRange()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const Size len = _reader.ReadValue<Size>();
		return len ? { (T*)_reader.ReadBytes(len * sizeof(T)).Data(), len } : { nullptr, 0u };
	}
	template<typename T, typename Size>
	TArrayView<const T> PeekRange() const
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const Size len = _reader.PeekValue<Size>();
		return len ? { (T*)(_reader.PeekBytes(sizeof(Size) + len * sizeof(T)).Data() + sizeof(Size)), len } : { nullptr, 0u };
	}
	template<typename T, typename Size>
	bool SerializeRange(TArrayView<const T>& values, size_t expected, bool exact)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		if (_reader.WouldReadPastEnd(sizeof(Size)))
			return false;
		const Size len = _reader.ReadValue<Size>();
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
	inline void StartMeasuring() { _reader.StartMeasuring(); }
	inline void ClearMeasurement() { _reader.ClearMeasurement(); }
	inline TArrayView<const uint8_t> GetMeasuredData() const { return _reader.GetMeasuredData(); }

	bool SkipBytes(size_t bytes)
	{
		if (_reader.WouldReadPastEnd(bytes))
			return false;
		_reader.SkipBytes(bytes);
		return true;
	}

	// Direct read API. This has no safety checking so use with extreme caution.
	FString ReadString()
	{
		const auto data = ReadChunk<char>();
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
	TArrayView<const T> ReadSmallArray()
	{
		return ReadRange<T, uint8_t>();
	}
	template<typename T>
	TArrayView<const T> ReadArray()
	{
		return ReadRange<T, uint16_t>();
	}
	template<typename T>
	TArrayView<const T> ReadLargeArray()
	{
		return ReadRange<T, uint32_t>();
	}
	template<typename T>
	TArrayView<const T> ReadChunk()
	{
		return ReadRange<T, uint64_t>();
	}

	// Peeking API.
	FString PeekString() const
	{
		const auto data = PeekChunk<char>();
		return FString(data.Data(), data.Size());
	}
	template<typename T>
	T PeekValue() const
	{
		return _reader.PeekValue<T>();
	}
	template<typename T>
	void PeekType(T& value) const
	{
		_reader.PeekType<T>(value);
	}
	template<typename T>
	TArrayView<const T> PeekSmallArray() const
	{
		return PeekRange<T, uint8_t>();
	}
	template<typename T>
	TArrayView<const T> PeekArray() const
	{
		return PeekRange<T, uint16_t>();
	}
	template<typename T>
	TArrayView<const T> PeekLargeArray() const
	{
		return PeekRange<T, uint32_t>();
	}
	template<typename T>
	TArrayView<const T> PeekChunk() const
	{
		return PeekRange<T, uint64_t>();
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
	bool SerializeSmallArray(TArrayView<const T>& values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint8_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeArray(TArrayView<const T>& values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint16_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeLargeArray(TArrayView<const T>& values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint32_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeChunk(TArrayView<const T>& values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint64_t>(values, expected, exact);
	}
};

// Useful for getting the size of a packet.
class MeasureStream
{
	ByteReader _reader;

	template<typename T, typename Size>
	void SkipRange()
	{
		_reader.SkipBytes(_reader.ReadValue<Size>() * sizeof(T));
	}
	template<typename T, typename Size>
	bool SerializeRange(const TArrayView<const T> values, size_t expected, bool exact)
	{
		_reader.SkipBytes(_reader.ReadValue<Size>() * sizeof(T));
		return true;
	}
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
		SkipChunk<char>();
	}
	template<typename T>
	void SkipValue()
	{
		_reader.SkipBytes(sizeof(T));
	}
	template<typename T>
	void SkipSmallArray()
	{
		SkipRange<T, uint8_t>();
	}
	template<typename T>
	void SkipArray()
	{
		SkipRange<T, uint16_t>();
	}
	template<typename T>
	void SkipLargeArray()
	{
		SkipRange<T, uint32_t>();
	}
	template<typename T>
	void SkipChunk()
	{
		SkipRange<T, uint64_t>();
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
	bool SerializeSmallArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint8_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint16_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeLargeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint32_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeChunk(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint64_t>(values, expected, exact);
	}
};

// For quickly writing chunks of data off into dynamic buffers.
class DynamicWriteStream
{
	TArray<uint8_t> _array = {};
	size_t _unwindPos = ~0u;

	template<typename T, typename Size>
	void WriteRange(const TArrayView<const T> data)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const size_t len = data.Size();
		if (len > std::numeric_limits<Size>::max())
			return;
		Size l = (Size)len;
		WriteValue<Size>(l);
		if (l)
			WriteBytes({ (uint8_t*)data.Data(), l * sizeof(T) });
	}
	template<typename T, typename Size>
	bool SerializeRange(const TArrayView<const T> values, size_t expected, bool exact)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const size_t len = values.Size();
		if (len > std::numeric_limits<Size>::max() || (expected && ((!exact && len > expected) || (exact && len != expected))))
			return false;
		const size_t size = len * sizeof(T);
		const Size l = (Size)len;
		WriteValue<Size>(l);
		if (size)
		{
			const TArrayView<const uint8_t> bytes = { (const uint8_t*)values.Data(), size };
			WriteBytes(bytes);
		}
		return true;
	}
public:
	enum { IsWriting = 1 };
	enum { IsReading = 0 };

	inline void Clear() { _array.Clear(); _unwindPos = ~0u; }
	inline void Reset() { _array.Reset(); _unwindPos = ~0u; }
	inline TArrayView<const uint8_t> GetView() const { return { Data(), Size() }; }
	inline TArrayView<const uint8_t> GetUnwindData() const { return _unwindPos < Size() ? TArrayView<const uint8_t>{ &_array[_unwindPos], Size() - _unwindPos } : TArrayView<const uint8_t>{ nullptr, 0u }; }
	inline size_t Size() const { return _array.Size(); }
	inline const uint8_t* Data() const { return _array.Data(); }

	void SetUnwindPos()
	{
		_unwindPos = Size();
	}
	void Unwind()
	{
		if (_unwindPos < Size())
			_array.Resize(_unwindPos);
	}
	void ClearUnwindPos()
	{
		_unwindPos = ~0u;
	}

	void WriteBytes(const TArrayView<const uint8_t> bytes)
	{
		for (auto byte : bytes)
			_array.Push(byte);
	}
	void WriteString(const FString& value)
	{
		WriteChunk<char>({ value.GetChars(), value.Len() });
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
	void WriteSmallArray(const TArrayView<const T> data)
	{
		WriteRange<T, uint8_t>(data);
	}
	template<typename T>
	void WriteArray(const TArrayView<const T> data)
	{
		WriteRange<T, uint16_t>(data);
	}
	template<typename T>
	void WriteLargeArray(const TArrayView<const T> data)
	{
		WriteRange<T, uint32_t>(data);
	}
	template<typename T>
	void WriteChunk(const TArrayView<const T> data)
	{
		WriteRange<T, uint64_t>(data);
	}

	// Network API.
	bool SerializeInt8(int8_t value)
	{
		WriteValue<int8_t>(value);
		return true;
	}
	bool SerializeUInt8(uint8_t value)
	{
		WriteValue<uint8_t>(value);
		return true;
	}
	bool SerializeInt16(int16_t value)
	{
		WriteValue<int16_t>(value);
		return true;
	}
	bool SerializeUInt16(uint16_t value)
	{
		WriteValue<uint16_t>(value);
		return true;
	}
	bool SerializeInt32(int32_t value)
	{
		WriteValue<int32_t>(value);
		return true;
	}
	bool SerializeUInt32(uint32_t value)
	{
		WriteValue<uint32_t>(value);
		return true;
	}
	bool SerializeInt64(int64_t value)
	{
		WriteValue<int64_t>(value);
		return true;
	}
	bool SerializeUInt64(uint64_t value)
	{
		WriteValue<uint64_t>(value);
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
		WriteType<T>(value);
		return true;
	}
	template<typename T>
	bool SerializeSmallArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint8_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint16_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeLargeArray(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint32_t>(values, expected, exact);
	}
	template<typename T>
	bool SerializeChunk(const TArrayView<const T> values, size_t expected, bool exact)
	{
		return SerializeRange<T, uint64_t>(values, expected, exact);
	}
};

// Storage to a static buffer of any size that the object itself has
// ownership of. Makes TArrayViews into dynamic arrays significantly safer.
class StaticReadBuffer
{
	size_t _curPos = 0u;
	TArray<uint8_t> _array = {};

	template<typename T, typename Size>
	TArrayView<const T> ReadRange()
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const Size size = ReadValue<Size>();
		return size ? { (const T*)ReadBytes(size * sizeof(T)).Data(), size } : { nullptr, 0u };
	}
	template<typename T, typename Size>
	TArrayView<const T> PeekRange() const
	{
		static_assert(std::is_trivially_copyable_v<T>);
		const Size size = PeekValue<Size>();
		return size ? { (const T*)(PeekBytes(sizeof(Size) + size * sizeof(T)).Data() + sizeof(Size)), size } : { nullptr, 0u };
	}
	TArrayView<const uint8_t> InternalReadBytes(size_t bytes) const
	{
		if (!bytes || _curPos + bytes >= Size())
			return { nullptr, 0u };

		return { &_array[_curPos], bytes };
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
		const auto data = InternalReadBytes(bytes);
		SkipBytes(bytes);
		return data;
	}
	void ReadString(FString& value)
	{
		const auto data = ReadChunk<char>();
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
	TArrayView<const T> ReadSmallArray()
	{
		return ReadRange<T, uint8_t>();
	}
	template<typename T>
	TArrayView<const T> ReadArray()
	{
		return ReadRange<T, uint16_t>();
	}
	template<typename T>
	TArrayView<const T> ReadLargeArray()
	{
		return ReadRange<T, uint32_t>();
	}
	template<typename T>
	TArrayView<const T> ReadChunk()
	{
		return ReadRange<T, uint64_t>();
	}
	TArrayView<const uint8_t> PeekBytes(size_t bytes) const
	{
		return InternalReadBytes(bytes);
	}
	void PeekString(FString& value) const
	{
		const auto data = PeekChunk<char>();
		value = FString(data.Data(), data.Size());
	}
	template<typename T>
	T PeekValue() const
	{
		static_assert(std::is_trivially_copyable_v<T>);
		return *(T*)PeekBytes(sizeof(T)).Data();
	}
	template<typename T>
	void PeekType(T& value) const
	{
		static_assert(std::is_trivially_copyable_v<T>);
		value = *(T*)PeekBytes(sizeof(T)).Data();
	}
	template<typename T>
	TArrayView<const T> PeekSmallArray() const
	{
		return PeekRange<T, uint8_t>();
	}
	template<typename T>
	TArrayView<const T> PeekArray() const
	{
		return PeekRange<T, uint16_t>();
	}
	template<typename T>
	TArrayView<const T> PeekLargeArray() const
	{
		return PeekRange<T, uint32_t>();
	}
	template<typename T>
	TArrayView<const T> PeekChunk() const
	{
		return PeekRange<T, uint64_t>();
	}
};

#endif //__I_BYTESTREAM_H__
