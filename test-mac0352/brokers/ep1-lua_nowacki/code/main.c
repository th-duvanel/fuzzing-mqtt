#include "portable_header.h"

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define Allocate(_Type, _Size)    (_Type*)calloc(sizeof(_Type), (_Size))
#define Deallocate(_Ptr)          (free((_Ptr)));
#define JOIN_BYTES(_A, _B)        (u16)((_A << 8) + (_B))
#define MAX(_A, _B)               (((_A) > (_B)) ? (_A) : (_B))
#define BUFFER_SIZE               10
#define CONTINUATION_BIT_SET(_A)  (((_A) & 0x80) ? true : false)

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

typedef enum
{
    RESERVED = 0,
    CONNECT = 1,
    CONNACK = 2,
    PUBLISH = 3,
    PUBACK = 4,
    PUBREC = 5,
    PUBREL = 6,
    PUBCOMP = 7,
    SUBSCRIBE = 8,
    SUBACK = 9,
    UNSUBSCRIBE = 10,
    UNSUBACK = 11,
    PINGREQ = 12,
    PINGRESP = 13,
    DISCONNECT = 14
} mqtt_packet_type;

static char* MQTT_PacketName[] = 
{
    [RESERVED] = "RESERVED",
    [CONNECT] = "CONNECT",
    [CONNACK] = "CONNACK",
    [PUBLISH] = "PUBLISH",
    [PUBACK] = "PUBACK",
    [PUBREL] = "PUBREL",
    [PUBCOMP] = "PUBCOMP",
    [SUBSCRIBE] = "SUBSCRIBE",
    [SUBACK] = "SUBACK",
    [UNSUBSCRIBE] = "UNSUBSCRIBE",
    [UNSUBACK] = "UNSUBACK",
    [PINGREQ] = "PINGREQ",
    [PINGRESP] = "PINGRESP",
    [DISCONNECT] = "DISCONNECT"
};

static char* MQTT_Direction[] = 
{
    [RESERVED] = "..",
    [PUBREL] = "..",
    [PUBCOMP] = "..",
    [UNSUBSCRIBE] = "..",
    [UNSUBACK] = "..",
    [CONNECT] = "<-",
    [CONNACK] = "->",
    [PUBLISH] = "<->",
    [PUBACK] = "->",
    [SUBSCRIBE] = "<-",
    [SUBACK] = "->",
    [PINGREQ] = "<-",
    [PINGRESP] = "->",
    [DISCONNECT] = "<-"
};

typedef struct
{
    char* key;
    SOCKET** value;
} topic_name_to_sockets;

typedef struct
{
    char* TopicName;
    char* Address;
    char* ClientID;
} socket_info;

typedef struct
{
    SOCKET key;
    socket_info value;
} socket_to_info;

static void
ClearSocketInfo(socket_info SocketInfo)
{
    if(SocketInfo.TopicName) Deallocate(SocketInfo.TopicName);
    if(SocketInfo.Address)   Deallocate(SocketInfo.Address);
    if(SocketInfo.ClientID)  Deallocate(SocketInfo.ClientID);
}

static void
ClearSocket(SOCKET Socket, 
            topic_name_to_sockets* HashMap, 
            socket_to_info* InvertedMap,
            fd_set* AllSocketsPtr)
{
    if(hmgeti(InvertedMap, Socket) >= 0)
    {
        socket_info SocketInfo = hmget(InvertedMap, Socket);
        
        if(SocketInfo.TopicName)
        {
            SOCKET** SubscribedSockets = shget(HashMap, SocketInfo.TopicName);
            for(SOCKET J = 0; J < arrlen(*SubscribedSockets); J++)
            {
                if((*SubscribedSockets)[J] == Socket)
                {
                    arrdel(*SubscribedSockets, J);
                }
            }
        }
        
        ClearSocketInfo(SocketInfo);
        hmdel(InvertedMap, Socket);
    }
    FD_CLR(Socket, AllSocketsPtr);
    CLOSESOCKET(Socket);
}

// NOTE(luatil): Receives a Ptr starting at the first byte of the remaining length
static u32
GetRemainingLength(u8* In)
{
    u32 Multiplier = 1;
    u32 Value = 0;
    u8 EncodedByte;
    do
    {
        EncodedByte = *In++;
        Value += (EncodedByte & 127) * Multiplier;
        Multiplier *= 128;
        if(Multiplier > 128*128*128)
        {
            // ERROR(Malformed Remaining Length)
        }
    } while((EncodedByte & 128) != 0);
    
    return Value;
}

