/*
 * Unit tests for mmio APIs
 *
 * Copyright 2005 Dmitry Timoshkov
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "mmsystem.h"
#include "vfw.h"
#include "wine/test.h"

static DWORD RIFF_buf[] =
{
    FOURCC_RIFF, 32*sizeof(DWORD), mmioFOURCC('A','V','I',' '),
    FOURCC_LIST, 29*sizeof(DWORD), listtypeAVIHEADER, ckidAVIMAINHDR,
    sizeof(MainAVIHeader), 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef,0xdeadbeef,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef,0xdeadbeef,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
    FOURCC_LIST, 10*sizeof(DWORD),listtypeSTREAMHEADER, ckidSTREAMHEADER,
    7*sizeof(DWORD), streamtypeVIDEO, 0xdeadbeef, 0xdeadbeef,
    0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void expect_buf_offset_dbg(HMMIO hmmio, LONG off, int line)
{
    MMIOINFO mmio;
    LONG ret;

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok_(__FILE__, line)(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok_(__FILE__, line)(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok_(__FILE__, line)(ret == off, "expected %d, got %d\n", off, ret);
}

#define expect_buf_offset(a1, a2) expect_buf_offset_dbg(a1, a2, __LINE__)

static void test_mmioDescend(char *fname)
{
    MMRESULT ret;
    HMMIO hmmio;
    MMIOINFO mmio;
    MMCKINFO ckRiff, ckList, ck, ckList2;

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = sizeof(RIFF_buf);
    mmio.pchBuffer = (char *)RIFF_buf;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    if (fname && !hmmio)
    {
        trace("No optional %s file. Skipping the test\n", fname);
        return;
    }
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    expect_buf_offset(hmmio, 0);

    /* first normal RIFF AVI parsing */
    ret = mmioDescend(hmmio, &ckRiff, NULL, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ckRiff.ckid == FOURCC_RIFF, "wrong ckid: %04x\n", ckRiff.ckid);
    ok(ckRiff.fccType == formtypeAVI, "wrong fccType: %04x\n", ckRiff.fccType);
    ok(ckRiff.dwDataOffset == 8, "expected 8 got %u\n", ckRiff.dwDataOffset);
    trace("ckid %4.4s cksize %04x fccType %4.4s off %04x flags %04x\n",
          (LPCSTR)&ckRiff.ckid, ckRiff.cksize, (LPCSTR)&ckRiff.fccType,
          ckRiff.dwDataOffset, ckRiff.dwFlags);

    expect_buf_offset(hmmio, 12);

    ret = mmioDescend(hmmio, &ckList, &ckRiff, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ckList.ckid == FOURCC_LIST, "wrong ckid: %04x\n", ckList.ckid);
    ok(ckList.fccType == listtypeAVIHEADER, "wrong fccType: %04x\n", ckList.fccType);
    ok(ckList.dwDataOffset == 20, "expected 20 got %u\n", ckList.dwDataOffset);
    trace("ckid %4.4s cksize %04x fccType %4.4s off %04x flags %04x\n",
          (LPCSTR)&ckList.ckid, ckList.cksize, (LPCSTR)&ckList.fccType,
          ckList.dwDataOffset, ckList.dwFlags);

    expect_buf_offset(hmmio, 24);

    ret = mmioDescend(hmmio, &ck, &ckList, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == ckidAVIMAINHDR, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == 0, "wrong fccType: %04x\n", ck.fccType);
    trace("ckid %4.4s cksize %04x fccType %4.4s off %04x flags %04x\n",
          (LPCSTR)&ck.ckid, ck.cksize, (LPCSTR)&ck.fccType,
          ck.dwDataOffset, ck.dwFlags);

    expect_buf_offset(hmmio, 32);

    /* Skip chunk data */
    ret = mmioSeek(hmmio, ck.cksize, SEEK_CUR);
    ok(ret == 0x58, "expected 0x58, got %#x\n", ret);

    ret = mmioDescend(hmmio, &ckList2, &ckList, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ckList2.ckid == FOURCC_LIST, "wrong ckid: %04x\n", ckList2.ckid);
    ok(ckList2.fccType == listtypeSTREAMHEADER, "wrong fccType: %04x\n", ckList2.fccType);
    trace("ckid %4.4s cksize %04x fccType %4.4s off %04x flags %04x\n",
          (LPCSTR)&ckList2.ckid, ckList2.cksize, (LPCSTR)&ckList2.fccType,
          ckList2.dwDataOffset, ckList2.dwFlags);

    expect_buf_offset(hmmio, 100);

    ret = mmioDescend(hmmio, &ck, &ckList2, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == ckidSTREAMHEADER, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == 0, "wrong fccType: %04x\n", ck.fccType);
    trace("ckid %4.4s cksize %04x fccType %4.4s off %04x flags %04x\n",
          (LPCSTR)&ck.ckid, ck.cksize, (LPCSTR)&ck.fccType,
          ck.dwDataOffset, ck.dwFlags);

    expect_buf_offset(hmmio, 108);

    /* test various mmioDescend flags */

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDRIFF);
    ok(ret == MMIOERR_CHUNKNOTFOUND ||
       ret == MMIOERR_INVALIDFILE, "mmioDescend returned %u\n", ret);

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ck.ckid = 0;
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDRIFF);
    ok(ret == MMIOERR_CHUNKNOTFOUND ||
       ret == MMIOERR_INVALIDFILE, "mmioDescend returned %u\n", ret);

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ck.fccType = 0;
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDRIFF);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == FOURCC_RIFF, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == formtypeAVI, "wrong fccType: %04x\n", ck.fccType);

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ret = mmioDescend(hmmio, &ck, NULL, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == FOURCC_RIFF, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == formtypeAVI, "wrong fccType: %04x\n", ck.fccType);

    /* do NOT seek, use current file position */
    memset(&ck, 0x66, sizeof(ck));
    ck.fccType = 0;
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDLIST);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == FOURCC_LIST, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == listtypeAVIHEADER, "wrong fccType: %04x\n", ck.fccType);

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ck.ckid = 0;
    ck.fccType = listtypeAVIHEADER;
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDCHUNK);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == FOURCC_RIFF, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == formtypeAVI, "wrong fccType: %04x\n", ck.fccType);

    /* do NOT seek, use current file position */
    memset(&ck, 0x66, sizeof(ck));
    ck.ckid = FOURCC_LIST;
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDCHUNK);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == FOURCC_LIST, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == listtypeAVIHEADER, "wrong fccType: %04x\n", ck.fccType);

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ck.ckid = FOURCC_RIFF;
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDCHUNK);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ck.ckid == FOURCC_RIFF, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType == formtypeAVI, "wrong fccType: %04x\n", ck.fccType);

    /* do NOT seek, use current file position */
    memset(&ckList, 0x66, sizeof(ckList));
    ckList.ckid = 0;
    ret = mmioDescend(hmmio, &ckList, &ck, MMIO_FINDCHUNK);
    ok(ret == MMSYSERR_NOERROR, "mmioDescend error %u\n", ret);
    ok(ckList.ckid == FOURCC_LIST, "wrong ckid: %04x\n", ckList.ckid);
    ok(ckList.fccType == listtypeAVIHEADER, "wrong fccType: %04x\n", ckList.fccType);

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDCHUNK);
    ok(ret == MMIOERR_CHUNKNOTFOUND ||
       ret == MMIOERR_INVALIDFILE, "mmioDescend returned %u\n", ret);
    ok(ck.ckid != 0x66666666, "wrong ckid: %04x\n", ck.ckid);
    ok(ck.fccType != 0x66666666, "wrong fccType: %04x\n", ck.fccType);
    ok(ck.dwDataOffset != 0x66666666, "wrong dwDataOffset: %04x\n", ck.dwDataOffset);

    mmioSeek(hmmio, 0, SEEK_SET);
    memset(&ck, 0x66, sizeof(ck));
    ret = mmioDescend(hmmio, &ck, NULL, MMIO_FINDRIFF);
    ok(ret == MMIOERR_CHUNKNOTFOUND ||
       ret == MMIOERR_INVALIDFILE, "mmioDescend returned %u\n", ret);

    mmioClose(hmmio, 0);
}

