/* ----------------------------------------------------------------------------
 *         ATMEL Microcontroller Software Support
 * ----------------------------------------------------------------------------
 * Copyright (c) 2009, Atmel Corporation
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

/** \page nandflash_applet Nand Flash Applet
 * Flash applet is a small piece of software running on the target board, embedding the 
 * flash read, write, and erase algorithms etc. \n
 * 
 * SAM-BA host controls commands and data transfering, it loads Nand flash applet and 
 * executes it to implement flash memory operation.
 * An applet consists of:
 * <ul>
 * <li>A mailbox which can share information between the applet and SAM-BA host.</li>
 * <li>Implement APPLET_CMD_INIT command to initializes EEFC and reports memory size, 
 * number of lock region, lock region size and buffer size through the mailbox.</li>
 * <li>A flash programming algorithm works for APPLET_CMD_WRITE command, it erases and programs 
 * flash data in buffer at the address which specified in mailbox.</li>
 * <li>Erase all algorithm is necessary to erasing entire flash memory. </li>
 * </ul>
 *
 * It reports command status and written/read size (for write/read algorithm) in mailbox to 
 * SAM-BA host while the command is achieved.
 *
 *
*/
/*@{*/
/*@}*/


/**
 * \file
 *
 * Implementation of Nand flash applet for ISP.
 *
 */
 
/*----------------------------------------------------------------------------
 *        Headers
 *----------------------------------------------------------------------------*/

#include "../common/applet.h"
#include <board.h>

#include <../../libraries/memories/nandflash/SkipBlockNandFlash.h>
#include <../../libraries/memories/nandflash/NandSpareScheme.h>

#include <string.h>

/*----------------------------------------------------------------------------
 *        Local structures
 *----------------------------------------------------------------------------*/

/** \brief Structure for storing parameters for each command that can be 
 * performed by the applet.*/

struct _Mailbox {

    /// Command send to the monitor to be executed.
    unsigned int command;
    /// Returned status, updated at the end of the monitor execution.
    unsigned int status;

    /// Input Arguments in the argument area
    union {

        /// Input arguments for the Init command.
        struct {

            /// Communication link used.
            unsigned int comType;

            /// Trace level.
            unsigned int traceLevel;

        } inputInit;

        /// Output arguments for the Init command.
        struct {

            /// Memory size.
            unsigned int memorySize;
            /// Buffer address.
            unsigned int bufferAddress;
            /// Buffer size.
            unsigned int bufferSize;

        } outputInit;

        /// Input arguments for the Write command.
        struct {

            /// Buffer address.
            unsigned int bufferAddr;
            /// Buffer size.
            unsigned int bufferSize;
            /// Memory offset.
            unsigned int memoryOffset;

        } inputWrite;

        /// Output arguments for the Write command.
        struct {

            /// Bytes written.
            unsigned int bytesWritten;

        } outputWrite;

        /// Input arguments for the Read command.
        struct {

            /// Buffer address.
            unsigned int bufferAddr;
            /// Buffer size.
            unsigned int bufferSize;
            /// Memory offset.
            unsigned int memoryOffset;

        } inputRead;

        /// Output arguments for the Read command.
        struct {

            /// Bytes read.
            unsigned int bytesRead;

        } outputRead;

        // Input arguments for the Full Erase command.
        struct {

            /// Type of Erase to perform
            unsigned int eraseType;

        } inputFullErase;

        /// Input arguments for the Batch Erase command.
        struct {
            /// Type of Erase to perform
            unsigned int eraseType;
            /// Batch number.
            unsigned int batch;
        } inputBatchErase;
        
         // Input arguments for the Block Erase command.
        struct {

            /// Memory start offset to be erased.
            unsigned int memoryOffsetStart;
            /// Memory end offset to be erased.
            unsigned int memoryOffsetEnd;
        } inputBlocksErase;
        // Output arguments for the Full Erase command.
        // NONE

        // Output arguments for the Batch Erase command.
        struct {
            /// next eraseing batch.
            unsigned int nextBatch;
        } outputBatchErase;
        // Input arguments for the List Bad Blocks command.
        // NONE

        /// Output arguments for the List Bad Blocks command.
        struct {

