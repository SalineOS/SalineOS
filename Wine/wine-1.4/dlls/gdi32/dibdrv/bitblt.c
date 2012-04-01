/*
 * DIB driver blitting
 *
 * Copyright 2011 Huw Davies
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

#include <assert.h>

#include "gdi_private.h"
#include "dibdrv.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dib);

#define DST 0   /* Destination dib */
#define SRC 1   /* Source dib */
#define TMP 2   /* Temporary dib */
#define PAT 3   /* Pattern (brush) in destination DC */

#define OP(src,dst,rop)   (OP_ARGS(src,dst) << 4 | ((rop) - 1))
#define OP_ARGS(src,dst)  (((src) << 2) | (dst))

#define OP_SRC(opcode)    ((opcode) >> 6)
#define OP_DST(opcode)    (((opcode) >> 4) & 3)
#define OP_SRCDST(opcode) ((opcode) >> 4)
#define OP_ROP(opcode)    (((opcode) & 0x0f) + 1)

#define MAX_OP_LEN  6  /* Longest opcode + 1 for the terminating 0 */

static const unsigned char BITBLT_Opcodes[256][MAX_OP_LEN] =
{
    { OP(PAT,DST,R2_BLACK) },                                       /* 0x00  0              */
    { OP(PAT,SRC,R2_MERGEPEN), OP(SRC,DST,R2_NOTMERGEPEN) },        /* 0x01  ~(D|(P|S))     */
    { OP(PAT,SRC,R2_NOTMERGEPEN), OP(SRC,DST,R2_MASKPEN) },         /* 0x02  D&~(P|S)       */
    { OP(PAT,SRC,R2_NOTMERGEPEN) },                                 /* 0x03  ~(P|S)         */
    { OP(PAT,DST,R2_NOTMERGEPEN), OP(SRC,DST,R2_MASKPEN) },         /* 0x04  S&~(D|P)       */
    { OP(PAT,DST,R2_NOTMERGEPEN) },                                 /* 0x05  ~(D|P)         */
    { OP(SRC,DST,R2_NOTXORPEN), OP(PAT,DST,R2_NOTMERGEPEN), },      /* 0x06  ~(P|~(D^S))    */
    { OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_NOTMERGEPEN) },         /* 0x07  ~(P|(D&S))     */
    { OP(PAT,DST,R2_MASKNOTPEN), OP(SRC,DST,R2_MASKPEN) },          /* 0x08  S&D&~P         */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_NOTMERGEPEN) },          /* 0x09  ~(P|(D^S))     */
    { OP(PAT,DST,R2_MASKNOTPEN) },                                  /* 0x0a  D&~P           */
    { OP(SRC,DST,R2_MASKPENNOT), OP(PAT,DST,R2_NOTMERGEPEN) },      /* 0x0b  ~(P|(S&~D))    */
    { OP(PAT,SRC,R2_MASKNOTPEN) },                                  /* 0x0c  S&~P           */
    { OP(SRC,DST,R2_MASKNOTPEN), OP(PAT,DST,R2_NOTMERGEPEN) },      /* 0x0d  ~(P|(D&~S))    */
    { OP(SRC,DST,R2_NOTMERGEPEN), OP(PAT,DST,R2_NOTMERGEPEN) },     /* 0x0e  ~(P|~(D|S))    */
    { OP(PAT,DST,R2_NOTCOPYPEN) },                                  /* 0x0f  ~P             */
    { OP(SRC,DST,R2_NOTMERGEPEN), OP(PAT,DST,R2_MASKPEN) },         /* 0x10  P&~(S|D)       */
    { OP(SRC,DST,R2_NOTMERGEPEN) },                                 /* 0x11  ~(D|S)         */
    { OP(PAT,DST,R2_NOTXORPEN), OP(SRC,DST,R2_NOTMERGEPEN) },       /* 0x12  ~(S|~(D^P))    */
    { OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_NOTMERGEPEN) },         /* 0x13  ~(S|(D&P))     */
    { OP(PAT,SRC,R2_NOTXORPEN), OP(SRC,DST,R2_NOTMERGEPEN) },       /* 0x14  ~(D|~(P^S))    */
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_NOTMERGEPEN) },         /* 0x15  ~(D|(P&S))     */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_NOTMASKPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0x16  P^S^(D&~(P&S)  */
    { OP(SRC,TMP,R2_COPYPEN), OP(SRC,DST,R2_XORPEN),
      OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0x17 ~S^((S^P)&(S^D))*/
    { OP(PAT,SRC,R2_XORPEN), OP(PAT,DST,R2_XORPEN),
        OP(SRC,DST,R2_MASKPEN) },                                   /* 0x18  (S^P)&(D^P)    */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_NOTMASKPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_NOTXORPEN) },           /* 0x19  ~S^(D&~(P&S))  */
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0x1a  P^(D|(S&P))    */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_NOTXORPEN) },           /* 0x1b  ~S^(D&(P^S))   */
    { OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0x1c  P^(S|(D&P))    */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_XORPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_NOTXORPEN) },           /* 0x1d  ~D^(S&(D^P))   */
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_XORPEN) },             /* 0x1e  P^(D|S)        */
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_NOTMASKPEN) },         /* 0x1f  ~(P&(D|S))     */
    { OP(PAT,SRC,R2_MASKPENNOT), OP(SRC,DST,R2_MASKPEN) },          /* 0x20  D&(P&~S)       */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_NOTMERGEPEN) },          /* 0x21  ~(S|(D^P))     */
    { OP(SRC,DST,R2_MASKNOTPEN) },                                  /* 0x22  ~S&D           */
    { OP(PAT,DST,R2_MASKPENNOT), OP(SRC,DST,R2_NOTMERGEPEN) },      /* 0x23  ~(S|(P&~D))    */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_MASKPEN) },                                     /* 0x24   (S^P)&(S^D)   */
    { OP(PAT,SRC,R2_NOTMASKPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0x25  ~P^(D&~(S&P))  */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN) },             /* 0x26  S^(D|(S&P))    */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_NOTXORPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN) },             /* 0x27  S^(D|~(P^S))   */
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MASKPEN) },              /* 0x28  D&(P^S)        */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0x29  ~P^S^(D|(P&S)) */
    { OP(PAT,SRC,R2_NOTMASKPEN), OP(SRC,DST,R2_MASKPEN) },          /* 0x2a  D&~(P&S)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_XORPEN),
      OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0x2b ~S^((P^S)&(P^D))*/
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x2c  S^(P&(S|D))    */
    { OP(SRC,DST,R2_MERGEPENNOT), OP(PAT,DST,R2_XORPEN) },          /* 0x2d  P^(S|~D)       */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0x2e  P^(S|(D^P))    */
    { OP(SRC,DST,R2_MERGEPENNOT), OP(PAT,DST,R2_NOTMASKPEN) },      /* 0x2f  ~(P&(S|~D))    */
    { OP(PAT,SRC,R2_MASKPENNOT) },                                  /* 0x30  P&~S           */
    { OP(PAT,DST,R2_MASKNOTPEN), OP(SRC,DST,R2_NOTMERGEPEN) },      /* 0x31  ~(S|(D&~P))    */
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x32  S^(D|P|S)      */
    { OP(SRC,DST,R2_NOTCOPYPEN) },                                  /* 0x33  ~S             */
    { OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x34  S^(P|(D&S))    */
    { OP(SRC,DST,R2_NOTXORPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x35  S^(P|~(D^S))   */
    { OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_XORPEN) },             /* 0x36  S^(D|P)        */
    { OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_NOTMASKPEN) },         /* 0x37  ~(S&(D|P))     */
    { OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0x38  P^(S&(D|P))    */
    { OP(PAT,DST,R2_MERGEPENNOT), OP(SRC,DST,R2_XORPEN) },          /* 0x39  S^(P|~D)       */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x3a  S^(P|(D^S))    */
    { OP(PAT,DST,R2_MERGEPENNOT), OP(SRC,DST,R2_NOTMASKPEN) },      /* 0x3b  ~(S&(P|~D))    */
    { OP(PAT,SRC,R2_XORPEN) },                                      /* 0x3c  P^S            */
    { OP(SRC,DST,R2_NOTMERGEPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x3d  S^(P|~(D|S))   */
    { OP(SRC,DST,R2_MASKNOTPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x3e  S^(P|(D&~S))   */
    { OP(PAT,SRC,R2_NOTMASKPEN) },                                  /* 0x3f  ~(P&S)         */
    { OP(SRC,DST,R2_MASKPENNOT), OP(PAT,DST,R2_MASKPEN) },          /* 0x40  P&S&~D         */
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_NOTMERGEPEN) },          /* 0x41  ~(D|(P^S))     */
    { OP(DST,SRC,R2_XORPEN), OP(PAT,DST,R2_XORPEN),
      OP(SRC,DST,R2_MASKPEN) },                                     /* 0x42  (S^D)&(P^D)    */
    { OP(SRC,DST,R2_NOTMASKPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0x43  ~S^(P&~(D&S))  */
    { OP(SRC,DST,R2_MASKPENNOT) },                                  /* 0x44  S&~D           */
    { OP(PAT,SRC,R2_MASKPENNOT), OP(SRC,DST,R2_NOTMERGEPEN) },      /* 0x45  ~(D|(P&~S))    */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN) },             /* 0x46  D^(S|(P&D))    */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0x47  ~P^(S&(D^P))   */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MASKPEN) },              /* 0x48  S&(P^D)        */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0x49  ~P^D^(S|(P&D)) */
    { OP(DST,SRC,R2_MERGEPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x4a  D^(P&(S|D))    */
    { OP(SRC,DST,R2_MERGENOTPEN), OP(PAT,DST,R2_XORPEN) },          /* 0x4b  P^(D|~S)       */
    { OP(PAT,DST,R2_NOTMASKPEN), OP(SRC,DST,R2_MASKPEN) },          /* 0x4c  S&~(D&P)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(SRC,DST,R2_XORPEN),
      OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0x4d ~S^((S^P)|(S^D))*/
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0x4e  P^(D|(S^P))    */
    { OP(SRC,DST,R2_MERGENOTPEN), OP(PAT,DST,R2_NOTMASKPEN) },      /* 0x4f  ~(P&(D|~S))    */
    { OP(PAT,DST,R2_MASKPENNOT) },                                  /* 0x50  P&~D           */
    { OP(PAT,SRC,R2_MASKNOTPEN), OP(SRC,DST,R2_NOTMERGEPEN) },      /* 0x51  ~(D|(S&~P))    */
    { OP(DST,SRC,R2_MASKPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x52  D^(P|(S&D))    */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0x53  ~S^(P&(D^S))   */
    { OP(PAT,SRC,R2_NOTMERGEPEN), OP(SRC,DST,R2_NOTMERGEPEN) },     /* 0x54  ~(D|~(P|S))    */
    { OP(PAT,DST,R2_NOT) },                                         /* 0x55  ~D             */
    { OP(PAT,SRC,R2_MERGEPEN), OP(SRC,DST,R2_XORPEN) },             /* 0x56  D^(P|S)        */
    { OP(PAT,SRC,R2_MERGEPEN), OP(SRC,DST,R2_NOTMASKPEN) },         /* 0x57  ~(D&(P|S))     */
    { OP(PAT,SRC,R2_MERGEPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0x58  P^(D&(P|S))    */
    { OP(PAT,SRC,R2_MERGEPENNOT), OP(SRC,DST,R2_XORPEN) },          /* 0x59  D^(P|~S)       */
    { OP(PAT,DST,R2_XORPEN) },                                      /* 0x5a  D^P            */
    { OP(DST,SRC,R2_NOTMERGEPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x5b  D^(P|~(S|D))   */
    { OP(DST,SRC,R2_XORPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x5c  D^(P|(S^D))    */
    { OP(PAT,SRC,R2_MERGEPENNOT), OP(SRC,DST,R2_NOTMASKPEN) },      /* 0x5d  ~(D&(P|~S))    */
    { OP(DST,SRC,R2_MASKNOTPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x5e  D^(P|(S&~D))   */
    { OP(PAT,DST,R2_NOTMASKPEN) },                                  /* 0x5f  ~(D&P)         */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_MASKPEN) },              /* 0x60  P&(D^S)        */
    { OP(DST,TMP,R2_COPYPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_XORPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0x61  ~D^S^(P|(D&S)) */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN) },              /* 0x62  D^(S&(P|D))    */
    { OP(PAT,DST,R2_MERGENOTPEN), OP(SRC,DST,R2_XORPEN) },          /* 0x63  S^(D|~P)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN) },              /* 0x64  S^(D&(P|S))    */
    { OP(PAT,SRC,R2_MERGENOTPEN), OP(SRC,DST,R2_XORPEN) },          /* 0x65  D^(S|~P)       */
    { OP(SRC,DST,R2_XORPEN) },                                      /* 0x66  S^D            */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_NOTMERGEPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN) },             /* 0x67  S^(D|~(S|P)    */
    { OP(DST,TMP,R2_COPYPEN), OP(SRC,DST,R2_NOTMERGEPEN),
      OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_XORPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0x68  ~D^S^(P|~(D|S))*/
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_NOTXORPEN) },            /* 0x69  ~P^(D^S)       */
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_XORPEN) },              /* 0x6a  D^(P&S)        */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0x6b  ~P^S^(D&(P|S)) */
    { OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_XORPEN) },              /* 0x6c  S^(D&P)        */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0x6d  ~P^D^(S&(P|D)) */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MERGEPENNOT),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN) },              /* 0x6e  S^(D&(P|~S))   */
    { OP(SRC,DST,R2_NOTXORPEN), OP(PAT,DST,R2_NOTMASKPEN) },        /* 0x6f  ~(P&~(S^D))    */
    { OP(SRC,DST,R2_NOTMASKPEN), OP(PAT,DST,R2_MASKPEN) },          /* 0x70  P&~(D&S)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(DST,SRC,R2_XORPEN),
      OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0x71 ~S^((S^D)&(P^D))*/
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN) },             /* 0x72  S^(D|(P^S))    */
    { OP(PAT,DST,R2_MERGENOTPEN), OP(SRC,DST,R2_NOTMASKPEN) },      /* 0x73  ~(S&(D|~P))    */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_XORPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN) },             /* 0x74   D^(S|(P^D))   */
    { OP(PAT,SRC,R2_MERGENOTPEN), OP(SRC,DST,R2_NOTMASKPEN) },      /* 0x75  ~(D&(S|~P))    */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MASKPENNOT),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_XORPEN) },             /* 0x76  S^(D|(P&~S))   */
    { OP(SRC,DST,R2_NOTMASKPEN) },                                  /* 0x77  ~(S&D)         */
    { OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_XORPEN) },              /* 0x78  P^(D&S)        */
    { OP(DST,TMP,R2_COPYPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_XORPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0x79  ~D^S^(P&(D|S)) */
    { OP(DST,SRC,R2_MERGENOTPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x7a  D^(P&(S|~D))   */
    { OP(PAT,DST,R2_NOTXORPEN), OP(SRC,DST,R2_NOTMASKPEN) },        /* 0x7b  ~(S&~(D^P))    */
    { OP(SRC,DST,R2_MERGENOTPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0x7c  S^(P&(D|~S))   */
    { OP(PAT,SRC,R2_NOTXORPEN), OP(SRC,DST,R2_NOTMASKPEN) },        /* 0x7d  ~(D&~(P^S))    */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_MERGEPEN) },                                    /* 0x7e  (S^P)|(S^D)    */
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_NOTMASKPEN) },          /* 0x7f  ~(D&P&S)       */
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_MASKPEN) },             /* 0x80  D&P&S          */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_NOTMERGEPEN) },                                 /* 0x81  ~((S^P)|(S^D)) */
    { OP(PAT,SRC,R2_NOTXORPEN), OP(SRC,DST,R2_MASKPEN) },           /* 0x82  D&~(P^S)       */
    { OP(SRC,DST,R2_MERGENOTPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0x83  ~S^(P&(D|~S))  */
    { OP(PAT,DST,R2_NOTXORPEN), OP(SRC,DST,R2_MASKPEN) },           /* 0x84  S&~(D^P)       */
    { OP(PAT,SRC,R2_MERGENOTPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0x85  ~P^(D&(S|~P))  */
    { OP(DST,TMP,R2_COPYPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_XORPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0x86  D^S^(P&(D|S))  */
    { OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_NOTXORPEN) },           /* 0x87  ~P^(D&S)       */
    { OP(SRC,DST,R2_MASKPEN) },                                     /* 0x88  S&D            */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MASKPENNOT),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_NOTXORPEN) },          /* 0x89  ~S^(D|(P&~S))  */
    { OP(PAT,SRC,R2_MERGENOTPEN), OP(SRC,DST,R2_MASKPEN) },         /* 0x8a  D&(S|~P)       */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_XORPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_NOTXORPEN) },          /* 0x8b  ~D^(S|(P^D))   */
    { OP(PAT,DST,R2_MERGENOTPEN), OP(SRC,DST,R2_MASKPEN) },         /* 0x8c  S&(D|~P)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_NOTXORPEN) },          /* 0x8d  ~S^(D|(P^S))   */
    { OP(SRC,TMP,R2_COPYPEN), OP(DST,SRC,R2_XORPEN),
      OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0x8e  S^((S^D)&(P^D))*/
    { OP(SRC,DST,R2_NOTMASKPEN), OP(PAT,DST,R2_NOTMASKPEN) },       /* 0x8f  ~(P&~(D&S))    */
    { OP(SRC,DST,R2_NOTXORPEN), OP(PAT,DST,R2_MASKPEN) },           /* 0x90  P&~(D^S)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MERGEPENNOT),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_NOTXORPEN) },           /* 0x91  ~S^(D&(P|~S))  */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_XORPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0x92  D^P^(S&(D|P))  */
    { OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_NOTXORPEN) },           /* 0x93  ~S^(P&D)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_XORPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0x94  S^P^(D&(P|S))  */
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_NOTXORPEN) },           /* 0x95  ~D^(P&S)       */
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_XORPEN) },               /* 0x96  D^P^S          */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_NOTMERGEPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_XORPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0x97  S^P^(D|~(P|S)) */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_NOTMERGEPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_NOTXORPEN) },          /* 0x98  ~S^(D|~(P|S))  */
    { OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0x99  ~S^D           */
    { OP(PAT,SRC,R2_MASKPENNOT), OP(SRC,DST,R2_XORPEN) },           /* 0x9a  D^(P&~S)       */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_NOTXORPEN) },           /* 0x9b  ~S^(D&(P|S))   */
    { OP(PAT,DST,R2_MASKPENNOT), OP(SRC,DST,R2_XORPEN) },           /* 0x9c  S^(P&~D)       */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_NOTXORPEN) },           /* 0x9d  ~D^(S&(P|D))   */
    { OP(DST,TMP,R2_COPYPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_XORPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0x9e  D^S^(P|(D&S))  */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_NOTMASKPEN) },           /* 0x9f  ~(P&(D^S))     */
    { OP(PAT,DST,R2_MASKPEN) },                                     /* 0xa0  D&P            */
    { OP(PAT,SRC,R2_MASKNOTPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xa1  ~P^(D|(S&~P))  */
    { OP(PAT,SRC,R2_MERGEPENNOT), OP(SRC,DST,R2_MASKPEN) },         /* 0xa2  D&(P|~S)       */
    { OP(DST,SRC,R2_XORPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xa3  ~D^(P|(S^D))   */
    { OP(PAT,SRC,R2_NOTMERGEPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xa4  ~P^(D|~(S|P))  */
    { OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xa5  ~P^D           */
    { OP(PAT,SRC,R2_MASKNOTPEN), OP(SRC,DST,R2_XORPEN) },           /* 0xa6  D^(S&~P)       */
    { OP(PAT,SRC,R2_MERGEPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xa7  ~P^(D&(S|P))   */
    { OP(PAT,SRC,R2_MERGEPEN), OP(SRC,DST,R2_MASKPEN) },            /* 0xa8  D&(P|S)        */
    { OP(PAT,SRC,R2_MERGEPEN), OP(SRC,DST,R2_NOTXORPEN) },          /* 0xa9  ~D^(P|S)       */
    { OP(PAT,DST,R2_NOP) },                                         /* 0xaa  D              */
    { OP(PAT,SRC,R2_NOTMERGEPEN), OP(SRC,DST,R2_MERGEPEN) },        /* 0xab  D|~(P|S)       */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0xac  S^(P&(D^S))    */
    { OP(DST,SRC,R2_MASKPEN), OP(PAT,SRC,R2_MERGEPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xad  ~D^(P|(S&D))   */
    { OP(PAT,SRC,R2_MASKNOTPEN), OP(SRC,DST,R2_MERGEPEN) },         /* 0xae  D|(S&~P)       */
    { OP(PAT,DST,R2_MERGENOTPEN) },                                 /* 0xaf  D|~P           */
    { OP(SRC,DST,R2_MERGENOTPEN), OP(PAT,DST,R2_MASKPEN) },         /* 0xb0  P&(D|~S)       */
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xb1  ~P^(D|(S^P))   */
    { OP(SRC,TMP,R2_COPYPEN), OP(SRC,DST,R2_XORPEN),
      OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0xb2  S^((S^P)|(S^D))*/
    { OP(PAT,DST,R2_NOTMASKPEN), OP(SRC,DST,R2_NOTMASKPEN) },       /* 0xb3  ~(S&~(D&P))    */
    { OP(SRC,DST,R2_MASKPENNOT), OP(PAT,DST,R2_XORPEN) },           /* 0xb4  P^(S&~D)       */
    { OP(DST,SRC,R2_MERGEPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xb5  ~D^(P&(S|D))   */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_XORPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0xb6  D^P^(S|(D&P))  */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_NOTMASKPEN) },           /* 0xb7  ~(S&(D^P))     */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0xb8  P^(S&(D^P))    */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_NOTXORPEN) },          /* 0xb9  ~D^(S|(P&D))   */
    { OP(PAT,SRC,R2_MASKPENNOT), OP(SRC,DST,R2_MERGEPEN) },         /* 0xba  D|(P&~S)       */
    { OP(SRC,DST,R2_MERGENOTPEN) },                                 /* 0xbb  ~S|D           */
    { OP(SRC,DST,R2_NOTMASKPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0xbc  S^(P&~(D&S))   */
    { OP(DST,SRC,R2_XORPEN), OP(PAT,DST,R2_XORPEN),
      OP(SRC,DST,R2_NOTMASKPEN) },                                  /* 0xbd  ~((S^D)&(P^D)) */
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN) },             /* 0xbe  D|(P^S)        */
    { OP(PAT,SRC,R2_NOTMASKPEN), OP(SRC,DST,R2_MERGEPEN) },         /* 0xbf  D|~(P&S)       */
    { OP(PAT,SRC,R2_MASKPEN) },                                     /* 0xc0  P&S            */
    { OP(SRC,DST,R2_MASKNOTPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xc1  ~S^(P|(D&~S))  */
    { OP(SRC,DST,R2_NOTMERGEPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xc2  ~S^(P|~(D|S))  */
    { OP(PAT,SRC,R2_NOTXORPEN) },                                   /* 0xc3  ~P^S           */
    { OP(PAT,DST,R2_MERGEPENNOT), OP(SRC,DST,R2_MASKPEN) },         /* 0xc4  S&(P|~D)       */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xc5  ~S^(P|(D^S))   */
    { OP(PAT,DST,R2_MASKNOTPEN), OP(SRC,DST,R2_XORPEN) },           /* 0xc6  S^(D&~P)       */
    { OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xc7  ~P^(S&(D|P))   */
    { OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_MASKPEN) },            /* 0xc8  S&(D|P)        */
    { OP(PAT,DST,R2_MERGEPEN), OP(SRC,DST,R2_NOTXORPEN) },          /* 0xc9  ~S^(P|D)       */
    { OP(DST,SRC,R2_XORPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0xca  D^(P&(S^D))    */
    { OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_MERGEPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xcb  ~S^(P|(D&S))   */
    { OP(SRC,DST,R2_COPYPEN) },                                     /* 0xcc  S              */
    { OP(PAT,DST,R2_NOTMERGEPEN), OP(SRC,DST,R2_MERGEPEN) },        /* 0xcd  S|~(D|P)       */
    { OP(PAT,DST,R2_MASKNOTPEN), OP(SRC,DST,R2_MERGEPEN) },         /* 0xce  S|(D&~P)       */
    { OP(PAT,SRC,R2_MERGENOTPEN) },                                 /* 0xcf  S|~P           */
    { OP(SRC,DST,R2_MERGEPENNOT), OP(PAT,DST,R2_MASKPEN) },         /* 0xd0  P&(S|~D)       */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xd1  ~P^(S|(D^P))   */
    { OP(SRC,DST,R2_MASKNOTPEN), OP(PAT,DST,R2_XORPEN) },           /* 0xd2  P^(D&~S)       */
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_MASKPEN),
      OP(SRC,DST,R2_NOTXORPEN) },                                   /* 0xd3  ~S^(P&(D|S))   */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_XORPEN),
      OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0xd4  S^((S^P)&(D^P))*/
    { OP(PAT,SRC,R2_NOTMASKPEN), OP(SRC,DST,R2_NOTMASKPEN) },       /* 0xd5  ~(D&~(P&S))    */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_XORPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0xd6  S^P^(D|(P&S))  */
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_NOTMASKPEN) },           /* 0xd7  ~(D&(P^S))     */
    { OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(PAT,DST,R2_XORPEN) },                                      /* 0xd8  P^(D&(S^P))    */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_MERGEPEN), OP(TMP,DST,R2_NOTXORPEN) },          /* 0xd9  ~S^(D|(P&S))   */
    { OP(DST,SRC,R2_NOTMASKPEN), OP(PAT,SRC,R2_MASKPEN),
      OP(SRC,DST,R2_XORPEN) },                                      /* 0xda  D^(P&~(S&D))   */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_NOTMASKPEN) },                                  /* 0xdb  ~((S^P)&(S^D)) */
    { OP(PAT,DST,R2_MASKPENNOT), OP(SRC,DST,R2_MERGEPEN) },         /* 0xdc  S|(P&~D)       */
    { OP(SRC,DST,R2_MERGEPENNOT) },                                 /* 0xdd  S|~D           */
    { OP(PAT,DST,R2_XORPEN), OP(SRC,DST,R2_MERGEPEN) },             /* 0xde  S|(D^P)        */
    { OP(PAT,DST,R2_NOTMASKPEN), OP(SRC,DST,R2_MERGEPEN) },         /* 0xdf  S|~(D&P)       */
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_MASKPEN) },            /* 0xe0  P&(D|S)        */
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_NOTXORPEN) },          /* 0xe1  ~P^(D|S)       */
    { OP(DST,TMP,R2_COPYPEN), OP(PAT,DST,R2_XORPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN) },              /* 0xe2  D^(S&(P^D))    */
    { OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xe3  ~P^(S|(D&P))   */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_XORPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN) },              /* 0xe4  S^(D&(P^S))    */
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_MERGEPEN),
      OP(PAT,DST,R2_NOTXORPEN) },                                   /* 0xe5  ~P^(D|(S&P))   */
    { OP(SRC,TMP,R2_COPYPEN), OP(PAT,SRC,R2_NOTMASKPEN),
      OP(SRC,DST,R2_MASKPEN), OP(TMP,DST,R2_XORPEN) },              /* 0xe6  S^(D&~(P&S))   */
    { OP(PAT,SRC,R2_XORPEN), OP(PAT,DST,R2_XORPEN),
      OP(SRC,DST,R2_NOTMASKPEN) },                                  /* 0xe7  ~((S^P)&(D^P)) */
    { OP(SRC,TMP,R2_COPYPEN), OP(SRC,DST,R2_XORPEN),
      OP(PAT,SRC,R2_XORPEN), OP(SRC,DST,R2_MASKPEN),
      OP(TMP,DST,R2_XORPEN) },                                      /* 0xe8  S^((S^P)&(S^D))*/
    { OP(DST,TMP,R2_COPYPEN), OP(SRC,DST,R2_NOTMASKPEN),
      OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_XORPEN),
      OP(TMP,DST,R2_NOTXORPEN) },                                   /* 0xe9  ~D^S^(P&~(S&D))*/
    { OP(PAT,SRC,R2_MASKPEN), OP(SRC,DST,R2_MERGEPEN) },            /* 0xea  D|(P&S)        */
    { OP(PAT,SRC,R2_NOTXORPEN), OP(SRC,DST,R2_MERGEPEN) },          /* 0xeb  D|~(P^S)       */
    { OP(PAT,DST,R2_MASKPEN), OP(SRC,DST,R2_MERGEPEN) },            /* 0xec  S|(D&P)        */
    { OP(PAT,DST,R2_NOTXORPEN), OP(SRC,DST,R2_MERGEPEN) },          /* 0xed  S|~(D^P)       */
    { OP(SRC,DST,R2_MERGEPEN) },                                    /* 0xee  S|D            */
    { OP(PAT,DST,R2_MERGENOTPEN), OP(SRC,DST,R2_MERGEPEN) },        /* 0xef  S|D|~P         */
    { OP(PAT,DST,R2_COPYPEN) },                                     /* 0xf0  P              */
    { OP(SRC,DST,R2_NOTMERGEPEN), OP(PAT,DST,R2_MERGEPEN) },        /* 0xf1  P|~(D|S)       */
    { OP(SRC,DST,R2_MASKNOTPEN), OP(PAT,DST,R2_MERGEPEN) },         /* 0xf2  P|(D&~S)       */
    { OP(PAT,SRC,R2_MERGEPENNOT) },                                 /* 0xf3  P|~S           */
    { OP(SRC,DST,R2_MASKPENNOT), OP(PAT,DST,R2_MERGEPEN) },         /* 0xf4  P|(S&~D)       */
    { OP(PAT,DST,R2_MERGEPENNOT) },                                 /* 0xf5  P|~D           */
    { OP(SRC,DST,R2_XORPEN), OP(PAT,DST,R2_MERGEPEN) },             /* 0xf6  P|(D^S)        */
    { OP(SRC,DST,R2_NOTMASKPEN), OP(PAT,DST,R2_MERGEPEN) },         /* 0xf7  P|~(S&D)       */
    { OP(SRC,DST,R2_MASKPEN), OP(PAT,DST,R2_MERGEPEN) },            /* 0xf8  P|(D&S)        */
    { OP(SRC,DST,R2_NOTXORPEN), OP(PAT,DST,R2_MERGEPEN) },          /* 0xf9  P|~(D^S)       */
    { OP(PAT,DST,R2_MERGEPEN) },                                    /* 0xfa  D|P            */
    { OP(PAT,SRC,R2_MERGEPENNOT), OP(SRC,DST,R2_MERGEPEN) },        /* 0xfb  D|P|~S         */
    { OP(PAT,SRC,R2_MERGEPEN) },                                    /* 0xfc  P|S            */
    { OP(SRC,DST,R2_MERGEPENNOT), OP(PAT,DST,R2_MERGEPEN) },        /* 0xfd  P|S|~D         */
    { OP(SRC,DST,R2_MERGEPEN), OP(PAT,DST,R2_MERGEPEN) },           /* 0xfe  P|D|S          */
    { OP(PAT,DST,R2_WHITE) }                                        /* 0xff  1              */
};

