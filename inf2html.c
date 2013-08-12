/*
 * inf2html.c:
 *      converts a compiled .INF or .HLP file into HTML code.
 *      Takes the file to be converted as command-line input.
 *
 *      Credits:
 *      --  Most of the information for creating this stems
 *          from the article in EDM/2 vol. 3 no. 8 about INF internals.
 *          Big thanks go out to Peter Childs for this.
 *          His work in turn is based on that of others. See inf03.txt
 *          for details.
 *      --  The bitmap decompression code at the bottom was
 *          written by Peter Fitzsimmons, who kindly sent me
 *          his code (pfitz@ican.net).
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

#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

#include <pmbitmap.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <setjmp.h>         // needed for exception handlers
#include <assert.h>         // needed for exception handlers

#include "inf.h"
#include "inf2html.h"

#include "ansi.h"

// #define DEBUG_SPACING

/********************************************************************
 *                                                                  *
 *  prototypes                                                      *
 *                                                                  *
 *******************************************************************/

PTOCENTRY DecodeTocHeader(ULONG ulTocEntry,
                          PTOCENTRYINFO ptei);
VOID TranslateArticle(FILE *file,
                      PTOCENTRY pteThis,
                      PTOCENTRYINFO ptei);
VOID TranslateSlot(FILE *file,
                   PTOCENTRYINFO ptei,
                   PFORMAT pFormat);
BOOL DecodeINFBitmap(PSZ pszBmpFile, ULONG ulBitmapOfs,
            PULONG pulWidth, PULONG pulHeight);
BOOL WriteBitmapFile(int hBitmapFile,
                PINFBITMAPHEADER pbmh,
                PBYTE pbRGBTable,
                PBYTE pbBits);
BOOL LZWDecompressBlock(PBYTE *ppbInput,
                    FILE *fOutput,
                    unsigned int number_bytes,
                    unsigned long *pBytesOut,
                    unsigned *pLastCode);


CHAR        szCurrentDir[CCHMAXPATH];
ULONG       cbCurrentDir = sizeof(szCurrentDir);

/********************************************************************
 *                                                                  *
 *  globals                                                         *
 *                                                                  *
 *******************************************************************/

// command line arguments
CHAR        szInfFile[CCHMAXPATH] = "",
            szOutputDir[CCHMAXPATH] = "",
            szInfFilestem[CCHMAXPATH] = "";
BOOL        optCreateFrames = FALSE,
            optRewriteTitle = FALSE,
            optDecodeBitmaps = FALSE,
            optExcplitAutoLinks = FALSE;
ULONG       ulVerboseMode = 0,
            ulNavigationMode = 0,
            ulMaxCharsPerLine = 70;

// file content
PBYTE       pbContent = NULL;

// INF header @ offset 0
PINFHEADER  pInfHeader = NULL;

// dictionary: array of CHAR pointers
PSZ         apszDictionary[65536] = {0};

// slots: array of SLOT pointers
PSLOT       aPSlots[65536];

// table of contents (TOC): array of TOCENTRY pointers
PTOCENTRY   aPTocEntries[65535];

// bitmap count
ULONG       ulBitmapCount = 0;

/********************************************************************
 *                                                                  *
 *  cmdline interface                                               *
 *                                                                  *
 *******************************************************************/

/*
 * Exit:
 *
 */

VOID Exit(ULONG ulExit)
{
    DosSetCurrentDir(szCurrentDir);
    DosExit(EXIT_PROCESS, ulExit);
}

/*
 * PrintHeader:
 *
 */

VOID PrintHeader(VOID)
{
    printf("\ninf2html V" VERSION_NUMBER " - (W)(C) 1998 Ulrich M”ller");
    printf("\n  This program comes with ABSOLUTELY NO WARRANTY. This is free software, ");
    printf("\n  and you are welcome to redistribute it under the conditions of the GNU ");
    printf("\n  General Public Licence, version 2, as specified in the 'COPYING' file of ");
    printf("\n  this distribution.\n");
}

/*
 * Explain:
 *
 */

VOID Explain(VOID)
{
    PrintHeader();
    printf("Command line usage:");
    printf("\n    inf2html [-w<num>] [-vVaFTnNb] <input> [<outdir>]");
    printf("\nwith:");
    printf("\n    <input>     being either an INF or a HLP file");
    printf("\n    <outdir>    directory for the output files; default is to create a new\n");
    printf("                subdirectory from the filestem of <input>\n");
    printf("General options:\n");
    printf("    -v          verbose mode, list all files\n");
    printf("    -V          very verbose mode, lotsa output\n");
    printf("HTML output options:\n");
    printf("    -w<num>     maximum no. of characters per HTML line (def: 70)\n");
    printf("    -F          create frames version (index.html)\n");
    printf("    -T          rewrite panel title on every page (header 1)\n");
    printf("    -n          create next/back links on every page\n");
    printf("    -N          dito, but include titles\n");
    printf("    -b          attempt to decode bitmaps (might crash)\n");
    printf("    -a          write out auto-links with target title\n");
    Exit(1);
}

/*
 * CheckForError:
 *      asserts successful completion of Dos* functions.
 *      Usage: CheckForError(Dos*(), "description");
 *      If the Dos* function returns an error, "description"
 *      is printed with the error code.
 */

VOID CheckForError(APIRET arc, PSZ pszAction)
{
    if (arc != NO_ERROR)
    {
        PSZ pszError = NULL;

        switch (arc) {
            case 110: pszError = "Open failed"; break;
            default: pszError = "unknown";
        }

        PrintHeader();
        printf("\nError %s: %s. Terminating.\n", pszAction, pszError);

        Exit(1);
    }
}

/*
 * strrpl:
 *      replace oldStr with newStr in str.
 *
 *      str should have enough allocated space for the replacement, no check
 *      is made for this. str and OldStr/NewStr should not overlap.
 *      The empty string ("") is found at the beginning of every string.
 *
 *      Returns: pointer to first location behind where NewStr was inserted
 *      or NULL if OldStr was not found.
 *      This is useful for multiple replacements also.
 *      (be careful not to replace the empty string this way !)
 *
 *      Author:     Gilles Kohl
 *      Started:    09.06.1992   12:16:47
 *      Modified:   09.06.1992   12:41:41
 *      Subject:    Replace one string by another in a given buffer.
 *                  This code is public domain. Use freely.
 */

PBYTE strrpl(PBYTE str, PBYTE oldStr, PBYTE newStr)
{
      int OldLen, NewLen;
      char *p, *q;

      if (NULL == (p = strstr(str, oldStr)))
            return p;
      OldLen = strlen(oldStr);
      NewLen = strlen(newStr);
      memmove(q = p+NewLen, p+OldLen, strlen(p+OldLen)+1);
      memcpy(p, newStr, NewLen);
      return (q);
}

/*
 * DumpBytes:
 *
 */

VOID DumpBytes(PSZ pszBuf, PBYTE pb, ULONG ulCount)
{
    PBYTE pb2 = pb;
    PSZ pszBuf2 = pszBuf;
    ULONG ul;
    for (ul = 0; ul < ulCount; ul++, pb2++)
        pszBuf2 += sprintf(pszBuf2, "%02lX ", *pb2);
}

/*
 * DecodeWindowBytes:
 *
 */