            /// Number of bad blocks found
            unsigned int nbBadBlocks;
            /// Address of the buffer containing bad block list
            unsigned int bufferAddress;

        } outputListBadBlocks;

        /// Input arguments for the Tag Block command.
        struct {

            /// ID of the block to tag
            unsigned int blockId;
            /// Data to be written in the bad block marker word
            unsigned int tag;

        } inputTagBlock;

        // Output arguments for the Tag Block command.
        // NONE

    } argument;    
};


/*----------------------------------------------------------------------------
 *         Global variables
 *----------------------------------------------------------------------------*/
/// End of program space (code + data).
extern unsigned int end;


/*----------------------------------------------------------------------------
 *         Local variables
 *----------------------------------------------------------------------------*/
/// Nandflash memory size.
static unsigned int memSize;
/// Number of blocks in nandflash.
static unsigned int numBlocks;
/// Size of one block in the nandflash, in bytes.
static unsigned int blockSize;
/// Size of one page in the nandflash, in bytes.
static unsigned int pageSize;
/// Number of page per block
static unsigned int numPagesPerBlock;

/// Pins used to access to nandflash.
static const Pin pPinsNf[] = {PINS_NANDFLASH};
/// Nandflash device structure.
static struct SkipBlockNandFlash skipBlockNf;
/// Address for transferring command bytes to the nandflash.
static unsigned int cmdBytesAddr = BOARD_NF_COMMAND_ADDR;
/// Address for transferring address bytes to the nandflash.
static unsigned int addrBytesAddr = BOARD_NF_ADDRESS_ADDR;
/// Address for transferring data bytes to the nandflash.
static unsigned int dataBytesAddr = BOARD_NF_DATA_ADDR;
/// Nandflash chip enable pin.
static const Pin nfCePin = BOARD_NF_CE_PIN;
/// Nandflash ready/busy pin.
static const Pin nfRbPin = BOARD_NF_RB_PIN;
/// Latest block have been erased.
static unsigned short latestErasedBlock ;
/// Latest page have been erased.
static unsigned char latestErasedPage ;

/*----------------------------------------------------------------------------
 *         Definiation
 *----------------------------------------------------------------------------*/
#define NandEraseFree        0xFFFF
#define BufferSize           0x4000
#define ERASE_BATCH          8
/*----------------------------------------------------------------------------
 *         Global functions
 *----------------------------------------------------------------------------*/
/**
 * \brief  Applet main entry. This function decodes received command and executes it.
 *
 * \param argc  always 1
 * \param argv  Address of the argument area..
 */
