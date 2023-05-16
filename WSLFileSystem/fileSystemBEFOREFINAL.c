/*
 * fileSystem.c
 *
 *  Modified on:
 *      Author: dher577
 *
 * Complete this file.
 */

#include <stdio.h>
#include <stdlib.h>

#include "device.h"
#include "fileSystem.h"
#include <string.h>

/* The file system error number. */
int file_errno = 0;

/*
 * Formats the device for use by this file system.
 * The volume name must be < 64 bytes long.
 * All information previously on the device is lost.
 * Also creates the root directory "/".
 * Returns 0 if no problem or -1 if the call failed.
 */
int format(char *volumeName)
{
    // Clears all blocks
    unsigned char zeroArray[64];
    memset(zeroArray, 0, 64);
    for (int block = 0; block < numBlocks(); block++)
    {
        blockWrite(block, zeroArray);
    }
    // Assigns volume name to first block
    unsigned char unsignedVol[64];
    memset(unsignedVol, 0, 64);
    strcpy(unsignedVol, volumeName);
    if (strlen(volumeName) > 63)
    {
        file_errno = 101;
        return -1;
    }

    blockWrite(0, unsignedVol);

    // Add information for system area
    unsigned char system[64];
    memset(system, 0, 64);
    char *systemInfo = "0003<-FreeBlock 0000<-NumberOfBlocks";
    strcpy(system, systemInfo);

    blockWrite(1, system);

    // Changes the number of blocks value depending on device num
    unsigned char systemBlock[64];
    blockRead(1, systemBlock);
    char numBlockStr[5] = {0};
    sprintf(numBlockStr, "%04d", numBlocks());
    memcpy(systemBlock + 16, numBlockStr, 4);

    blockWrite(1, systemBlock);

    // Establishes root directory
    unsigned char rootDir[64];
    memset(rootDir, 0, 64);
    char *rootInfo = "0000100010";
    strcpy(rootDir, rootInfo);

    blockWrite(2, rootDir);
    return 0;
}

/*
 * Returns the volume's name in the result.
 * Returns 0 if no problem or -1 if the call failed.
 */
int volumeName(char *result)
{
    blockRead(0, result);
    return 0;
}

/*
 * Makes a file with a fully qualified pathname starting with "/".
 * It automatically creates all intervening directories.
 * Pathnames can consist of any printable ASCII characters (0x20 - 0x7e)
 * including the space character.
 * The occurrence of "/" is interpreted as starting a new directory
 * (or the file name).
 * Each section of the pathname must be between 1 and 7 bytes long (not
 * counting the "/"s).
 * The pathname cannot finish with a "/" so the only way to create a directory
 * is to create a file in that directory. This is not true for the root
 * directory "/" as this needs to be created when format is called.
 * The total length of a pathname is limited only by the size of the device.
 * Returns 0 if no problem or -1 if the call failed.
 */
