/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support 
 * ----------------------------------------------------------------------------
 * Copyright (c) 2010, Atmel Corporation
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Atmel's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 */


The CDC version of SAM-BA is designed to communicate with ATMEL AT91 devices
only via a USB CDC Serial communication channel, on Microsoft Windows 2000,
XP and Vista.

USB CDC driver installation :

If your AT91 EK board is recognised as an ATMEL AT91xxxxx Test Board using
atm6124.sys driver, you have to uninstall this driver first, or connect the
board to another USB port on the computer where the board will not be detected.

To uninstall a previously installed driver on a USB port, you have to open the
Windows Device Manager tool (the following steps are explained for Windows XP,
but very similar for the other versions of the OS) :
- Connect the board to the computer via a USB port and power it up,
- Control panel -> System -> 'Hardware' pane,
- Expand the 'Universal Serial Bus controllers' folder,
- Right click on the 'atm6124.Sys ATMEL AT91xxxxx Test Board' entry,
- Choose 'uninstall' and confirm, (the entry disappears from the list)
- Power down the board.

To install the CDC Serial driver :
- Power UP the board.
- The system finds a new hardware and asks you to search a new driver for it,
- Tell it to NOT connect to Windows update, then 'Next' button,
- Choose to install from a list or specific location (Advanced), then 'Next' button,
- Click on 'Don't search, ...', then 'Next' button,
- Choose 'Have Disk...' button, and look for the 'atm6124_cdc.inf' file where 
  you have installed this CDC version of SAM-BA, then 'Open' and 'OK',
- Select the Model : 'AT91 USB to Serial Converter' in the list and click 'Next',
- Click on 'Continue Anyway' in the next window, and then 'Finish'.

IMPORTANT : If you change the USB port on wich you connect the board, you will
need to redo the install procedure described above.  

In the 'Device Manager' window, your board appears in the 'Ports (COM & LPT)'
folder, with the 'virual' COM port name indicated in parenthesis.

IMPORTANT : You have to keep this number in mind to select it when SAM-BA shows
the 'Choose Connection' message box.  



Running SAM-BA CDC version :

Launch 'sam-ba.exe' file, and select your board and the COM
port number where your board in connected to.

