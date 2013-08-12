
/*
 * inf2html.h:
 *      additional declarations used in inf2html.c only
 *      (not in INF books)
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

#define VERSION_NUMBER "0.91"

// structure for remembering formatting settings
// between several slots
typedef struct _FORMAT
{
    BOOL    fSpace,                 // TRUE: insert space after every "word"
            fMonoSpace,             // <PRE> mode
            fBold,                  // formatting
            fItalics,
            fUnderlined,
            fJustHadLinebreak,      // TRUE: last "word" was <P> or <BR>
            fJustHadLeftMargin,     // TRUE: last "word" was left margin format
            fJustHadBullet,         // TRUE: last "word" was unordered list bullet
            fOutputSpaceBeforeNextWord,
            fSuppressNextBR;
    ULONG   ulLeftMargin,           // current left margin
            ulLastLeftMargin;       // last left margin (keeps changing)
    ULONG   ulCurX;                 // no. of HTML characters put out for this
                                    // line; used for inserting line breaks into HTML
} FORMAT, *PFORMAT;

// structure for maintaining TOC entry info;
// this is used because in the INF file, this
// is really awkward to access
typedef struct _TOCENTRYINFO
{
    BYTE    bNestingLevel;          // entry level in TOC tree
    BOOL    fExtended,              // extended mode
            fHidden,                // entry is hidden
            fHasChildren,           // following TOC entries are higher
            fXPosSet,
            fYPosSet;
    BYTE    bNTocSlots;             // no. of slots for this TOC entry

    CHAR    szTocTitle[CCHMAXPATH], // title of article
            szHTMLFilestem[CCHMAXPATH]; // filestem of HTML output file

    BYTE    bExt1,
            bExt2,
            bBytes2Skip;
    PUSHORT pusSlotList;            // pointer to usTocslots[ntocslots] array
    PBYTE   pbExtraBytes;           // pointer to additional bytes for tocentry

} TOCENTRYINFO, *PTOCENTRYINFO;

#define BFT_BMAP    0x4d42
#define BFT_BITMAPARRAY 0x4142
// #define BCA_UNCOMP  0x00000000L
// #define BCA_RLE8    0x00000001L
// #define BCA_RLE4    0x00000002L
#define BCA_HUFFFMAN1D  0x00000003L
// #define BCA_RLE24   0x00000004L
#define MSWCC_EOL   0
#define MSWCC_EOB   1
#define MSWCC_DELTA 2