static void test_mmioOpen(char *fname)
{
    char buf[MMIO_DEFAULTBUFFER];
    MMRESULT ret;
    HMMIO hmmio;
    MMIOINFO mmio;

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = sizeof(buf);
    mmio.pchBuffer = buf;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    if (fname && !hmmio)
    {
        trace("No optional %s file. Skipping the test\n", fname);
        return;
    }
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == sizeof(buf), "got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer == buf, "expected %p, got %p\n", buf, mmio.pchBuffer);
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    if (mmio.fccIOProc == FOURCC_DOS)
        ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    else
        ok(mmio.pchEndRead == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);

    mmioClose(hmmio, 0);

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = 0;
    mmio.pchBuffer = buf;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 0, "expected 0, got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer == buf, "expected %p, got %p\n", buf, mmio.pchBuffer);
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);

    mmioClose(hmmio, 0);

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = 0;
    mmio.pchBuffer = NULL;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 0, "expected 0, got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer == NULL, "expected NULL\n");
    ok(mmio.pchNext == NULL, "expected NULL\n");
    ok(mmio.pchEndRead == NULL, "expected NULL\n");
    ok(mmio.pchEndWrite == NULL, "expected NULL\n");
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    mmioClose(hmmio, 0);

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = 256;
    mmio.pchBuffer = NULL;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == (MMIO_READ|MMIO_ALLOCBUF), "expected MMIO_READ|MMIO_ALLOCBUF, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 256, "expected 256, got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer != NULL, "expected not NULL\n");
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    if (mmio.fccIOProc == FOURCC_DOS)
        ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    else
        ok(mmio.pchEndRead == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    mmioClose(hmmio, 0);

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = sizeof(buf);
    mmio.pchBuffer = buf;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ | MMIO_ALLOCBUF);
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == sizeof(buf), "got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer == buf, "expected %p, got %p\n", buf, mmio.pchBuffer);
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    if (mmio.fccIOProc == FOURCC_DOS)
        ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    else
        ok(mmio.pchEndRead == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);

    mmioClose(hmmio, 0);

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = 0;
    mmio.pchBuffer = NULL;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ | MMIO_ALLOCBUF);
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == (MMIO_READ|MMIO_ALLOCBUF), "expected MMIO_READ|MMIO_ALLOCBUF, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == MMIO_DEFAULTBUFFER, "expected MMIO_DEFAULTBUFFER, got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer != NULL, "expected not NULL\n");
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    if (mmio.fccIOProc == FOURCC_DOS)
        ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    else
        ok(mmio.pchEndRead == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    mmioClose(hmmio, 0);

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = 256;
    mmio.pchBuffer = NULL;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ | MMIO_ALLOCBUF);
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == (MMIO_READ|MMIO_ALLOCBUF), "expected MMIO_READ|MMIO_ALLOCBUF, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 256, "expected 256, got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer != NULL, "expected not NULL\n");
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    if (mmio.fccIOProc == FOURCC_DOS)
        ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    else
        ok(mmio.pchEndRead == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    mmioClose(hmmio, 0);

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = 0;
    mmio.pchBuffer = buf;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ | MMIO_ALLOCBUF);
    if (!hmmio && mmio.wErrorRet == ERROR_BAD_FORMAT)
    {
        /* Seen on Win9x, WinMe but also XP-SP1 */
        skip("Some Windows versions don't like a 0 size and a given buffer\n");
        return;
    }
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == MMIO_DEFAULTBUFFER, "expected MMIO_DEFAULTBUFFER, got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer == buf, "expected %p, got %p\n", buf, mmio.pchBuffer);
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    if (mmio.fccIOProc == FOURCC_DOS)
        ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    else
        ok(mmio.pchEndRead == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);

    mmioClose(hmmio, 0);
}

