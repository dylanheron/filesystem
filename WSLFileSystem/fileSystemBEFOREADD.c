/*
 * fileSystem.c
 *
 *  Modified on:
 *      Author: Your UPI
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
        return -1;
    }

    // TESTING -----------------------------------------

    // printf("Value at unsignedVol = ");

    // for (int i = 0; unsignedVol[i] != '\0'; i++)
    // {
    // 	printf("%c", unsignedVol[i]);
    // }

    // printf("\n");

    // TESTING -----------------------------------------

    blockWrite(0, unsignedVol);

    unsigned char rootDir[64];
    memset(rootDir, 0, 64);
    char *overflowBlock = "00020000";
    strcpy(rootDir, overflowBlock);

    blockWrite(1, rootDir);

    // TESTING -----------------------------------------

    // unsigned char *readArray = malloc(64); // allocate 64 bytes of memory

    // blockRead(1, readArray);

    // printf("Value at readArray = ");

    // for (int i = 0; readArray[i] != '\0'; i++)
    // {
    // 	printf("%c", readArray[i]);
    // }

    // printf("\n");

    // TESTING -----------------------------------------

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
    int parentBlockNum = 1;

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
        if (i == numParts - 1) // file
        {
            /// NEED TO ADJUST TO CHECK IF ROOM
            unsigned char *parentBlock = malloc(64); // allocate 64 bytes of memory
            // placing file name in parentBlock
            blockRead(parentBlockNum, parentBlock);
            int freePosition = endOfData(parentBlockNum);
            if ((63 - freePosition) < 12)
            {
                int overflowBlock = checkOverflow(parentBlockNum);
                if (overflowBlock > 0)
                {
                    parentBlockNum = overflowBlock;
                    blockRead(parentBlockNum, parentBlock);
                    freePosition = endOfData(parentBlockNum);
                }
                else
                {
                    updateOverflow(parentBlockNum);
                    int newOverflowBlock = checkOverflow(parentBlockNum);
                    parentBlockNum = newOverflowBlock;
                    // create new file in block
                    createNewFileBlock(parentBlockNum, 0);
                    updateFreeBlock();
                    blockRead(parentBlockNum, parentBlock);
                    freePosition = endOfData(parentBlockNum);
                }
            }
            memcpy(parentBlock + freePosition, parts[i], 8);
            // placinf file block in parentBlock
            int freeBlock = getFreeBlock();
            char freeBlockStr[5] = {0};
            sprintf(freeBlockStr, "%04d", freeBlock);
            // for (int i = 0; i < 5; i++)
            // {
            // 	printf("%c ", freeBlockStr[i]); // print out each individual character
            // }
            memcpy(parentBlock + freePosition + 8, freeBlockStr, 4);
            blockWrite(parentBlockNum, parentBlock);
            // prints
            // for (int i = 0; i < 10; i++)
            // {
            // 	printf("%c ", parentBlock[i]); // print out each individual character
            // }
            // printf("Parent block: %d", parentBlockNum);
            // printf("freePosition: %d", freePosition);
            // printf("\n");
            // creating file in new file block
            createNewFileBlock(freeBlock, 0);
            updateFreeBlock();
        }
        else
        { // dir
          // if dir exists move into dir
          // if dir does not exist create dir
            unsigned char *currentDirBlock = malloc(64);
            blockRead(parentBlockNum, currentDirBlock);
            unsigned char *noInfoBlock = malloc(58);
            memcpy(noInfoBlock, currentDirBlock + 6, 58);
            int numOfPossibleFiles = 5;
            if (parentBlockNum == 1)
            { // root
                numOfPossibleFiles = 4;
                memcpy(noInfoBlock, currentDirBlock + 8, 56);
            }
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
                    // check if room
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
                    updateFreeBlock();
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
        char *overflowBlock = "0000";
        strcpy(fileBlock, overflowBlock);
        blockWrite(freeBlock, fileBlock);
    }
    else
    {
        char *overflowBlock = "000006";
        strcpy(fileBlock, overflowBlock);
        blockWrite(freeBlock, fileBlock);
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
}

int getFreeBlock()
{
    unsigned char *rootBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(1, rootBlock);
    char freeBlockStr[5] = {0};
    strncpy(freeBlockStr, rootBlock, 4);
    int freeBlock = atoi(freeBlockStr);
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
    updateFreeBlock();
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
}

void updateFreeBlock()
{
    // find next free block
    unsigned char *rootBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(1, rootBlock);
    char freeBlockStr[5] = {0};
    strncpy(freeBlockStr, rootBlock, 4);
    int freeBlock = atoi(freeBlockStr); // FOUND ----------->>>>
    // update free block value
    int nextFreeBlock = freeBlock + 1;
    sprintf(freeBlockStr, "%04d", nextFreeBlock);
    memcpy(rootBlock, freeBlockStr, 4);
    blockWrite(1, rootBlock);
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

    char *token;
    char *pathCopy = strdup(directoryName); // make a copy of directoryName since strtok() modifies the string
    char **parts = NULL;                    // array to store the individual parts of the path
    int numParts = 0;
    int parentBlockNum = 1;

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
        memcpy(noInfoBlock, currentDirBlock + 6, 58);
        int numOfPossibleFiles = 4;
        if (parentBlockNum == 1)
        { // root
            memcpy(noInfoBlock, currentDirBlock + 8, 56);
        }
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
            continue;
        }
    }
    unsigned char *finalDirBlock = malloc(64);
    blockRead(parentBlockNum, finalDirBlock);
    unsigned char *noInfoBlock = malloc(58);
    memcpy(noInfoBlock, finalDirBlock + 6, 58);
    int numOfPossibleFiles = 5;
    if (parentBlockNum == 1)
    { // root
        numOfPossibleFiles = 4;
        memcpy(noInfoBlock, finalDirBlock + 8, 56);
    }
    sprintf(result, "%s:\n", directoryName);
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
        int fileSize = findFileSize(nextFileLocNum);
        sprintf(result + strlen(result), "%s:\t%d\n", fileName, fileSize);
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
    int parentBlockNum = searchForFile(fileName);
    unsigned char *currentFile = malloc(64);
    blockRead(parentBlockNum, currentFile);
    int freePosition = endOfData(parentBlockNum);
    int roomLeft = 64 - freePosition;
    if (length > (roomLeft))
    {
        memcpy(currentFile + freePosition, data, roomLeft);
        blockWrite(parentBlockNum, currentFile);
        unsigned char *parentBlock = malloc(64); // allocate 64 bytes of memory
        // placing file name in parentBlock
        blockRead(parentBlockNum, parentBlock);
        int overflowBlock = checkOverflow(parentBlockNum);
        if (overflowBlock > 0)
        {
            parentBlockNum = overflowBlock;
            blockRead(parentBlockNum, parentBlock);
            freePosition = endOfData(parentBlockNum);
        }
        else
        {
            updateOverflow(parentBlockNum);
            int newOverflowBlock = checkOverflow(parentBlockNum);
            parentBlockNum = newOverflowBlock;
            // create new file in block
            createNewFileBlock(parentBlockNum, 0);
            updateFreeBlock();
            blockRead(parentBlockNum, parentBlock);
            freePosition = endOfData(parentBlockNum);
        }
        unsigned char *currentFileAgain = malloc(64);
        blockRead(parentBlockNum, currentFileAgain);
        freePosition = endOfData(parentBlockNum);
        memcpy(currentFileAgain + freePosition, data + roomLeft, length - roomLeft);
        blockWrite(parentBlockNum, currentFileAgain);
    }
    else
    {
        memcpy(currentFile + freePosition, data, length);
        blockWrite(parentBlockNum, currentFile);
    }

    free(currentFile);
    return 0;
}

int findFileSize(int startingBlock)
{
    unsigned char *currentFile = malloc(64);
    blockRead(startingBlock, currentFile);
    int fileSize = strlen(currentFile);
    fileSize = fileSize - 6;
    if ((fileSize + 6) > 63)
    {
        unsigned char *overFlowFile = malloc(64);
        int overflowBlock = checkOverflow(startingBlock);
        if (overflowBlock > 0)
        {
            blockRead(overflowBlock, overFlowFile);
            int overFlowSize = strlen(overFlowFile);
            overFlowSize = overFlowSize - 6;
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
    return fileSize;
}

int searchForFile(char *fileName)
{
    char *token;
    char *pathCopy = strdup(fileName); // make a copy of directoryName since strtok() modifies the string
    char **parts = NULL;               // array to store the individual parts of the path
    int numParts = 0;
    int parentBlockNum = 1;

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
        memcpy(noInfoBlock, currentDirBlock + 6, 58);
        int numOfPossibleFiles = 5;
        if (parentBlockNum == 1)
        { // root
            numOfPossibleFiles = 4;
            memcpy(noInfoBlock, currentDirBlock + 8, 56);
        }
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
            continue;
        }
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
    return readBlock;
}

void updateRead(int block, int reads)
{
    unsigned char *checkBlock = malloc(64); // allocate 64 bytes of memory
    blockRead(block, checkBlock);
    char readBlockStr[3] = {0};
    strncpy(readBlockStr, checkBlock + 4, 2);
    int readBlock = atoi(readBlockStr);
    readBlock = readBlock + reads;
    if (readBlock > 63)
    {
        readBlock = 63;
    }
    sprintf(readBlockStr, "%02d", readBlock);
    memcpy(checkBlock + 4, readBlockStr, 2);
    blockWrite(block, checkBlock);
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
    int parentBlockNum = searchForFile(fileName);
    int startPos = 6;
    unsigned char *currentFile = malloc(64);
    unsigned char *dataBlock = malloc(64);
    memset(currentFile, 0, 64);
    memset(dataBlock, 0, 64);
    blockRead(parentBlockNum, currentFile);
    memset(data, 0, strlen(data));
    startPos = checkRead(parentBlockNum);
    int space = 64 - startPos;
    int preOLength = length;
    if (preOLength > space)
    {
        preOLength = space + 1;
    }
    else
    {
        printf("len: %d\n", strlen(currentFile));
        if (length == strlen(currentFile))
        {
            preOLength = preOLength - 1;
        }
    }
    memcpy(dataBlock, currentFile + startPos, preOLength);
    // currentFile[startPos + preOLength];
    // memcpy(dataBlock, currentFile + startPos, preOLength - 1);
    updateRead(parentBlockNum, (length - 1));
    int overflow = checkOverflow(parentBlockNum);
    if (overflow > 0 && preOLength == space + 1)
    {
        int overflowBlockNum = overflow;
        unsigned char *overflowFile = malloc(64);
        memset(overflowFile, 0, 64);
        blockRead(overflowBlockNum, overflowFile);
        startPos = checkRead(overflowBlockNum);
        memcpy(dataBlock + (space), overflowFile + startPos, (length - space));
        updateRead(overflowBlockNum, (length - 1));
    }
    // memcpy(data, dataBlock, length);
    memcpy(data, dataBlock, length);
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
    updateRead(parentBlockNum, location);
    return 0;
}
