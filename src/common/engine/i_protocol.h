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
			using Super = parentCls

#define DEFINE_NETPACKET_BASE_CONDITIONAL(parentCls)	\
		public:											\
			bool ShouldWrite() const override;			\
		DEFINE_NETPACKET_BASE(parentCls)

#define DEFINE_NETPACKET(cls, parentCls, id, argCount)		\
		public:												\
			static constexpr uint8_t Type = id;				\
			static cls GetDefault() { return {}; }			\
			bool Execute(int player) override;				\
		private:											\
			cls() : parentCls(id, argCount) {}				\
			using Super = parentCls
			

#define DEFINE_NETPACKET_CONDITIONAL(cls, parentCls, id, argCount)	\
		public:														\
			bool ShouldWrite() const override;						\
		DEFINE_NETPACKET(cls, parentCls, id, argCount)

// Easy setup macros for base packet types.
#define DEFINE_NETPACKET_EMPTY(cls,	id)		class cls : public EmptyPacket	{ DEFINE_NETPACKET(cls, EmptyPacket,	id, 0);	}
#define DEFINE_NETPACKET_INT8(cls,	id)		class cls : public Int8Packet	{ DEFINE_NETPACKET(cls, Int8Packet,		id, 1);	public: cls(int8_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT8(cls, id)		class cls : public UInt8Packet	{ DEFINE_NETPACKET(cls, UInt8Packet,	id, 1);	public: cls(uint8_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_BOOL(cls, id)		class cls : public BoolPacket	{ DEFINE_NETPACKET(cls, BoolPacket,		id, 1);	public: cls(bool value)				: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT16(cls, id)		class cls : public Int16Packet	{ DEFINE_NETPACKET(cls, Int16Packet,	id, 1);	public: cls(int16_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT16(cls, id)	class cls : public UInt16Packet	{ DEFINE_NETPACKET(cls, UInt16Packet,	id, 1);	public: cls(uint16_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT32(cls, id)		class cls : public Int32Packet	{ DEFINE_NETPACKET(cls, Int32Packet,	id, 1);	public: cls(int32_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT32(cls, id)	class cls : public UInt32Packet { DEFINE_NETPACKET(cls, UInt32Packet,	id, 1);	public: cls(uint32_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT64(cls, id)		class cls : public Int64Packet	{ DEFINE_NETPACKET(cls, Int64Packet,	id, 1);	public: cls(int64_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT64(cls, id)	class cls : public UInt64Packet { DEFINE_NETPACKET(cls, UInt64Packet,	id, 1);	public: cls(uint64_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_FLOAT(cls, id)		class cls : public FloatPacket	{ DEFINE_NETPACKET(cls, FloatPacket,	id, 1);	public: cls(float value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_DOUBLE(cls, id)	class cls : public DoublePacket { DEFINE_NETPACKET(cls, DoublePacket,	id, 1);	public: cls(double value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_STRING(cls, id)	class cls : public StringPacket { DEFINE_NETPACKET(cls, StringPacket,	id, 1);	public: cls(const FString& value)	: cls() { Value = value; } }

// Conditional variants.
#define DEFINE_NETPACKET_EMPTY_CONDITIONAL(cls,	id)		class cls : public EmptyPacket	{ DEFINE_NETPACKET_CONDITIONAL(cls, EmptyPacket,	id, 0);	}
#define DEFINE_NETPACKET_INT8_CONDITIONAL(cls,	id)		class cls : public Int8Packet	{ DEFINE_NETPACKET_CONDITIONAL(cls, Int8Packet,		id, 1);	public: cls(int8_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT8_CONDITIONAL(cls, id)		class cls : public UInt8Packet	{ DEFINE_NETPACKET_CONDITIONAL(cls, UInt8Packet,	id, 1);	public: cls(uint8_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_BOOL_CONDITIONAL(cls, id)		class cls : public BoolPacket	{ DEFINE_NETPACKET_CONDITIONAL(cls, BoolPacket,		id, 1);	public: cls(bool value)				: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT16_CONDITIONAL(cls, id)		class cls : public Int16Packet	{ DEFINE_NETPACKET_CONDITIONAL(cls, Int16Packet,	id, 1);	public: cls(int16_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT16_CONDITIONAL(cls, id)	class cls : public UInt16Packet	{ DEFINE_NETPACKET_CONDITIONAL(cls, UInt16Packet,	id, 1);	public: cls(uint16_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT32_CONDITIONAL(cls, id)		class cls : public Int32Packet	{ DEFINE_NETPACKET_CONDITIONAL(cls, Int32Packet,	id, 1);	public: cls(int32_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT32_CONDITIONAL(cls, id)	class cls : public UInt32Packet { DEFINE_NETPACKET_CONDITIONAL(cls, UInt32Packet,	id, 1);	public: cls(uint32_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_INT64_CONDITIONAL(cls, id)		class cls : public Int64Packet	{ DEFINE_NETPACKET_CONDITIONAL(cls, Int64Packet,	id, 1);	public: cls(int64_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_UINT64_CONDITIONAL(cls, id)	class cls : public UInt64Packet { DEFINE_NETPACKET_CONDITIONAL(cls, UInt64Packet,	id, 1);	public: cls(uint64_t value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_FLOAT_CONDITIONAL(cls, id)		class cls : public FloatPacket	{ DEFINE_NETPACKET_CONDITIONAL(cls, FloatPacket,	id, 1);	public: cls(float value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_DOUBLE_CONDITIONAL(cls, id)	class cls : public DoublePacket { DEFINE_NETPACKET_CONDITIONAL(cls, DoublePacket,	id, 1);	public: cls(double value)			: cls() { Value = value; } }
#define DEFINE_NETPACKET_STRING_CONDITIONAL(cls, id)	class cls : public StringPacket { DEFINE_NETPACKET_CONDITIONAL(cls, StringPacket,	id, 1);	public: cls(const FString& value)	: cls() { Value = value; } }