static int get_overlap( const dib_info *dst, const RECT *dst_rect,
                        const dib_info *src, const RECT *src_rect )
{
    const char *src_top, *dst_top;
    int height, ret = 0;

    if (dst->stride != src->stride) return 0;  /* can't be the same dib */
    if (dst_rect->right <= src_rect->left) return 0;
    if (dst_rect->left >= src_rect->right) return 0;

    src_top = (const char *)src->bits.ptr + src_rect->top * src->stride;
    dst_top = (const char *)dst->bits.ptr + dst_rect->top * dst->stride;
    height = (dst_rect->bottom - dst_rect->top) * dst->stride;

    if (dst->stride > 0)
    {
        if (src_top >= dst_top + height) return 0;
        if (src_top + height <= dst_top) return 0;
        if (dst_top < src_top) ret |= OVERLAP_ABOVE;
        else if (dst_top > src_top) ret |= OVERLAP_BELOW;
    }
    else
    {
        if (src_top <= dst_top + height) return 0;
        if (src_top + height >= dst_top) return 0;
        if (dst_top > src_top) ret |= OVERLAP_ABOVE;
        else if (dst_top < src_top) ret |= OVERLAP_BELOW;
    }

    if (dst_rect->left < src_rect->left) ret |= OVERLAP_LEFT;
    else if (dst_rect->left > src_rect->left) ret |= OVERLAP_RIGHT;

    return ret;
}

