#include "tsTransportStream.h"
#include <iostream>
#include <cstring>  // Dla memcpy

// xTS_PacketHeader  -----------------------------------------------------------------------

void xTS_PacketHeader::Reset() {
  m_SB = 0;
  m_E = 0;
  m_S = 0; 
  m_P = 0;
  m_PID = 0;
  m_TSC = 0;
  m_AFC = 0;
  m_CC = 0;
}

int32_t xTS_PacketHeader::Parse(const uint8_t* Input) {

  m_SB = Input[0];
  if (m_SB != 0x47) {
     return -1;
  }

  m_E   = (Input[1] >> 7) & 0x01;
  m_S   = (Input[1] >> 6) & 0x01;
  m_P   = (Input[1] >> 5) & 0x01;
  m_PID = ((uint16_t)(Input[1] & 0x1F) << 8) | Input[2];
  
  m_TSC = (Input[3] >> 6) & 0x03;
  m_AFC = (Input[3] >> 4) & 0x03;
  m_CC  = Input[3] & 0x0F;
  
  return 0;
}

void xTS_PacketHeader::Print() const {
  printf("TS: SB=%02X E=%d S=%d P=%d PID=%04X (%5d) TSC=%d AF=%d CC=%2d", 
    m_SB, m_E, m_S, m_P, m_PID, m_PID, m_TSC, m_AFC, m_CC);
}

// xTS_AdaptationField Implementation ----------------------------------------------------------------

void xTS_AdaptationField::Reset() {
  m_AdaptationFieldLength = 0;
  m_DC = 0; 
  m_RA = 0; 
  m_SP = 0;
  m_PR = 0; 
  m_OR = 0; 
  m_SF = 0; 
  m_TP = 0; 
  m_EX = 0;
}


int32_t xTS_AdaptationField::Parse(const uint8_t* PacketBuffer_Base, uint8_t AdaptationFieldControl_AFC_Value) {
  if (AdaptationFieldControl_AFC_Value == 0x02 || AdaptationFieldControl_AFC_Value == 0x03) { // AF obecne
    
    const uint8_t* afStartPtr = PacketBuffer_Base + xTS::TS_HeaderLength;
    m_AdaptationFieldLength = afStartPtr[0]; 

    if (m_AdaptationFieldLength > 0) { 
        const uint8_t* flagsPtr = afStartPtr + 1; 
        uint8_t flagsByte = *flagsPtr;
        m_DC = (flagsByte >> 7) & 0x01;
        m_RA = (flagsByte >> 6) & 0x01;
        m_SP = (flagsByte >> 5) & 0x01;
        m_PR = (flagsByte >> 4) & 0x01;
        m_OR = (flagsByte >> 3) & 0x01;
        m_SF = (flagsByte >> 2) & 0x01;
        m_TP = (flagsByte >> 1) & 0x01;
        m_EX = (flagsByte >> 0) & 0x01;
    } else {}
  } else {
      m_AdaptationFieldLength = 0; // Brak AF
  }
  return 0;
}

void xTS_AdaptationField::Print() const {
  if (m_AdaptationFieldLength > 0) { 
    printf(" AF: L=%d", m_AdaptationFieldLength);
    printf(" DC=%d RA=%d SP=%d PR=%d OR=%d SF=%d TP=%d EX=%d",
           m_DC, m_RA, m_SP, m_PR, 
           m_OR, m_SF, m_TP, m_EX);
  } else if (m_AdaptationFieldLength == 0 && ( (reinterpret_cast<const xTS_PacketHeader*>(this-sizeof(xTS_PacketHeader)))->getAFC() == 2 || (reinterpret_cast<const xTS_PacketHeader*>(this-sizeof(xTS_PacketHeader)))->getAFC() == 3) ) {}
}

//  xPES_PacketHeader Implementation -------------------------------------------
void xPES_PacketHeader::Reset(){
  m_PacketStartCodePrefix = 0;
  m_StreamId = 0;
  m_PacketLength = 0;
  m_OptionalPESHeaderLengthVal = 0;
  m_ActualFullHeaderLength = 0;
}