BOOL DecodeWindowBytes(PSZ pszBuf, PBYTE pb, ULONG ulCount, PSZ pszDebug)
{
    BOOL         fAutoLink = FALSE,
                 fSplit = FALSE,
                 fViewport = FALSE,
                 fDependent = FALSE,
                 fTargetGroup = FALSE,
                 fTargetPos = FALSE,
                 fTargetSize = FALSE;
    USHORT       usTargetGroup;
    ULONG        ulGroup = 2,
                 ulSize = 0;

    if (ulCount)
    {
        strcpy(pszDebug, "<!-- extra bytes: ");
        DumpBytes(pszDebug+strlen(pszDebug),
                pb,
                ulCount);
        strcat(pszDebug, "-->");

        // first extra byte
        if ((*(pb) & 0x40))
            fAutoLink = TRUE;
        if ((*(pb) & 0x80))
            fSplit = TRUE;
        if ((*(pb) & 0x04))
            fViewport = TRUE;
        if ((*(pb) & 0x01))
        {
            fTargetPos = TRUE;
            ulGroup += 5;
        }
        if ((*(pb) & 0x02))
        {
            fTargetSize = TRUE;
            ulSize = ulGroup;
            ulGroup += 5;
        }
        // second extra byte
        if ((*(pb+1) & 0x02))
            fDependent = TRUE;
        if ((*(pb+1) & 0x04))
        {
            fTargetGroup = TRUE;
            // if so, target group follows
            // in the next 2 bytes
            usTargetGroup =
                *(PUSHORT)(pb+ulGroup);
        }
    }

    if (fAutoLink)
        strcat(pszBuf, " AUTO");
    if (fSplit)
        strcat(pszBuf, " SPLIT");
    if (fViewport)
        strcat(pszBuf, " VIEWPORT");
    if (fDependent)
        strcat(pszBuf, " DEPENDENT");

    if (fTargetPos)
    {
        if (*(pb+2) & 0x40)
        {
            // dynamic x-positioning
            if (*(pb+3) & 0x01)
                strcat(pszBuf, " XPOS=LEFT");
            if (*(pb+3) & 0x02)
                strcat(pszBuf, " XPOS=RIGHT");
            if (*(pb+3) & 0x10)
                strcat(pszBuf, " XPOS=CENTER");
        }
        else if (*(pb+2) & 0x10)
        {
            // relative (percentage) x-positioning
            sprintf(pszBuf+strlen(pszBuf), " XPOS=%d",
                *(PUSHORT)&(*(pb+3)));
            strcat(pszBuf, "%");
        }

        if (*(pb+2) & 0x04)
        {
            // dynamic y-positioning
            if (*(pb+5) & 0x08)
                strcat(pszBuf, " YPOS=BOTTOM");
            if (*(pb+5) & 0x04)
                strcat(pszBuf, " YPOS=TOP");
            if (*(pb+5) & 0x10)
                strcat(pszBuf, " YPOS=CENTER");
        }
        else if (*(pb+2) & 0x01)
        {
            // relative (percentage) x-positioning
            sprintf(pszBuf+strlen(pszBuf), " YPOS=%d",
                *(PUSHORT)&(*(pb+5)));
            strcat(pszBuf, "%");
        }
    }

    if (fTargetSize)
    {
        if (*(pb+ulSize) & 0x40)
        {
            // dynamic x-positioning
            if (*(pb+ulSize+1) & 0x01)
                strcat(pszBuf, " WIDTH=LEFT");
            if (*(pb+ulSize+1) & 0x02)
                strcat(pszBuf, " WIDTH=RIGHT");
            if (*(pb+ulSize+1) & 0x10)
                strcat(pszBuf, " WIDTH=CENTER");
        }
        else if (*(pb+ulSize) & 0x10)
        {
            // relative (percentage) x-positioning
            sprintf(pszBuf+strlen(pszBuf), " WIDTH=%d",
                *(PUSHORT)&(*(pb+ulSize+1)));
            strcat(pszBuf, "%");
        }

        if (*(pb+ulSize) & 0x04)
        {
            // dynamic y-positioning
            if (*(pb+ulSize+3) & 0x08)
                strcat(pszBuf, " HEIGHT=BOTTOM");
            if (*(pb+ulSize+3) & 0x04)
                strcat(pszBuf, " HEIGHT=TOP");
            if (*(pb+ulSize+3) & 0x10)
                strcat(pszBuf, " HEIGHT=CENTER");
        }
        else if (*(pb+ulSize) & 0x01)
        {
            // relative (percentage) x-positioning
            sprintf(pszBuf+strlen(pszBuf), " HEIGHT=%d",
                *(PUSHORT)&(*(pb+ulSize+3)));
            strcat(pszBuf, "%");
        }
    }

    if (fTargetGroup)
        sprintf(pszBuf+strlen(pszBuf), " GROUP=%d",
            usTargetGroup);
    return (fAutoLink);
}

/*
 * main:
 *      parses cmdline options and then sets up
 *      a lot of data structures from the read-in
 *      inf file; finally, it goes thru the table
 *      of contents (TOC) to produce HTML output
 *      files by calling subroutines
 */

