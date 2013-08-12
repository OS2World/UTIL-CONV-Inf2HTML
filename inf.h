
/*
 * inf.h:
 *      typedef's for structures in INF files
 *
 *      Credits: Most of the information for creating this stems
 *      from the article in EDM/2 vol. 3 no. 8 about INF internals.
 *      Big thanks go out to Peter Childs for this.
 *      His work in turn is based on that of others. See inf03.txt
 *      for details.
 *
 *      Copyright (C) 1997-98 Ulrich M”ller.
 *      This file is part of the INF2HTML package.
 *      INF2HTML is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published
 *      by the Free Software Foundation, in version 2 as it comes in the
 *      "COPYING" file of INF2HTML distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

// pack all structures, i.e. do not align on
// 4-byte borders
#pragma pack (1)

// INF header at beginning of file
typedef struct _INFHEADER
{
    USHORT usMagicID;           // ID magic word (5348h = "HS")
    BYTE   bUnknown1;           // unknown purpose, could be third letter of ID
    BYTE   bFlags;              // probably a flag word... bit0 if INF, bit 4 if HLP
    USHORT usHdrsize;           // total size of header
    USHORT usUnknown2;          // unknown purpose
    USHORT usNToc;              // number of entries in the tocarray
    ULONG  ulTocStrTableStart;  // file offset of the start of the strings for the table-of-contents
    ULONG  ulTocStrLen;         // number of bytes in file occupied by the table-of-contents strings
    ULONG  ulTocStart;          // file offset of the start of tocarray
    USHORT usNRes;              // number of panels with ressource numbers
    ULONG  ulResStart;          // file offset of ressource number table
    USHORT usNName;             // number of panels with textual name
    ULONG  ulNameStart;         // file offset to panel name table
    USHORT usNIndex;            // number of index entries
    ULONG  ulIndexStart;        // file offset to index table
    ULONG  ulIndexLen;          // size of index table
    CHAR   acUnknown3[10] ;     // unknown purpose
    ULONG  ulSearchStart;       // file offset of full text search table
    ULONG  ulSearchLen;         // size of full text search table
    USHORT usNSlots;            // number of "slots"
    ULONG  ulSlotsStart;        // file offset of the slots array
    ULONG  ulDictLen;           // number of bytes occupied by the "dictionary"
    USHORT usNDict;             // number of entries in the dictionary
    ULONG  ulDictStart;         // file offset of the start of the dictionary
    ULONG  ulImgStart;          // file offset of image data
    BYTE   bUnknown4;           // unknown purpose
    ULONG  ulNlsStart;          // file offset of NLS table
    ULONG  ulNlsLen;            // size of NLS table
    ULONG  ulExtStart;          // file offset of extended data block
    CHAR   acUnknown5[12];      // unknown purpose
    CHAR   szTitle[48];         // ASCII title of database
} INFHEADER, *PINFHEADER;

// Table of Contents (TOC) entry. The TOC starts at file
// offset ulTocStart in the INF header.
typedef struct _TOCENTRY
{
    BYTE bLen;                  // length of the entry including this byte
    BYTE bFlags;                // flag byte:
                                //      0x80    fHasChildren:
                                //              following nodes are a higher level
                                //      0x40    fHidden:
                                //              entry is not shown in VIEW.EXE TOC
                                //      0x20    fExtended:
                                //              extended TOC structure format (see below)
                                //      0x10    unknown
                                // The lower four bits (0x0F) show the TOC level
                                // of this entry.
    BYTE bNTocSlots;            // number of slots occupied by the text for
                                // this toc entry

} TOCENTRY, *PTOCENTRY;

/*  if the "extended" bit is 0, this is immediately followed by
    {
        USHORT usTocslots[ntocslots];   // indices of the slots that make up
                                        // the article for this entry
        CHAR  achTitle[];               // the remainder of the tocentry
                                        // until bLen bytes have been used [not
                                        // zero terminated]
    }

    if extended is 1 there are intervening bytes that (I think) describe
    the kind, size and position of the window in which to display the
    article.  I haven't decoded these bytes, though in most cases the
    following tells how many there are.  Overlay the following on the next
    two bytes
        {
           int8 w1;
           int8 w2;
        }
    Here's a C code fragment for computing the number of bytes to skip
        int bytestoskip = 0;
        if (w1 & 0x8) { bytestoskip += 2 };
        if (w1 & 0x1) { bytestoskip += 5 };
        if (w1 & 0x2) { bytestoskip += 5 };
        if (w2 & 0x4) { bytestoskip += 2 };

    skip over bytestoskip bytes (after w2) and find the tocslots and title
    as in the non-extended case. */

// Slot entry structure. Each article (i.e. INF page) consists
// of one or more slots. The encoded "text" starts at abText[];
// each byte is an offset into the local dictionary, which in
// turn contains offsets into the global dictionary.
typedef struct _SLOT
{
    BYTE   bUnknown;            // ?? [always seen 0]
    ULONG  ulLocalDictStart;    // file offset  of  the  local dictionary
    BYTE   bNLocalDict;         // number of entries in  the local dictionary
    USHORT usNText;             // number of bytes in the text
    BYTE   abText[1];           // encoded text of the article (usNText bytes)
} SLOT, *PSLOT;

typedef struct _DICTENTRY
{
    BYTE    bLength;
    CHAR    achEntry[1];
} DICTENTRY, *PDICTENTRY;

typedef struct _LOCALDICT
{
    USHORT  usEntries[1];       // with Slot.bNLocalDict entries
} LOCALDICT, *PLOCALDICT;

#define BFT_bMAP 0x4d62

// bitmaps
typedef struct _INFBITMAPHEADER
{ // BITMAP FILE HEADER
    USHORT    usType;      // = 'bM';
    ULONG     cbSize;
    USHORT    xHotspot;
    USHORT    yHotspot;
    ULONG     offBits;     // =size(hdr)+size(colortbl)
    // BITMAP INFO HEADER
    ULONG     cbFix;       // =size(info_hdr) (usually = 12?)
    USHORT    cx;          // x size
    USHORT    cy;          // y size
    USHORT    cPlanes;     // color planes
    CHAR      cBitCount;
} INFBITMAPHEADER, *PINFBITMAPHEADER;

typedef struct _BMPDATAHEADER {
    ULONG ulTotalSize;
    USHORT usUncompPerBlock;
} BMPDATAHEADER, *PBMPDATAHEADER;

typedef struct _BMPDATABLOCK {
    USHORT usCompressedSize;
    UCHAR  ucCompressionType;
    BYTE   Data;
} BMPDATABLOCK, *PBMPDATABLOCK;

#pragma pack()