static DWORD copy_rect( dib_info *dst, const RECT *dst_rect, const dib_info *src, const RECT *src_rect,
                        const struct clipped_rects *clipped_rects, INT rop2 )
{
    POINT origin;
    const RECT *rects;
    int i, count, start, end, overlap;
    DWORD and = 0, xor = 0;

    if (clipped_rects)
    {
        rects = clipped_rects->rects;
        count = clipped_rects->count;
    }
    else
    {
        rects = dst_rect;
        count = 1;
    }

    switch (rop2)
    {
    case R2_NOT:   and = ~0u;
        /* fall through */
    case R2_WHITE: xor = ~0u;
        /* fall through */
    case R2_BLACK:
        dst->funcs->solid_rects( dst, count, rects, and, xor );
        /* fall through */
    case R2_NOP:
        return ERROR_SUCCESS;
    }

    overlap = get_overlap( dst, dst_rect, src, src_rect );
    if (overlap & OVERLAP_BELOW)
    {
        if (overlap & OVERLAP_RIGHT)  /* right to left, bottom to top */
        {
            for (i = count - 1; i >= 0; i--)
            {
                origin.x = src_rect->left + rects[i].left - dst_rect->left;
                origin.y = src_rect->top  + rects[i].top  - dst_rect->top;
                dst->funcs->copy_rect( dst, &rects[i], src, &origin, rop2, overlap );
            }
        }
        else  /* left to right, bottom to top */
        {
            for (start = count - 1; start >= 0; start = end)
            {
                for (end = start - 1; end >= 0; end--)
                    if (rects[start].top != rects[end].top) break;

                for (i = end + 1; i <= start; i++)
                {
                    origin.x = src_rect->left + rects[i].left - dst_rect->left;
                    origin.y = src_rect->top  + rects[i].top  - dst_rect->top;
                    dst->funcs->copy_rect( dst, &rects[i], src, &origin, rop2, overlap );
                }
            }
        }
    }
    else if (overlap & OVERLAP_RIGHT)  /* right to left, top to bottom */
    {
        for (start = 0; start < count; start = end)
        {
            for (end = start + 1; end < count; end++)
                if (rects[start].top != rects[end].top) break;

            for (i = end - 1; i >= start; i--)
            {
                origin.x = src_rect->left + rects[i].left - dst_rect->left;
                origin.y = src_rect->top  + rects[i].top  - dst_rect->top;
                dst->funcs->copy_rect( dst, &rects[i], src, &origin, rop2, overlap );
            }
        }
    }
    else  /* left to right, top to bottom */
    {
        for (i = 0; i < count; i++)
        {
            origin.x = src_rect->left + rects[i].left - dst_rect->left;
            origin.y = src_rect->top  + rects[i].top  - dst_rect->top;
            dst->funcs->copy_rect( dst, &rects[i], src, &origin, rop2, overlap );
        }
    }
    return ERROR_SUCCESS;
}