int main(int argc, char *argv[])
{
    HFILE       hfInf;
    APIRET      arc;
    ULONG       ulAction = 0,
                ulBytesRead = 0;
    FILESTATUS3 fs3;

    PBYTE       pbTemp;
    PULONG      pulTemp;

    ULONG       ul, ulTocEntry;

    CHAR        szMainIndexFile[CCHMAXPATH];
    FILE        *MainIndexFile;
    BYTE        bLastNestingLevel = 0;

    // parse cmdline parameters

    if (argc < 2) {
        // we need at least one input file
        Explain();
    } else {
        // parse arguments
        SHORT i = 0;
        while (i++ < argc-1) {
            if (argv[i][0] == '-') {
                SHORT i2;
                for (i2 = 1; i2 < strlen(argv[i]); i2++) {
                    switch (argv[i][i2]) {
                        case 'v':
                            ulVerboseMode = 1;
                        break;

                        case 'V':
                            ulVerboseMode = 2;
                        break;

                        case 'F':
                            optCreateFrames = TRUE;
                        break;

                        case 'T':
                            optRewriteTitle = TRUE;
                        break;

                        case 'n':
                            ulNavigationMode = 1;
                        break;

                        case 'N':
                            ulNavigationMode = 2;
                        break;

                        case 'b':
                            optDecodeBitmaps = TRUE;
                        break;

                        case 'a':
                            optExcplitAutoLinks = TRUE;
                        break;

                        case 'w': {
                            if (sscanf(&(argv[i][i2+1]), "%d", &ulMaxCharsPerLine) == 0)
                                Explain();
                            printf("%d", ulMaxCharsPerLine);
                            // skip rest of this arg
                            i2 = strlen(argv[i]);
                        break; }

                        default:  // unknown parameter
                            Explain();
                        break;
                    }
                }
            }
            else {
                // no option ("-"): seems to be file
                if (szInfFile[0] == 0)
                    // first no-option argument: must be input file
                    strcpy(szInfFile, argv[i]);
                else
                    // second no-option argument: must be ouput dir
                    strcpy(szOutputDir, argv[i]);
            }
        }
    }

    // OK, continue

    // open INF/HLP file
    CheckForError(DosOpen(szInfFile, &hfInf, &ulAction,
                            0, 0,    // write flags
                            OPEN_ACTION_FAIL_IF_NEW
                                | OPEN_ACTION_OPEN_IF_EXISTS,
                            OPEN_FLAGS_NOINHERIT
                                | OPEN_SHARE_DENYWRITE
                                | OPEN_ACCESS_READONLY,
                            NULL), // EAs for writing
                    "opening input file");

    // open successful: read complete file into buffer
    CheckForError(DosQueryFileInfo(hfInf, FIL_STANDARD, &fs3, sizeof(fs3)),
                    "querying file size");
    pbContent = malloc(fs3.cbFile);
    printf("Reading contents of \"%s\" (%d bytes)...\n", szInfFile, fs3.cbFile);
    CheckForError(DosRead(hfInf, pbContent, fs3.cbFile, &ulBytesRead),
                    "reading contents");
    // close input file
    CheckForError(DosClose(hfInf), "closing file");

    // INF header is at beginning
    pInfHeader = (PINFHEADER)pbContent;

    if (pInfHeader->usMagicID != 0x5348) {
        // no INF/HLP file: stop
        PrintHeader();
        printf("Error: \"%s\" is not an INF or HLP file.\n", szInfFile);
        Exit(1);
    }

    if (ulVerboseMode == 2) {
        printf("Header information:\n");
        printf("  Title: \"%s\"\n", pInfHeader->szTitle);
        printf("  %s style\n", (pInfHeader->bFlags & 1) ? "INF" : "HLP");
        printf("  No. of TOC entries:   %5d\n", pInfHeader->usNToc);
        printf("  No. of res panels:    %5d\n", pInfHeader->usNRes);
        printf("  No. of named panels:  %5d\n", pInfHeader->usNName);
        printf("  No. of index entries: %5d\n", pInfHeader->usNIndex);
        printf("  No. of slots:         %5d\n", pInfHeader->usNSlots);
        printf("  No. of dict entries:  %5d\n", pInfHeader->usNDict);
        printf("  Dict offset:          %5d\n", pInfHeader->ulDictStart);
    }

    // find master dictionary:
    pbTemp = (PBYTE)(pbContent + (pInfHeader->ulDictStart));
    memset(&apszDictionary, sizeof(apszDictionary), 0);

    // set up an array of PSZs to the dictionary entries
    // to speed up processing; otherwise we couldn't use
    // the str* C library functions, because the dictionary
    // doesn't use zero-terminated strings (duh)
    if (ulVerboseMode == 2)
        printf("Getting dictionary entries... \n");
    for (ul = 0; ul < pInfHeader->usNDict; ul++)
    {
        BYTE bSizeThis = *pbTemp;
        PSZ  pszThis = malloc(bSizeThis+50);

        memcpy(pszThis, pbTemp+1, bSizeThis-1);
        pszThis[bSizeThis-1] = 0;
        apszDictionary[ul] = pszThis;

        if (ulVerboseMode == 2)
            if (((ul % 50) == 0) || (ul == pInfHeader->usNDict - 1)) {
                printf("  Item %5d out of %5d\n", ul+1, pInfHeader->usNDict);
                ANSI_up(1);
            }

        pbTemp += bSizeThis;
    }

    // translate special characters
    if (ulVerboseMode == 2)
        printf("\nTranslating special characters... \n");
    for (ul = 0; ul < pInfHeader->usNDict; ul++)
    {
        strrpl(apszDictionary[ul], "&", "&amp;");
        strrpl(apszDictionary[ul], "<", "&lt;");
        strrpl(apszDictionary[ul], ">", "&gt;");
        strrpl(apszDictionary[ul], "„", "&auml;");
        strrpl(apszDictionary[ul], "”", "&ouml;");
        strrpl(apszDictionary[ul], "", "&uuml;");
        strrpl(apszDictionary[ul], "á", "&szlig;");
        strrpl(apszDictionary[ul], "Ž", "&Auml;");
        strrpl(apszDictionary[ul], "™", "&Ouml;");
        strrpl(apszDictionary[ul], "š", "&Uuml;");
    }

    // find slots table
    if (ulVerboseMode == 2)
        printf("Getting slots... \n");
    pulTemp = (PULONG)(pbContent + (pInfHeader->ulSlotsStart));
    memset(&aPSlots, sizeof(aPSlots), 0);

    // set up an array of PSLOTs pointing to the slots in the file
    for (ul = 0; ul < pInfHeader->usNSlots; ul++)
    {
        aPSlots[ul] = (PSLOT)(pbContent + (*pulTemp));

        if (ulVerboseMode == 2)
            if (((ul % 50) == 0) || (ul == pInfHeader->usNSlots - 1)) {
                printf("  Slot %5d out of %5d\n", ul+1, pInfHeader->usNSlots);
                ANSI_up(1);
            }
        pulTemp++;
    }

    // get table of contents (TOC)
    if (ulVerboseMode == 2)
        printf("\nGetting table of contents... \n");
    pulTemp = (PULONG)(pbContent + (pInfHeader->ulTocStart));
    memset(&aPTocEntries, sizeof(aPTocEntries), 0);

    // set up an array of PTOCENTRYs
    for (ul = 0; ul < pInfHeader->usNToc; ul++)
    {
        aPTocEntries[ul] = (PTOCENTRY)(pbContent + (*pulTemp));

        if (ulVerboseMode == 2)
            if (((ul % 50) == 0) || (ul == pInfHeader->usNToc - 1)) {
                printf("  TOC entry %5d out of %5d\n", ul+1, pInfHeader->usNToc);
                ANSI_up(1);
            }
            pulTemp++;
    }
    if (ulVerboseMode == 2)
        printf("\n");

    // prepare output: create directory, if neccessary
    _splitpath(szInfFile, NULL, NULL, szInfFilestem, NULL);
    if (szOutputDir[0] == 0)
        strcpy(szOutputDir, szInfFilestem);
    printf("Output goes to dir \"%s\"\n", szOutputDir);
    DosCreateDir(szOutputDir, NULL);
    DosQueryCurrentDir(0, szCurrentDir, &cbCurrentDir);
    CheckForError(DosSetCurrentDir(szOutputDir), "creating target directory");

    if (ulVerboseMode > 0)
        printf("Creating HTML files... \n");
    // create main index file; we'll keep this open while processing
    // and add a new link to every HTML file we've created
    sprintf(szMainIndexFile, "%s.html", szInfFilestem);
    MainIndexFile = fopen(szMainIndexFile, "w");
    fprintf(MainIndexFile, "<HTML>\n<HEAD>\n");
    fprintf(MainIndexFile, "<TITLE>%s</TITLE>\n", pInfHeader->szTitle);
    fprintf(MainIndexFile, "</HEAD>\n<BODY>\n");

    // *** translate

    // now go thru all the TOC entries and produce HTML files
    for (ulTocEntry = 0; ulTocEntry < pInfHeader->usNToc; ulTocEntry++)
    {
        TOCENTRYINFO tei;
        PTOCENTRY pteThis;
        FILE    *file;
        ULONG   ul1, ul2;
        CHAR    szHTMLFile[CCHMAXPATH],
                szTemp[400],
                szDebug[400];

        if (ulVerboseMode == 2)
            printf("Decoding TOC entry...\n");

        // have the current TOC entry decoded; this will
        // fill the TOCENTRYINFO structure above and also
        // return a PTOCENTRY for the current entry (ulTocEntry)
        pteThis = DecodeTocHeader(ulTocEntry, &tei);

        // open HTML output file
        sprintf(szHTMLFile, "%s.html", tei.szHTMLFilestem);
        file = fopen(szHTMLFile, "w");

        if (ulVerboseMode == 2) {
            printf("    Title: \"%s\"\n", tei.szTocTitle);
            printf("    TOC level: %d, slots: %d\n",
                        tei.bNestingLevel,
                        pteThis->bNTocSlots);
            printf("    Flags: ");
            if (tei.fHidden)
                printf("hidden ");
            if (tei.fHasChildren)
                printf("hasChildren ");
            if (tei.fExtended)
                printf("extended");
            printf("\n");
        }

        if (ulVerboseMode > 0) {
            printf("    %d/%d: Writing \"%s.html\"... ",
                    ulTocEntry+1, pInfHeader->usNToc,
                    tei.szHTMLFilestem);
            ANSI_savecurs();
        } else {
            printf("Writing file %3d out of %3d (%3d%%)...\n",
                    ulTocEntry+1, pInfHeader->usNToc,
                    ((ulTocEntry+1)*100) / pInfHeader->usNToc);
            ANSI_up(1);
        }

        // store reference in main index file
        while (bLastNestingLevel < tei.bNestingLevel) {
            fprintf(MainIndexFile, "<UL>");
            bLastNestingLevel ++;
        }
        while (bLastNestingLevel > tei.bNestingLevel) {
            fprintf(MainIndexFile, "</UL>");
            bLastNestingLevel--;
        }
        if (!tei.fHidden)
            fprintf(MainIndexFile, "<LI><A HREF=\"%s.html\"%s>%s</A>\n",
                    tei.szHTMLFilestem,
                    (optCreateFrames) ? " TARGET=\"main\"" : "",
                    tei.szTocTitle);

        // write HTML header for new file
        szTemp[0] = 0;
        if (tei.fExtended)
            DecodeWindowBytes(szTemp,
                              tei.pbExtraBytes,
                              tei.bBytes2Skip,
                              szDebug);
        if (tei.fHidden)
            strcat(szTemp, "HIDDEN");

        fprintf(file, "<HTML%s>\n<HEAD>\n", szTemp);
        if (tei.fExtended)
            fprintf(file, szDebug);
        /* if (tei.fExtended) {
            CHAR szTemp2[400];
            sprintf(szTemp2, "<!-- bExt1: 0x%lX, bExt2: 0x%lX, skipped bytes: %d\n",
                          tei.bExt1, tei.bExt2, tei.bBytes2Skip);
            DumpBytes(szTemp2+strlen(szTemp2), tei.pbExtraBytes, tei.bBytes2Skip);
            strcat(szTemp2, "-->\n");
            fprintf(file, szTemp2);
        } */
        fprintf(file, "<TITLE>%s</TITLE>\n", tei.szTocTitle);
        fprintf(file, "</HEAD>\n<BODY>\n");
        if (optRewriteTitle)
            fprintf(file, "<H1>%s</H1>", tei.szTocTitle);

        TranslateArticle(file, pteThis, &tei);

        // add navigation links for next/back, if desired
        if (ulNavigationMode > 0) {
            TOCENTRYINFO tei2;
            fprintf(file, "\n\n<P><HR>\n");
            if (ulTocEntry > 0) {
                DecodeTocHeader(ulTocEntry-1, &tei2);
                fprintf(file, "\n<A HREF=\"%s.html\">[%s%s]</A> ",
                        tei2.szHTMLFilestem,
                        (ulNavigationMode == 2) ? "Back: " : "Back",
                        (ulNavigationMode == 2) ? tei2.szTocTitle : "");
            }
            if (ulNavigationMode == 2)
                fprintf(file, "<BR>");
            if (ulTocEntry < pInfHeader->usNToc-1) {
                DecodeTocHeader(ulTocEntry+1, &tei2);
                fprintf(file, "\n<A HREF=\"%s.html\">[%s%s]</A> ",
                        tei2.szHTMLFilestem,
                        (ulNavigationMode == 2) ? "Next: " : "Next",
                        (ulNavigationMode == 2) ? tei2.szTocTitle : "");
            }
        }

        // close HTML output file
        fprintf(file, "\n</BODY>\n</HTML>\n");
        fclose(file);
    }
    // done with all files:

    // close main index file
    if (ulVerboseMode > 0)
        printf("Writing main index \"%s\"... \n", szMainIndexFile);
    fprintf(MainIndexFile, "\n</BODY>\n</HTML>\n");
    fclose(MainIndexFile);

    // create "index.html" if frame mode
    if (optCreateFrames) {
        TOCENTRYINFO tei;
        if (ulVerboseMode > 0)
            printf("Writing frameset file \"index.html\" ... \n");
        DecodeTocHeader(0, &tei);
        MainIndexFile = fopen("index.html", "w");
        fprintf(MainIndexFile, "<HTML>\n<HEAD>\n");
        fprintf(MainIndexFile, "<TITLE>%s</TITLE>\n", pInfHeader->szTitle);
        fprintf(MainIndexFile, "</HEAD>");
        fprintf(MainIndexFile, "\n<FRAMESET cols=\"30%%, *\">");
        fprintf(MainIndexFile, "\n<FRAME NAME=\"navigate\" SRC=\"%s\">", szMainIndexFile);
        fprintf(MainIndexFile, "\n<FRAME NAME=\"main\" SRC=\"%s.html\">",
                        tei.szHTMLFilestem);
        fprintf(MainIndexFile, "\n</FRAMESET>");
        fprintf(MainIndexFile, "\n<BODY>\n");
        fprintf(MainIndexFile, "\n</BODY>\n</HTML>\n");
        fclose(MainIndexFile);
    }

    printf("\nDone!\n");

    Exit(0);
    return (0); // keep compiler happy
}

