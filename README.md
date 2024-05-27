# wsrm
Windows Secure Delete

wsrm is a secure delete file.

```
wsrm -hl
      wsrm (x64 Unicode) : Mon May 27 13:54 2024 1.0.15.002

               Syntax is : wsrm [options] {files or directories...}
                         : Optionally Followed By [filter] For -sd or -sf
                         : Followed By [output] For -copy
                         : wsrm [options] -sf {drive or volume} {filter}
                         : wsrm [options] -sd {drive or volume} {filter}
                         : wsrm [options] -sdir {drive or volume} {filter}
                         : wsrm [options] -xsd {drive}
                         : wsrm [options] -filldisk {drive/directory}
                         : wsrm [options] -fillname {drive/directory} -count num
                         : wsrm [options] -count {count} -fillname {drive or volume}
                         : wsrm [options] -copy {filenumber} {drive or volume} -output file
                         : wsrm [options] -copy {filenumber} {drive or volume} {outputfile}
                         : wsrm [options] -locate {filenumber} {drive or volume}
                         : wsrm [options] -info {filenumber} {drive or volume}

                         : files and directories can use wildcards '*' (C:\TEMP\*, C:\TEMP\*.log)
                         : Pathname are drive:\directories\files (C:\TEMP\test.tmp)
                         : Drives are disk: (C:, D:, ...)
                         : Drives are also \\?\disk: without trailing \
                         : Volumes are     \\?\{id}  without trailing \
                         : See cmd mountvol for Volumes

                         : Clear File Content, Then Rename File And Finally Remove File

          -h, -help, -?  : Show Help
      -hl, -l, -longhelp : Show Long Help
                -1 to -9 : Number Of Passes For Writing File
              -b, -block : Last Writing Will Be Rounded to Block Size
                 -banner : Display Banner
           -c, -clusters : Check Cluster Allocation
           -count number : Count for -fillname
        -copy filenumber : Copy Filenumber To A File
                         : Make Sure To Specify The Output File On Another Disk
             -d, -delete : Delete Only (no write over)
          -e, -encrypted : Write Over Encrypted Files (Normally Not Done)
 -f file, -fillfile file : Fill Buffer With This File
                         : Only the first 512k bytes will be used
                         : (If File Is Not Found Or '-' It Will Be Not Used)
       -filter substring : Filter for -sd and -sf
                         : A Filter For Filenames (*.doc, foo*.log)
   -filldisk {drive/dir} : Fill One Disk
   -fillname {drive/dir} : Create Files on The Disk
                      -g : Generate Table For Each Write
                         : (Otherwise Tables are Generated Once At Start)
                      -i : Interactive Mode (Ask Confirmation Before Deletion)
        -info filenumber : Display Infos For A Fiie Number
  -j text, -jpgtext text : Generate a jpeg file From Text That Will Be Used For Buffers
                         : (If Text Is Empty Or '-' It Will Be Not Used)
             -k filename : Create The JPEG file To See What It Looks Like
          -locale string : Display using locale information (eg FR-fr, .1252)
      -locate filenumber : Display Hierarchy Info
               -nobanner : Do Not Display Banner
      -nomatch substring : Filter for -sd and -sf
               -o, -over : Overwrite Read Only / System Flag
        -output filename : For -copy Specify The Output Filename
                         : Can be A Directory Or A Filename
              -p, -probe : Probe Mode : Just Test And Do Nothing
              -q, -quiet : Quiet Mode (Only Display Errors)
          -r, -recursive : Recursive Mode For File Deletion (Same as 'del /s')
             -s, -subdir : Remove Subdirectory (Same As 'rd /s')
        -sd drive/volume : Show Deleted Files
        -sf drive/volume : Show All Files (including deleted)
      -sdir drive/volume : Show Directories Only (including deleted)
              -t, -trace : Trace Mode (More Verbose)
            -v, -verbose : Verbose Mode
            -w, -write n : Write n Times (The Default is One Pass)
              -xsd drive : Show Deleted Files
      -x, -exclude files : Exclude files on deletion
              -xsd drive : Show Deleted Files
                -y, -yes : Yes Response To Questions (No Confirmation On Delete)
                         : It Disables Interactive Mode
               -z, -zero : Use Zeroes Buffer

             Environment : Variables
      WSRM_FILL_FILENAME : If Set This Will Be The File Used To Fill Buffers
          WSRM_JPEG_TEXT : If Set This Will Be Used to Create a Jpeg To Fill Buffers
        WSRM_INTERACTIVE : If Set to YES The Mode Will Be Interactive
          WSRM_NO_BANNER : If Set to YES No Banner Will Be Displayed

            -handlecrash : Handle Program Crashes
                         :  use also -verbose, -debug, -trace, -crashtostd
```