int32_t xPES_PacketHeader::Parse(const uint8_t* pesHeaderStartPtr){
  m_PacketStartCodePrefix = ((uint32_t)pesHeaderStartPtr[0] << 16) | 
                            ((uint32_t)pesHeaderStartPtr[1] << 8)  | 
                             (uint32_t)pesHeaderStartPtr[2];
  
  if (m_PacketStartCodePrefix != 0x000001) {
    //fprintf(stderr, "Invalid PES PSCP: %X at offset\n", m_PacketStartCodePrefix);
    return -1; 
  }

  m_StreamId = pesHeaderStartPtr[3];
  m_PacketLength = ((uint16_t)pesHeaderStartPtr[4] << 8) | (uint16_t)pesHeaderStartPtr[5];

  if (m_StreamId == eStreamId_program_stream_map ||
      m_StreamId == eStreamId_padding_stream ||
      m_StreamId == eStreamId_private_stream_2 ||
      m_StreamId == eStreamId_ECM ||
      m_StreamId == eStreamId_EMM ||
      m_StreamId == eStreamId_program_stream_directory ||
      m_StreamId == eStreamId_DSMCC_stream ||
      m_StreamId == eStreamId_ITUT_H222_1_type_E ||
      m_StreamId == 0xBD 
     ) {
    if ((pesHeaderStartPtr[6] & 0xF0) == 0x20 || (pesHeaderStartPtr[6] & 0xF0) == 0x30 ) { 
         m_OptionalPESHeaderLengthVal = 0;
         m_ActualFullHeaderLength = 6;
    } else if ((pesHeaderStartPtr[6] & 0xC0) != 0x80 && m_StreamId != 0xFD /* Extension Stream ID */) { 
        m_OptionalPESHeaderLengthVal = 0;
        m_ActualFullHeaderLength = 6;
    } else {
        m_OptionalPESHeaderLengthVal = pesHeaderStartPtr[8];
        m_ActualFullHeaderLength = 6 + 3 + m_OptionalPESHeaderLengthVal;
    }

  } else {
    m_OptionalPESHeaderLengthVal = pesHeaderStartPtr[8]; 
    m_ActualFullHeaderLength = 6 + 3 + m_OptionalPESHeaderLengthVal;
  }
  return m_ActualFullHeaderLength;
}

uint32_t xPES_PacketHeader::getElementaryStreamPayloadLength() const {
    if (m_PacketLength == 0) { return 0xFFFFFFFF; }
    
    uint32_t optionalHeaderAndFlagsBytes = 3 + m_OptionalPESHeaderLengthVal;
    if (m_ActualFullHeaderLength <= 6) { optionalHeaderAndFlagsBytes = 0; }

    if (m_PacketLength < optionalHeaderAndFlagsBytes) { return 0; }
    return m_PacketLength - optionalHeaderAndFlagsBytes;
}

void xPES_PacketHeader::Print() const {
  printf(" PES: PSCP=%06X SID=%02X (%3d) L=%5d", m_PacketStartCodePrefix, m_StreamId, m_StreamId, m_PacketLength);
  if (m_ActualFullHeaderLength > 6) { 
    printf(" OptHdrLenVal=%d FullHdrLen=%d ESPayloadLenEst=%d", m_OptionalPESHeaderLengthVal, m_ActualFullHeaderLength, getElementaryStreamPayloadLength());
  }
}

//  xPES_Assembler Implementation ------------------------------------------
xPES_Assembler::xPES_Assembler(){
  m_PID = -1; 
  m_Buffer = nullptr;
  m_BufferSize = 0;
  m_DataOffset = 0;
  m_LastContinuityCounter = -1; 
  m_Started = false;
  m_TargetPayloadBytes = 0;
}

xPES_Assembler::~xPES_Assembler(){
  if (m_OutputFile.is_open()) {
    if (m_Started && m_DataOffset > 0) { 
        if (m_TargetPayloadBytes == 0xFFFFFFFF) { 
            m_OutputFile.write(reinterpret_cast<const char*>(m_Buffer), m_DataOffset);
            std::cout << "\nDestructor: Wrote " << m_DataOffset << " trailing bytes for unbounded PES PID " << m_PID << std::endl;
        } else if (m_DataOffset <= m_TargetPayloadBytes) { 
            
        }
    }
    m_OutputFile.close();
  }
  delete[] m_Buffer; 
  m_Buffer = nullptr;
}

void xPES_Assembler::Init(int32_t PID, const std::string& outFileName){
  m_PID = PID;
  m_OutputFileName = outFileName;
  xBufferReset(); 
  m_LastContinuityCounter = -1; 
  m_Started = false;
  m_TargetPayloadBytes = 0;

  if (m_OutputFile.is_open()) { 
      m_OutputFile.close();
  }
  m_OutputFile.open(m_OutputFileName, std::ios::binary | std::ios::out | std::ios::trunc); 
  if (!m_OutputFile.is_open()) {
    std::cerr << "Error: Cannot open output file " << m_OutputFileName << std::endl;
  }
}

