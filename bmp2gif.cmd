/* BMP2GIF:
   This little utility will convert all BMP files which
   have been created by HTML2IPF to GIF files so you can
   view them using a regular browser.
   Requirements:
   --  The "Generalized Bitmap Module" (GBM) must be on
       your PATH.
   --  This script must be started _in_ the directory
       where the BMP files reside.
*/
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

rc = SysFileTree("*.bmp", "BmpFiles", "FO");

if (BmpFiles.0 > 0) then
    do i = 1 to BmpFiles.0
        Filename = filespec("name", BmpFiles.i);
        Filestem = left(Filename, length(Filename)-4);
        Say 'Converting "'Filename"...";
        'gbmsize "'Filestem'.bmp" "'Filestem'.gif",1.1'
    end
else
    Say 'No bitmap files found in directory "'directory()'".';
