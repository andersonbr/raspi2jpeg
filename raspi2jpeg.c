//-------------------------------------------------------------------------
//
// The MIT License (MIT)
//
// Copyright (c) 2014 Andrew Duncan
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------

#define _GNU_SOURCE

#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "bcm_host.h"

// JPEG
#include <jpeglib.h>
#include <setjmp.h>

//-----------------------------------------------------------------------

#ifndef ALIGN_TO_16
#define ALIGN_TO_16(x)  ((x + 15) & ~15)
#endif

//-----------------------------------------------------------------------

#define DEFAULT_DELAY 0
#define DEFAULT_DISPLAY_NUMBER 0
#define DEFAULT_NAME "snapshot.jpg"
#define DEFAULT_COMPRESSION (0.75)

//-----------------------------------------------------------------------

const char* program = NULL;

//-----------------------------------------------------------------------

void
usage(void)
{
    fprintf(stderr, "Usage: %s [--jpegname name]", program);
    fprintf(stderr, " [--width <width>] [--height <height>]");
    fprintf(stderr, " [--compression <level>]");
    fprintf(stderr, " [--delay <delay>] [--display <number>]");
    fprintf(stderr, " [--stdout] [--help]\n");

    fprintf(stderr, "\n");

    fprintf(stderr, "    --jpegname,-j - name of jpeg file to create ");
    fprintf(stderr, "(default is %s)\n", DEFAULT_NAME);

    fprintf(stderr, "    --height,-h - image height ");
    fprintf(stderr, "(default is screen height)\n");

    fprintf(stderr, "    --width,-w - image width ");
    fprintf(stderr, "(default is screen width)\n");

    fprintf(stderr, "    --compression,-c - JPEG compression level ");
    fprintf(stderr, "[0.0 - 1.0]. (default is %.2f) \n", DEFAULT_COMPRESSION);

    fprintf(stderr, "    --delay,-d - delay in seconds ");
    fprintf(stderr, "(default %d)\n", DEFAULT_DELAY);

    fprintf(stderr, "    --display,-D - Raspberry Pi display number ");
    fprintf(stderr, "(default %d)\n", DEFAULT_DISPLAY_NUMBER);

    fprintf(stderr, "    --stdout,-s - write file to stdout\n");

    fprintf(stderr, "    --help,-H - print this usage information\n");

    fprintf(stderr, "\n");
}


//-----------------------------------------------------------------------

