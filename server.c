#include <stdio.h>
#include <rpc/rpc.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "ssnfs.h"

#define MAX_FILES 100 // Maximum number of files in filesystem
#define MAX_BLOCKS 64 // Maximum number of blocks per file.
#define BLOCK_SIZE 512 // size of each block in bytes.
#define MAX_OPEN 10 // maximum number of file open at any instance

#define DATA_SEG (1024 * 1024 * 10) //total size allocated for file data
#define FILE_SEG (sizeof(file_info) * MAX_FILES) //total size for storing file metadata
#define BLOCKS_SEG 2560 //total size for storing virtual disk block information

#define DATA_SEG_LOC 0 //start point of segment for storing file data
#define FILE_SEG_LOC (DATA_SEG) //start point of segment for storing
#define BLOCKS_SEG_LOC (FILE_SEG_LOC + FILE_SEG) //start point of segment block information


struct file_info { // Holds the file information.
    char username[10]; // Username of the file's owner.
    char filename[10]; // Name of the file.
    int used; // Number of blocks used.
    int blocks[MAX_BLOCKS]; // A list of the block numbers used by the file.
};
typedef struct file_info file_info;

struct file_session { // Holds the session information for all files.
    int fd; //file descriptor value
    char filename[20]; // Name of the file.
    char username[20]; //owner of the file
    int write_position; //for storing last write position
    int read_position; //for storing last read position
};
typedef struct file_session file_session;

file_session session_table[10]; // where the list of all open files are stored in memory
int vfd = 1; // for assignment fd to open files


// Initializes the virtual disk file. Uses only one .dat file
int init_disk() {
    if (access("disk.dat", F_OK) == 0) {
    } else {
        ftruncate(creat("disk.dat", 0666), DATA_SEG + FILE_SEG + BLOCKS_SEG);
    }
}

// For obtaining a free block from virtual memory for a file. Returns location of newly aquired block
int get_free_blocks() {
    int fd,i;
    char flag;
    fd = open("disk.dat", O_RDWR);
    lseek(fd, BLOCKS_SEG_LOC, SEEK_SET); // go to segment where block info is stored

    if (fd == -1)
        printf("%s\n", strerror(errno));
    read(fd, &flag, 1);
    for (i = 0; flag; i++) {
        if (read(fd, &flag, 1) == -1) {
            printf("%s\n", strerror(errno));
            break;
        }
    }
    lseek(fd, -1, SEEK_CUR);
    write(fd, "\xff", 1); // mark block location as occupied
    close(fd);
    return i;
}

// Checks if a file exists.  Return 0 on false, 1 on true.
int file_exists(char *username, char *filename) {
    int fd, exists;
    file_info fi;
    fd = open("disk.dat", O_RDONLY);
    lseek(fd, FILE_SEG_LOC, SEEK_SET); // go to segment where file info is stored

    for (exists = 0; read(fd, &fi, sizeof(fi)) > 0 && lseek(fd,0,SEEK_CUR) < BLOCKS_SEG_LOC;) // searching is limited to only where file table is stored
    {
        if ((strcmp(username, fi.username) == 0) && (strcmp(filename, fi.filename) == 0)) {
            exists = 1;
            break;
        }
    }
    close(fd);
    return exists;
}

// Get a specific file information.
int get_file_info(char *username, char *filename, file_info *fi) {
    int fd, exists;
    fd = open("disk.dat", O_RDONLY);
    lseek(fd, FILE_SEG_LOC, SEEK_SET); //// go to segment where block info is stored
    for (exists = 0; read(fd, fi, sizeof(file_info)) > 0 && lseek(fd,0,SEEK_CUR) < BLOCKS_SEG_LOC;) {
        if ((strcmp(username, fi->username) == 0) && (strcmp(filename, fi->filename) == 0)) {
            exists = 1;
            break;
        }
    }
    close(fd);
    return exists;
}

// Modifies a files information
void change_file_info(file_info *fi) {
    int fd, exists;
    file_info block;
    fd = open("disk.dat", O_RDWR);
    lseek(fd, FILE_SEG_LOC, SEEK_SET);
    for (exists = 0; read(fd, &block, sizeof(file_info)) > 0 && lseek(fd,0,SEEK_CUR) < BLOCKS_SEG_LOC;) {
        if ((strcmp(fi->username, block.username) == 0) && (strcmp(fi->filename, block.filename) == 0)) {
            exists = 1;
            break;
        }
    }
    if (exists) {
        lseek(fd, -sizeof(file_info), SEEK_CUR);
        write(fd, fi, sizeof(file_info));
    }
    close(fd);
}