static DWORD blend_rect( dib_info *dst, const RECT *dst_rect, const dib_info *src, const RECT *src_rect,
                         HRGN clip, BLENDFUNCTION blend )
{
    POINT origin;
    struct clipped_rects clipped_rects;
    int i;

    if (!get_clipped_rects( dst, dst_rect, clip, &clipped_rects )) return ERROR_SUCCESS;
    for (i = 0; i < clipped_rects.count; i++)
    {
        origin.x = src_rect->left + clipped_rects.rects[i].left - dst_rect->left;
        origin.y = src_rect->top  + clipped_rects.rects[i].top  - dst_rect->top;
        dst->funcs->blend_rect( dst, &clipped_rects.rects[i], src, &origin, blend );
    }
    free_clipped_rects( &clipped_rects );
    return ERROR_SUCCESS;
}

/* compute y-ordered, device coords vertices for a horizontal rectangle gradient */
static void get_gradient_hrect_vertices( const GRADIENT_RECT *rect, const TRIVERTEX *vert,
                                         const POINT *dev_pts, TRIVERTEX v[2] )
{
    int v0 = rect->UpperLeft;
    int v1 = rect->LowerRight;

    if (dev_pts[v1].x < dev_pts[v0].x)  /* swap the colors */
    {
        v0 = rect->LowerRight;
        v1 = rect->UpperLeft;
    }
    v[0]   = vert[v0];
    v[1]   = vert[v1];
    v[0].x = dev_pts[v0].x;
    v[1].x = dev_pts[v1].x;
    v[0].y = min( dev_pts[v0].y, dev_pts[v1].y );
    v[1].y = max( dev_pts[v0].y, dev_pts[v1].y );
}

