/* mz_os.c -- System functions
   Version 2.2.9, April 18th, 2018
   part of the MiniZip project

   Copyright (C) 2010-2018 Nathan Moinvaziri
     https://github.com/nmoinvaz/minizip
   Copyright (C) 1998-2010 Gilles Vollant
     http://www.winimage.com/zLibDll/minizip.html

   This program is distributed under the terms of the same license as zlib.
   See the accompanying LICENSE file for the full text of the license.
*/

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "mz.h"
#include "mz_os.h"
#include "mz_strm.h"
#ifdef HAVE_LZMA
#  include "mz_strm_lzma.h"
#endif
#ifdef HAVE_ZLIB
#  include "mz_strm_zlib.h"
#endif

/***************************************************************************/

int32_t mz_make_dir(const char *path)
{
    int32_t err = MZ_OK;
    int16_t len = 0;
    char *current_dir = NULL;
    char *match = NULL;
    char hold = 0;


    len = (int16_t)strlen(path);
    if (len <= 0)
        return 0;

    current_dir = (char *)malloc(len + 1);
    if (current_dir == NULL)
        return MZ_MEM_ERROR;

    strcpy(current_dir, path);

    if (current_dir[len - 1] == '/')
        current_dir[len - 1] = 0;

    err = mz_os_make_dir(current_dir);
    if (err != MZ_OK)
    {
        match = current_dir + 1;
        while (1)
        {
            while (*match != 0 && *match != '\\' && *match != '/')
                match += 1;
            hold = *match;
            *match = 0;

            err = mz_os_make_dir(current_dir);
            if (err != MZ_OK)
                break;
            if (hold == 0)
                break;

            *match = hold;
            match += 1;
        }
    }

    free(current_dir);
    return err;
}

int32_t mz_path_combine(char *path, const char *join, int32_t max_path)
{
    int32_t path_len = 0;

    if (path == NULL || join == NULL || max_path == 0)
        return MZ_PARAM_ERROR;

    path_len = strlen(path);

    if (path_len == 0)
    {
        strncpy(path, join, max_path);
    }
    else
    {
        if (path[path_len - 1] != '\\' && path[path_len - 1] != '/')
            strncat(path, "/", max_path - path_len - 1);
        strncat(path, join, max_path - path_len);
    }

    return MZ_OK;
}

int32_t mz_attrib_is_dir(int32_t attributes, int32_t version_madeby)
{
    int32_t host_system = 0;

    if (version_madeby == 0)
        host_system = MZ_VERSION_MADEBY_HOST_SYSTEM;
    else
        host_system = (uint8_t)(version_madeby >> 8);

    if (host_system == MZ_HOST_SYSTEM_WINDOWS_NTFS)
    {
        if ((attributes & 0x10) == 0x10) // FILE_ATTRIBUTE_DIRECTORY
            return MZ_OK;
    }
    else if (host_system == MZ_HOST_SYSTEM_UNIX || host_system == MZ_HOST_SYSTEM_OSX_DARWIN)
    {
        if ((attributes & 00170000) == 0040000) // S_ISDIR
            return MZ_OK;
    }

    return MZ_EXIST_ERROR;
}

int32_t mz_get_file_crc(const char *path, uint32_t *result_crc)
{
    void *stream = NULL;
    void *crc32_stream = NULL;
    int32_t read = 0;
    int32_t err = MZ_OK;
    uint8_t buf[INT16_MAX];

    mz_stream_os_create(&stream);

    err = mz_stream_os_open(stream, path, MZ_OPEN_MODE_READ);

    mz_stream_crc32_create(&crc32_stream);
#ifdef HAVE_ZLIB
    mz_stream_crc32_set_update_func(crc32_stream,
        (mz_stream_crc32_update)mz_stream_zlib_get_crc32_update());
#elif defined(HAVE_LZMA)
    mz_stream_crc32_set_update_func(crc32_stream,
        (mz_stream_crc32_update)mz_stream_lzma_get_crc32_update());
#else
    #error ZLIB or LZMA required for CRC32
#endif

    mz_stream_crc32_open(crc32_stream, NULL, MZ_OPEN_MODE_READ);

    mz_stream_set_base(crc32_stream, stream);

    if (err == MZ_OK)
    {
        do
        {
            read = mz_stream_crc32_read(crc32_stream, buf, sizeof(buf));

            if (read < 0)
            {
                err = read;
                break;
            }
        }
        while ((err == MZ_OK) && (read > 0));

        mz_stream_os_close(stream);
    }

    mz_stream_crc32_close(crc32_stream);
    *result_crc = mz_stream_crc32_get_value(crc32_stream);
    mz_stream_crc32_delete(&crc32_stream);

    mz_stream_os_delete(&stream);

    return err;
}
