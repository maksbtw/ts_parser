#pragma once
#include "tsCommon.h"
#include <string>
#include <fstream>

class xTS
{
public:
  static constexpr uint32_t TS_PacketLength  = 188;
  static constexpr uint32_t TS_HeaderLength  = 4;
  static constexpr uint32_t PES_HeaderLength = 6; // Minimalna długość nagłówka PES
  static constexpr uint32_t BaseClockFrequency_Hz         =    90000; //Hz
  static constexpr uint32_t ExtendedClockFrequency_Hz     = 27000000; //Hz
  static constexpr uint32_t BaseClockFrequency_kHz        =       90; //kHz
  static constexpr uint32_t ExtendedClockFrequency_kHz    =    27000; //kHz
  static constexpr uint32_t BaseToExtendedClockMultiplier =      300;
};

// TS Packet Header --------------------------------------
class xTS_PacketHeader
{
protected:
    uint8_t  m_SB; 
    bool     m_E; 
    bool     m_S; 
    bool     m_P; 
    uint16_t m_PID; 
    uint8_t  m_TSC; 
    uint8_t  m_AFC; 
    uint8_t  m_CC; 
public:
    void Reset();
    int32_t Parse(const uint8_t* Input);
    void Print() const;

    uint8_t getSyncByte() const { return m_SB; }
    bool getTransportErrorIndicator() const { return m_E; }
    bool getPUSI() const { return m_S; }
    bool getTransportPriority() const { return m_P; }
    uint16_t getPid() const { return m_PID; }
    uint8_t getTSC() const { return m_TSC; }
    uint8_t getAFC() const { return m_AFC; }
    uint8_t getCC() const { return m_CC; }

    bool hasAdaptationField() const { return (m_AFC == 0x02 || m_AFC == 0x03); } 
    bool hasPayload() const { return (m_AFC == 0x01 || m_AFC == 0x03); } 
};

// TS Packet Adaptation Field (AF) ------------------------------------------------------
class xTS_AdaptationField
{
protected:
    uint8_t m_AdaptationFieldLength;  
    bool m_DC;
    bool m_RA;
    bool m_SP; 
    bool m_PR;
    bool m_OR;
    bool m_SF; 
    bool m_TP; 
    bool m_EX;
public:
    void Reset();

    int32_t Parse(const uint8_t* PacketBuffer_Base, uint8_t AdaptationFieldControl_AFC_Value);
    void Print() const;
    uint8_t getAdaptationFieldLengthValue() const { return m_AdaptationFieldLength; }
};

// PES Header --------------------------------------------------------------------

class xPES_PacketHeader
{
public:
  enum eStreamId : uint8_t
  {
    eStreamId_program_stream_map = 0xBC,
    eStreamId_padding_stream = 0xBE,
    eStreamId_private_stream_2 = 0xBF,
    eStreamId_ECM = 0xF0,
    eStreamId_EMM = 0xF1,
    eStreamId_program_stream_directory = 0xFF,
    eStreamId_DSMCC_stream = 0xF2,
    eStreamId_ITUT_H222_1_type_E = 0xF8,
  };  

protected:
  uint32_t m_PacketStartCodePrefix;
  uint8_t  m_StreamId;
  uint16_t m_PacketLength;          
  uint8_t  m_OptionalPESHeaderLengthVal; 
  uint32_t m_ActualFullHeaderLength;  

public:
  void Reset();

  int32_t Parse(const uint8_t* pesHeaderStartPtr); 
  void Print() const;

  uint32_t getPacketStartCodePrefix() const { return m_PacketStartCodePrefix; }
  uint8_t getStreamId () const { return m_StreamId; }
  uint16_t getPESPacketLengthFieldVal () const { return m_PacketLength; }
  uint32_t getFullHeaderLength() const { return m_ActualFullHeaderLength; }
  uint32_t getElementaryStreamPayloadLength() const; 
};

// PES Assembler -------------------------------------------------------------------

class xPES_Assembler
{
public:
  enum class eResult : int32_t
  {
    UnexpectedPID = 1,
    StreamPacketLost,
    AssemblingStarted,
    AssemblingContinue,
    AssemblingFinished,
    FileError,
  };

protected:
  int32_t  m_PID; 
  //buffer
  uint8_t* m_Buffer;
  uint32_t m_BufferSize;
  uint32_t m_DataOffset;
  //operation
  bool     m_Started; 
  int8_t   m_LastContinuityCounter;
  xPES_PacketHeader m_PESH; 
  uint32_t m_TargetPayloadBytes; 

  std::ofstream m_OutputFile;
  std::string   m_OutputFileName;

public:
  xPES_Assembler ();
  ~xPES_Assembler();
  void Init(int32_t PID, const std::string& outFileName);
  
  eResult AbsorbPacket(const uint8_t* TransportStreamPacket, const xTS_PacketHeader* PacketHeader, const xTS_AdaptationField* AdaptationField);
  void PrintPESH () const { m_PESH.Print(); }
  uint8_t* getPacket () { return m_Buffer; }
  int32_t getNumPacketBytes() const { return m_DataOffset; }

protected:
  void xBufferReset ();
  void xBufferAppend(const uint8_t* Data, int32_t Size);
};