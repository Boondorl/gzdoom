/*
** i_protocol.h
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

#ifndef __I_PROTOCOL_H__
#define __I_PROTOCOL_H__

#include "tarray.h"
#include "zstring.h"

class ByteWriter
{
	const TArrayView<uint8_t> _buffer = { nullptr, 0u };
	size_t _curPos = 0u;
public:
	ByteWriter(const TArrayView<uint8_t>& buffer) : _buffer(buffer) {}

	inline bool WouldWritePastEnd(size_t bytes) const { return _curPos + bytes >= _buffer.Size(); }
	inline size_t GetWrittenBytes() const { return _curPos; }

	void WriteBytes(const TArrayView<const uint8_t>& bytes)
	{
		assert(_curPos + bytes.Size() < _buffer.Size());
		memcpy(&_buffer[_curPos], bytes.Data(), bytes.Size());
		_curPos += bytes.Size();
	}
	template<typename T>
	void WriteValue(T value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		WriteBytes({ (uint8_t*)&value, sizeof(T) });
	}
	template<typename T>
	void WriteType(const T& value)
	{
		static_assert(std::is_trivially_copyable_v<T>);
		WriteBytes({ (uint8_t*)&value, sizeof(T) });
	}
};

class ByteReader
{
	const TArrayView<const uint8_t> _buffer = { nullptr, 0u };
	size_t _curPos = 0u;
public:
	ByteReader(const TArrayView<const uint8_t>& buffer) : _buffer(buffer) {}

	inline bool WouldReadPastEnd(size_t bytes) const { return _curPos + bytes >= _buffer.Size(); }
	inline size_t GetReadBytes() const { return _curPos; }

	TArrayView<const uint8_t> ReadBytes(size_t bytes)
	{
		assert(_curPos + bytes < _buffer.Size());
		TArrayView<const uint8_t> view = { &_buffer[_curPos], bytes };
		_curPos += bytes;
		return view;
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
};

class WriteStream
{
	ByteWriter _writer;
public:
	enum { IsWriting = 1 };
	enum { IsReading = 0 };

	WriteStream(const TArrayView<uint8_t>& buffer) : _writer(buffer) {}

	inline size_t GetWrittenBytes() const { return _writer.GetWrittenBytes(); }

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
	template<typename T>
	bool SerializeType(const T& value)
	{
		if (_writer.WouldWritePastEnd(sizeof(T)))
			return false;
		_writer.WriteType<T>(value);
		return true;
	}
	template<typename T>
	bool SerializeArray(const TArrayView<const T>& values, size_t expected, bool exact)
	{
		const size_t len = values.Size();
		if (expected && ((!exact && len > expected) || (exact && len != expected)))
			return false;
		const size_t size = len * sizeof(T);
		if (_writer.WouldWritePastEnd((sizeof(uint16_t) + size)))
			return false;
		const uint16_t l = (uint16_t)len;
		const TArrayView<const uint8_t> values = { (const uint8_t*)values.Data(), size };
		_writer.WriteValue<uint16_t>(l);
		_writer.WriteBytes(values);
		return true;
	}
};

class ReadStream
{
	ByteReader _reader;
public:
	enum { IsWriting = 0 };
	enum { IsReading = 1 };

	ReadStream(const TArrayView<const uint8_t>& buffer) : _reader(buffer) {}

	inline size_t GetReadBytes() const { return _reader.GetReadBytes(); }

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
		if (_reader.WouldReadPastEnd(sizeof(uint16_t)))
			return false;
		const uint16_t len = _reader.ReadValue<uint16_t>();
		if (expected && ((!exact && len > expected) || (exact && len != expected)))
			return false;
		const size_t size = len * sizeof(T);
		if (_reader.WouldReadPastEnd(size))
			return false;
		values = { (const T*)_reader.ReadBytes(size).Data(), len };
		return true;
	}
};

// Useful for getting the size of a packet.
class MeasureStream
{
	size_t _totalBytes = 0;
public:
	enum { IsWriting = 0 };
	enum { IsReading = 0 };

	inline size_t GetTotalBytes() const { return _totalBytes; }

	bool SerializeInt8(int8_t value)
	{
		_totalBytes += 1u;
		return true;
	}
	bool SerializeUInt8(uint8_t value)
	{
		_totalBytes += 1u;
		return true;
	}
	bool SerializeInt16(int16_t value)
	{
		_totalBytes += 2u;
		return true;
	}
	bool SerializeUInt16(uint16_t value)
	{
		_totalBytes += 2u;
		return true;
	}
	bool SerializeInt32(int32_t value)
	{
		_totalBytes += 4u;
		return true;
	}
	bool SerializeUInt32(uint32_t value)
	{
		_totalBytes += 4u;
		return true;
	}
	bool SerializeInt64(int64_t value)
	{
		_totalBytes += 8u;
		return true;
	}
	bool SerializeUInt64(uint64_t value)
	{
		_totalBytes += 8u;
		return true;
	}
	template<typename T>
	bool SerializeType(const T& value)
	{
		_totalBytes += sizeof(T);
		return true;
	}
	template<typename T>
	bool SerializeArray(const TArrayView<const T>& values, size_t expected, bool exact)
	{
		_totalBytes += sizeof(uint16_t) + values.Size() * sizeof(T);
		return true;
	}
};

#define REGISTER_NETPACKET(cls)																						\
		NetPacketFactory.Insert(cls::Type, [](){ return std::unique_ptr<NetPacket>(new cls(cls::GetReader())); })

#define DEFINE_NETPACKET_BASE(parentCls)		\
		protected:								\
			using parentCls::parentCls;			\
		private:								\
			using Super = parentCls;

#define DEFINE_NETPACKET(cls, parentCls, id, argCount)		\
		public:												\
			static constexpr uint8_t Type = id;				\
			static cls GetReader() { return {}; }			\
		private:											\
			using Super = parentCls;						\
			cls() : parentCls(id, argCount) {}

#define DEFINE_NETPACKET_SERIALIZER()																										\
		protected:																															\
			virtual bool Serialize(WriteStream& stream, size_t& argCount) { return SerializeInternal<WriteStream>(stream, argCount); }		\
			virtual bool Serialize(ReadStream& stream, size_t& argCount) { return SerializeInternal<ReadStream>(stream, argCount); }		\
			virtual bool Serialize(MeasureStream& stream, size_t& argCount) { return SerializeInternal<MeasureStream>(stream, argCount); }	\
			template<typename Stream>																										\
			bool SerializeInternal(Stream& stream, size_t& argCount)

#define SERIALIZE_INT8(value)						\
		do											\
		{											\
			int8_t i8Value;							\
			if (Stream::IsWriting)					\
				i8Value = (int8_t)value;			\
			if (!stream.SerializeInt8(i8Value))		\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = i8Value;					\
		} while (0)

#define SERIALIZE_UINT8(value)						\
		do											\
		{											\
			uint8_t u8Value;						\
			if (Stream::IsWriting)					\
				u8Value = (uint8_t)value;			\
			if (!stream.SerializeUInt8(u8Value))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = u8Value;					\
		} while (0)

#define SERIALIZE_INT16(value)						\
		do											\
		{											\
			int16_t i16Value;						\
			if (Stream::IsWriting)					\
				i16Value = (int16_t)value;			\
			if (!stream.SerializeInt16(i16Value))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = i16Value;					\
		} while (0)

#define SERIALIZE_UINT16(value)						\
		do											\
		{											\
			uint16_t u16Value;						\
			if (Stream::IsWriting)					\
				u16Value = (uint16_t)value;			\
			if (!stream.SerializeUInt16(u16Value))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = u16Value;					\
		} while (0)

#define SERIALIZE_INT32(value)						\
		do											\
		{											\
			int32_t i32Value;						\
			if (Stream::IsWriting)					\
				i32Value = (int32_t)value;			\
			if (!stream.SerializeInt32(i32Value))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = i32Value;					\
		} while (0)

#define SERIALIZE_UINT32(value)						\
		do											\
		{											\
			uint32_t u32Value;						\
			if (Stream::IsWriting)					\
				u32Value = (uint32_t)value;			\
			if (!stream.SerializeUInt32(u32Value))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = u32Value;					\
		} while (0)

#define SERIALIZE_INT64(value)						\
		do											\
		{											\
			int64_t i64Value;						\
			if (Stream::IsWriting)					\
				i64Value = (int64_t)value;			\
			if (!stream.SerializeInt64(i64Value))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = i64Value;					\
		} while (0)

#define SERIALIZE_UINT64(value)						\
		do											\
		{											\
			uint64_t u64Value;						\
			if (Stream::IsWriting)					\
				u64Value = (uint64_t)value;			\
			if (!stream.SerializeUInt64(u64Value))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = u64Value;					\
		} while (0)

#define SERIALIZE_FLOAT(value)						\
		do											\
		{											\
			union {									\
				float f;							\
				uint32_t u;							\
			} temp;									\
			if (Stream::IsWriting)					\
				temp.f = (float)value;				\
			if (!stream.SerializeUInt32(temp.u))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = temp.f;						\
		} while (0)

#define SERIALIZE_DOUBLE(value)						\
		do											\
		{											\
			union {									\
				double d;							\
				uint64_t u;							\
			} temp;									\
			if (Stream::IsWriting)					\
				temp.d = (double)value;				\
			if (!stream.SerializeUInt64(temp.u))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = temp.d;						\
		} while (0)

#define SERIALIZE_STRING(value)									\
		do														\
		{														\
			TArrayView<const char> str;							\
			if (Stream::Writing)								\
				str = { value.GetChars(), value.Len() };		\
			if (!stream.SerializeArray<char>(str, 0u, false))	\
				return false;									\
			++argCount;											\
			if (Stream::Reading)								\
				value = FString(str.Data(), str.Size());		\
		} while (0)

#define SERIALIZE_NAME(value)									\
		do														\
		{														\
			TArrayView<const char> str;							\
			if (Stream::Writing)								\
			{													\
				FString s = value.GetChars();					\
				str = { s.GetChars(), s.Len() };				\
			}													\
			if (!stream.SerializeArray<char>(str, 0u, false))	\
				return false;									\
			++argCount;											\
			if (Stream::Reading)								\
				value = FString(str.Data(), str.Size());		\
		} while (0)

#define SERIALIZE_TYPE(type, value)					\
		do											\
		{											\
			if (!stream.SerializeType<type>(value))	\
				return false;						\
			++argCount;								\
		} while (0)

#define SERIALIZE_ARRAY(type, data)								\
		do														\
		{														\
			TArrayView<const type> arr;							\
			if (Stream::IsWriting)								\
				arr = data;										\
			if (!stream.SerializeArray<type>(arr, 0u, false))	\
				return false;									\
			++argCount;											\
			if (Stream::IsReading)								\
				data = arr;										\
		} while (0)

#define SERIALIZE_ARRAY_EXPECTING(type, data, len, exact)			\
		do															\
		{															\
			TArrayView<const type> arr;								\
			if (Stream::IsWriting)									\
				arr = data;											\
			if (!stream.SerializeArray<type>(arr, len, exact))		\
				return false;										\
			++argCount;												\
			if (Stream::IsReading)									\
				data = arr;											\
		} while (0)

class NetPacket
{
	DEFINE_NETPACKET_SERIALIZER()
	{
		return false;
	}
public:
	static constexpr uint8_t Type = 0u;
	const uint8_t ArgCount = 0u;
	const uint8_t NetCommand = 0u;

	NetPacket(uint8_t netCommand, uint8_t argCount) : NetCommand(netCommand), ArgCount(argCount) {}
	virtual ~NetPacket() = default;
	virtual bool ShouldWrite() const
	{
		return true;
	}
	virtual bool Execute(int pNum)
	{
		return false;
	}
	bool Read(const TArrayView<const uint8_t>& stream, size_t& read);
	bool Write(const TArrayView<uint8_t>& stream, size_t& written);
	size_t GetSize();
};

class EmptyPacket;
class Int8Packet;
class UInt8Packet;
class Int16Packet;
class UInt16Packet;
class Int32Packet;
class UInt32Packet;
class Int64Packet;
class UInt64Packet;
class FloatPacket;
class DoublePacket;
class StringPacket;

extern TMap<uint8_t, std::unique_ptr<NetPacket>(*)()> NetPacketFactory;

std::unique_ptr<NetPacket> CreatePacket(uint8_t type);
void ReadPacket(NetPacket& packet, TArrayView<const uint8_t>& stream, int pNum = -1);
void WritePacket(NetPacket& packet, TArrayView<uint8_t>& stream);
void SkipPacket(NetPacket& packet, TArrayView<const uint8_t>& stream);

#endif //__I_PROTOCOL_H__
