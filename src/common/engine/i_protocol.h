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

#include "i_bytestream.h"
#include "engineerrors.h"

#define REGISTER_NETPACKET(cls)																						\
		if (NetPacketFactory.CheckKey(cls::Type) != nullptr)														\
			I_Error("Attempted to override existing net command %u", cls::Type);									\
		NetPacketFactory.Insert(cls::Type, [](){ return std::unique_ptr<NetPacket>(new cls(cls::GetDefault())); })

#define DEFINE_NETPACKET_BASE(parentCls)		\
		protected:								\
			using parentCls::parentCls;			\
		private:								\
			using Super = parentCls;

#define DEFINE_NETPACKET(cls, parentCls, id, argCount)		\
		public:												\
			static constexpr uint8_t Type = id;				\
			static cls GetDefault() { return {}; }			\
			bool Execute(int player) override;				\
		private:											\
			using Super = parentCls;						\
			cls() : parentCls(id, argCount) {}

// Easy setup macros for base packet types.
#define DEFINE_NETPACKET_EMPTY(cls,	id)		class cls : public EmptyPacket	{ DEFINE_NETPACKET(cls, EmptyPacket,	id, 0)	}
#define DEFINE_NETPACKET_INT8(cls,	id)		class cls : public Int8Packet	{ DEFINE_NETPACKET(cls, Int8Packet,		id, 1)	public: cls(int8_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT8(cls, id)		class cls : public UInt8Packet	{ DEFINE_NETPACKET(cls, UInt8Packet,	id, 1)	public: cls(uint8_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT16(cls, id)		class cls : public Int16Packet	{ DEFINE_NETPACKET(cls, Int16Packet,	id, 1)	public: cls(int16_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT16(cls, id)	class cls : public UInt16Packet	{ DEFINE_NETPACKET(cls, UInt16Packet,	id, 1)	public: cls(uint16_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT32(cls, id)		class cls : public Int32Packet	{ DEFINE_NETPACKET(cls, Int32Packet,	id, 1)	public: cls(int32_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT32(cls, id)	class cls : public UInt32Packet { DEFINE_NETPACKET(cls, UInt32Packet,	id, 1)	public: cls(uint32_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT64(cls, id)		class cls : public Int64Packet	{ DEFINE_NETPACKET(cls, Int64Packet,	id, 1)	public: cls(int64_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT64(cls, id)	class cls : public UInt64Packet { DEFINE_NETPACKET(cls, UInt64Packet,	id, 1)	public: cls(uint64_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_FLOAT(cls, id)		class cls : public FloatPacket	{ DEFINE_NETPACKET(cls, FloatPacket,	id, 1)	public: cls(float value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_DOUBLE(cls, id)	class cls : public DoublePacket { DEFINE_NETPACKET(cls, DoublePacket,	id, 1)	public: cls(double value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_STRING(cls, id)	class cls : public StringPacket { DEFINE_NETPACKET(cls, StringPacket,	id, 1)	public: cls(const FString& value)	: cls() { Value = value; } }

#define NETPACKET_EXECUTE(cls)			\
		bool cls::Execute(int player)

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

#define SERIALIZE_BOOL(value)					\
		do										\
		{										\
			bool bValue;						\
			if (Stream::IsWriting)				\
				bValue = (bool)value;			\
			if (!stream.SerializeBool(bValue))	\
				return false;					\
			++argCount;							\
			if (Stream::IsReading)				\
				value = bValue;					\
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

#define SERIALIZE_FLOAT(value)					\
		do										\
		{										\
			float fValue;						\
			if (Stream::IsWriting)				\
				fValue = (float)value;			\
			if (!stream.SerializeFloat(fValue))	\
				return false;					\
			++argCount;							\
			if (Stream::IsReading)				\
				value = fValue;					\
		} while (0)

#define SERIALIZE_DOUBLE(value)						\
		do											\
		{											\
			double dValue;							\
			if (Stream::IsWriting)					\
				dValue = (double)value;				\
			if (!stream.SerializeDouble(dValue))	\
				return false;						\
			++argCount;								\
			if (Stream::IsReading)					\
				value = dValue;						\
		} while (0)

#define SERIALIZE_STRING(value)					\
		do										\
		{										\
			FString str;						\
			if (Stream::Writing)				\
				str = value;					\
			if (!stream.SerializeString(str))	\
				return false;					\
			++argCount;							\
			if (Stream::Reading)				\
				value = str;					\
		} while (0)

// Boon TODO: This definitely needs to be fixed lol
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

#define SERIALIZE_TYPE(type, value)						\
		do												\
		{												\
			type tValue;								\
			if (Stream::IsWriting)						\
				tValue = (type)value;					\
			if (!stream.SerializeType<type>(tValue))	\
				return false;							\
			++argCount;									\
			if (Stream::IsReading)						\
				value = tValue;							\
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
	const uint8_t NetCommand = 0u;
	const uint8_t ArgCount = 0u;

	NetPacket(uint8_t netCommand, uint8_t argCount) : NetCommand(netCommand), ArgCount(argCount) {}
	virtual ~NetPacket() = default;
	virtual bool ShouldWrite() const
	{
		return true;
	}
	virtual bool Execute(int player)
	{
		return false;
	}
	bool Read(const TArrayView<const uint8_t>& stream, size_t& read);
	bool Write(const TArrayView<uint8_t>& stream, size_t& written);
	bool Skip(const TArrayView<const uint8_t>& stream, size_t& skipped);
};

// Common packet types for easier defining.

class EmptyPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
	DEFINE_NETPACKET_SERIALIZER()
	{
		return true;
	}
};
class Int8Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	int8_t Value = 0;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_INT8(Value);
		return true;
	}
};
class UInt8Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	uint8_t Value = 0u;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_UINT8(Value);
		return true;
	}
};
class Int16Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	int16_t Value = 0;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_INT16(Value);
		return true;
	}
};
class UInt16Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	uint16_t Value = 0u;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_UINT16(Value);
		return true;
	}
};
class Int32Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	int32_t Value = 0;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_INT32(Value);
		return true;
	}
};
class UInt32Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	uint32_t Value = 0u;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_UINT32(Value);
		return true;
	}
};
class Int64Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	int64_t Value = 0;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_INT64(Value);
		return true;
	}
};
class UInt64Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	uint64_t Value = 0u;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_UINT64(Value);
		return true;
	}
};
class FloatPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	float Value = 0.0f;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_FLOAT(Value);
		return true;
	}
};
class DoublePacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	double Value = 0.0;
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_DOUBLE(Value);
		return true;
	}
};
class StringPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket)
protected:
	FString Value = {};
	DEFINE_NETPACKET_SERIALIZER()
	{
		SERIALIZE_STRING(Value);
		return true;
	}
};

extern TMap<uint8_t, std::unique_ptr<NetPacket>(*)()> NetPacketFactory;

std::unique_ptr<NetPacket> CreatePacket(uint8_t type);
void ReadPacket(NetPacket& packet, TArrayView<const uint8_t>& stream, int pNum = -1);
void WritePacket(NetPacket& packet, TArrayView<uint8_t>& stream);
void SkipPacket(NetPacket& packet, TArrayView<const uint8_t>& stream);

#endif //__I_PROTOCOL_H__