/* compute y-ordered, device coords vertices for a vertical rectangle gradient */
static void get_gradient_vrect_vertices( const GRADIENT_RECT *rect, const TRIVERTEX *vert,
                                         const POINT *dev_pts, TRIVERTEX v[2] )
{
    int v0 = rect->UpperLeft;
    int v1 = rect->LowerRight;

    if (dev_pts[v1].y < dev_pts[v0].y)  /* swap the colors */
    {
        v0 = rect->LowerRight;
        v1 = rect->UpperLeft;
    }
    v[0]   = vert[v0];
    v[1]   = vert[v1];
    v[0].x = min( dev_pts[v0].x, dev_pts[v1].x );
    v[1].x = max( dev_pts[v0].x, dev_pts[v1].x );
    v[0].y = dev_pts[v0].y;
    v[1].y = dev_pts[v1].y;
}

/* compute y-ordered, device coords vertices for a triangle gradient */
static void get_gradient_triangle_vertices( const GRADIENT_TRIANGLE *tri, const TRIVERTEX *vert,
                                            const POINT *dev_pts, TRIVERTEX v[3] )
{
    int v0, v1, v2;

    if (dev_pts[tri->Vertex1].y > dev_pts[tri->Vertex2].y)
    {
        if (dev_pts[tri->Vertex3].y < dev_pts[tri->Vertex2].y)
            { v0 = tri->Vertex3; v1 = tri->Vertex2; v2 = tri->Vertex1; }
        else if (dev_pts[tri->Vertex3].y < dev_pts[tri->Vertex1].y)
            { v0 = tri->Vertex2; v1 = tri->Vertex3; v2 = tri->Vertex1; }
        else
            { v0 = tri->Vertex2; v1 = tri->Vertex1; v2 = tri->Vertex3; }
    }
    else
    {
        if (dev_pts[tri->Vertex3].y < dev_pts[tri->Vertex1].y)
            { v0 = tri->Vertex3; v1 = tri->Vertex1; v2 = tri->Vertex2; }
        else if (dev_pts[tri->Vertex3].y < dev_pts[tri->Vertex2].y)
            { v0 = tri->Vertex1; v1 = tri->Vertex3; v2 = tri->Vertex2; }
        else
            { v0 = tri->Vertex1; v1 = tri->Vertex2; v2 = tri->Vertex3; }
    }
    v[0]   = vert[v0];
    v[1]   = vert[v1];
    v[2]   = vert[v2];
    v[0].x = dev_pts[v0].x;
    v[0].y = dev_pts[v0].y;
    v[1].y = dev_pts[v1].y;
    v[1].x = dev_pts[v1].x;
    v[2].x = dev_pts[v2].x;
    v[2].y = dev_pts[v2].y;
}

static BOOL gradient_rect( dib_info *dib, TRIVERTEX *v, int mode, HRGN clip )
{
    int i;
    struct clipped_rects clipped_rects;
    RECT rect;
    BOOL ret = TRUE;

    if (mode == GRADIENT_FILL_TRIANGLE)
    {
        rect.left   = min( v[0].x, min( v[1].x, v[2].x ));
        rect.top    = v[0].y;
        rect.right  = max( v[0].x, max( v[1].x, v[2].x ));
        rect.bottom = v[2].y;
    }
    else
    {
        rect.left   = v[0].x;
        rect.top    = v[0].y;
        rect.right  = v[1].x;
        rect.bottom = v[1].y;
    }

    if (!get_clipped_rects( dib, &rect, clip, &clipped_rects )) return TRUE;
    for (i = 0; i < clipped_rects.count; i++)
    {
        if (!(ret = dib->funcs->gradient_rect( dib, &clipped_rects.rects[i], v, mode ))) break;
    }
    free_clipped_rects( &clipped_rects );
    return ret;
}

static DWORD copy_src_bits( dib_info *src, RECT *src_rect )
{
    int y, stride = get_dib_stride( src->width, src->bit_count );
    int height = src_rect->bottom - src_rect->top;
    void *ptr = HeapAlloc( GetProcessHeap(), 0, stride * height );

    if (!ptr) return ERROR_OUTOFMEMORY;

    for (y = 0; y < height; y++)
        memcpy( (char *)ptr + y * stride,
                (char *)src->bits.ptr + (src_rect->top + y) * src->stride, stride );
    src->stride = stride;
    src->height = height;
    if (src->bits.free) src->bits.free( &src->bits );
    src->bits.is_copy = TRUE;
    src->bits.ptr = ptr;
    src->bits.free = free_heap_bits;
    src->bits.param = NULL;

    offset_rect( src_rect, 0, -src_rect->top );
    return ERROR_SUCCESS;
}

static DWORD create_tmp_dib( const dib_info *copy, int width, int height, dib_info *ret )
{
    copy_dib_color_info( ret, copy );
    ret->width = width;
    ret->height = height;
    ret->stride = get_dib_stride( width, ret->bit_count );
    ret->bits.ptr = HeapAlloc( GetProcessHeap(), 0, ret->height * ret->stride );
    ret->bits.is_copy = TRUE;
    ret->bits.free = free_heap_bits;
    ret->bits.param = NULL;

    return ret->bits.ptr ? ERROR_SUCCESS : ERROR_OUTOFMEMORY;
}

static DWORD execute_rop( dibdrv_physdev *pdev, const RECT *dst_rect, dib_info *src,
                          const RECT *src_rect, const struct clipped_rects *clipped_rects, DWORD rop )
{
    dib_info *dibs[3], *result = src, tmp;
    RECT rects[3];
    int width  = dst_rect->right - dst_rect->left;
    int height = dst_rect->bottom - dst_rect->top;
    const BYTE *opcode = BITBLT_Opcodes[(rop >> 16) & 0xff];
    DWORD err;

    dibs[SRC] = src;
    dibs[DST] = &pdev->dib;
    dibs[TMP] = 0;

    rects[SRC] = *src_rect;
    rects[DST] = *dst_rect;
    rects[TMP] = *dst_rect;
    offset_rect( &rects[TMP], -rects[TMP].left, -rects[TMP].top );

    for ( ; *opcode; opcode++)
    {
        if (OP_DST(*opcode) == DST) result = dibs[DST];
        if (OP_DST(*opcode) == SRC && src->bits.is_copy == FALSE)
        {
            err = copy_src_bits( src, &rects[SRC] );
            if (err) return err;
        }
        if (OP_DST(*opcode) == TMP && !dibs[TMP])
        {
            err = create_tmp_dib( &pdev->dib, width, height, &tmp );
            if (err) return err;
            dibs[TMP] = &tmp;
        }

        switch(OP_SRCDST(*opcode))
        {
        case OP_ARGS(DST,TMP):
        case OP_ARGS(SRC,TMP):
        case OP_ARGS(DST,SRC):
        case OP_ARGS(SRC,DST):
        case OP_ARGS(TMP,SRC):
        case OP_ARGS(TMP,DST):
            copy_rect( dibs[OP_DST(*opcode)], &rects[OP_DST(*opcode)],
                       dibs[OP_SRC(*opcode)], &rects[OP_SRC(*opcode)],
                       OP_DST(*opcode) == DST ? clipped_rects : NULL, OP_ROP(*opcode) );
            break;
        case OP_ARGS(PAT,DST):
            pdev->brush.rects( pdev, &pdev->brush, dibs[DST], clipped_rects->count, clipped_rects->rects,
                               OP_ROP(*opcode) );
            break;
        case OP_ARGS(PAT,SRC):
            pdev->brush.rects( pdev, &pdev->brush, dibs[SRC], 1, &rects[SRC], OP_ROP(*opcode) );
            break;
        }
    }

    if (dibs[TMP]) free_dib_info( dibs[TMP] );

    if (result == src)
        copy_rect( dibs[DST], &rects[DST], dibs[SRC], &rects[SRC], clipped_rects, R2_COPYPEN );

    return ERROR_SUCCESS;
}