int main(int argc, char **argv)
{
    struct _Mailbox *pMailbox = (struct _Mailbox *) argv;
    unsigned int bufferSize, bufferAddr, memoryOffset, bytesToWrite;
    unsigned short pagesToWrite, pagesToRead;
    unsigned int bytesWritten = 0;
    unsigned int bytesRead = 0;
    unsigned int nbBadBlocks = 0;
    unsigned int nbBlocks = 0;
    // Temporary buffer used for non block aligned read / write 
    unsigned int tempBufferAddr;
    unsigned short block, page, offset, i;
    unsigned char unAlignedPage;
    // Index in source buffer during buffer copy
    unsigned int offsetInSourceBuff;
    // Index in destination buffer during buffer copy
    unsigned int offsetInTargetBuff;
    // Errors returned by SkipNandFlash functions
    unsigned char error = 0;

    // ----------------------------------------------------------
    // INIT: 
    // ----------------------------------------------------------
    if (pMailbox->command == APPLET_CMD_INIT) {
        
        TRACE_INFO("-- NandFlash ISP applet %s --\n\r", SAM_BA_APPLETS_VERSION);
        TRACE_INFO("-- %s\n\r", BOARD_NAME);
        TRACE_INFO("-- Compiled: %s %s --\n\r", __DATE__, __TIME__);
        TRACE_INFO("INIT command\n\r");
        // Configure SMC for Nandflash accesses (done each time applet is launched because of old ROM codes)
        BOARD_ConfigureNandFlash();
        PIO_Configure(pPinsNf, PIO_LISTSIZE(pPinsNf));

        if (pPinsNf->pio == 0) {
            pMailbox->status = APPLET_NO_DEV;
            pMailbox->argument.outputInit.bufferSize = 0;
            pMailbox->argument.outputInit.memorySize = 0;
            pMailbox->argument.outputInit.bufferAddress = (unsigned int) &end;  
            TRACE_INFO("INIT command: No Nandflash defined for this board\n\r");
        }
        else {

            memset(&skipBlockNf, 0, sizeof(skipBlockNf));            

            if (SkipBlockNandFlash_Initialize(&skipBlockNf,
                                              0,
                                              cmdBytesAddr,
                                              addrBytesAddr,
                                              dataBytesAddr,
                                              nfCePin,
                                              nfRbPin)) {
    
                pMailbox->status = APPLET_DEV_UNKNOWN;
                pMailbox->argument.outputInit.bufferSize = 0;
                pMailbox->argument.outputInit.memorySize = 0;
                TRACE_INFO("\tDevice Unknown\n\r");
            }
            else {

                TRACE_INFO("\tNandflash driver initialized\n\r");
        
                pMailbox->argument.outputInit.bufferAddress = (unsigned int) &end;
                // Get device parameters
                memSize = NandFlashModel_GetDeviceSizeInBytes(&skipBlockNf.ecc.raw.model);
                blockSize = NandFlashModel_GetBlockSizeInBytes(&skipBlockNf.ecc.raw.model);
                numBlocks = NandFlashModel_GetDeviceSizeInBlocks(&skipBlockNf.ecc.raw.model);
                pageSize = NandFlashModel_GetPageDataSize(&skipBlockNf.ecc.raw.model);
                numPagesPerBlock = NandFlashModel_GetBlockSizeInPages(&skipBlockNf.ecc.raw.model);
                latestErasedBlock = 0xFFFF;
                
                pMailbox->status = APPLET_SUCCESS;
                pMailbox->argument.outputInit.bufferSize = BufferSize; 
                pMailbox->argument.outputInit.memorySize = memSize;
                TRACE_INFO("\t pageSize : 0x%x blockSize : 0x%x blockNb : 0x%x \n\r",  pageSize, blockSize, numBlocks);
            }
        }
    }

    // ----------------------------------------------------------
    // WRITE: 
    // ----------------------------------------------------------
    else if (pMailbox->command == APPLET_CMD_WRITE) {

        memoryOffset = pMailbox->argument.inputWrite.memoryOffset;
        bufferAddr = pMailbox->argument.inputWrite.bufferAddr;
        tempBufferAddr = bufferAddr + BufferSize;
        bytesToWrite = pMailbox->argument.inputWrite.bufferSize;
        unAlignedPage = 0;
        TRACE_INFO("WRITE arguments : offset 0x%x, buffer at 0x%x, of 0x%x Bytes\n\r",
               memoryOffset, bufferAddr, bytesToWrite);

        pMailbox->argument.outputWrite.bytesWritten = 0;

        // Check word alignment
        if (memoryOffset % 4) {

            pMailbox->status = APPLET_ALIGN_ERROR;
            goto exit;
        }

        // Retrieve page and block addresses        
        
        if (NandFlashModel_TranslateAccess(&(skipBlockNf.ecc.raw.model),
                                           memoryOffset,
                                           bytesToWrite,
                                           &block,
                                           &page,
                                           &offset)) {
            pMailbox->status = APPLET_FAIL;
            goto exit;
        }
        if (block != latestErasedBlock ){
            // Erase target block
            error = SkipBlockNandFlash_EraseBlock(&skipBlockNf, block, NORMAL_ERASE);
            if (error == NandCommon_ERROR_BADBLOCK) {
                pMailbox->status = APPLET_BAD_BLOCK;
                goto exit;
            }
            if (error) {
                pMailbox->status = APPLET_FAIL;
                goto exit;
            }
            latestErasedBlock = block;
            latestErasedPage = 0xff;
        }
        if (page <= ((unsigned char)(latestErasedPage + 1))){
            error = SkipBlockNandFlash_EraseBlock(&skipBlockNf, block, NORMAL_ERASE);
            if (error == NandCommon_ERROR_BADBLOCK) {
                pMailbox->status = APPLET_BAD_BLOCK;
                goto exit;
            }
            if (error) {
                pMailbox->status = APPLET_FAIL;
                goto exit;
            }
            latestErasedBlock = block;
            latestErasedPage = page;
        }
        
        offsetInSourceBuff = 0;
        TRACE_INFO("WRITE at block 0x%x, page 0x%x, offset 0x%x in page \n\r", block, page, offset);
        if (offset ) {
            
            // We are not page aligned.
            offsetInTargetBuff = offset;
            memset((unsigned int *)tempBufferAddr, 0xFF, pageSize);
            while (offsetInTargetBuff < pageSize) {

                *(unsigned int *)(tempBufferAddr + offsetInTargetBuff) = *(unsigned int *)(bufferAddr + offsetInSourceBuff);
                offsetInSourceBuff += 4;
                offsetInTargetBuff += 4;
                bytesWritten += 4;
            }
            error = SkipBlockNandFlash_WritePage(&skipBlockNf, block, page, ( void *)tempBufferAddr ,0);
            if (error == NandCommon_ERROR_BADBLOCK) {
                pMailbox->status = APPLET_BAD_BLOCK;
                goto exit;
            }
            if (error) {
                pMailbox->status = APPLET_FAIL;
                goto exit;
            }
            unAlignedPage++;
        }   
        
        pagesToWrite = (bytesToWrite - bytesWritten) / pageSize;
        if (bytesToWrite - ((pagesToWrite + unAlignedPage)  * pageSize)) {
            pagesToWrite++;
        } 
        if ((pagesToWrite + page ) > numPagesPerBlock)
        {
            pagesToWrite = numPagesPerBlock - page;
        }
        // Write target block
        error = SkipBlockNandFlash_WriteBlockUnaligned(&skipBlockNf, block, (page + unAlignedPage), pagesToWrite, ( void *)(bufferAddr + offsetInSourceBuff));
        bytesWritten += pagesToWrite * pageSize;
        if (bytesWritten > bytesToWrite) {
            bytesWritten = bytesToWrite;
        }
        if (error == NandCommon_ERROR_BADBLOCK) {

            pMailbox->status = APPLET_BAD_BLOCK;
            goto exit;
        }
        if (error) {

            pMailbox->status = APPLET_FAIL;
            goto exit;
        }
        
        pMailbox->argument.outputWrite.bytesWritten = bytesWritten;
        pMailbox->status = APPLET_SUCCESS;
    }

    // ----------------------------------------------------------
    // READ: 
    // ----------------------------------------------------------
    else if (pMailbox->command == APPLET_CMD_READ) {

        memoryOffset = pMailbox->argument.inputRead.memoryOffset;
        bufferAddr   = pMailbox->argument.inputRead.bufferAddr;
        tempBufferAddr = bufferAddr + BufferSize;
        
        bufferSize   = pMailbox->argument.inputRead.bufferSize;

        TRACE_INFO("READ at offset: 0x%x buffer at : 0x%x of: 0x%x Bytes\n\r",
               memoryOffset, bufferAddr, bufferSize);

        pMailbox->argument.outputRead.bytesRead = 0;
        unAlignedPage = 0;
        // Check word alignment
        
        if (memoryOffset % 4) {

            pMailbox->status = APPLET_ALIGN_ERROR;
            goto exit;
        }

        // Retrieve page and block addresses        
        if (NandFlashModel_TranslateAccess(&(skipBlockNf.ecc.raw.model),
                                           memoryOffset,
                                           bufferSize,
                                           &block,
                                           &page,
                                           &offset)) {
            pMailbox->status = APPLET_FAIL;
            goto exit;
        }

        TRACE_INFO("READ at block 0x%x, page 0x%x, offset in page 0x%x\n\r", block, page, offset);
        offsetInTargetBuff = 0;
        if (offset) {
            memset((unsigned int *)tempBufferAddr, 0xFF, pageSize);
            error = SkipBlockNandFlash_ReadPage(&skipBlockNf, block, page, ( void *)tempBufferAddr ,0);
            if (error == NandCommon_ERROR_BADBLOCK) {
                pMailbox->status = APPLET_BAD_BLOCK;
                goto exit;
            }
            if (error) {
                pMailbox->status = APPLET_FAIL;
                goto exit;
            }

            // Fill dest buffer with read data
            offsetInSourceBuff = offset;
            while (offsetInSourceBuff < pageSize) {
                *(unsigned int *)(bufferAddr + offsetInTargetBuff) = *(unsigned int *)(tempBufferAddr + offsetInSourceBuff);
                offsetInSourceBuff += 4;
                offsetInTargetBuff += 4;
                bytesRead += 4;
            }
            unAlignedPage++;
        }
        pagesToRead = (bufferSize - bytesRead) / pageSize;
        if (bufferSize - ((pagesToRead + unAlignedPage)* pageSize)) {
            pagesToRead++;
        }
        if ((pagesToRead + page ) > numPagesPerBlock)
        {
            pagesToRead = numPagesPerBlock - page;
        }
        // Read target block
        error = SkipBlockNandFlash_ReadBlockUnaligned(&skipBlockNf, block, (page + unAlignedPage), pagesToRead, ( void *)(bufferAddr + offsetInTargetBuff));
        bytesRead += pagesToRead * pageSize;
        if (bytesRead > bufferSize) {
            bytesRead = bufferSize;
        }
        if (error == NandCommon_ERROR_BADBLOCK) {

            pMailbox->status = APPLET_BAD_BLOCK;
            goto exit;
        }
        if (error) {
            pMailbox->status = APPLET_FAIL;
            goto exit;
        }
        
        pMailbox->argument.outputRead.bytesRead = bytesRead;
        pMailbox->status = APPLET_SUCCESS;
    }

    // ----------------------------------------------------------
    // FULL ERASE: 
    // ----------------------------------------------------------
    else if (pMailbox->command == APPLET_CMD_FULL_ERASE) {

        TRACE_INFO("FULL ERASE command\n\r");
        TRACE_INFO("\tForce erase flag: 0x%x\n\r", pMailbox->argument.inputFullErase.eraseType);

        for (i = 0; i < numBlocks; i++) {

            // Erase the page
            if (SkipBlockNandFlash_EraseBlock(&skipBlockNf, i, pMailbox->argument.inputFullErase.eraseType)) {

                TRACE_INFO("Found block #%d BAD, skip it\n\r", i);
            }
        }

        TRACE_INFO("Full Erase achieved\n\r");
        pMailbox->status = APPLET_SUCCESS;
    }
    
    // ----------------------------------------------------------
    // BATCH FULL ERASE:
    // ----------------------------------------------------------
    else if (pMailbox->command == APPLET_CMD_BATCH_ERASE) {

        TRACE_INFO("BATCH ERASE command\n\r");
        block = pMailbox->argument.inputBatchErase.batch * (numBlocks / ERASE_BATCH);
        
        TRACE_INFO("Erase block from #%d to #%d\n\r", block, block + (numBlocks / ERASE_BATCH));
        for (i = block ; i < block + (numBlocks / ERASE_BATCH) ; i++) {

            // Erase the block
            if (SkipBlockNandFlash_EraseBlock(&skipBlockNf, i, pMailbox->argument.inputBatchErase.eraseType)) {

                TRACE_INFO("Found block #%d BAD, skip it\n\r", i);
            }
        }
        
        if ((pMailbox->argument.inputBatchErase.batch + 1) == ERASE_BATCH) {
            TRACE_INFO("Full Erase achieved, erase type is %d\n\r", pMailbox->argument.inputBatchErase.eraseType);
            pMailbox->argument.outputBatchErase.nextBatch = 0;  
        }
        else {
            pMailbox->argument.outputBatchErase.nextBatch =  pMailbox->argument.inputBatchErase.batch + 1;
            TRACE_INFO("Batch Erase achieved\n\r");
        }
        pMailbox->status = APPLET_SUCCESS;
    }
    
    // ----------------------------------------------------------
    // ERASE_BLOCKS: 
    // ----------------------------------------------------------

    else if (pMailbox->command == APPLET_CMD_ERASE_BLOCKS) {

        TRACE_INFO("BLOCKS ERASE command\n\r");
        memoryOffset = pMailbox->argument.inputBlocksErase.memoryOffsetStart;
        if ((pMailbox->argument.inputBlocksErase.memoryOffsetEnd > memSize) || (pMailbox->argument.inputBlocksErase.memoryOffsetEnd < memoryOffset) ) {
            TRACE_INFO("Out of memory space\n\r");
            pMailbox->status = APPLET_ERASE_FAIL;
            goto exit;
        }
        nbBlocks = ((pMailbox->argument.inputBlocksErase.memoryOffsetEnd- memoryOffset)/ blockSize) + 1;
                                           
        TRACE_INFO("Erase blocks from %d  to %d \n\r",  memoryOffset / blockSize, (memoryOffset / blockSize)+ nbBlocks );                                    
        // Erase blocks
        for (i =  memoryOffset / blockSize; i < memoryOffset / blockSize + nbBlocks ; i++) {
            if (SkipBlockNandFlash_EraseBlock(&skipBlockNf,  i , NORMAL_ERASE)) {
                 TRACE_INFO("Found block #%d BAD, skip it\n\r",  i);
            }
        }
        TRACE_INFO("Blocks Erase achieved\n\r");
        pMailbox->status = APPLET_SUCCESS;
    }
    // ----------------------------------------------------------
    // LIST BAD BLOCKS:
    // ----------------------------------------------------------
    else if (pMailbox->command == APPLET_CMD_LIST_BAD_BLOCKS) {

        TRACE_INFO("LIST BAD BLOCKS command\n\r");

        nbBadBlocks = 0;
        bufferAddr = (unsigned int) &end;
        pMailbox->argument.outputListBadBlocks.bufferAddress = bufferAddr;

        for (i = 0; i < numBlocks; i++) {

            // Erase the page
            if (SkipBlockNandFlash_CheckBlock(&skipBlockNf, i) == BADBLOCK) {

                nbBadBlocks++;
                *((unsigned int *)bufferAddr) = i;
                bufferAddr += 4;
                TRACE_INFO("Found block #%d BAD\n\r", i);
            }
        }

        TRACE_INFO("LIST BAD BLOCKS achieved\n\r");

        pMailbox->argument.outputListBadBlocks.nbBadBlocks = nbBadBlocks;

        pMailbox->status = APPLET_SUCCESS;
    }

    // ----------------------------------------------------------
    // TAG BLOCK:
    // ----------------------------------------------------------
    else if (pMailbox->command == APPLET_CMD_TAG_BLOCK) {

        TRACE_INFO("TAG BLOCK command\n\r");

        bufferAddr = (unsigned int) &end;
        block = pMailbox->argument.inputTagBlock.blockId;

        // To tag the block as good, just erase it without bad block check
        if ((unsigned char)pMailbox->argument.inputTagBlock.tag == 0xFF)
        {
            if (SkipBlockNandFlash_EraseBlock(&skipBlockNf, block, SCRUB_ERASE)) {

                TRACE_INFO("Cannot erase block %d\n\r", block);
                pMailbox->status = APPLET_FAIL;
                goto exit;
            }
        }
        else {

            for (i = 0; i < 2; i++) {

                // Start by reading the spare
                memset((unsigned char *)bufferAddr, 0xFF, NandCommon_MAXSPAREECCBYTES);

                TRACE_INFO("Tag to write : 0x%x\n\r", (unsigned char)pMailbox->argument.inputTagBlock.tag);

                NandSpareScheme_WriteBadBlockMarker((struct NandSpareScheme *)(NandFlashModel_GetScheme((struct NandFlashModel *)(&skipBlockNf))),
                                                    (unsigned char *)bufferAddr,
                                                    ((unsigned char)pMailbox->argument.inputTagBlock.tag));

                if (RawNandFlash_WritePage((struct RawNandFlash *)(&skipBlockNf), block, i, 0, (unsigned char *)bufferAddr)) {

                    TRACE_ERROR("Failed to write spare data of page %d of block %d\n\r", i, block);
                    pMailbox->status = APPLET_FAIL;
                    goto exit;
                }
            }
        }

        TRACE_INFO("TAG BLOCK achieved\n\r");

        pMailbox->status = APPLET_SUCCESS;
    }

exit :
    // Acknowledge the end of command
    TRACE_INFO("\tEnd of applet (command : %x --- status : %x)\n\r", pMailbox->command, pMailbox->status);

    // Notify the host application of the end of the command processing
    pMailbox->command = ~(pMailbox->command);
    return 0;
}