static bool
SendAllBytes(u8* Buffer, SOCKET Socket, int TotalBytesToSend)
{
    int TotalBytesSent = 0;
    u8* SendPtr = Buffer;
    while(TotalBytesSent < TotalBytesToSend)
    {
        int BytesSent = send(Socket, SendPtr,
                             TotalBytesToSend - TotalBytesSent, 
                             0);
        SendPtr += BytesSent;
        TotalBytesSent += BytesSent;
        if(BytesSent < 1)
        {
            break;
        }
    }
    if(TotalBytesSent != TotalBytesToSend)
    {
        return false;
    }
    return true;
}

void
PrintWithTimestamp(char* Fmt, ...)
{
    va_list Args;
#if defined(_WIN32)
    fprintf(stdout, "%lld: ", time(0));
#else
    fprintf(stdout, "%ld: ", time(0));
#endif
    va_start(Args, Fmt);
    vfprintf(stdout, Fmt, Args);
    va_end(Args);
    fprintf(stdout, "\n");
}

void
PrintErrorWithErrorNumber(char* Fmt, ...)
{
    va_list Args;
#if defined(_WIN32)
    fprintf(stderr, "%lld: Error: ", time(0));
#else
    fprintf(stderr, "%ld: Error: ", time(0));
#endif
    va_start(Args, Fmt);
    vfprintf(stderr, Fmt, Args);
    va_end(Args);
    fprintf(stderr, "(%d)\n", GETSOCKETERRNO());
}

