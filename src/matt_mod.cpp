/* Copyright (c) 2013-2015 the Civetweb developers
 * Copyright (c) 2004-2013 Sergey Lyubka
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
*/

#if defined(_WIN32)

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS /* Disable deprecation warning in VS2005 */
#endif
#ifndef _CRT_SECURE_NO_DEPRECATE
#define _CRT_SECURE_NO_DEPRECATE
#endif

#endif

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include "civetweb.h"

#include <vector>
#include <set>
#include "../libjpeg-turbo/turbojpeg.h"

#if defined(_WIN32) && !defined(__SYMBIAN32__) /* WINDOWS / UNIX include block */
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0600 /* for tdm-gcc so we can use getconsolewindow */
    #endif
    #undef UNICODE
    #include <windows.h>
#endif /* defined(_WIN32) && !defined(__SYMBIAN32__) - WINDOWS / UNIX include  \
          block */

static volatile long g_threadDieFlag = 0;
static volatile long g_threadRunningFlag = 0;

typedef std::vector<BYTE> ImageData;
static std::vector<ImageData> s_images;
static size_t s_nextImage = 0;

static CRITICAL_SECTION s_csConnections;
static std::set<struct mg_connection *> s_connections;

static unsigned int const s_FPS = 60;

static unsigned __int64 s_lastTick = 0;
static unsigned int s_msgCount = 0;


void SendImageToAll( ImageData const & img )
{
    ::EnterCriticalSection( &s_csConnections );
    
    if( !img.empty() )
    {
        for( auto it = s_connections.begin(); it != s_connections.end(); ++it )
        {
            //mg_websocket_write( *it, WEBSOCKET_OPCODE_BINARY, (const char*)&img[0], img.size() );
            mg_websocket_write( *it, WEBSOCKET_OPCODE_TEXT, (const char*)&img[0], img.size() );
            ++s_msgCount;
        }
    }

    ::LeaveCriticalSection( &s_csConnections );
}

void * sender_thread_entry( void * ctx )
{
    (void)ctx;
    
    g_threadRunningFlag = 1;

    if( !s_images.empty() )
    {
        s_lastTick = ::GetTickCount64();
        s_msgCount = 0;
        while( !g_threadDieFlag )
        {
            SendImageToAll( s_images[s_nextImage] );
            s_nextImage = (s_nextImage + 1) % s_images.size();
            
            auto const tick = ::GetTickCount64();
            auto const diffMillis = (tick - s_lastTick);
            if( diffMillis > 2000 )
            {
                fprintf( stderr, "Msg rate: %f\n", float(s_msgCount)*1000.f/float(diffMillis) );
                s_msgCount = 0;
                s_lastTick = tick;
            }
            ::Sleep(1000/s_FPS);
        }
    }

    g_threadRunningFlag = 0;
    return nullptr;
}

#pragma pack(push,1)
struct ImgHeader
{
    unsigned __int16 width;
    unsigned __int16 height;
};
#pragma pack(pop)