// Adds a new entry to the file table.
int add_file(file_info fi) {
    int fd, found;
    file_info block;
    fd = open("disk.dat", O_RDWR);
    lseek(fd, FILE_SEG_LOC, SEEK_SET);
    for (found = 0; read(fd, &block, sizeof(block)) > 0 && lseek(fd,0,SEEK_CUR) < BLOCKS_SEG_LOC;) {
        if (block.username[0] == 0) {
            found = 1;
            break;
        }
    }
    // insert file
    if (found) {
        lseek(fd, -sizeof(fi), SEEK_CUR);
        printf("write %zd\n", write(fd, &fi, sizeof(fi)));
    }
    close(fd);
    // return result
    return found;
}

// Gets the names of all the files owned by a particular user
void file_list(char *username, char *buffer) {
    memset(buffer, 0, 512);
    int fd;
    file_info fi;
    fd = open("disk.dat", O_RDONLY);
    lseek(fd, FILE_SEG_LOC, SEEK_SET);
    while (read(fd, &fi, sizeof(fi)) > 0 && lseek(fd,0,SEEK_CUR) < BLOCKS_SEG_LOC) {
        if (strcmp(username, fi.username) == 0) {
            strcat(buffer, "\n");
            strcat(buffer, fi.filename);
        }
    }
    close(fd);
}

// Free a block after a file has been deleted by setting its entry to 0.
void free_blocks(int block_index) {
    int fd;
    fd = open("disk.dat", O_RDWR);
    lseek(fd, BLOCKS_SEG_LOC, SEEK_SET);

 //   lseek(fd, block_index, SEEK_CUR);
    lseek(fd, block_index, SEEK_SET);
    write(fd, "\x00", 1);
    close(fd);
}

// Delete a file owned by a user.
int file_delete(char *username, char *filename) {
    int found, i, fd;
    file_info fi;

    fd = open("disk.dat", O_RDWR);
    lseek(fd, FILE_SEG_LOC, SEEK_SET); //go to file table segment and search for file
    for (found = 0; read(fd, &fi, sizeof(fi)) > 0 && lseek(fd,0,SEEK_CUR) < BLOCKS_SEG_LOC;) {
        if ((strcmp(username, fi.username) == 0) && (strcmp(filename, fi.filename) == 0)) {
            found = 1;
            break;
        }
    }
    // delete file
    if (found) {
        // free disk blocks allocated to file
        for (i = 0; i < fi.used; i++) {
            free_blocks(fi.blocks[i]);
        }
        lseek(fd, -sizeof(fi), SEEK_CUR);
        memset(&fi, 0, sizeof(fi));
        write(fd, &fi, sizeof(fi));
    }
    close(fd);

    return found;
}

//Adds a file to the file session table whenever it is opened. Returns the file_session struct
file_session *add_fs(char *username, char *filename) {
    int i;

    //check if the table is already open
    for (i = 0; i < 10; ++i) {
        if (strcmp(filename, session_table[i].filename) == 0 && strcmp(session_table[i].username, username) == 0) {
            return &session_table[i];
        }

        //if file not, create a new entry
        if (strcmp(session_table[i].filename, "") == 0) {
            strcpy(session_table[i].filename, filename);
            strcpy(session_table[i].username, username);
            session_table[i].fd = vfd++;
            session_table[i].read_position = 0;
            session_table[i].write_position = 0;
            break;
        }
    }
    //return session
    return &session_table[i];
}

//Gets a file session
file_session *get_fs(char *username, int *fd) {
    int i;
    for (i = 0; i < MAX_OPEN; ++i) {
        if (session_table[i].fd == *fd && strcmp(session_table[i].username, username) == 0) {
            return &session_table[i];
        }
    }

    return NULL;
}

//Deletes a file session from memory when file is closed or deleted
int del_fs(char *filename) {
    int i;

    for (i = 0; i < MAX_OPEN; ++i) {
        if (strcmp(session_table[i].filename, filename) == 0) {

            strcpy(session_table[i].filename, "");
            strcpy(session_table[i].username, "");
            session_table[i].fd = -2;
            return 1;
        }
    }

    return 0;
}


/*
 * Server RPC procedures
 * */