int
main(int ArgumentCount, char** Arguments)
{
    if(ArgumentCount != 2)
    {
        fprintf(stderr,"Usage: %s <Port>\n", Arguments[0]);
        return 1;
    }
    
#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d)) 
    {
        fprintf(stderr, "Failed to initialize.\n");
        return 1;
    }
#endif
    
    struct addrinfo Hints = {0};
    Hints.ai_family   = AF_INET;
    Hints.ai_socktype = SOCK_STREAM;
    Hints.ai_flags    = AI_PASSIVE;
    
    struct addrinfo *BindAddress;
    getaddrinfo(0, Arguments[1], &Hints, &BindAddress);
    
    SOCKET SocketListen;
    SocketListen = socket(BindAddress->ai_family, 
                          BindAddress->ai_socktype,
                          BindAddress->ai_protocol);
    if(!ISVALIDSOCKET(SocketListen))
    {
        PrintErrorWithErrorNumber("socket() failed. ");
        return 1;
    }
    
    if (bind(SocketListen, BindAddress->ai_addr, BindAddress->ai_addrlen)) 
    {
        PrintErrorWithErrorNumber("bind() failed. ");
        return 1;
    }
    freeaddrinfo(BindAddress);
    
    
    if (listen(SocketListen, 10) < 0) 
    {
        PrintErrorWithErrorNumber("listen() failed. ");
        return 1;
    }
    
    PrintWithTimestamp("Opening ipv4 listen socket on port %s", Arguments[1]);
    
    fd_set AllSockets;
    FD_ZERO(&AllSockets);
    FD_SET(SocketListen, &AllSockets);
    SOCKET MaxSocket = SocketListen;
    
    topic_name_to_sockets *HashMap = NULL;
    sh_new_arena(HashMap);
    shdefault(HashMap, 0);
    
    socket_to_info *InvertedMap = NULL;
    
    for(;;)
    {
        fd_set ReadSockets;
        ReadSockets = AllSockets;
        
        if(select(MaxSocket+1, &ReadSockets, 0, 0, 0) < 0)
        {
            PrintErrorWithErrorNumber("select() failed. ");
            return 1;
        }
        
        SOCKET CurrentSocket;
        for(CurrentSocket = 1; CurrentSocket <= MaxSocket; ++CurrentSocket)
        {
            if(FD_ISSET(CurrentSocket, &ReadSockets))
            {
                if(CurrentSocket == SocketListen)
                {
                    struct sockaddr_storage ClientAddress;
                    socklen_t ClientLength = sizeof(ClientAddress);
                    SOCKET SocketClient = accept(SocketListen,
                                                 (struct sockaddr*) &ClientAddress,
                                                 &ClientLength);
                    if(!ISVALIDSOCKET(SocketClient))
                    {
                        PrintErrorWithErrorNumber("accept() failed. ");
                        return 1;
                    }
                    
                    FD_SET(SocketClient, &AllSockets);
                    MaxSocket = MAX(SocketClient, MaxSocket);
                    
                    char* AddressBuffer = Allocate(char, 256);
                    getnameinfo((struct sockaddr*)&ClientAddress,
                                ClientLength,
                                AddressBuffer, 256, 0, 0,
                                NI_NUMERICHOST);
                    
                    socket_info SocketInfo = {0};
                    SocketInfo.Address = AddressBuffer;
                    hmput(InvertedMap, SocketClient, SocketInfo);
                    PrintWithTimestamp("New connection from %s", AddressBuffer);
                }
                else
                {
                    u8 FixedHeaderBuffer[5] = {0};
                    int FixedHeaderBytesReceived = recv(CurrentSocket, FixedHeaderBuffer,
                                                        2, 0);
                    bool FailedToParseHeader = false;
                    u32 RemainingLength = 0;
                    
                    u8* AllocatedBuffer = {0};
                    switch(FixedHeaderBytesReceived)
                    {
                        case 1:
                        {
                            int BytesReceived = recv(CurrentSocket, FixedHeaderBuffer + 1,
                                                     1, 0);
                        };
                        case 2:
                        {
                            while(FixedHeaderBytesReceived < 5 &&
                                  CONTINUATION_BIT_SET(*(FixedHeaderBuffer + FixedHeaderBytesReceived - 1)))
                            {
                                int BytesReceived = recv(CurrentSocket, 
                                                         FixedHeaderBuffer + FixedHeaderBytesReceived,
                                                         1, 0);
                                FixedHeaderBytesReceived += BytesReceived;
                            }
                            RemainingLength = GetRemainingLength(FixedHeaderBuffer + 1);
                        } break;
                        default:
                        {
                            FailedToParseHeader = true;
                        } break;
                    }
                    
                    if(FailedToParseHeader)
                    {
                        ClearSocket(CurrentSocket, 
                                    (topic_name_to_sockets*)HashMap, 
                                    (socket_to_info*)InvertedMap, &AllSockets);
                        continue;
                    }
                    
                    AllocatedBuffer = Allocate(u8, RemainingLength + FixedHeaderBytesReceived);
                    memcpy(AllocatedBuffer, FixedHeaderBuffer, FixedHeaderBytesReceived);
                    int TotalBytesReceived = FixedHeaderBytesReceived;
                    u8* Ptr = AllocatedBuffer + FixedHeaderBytesReceived;
                    while(TotalBytesReceived < FixedHeaderBytesReceived + RemainingLength)
                    {
                        int BytesReceived = recv(CurrentSocket, Ptr,
                                                 RemainingLength + FixedHeaderBytesReceived - TotalBytesReceived,
                                                 0);
                        TotalBytesReceived += BytesReceived;
                        if(BytesReceived < 1)
                        {
                            break;
                        }
                    }
                    
                    if(TotalBytesReceived != FixedHeaderBytesReceived + RemainingLength)
                    {
                        ClearSocket(CurrentSocket,
                                    (topic_name_to_sockets*)HashMap,
                                    (socket_to_info*)InvertedMap,
                                    &AllSockets);
                        Deallocate(AllocatedBuffer);
                        continue;
                    }
                    
                    u8 TypeAndFlags = FixedHeaderBuffer[0];
                    u8* In = AllocatedBuffer + FixedHeaderBytesReceived;
                    mqtt_packet_type PacketType = (mqtt_packet_type)(TypeAndFlags >> 4);
                    switch(PacketType)
                    {
                        case CONNECT:
                        {
                            u8 LengthMSB     = *In++;
                            u8 LengthLSB     = *In++;
                            u8* MQTT = In;   In += 4;
                            u8 ProtocolLevel = *In++;
                            u8 ConnectFlags  = *In++;
                            u8 KeepAliveMSB  = *In++;
                            u8 KeepAliveLSB  = *In++;
                            u8 KeepAliveSeconds = JOIN_BYTES(KeepAliveMSB,
                                                             KeepAliveLSB);
                            u8 ClientIdLengthMSB = *In++;
                            u8 ClientIdLengthLSB = *In++;
                            u16 ClientIDLength   = JOIN_BYTES(ClientIdLengthMSB,
                                                              ClientIdLengthLSB);
                            
                            char* ClientIdBuffer = Allocate(char, ClientIDLength+1);
                            memcpy(ClientIdBuffer, In, ClientIDLength);
                            ClientIdBuffer[ClientIDLength] = 0;
                            
                            int SocketInfoIndex = hmgeti(InvertedMap, CurrentSocket);
                            InvertedMap[SocketInfoIndex].value.ClientID = ClientIdBuffer;
                            PrintWithTimestamp("%-10s %s %s",
                                               MQTT_PacketName[PacketType],
                                               MQTT_Direction[PacketType],
                                               InvertedMap[SocketInfoIndex].value.ClientID);
                            
                            PrintWithTimestamp("%-10s %s %s",
                                               MQTT_PacketName[CONNACK],
                                               MQTT_Direction[CONNACK],
                                               InvertedMap[SocketInfoIndex].value.ClientID);
                            u8 Connack[] = {0x20, 0x02, 0x00, 0x00};
                            if(!SendAllBytes(Connack, 
                                             CurrentSocket,
                                             sizeof(Connack)))
                            {
                                PrintWithTimestamp("Unable To Send All Bytes to %s. "
                                                   "Closing Connection. ",
                                                   InvertedMap[SocketInfoIndex].value.ClientID);
                                ClearSocket(CurrentSocket, 
                                            (topic_name_to_sockets*)HashMap, 
                                            (socket_to_info*)InvertedMap, &AllSockets);
                            }
                        } break;
                        case PUBLISH:
                        {
                            u8 TopicLengthMSB = *In++;
                            u8 TopicLengthLSB = *In++;
                            u16 TopicLength = JOIN_BYTES(TopicLengthMSB,
                                                         TopicLengthLSB);
                            char* TopicName = (char*)In;
                            In += TopicLength;
                            u8  ReplaceChar    = *In;
                            u8* ReplaceCharPos = In;
                            
                            u16 MessageLength = RemainingLength - TopicLength - 2;
                            
                            u8* MessageBuffer = In;
                            In += MessageLength;
                            
                            socket_info SocketInfo = hmget(InvertedMap, CurrentSocket);
                            
                            
                            PrintWithTimestamp("%-10s <- %s (%.*s)",
                                               MQTT_PacketName[PUBLISH],
                                               SocketInfo.ClientID,
                                               TopicLength,
                                               TopicName);
                            
                            // NOTE(luatil): There is no need to respond given QoS = 0
                            *ReplaceCharPos = 0;
                            SOCKET** SubscribedSockets = shget(HashMap, TopicName);
                            *ReplaceCharPos = ReplaceChar;
                            if(SubscribedSockets)
                            {
                                for(u32 I = 0; I < arrlen(*SubscribedSockets); I++)
                                {
                                    socket_info SocketInfoSubscride = hmget(InvertedMap, (*SubscribedSockets)[I]);
                                    PrintWithTimestamp("%-10s -> %s (%.*s)",
                                                       MQTT_PacketName[PUBLISH],
                                                       SocketInfoSubscride.ClientID,
                                                       TopicLength,
                                                       TopicName);
                                    
                                    if(!SendAllBytes(AllocatedBuffer, 
                                                     (*SubscribedSockets)[I],
                                                     RemainingLength + FixedHeaderBytesReceived))
                                    {
                                        PrintWithTimestamp("Unable To Send All Bytes to %s. "
                                                           "Closing Connection. ",
                                                           SocketInfoSubscride.ClientID);
                                        ClearSocket(CurrentSocket, 
                                                    (topic_name_to_sockets*)HashMap, 
                                                    (socket_to_info*)InvertedMap, &AllSockets);
                                    }
                                }
                            }
                        } break;
                        case SUBSCRIBE:
                        {
                            u8 PacketIdentifierMSB = *In++;
                            u8 PacketIdentifierLSB = *In++;
                            u16 PacketIdentifier   = JOIN_BYTES(PacketIdentifierMSB, 
                                                                PacketIdentifierLSB);
                            u8 TopicLengthMSB = *In++;
                            u8 TopicLengthLSB = *In++;
                            u16 TopicLength   = JOIN_BYTES(TopicLengthMSB,
                                                           TopicLengthLSB);
                            
                            char* TopicBuffer = (char*)In;
                            In += TopicLength;
                            u8 RequestedQos = *In;
                            *In = 0;
                            
                            int Index = shgeti(HashMap, TopicBuffer);
                            if(Index < 0)
                            {
                                char* AllocatedTopicName = Allocate(char, TopicLength + 1);
                                memcpy(AllocatedTopicName, TopicBuffer, TopicLength + 1);
                                SOCKET** SubscribedSockets = Allocate(SOCKET*, 1);
                                arrput(*SubscribedSockets, CurrentSocket);
                                shput(HashMap, TopicBuffer, SubscribedSockets);
                            }
                            else
                            {
                                arrput((*(HashMap[Index].value)), CurrentSocket);
                            }
                            
                            TopicLength += 1;
                            char* TopicBufferAllocated = Allocate(char, TopicLength);
                            memcpy(TopicBufferAllocated, TopicBuffer, TopicLength);
                            int SocketInfoIndex = hmgeti(InvertedMap, CurrentSocket);
                            InvertedMap[SocketInfoIndex].value.TopicName = TopicBufferAllocated;
                            
                            PrintWithTimestamp("%-10s %s %s (%s)",
                                               MQTT_PacketName[SUBSCRIBE],
                                               MQTT_Direction[SUBSCRIBE],
                                               InvertedMap[SocketInfoIndex].value.ClientID,
                                               TopicBufferAllocated);
                            
                            PrintWithTimestamp("%-10s %s %s",
                                               MQTT_PacketName[SUBACK],
                                               MQTT_Direction[SUBACK],
                                               InvertedMap[SocketInfoIndex].value.ClientID);
                            u8 Suback[] = {0x90, 0x03, 0x00, 0x01, 0x00};
                            if(!SendAllBytes(Suback, 
                                             CurrentSocket,
                                             sizeof(Suback)))
                            {
                                PrintWithTimestamp("Unable To Send All Bytes to %s. "
                                                   "Closing Connection. ",
                                                   InvertedMap[SocketInfoIndex].value.ClientID);
                                ClearSocket(CurrentSocket, 
                                            (topic_name_to_sockets*)HashMap, 
                                            (socket_to_info*)InvertedMap, &AllSockets);
                            }
                        } break;
                        case PINGREQ:
                        {
                            socket_info SocketInfo = hmget(InvertedMap, CurrentSocket);
                            PrintWithTimestamp("%-10s %s %s",
                                               MQTT_PacketName[PINGREQ],
                                               MQTT_Direction[PINGREQ],
                                               SocketInfo.ClientID);
                            PrintWithTimestamp("%-10s %s %s",
                                               MQTT_PacketName[PINGRESP],
                                               MQTT_Direction[PINGRESP],
                                               SocketInfo.ClientID);
                            u8 PingResp[] = {0xd0, 0x00};
                            if(!SendAllBytes(PingResp, 
                                             CurrentSocket,
                                             sizeof(PingResp)))
                            {
                                PrintWithTimestamp("Unable To Send All Bytes to %s. "
                                                   "Closing Connection. ",
                                                   SocketInfo.ClientID);
                                ClearSocket(CurrentSocket, 
                                            (topic_name_to_sockets*)HashMap, 
                                            (socket_to_info*)InvertedMap, &AllSockets);
                            }
                        } break;
                        case DISCONNECT:
                        {
                            socket_info SocketInfo = hmget(InvertedMap, CurrentSocket);
                            PrintWithTimestamp("%-10s %s %s",
                                               MQTT_PacketName[DISCONNECT],
                                               MQTT_Direction[DISCONNECT],
                                               SocketInfo.ClientID);
                            ClearSocket(CurrentSocket,
                                        (topic_name_to_sockets*)HashMap,
                                        (socket_to_info*)InvertedMap,
                                        &AllSockets);
                        } break;
                        case RESERVED:
                        case CONNACK:
                        case PUBACK:
                        case PUBREL:
                        case PUBCOMP:
                        case SUBACK:
                        case UNSUBACK:
                        case PINGRESP:
                        {
                            PrintWithTimestamp("%s: NOT IMPLEMENTED", MQTT_PacketName[PacketType]);
                        } break;
                        default:
                        {
                            PrintWithTimestamp("PACKET TYPE NOT RECOGNIZED");
                        } break;
                    }
                    Deallocate(AllocatedBuffer);
                }
            }
        }
    }
    
    
    // NOTE(luatil): Free the HashMap
    for(int I = 0; I < shlen(HashMap); I++)
    {
        SOCKET** Result = HashMap[I].value;
        arrfree(*Result);
        Deallocate(Result);
        Deallocate(HashMap[I].key);
    }
    shfree(HashMap);
    
    // NOTE(luatil): Free the Inverted Map
    for(int I = 0; I < hmlen(InvertedMap); I++)
    {
        socket_info SocketInfo = InvertedMap[I].value;
        ClearSocketInfo(SocketInfo);
    }
    hmfree(InvertedMap);
    
    CLOSESOCKET(SocketListen);
    
#if defined(_WIN32)
    WSACleanup();
#endif
    
    return 0;
}
