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

bool NetPacket::Read(const TArrayView<const uint8_t>& stream, size_t& read)
{
	read = 0u;
	ReadStream r = { stream };
	uint8_t type = 0u;
	if (!r.SerializeUInt8(type) || type != NetCommand)
		return false;

	uint8_t argC = 0u;
	if (!r.SerializeUInt8(argC) || argC != ArgCount)
		return false;

	size_t parsedArgs = 0u;
	if (!Serialize(r, parsedArgs) || parsedArgs != ArgCount)
		return false;

	read = r.GetReadBytes();
	return true;
}

bool NetPacket::Write(const TArrayView<uint8_t>& stream, size_t& written)
{
	written = 0u;
	if (!ShouldWrite())
		return true;

	WriteStream w = { stream };
	if (!w.SerializeUInt8(NetCommand))
		return false;
	if (!w.SerializeUInt8(ArgCount))
		return false;

	size_t parsedArgs = 0u;
	if (!Serialize(w, parsedArgs) || parsedArgs != ArgCount)
		return false;

	written = w.GetWrittenBytes();
	return true;
}

bool NetPacket::Skip(const TArrayView<const uint8_t>& stream, size_t& skipped)
{
	skipped = 0u;
	MeasureStream m = { stream };
	if (!m.SerializeUInt8(NetCommand))
		return false;
	if (!m.SerializeUInt8(ArgCount))
		return false;

	size_t parsedArgs = 0u;
	if (!Serialize(m, parsedArgs) || parsedArgs != ArgCount)
		return false;

	skipped = m.GetSkippedBytes();
	return skipped <= stream.Size();
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

void ReadPacket(NetPacket& packet, TArrayView<const uint8_t>& stream, int pNum)
{
	size_t bytes = 0u;
	if (!packet.Read({ stream.Data(), stream.Size() }, bytes))
		I_Error("Failed to read net command %u", packet.NetCommand);

	assert(bytes <= stream.Size());
	stream = { stream.Data() + bytes, stream.Size() - bytes };

	if (pNum >= 0 && pNum < MaxClients && !packet.Execute(pNum))
		I_Error("Failed to execute net command %u", packet.NetCommand);
}

void WritePacket(NetPacket& packet, TArrayView<uint8_t>& stream)
{
	size_t bytes = 0u;
	if (!packet.Write(stream, bytes))
		I_Error("Failed to write net command %u", packet.NetCommand);

	assert(bytes <= stream.Size());
	stream = { stream.Data() + bytes, stream.Size() - bytes };
}

void SkipPacket(NetPacket& packet, TArrayView<const uint8_t>& stream)
{
	size_t bytes = 0u;
	if (!packet.Skip(stream, bytes))
		I_Error("Malformed net command %u", packet.NetCommand);

	stream = { stream.Data() + bytes, stream.Size() - bytes };
}