/********************************************************************
 *                                                                  *
 *  INF-to-HTML logic                                               *
 *                                                                  *
 *******************************************************************/

/*
 * Output:
 *      writes HTML output, monitoring the line
 *      length
 */

VOID Output(FILE *file, PSZ pszOutput, PFORMAT pFormat)
{
    fwrite(pszOutput, 1, strlen(pszOutput), file);
    if (pFormat)
        pFormat->ulCurX += strlen(pszOutput);
}

/*
 * DecodeTocHeader:
 *      this takes an index in to the table of contents (TOC)
 *      in the INF file and returns the corresponding TOCENTRY
 *      pointer. Since that structure is somewhat obscure
 *      with variable length struct entries, this is translated
 *      into a fixed TOCENTRYINFO structure.
 */

PTOCENTRY DecodeTocHeader(ULONG ulTocEntry,     // in: TOC index
                          PTOCENTRYINFO ptei)   // out: TOC entry info
{
    CHAR        szTemp[CCHMAXPATH];
    ULONG   ul1, ul2;
    CHAR    *pachTitle;

    PTOCENTRY   pteThis = aPTocEntries[ulTocEntry];
    PBYTE   pb = ((PBYTE)(pteThis) + sizeof(TOCENTRY));

    // now translate the TOCENTRY into TOCENTRYINFO
    ptei->bNestingLevel = (pteThis->bFlags & 0x0F);
    ptei->fExtended    = ((pteThis->bFlags & 0x20) != 0),
    ptei->fHidden      = ((pteThis->bFlags & 0x40) != 0),
    ptei->fHasChildren = ((pteThis->bFlags & 0x80) != 0),
    ptei->fXPosSet = FALSE,
    ptei->fYPosSet = FALSE;
    ptei->bNTocSlots = pteThis->bNTocSlots;

    // "extended" flag on: extra bytes follow (see inf.h)
    if (ptei->fExtended) {
        ULONG ul3 = 0;
        ptei->bBytes2Skip = 2; // always skip at least the first two bytes
        ptei->pbExtraBytes = pb;
        ptei->bExt1 = *(PBYTE)(pb);
        ptei->bExt2 = *(PBYTE)(pb + 1);
        if (ptei->bExt1 & 0x8)
            ptei->bBytes2Skip += 2;
        if (ptei->bExt1 & 0x1) {
            ptei->fXPosSet = TRUE;
            ptei->bBytes2Skip += 5;
        }
        if (ptei->bExt1 & 0x2) {
            ptei->fYPosSet = TRUE;
            ptei->bBytes2Skip += 5;
        }
        if (ptei->bExt2 & 0x4)
            ptei->bBytes2Skip += 2;

        if (ulVerboseMode == 2) {
            printf("  Skipping %d bytes: ", ptei->bBytes2Skip);
            for (ul3 = 0; ul3 < ptei->bBytes2Skip; ul3++)
                printf("%lX ", *(PBYTE)(pb+ul3));
            printf("\n");
        }

        // skip
        pb += ptei->bBytes2Skip;
    }

    // next byte is PUSHORT to slots
    ptei->pusSlotList = (PUSHORT)(pb);

    // copy TOC entry title
    pachTitle = (CHAR*)(pb + (sizeof(USHORT) * ptei->bNTocSlots) );
    ul1 = 0;
    while (pachTitle < (PBYTE)(pteThis)
                        + (pteThis->bLen))
    {
        ptei->szTocTitle[ul1] = *pachTitle;
        ul1++;
        pachTitle++;
    }
    ptei->szTocTitle[ul1] = 0;

    // compose HTML output filename:
    ul1 = 0;
    ul2 = 0;
    while ((ptei->szTocTitle[ul2]) && (ul1 < 20))
    {
        // skip characters which we don't want in filename
        if (    (!strchr(".<>?*|+ = -:;, /[]()\\\"\' ", ptei->szTocTitle[ul2]))
             && (ptei->szTocTitle[ul2] > (CHAR)32)
           )
        {
            szTemp[ul1] = ptei->szTocTitle[ul2];
            ul1++;
        }
        ul2++;
    }
    szTemp[ul1] = 0;
    // format is: <TOC entry index>_L<level>_<title>
    // "H" is added to the level if the entry is hidden.
    // Note that ".html" is not added here, because the
    // filestem is needed for bitmap files also.
    sprintf(ptei->szHTMLFilestem, "%03d_L%d%s_%s",
            ulTocEntry,
            ptei->bNestingLevel,
            (ptei->fHidden) ? "H" : "",
            szTemp);

    return (pteThis);
}