void ReadImages()
{
    FILE * fp = fopen( "images.bin", "rb" );
    if( !fp )
    {
        fprintf( stderr, "Could not open images.bin\n" );
        return;
    }

    tjhandle hTJ = tjInitCompress();

    size_t totalRaw = 0;
    size_t totalJpeg = 0;
    size_t totalB64 = 0;

    ImgHeader header;
    std::vector<BYTE> rawBuffer;
    std::vector<unsigned char> jpegBuffer;
    std::vector<unsigned char> b64Buffer;
    while( 1 == fread( &header, sizeof(header), 1, fp ) )
    {
        auto const w = ntohs( header.width ), h = ntohs( header.height );
        unsigned int const numBytesRaw = w * h;
        //fprintf( stderr, "[%u] %u x %u == %u\n", s_images.size(), w, h, numBytes );
        //buffer.resize( sizeof(header) + numBytes );
        //memcpy( &buffer[0], &header, sizeof(header) );
        rawBuffer.resize( numBytesRaw );
        
        //size_t const numRead = fread( &buffer[sizeof(header)], sizeof(BYTE), numBytes, fp );
        size_t const numRead = fread( &rawBuffer[0], sizeof(BYTE), numBytesRaw, fp );
        if( numRead < numBytesRaw )
        {
            fprintf( stderr, "Stopping on partial read: %u bytes\n", numRead );
            break;
        }

        unsigned long maxJpegBytes = tjBufSize(w, h, TJSAMP_GRAY);
        jpegBuffer.resize( sizeof(header) + maxJpegBytes );
        memcpy( &jpegBuffer[0], &header, sizeof(header) );
        unsigned char * jpegPtr = &jpegBuffer[sizeof(header)];
        unsigned long jpegSize = maxJpegBytes;
        if( tjCompress2(hTJ, &rawBuffer[0], w, w, h, TJPF_GRAY, &jpegPtr, &jpegSize, TJSAMP_GRAY, 95, TJFLAG_NOREALLOC ) )
        {
            fprintf( stderr, "TurboJPEG error: %s\n", tjGetErrorStr() );
        }
        else
        {
            size_t const triplets = (jpegSize+2)/3; // Relying on integer truncation
            b64Buffer.resize( triplets * 4 );
            base64_encode( &jpegBuffer[sizeof(header)], jpegSize, (char*)&b64Buffer[0] );

            //jpegBuffer.resize( sizeof(header) + jpegSize );

            totalRaw += rawBuffer.size();
            totalJpeg += jpegSize;
            totalB64 += b64Buffer.size();

            s_images.push_back( std::move(b64Buffer) );
        }
    }

    size_t const count = s_images.size();
    fprintf( stderr, "Read in %u images @ %u FPS\n", count, s_FPS );
    fprintf( stderr, "Average raw bytes: %f\n", float(totalRaw)/float(count) );
    fprintf( stderr, "Average JPEG bytes: %f\n", float(totalJpeg)/float(count) );
    fprintf( stderr, "Average B64 bytes: %f\n", float(totalB64)/float(count) );
    fprintf( stderr, "Average B64/raw pct: %f\n", float(totalB64)*100.f/float(totalRaw) );
}

static int ws_on_connect(const struct mg_connection *, void *)
{
    fprintf( stderr, "Attempted new connection\n");
    return 0;
}

static void ws_on_ready(struct mg_connection * conn, void *)
{
    
    ::EnterCriticalSection( &s_csConnections );
        s_connections.insert( conn );
        size_t const count = s_connections.size();
    ::LeaveCriticalSection( &s_csConnections );

    mg_websocket_write( conn, WEBSOCKET_OPCODE_TEXT, "Hello world!", 12 );

    fprintf( stderr, "New websocket connection ready and added. Total: %u\n", count );
}

static int ws_on_data(struct mg_connection *,int,char *,size_t,void *)
{
    return 1;
}

static void ws_on_close(const struct mg_connection * conn, void *)
{
    ::EnterCriticalSection( &s_csConnections );
        s_connections.erase( (struct mg_connection *)conn );
        size_t const count = s_connections.size();
    ::LeaveCriticalSection( &s_csConnections );
    fprintf( stderr, "Closed websocket connection. Total: %u\n", count );
}

extern "C" void matt_mod( struct mg_context * ctx )
{
    ::InitializeCriticalSection( &s_csConnections );

    ReadImages();

    mg_set_websocket_handler( ctx, "/ws/rle",
        &ws_on_connect,
        &ws_on_ready,
        &ws_on_data,
        &ws_on_close,
        nullptr );

    g_threadDieFlag = 0;
    mg_start_thread( &sender_thread_entry, nullptr );
}

extern "C" void matt_mod_stop( )
{
    g_threadDieFlag = 1;
    for( unsigned int i = 0; i < 50 && g_threadRunningFlag; ++i )
    {
        ::Sleep(100);
    }

    ::EnterCriticalSection( &s_csConnections );
    ::LeaveCriticalSection( &s_csConnections );
}