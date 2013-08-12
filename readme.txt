

INF2HTML V0.91 (w) (c) 1998 Ulrich M”ller


INTRODUCTION
============

INF2HTML is a small utility which will take any INF or HLP file as an
input and produce a set of HTML files, each containing one page
("panel") of the input file.

INF2HTML automatically creates links between the files so that you
can view the HTML ouput with any browser in a way similar to
VIEW.EXE.

INF2HTML's HTML output will contain a few extra tag attributes which
are not part of standard HTML. Browsers will ignore this, but the
most valuable HTML2IPF utility by Andrew Pawel Zabolotny will be able
to produce IPF source files accordingly. I will publish an updated
version soon which can handle more extra tags, so perfect translation
between the two formats will be possible.


USAGE
=====

Per default, INF2HTML will create a subdirectory in the directory of
the input file where it will store the HTML ouput files. This
directory will have the same name as the filestem of the input file
(e.g. "CMDREF" if you decode "CMDREF.INF").

Each HTML file will be given a file name according to the following:

    <idx>_L<level>_<title>.html

with:
    <idx> being the table-of-contents index of the file, so that each
          file will be given a unique number;
    <level> being the table-of-contents level of the file (1 for root
            level entries); a "H" letter will be appended if that
            panel is hidden in the table of contents;
    <title> being the first 20-or-so characters of the panel's title,
            as it would be shown in VIEW.EXE.

In addition, INF2HTML will create one HTML file with the same filestem
as the input file which will contain the main index (as displayed in
the "Contents" tree of VIEW.EXE).

Note that this implies that INF2HTML will only work on HPFS drives.
There is no error checking for this, so be careful.

INF2HTML will overwrite any files in the output directory without
warning.


OPTIONS
=======

For a description of further options at the command line, simply type
"INF2HTML", and it will briefly explain itself.

You can, for example, specify a different output directory.

In addition, by using the "-F" flag, INF2HTML can operate in "frames"
mode also, which will create an additional "index.html" file in the
output directory with a frameset for both the contents tree
(<filestem>.html) on the left and the panels on the right. In
"frames" mode, all the links in the HTML output will have proper
"TARGET" parameters in the <A> tags.

INF2HTML can also produce additional titles at the top and "Next"/
"Back" links at the bottom of each page by using the "-n" or "-N"
flags.

If you specify "-b", INF2HTML will output uncompressed OS/2 V1.3
bitmaps too. These can be handled by HTML2IPF to produce IPF source
files accordingly.

If you wish to view images using a normal browser, you'll have to
convert them to GIF. There is a small REXX script ("BMP2GIF.CMD")
which will do this job for you, provided that you have the
"Generalized Bitmap Module" (GBM) on your PATH, which is available as
Freeware with source code at Hobbes. INF2HTML has already inserted
proper <IMG> tags into the HTML output, which will point to GIF files
with the same filestem.


REVISION HISTORY
================

    V0.91 (Nov. 10, 1998)
    ---------------------
        --  Added bitmap decoding.
        --  Added additional tags for window positioning, which
            are not part of default HTML and should thus not
            hurt.

    V0.90 (Nov. 10, 1998)
    ---------------------
        Initial release.


KNOWN LIMITATIONS
=================

--  There are problems with codepages. INF2HTML simply writes out
    the text as it comes in the INF file, so certain special
    characters might turn out to be displayed wrong.

    This applies especially to INF tables. These are implemented
    using pre-formatted text with ASCII graphics characters, which
    browsers do not display correctly.

--  Even though window positioning is now implemented, there are
    still problems with the Toolkit docs. Since HTML does not
    support this, you'll get many empty pages for API descriptions,
    because these are implemented using auto-links. Specify "-a"
    at the command line to have "real" links created. I'll think
    of some compatible way to have this work.


LICENSE, COPYRIGHT, DISCLAIMER
==============================

    Copyright (C) 1998 Ulrich M”ller.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as contained in
    the file COPYING in this directory.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    For details, refer to the "COPYING" file.


CREDITS
=======

    Thanks go out to Peter Childs, Australia, for writing that
    "Inside INF" article in EDM/2, vol. 3 no. 8, which has the most
    valuable information about the INF format.

    I have included "inf03.txt" from that article in the source
    directory.

    The bitmap decompression code (new with V0.91) at the bottom was
    written by Peter Fitzsimmons (pfitz@ican.net), who kindly sent me
    his code upon my cry for help in WarpCast. Thanks a lot.


CONTACT, SUGGESTIONS
====================

Ulrich M”ller
e-mail:     ulrich.moeller@rz.hu-berlin.de
www:        http://www2.rz-hu-berlin.de/~h0444vnd/os2.htm