// Open Procedure
open_output *open_file_1_svc(open_input *inp, struct svc_req *rqstp) {
    int fd, block;
    file_info fi;

    printf("open_file_1_svc: (user_name = '%s', file_name = '%s')\n", inp->user_name, inp->file_name);
    init_disk();

    if (!file_exists(inp->user_name, inp->file_name)) {
        strcpy(fi.username, inp->user_name);
        strcpy(fi.filename, inp->file_name);
        fi.used = 1;
        block = get_free_blocks();
        if (block != -1)
            fi.blocks[0] = block;
        else
            printf("\nno blocks left\n");
        if (add_file(fi)) {
            fd = add_fs(inp->user_name, inp->file_name)->fd;

        } else {
            fd = -1;
        }
    } else {
        fd = add_fs(inp->user_name, inp->file_name)->fd;
    }

    static open_output out;
    out.fd = fd;
    return &out;
}

// Write Procedure
write_output *write_file_1_svc(write_input *inp, struct svc_req *rqstp) {
    char message[512];
    file_session *fs;
    char *buffer;
    file_info fi;
    int fdi, numbytes, offset, at, block_index, block_num, len;
    init_disk();
    printf("write_file_1_svc: (user_name = '%s', fdi = '%d' numbytes = %d)\n",
           inp->user_name, inp->fd, inp->numbytes);
    printf("write buffer: %s\n", inp->buffer.buffer_val);
    static write_output out;

    fs = get_fs(inp->user_name, &inp->fd);

    if (!fs) {
        snprintf(message, 512, "Error: File is not Open or does not exist.\n");
        out.out_msg.out_msg_len = strlen(message) + 1;
        out.out_msg.out_msg_val = strdup(message);
        printf("%s (%d)\n", out.out_msg.out_msg_val, out.out_msg.out_msg_len);
        return &out;
    }

    if (file_exists(inp->user_name, fs->filename)) {
        //you may need to adjust this
        numbytes = inp->numbytes < strlen(inp->buffer.buffer_val) ? inp->numbytes : strlen(inp->buffer.buffer_val);
        buffer = inp->buffer.buffer_val;
        get_file_info(inp->user_name, fs->filename, &fi);

        if (fs->write_position > (fi.used * BLOCK_SIZE - 1)) {
            printf("used = %d\n", fi.used);
            snprintf(message, 512, "Error: writing past end of file.");
        } else if ((fs->write_position + numbytes) > (BLOCK_SIZE * MAX_BLOCKS)) {
            snprintf(message, 512, "Error: write is too large.");
        } else {
            offset = fs->write_position;
            at = 0;
            while (at < numbytes) {
                block_index = offset / BLOCK_SIZE;
                if (block_index == fi.used) {
                    block_num = get_free_blocks();
                    fi.blocks[fi.used] = block_num;
                    fi.used++;
                    change_file_info(&fi);
                } else {
                    block_num = fi.blocks[block_index];
                }
                if ((numbytes - at) < (BLOCK_SIZE - (offset % BLOCK_SIZE))) {
                    len = numbytes - at;
                } else {
                    len = BLOCK_SIZE - (offset % BLOCK_SIZE);
                }
                fdi = open("disk.dat", O_RDWR);
                lseek(fdi, (BLOCK_SIZE * block_num) + (offset % BLOCK_SIZE), SEEK_SET);
                write(fdi, buffer + at, len);
                close(fdi);
                at += len;
                offset += len;
            }
            snprintf(message, 512, "%d BYTES SUCCESSFULLY WRITTEN TO %s", numbytes, fs->filename);
            fs->write_position = offset;
            change_file_info(&fi);
        }
    } else {
        // file doesn't exist
        snprintf(message, 512, "ERROR: FILE %s DOES NOT EXIST.", fs->filename);
    }
    out.out_msg.out_msg_len = strlen(message) + 1;
    out.out_msg.out_msg_val = strdup(message);
    return &out;
}

