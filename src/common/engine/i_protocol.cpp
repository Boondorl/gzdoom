/*
** i_protocol.cpp
** Basic network packet creation routines and simple IFF parsing
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

#include "i_protocol.h"
#include "i_net.h"

TMap<uint8_t, std::unique_ptr<NetPacket>(*)()> NetPacketFactory = {};

bool NetPacket::Read(ReadStream& stream)
{
	uint8_t type = 0u;
	if (!stream.SerializeUInt8(type) || type != NetCommand)
		return false;

	uint8_t argC = 0u;
	if (!stream.SerializeUInt8(argC) || argC != ArgCount)
		return false;

	size_t parsedArgs = 0u;
	if (!Serialize(stream, parsedArgs) || parsedArgs != ArgCount)
		return false;

	return true;
}

// Boon TODO: Probably template this
bool NetPacket::Write(WriteStream& stream)
{
	if (!ShouldWrite())
		return true;

	if (!stream.SerializeUInt8(NetCommand))
		return false;
	if (!stream.SerializeUInt8(ArgCount))
		return false;

	size_t parsedArgs = 0u;
	if (!Serialize(stream, parsedArgs) || parsedArgs != ArgCount)
		return false;

	return true;
}

bool NetPacket::Write(DynamicWriteStream& stream)
{
	if (!ShouldWrite())
		return true;

	if (!stream.SerializeUInt8(NetCommand))
		return false;
	if (!stream.SerializeUInt8(ArgCount))
		return false;

	size_t parsedArgs = 0u;
	if (!Serialize(stream, parsedArgs) || parsedArgs != ArgCount)
		return false;

	return true;
}

bool NetPacket::Skip(ReadStream& stream)
{
	MeasureStream m = { stream.GetRemainingData() };
	if (!m.SerializeUInt8(NetCommand))
		return false;
	if (!m.SerializeUInt8(ArgCount))
		return false;

	size_t parsedArgs = 0u;
	if (!Serialize(m, parsedArgs) || parsedArgs != ArgCount)
		return false;

	return stream.SkipBytes(m.GetSkippedBytes());
}

std::unique_ptr<NetPacket> CreatePacket(uint8_t type)
{
	auto packet = NetPacketFactory.CheckKey(type);
	if (packet == nullptr)
		I_Error("Unknown net command %u", type);

	auto p = (*packet)();
	if (p == nullptr)
		I_Error("Failed to create packet type %u", type);

	return p;
}

void ReadPacket(NetPacket& packet, ReadStream& stream, int pNum)
{
	if (!packet.Read(stream))
		I_Error("Failed to read net command %u", packet.NetCommand);

	if (pNum >= 0 && pNum < MaxClients && !packet.Execute(pNum))
		I_Error("Failed to execute net command %u", packet.NetCommand);
}

void WritePacket(NetPacket& packet, WriteStream& stream)
{
	if (!packet.Write(stream))
		I_Error("Failed to write net command %u", packet.NetCommand);
}

void WritePacket(NetPacket& packet, DynamicWriteStream& stream)
{
	if (!packet.Write(stream))
		I_Error("Failed to write net command %u", packet.NetCommand);
}

void SkipPacket(NetPacket& packet, ReadStream& stream)
{
	if (!packet.Skip(stream))
		I_Error("Malformed net command %u", packet.NetCommand);
}