int create(char *pathName)
{
    // for each dir/file in between "/"
    // if file create file and move into file
    // Check if dir exists in current directory if not create dir details in parent dir and move into dir

    char *token;
    char *pathCopy = strdup(pathName); // make a copy of pathName since strtok() modifies the string
    char **parts = NULL;               // array to store the individual parts of the path
    int numParts = 0;
    int parentBlockNum = 2;

    // Split the path into parts based on the '/' delimiter
    token = strtok(pathCopy, "/");

    while (token != NULL)
    {
        // Resize the parts array to fit the new part
        parts = realloc(parts, sizeof(char *) * (numParts + 1));
        if (parts == NULL)
        {
            fprintf(stderr, "Error: Failed to allocate memory\n");
            free(pathCopy);
            return -1;
        }

        // Store the new part in the parts array
        parts[numParts] = strdup(token);
        numParts++;

        token = strtok(NULL, "/");
    }

    // Loop through each file/dir in path
    for (int i = 0; i < numParts; i++)
    {
        if (i == numParts - 1) // file
        {
            unsigned char *parentBlock = malloc(64);
            // placing file name in parentBlock
            blockRead(parentBlockNum, parentBlock);
            int freePosition = checkFileSize(parentBlockNum);
            int k = 1;
            // Loop allows for overflow
            while (k == 1)
            {
                k = 0;
                if ((63 - freePosition) < 12)
                {
                    k = 1;
                    int overflowBlock = checkOverflow(parentBlockNum);
                    if (overflowBlock > 0)
                    {
                        // If block already has an overflow block then use this block
                        parentBlockNum = overflowBlock;
                        blockRead(parentBlockNum, parentBlock);
                        freePosition = checkFileSize(parentBlockNum);
                    }
                    else
                    {
                        // If file does not have an overflow block create one
                        updateOverflow(parentBlockNum);
                        int newOverflowBlock = checkOverflow(parentBlockNum);
                        parentBlockNum = newOverflowBlock;
                        // create new file in block
                        createNewFileBlock(parentBlockNum, 0);
                        if (updateFreeBlock() == -1)
                        {
                            return -1;
                        }
                        blockRead(parentBlockNum, parentBlock);
                        freePosition = checkFileSize(parentBlockNum);
                    }
                }
                else
                {
                    break;
                }
            }
            // Place file name in parent block
            memcpy(parentBlock + freePosition, parts[i], 8);
            // Assign new block to file
            int freeBlock = getFreeBlock();
            char freeBlockStr[5] = {0};
            sprintf(freeBlockStr, "%04d", freeBlock);
            memcpy(parentBlock + freePosition + 8, freeBlockStr, 4);
            blockWrite(parentBlockNum, parentBlock);
            createNewFileBlock(freeBlock, 0);
            if (updateFreeBlock() == -1)
            {
                return -1;
            }
            updateFileSize(parentBlockNum, 12);
            free(parentBlock);
        }
        else
        { // dir
          // if dir exists move into dir
          // if dir does not exist create dir
            unsigned char *currentDirBlock = malloc(64);
            blockRead(parentBlockNum, currentDirBlock);
            unsigned char *noInfoBlock = malloc(58);
            memcpy(noInfoBlock, currentDirBlock + 10, 58);
            int numOfPossibleFiles = 4;
            for (int j = 0; j < numOfPossibleFiles; j++) // for each file/dir in dir
            {
                int startChar = j * 12;
                unsigned char *nextFile = noInfoBlock + startChar;
                if ((nextFile[0] == 0))
                {
                    // if no more files/dirs in dir then dir/ does not exist and we should create dir
                    unsigned char *parentDirBlock = malloc(64); // allocate 64 bytes of memory
                    // placing file name in parentBlock
                    blockRead(parentBlockNum, parentDirBlock);
                    int freePosition = endOfData(parentBlockNum);
                    // If no room in dir
                    if ((63 - freePosition) < 12)
                    {
                        int overflowBlock = checkOverflow(parentBlockNum);
                        if (overflowBlock > 0)
                        {
                            parentBlockNum = overflowBlock;
                        }
                        else
                        {
                            updateOverflow(parentBlockNum);
                            int newOverflowBlock = checkOverflow(parentBlockNum);
                            parentBlockNum = newOverflowBlock;
                        }
                        j = j - 1;
                        continue;
                    }
                    memcpy(parentDirBlock + freePosition, parts[i], 8);
                    // placing dir block in parentDirBlock
                    char freeBlockStr[5] = {0};
                    int freeDirBlock = getFreeBlock();
                    createNewFileBlock(freeDirBlock, 1);
                    sprintf(freeBlockStr, "%04d", freeDirBlock);
                    memcpy(parentDirBlock + freePosition + 8, freeBlockStr, 4);
                    blockWrite(parentBlockNum, parentDirBlock);
                    // creating dir in new dir block
                    createNewFileBlock(freeDirBlock, 1);
                    if (updateFreeBlock() == -1)
                    {
                        return -1;
                    }
                    updateFileSize(parentBlockNum, 12);
                    parentBlockNum = freeDirBlock;
                    break;
                }
                else
                {
                    if (strncmp(nextFile, parts[i], strlen(parts[i])) == 0) // if file/dir exists
                    {
                        // dir file found move into dir
                        char parentBlock[5];

                        // copy the substring into the new char array
                        strncpy(parentBlock, (const char *)nextFile + 8, 4);

                        // null-terminate the char array
                        parentBlock[4] = '\0';

                        // convert the char array to an integer using atoi()
                        parentBlockNum = atoi(parentBlock);
                        break;
                    }
                    continue;
                }
            }
            free(currentDirBlock);
            free(noInfoBlock);
        }
    }
    // need to get parentBlockNum (currentBlockNum)
    return 0;
}