#define NETPACKET_EXECUTE(cls)			\
		bool cls::Execute(int player)

#define NETPACKET_CONDITION(cls)		\
		bool cls::ShouldWrite() const

#define NETPACKET_SERIALIZE()																													\
		protected:																																\
			bool Serialize(WriteStream& stream, size_t& argCount)	override { return SerializeInternal<WriteStream>(stream, argCount);		}	\
			bool Serialize(ReadStream& stream, size_t& argCount)	override { return SerializeInternal<ReadStream>(stream, argCount);		}	\
			bool Serialize(MeasureStream& stream, size_t& argCount)	override { return SerializeInternal<MeasureStream>(stream, argCount);	}	\
			template<typename Stream>																											\
			bool SerializeInternal(Stream& stream, size_t& argCount)

#define IF_WRITING()		if constexpr(Stream::IsWriting)
#define IF_READING()		if constexpr(Stream::IsReading)
#define SUPER_SERIALIZE()	if (!Super::Serialize(stream, argCount)) return false
#define SUPER_CONDITION()	if (!Super::ShouldWrite())		return false
#define SUPER_EXECUTE()		if (!Super::Execute(player))	return false

#define SERIALIZE_INT8(value)						\
		do											\
		{											\
			int8_t i8Value;							\
			IF_WRITING()							\
				i8Value = (int8_t)value;			\
			if (!stream.SerializeInt8(i8Value))		\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = i8Value;					\
		} while (0)

#define SERIALIZE_UINT8(value)						\
		do											\
		{											\
			uint8_t u8Value;						\
			IF_WRITING()							\
				u8Value = (uint8_t)value;			\
			if (!stream.SerializeUInt8(u8Value))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = u8Value;					\
		} while (0)

#define SERIALIZE_BOOL(value)					\
		do										\
		{										\
			bool bValue;						\
			IF_WRITING()						\
				bValue = (bool)value;			\
			if (!stream.SerializeBool(bValue))	\
				return false;					\
			++argCount;							\
			IF_READING()						\
				value = bValue;					\
		} while (0)

#define SERIALIZE_INT16(value)						\
		do											\
		{											\
			int16_t i16Value;						\
			IF_WRITING()							\
				i16Value = (int16_t)value;			\
			if (!stream.SerializeInt16(i16Value))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = i16Value;					\
		} while (0)

#define SERIALIZE_UINT16(value)						\
		do											\
		{											\
			uint16_t u16Value;						\
			IF_WRITING()							\
				u16Value = (uint16_t)value;			\
			if (!stream.SerializeUInt16(u16Value))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = u16Value;					\
		} while (0)

#define SERIALIZE_INT32(value)						\
		do											\
		{											\
			int32_t i32Value;						\
			IF_WRITING()							\
				i32Value = (int32_t)value;			\
			if (!stream.SerializeInt32(i32Value))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = i32Value;					\
		} while (0)

#define SERIALIZE_UINT32(value)						\
		do											\
		{											\
			uint32_t u32Value;						\
			IF_WRITING()							\
				u32Value = (uint32_t)value;			\
			if (!stream.SerializeUInt32(u32Value))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = u32Value;					\
		} while (0)

#define SERIALIZE_INT64(value)						\
		do											\
		{											\
			int64_t i64Value;						\
			IF_WRITING()							\
				i64Value = (int64_t)value;			\
			if (!stream.SerializeInt64(i64Value))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = i64Value;					\
		} while (0)

#define SERIALIZE_UINT64(value)						\
		do											\
		{											\
			uint64_t u64Value;						\
			IF_WRITING()							\
				u64Value = (uint64_t)value;			\
			if (!stream.SerializeUInt64(u64Value))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = u64Value;					\
		} while (0)

#define SERIALIZE_FLOAT(value)					\
		do										\
		{										\
			float fValue;						\
			IF_WRITING()						\
				fValue = (float)value;			\
			if (!stream.SerializeFloat(fValue))	\
				return false;					\
			++argCount;							\
			IF_READING()						\
				value = fValue;					\
		} while (0)

#define SERIALIZE_DOUBLE(value)						\
		do											\
		{											\
			double dValue;							\
			IF_WRITING()							\
				dValue = (double)value;				\
			if (!stream.SerializeDouble(dValue))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = dValue;						\
		} while (0)