// RPC call
read_output *read_file_1_svc(read_input *inp, struct svc_req *rqstp) {

    char *message, *buffer;
    int offset, numbytes, at, block_index, block_num, len, fd, buffer_size, message_size, read_fail;
    file_session *fs;
    file_info fi;
    init_disk();

    static read_output out;

    fs = get_fs(inp->user_name, &inp->fd);

    if (!fs) {
        message = malloc(512);
        snprintf(message, 512, "0\n");
        out.out_msg.out_msg_len = strlen(message) + 1;
        out.out_msg.out_msg_val = strdup(message);
        free(message);
        printf("%s (%d)\n", out.out_msg.out_msg_val, out.out_msg.out_msg_len);
        return &out;
    }

    printf("read_file_1_svc: (user_name = '%s', file_name = '%s' offset = %d numbytes = %d)\n",
           inp->user_name, fs->filename, fs->read_position, inp->numbytes);


    if (file_exists(inp->user_name, fs->filename)) {
        offset = fs->read_position;
        numbytes = inp->numbytes;
        at = 0;
        buffer_size = numbytes + 1;
        buffer = malloc(buffer_size);
        memset(buffer, 0, buffer_size);
        char *reply = "";
        message_size = strlen(reply) + buffer_size;
        get_file_info(inp->user_name, fs->filename, &fi);
        read_fail = 0;
        while (at < numbytes) {
            block_index = offset / BLOCK_SIZE;
            if (block_index >= fi.used) {
                read_fail = 1;
                break;
            }
            block_num = fi.blocks[block_index];
            if ((numbytes - at) < (BLOCK_SIZE - (offset % BLOCK_SIZE))) {
                len = numbytes - at;
            } else {
                len = BLOCK_SIZE - (offset % BLOCK_SIZE);
            }
            fd = open("disk.dat", O_RDONLY);
            lseek(fd, (BLOCK_SIZE * block_num) + (offset % BLOCK_SIZE), SEEK_SET);
            read(fd, buffer + at, len);
            close(fd);
            at += len;
            offset += len;
        }
        fs->read_position = offset;
        buffer[numbytes] = '\x00';
        if (read_fail) {
            message = malloc(512);
            snprintf(message, 512, "0");
            printf("%s\n", message);
        } else {
            message = malloc(message_size);
            memset(message, 0, message_size);
            fs->read_position = offset;
            snprintf(message, message_size, "%s", buffer);
        }
    } else {
        message = malloc(512);
        snprintf(message, 512, "0");
    }

    out.out_msg.out_msg_len = strlen(message) + 1;
    out.out_msg.out_msg_val = strdup(message);
    free(buffer);
    free(message);
    return &out;
}

// List Procedure
list_output *list_files_1_svc(list_input *inp, struct svc_req *rqstp) {
    char message[512];
    char buffer[512];

    init_disk();

    printf("list_file_1_svc: (username = '%s')\n", inp->user_name);
    file_list(inp->user_name, buffer);
    printf("files: %s\n", buffer);
    static list_output out;
    snprintf(message, 512, "USER %s HAS THE FOLLOWING FILES:%s", inp->user_name, buffer);
    out.out_msg.out_msg_len = strlen(message) + 1;
    out.out_msg.out_msg_val = strdup(message);
    return &out;
}

// close Procedure
close_output *close_file_1_svc(close_input *inp, struct svc_req *rqstp) {

    char *message, *filename;
    file_session *fs;

    init_disk();
    static close_output out;

    printf("close_file_1_svc: (user_name = '%s', fd = %i )\n",
           inp->user_name, inp->fd);

    fs = get_fs(inp->user_name, &inp->fd);

    if (!fs) {
        message = malloc(512);
        snprintf(message, 512, "Error: File is not Open or does not exist.\n");
        out.out_msg.out_msg_len = strlen(message) + 1;
        out.out_msg.out_msg_val = strdup(message);
        free(message);
        printf("%s (%d)\n", out.out_msg.out_msg_val, out.out_msg.out_msg_len);
        return &out;
    }

    strcpy(filename, fs->filename);

    if (del_fs(fs->filename) == 1) {
        message = malloc(512);
        snprintf(message, 512, "%s CLOSED", filename);

    } else {
        message = malloc(512);
        snprintf(message, 512, "\nERROR CLOSING %s\n", fs->filename);
    }

    out.out_msg.out_msg_len = strlen(message) + 1;
    out.out_msg.out_msg_val = strdup(message);
    free(message);
    printf("%s (%d)\n", out.out_msg.out_msg_val, out.out_msg.out_msg_len);
    return &out;
}

// delete Procedure
delete_output *delete_file_1_svc(delete_input *inp, struct svc_req *rqstp) {
    char message[512];

    init_disk();

    printf("delete_file_1_svc: (user_name = '%s', file_name = '%s')\n", inp->user_name, inp->file_name);
    static delete_output out;
    if (file_exists(inp->user_name, inp->file_name)) {
        file_delete(inp->user_name, inp->file_name);
        del_fs(inp->file_name);
        snprintf(message, 512, "%s DELETED", inp->file_name);
    } else {
        snprintf(message, 512, "Error: file %s does not exist.", inp->file_name);
    }
    out.out_msg.out_msg_len = strlen(message) + 1;
    out.out_msg.out_msg_val = strdup(message);
    return &out;
}