void createNewFileBlock(int freeBlock, int fileDir)
{
    // fileDir 0 for file, 1 for dir
    unsigned char fileBlock[64];
    memset(fileBlock, 0, 64);
    if (fileDir == 1)
    {
        char *overflowBlock = "0000100000";
        strcpy(fileBlock, overflowBlock);
        blockWrite(freeBlock, fileBlock);
        updateFileSize(freeBlock, 0);
    }
    else
    {
        char *overflowBlock = "0000100000";
        strcpy(fileBlock, overflowBlock);
        blockWrite(freeBlock, fileBlock);
        updateFileSize(freeBlock, 0);
    }
}

int endOfData(int block)
{
    // if more than 6 zeroes then at end
    int numZeroes = 0;
    int firstZeroPos = 0;
    unsigned char *posBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, posBlock);
    for (int i = 6; numZeroes < 6 && i < 64; i++)
    {
        if (posBlock[i] == 0)
        {
            if (numZeroes == 0)
            {
                firstZeroPos = i;
            }
            numZeroes++;
        }
        else
        {
            numZeroes = 0;
        }
        if (i == 63)
        {
            return 63;
        }
    }
    return firstZeroPos;
}

int freePos(int block)
{
    int freePosLoc = 0;
    unsigned char *posBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, posBlock);
    for (int i = 0; posBlock[i] != 0; i++)
    {
        freePosLoc = i;
    }
    return freePosLoc + 1;
    free(posBlock);
}

int getFreeBlock()
{
    unsigned char *rootBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(1, rootBlock);
    char freeBlockStr[5] = {0};
    strncpy(freeBlockStr, rootBlock, 4);
    int freeBlock = atoi(freeBlockStr);
    free(rootBlock);
    return freeBlock;
}

int checkOverflow(int block)
{
    unsigned char *checkBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, checkBlock);
    char overflowBlockStr[5] = {0};
    if (block == 1)
    {
        strncpy(overflowBlockStr, checkBlock + 4, 4);
    }
    else
    {
        strncpy(overflowBlockStr, checkBlock, 4);
    }
    int overflowBlock = atoi(overflowBlockStr);
    free(checkBlock);
    return overflowBlock;
}

void updateOverflow(int block)
{
    unsigned char *fullBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, fullBlock);
    char overflowBlockStr[5] = {0};
    if (block == 1)
    {
        strncpy(overflowBlockStr, fullBlock + 4, 4);
    }
    else
    {
        strncpy(overflowBlockStr, fullBlock, 4);
    }
    int overflowBlock = atoi(overflowBlockStr);
    // update free block value
    int freeBlock = getFreeBlock();
    if (updateFreeBlock() == -1)
    {
        return -1;
    };
    int newOverFlowBlock = freeBlock;
    sprintf(overflowBlockStr, "%04d", newOverFlowBlock);
    if (block == 1)
    {
        memcpy(fullBlock + 4, overflowBlockStr, 4);
    }
    else
    {
        memcpy(fullBlock, overflowBlockStr, 4);
    }
    blockWrite(block, fullBlock);
    free(fullBlock);
}