static void test_mmioSetBuffer(char *fname)
{
    char buf[256];
    MMRESULT ret;
    HMMIO hmmio;
    MMIOINFO mmio;

    memset(&mmio, 0, sizeof(mmio));
    mmio.fccIOProc = fname ? FOURCC_DOS : FOURCC_MEM;
    mmio.cchBuffer = sizeof(buf);
    mmio.pchBuffer = buf;
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    if (fname && !hmmio)
    {
        trace("No optional %s file. Skipping the test\n", fname);
        return;
    }
    ok(hmmio != 0, "mmioOpen error %u\n", mmio.wErrorRet);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == sizeof(buf), "got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer == buf, "expected %p, got %p\n", buf, mmio.pchBuffer);
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    if (mmio.fccIOProc == FOURCC_DOS)
        ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    else
        ok(mmio.pchEndRead == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);

    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);

    ret = mmioSetBuffer(hmmio, NULL, 0, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioSetBuffer error %u\n", ret);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 0, "got not 0\n");
    ok(mmio.pchBuffer == NULL, "got not NULL buf\n");
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndWrite);
#if 0 /* remove once passes under Wine */
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);
#endif

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    ret = mmioSetBuffer(hmmio, NULL, 0, MMIO_ALLOCBUF);
    ok(ret == MMSYSERR_NOERROR, "mmioSetBuffer error %u\n", ret);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 0, "got not 0\n");
    ok(mmio.pchBuffer == NULL, "got not NULL buf\n");
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndWrite);
#if 0 /* remove once passes under Wine */
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);
#endif

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    ret = mmioSetBuffer(hmmio, buf, 0, MMIO_ALLOCBUF);
    ok(ret == MMSYSERR_NOERROR, "mmioSetBuffer error %u\n", ret);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == MMIO_READ, "expected MMIO_READ, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 0, "got not 0\n");
    ok(mmio.pchBuffer == buf, "expected %p, got %p\n", buf, mmio.pchBuffer);
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchEndWrite);
#if 0 /* remove once passes under Wine */
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);
#endif

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    ret = mmioSetBuffer(hmmio, NULL, 256, MMIO_WRITE|MMIO_ALLOCBUF);
    ok(ret == MMSYSERR_NOERROR, "mmioSetBuffer error %u\n", ret);

    memset(&mmio, 0, sizeof(mmio));
    ret = mmioGetInfo(hmmio, &mmio, 0);
    ok(ret == MMSYSERR_NOERROR, "mmioGetInfo error %u\n", ret);
    ok(mmio.dwFlags == (MMIO_READ|MMIO_ALLOCBUF), "expected MMIO_READ|MMIO_ALLOCBUF, got %x\n", mmio.dwFlags);
    ok(mmio.wErrorRet == MMSYSERR_NOERROR, "expected MMSYSERR_NOERROR, got %u\n", mmio.wErrorRet);
    ok(mmio.fccIOProc == (fname ? FOURCC_DOS : FOURCC_MEM), "got %4.4s\n", (LPCSTR)&mmio.fccIOProc);
    ok(mmio.cchBuffer == 256, "got %u\n", mmio.cchBuffer);
    ok(mmio.pchBuffer != NULL, "expected not NULL\n");
    ok(mmio.pchBuffer != buf, "expected != buf\n");
    ok(mmio.pchNext == mmio.pchBuffer, "expected %p, got %p\n", mmio.pchBuffer, mmio.pchNext);
    ok(mmio.pchEndRead == mmio.pchBuffer, "expected %p, got %p\n", buf, mmio.pchEndRead);
    ok(mmio.pchEndWrite == mmio.pchBuffer + mmio.cchBuffer, "expected %p + %d, got %p\n", mmio.pchBuffer, mmio.cchBuffer, mmio.pchEndWrite);