static void set_color_info( const dib_info *dib, BITMAPINFO *info )
{
    DWORD *masks = (DWORD *)info->bmiColors;

    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biClrUsed     = 0;

    switch (info->bmiHeader.biBitCount)
    {
    case 1:
    case 4:
    case 8:
        if (dib->color_table)
        {
            info->bmiHeader.biClrUsed = 1 << dib->bit_count;
            memcpy( info->bmiColors, dib->color_table, info->bmiHeader.biClrUsed * sizeof(RGBQUAD) );
        }
        break;
    case 16:
        masks[0] = dib->red_mask;
        masks[1] = dib->green_mask;
        masks[2] = dib->blue_mask;
        info->bmiHeader.biCompression = BI_BITFIELDS;
        break;
    case 32:
        if (dib->funcs != &funcs_8888)
        {
            masks[0] = dib->red_mask;
            masks[1] = dib->green_mask;
            masks[2] = dib->blue_mask;
            info->bmiHeader.biCompression = BI_BITFIELDS;
        }
        break;
    }
}

/***********************************************************************
 *           dibdrv_GetImage
 */
DWORD dibdrv_GetImage( PHYSDEV dev, HBITMAP hbitmap, BITMAPINFO *info,
                       struct gdi_image_bits *bits, struct bitblt_coords *src )
{
    DWORD ret = ERROR_SUCCESS;
    dib_info *dib = NULL, stand_alone;

    TRACE( "%p %p %p\n", dev, hbitmap, info );

    info->bmiHeader.biSize          = sizeof(info->bmiHeader);
    info->bmiHeader.biPlanes        = 1;
    info->bmiHeader.biCompression   = BI_RGB;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrImportant  = 0;

    if (hbitmap)
    {
        BITMAPOBJ *bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );

        if (!bmp) return ERROR_INVALID_HANDLE;
        if (!init_dib_info_from_bitmapobj( &stand_alone, bmp, 0 ))
        {
            ret = ERROR_OUTOFMEMORY;
            goto done;
        }
        dib = &stand_alone;
    }
    else
    {
        dibdrv_physdev *pdev = get_dibdrv_pdev(dev);
        dib = &pdev->dib;
    }

    info->bmiHeader.biWidth     = dib->width;
    info->bmiHeader.biHeight    = dib->stride > 0 ? -dib->height : dib->height;
    info->bmiHeader.biBitCount  = dib->bit_count;
    info->bmiHeader.biSizeImage = dib->height * abs( dib->stride );

    set_color_info( dib, info );

    if (bits)
    {
        bits->ptr = dib->bits.ptr;
        if (dib->stride < 0)
            bits->ptr = (char *)bits->ptr + (dib->height - 1) * dib->stride;
        bits->is_copy = FALSE;
        bits->free = NULL;
    }

done:
   if (hbitmap) GDI_ReleaseObj( hbitmap );
   return ret;
}

static BOOL matching_color_info( const dib_info *dib, const BITMAPINFO *info )
{
    switch (info->bmiHeader.biBitCount)
    {
    case 1:
    case 4:
    case 8:
    {
        RGBQUAD *color_table = (RGBQUAD *)((char *)info + info->bmiHeader.biSize);
        if (dib->color_table_size != info->bmiHeader.biClrUsed) return FALSE;
        return !memcmp( color_table, dib->color_table, dib->color_table_size * sizeof(RGBQUAD) );
    }

    case 16:
    {
        DWORD *masks = (DWORD *)info->bmiColors;
        if (info->bmiHeader.biCompression == BI_RGB) return dib->funcs == &funcs_555;
        if (info->bmiHeader.biCompression == BI_BITFIELDS)
            return masks[0] == dib->red_mask && masks[1] == dib->green_mask && masks[2] == dib->blue_mask;
        break;
    }

    case 24:
        return TRUE;

    case 32:
    {
        DWORD *masks = (DWORD *)info->bmiColors;
        if (info->bmiHeader.biCompression == BI_RGB) return dib->funcs == &funcs_8888;
        if (info->bmiHeader.biCompression == BI_BITFIELDS)
            return masks[0] == dib->red_mask && masks[1] == dib->green_mask && masks[2] == dib->blue_mask;
        break;
    }

    }

    return FALSE;
}

static inline BOOL rop_uses_pat(DWORD rop)
{
    return ((rop >> 4) & 0x0f0000) != (rop & 0x0f0000);
}

/***********************************************************************
 *           dibdrv_PutImage
 */
DWORD dibdrv_PutImage( PHYSDEV dev, HBITMAP hbitmap, HRGN clip, BITMAPINFO *info,
                       const struct gdi_image_bits *bits, struct bitblt_coords *src,
                       struct bitblt_coords *dst, DWORD rop )
{
    dib_info *dib = NULL, stand_alone;
    struct clipped_rects clipped_rects;
    DWORD ret;
    dib_info src_dib;
    HRGN tmp_rgn = 0;
    dibdrv_physdev *pdev = NULL;

    TRACE( "%p %p %p\n", dev, hbitmap, info );

    if (hbitmap)
    {
        BITMAPOBJ *bmp = GDI_GetObjPtr( hbitmap, OBJ_BITMAP );

        if (!bmp) return ERROR_INVALID_HANDLE;
        if (!init_dib_info_from_bitmapobj( &stand_alone, bmp, 0 ))
        {
            ret = ERROR_OUTOFMEMORY;
            goto done;
        }
        dib = &stand_alone;
        rop = SRCCOPY;
    }
    else
    {
        pdev = get_dibdrv_pdev( dev );
        dib = &pdev->dib;
    }

    if (info->bmiHeader.biPlanes != 1) goto update_format;
    if (info->bmiHeader.biBitCount != dib->bit_count) goto update_format;
    if (!matching_color_info( dib, info )) goto update_format;
    if (!bits)
    {
        ret = ERROR_SUCCESS;
        goto done;
    }
    if ((src->width != dst->width) || (src->height != dst->height))
    {
        ret = ERROR_TRANSFORM_NOT_SUPPORTED;
        goto done;
    }

    init_dib_info_from_bitmapinfo( &src_dib, info, bits->ptr, 0 );
    src_dib.bits.is_copy = bits->is_copy;

    if (!hbitmap)
    {
        if (clip && pdev->clip)
        {
            tmp_rgn = CreateRectRgn( 0, 0, 0, 0 );
            CombineRgn( tmp_rgn, clip, pdev->clip, RGN_AND );
            clip = tmp_rgn;
        }
        else if (!clip) clip = pdev->clip;
    }

    if (!get_clipped_rects( dib, &dst->visrect, clip, &clipped_rects ))
    {
        ret = ERROR_SUCCESS;
        goto done;
    }

    if (!rop_uses_pat( rop ))
    {
        int rop2 = ((rop >> 16) & 0xf) + 1;
        ret = copy_rect( dib, &dst->visrect, &src_dib, &src->visrect, &clipped_rects, rop2 );
    }
    else
    {
        ret = execute_rop( pdev, &dst->visrect, &src_dib, &src->visrect, &clipped_rects, rop );
    }
    goto done;

update_format:
    info->bmiHeader.biPlanes   = 1;
    info->bmiHeader.biBitCount = dib->bit_count;
    set_color_info( dib, info );
    ret = ERROR_BAD_FORMAT;

done:
    if (tmp_rgn) DeleteObject( tmp_rgn );
    if (hbitmap) GDI_ReleaseObj( hbitmap );
    return ret;
}

/***********************************************************************
 *           dibdrv_BlendImage
 */
DWORD dibdrv_BlendImage( PHYSDEV dev, BITMAPINFO *info, const struct gdi_image_bits *bits,
                         struct bitblt_coords *src, struct bitblt_coords *dst, BLENDFUNCTION blend )
{
    dibdrv_physdev *pdev = get_dibdrv_pdev( dev );
    dib_info src_dib;