/*
 * TranslateArticle:
 *
 */

VOID TranslateArticle(FILE *file,          // output file (from fopen)
                      PTOCENTRY pteThis,
                      PTOCENTRYINFO ptei)     // info structure maintained by main()
{
    USHORT  usSlot;
    FORMAT  Format = {0};
    CHAR    szTemp[100];
    // reset format info structure (we need to maintain
    // formatting info across several slots)
    memset(&Format, sizeof(Format), 0);
    Format.fSpace = TRUE;

    for (usSlot = 0; usSlot < pteThis->bNTocSlots; usSlot++)
    {
        if (ulVerboseMode > 0) {
            ANSI_restcurs();
            printf("%3d%%", (usSlot *100 / (pteThis->bNTocSlots+1)));
            fflush(stdout);
        }

        sprintf(szTemp, "<!-- entering slot %d -->", *(ptei->pusSlotList));
        Output(file, szTemp, &Format);

        // decode!! this will write HTML code
        TranslateSlot(file, ptei, &Format);

        // next slot
        ptei->pusSlotList++;
    }

    if (ulVerboseMode > 0) {
        ANSI_restcurs();
        printf("100\n");
    }
}

/*
 * TranslateSlot:
 *      goes through the contents of one slot and
 *      puts out HTML text into "file", using the
 *      local and global dictionaries and all that.
 *      Each article is composed of at least one
 *      slot; is has several slots if the localdict
 *      contains more than 250 different words.
 */