#if 0 /* remove once passes under Wine */
    ok(mmio.lBufOffset == 0, "expected 0, got %d\n", mmio.lBufOffset);
    ok(mmio.lDiskOffset == 0, "expected 0, got %d\n", mmio.lDiskOffset);
#endif

#if 0 /* remove once passes under Wine */
    ret = mmioSeek(hmmio, 0, SEEK_CUR);
    ok(ret == 0, "expected 0, got %d\n", ret);
#endif

    mmioClose(hmmio, 0);
}

#define FOURCC_XYZ mmioFOURCC('X', 'Y', 'Z', ' ')

static LRESULT CALLBACK mmio_test_IOProc(LPSTR lpMMIOInfo, UINT uMessage, LPARAM lParam1, LPARAM lParam2)
{
    LPMMIOINFO lpInfo = (LPMMIOINFO) lpMMIOInfo;

    switch (uMessage)
    {
    case MMIOM_OPEN:
        if (lpInfo->fccIOProc == FOURCC_DOS)
            lpInfo->fccIOProc = mmioFOURCC('F', 'A', 'I', 'L');
        return MMSYSERR_NOERROR;
    case MMIOM_CLOSE:
        return MMSYSERR_NOERROR;
    default:
        return 0;
    }
}

static void test_mmioOpen_fourcc(void)
{
    char fname[] = "file+name.xyz+one.two";

    LPMMIOPROC lpProc;
    HMMIO hmmio;
    MMIOINFO mmio;

    lpProc = mmioInstallIOProc(FOURCC_DOS, mmio_test_IOProc, MMIO_INSTALLPROC);
    ok(lpProc == mmio_test_IOProc, "mmioInstallIOProc error\n");

    lpProc = mmioInstallIOProc(FOURCC_XYZ, mmio_test_IOProc, MMIO_INSTALLPROC);
    ok(lpProc == mmio_test_IOProc, "mmioInstallIOProc error\n");

    memset(&mmio, 0, sizeof(mmio));
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    mmioGetInfo(hmmio, &mmio, 0);
    ok(hmmio != NULL && mmio.fccIOProc == FOURCC_XYZ, "mmioOpen error %u, got %4.4s\n", mmio.wErrorRet, (LPCSTR)&mmio.fccIOProc);
    mmioClose(hmmio, 0);

    mmioInstallIOProc(FOURCC_XYZ, NULL, MMIO_REMOVEPROC);

    memset(&mmio, 0, sizeof(mmio));
    hmmio = mmioOpen(fname, &mmio, MMIO_READ);
    mmioGetInfo(hmmio, &mmio, 0);
    ok(hmmio == NULL && mmio.wErrorRet == MMIOERR_FILENOTFOUND, "mmioOpen error %u, got %4.4s\n", mmio.wErrorRet, (LPCSTR)&mmio.fccIOProc);
    mmioClose(hmmio, 0);

    mmioInstallIOProc(FOURCC_DOS, NULL, MMIO_REMOVEPROC);
}

START_TEST(mmio)
{
    /* Make it possible to run the tests against a specific AVI file in
     * addition to the builtin test data. This is mostly meant as a
     * debugging aid and is not part of the standard tests.
     */
    char fname[] = "msrle.avi";

    test_mmioDescend(NULL);
    test_mmioDescend(fname);
    test_mmioOpen(NULL);
    test_mmioOpen(fname);
    test_mmioSetBuffer(NULL);
    test_mmioSetBuffer(fname);
    test_mmioOpen_fourcc();
}