int updateFreeBlock()
{
    // find next free block
    unsigned char *rootBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(1, rootBlock);
    char freeBlockStr[5] = {0};
    strncpy(freeBlockStr, rootBlock, 4);
    int freeBlock = atoi(freeBlockStr); // FOUND ----------->>>>
    // update free block value
    int nextFreeBlock = freeBlock + 1;
    if (nextFreeBlock > numBlocks() + 1)
    {
        printf("No more room on device");
        file_errno = 103;
        return -1;
    }
    sprintf(freeBlockStr, "%04d", nextFreeBlock);
    memcpy(rootBlock, freeBlockStr, 4);
    blockWrite(1, rootBlock);
    free(rootBlock);
    return 0;
}

/*
 * Returns a list of all files in the named directory.
 * The "result" string is filled in with the output.
 * The result looks like this

/dir1:
file1	42
file2	0

 * The fully qualified pathname of the directory followed by a colon and
 * then each file name followed by a tab "\t" and then its file size.
 * Each file on a separate line.
 * The directoryName is a full pathname.
 */
void list(char *result, char *directoryName)
{
    int parentBlockNum = searchForFile(directoryName);
    int k = 1;
    sprintf(result, "%s:\n", directoryName);
    while (k == 1)
    {
        k = 0;
        unsigned char *finalDirBlock = malloc(64);
        blockRead(parentBlockNum, finalDirBlock);
        unsigned char *noInfoBlock = malloc(58);
        memcpy(noInfoBlock, finalDirBlock + 10, 58);
        int numOfPossibleFiles = 4;
        for (int j = 0; j < numOfPossibleFiles; j++) // for each file/dir in dir
        {
            int startChar = j * 12;
            unsigned char *nextFile = noInfoBlock + startChar;
            if (nextFile[0] == 0)
            {
                break;
            }
            unsigned char *currentFileBlock = malloc(64);
            char nextFileLoc[5];
            strncpy(nextFileLoc, (const char *)nextFile + 8, 4);
            nextFileLoc[4] = '\0';
            int nextFileLocNum = atoi(nextFileLoc);
            blockRead(nextFileLocNum, currentFileBlock);
            char fileName[9]; // Need space for null terminator
            strncpy(fileName, nextFile, 8);
            fileName[8] = '\0';
            int fileSize = checkFileSize(nextFileLocNum);
            fileSize = fileSize - 10;
            sprintf(result + strlen(result), "%s:\t%d\n", fileName, fileSize);
            free(currentFileBlock);
        }
        int overflowBlock = checkOverflow(parentBlockNum);
        if (checkOverflow(parentBlockNum) == 0)
        {
            k = 0;
        }
        else
        {
            parentBlockNum = checkOverflow(parentBlockNum);
            k = 1;
        }
        free(finalDirBlock);
        free(noInfoBlock);
    }
}

