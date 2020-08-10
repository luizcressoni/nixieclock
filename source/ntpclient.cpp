/*! \file ntpclient.cpp */
#include "ntpclient.h"
#include <string.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

#define NTP_TIMESTAMP_DELTA 2208988800ull

cNtpClient::cNtpClient()
{
    memset( &mpacket, 0, sizeof( ntp_packet ) );
    mpacket.li_vn_mode = 0x1b;
}

cNtpClient::~cNtpClient()
{
}

const tm *cNtpClient::GetTimeFrom(const char *_hostname, int _gmtoffsecs)
{
    sockaddr_in serv_addr; // Server address data structure.
    hostent *server;      // Server data structure.

    int sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.
    if ( sockfd < 0 )
        return NULL;

    server = gethostbyname( _hostname ); // Convert URL to IP.
    if ( server == NULL )
        return NULL;

    bzero( ( char* ) &serv_addr, sizeof( serv_addr ) );
    serv_addr.sin_family = AF_INET;

    bcopy( ( char* )server->h_addr, ( char* ) &serv_addr.sin_addr.s_addr, server->h_length );
    serv_addr.sin_port = htons( 123 );


    // Call up the server using its IP address and port number.

    if ( connect( sockfd, ( sockaddr * ) &serv_addr, sizeof( serv_addr) ) < 0 )
        return NULL;

    // Send it the NTP packet it wants. If n == -1, it failed.

    int n = write( sockfd, ( char* ) &mpacket, sizeof( ntp_packet ) );

    if ( n < 0 )
        return NULL;

    //sleep(1);

    n = read( sockfd, ( char* ) &mpacket, sizeof( ntp_packet ) );

    if ( n < 0 )
        return NULL;

    // These two fields contain the time-stamp seconds as the packet left the NTP server.
// The number of seconds correspond to the seconds passed since 1900.
// ntohl() converts the bit/byte order from the network's to host's "endianness".

    mpacket.txTm_s = ntohl( mpacket.txTm_s ); // Time-stamp seconds.
    mpacket.txTm_f = ntohl( mpacket.txTm_f ); // Time-stamp fraction of a second.

// Extract the 32 bits that represent the time-stamp seconds (since NTP epoch) from when the packet left the server.
// Subtract 70 years worth of seconds from the seconds since 1900.
// This leaves the seconds since the UNIX epoch of 1970.
// (1900)------------------(1970)**************************************(Time Packet Left the Server)

    time_t txTm = ( time_t ) ( mpacket.txTm_s - NTP_TIMESTAMP_DELTA + _gmtoffsecs);

    tm *now = gmtime(&txTm);
    now->tm_gmtoff =_gmtoffsecs / 3600;
//    printf("now.hour = %d (%d)\n", now->tm_hour, _gmtoffsecs);
    mResultTm = *now;
    return &mResultTm;
}



//eof ntpclient.cpp