VOID TranslateSlot(FILE *file,          // output file (from fopen)
                   PTOCENTRYINFO ptei,  // info structure from DecodeTocHeader
                   PFORMAT pFormat)     // formatting info; since formatting flags
                           // need to be maintained across slots, if one panel has
                           // several slots, we need a permanent structure
{
    // get current slot number (updated by main())
    USHORT usSlot = *(ptei->pusSlotList);

    if (usSlot < pInfHeader->usNSlots)
    {

        // get pointer to slot structure in INF file data
        PSLOT       pSlotThis = aPSlots[usSlot];
        ULONG       ulCode;

        // get pointer to slot's local dictionary
        PLOCALDICT  pLocalDict = (PLOCALDICT)(pbContent + (pSlotThis->ulLocalDictStart));

        // now go through the encoded data of this slot byte by byte
        for (ulCode = 0 ; ulCode < pSlotThis->usNText; ulCode++ )

            switch(pSlotThis->abText[ulCode])
            {
                // check for special codes:

                // end of paragraph, sets space to TRUE
                case 0xfa:
                    if (!pFormat->fJustHadBullet)
                        if (pFormat->ulLeftMargin < pFormat->ulLastLeftMargin)
                        {
                            Output(file, "\n</UL>", pFormat);
                            pFormat->fJustHadBullet = FALSE;
                            pFormat->ulLastLeftMargin = pFormat->ulLeftMargin;
                            break;
                        }

                    if (!pFormat->fMonoSpace)
                        fprintf(file, "\n<P>\n");
                    else
                        fprintf(file, "\n");
                    pFormat->ulCurX = 0;
                    pFormat->fSpace = TRUE;
                    pFormat->fJustHadLinebreak = TRUE;
                    pFormat->fOutputSpaceBeforeNextWord = FALSE;
                    break;

                // line break, set space to TRUE if not monospaced example
                case 0xfd:
                    if (!pFormat->fJustHadBullet)
                        if (pFormat->ulLeftMargin < pFormat->ulLastLeftMargin)
                        {
                            fprintf(file, "\n</UL>");
                            pFormat->ulCurX = 5;
                            pFormat->fJustHadBullet = FALSE;
                            pFormat->ulLastLeftMargin = pFormat->ulLeftMargin;
                            pFormat->fSuppressNextBR = TRUE;
                            pFormat->fOutputSpaceBeforeNextWord = FALSE;
                            break;
                        }

                    if (!pFormat->fSuppressNextBR)
                    {
                        if (!(pFormat->fMonoSpace)) {
                            fprintf(file, "\n<BR>\n");
                            pFormat->fSpace = TRUE;
                            pFormat->fJustHadLinebreak = TRUE;
                        } else
                            fprintf(file, "\n");
                        pFormat->ulCurX = 0;
                        pFormat->fOutputSpaceBeforeNextWord = FALSE;
                    } else
                        pFormat->fSuppressNextBR = FALSE;
                break;

                // [unknown]
                case 0xfb:
                    Output(file, "<!--0xfb-->", pFormat);
                    break;

                // spacing = !spacing
                case 0xfc:
                    pFormat->fSpace = !(pFormat->fSpace);
                    #ifdef DEBUG_SPACING
                        fprintf(file, "<!--spacing %d-->", pFormat->fSpace);
                    #endif
                    break;

                // space
                case 0xfe:
                    // if the line is long enough, put out linebreak
                    if (pFormat->ulCurX > ulMaxCharsPerLine) {
                        fprintf(file, "\n");
                        pFormat->ulCurX = 0;
                        pFormat->fOutputSpaceBeforeNextWord = FALSE;
                    } else {
                        // else normal space
                        fprintf(file, " ");
                        pFormat->ulCurX++;
                    }
                break;

                // escape code
                case 0xff: {
                    BYTE bEscLen  = pSlotThis->abText[ulCode+1];
                    BYTE bEscCode = pSlotThis->abText[ulCode+2];

                    switch (bEscCode)
                    {
                        // formatting flags:
                        case 0x04: {
                            BYTE bFormat = pSlotThis->abText[ulCode+3];
                            switch (bFormat) {
                                case 1: // italics
                                    Output(file, "<I>", pFormat);
                                    pFormat->fItalics = TRUE;
                                    break;
                                case 2: // bold
                                    Output(file, "<B>", pFormat);
                                    pFormat->fBold = TRUE;
                                    break;
                                case 3: // bold italics
                                    Output(file, "<B><I>", pFormat);
                                    pFormat->fBold = TRUE;
                                    pFormat->fItalics = TRUE;
                                    break;
                                case 5: // underlined
                                    Output(file, "<U>", pFormat);
                                    pFormat->fUnderlined = TRUE;
                                    break;
                                case 6: // italics underlined
                                    Output(file, "<U><I>", pFormat);
                                    pFormat->fUnderlined = TRUE;
                                    pFormat->fItalics = TRUE;
                                    break;
                                case 7: // bold underlined
                                    Output(file, "<B><U>", pFormat);
                                    pFormat->fBold = TRUE;
                                    pFormat->fUnderlined = TRUE;
                                    break;
                                case 0: // reset (plain text)
                                    if (pFormat->fBold)
                                        Output(file, "</B>", pFormat);
                                    if (pFormat->fItalics)
                                        Output(file, "</I>", pFormat);
                                    if (pFormat->fUnderlined)
                                        Output(file, "</U>", pFormat);
                                    pFormat->fBold = FALSE;
                                    pFormat->fItalics = FALSE;
                                    pFormat->fUnderlined = FALSE;
                                break;
                            }
                        break; }

                        // begin link
                        case 0x05:  // "real" link
                        case 0x07:  // footnote
                        {
                            // the next two bytes are are a USHORT index into
                            // the TOC array
                            PUSHORT pusTarget = (PUSHORT)&(pSlotThis->abText[ulCode+3]);
                            TOCENTRYINFO teiTarget;
                            CHAR         szTemp[400] = "",
                                         szDebug[400] = "";
                            ULONG        ul;
                            BOOL         fAutoLink = FALSE;

                            // set up HTML filename to link to
                            DecodeTocHeader(*pusTarget, &teiTarget);

                            strcpy(szTemp, "<A");
                            if (bEscLen >= 5)
                                fAutoLink = DecodeWindowBytes(szTemp+strlen(szTemp),
                                        &(pSlotThis->abText[ulCode+5]),
                                        bEscLen-4,
                                        szDebug);

                            sprintf(szTemp+strlen(szTemp), " HREF=\"%s.html\">",
                                    teiTarget.szHTMLFilestem);
                            if (fAutoLink) {
                                if (optExcplitAutoLinks) {
                                    strcat(szTemp, "[Autolink] ");
                                    strcat(szTemp, teiTarget.szTocTitle);
                                }
                                strcat(szTemp, "</A><P>");
                            }

                            if (pFormat->fOutputSpaceBeforeNextWord) {
                                Output(file, " ", pFormat);
                                pFormat->fOutputSpaceBeforeNextWord = FALSE;
                            }
                            Output(file, szTemp, pFormat);
                            Output(file, szDebug, pFormat);
                        break; }

                        // end link:
                        case 0x08: {
                            Output(file, "</A>", pFormat);
                        break; }

                        // left margin:
                        case 0x2:
                        case 0x11:
                        case 0x12: {
                            CHAR szTemp[100];
                            ULONG ulNewLeftMargin = pSlotThis->abText[ulCode+3];
                            sprintf(szTemp, "<!-- lm: 0x%lX %d -->",
                                    bEscCode, ulNewLeftMargin);
                            Output(file, szTemp, pFormat);
                            pFormat->ulLeftMargin = ulNewLeftMargin;
                            pFormat->fJustHadLeftMargin = TRUE;
                            if (!pFormat->fJustHadBullet)
                                if (ulNewLeftMargin > pFormat->ulLastLeftMargin)
                                {
                                    Output(file, "\n<UL>", pFormat);
                                    pFormat->fJustHadBullet = FALSE;
                                    pFormat->ulLastLeftMargin = ulNewLeftMargin;
                                }
                        break; }

                        // begin monospace:
                        case 0x0B:
                            if (bEscLen == 0x02) {
                                pFormat->fMonoSpace = TRUE;  // begin monospaced example!
                                pFormat->fSpace = FALSE;
                                fprintf(file, "\n<PRE>");
                                pFormat->ulCurX = 5;
                                pFormat->fOutputSpaceBeforeNextWord = FALSE;
                            }
                        break;

                        // end monospace
                        case 0x0C:
                            if (bEscLen == 0x02) {
                                pFormat->fMonoSpace = FALSE;  // end monospaced example!
                                pFormat->fSpace = TRUE;
                                fprintf(file, "</PRE>\n");
                                pFormat->ulCurX = 0;
                                pFormat->fOutputSpaceBeforeNextWord = FALSE;
                            }
                        break;

                        // bitmap
                        case 0x0E: {
                            CHAR szBmpFile[CCHMAXPATH],
                                 szTemp[400];
                            BYTE bBitmapFlags = pSlotThis->abText[ulCode+3];
                            ULONG ulBitmapOfs = *(PULONG)&(pSlotThis->abText[ulCode+4]);
                            ULONG ulWidth, ulHeight;

                            sprintf(szBmpFile, "%s_%d.bmp",
                                    ptei->szHTMLFilestem,
                                    ulBitmapCount);

                            if (!DecodeINFBitmap(szBmpFile, ulBitmapOfs,
                                        &ulWidth, &ulHeight))
                                Output(file, "<!-- Unable to decode bitmap format -->",
                                        pFormat);

                            sprintf(szTemp, "<IMG SRC=\"%s_%d.gif\" WIDTH=%d HEIGHT=%d",
                                    ptei->szHTMLFilestem,
                                    ulBitmapCount,
                                    ulWidth, ulHeight);
                            /* if (bBitmapFlags & 0x01)
                                strcat(szTemp, " ALIGN=left"); */
                                        // left not needed
                            if (bBitmapFlags & 0x02)
                                strcat(szTemp, " ALIGN=right");
                            if (bBitmapFlags & 0x04)
                                strcat(szTemp, " ALIGN=center");
                            strcat(szTemp, ">");
                            /* if (bBitmapFlags & 0x08 ) printf ("Fit ");
                                strcat(szTemp, "left");
                            if (bBitmapFlags & 0x10 ) printf ("Runin ");  // 00010000
                                strcat(szTemp, "left");   */
                            Output(file, szTemp, pFormat);
                            ulBitmapCount++;
                        break; }

                        // link to external file
                        case 0x1D: {
                            CHAR szTemp[400];
                            PUSHORT pusTarget = (PUSHORT)&(pSlotThis->abText[ulCode+3]);
                            int i;
                            /* sprintf(szTemp, "<!-- external link: ");
                            strcat(szTemp, pbContent+
                                           pInfHeader->ulExtStart
                                           + (*pusTarget));
                                           // );
                            strcat(szTemp, " -->");
                            Output(file, szTemp, pFormat); */
                            strcpy(szTemp, "<A><!-- external link: ");
                            DumpBytes(szTemp+strlen(szTemp),
                                &(pSlotThis->abText[ulCode+2]),
                                bEscLen);
                            strcat(szTemp, "-->");
                            Output(file, szTemp, pFormat);
                            /* for (i = 0; i < 4096; i+=128)
                            {
                                fprintf(file, "%04lX: ", i);
                                fwrite(pbContent+
                                       pInfHeader->ulExtStart
                                       // + (*pusTarget)
                                       + i,
                                       1, 128, file);
                                fprintf(file, "\n");
                            } */
                        break; }

                        default: {
                            /*  */
                        }

                    } // end switch (bEscCode)

                    ulCode = ulCode + bEscLen; // skip the esccode!
                break; }

                // other code: should be offset into dictionaries
                default: {
                    BYTE  bLocalDictOfs = pSlotThis->abText[ulCode];
                    USHORT usGlobalDictOfs = pLocalDict->usEntries[bLocalDictOfs];

                    if (    (pFormat->fJustHadLinebreak)
                         && (pFormat->fJustHadLeftMargin)
                         && (!pFormat->fMonoSpace)
                       )
                    {
                        // graphics character? seems to be list bullet
                        if (    (strlen(apszDictionary[usGlobalDictOfs]) == 1)
                             && ( (isalpha(*apszDictionary[usGlobalDictOfs]) == 0) )
                           )
                        {
                            pFormat->fJustHadBullet = TRUE;
                            Output(file, "<LI>", pFormat);
                            break; // don't print explicit bullet
                            /* if (pFormat->ulLastLeftMargin == pFormat->ulLeftMargin)
                                Output(file, "<LI>", pFormat);
                            else if (pFormat->ulLastLeftMargin > pFormat->ulLeftMargin) {
                                Output(file, "</UL><LI>", pFormat);
                                pFormat->ulLastLeftMargin = pFormat->ulLeftMargin;
                            } else if (pFormat->ulLastLeftMargin < pFormat->ulLeftMargin) {
                                Output(file, "<UL><LI>", pFormat);
                                pFormat->ulLastLeftMargin = pFormat->ulLeftMargin;
                            }
                            break; */
                        }
                        /* else
                            if (pFormat->ulLastLeftMargin > pFormat->ulLeftMargin)
                            {
                                CHAR szTemp[100];
                                sprintf(szTemp, "</UL><!-- lm now: %d -->",
                                        pFormat->ulLeftMargin);
                                Output(file, szTemp, pFormat);
                                pFormat->ulLastLeftMargin = pFormat->ulLeftMargin;
                            }  */
                    }

                    // otherwise: print word from global dictionary
                    if (pFormat->fOutputSpaceBeforeNextWord) {
                        Output(file, " ", pFormat);
                        pFormat->fOutputSpaceBeforeNextWord = FALSE;
                    }
                    Output(file, apszDictionary[usGlobalDictOfs], pFormat);

                    // increase X count for line breaks in HTML source
                    // pFormat->ulCurX += strlen(apszDictionary[usGlobalDictOfs]);
                    pFormat->fJustHadLeftMargin = FALSE;
                    pFormat->fJustHadLinebreak = FALSE;
                    pFormat->fJustHadBullet = FALSE;

                    if (pFormat->fSpace) {
                        pFormat->fOutputSpaceBeforeNextWord = TRUE;
                        // Output(file, " ", pFormat);
                        // pFormat->ulCurX++;

                        if (!pFormat->fMonoSpace)
                            // line long enough?
                            if (pFormat->ulCurX > ulMaxCharsPerLine) {
                                // insert line break in HTML source
                                fprintf(file, "\n");
                                pFormat->ulCurX = 0;
                                pFormat->fOutputSpaceBeforeNextWord = FALSE;
                            }
                    }
                    break;
                }
            }
    } else {
        // shouldn't happen
        Output(file, "<!-- Error: invalid slot number found -->", pFormat);
        if (ulVerboseMode > 0)
            printf("\nError: Invalid slot number %d\n", usSlot);
    }
}