xPES_Assembler::eResult xPES_Assembler::AbsorbPacket(
    const uint8_t* TransportStreamPacket, 
    const xTS_PacketHeader* PacketHeader, 
    const xTS_AdaptationField* AdaptationField
) {
  if (!m_OutputFile.is_open()) {
      return eResult::FileError;
  }

  uint8_t currentCC = PacketHeader->getCC();
  if (m_Started && m_LastContinuityCounter != -1) {
      if (((m_LastContinuityCounter + 1) % 16) != currentCC) {
          if (!PacketHeader->getPUSI()) {
              std::cerr << "\nPID " << m_PID << ": CC Error! Expected "
                        << ((m_LastContinuityCounter + 1) % 16) << ", got " << (int)currentCC 
                        << ". Resetting PES." << std::endl;
              xBufferReset();
              m_Started = false;
              m_LastContinuityCounter = currentCC; // Resync
              return eResult::StreamPacketLost;
          }
          
          if (m_DataOffset > 0 && m_TargetPayloadBytes == 0xFFFFFFFF) { 
              m_OutputFile.write(reinterpret_cast<const char*>(m_Buffer), m_DataOffset);
          }
          xBufferReset(); 
      }
  }
  m_LastContinuityCounter = currentCC;

  const uint8_t* currentTSPayloadPtr = TransportStreamPacket + xTS::TS_HeaderLength;
  int32_t bytesInTSPayload = xTS::TS_PacketLength - xTS::TS_HeaderLength;

  if (PacketHeader->hasAdaptationField()) {
    uint8_t afActualLength = AdaptationField->getAdaptationFieldLengthValue();

    currentTSPayloadPtr += (afActualLength + 1);
    bytesInTSPayload -= (afActualLength + 1);
  }

  if (bytesInTSPayload <= 0) { 
    return m_Started ? eResult::AssemblingContinue : eResult::UnexpectedPID; 
  }
  
  eResult currentResult = eResult::AssemblingContinue;

  if (PacketHeader->getPUSI()) { 
    if (m_Started && m_DataOffset > 0 && m_TargetPayloadBytes == 0xFFFFFFFF) {
        m_OutputFile.write(reinterpret_cast<const char*>(m_Buffer), m_DataOffset);
    }
    xBufferReset();
    m_Started = true;

    m_PESH.Reset();
    int32_t actualPesHdrLen = m_PESH.Parse(currentTSPayloadPtr); 

    if (actualPesHdrLen < 0 || actualPesHdrLen > bytesInTSPayload) {
      std::cerr << "\nPID " << m_PID << ": PES Hdr Parse Err/Hdr Spans Packets (len=" 
                << actualPesHdrLen << ", avail=" << bytesInTSPayload << "). Reset." << std::endl;
      m_Started = false;
      return eResult::StreamPacketLost;
    }
    m_PESH.Print(); 

    const uint8_t* elementaryDataStartInTS = currentTSPayloadPtr + actualPesHdrLen;
    int32_t elementaryDataBytesInTS = bytesInTSPayload - actualPesHdrLen;

    if (elementaryDataBytesInTS > 0) {
      xBufferAppend(elementaryDataStartInTS, elementaryDataBytesInTS);
    }
    
    m_TargetPayloadBytes = m_PESH.getElementaryStreamPayloadLength();
    currentResult = eResult::AssemblingStarted;

  } else { 
    if (!m_Started) { 
      return eResult::UnexpectedPID; 
    }
    if (bytesInTSPayload > 0) {
        xBufferAppend(currentTSPayloadPtr, bytesInTSPayload);
    }
    currentResult = eResult::AssemblingContinue;
  }

  if (m_Started && m_TargetPayloadBytes != 0xFFFFFFFF) { 
    if (m_DataOffset >= m_TargetPayloadBytes) {
      m_OutputFile.write(reinterpret_cast<const char*>(m_Buffer), m_TargetPayloadBytes);
      
      uint32_t overflowBytes = m_DataOffset - m_TargetPayloadBytes;
      if (overflowBytes > 0) {
          std::memmove(m_Buffer, m_Buffer + m_TargetPayloadBytes, overflowBytes);
          m_DataOffset = overflowBytes;
      } else {
          xBufferReset(); 
      }
      m_Started = false;
      currentResult = eResult::AssemblingFinished;
    }
  }
  return currentResult;
}

void xPES_Assembler::xBufferReset() {
  m_DataOffset = 0;
}

void xPES_Assembler::xBufferAppend(const uint8_t* Data, int32_t Size) {
  if (Size <= 0) return;

  if (m_Buffer == nullptr) {
    m_BufferSize = (Size < 8192) ? 8192 : (Size * 2); 
    m_Buffer = new (std::nothrow) uint8_t[m_BufferSize];
    if (!m_Buffer) {
        std::cerr << "Failed to allocate PES buffer of size " << m_BufferSize << std::endl;
        m_Started = false;
        return;
    }
    m_DataOffset = 0;
  } else if (m_DataOffset + Size > m_BufferSize) {
    uint32_t requiredTotalSize = m_DataOffset + Size;
    uint32_t newPotentialSize = m_BufferSize;
    while (newPotentialSize < requiredTotalSize) {
        newPotentialSize *= 2;
        if (newPotentialSize > 20 * 1024 * 1024) { 
             std::cerr << "PES Buffer allocation limit exceeded: " << newPotentialSize << ". PID: " << m_PID << std::endl;
             xBufferReset(); 
             m_Started = false; 
             return; 
        }
    }
    
    uint8_t* newBuffer = new (std::nothrow) uint8_t[newPotentialSize];
    if (!newBuffer) {
        std::cerr << "Failed to reallocate PES buffer to size " << newPotentialSize << std::endl;
        xBufferReset();
        m_Started = false;
        return;
    }
    std::memcpy(newBuffer, m_Buffer, m_DataOffset);
    delete[] m_Buffer;
    m_Buffer = newBuffer;
    m_BufferSize = newPotentialSize;
  }

  std::memcpy(m_Buffer + m_DataOffset, Data, Size);
  m_DataOffset += Size;
}