int
main(
    int argc,
    char *argv[])
{
    int opt = 0;

    bool writeToStdout = false;
    char *jpegName = DEFAULT_NAME;
    int32_t requestedWidth = 0;
    int32_t requestedHeight = 0;
    uint32_t displayNumber = DEFAULT_DISPLAY_NUMBER;
    float compression = DEFAULT_COMPRESSION;
    int delay = DEFAULT_DELAY;

    VC_IMAGE_TYPE_T imageType = VC_IMAGE_RGBA32;
    int8_t dmxBytesPerPixel  = 4;

    int result = 0;

    program = basename(argv[0]);

    //-------------------------------------------------------------------

    char *sopts = "c:d:D:Hh:j:w:s";

    struct option lopts[] =
    {
        { "compression", required_argument, NULL, 'c' },
        { "delay", required_argument, NULL, 'd' },
        { "display", required_argument, NULL, 'D' },
        { "height", required_argument, NULL, 'h' },
        { "help", no_argument, NULL, 'H' },
        { "jpegname", required_argument, NULL, 'j' },
        { "width", required_argument, NULL, 'w' },
        { "stdout", no_argument, NULL, 's' },
        { NULL, no_argument, NULL, 0 }
    };

    while ((opt = getopt_long(argc, argv, sopts, lopts, NULL)) != -1)
    {
        switch (opt)
        {
        case 'c':

            compression = atof(optarg);

            if ((compression < 0) || (compression > 1))
            {
                compression = DEFAULT_COMPRESSION;
            }

            break;

        case 'd':

            delay = atoi(optarg);
            break;

        case 'D':

            displayNumber = atoi(optarg);
            break;

        case 'h':

            requestedHeight = atoi(optarg);
            break;

        case 'j':

            jpegName = optarg;
            break;

        case 'w':

            requestedWidth = atoi(optarg);
            break;

        case 's':

            writeToStdout = true;
            break;

        case 'H':
        default:

            usage();

            if (opt == 'H')
            {
                exit(EXIT_SUCCESS);
            }
            else
            {
                exit(EXIT_FAILURE);
            }

            break;
        }
    }

    //-------------------------------------------------------------------

    bcm_host_init();

    //-------------------------------------------------------------------
    //
    // When the display is rotate (either 90 or 270 degrees) we need to
    // swap the width and height of the snapshot
    //

    char response[1024];
    int displayRotated = 0;

    if (vc_gencmd(response, sizeof(response), "get_config int") == 0)
    {
        vc_gencmd_number_property(response,
                                  "display_rotate",
                                  &displayRotated);
    }

    //-------------------------------------------------------------------

    if (delay)
    {
        sleep(delay);
    }

    //-------------------------------------------------------------------

    DISPMANX_DISPLAY_HANDLE_T displayHandle
        = vc_dispmanx_display_open(displayNumber);

    if (displayHandle == 0)
    {
        fprintf(stderr,
                "%s: unable to open display %d\n",
                program,
                displayNumber);

        exit(EXIT_FAILURE);
    }

    DISPMANX_MODEINFO_T modeInfo;
    result = vc_dispmanx_display_get_info(displayHandle, &modeInfo);

    if (result != 0)
    {
        fprintf(stderr, "%s: unable to get display information\n", program);
        exit(EXIT_FAILURE);
    }

    int32_t jpegWidth = modeInfo.width;
    int32_t jpegHeight = modeInfo.height;

    if (requestedWidth > 0)
    {
        jpegWidth = requestedWidth;

        if (requestedHeight == 0)
        {
            double numerator = modeInfo.height * requestedWidth;
            double denominator = modeInfo.width;

            jpegHeight = (int32_t)ceil(numerator / denominator);
        }
    }

    if (requestedHeight > 0)
    {
        jpegHeight = requestedHeight;

        if (requestedWidth == 0)
        {
            double numerator = modeInfo.width * requestedHeight;
            double denominator = modeInfo.height;

            jpegWidth = (int32_t)ceil(numerator / denominator);
        }
    }

    //-------------------------------------------------------------------
    // only need to check low bit of displayRotated (value of 1 or 3).
    // If the display is rotated either 90 or 270 degrees (value 1 or 3)
    // the width and height need to be transposed.

    int32_t dmxWidth = jpegWidth;
    int32_t dmxHeight = jpegHeight;

    if (displayRotated & 1)
    {
        dmxWidth = jpegHeight;
        dmxHeight = jpegWidth;
    }

    int32_t dmxPitch = dmxBytesPerPixel * ALIGN_TO_16(dmxWidth);

    void *dmxImagePtr = malloc(dmxPitch * dmxHeight);

    if (dmxImagePtr == NULL)
    {
        fprintf(stderr, "%s: unable to allocated image buffer\n", program);
        exit(EXIT_FAILURE);
    }

    //-------------------------------------------------------------------

    uint32_t vcImagePtr = 0;
    DISPMANX_RESOURCE_HANDLE_T resourceHandle;
    resourceHandle = vc_dispmanx_resource_create(imageType,
                                                 dmxWidth,
                                                 dmxHeight,
                                                 &vcImagePtr);

    result = vc_dispmanx_snapshot(displayHandle,
                                  resourceHandle,
                                  DISPMANX_NO_ROTATE);

    if (result != 0)
    {
        vc_dispmanx_resource_delete(resourceHandle);
        vc_dispmanx_display_close(displayHandle);

        fprintf(stderr, "%s: vc_dispmanx_snapshot() failed\n", program);
        exit(EXIT_FAILURE);
    }

    VC_RECT_T rect;
    result = vc_dispmanx_rect_set(&rect, 0, 0, dmxWidth, dmxHeight);

    if (result != 0)
    {
        vc_dispmanx_resource_delete(resourceHandle);
        vc_dispmanx_display_close(displayHandle);

        fprintf(stderr, "%s: vc_dispmanx_rect_set() failed\n", program);
        exit(EXIT_FAILURE);
    }

    result = vc_dispmanx_resource_read_data(resourceHandle,
                                            &rect,
                                            dmxImagePtr,
                                            dmxPitch);


    if (result != 0)
    {
        vc_dispmanx_resource_delete(resourceHandle);
        vc_dispmanx_display_close(displayHandle);

        fprintf(stderr,
                "%s: vc_dispmanx_resource_read_data() failed\n",
                program);

        exit(EXIT_FAILURE);
    }

    vc_dispmanx_resource_delete(resourceHandle);
    vc_dispmanx_display_close(displayHandle);

    //-------------------------------------------------------------------
    // Convert from RGBA (32 bit) to RGB (24 bit)

    int8_t jpegBytesPerPixel = 3;
    int32_t jpegPitch = jpegBytesPerPixel * jpegWidth;
    void *jpegImagePtr = malloc(jpegPitch * jpegHeight);

    int32_t j = 0;
    for (j = 0 ; j < jpegHeight ; j++)
    {
        int32_t dmxXoffset = 0;
        int32_t dmxYoffset = 0;

        switch (displayRotated & 3)
        {
        case 0: // 0 degrees

            if (displayRotated & 0x20000) // flip vertical
            {
                dmxYoffset = (dmxHeight - j - 1) * dmxPitch;
            }
            else
            {
                dmxYoffset = j * dmxPitch;
            }

            break;

        case 1: // 90 degrees


            if (displayRotated & 0x20000) // flip vertical
            {
                dmxXoffset = j * dmxBytesPerPixel;
            }
            else
            {
                dmxXoffset = (dmxWidth - j - 1) * dmxBytesPerPixel;
            }

            break;

        case 2: // 180 degrees

            if (displayRotated & 0x20000) // flip vertical
            {
                dmxYoffset = j * dmxPitch;
            }
            else
            {
                dmxYoffset = (dmxHeight - j - 1) * dmxPitch;
            }

            break;

        case 3: // 270 degrees

            if (displayRotated & 0x20000) // flip vertical
            {
                dmxXoffset = (dmxWidth - j - 1) * dmxBytesPerPixel;
            }
            else
            {
                dmxXoffset = j * dmxBytesPerPixel;
            }

            break;
        }

        int32_t i = 0;
        for (i = 0 ; i < jpegWidth ; i++)
        {
            uint8_t *jpegPixelPtr = jpegImagePtr
                                 + (i * jpegBytesPerPixel)
                                 + (j * jpegPitch);

            switch (displayRotated & 3)
            {
            case 0: // 0 degrees

                if (displayRotated & 0x10000) // flip horizontal
                {
                    dmxXoffset = (dmxWidth - i - 1) * dmxBytesPerPixel;
                }
                else
                {
                    dmxXoffset = i * dmxBytesPerPixel;
                }

                break;

            case 1: // 90 degrees

                if (displayRotated & 0x10000) // flip horizontal
                {
                    dmxYoffset = (dmxHeight - i - 1) * dmxPitch;
                }
                else
                {
                    dmxYoffset = i * dmxPitch;
                }

                break;

            case 2: // 180 degrees

                if (displayRotated & 0x10000) // flip horizontal
                {
                    dmxXoffset = i * dmxBytesPerPixel;
                }
                else
                {
                    dmxXoffset = (dmxWidth - i - 1) * dmxBytesPerPixel;
                }

                break;

            case 3: // 270 degrees

                if (displayRotated & 0x10000) // flip horizontal
                {
                    dmxYoffset = i * dmxPitch;
                }
                else
                {
                    dmxYoffset = (dmxHeight - i - 1) * dmxPitch;
                }

                break;
            }

            uint8_t *dmxPixelPtr = dmxImagePtr + dmxXoffset + dmxYoffset;

            memcpy(jpegPixelPtr, dmxPixelPtr, 3);
        }
    }

    free(dmxImagePtr);
    dmxImagePtr = NULL;

    //-------------------------------------------------------------------
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    ///// OUTPUT FILE / STDOUT
    FILE *outfp = NULL;

    if (writeToStdout)
    {
        outfp = stdout;
    }
    else
    {
        outfp = fopen(jpegName, "wb");

        if (outfp == NULL)
        {
            fprintf(stderr,
                    "%s: unable to create %s - %s\n",
                    program,
                    jpegName,
                    strerror(errno));

            exit(EXIT_FAILURE);
        }
    }
    jpeg_stdio_dest(&cinfo, outfp);
    cinfo.image_width = jpegWidth;
    cinfo.image_height = jpegHeight;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, (int)(compression * 100), TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = jpegWidth * 3;
    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = jpegImagePtr + (cinfo.next_scanline * row_stride); 
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    jpeg_finish_compress(&cinfo);
    if (outfp != stdout)
    {
        fclose(outfp);
    }

    //-------------------------------------------------------------------

    free(jpegImagePtr);
    jpegImagePtr = NULL;

    return 0;
}

