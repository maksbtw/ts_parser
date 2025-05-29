#include "tsCommon.h"
#include "tsTransportStream.h"
#include <iostream>
#include <cstdio> 

int main() {
    const char* inputFileName = "../example_new.ts";
    const char* outputAudioFileName = "../pid136_audio.mp2";
    const int32_t targetAudioPID = 136;

    FILE* inputFile = fopen(inputFileName, "rb");
    if (!inputFile) {
        perror("File opening failed");
        std::cerr << "Error opening file: " << inputFileName << std::endl;
        return EXIT_FAILURE;
    }

    uint8_t tsPacketBuffer[xTS::TS_PacketLength];
    xTS_PacketHeader tsPacketHeader;
    xTS_AdaptationField tsAdaptationField;
    
    xPES_Assembler pesAssembler; 
    pesAssembler.Init(targetAudioPID, outputAudioFileName); 

    int32_t tsPacketId = 0;
    size_t bytesRead;

    std::cout << "Starting TS parsing for PID " << targetAudioPID 
              << ". Outputting to " << outputAudioFileName << std::endl;

    while ((bytesRead = fread(tsPacketBuffer, 1, xTS::TS_PacketLength, inputFile)) == xTS::TS_PacketLength) {
        tsPacketHeader.Reset();
        tsAdaptationField.Reset();

        if (tsPacketHeader.Parse(tsPacketBuffer) != 0) {
            continue;
        }
        
        if (tsPacketHeader.getPid() == targetAudioPID) { 
            printf("%010d ", tsPacketId);
            tsPacketHeader.Print();

            if (tsPacketHeader.hasAdaptationField()) {
                tsAdaptationField.Parse(tsPacketBuffer, tsPacketHeader.getAFC()); 
                tsAdaptationField.Print();
            }
            
            xPES_Assembler::eResult result = pesAssembler.AbsorbPacket(tsPacketBuffer, &tsPacketHeader, &tsAdaptationField);
            switch(result){
              case xPES_Assembler::eResult::AssemblingContinue:
                std::cout << " AssemblingContinue";
                break;
              case xPES_Assembler::eResult::AssemblingStarted:
                std::cout << " AssemblingStarted";
                break;
              case xPES_Assembler::eResult::AssemblingFinished:
                std::cout << " AssemblingFinished";
                break;
              case xPES_Assembler::eResult::StreamPacketLost: 
                std::cout << " StreamPacketLost"; 
                break;
              case xPES_Assembler::eResult::FileError:
                std::cout << " FileError";
                break;
              default:
                break;
            }
            printf("\n");
        } else {}
        tsPacketId++;
    }

    std::cout << "\nProcessed " << tsPacketId << " TS packets." << std::endl;
    if (ferror(inputFile)) {
        perror("Error");
    }

    fclose(inputFile);
    return EXIT_SUCCESS;
}