#define SERIALIZE_STRING(value)						\
		do											\
		{											\
			FString sValue;							\
			IF_WRITING()							\
				sValue = value;						\
			if (!stream.SerializeString(sValue))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = sValue;						\
		} while (0)

// Indexed types. These have to be sent over by their
// name value since the indices aren't guaranteed to
// match between clients.
#define SERIALIZE_NAME(value)						\
		do											\
		{											\
			FString nValue;							\
			IF_WRITING()							\
				nValue = value.GetChars();			\
			if (!stream.SerializeString(nValue))	\
				return false;						\
			++argCount;								\
			IF_READING()							\
				value = nValue;						\
		} while (0)

#define SERIALIZE_TYPE(type, value)						\
		do												\
		{												\
			type tValue;								\
			IF_WRITING()								\
				tValue = (type)value;					\
			if (!stream.SerializeType<type>(tValue))	\
				return false;							\
			++argCount;									\
			IF_READING()								\
				value = tValue;							\
		} while (0)

// Bigger array sizes aren't supported at the moment since the NetBuffer is currently
// static and only supports up to 14kb.
#define SERIALIZE_SMALL_ARRAY_INTERNAL(type, value, len, exact)			\
		do																\
		{																\
			TArrayView<const type> rValue;								\
			IF_WRITING()												\
				rValue = value;											\
			if (!stream.SerializeSmallArray<type>(rValue, len, exact))	\
				return false;											\
			++argCount;													\
			IF_READING()												\
				value = rValue;											\
		} while (0)

#define SERIALIZE_ARRAY_INTERNAL(type, value, len, exact)			\
		do															\
		{															\
			TArrayView<const type> rValue;							\
			IF_WRITING()											\
				rValue = value;										\
			if (!stream.SerializeArray<type>(rValue, len, exact))	\
				return false;										\
			++argCount;												\
			IF_READING()											\
				value = rValue;										\
		} while (0)

#define SERIALIZE_SMALL_ARRAY_EXPECTING(value, len, exact)	SERIALIZE_SMALL_ARRAY_INTERNAL(decltype(value[0]), value, len, exact)
#define SERIALIZE_SMALL_ARRAY(value)						SERIALIZE_SMALL_ARRAY_EXPECTING(value, 0u, false)
#define SERIALIZE_ARRAY_EXPECTING(value, len, exact)		SERIALIZE_ARRAY_INTERNAL(decltype(value[0]), value, len, exact)
#define SERIALIZE_ARRAY(value)								SERIALIZE_ARRAY_EXPECTING(value, 0u, false)

#undef SERIALIZE_SMALL_ARRAY_INTERNAL
#undef SERIALIZE_ARRAY_INTERNAL

class NetPacket
{
protected:
	virtual bool Serialize(WriteStream& stream, size_t& argCount) = 0;
	virtual bool Serialize(ReadStream& stream, size_t& argCount) = 0;
	virtual bool Serialize(MeasureStream& stream, size_t& argCount) = 0;
public:
	static constexpr uint8_t Type = 0u;
	const uint8_t NetCommand = 0u;
	const uint8_t ArgCount = 0u;

	NetPacket(uint8_t netCommand, uint8_t argCount) : NetCommand(netCommand), ArgCount(argCount) {}
	virtual ~NetPacket() = default;
	virtual bool Execute(int player) = 0;
	virtual bool ShouldWrite() const { return true; }

	bool Read(const TArrayView<const uint8_t>& stream, size_t& read);
	bool Write(const TArrayView<uint8_t>& stream, size_t& written);
	bool Skip(const TArrayView<const uint8_t>& stream, size_t& skipped);
};

// Common packet types for easier defining.

class EmptyPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
	NETPACKET_SERIALIZE()
	{
		return true;
	}
};
class Int8Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	int8_t Value = 0;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_INT8(Value);
		return true;
	}
};
class UInt8Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	uint8_t Value = 0u;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_UINT8(Value);
		return true;
	}
};
class BoolPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	bool Value = false;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_BOOL(Value);
		return true;
	}
};
class Int16Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	int16_t Value = 0;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_INT16(Value);
		return true;
	}
};
class UInt16Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	uint16_t Value = 0u;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_UINT16(Value);
		return true;
	}
};
class Int32Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	int32_t Value = 0;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_INT32(Value);
		return true;
	}
};
class UInt32Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	uint32_t Value = 0u;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_UINT32(Value);
		return true;
	}
};
class Int64Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	int64_t Value = 0;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_INT64(Value);
		return true;
	}
};
class UInt64Packet : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	uint64_t Value = 0u;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_UINT64(Value);
		return true;
	}
};
class FloatPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	float Value = 0.0f;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_FLOAT(Value);
		return true;
	}
};
class DoublePacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	double Value = 0.0;
	NETPACKET_SERIALIZE()
	{
		SERIALIZE_DOUBLE(Value);
		return true;
	}
};
class StringPacket : public NetPacket
{
	DEFINE_NETPACKET_BASE(NetPacket);
protected:
	FString Value = {};
	NETPACKET_SERIALIZE()
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