/********************************************************************
 *                                                                  *
 *  bitmap handling                                                 *
 *                                                                  *
 *******************************************************************/

/*
 * DecodeINFBitmap:
 *      this is called by TranslateSlot above ifottom is used.
 */

#define BFT_bMAP           0x4d62   /* 'bM' */

BOOL DecodeINFBitmap(PSZ pszBmpFile,        // in: bmp filename to create
                     ULONG ulBitmapOfs,     // in: bmp data offset in file
                     PULONG pulWidth,       // out: bitmap dimensions
                     PULONG pulHeight)
{
    BOOL brc = FALSE;

    // get start of this bitmap in file
    PBYTE pBitmapStart = pbContent + (pInfHeader->ulImgStart) + ulBitmapOfs;
    PINFBITMAPHEADER pbmh = (PINFBITMAPHEADER)pBitmapStart;

    *pulWidth = pbmh->cx;
    *pulHeight = pbmh->cy;

    pbmh->offBits = 14 + pbmh->cbFix;
    if (pbmh->cBitCount < 24)
        pbmh->offBits += ( 3 * (1 << (pbmh->cBitCount)) );

    if (ulVerboseMode > 0) {
        printf("\nWriting bitmap: %s", pszBmpFile);
        fflush(stdout);
    }

    if (optDecodeBitmaps)
        if (    (*(PCHAR)pBitmapStart == 'b')
             // && (*(PCHAR)pBitmapStart+1 == 'M')
           )
        {
            FILE *fBitmap = fopen(pszBmpFile, "wb");
            SHORT sRgbTableSize,
                  sScanLineSize;
            if (fBitmap)
            {
                unsigned int last_out_code;
                ULONG   ulTotalBytesToOutput,
                        ulBytesRead, ulTotal;
                ULONG bytes_output;
                PBMPDATAHEADER pHeader;
                PBMPDATABLOCK pBlock;
                PBYTE pbCurrent = (PBYTE)pbmh;
                PBYTE pRGBData;
                ULONG   ulBlockCount = 0;

                BITMAPFILEHEADER bmfh;

                if (ulVerboseMode >=2 )
                    printf("\n  Bitmap header:\n    usType    = %x (%c%c)\n",
                        pbmh->usType,
                        LOBYTE(pbmh->usType), HIBYTE(pbmh->usType)  );
                if(pbmh->usType != BFT_BMAP && pbmh->usType != BFT_bMAP)
                    return FALSE;
                if (ulVerboseMode >=2 ) {
                    printf("    cbSize    = %d\n", pbmh->cbSize   );
                    printf("    xHotspot  = %d\n", pbmh->xHotspot );
                    printf("    yHotspot  = %d\n", pbmh->yHotspot );
                    printf("    offBits   = %d\n", pbmh->offBits  );
                    printf("    bmp.cbFix       = %d\n", pbmh->cbFix      );
                    printf("    bmp.cx          = %d\n", pbmh->cx         );
                    printf("    bmp.cy          = %d\n", pbmh->cy         );
                    printf("    bmp.cPlanes     = %d\n", pbmh->cPlanes    );
                    printf("    bmp.cBitCount   = %d\n", pbmh->cBitCount  );
                }

                if (pbmh->cBitCount < 9)
                {
                    sRgbTableSize = (1 << pbmh->cPlanes * pbmh->cBitCount)
                                            * sizeof(RGB);
                    if(sRgbTableSize > 0x10000)
                        return (FALSE);
                }
                else
                    sRgbTableSize = 0;

                bmfh.usType = BFT_BMAP;
                sScanLineSize = ((pbmh->cBitCount * pbmh->cx + 31) / 32)
                                * 4 * pbmh->cPlanes;
                bmfh.cbSize = sizeof(BITMAPFILEHEADER);
                bmfh.xHotspot = 0;
                bmfh.yHotspot = 0;
                bmfh.offBits = sizeof(BITMAPFILEHEADER) + sRgbTableSize;

                bmfh.bmp.cbFix = pbmh->cbFix;
                bmfh.bmp.cx = pbmh->cx;
                bmfh.bmp.cy = pbmh->cy;
                bmfh.bmp.cPlanes = pbmh->cPlanes;
                bmfh.bmp.cBitCount = pbmh->cBitCount;

                if (ulVerboseMode >=2 ) {
                    printf("  Calculating:\n    sRgbTableSize = %d\n", sRgbTableSize);
                    printf("    sScanLineSize = %d\n", sScanLineSize);
                    printf("    new offBits   = %d\n", pbmh->offBits);
                }

                fwrite(&bmfh, sizeof(BITMAPFILEHEADER), 1, fBitmap);

                // go to RGB data
                pbCurrent = ((PBYTE)pbmh) + sizeof(BITMAPFILEHEADER);

                if (sRgbTableSize)
                {
                    pRGBData = pbCurrent;
                    fwrite(pRGBData, 1, sRgbTableSize, fBitmap);
                    pbCurrent += sRgbTableSize;
                }

                ulTotalBytesToOutput = sScanLineSize * pbmh->cy;
                if (ulVerboseMode >=2 )
                    printf("    ulTotalBytesToOutput = %ld\n", ulTotalBytesToOutput);

                pHeader = (PBMPDATAHEADER)pbCurrent;

                pbCurrent += sizeof(BMPDATAHEADER);
                if (ulVerboseMode >=2 ) {
                    printf("    Length of data = %d\n", pHeader->ulTotalSize);
                    printf("    Uncompressed data block size  = %d\n", pHeader->usUncompPerBlock);
                }

                ulBytesRead = sizeof(pHeader->usUncompPerBlock);  // 2

                ulTotal = 0L;
                while (ulBytesRead < pHeader->ulTotalSize) {
                    PBYTE pbInput;
                    pBlock = (PBMPDATABLOCK)pbCurrent;
                    ulBlockCount++;

                    if (ulVerboseMode >=2 )
                        printf("    Block %d: size %d, compression type %d\n",
                                ulBlockCount,
                                pBlock->usCompressedSize, pBlock->ucCompressionType);

                    bytes_output = 0;
                    pbInput = &(pBlock->Data);
                    if (!LZWDecompressBlock(&pbInput,
                                fBitmap,
                                (pBlock->usCompressedSize)-1,
                                &bytes_output,
                                &last_out_code))
                    {
                        printf("\nError: LZWDecompression failed -\n");
                    }

                    if (ulVerboseMode >=2 )
                        printf("    bytes_output = %d\n", bytes_output);

                    if (ferror(fBitmap))
                    {
                        fclose(fBitmap);
                        printf("\nError writing bitmap file %s (disk full?)\n",
                                pszBmpFile);
                        Exit(1);
                    }
                    ulBytesRead += sizeof(BMPDATABLOCK) + pBlock->usCompressedSize-1;
                    ulTotal += bytes_output;
                    if (ulVerboseMode >=2 ) {
                        printf("    Bytes written from this block: %ld\n", bytes_output);
                        printf("    Total so far =  %ld (of %ld)\n",
                                        ulTotal, ulTotalBytesToOutput);
                        printf("    Bytes read so far =  %ld (of %ld)\n",
                                ulBytesRead, pHeader->ulTotalSize);
                    }

                    if (    (bytes_output < pHeader->usUncompPerBlock)
                         && (ulTotal < ulTotalBytesToOutput)
                       )
                    {
                        unsigned i, cb;
                        cb = pHeader->usUncompPerBlock - bytes_output;
                        cb = min(cb, ulTotalBytesToOutput - ulTotal);
                        if (ulVerboseMode >=2 )
                            printf("    Catch up %d bytes\n",  cb);
                        ulTotal += cb;
                        for(i=0; i<cb; i++)
                            putc(last_out_code, fBitmap);
                    }
                    //flushall();

                    pbCurrent += ((pBlock->usCompressedSize) + 2);
                }
                fclose(fBitmap);
                *pulWidth = pbmh->cx;
                *pulHeight = pbmh->cy;
            } else {
                printf("    Error!");
                return (FALSE);
            }
        }

    return (brc);
}