    TRACE( "%p %p\n", dev, info );

    if (info->bmiHeader.biPlanes != 1) goto update_format;
    if (info->bmiHeader.biBitCount != 32) goto update_format;
    if (info->bmiHeader.biCompression == BI_BITFIELDS)
    {
        DWORD *masks = (DWORD *)info->bmiColors;
        if (masks[0] != 0xff0000 || masks[1] != 0x00ff00 || masks[2] != 0x0000ff)
            goto update_format;
    }

    if (!bits) return ERROR_SUCCESS;
    if ((src->width != dst->width) || (src->height != dst->height)) return ERROR_TRANSFORM_NOT_SUPPORTED;

    init_dib_info_from_bitmapinfo( &src_dib, info, bits->ptr, 0 );
    src_dib.bits.is_copy = bits->is_copy;
    return blend_rect( &pdev->dib, &dst->visrect, &src_dib, &src->visrect, pdev->clip, blend );

update_format:
    if (blend.AlphaFormat & AC_SRC_ALPHA)  /* source alpha requires A8R8G8B8 format */
        return ERROR_INVALID_PARAMETER;

    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biClrUsed     = 0;
    return ERROR_BAD_FORMAT;
}

/****************************************************************************
 *               calc_1d_stretch_params   (helper for stretch_bitmapinfo)
 *
 * If one plots the dst axis vertically, the src axis horizontally, then a
 * 1-d stretch/shrink is rather like finding the pixels to draw a straight
 * line.  Following a Bresenham type argument from point (s_i, d_i) we must
 * pick either (s_i + 1, d_i) or (s_i + 1, d_i + 1), however the choice
 * depends on where the centre of the next pixel is, ie whether the
 * point (s_i + 3/2, d_i + 1) (the mid-point between the two centres)
 * lies above or below the idea line.  The differences in error between
 * both choices and the current pixel is the same as per Bresenham,
 * the only difference is the error between the zeroth and first pixel.
 * This is now 3 dy - 2 dx (cf 2 dy - dx for the line case).
 *
 * Here we use the Bresenham line clipper to provide the start and end points
 * and add the additional dy - dx error term by passing this as the bias.
 *
 */
static DWORD calc_1d_stretch_params( INT dst_start, INT dst_length, INT dst_vis_start, INT dst_vis_end,
                                     INT src_start, INT src_length, INT src_vis_start, INT src_vis_end,
                                     INT *dst_clipped_start, INT *src_clipped_start,
                                     INT *dst_clipped_end, INT *src_clipped_end,
                                     struct stretch_params *stretch_params, BOOL *stretch )
{
    bres_params bres_params;
    POINT start, end, clipped_start, clipped_end;
    RECT clip;
    int clip_status, m, n;

    stretch_params->src_inc = stretch_params->dst_inc = 1;

    bres_params.dy = abs( dst_length );
    bres_params.dx = abs( src_length );

    if (bres_params.dx > bres_params.dy) bres_params.octant = 1;
    else bres_params.octant = 2;
    if (src_length < 0)
    {
        bres_params.octant = 5 - bres_params.octant;
        stretch_params->src_inc = -1;
    }
    if (dst_length < 0)
    {
        bres_params.octant = 9 - bres_params.octant;
        stretch_params->dst_inc = -1;
    }
    bres_params.octant = 1 << (bres_params.octant - 1);

    if (bres_params.dx > bres_params.dy)
        bres_params.bias = bres_params.dy - bres_params.dx;
    else
        bres_params.bias = bres_params.dx - bres_params.dy;

    start.x = src_start;
    start.y = dst_start;
    end.x   = src_start + src_length;
    end.y   = dst_start + dst_length;

    clip.left   = src_vis_start;
    clip.right  = src_vis_end;
    clip.top    = dst_vis_start;
    clip.bottom = dst_vis_end;

    clip_status = clip_line( &start, &end, &clip, &bres_params, &clipped_start, &clipped_end );

    if (!clip_status) return ERROR_NO_DATA;

    m = abs( clipped_start.x - start.x );
    n = abs( clipped_start.y - start.y );

    if (bres_params.dx > bres_params.dy)
    {
        stretch_params->err_start = 3 * bres_params.dy - 2 * bres_params.dx +
            m * 2 * bres_params.dy - n * 2 * bres_params.dx;
        stretch_params->err_add_1 = 2 * bres_params.dy - 2 * bres_params.dx;
        stretch_params->err_add_2 = 2 * bres_params.dy;
        stretch_params->length = abs( clipped_end.x - clipped_start.x );
        *stretch = FALSE;
    }
    else
    {
        stretch_params->err_start = 3 * bres_params.dx - 2 * bres_params.dy +
            n * 2 * bres_params.dx - m * 2 * bres_params.dy;
        stretch_params->err_add_1 = 2 * bres_params.dx - 2 * bres_params.dy;
        stretch_params->err_add_2 = 2 * bres_params.dx;
        stretch_params->length = abs( clipped_end.y - clipped_start.y );
        *stretch = TRUE;
    }

    /* The endpoint will usually have been clipped out as we don't want to touch
       that pixel, if it is we'll increment the length. */
    if (end.x != clipped_end.x || end.y != clipped_end.y)
    {
        clipped_end.x += stretch_params->src_inc;
        clipped_end.y += stretch_params->dst_inc;
        stretch_params->length++;
    }

    *src_clipped_start = clipped_start.x;
    *dst_clipped_start = clipped_start.y;
    *src_clipped_end   = clipped_end.x;
    *dst_clipped_end   = clipped_end.y;

    return ERROR_SUCCESS;
}


DWORD stretch_bitmapinfo( const BITMAPINFO *src_info, void *src_bits, struct bitblt_coords *src,
                          const BITMAPINFO *dst_info, void *dst_bits, struct bitblt_coords *dst,
                          INT mode )
{
    dib_info src_dib, dst_dib;
    POINT dst_start, src_start, dst_end, src_end;
    RECT rect;
    BOOL hstretch, vstretch;
    struct stretch_params v_params, h_params;
    int err;
    DWORD ret;
    void (* row_fn)(const dib_info *dst_dib, const POINT *dst_start,
                    const dib_info *src_dib, const POINT *src_start,
                    const struct stretch_params *params, int mode, BOOL keep_dst);

    TRACE("dst %d, %d - %d x %d visrect %s src %d, %d - %d x %d visrect %s\n",
          dst->x, dst->y, dst->width, dst->height, wine_dbgstr_rect(&dst->visrect),
          src->x, src->y, src->width, src->height, wine_dbgstr_rect(&src->visrect));

    init_dib_info_from_bitmapinfo( &src_dib, src_info, src_bits, 0 );
    init_dib_info_from_bitmapinfo( &dst_dib, dst_info, dst_bits, 0 );

    /* v */
    ret = calc_1d_stretch_params( dst->y, dst->height, dst->visrect.top, dst->visrect.bottom,
                                  src->y, src->height, src->visrect.top, src->visrect.bottom,
                                  &dst_start.y, &src_start.y, &dst_end.y, &src_end.y,
                                  &v_params, &vstretch );
    if (ret) return ret;

    /* h */
    ret = calc_1d_stretch_params( dst->x, dst->width, dst->visrect.left, dst->visrect.right,
                                  src->x, src->width, src->visrect.left, src->visrect.right,
                                  &dst_start.x, &src_start.x, &dst_end.x, &src_end.x,
                                  &h_params, &hstretch );
    if (ret) return ret;

    TRACE("got dst start %d, %d inc %d, %d. src start %d, %d inc %d, %d len %d x %d\n",
          dst_start.x, dst_start.y, h_params.dst_inc, v_params.dst_inc,
          src_start.x, src_start.y, h_params.src_inc, v_params.src_inc,
          h_params.length, v_params.length);

    get_bounding_rect( &rect, dst_start.x, dst_start.y, dst_end.x - dst_start.x, dst_end.y - dst_start.y );
    intersect_rect( &dst->visrect, &dst->visrect, &rect );

    dst_start.x -= dst->visrect.left;
    dst_start.y -= dst->visrect.top;

    err = v_params.err_start;

    row_fn = hstretch ? dst_dib.funcs->stretch_row : dst_dib.funcs->shrink_row;

    if (vstretch)
    {
        BOOL need_row = TRUE;
        RECT last_row, this_row;
        if (hstretch) mode = STRETCH_DELETESCANS;
        last_row.left = 0;
        last_row.right = dst->visrect.right - dst->visrect.left;

        while (v_params.length--)
        {
            if (need_row)
            {
                row_fn( &dst_dib, &dst_start, &src_dib, &src_start, &h_params, mode, FALSE );
                need_row = FALSE;
            }
            else
            {
                last_row.top = dst_start.y - v_params.dst_inc;
                last_row.bottom = dst_start.y;
                this_row = last_row;
                offset_rect( &this_row, 0, v_params.dst_inc );
                copy_rect( &dst_dib, &this_row, &dst_dib, &last_row, NULL, R2_COPYPEN );
            }

            if (err > 0)
            {
                src_start.y += v_params.src_inc;
                need_row = TRUE;
                err += v_params.err_add_1;
            }
            else err += v_params.err_add_2;
            dst_start.y += v_params.dst_inc;
        }
    }
    else
    {
        int merged_rows = 0;
        int faster_mode = mode;

        while (v_params.length--)
        {
            if (hstretch) faster_mode = merged_rows ? mode : STRETCH_DELETESCANS;
            row_fn( &dst_dib, &dst_start, &src_dib, &src_start, &h_params, faster_mode, merged_rows != 0 );
            merged_rows++;

            if (err > 0)
            {
                dst_start.y += v_params.dst_inc;
                merged_rows = 0;
                err += v_params.err_add_1;
            }
            else err += v_params.err_add_2;
            src_start.y += v_params.src_inc;
        }
    }

    /* update coordinates, the destination rectangle is always stored at 0,0 */
    *src = *dst;
    src->x -= src->visrect.left;
    src->y -= src->visrect.top;
    offset_rect( &src->visrect, -src->visrect.left, -src->visrect.top );
    return ERROR_SUCCESS;
}