/*
 * Writes data onto the end of the file.
 * Copies "length" bytes from data and appends them to the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2write(char *fileName, void *data, int length)
{
    int currentBlockNum = searchForFile(fileName);
    int fileBlockNum = currentBlockNum;
    unsigned char *currentBlock = malloc(64);
    blockRead(currentBlockNum, currentBlock);
    int writingLoop = 1;
    int freePosition = 0;
    int spaceFreeBlock = 0;
    int overflowBlockNum = 0;
    int offset = 0;
    while (writingLoop == 1)
    {
        writingLoop = 0;
        freePosition = checkFileSize(currentBlockNum);
        spaceFreeBlock = 64 - freePosition;
        if (length > spaceFreeBlock)
        {
            writingLoop = 1;
            // Need to create overflowblock but we make sure we write in the old one
            memcpy(currentBlock + freePosition, data + offset, spaceFreeBlock);
            blockWrite(currentBlockNum, currentBlock);
            offset = offset + spaceFreeBlock;
            updateFileSize(currentBlockNum, spaceFreeBlock);
            if (currentBlockNum != fileBlockNum)
            {
                // Update size of file blokc, to ensure its used for list
                updateFileSize(fileBlockNum, spaceFreeBlock);
            }
            length = length - spaceFreeBlock;
            overflowBlockNum = checkOverflow(currentBlockNum);
            if (overflowBlockNum > 0)
            {
                currentBlockNum = overflowBlockNum;
            }
            else
            {
                updateOverflow(currentBlockNum);
                currentBlockNum = checkOverflow(currentBlockNum);
                createNewFileBlock(currentBlockNum, 0);
            }
            blockRead(currentBlockNum, currentBlock);
        }
        else
        {
            // No need to create another overflowblock write into currentblock
            memcpy(currentBlock + freePosition, data + offset, length);
            blockWrite(currentBlockNum, currentBlock);
            offset = offset + length;
            updateFileSize(currentBlockNum, length);
            if (currentBlockNum != fileBlockNum)
            {
                // Update size of file blokc, to ensure its used for list
                updateFileSize(fileBlockNum, length);
            }
            length = 0;
            break;
        }
    }
    free(currentBlock);
    return 0;
}

int findFileSize(int startingBlock)
{
    unsigned char *currentFile = malloc(64);
    blockRead(startingBlock, currentFile);
    int fileSize = strlen(currentFile);
    fileSize = fileSize - 10;
    if ((fileSize + 10) > 63)
    {
        unsigned char *overFlowFile = malloc(64);
        int overflowBlock = checkOverflow(startingBlock);
        if (overflowBlock > 0)
        {
            blockRead(overflowBlock, overFlowFile);
            int overFlowSize = strlen(overFlowFile);
            overFlowSize = overFlowSize - 10;
            if (overFlowSize > 0)
            {
                overFlowSize = overFlowSize + 1; // for escape character
            }
            if (startingBlock == 1)
            {
                overFlowSize = overFlowSize - 4;
            }
            fileSize = fileSize + overFlowSize;
        }
        else
        {
            return 64;
        }
    }
    if (fileSize > 0)
    {
        fileSize = fileSize + 1; // for escape character
    }
    free(currentFile);
    return fileSize;
}

int searchForFile(char *fileName)
{
    char *token;
    char *pathCopy = strdup(fileName); // make a copy of directoryName since strtok() modifies the string
    char **parts = NULL;               // array to store the individual parts of the path
    int numParts = 0;
    int parentBlockNum = 2;

    // Split the path into parts based on the '/' delimiter
    token = strtok(pathCopy, "/");

    while (token != NULL)
    {
        // Resize the parts array to fit the new part
        parts = realloc(parts, sizeof(char *) * (numParts + 1));
        if (parts == NULL)
        {
            fprintf(stderr, "Error: Failed to allocate memory\n");
            free(pathCopy);
            return -1;
        }

        // Store the new part in the parts array
        parts[numParts] = strdup(token);
        numParts++;

        token = strtok(NULL, "/");
    }
    for (int i = 0; i < numParts; i++)
    {
        unsigned char *currentDirBlock = malloc(64);
        blockRead(parentBlockNum, currentDirBlock);
        unsigned char *noInfoBlock = malloc(58);
        memcpy(noInfoBlock, currentDirBlock + 10, 58);
        int numOfPossibleFiles = 4;
        for (int j = 0; j < numOfPossibleFiles; j++) // for each file/dir in dir
        {
            int startChar = j * 12;
            unsigned char *nextFile = noInfoBlock + startChar;
            if (strncmp(nextFile, parts[i], strlen(parts[i])) == 0)
            {
                // dir file found move into dir
                char parentBlock[5];

                // copy the substring into the new char array
                strncpy(parentBlock, (const char *)nextFile + 8, 4);

                // null-terminate the char array
                parentBlock[4] = '\0';

                // convert the char array to an integer using atoi()
                parentBlockNum = atoi(parentBlock);
                break;
            }
            if (j == (numOfPossibleFiles - 1))
            { // final file, file not found
                file_errno = 104;
                return -1;
            }
            continue;
        }
        free(currentDirBlock);
        free(noInfoBlock);
    }
    return parentBlockNum;
}

int checkRead(int block)
{
    unsigned char *checkBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, checkBlock);
    char readBlockStr[3] = {0};
    strncpy(readBlockStr, checkBlock + 4, 2);
    int readBlock = atoi(readBlockStr);
    free(checkBlock);
    return readBlock;
}

int checkFileSize(int block)
{
    unsigned char *fileBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, fileBlock);
    char fileBlockStr[5] = {0};
    if (block == 1)
    {
        strncpy(fileBlockStr, fileBlock + 10, 4);
    }
    else
    {
        strncpy(fileBlockStr, fileBlock + 6, 4);
    }
    int sizeBlock = atoi(fileBlockStr);
    free(fileBlock);
    return sizeBlock;
}

void updateRead(int block, int reads)
{
    unsigned char *checkBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, checkBlock);
    char readBlockStr[3] = {0};
    strncpy(readBlockStr, checkBlock + 4, 2);
    int readBlock = atoi(readBlockStr);
    readBlock = readBlock + reads + 1;
    if (readBlock > 63)
    {
        readBlock = 63;
    }
    sprintf(readBlockStr, "%02d", readBlock);
    memcpy(checkBlock + 4, readBlockStr, 2);
    blockWrite(block, checkBlock);
    free(checkBlock);
}

void updateFileSize(int block, int size)
{
    unsigned char *fileBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, fileBlock);
    char sizeBlockStr[5] = {0};
    if (block == 1)
    {
        strncpy(sizeBlockStr, fileBlock + 10, 4);
    }
    else
    {
        strncpy(sizeBlockStr, fileBlock + 6, 4);
    }
    int sizeBlock = atoi(sizeBlockStr);
    if (sizeBlock == 0)
    {
        sizeBlock = sizeBlock + 10;
    }
    sizeBlock = sizeBlock + size;
    sprintf(sizeBlockStr, "%04d", sizeBlock);
    if (block == 1)
    {
        memcpy(fileBlock + 10, sizeBlockStr, 4);
    }
    else
    {
        memcpy(fileBlock + 6, sizeBlockStr, 4);
    }
    blockWrite(block, fileBlock);
    free(fileBlock);
}

/*
 * Reads data from the start of the file.
 * Maintains a file position so that subsequent reads continue
 * from where the last read finished.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int a2read(char *fileName, void *data, int length)
{
    int currentBlockNum = searchForFile(fileName);
    unsigned char *currentBlock = malloc(64);
    unsigned char *dataBlock = malloc(length);
    int startPos = 0;
    int readingLoop = 1;
    int dataLeft = 0;
    int offset = 0;
    int lengthToData = length;
    while (readingLoop == 1)
    {
        readingLoop = 0;
        startPos = checkRead(currentBlockNum);
        blockRead(currentBlockNum, currentBlock);
        dataLeft = 64 - startPos;
        if (length > dataLeft)
        {
            readingLoop = 1;
            // Need to read overflowblock but make sure read this one first
            memcpy(dataBlock + offset, currentBlock + startPos, dataLeft);
            length = length - dataLeft;
            updateRead(currentBlockNum, 64);
            offset = offset + dataLeft;
            currentBlockNum = checkOverflow(currentBlockNum);
        }
        else
        {
            // No need to look for overflowblock this is last block
            memcpy(dataBlock + offset, currentBlock + startPos, length);
            updateRead(currentBlockNum, (length - 1));
            offset = length;
            length = 0;
            break;
        }
    }
    memcpy(data, dataBlock, lengthToData);
    free(currentBlock);
    free(dataBlock);
    return 0;
}

/*
 * Repositions the file pointer for the file at the specified location.
 * All positive integers are byte offsets from the start of the file.
 * 0 is the beginning of the file.
 * If the location is after the end of the file, move the location
 * pointer to the end of the file.
 * The filename is a full pathname.
 * The file must have been created before this call is made.
 * Returns 0 if no problem or -1 if the call failed.
 */
int seek(char *fileName, int location)
{
    int parentBlockNum = searchForFile(fileName);
    updateRead(parentBlockNum, location - 1);
    return 0;
}