/********************************************************************
 *                                                                  *
 *  LZW decompression                                               *
 *                                                                  *
 *******************************************************************/

/*
 *  This is based on code (W) by Peter Fitzsimmons, pfitz@ican.net.
 *  His liner notes in the original:
 *      has its roots in a June 1990
 *      DDJ article "LZW REVISITED", by Shawn M. Regan
 *      --=>revision history<=--
 *      1 lzw.c 21-Aug-96,2:24:36,`PLF' ;
 *      2 lzw.c 24-Aug-96,2:27:24,`PLF' wip
 *
 *  The code has been modified to take the input not from an
 *  open file, but from any memory region. For this, a double
 *  pointer is used, which must be passed to LZWDecompressBlock.
 *  I've also added a few comments for clarity.
 *
 */

/* -- Stuff for LZW decompression -- */
#define INIT_BITS 9
#define MAX_BITS 12     /*PLF Tue  95-10-03 02:16:56*/
#define HASHING_SHIFT MAX_BITS - 8

#if MAX_BITS == 15
#define TABLE_SIZE 36768
#elif MAX_BITS == 14
#define TABLE_SIZE 18041
#elif MAX_BITS == 13
#define TABLE_SIZE 9029
#else
#define TABLE_SIZE 5021
#endif

#define CLEAR_TABLE 256
#define TERMINATOR  257
#define FIRST_CODE  258
#define CHECK_TIME  100

#define MAXVAL(n) (( 1 << (n)) -1)

char *decode_string(unsigned char *buffer, unsigned int code);
unsigned input_code(PBYTE *ppbInput, unsigned bytes_to_read);

static unsigned int *prefix_code;
static unsigned char *append_character;
static unsigned char decode_stack[4000];
static int num_bits;
static int max_code;

unsigned int last_out_code = 0; /*PLF Tue  96-04-16 02:22:33*/

ULONG ulBytesRead = 0;

/*
 * decode_string:
 *
 */

char *decode_string(unsigned char *buffer, unsigned int code)
{
    int i = 0;

    while (code > 255) {
        *buffer++ = append_character[code];
        code = prefix_code[code];
        if (i++ >= 4000) {
            printf("Error during LZW code expansion.\n");
            Exit(1);
        }
    }
    *buffer = code;
    return (buffer);
}

/*
 * input_code:
 *      this function reads in bytes from the input
 *      stream.
 */

unsigned input_code(PBYTE *ppbInput, unsigned bytes_to_read)
{
    unsigned int return_value;
    static unsigned long bytes_out = 0;
    static int input_bit_count = 0;
    static unsigned long input_bit_buffer = 0L;

    while (input_bit_count <= 24) {
        if (bytes_out <= bytes_to_read) {
            input_bit_buffer |= (unsigned long)(**ppbInput) << (24 - input_bit_count);
            (*ppbInput)++;
            ulBytesRead++;
        } else
            input_bit_buffer |= (unsigned long) 0x00 << (24 - input_bit_count);
        bytes_out++;
        input_bit_count += 8;
    }
    return_value = input_bit_buffer >> (32 - num_bits);
    input_bit_buffer <<= num_bits;
    input_bit_count -= num_bits;
    if (bytes_out > bytes_to_read) {    /* flush static vars and quit */
        bytes_out = 0;
        input_bit_count = 0;
        input_bit_buffer = 0L;
        return (TERMINATOR);
    }
    else
        return (return_value);
}

/*
 * LZWDecompressBlock:
 *      this takes one of the INF bitmap blocks
 *      and decompresses it using LZW algorithms.
 *      The output is written to the fOutput file,
 *      to which a standard PM BITMAPFILEHEADER and
 *      the RGB table should already have been written
 *      to.
 */

BOOL LZWDecompressBlock(PBYTE *ppbInput,        // in: compressed data
                    FILE *fOutput,              // out: uncompressed data
                    unsigned int number_bytes,  // in: bytes to decompress
                    unsigned long *pBytesOut,
                    unsigned *pLastCode)
{
    unsigned int next_code = FIRST_CODE;
    unsigned int new_code;
    unsigned int old_code;
    int character, /*counter = 0,*/ clear_flag = 1;
    unsigned char *string;

    num_bits = INIT_BITS;
    max_code = MAXVAL(num_bits);

    ulBytesRead = number_bytes;

    /* -- allocate memory to buffers -- */
    prefix_code = malloc(TABLE_SIZE * sizeof(unsigned int));
    append_character = malloc(TABLE_SIZE * sizeof(unsigned char));

    if (ulVerboseMode >=2 )
        printf("      LZW - Expanding\n");

    while ((new_code = input_code(ppbInput, number_bytes)) != TERMINATOR) {
        if (clear_flag) {
            clear_flag = 0;
            old_code = new_code;
            character = old_code;
            if (EOF == putc(old_code, fOutput) )
              break;
            // **ppbOutput = old_code;
            // (*ppbOutput)++;
            /* if (ulBytesRead > number_bytes)
                break; */
            *pLastCode = old_code;
            (*pBytesOut)++;
            continue;
        }
        if (new_code == CLEAR_TABLE) {
            clear_flag = 1;
            num_bits = INIT_BITS;
            next_code = FIRST_CODE;
            max_code = MAXVAL(num_bits);
            continue;
        }

        if (new_code >= next_code) {
            *decode_stack = character;
            string = decode_string(decode_stack + 1, old_code);
        }
        else
            string = decode_string(decode_stack, new_code);

        character = *string;
        while (string >= decode_stack) {
            *pLastCode = *string;
            if(EOF==putc(*string--, fOutput))
               break;
            /* if (ulBytesRead > number_bytes)
                break; */
            // **ppbOutput = *string;
            // (*ppbOutput)++;
            // string--;

            (*pBytesOut)++;
        }

        if (next_code <= max_code) {
            prefix_code[next_code] = old_code;
            append_character[next_code++] = character;
            if (next_code == max_code && num_bits < MAX_BITS) {
                //printf("(Increase bit-size to %d)", num_bits + 1);
                max_code = MAXVAL(++num_bits);
            }
        }
        old_code = new_code;
    }
    free(prefix_code);
    free(append_character);
    return (TRUE);
}