DWORD blend_bitmapinfo( const BITMAPINFO *src_info, void *src_bits, struct bitblt_coords *src,
                        const BITMAPINFO *dst_info, void *dst_bits, struct bitblt_coords *dst,
                        BLENDFUNCTION blend )
{
    dib_info src_dib, dst_dib;

    init_dib_info_from_bitmapinfo( &src_dib, src_info, src_bits, 0 );
    init_dib_info_from_bitmapinfo( &dst_dib, dst_info, dst_bits, default_color_table );

    return blend_rect( &dst_dib, &dst->visrect, &src_dib, &src->visrect, NULL, blend );
}

DWORD gradient_bitmapinfo( const BITMAPINFO *info, void *bits, TRIVERTEX *vert_array, ULONG nvert,
                           void *grad_array, ULONG ngrad, ULONG mode, const POINT *dev_pts, HRGN rgn )
{
    dib_info dib;
    const GRADIENT_TRIANGLE *tri = grad_array;
    const GRADIENT_RECT *rect = grad_array;
    unsigned int i;
    int y;
    TRIVERTEX vert[3];
    RECT rc;
    DWORD ret = ERROR_SUCCESS;

    init_dib_info_from_bitmapinfo( &dib, info, bits, default_color_table );

    switch (mode)
    {
    case GRADIENT_FILL_RECT_H:
        for (i = 0; i < ngrad; i++, rect++)
        {
            get_gradient_hrect_vertices( rect, vert_array, dev_pts, vert );
            gradient_rect( &dib, vert, mode, 0 );
            rc.left   = vert[0].x;
            rc.top    = vert[0].y;
            rc.right  = vert[1].x;
            rc.bottom = vert[1].y;
            add_rect_to_region( rgn, &rc );
        }
        break;

    case GRADIENT_FILL_RECT_V:
        for (i = 0; i < ngrad; i++, rect++)
        {
            get_gradient_vrect_vertices( rect, vert_array, dev_pts, vert );
            gradient_rect( &dib, vert, mode, 0 );
            rc.left   = vert[0].x;
            rc.top    = vert[0].y;
            rc.right  = vert[1].x;
            rc.bottom = vert[1].y;
            add_rect_to_region( rgn, &rc );
        }
        break;

    case GRADIENT_FILL_TRIANGLE:
        for (i = 0; i < ngrad; i++, tri++)
        {
            get_gradient_triangle_vertices( tri, vert_array, dev_pts, vert );
            if (gradient_rect( &dib, vert, mode, 0 ))
            {
                for (y = vert[0].y; y < vert[2].y; y++)
                {
                    int x1, x2 = edge_coord( y, vert[0].x, vert[0].y, vert[2].x, vert[2].y );
                    if (y < vert[1].y) x1 = edge_coord( y, vert[0].x, vert[0].y, vert[1].x, vert[1].y );
                    else x1 = edge_coord( y, vert[1].x, vert[1].y, vert[2].x, vert[2].y );

                    rc.left   = min( x1, x2 );
                    rc.top    = y;
                    rc.right  = max( x1, x2 );
                    rc.bottom = y + 1;
                    add_rect_to_region( rgn, &rc );
                }
            }
            else ret = ERROR_INVALID_PARAMETER;
        }
        break;
    }
    return ret;
}

COLORREF get_pixel_bitmapinfo( const BITMAPINFO *info, void *bits, struct bitblt_coords *src )
{
    dib_info dib;
    DWORD pixel;

    init_dib_info_from_bitmapinfo( &dib, info, bits, default_color_table );
    pixel = dib.funcs->get_pixel( &dib, src->x, src->y );
    return dib.funcs->pixel_to_colorref( &dib, pixel );
}

/***********************************************************************
 *           dibdrv_StretchBlt
 */
BOOL dibdrv_StretchBlt( PHYSDEV dst_dev, struct bitblt_coords *dst,
                        PHYSDEV src_dev, struct bitblt_coords *src, DWORD rop )
{
    BOOL ret;
    DC *dc_dst = get_dc_ptr( dst_dev->hdc );

    if (!dc_dst) return FALSE;

    if (dst->width == 1 && src->width > 1) src->width--;
    if (dst->height == 1 && src->height > 1) src->height--;

    ret = dc_dst->nulldrv.funcs->pStretchBlt( &dc_dst->nulldrv, dst,
                                              src_dev, src, rop );
    release_dc_ptr( dc_dst );
    return ret;
}

/***********************************************************************
 *           dibdrv_AlphaBlend
 */
BOOL dibdrv_AlphaBlend( PHYSDEV dst_dev, struct bitblt_coords *dst,
                        PHYSDEV src_dev, struct bitblt_coords *src, BLENDFUNCTION blend )
{
    BOOL ret;
    DC *dc_dst = get_dc_ptr( dst_dev->hdc );

    if (!dc_dst) return FALSE;

    ret = dc_dst->nulldrv.funcs->pAlphaBlend( &dc_dst->nulldrv, dst, src_dev, src, blend );
    release_dc_ptr( dc_dst );
    return ret;
}

/***********************************************************************
 *           dibdrv_GradientFill
 */
BOOL dibdrv_GradientFill( PHYSDEV dev, TRIVERTEX *vert_array, ULONG nvert,
                          void *grad_array, ULONG ngrad, ULONG mode )
{
    dibdrv_physdev *pdev = get_dibdrv_pdev( dev );
    const GRADIENT_TRIANGLE *tri = grad_array;
    const GRADIENT_RECT *rect = grad_array;
    unsigned int i;
    POINT *pts;
    TRIVERTEX vert[3];
    BOOL ret = TRUE;

    if (!(pts = HeapAlloc( GetProcessHeap(), 0, nvert * sizeof(*pts) ))) return FALSE;
    for (i = 0; i < nvert; i++)
    {
        pts[i].x = vert_array[i].x;
        pts[i].y = vert_array[i].y;
    }
    LPtoDP( dev->hdc, pts, nvert );

    switch (mode)
    {
    case GRADIENT_FILL_RECT_H:
        for (i = 0; i < ngrad; i++, rect++)
        {
            get_gradient_hrect_vertices( rect, vert_array, pts, vert );
            /* Windows bug: no alpha on a8r8g8b8 created with bitfields */
            if (pdev->dib.funcs == &funcs_8888 && pdev->dib.compression == BI_BITFIELDS)
                vert[0].Alpha = vert[1].Alpha = 0;
            gradient_rect( &pdev->dib, vert, mode, pdev->clip );
        }
        break;

    case GRADIENT_FILL_RECT_V:
        for (i = 0; i < ngrad; i++, rect++)
        {
            get_gradient_vrect_vertices( rect, vert_array, pts, vert );
            /* Windows bug: no alpha on a8r8g8b8 created with bitfields */
            if (pdev->dib.funcs == &funcs_8888 && pdev->dib.compression == BI_BITFIELDS)
                vert[0].Alpha = vert[1].Alpha = 0;
            gradient_rect( &pdev->dib, vert, mode, pdev->clip );
        }
        break;

    case GRADIENT_FILL_TRIANGLE:
        for (i = 0; i < ngrad; i++, tri++)
        {
            get_gradient_triangle_vertices( tri, vert_array, pts, vert );
            /* Windows bug: no alpha on a8r8g8b8 created with bitfields */
            if (pdev->dib.funcs == &funcs_8888 && pdev->dib.compression == BI_BITFIELDS)
                vert[0].Alpha = vert[1].Alpha = vert[2].Alpha = 0;
            if (!gradient_rect( &pdev->dib, vert, mode, pdev->clip )) ret = FALSE;
        }
        break;
    }

    HeapFree( GetProcessHeap(), 0, pts );
    return ret;
}
