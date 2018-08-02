// TODO: replace void pointer with a generic output pointer.
// TODO: add readline history.

#include <stdio.h> // For printf, etc.
#include <stdlib.h> // For atoi, etc.
#include <string.h> // Needed for various string manipulations.
#include <rpc/rpc.h> // Header for RPC calls.
#include "ssnfs.h" // Header for the RPC service created by rpcgen.
#include <pwd.h> // Need to get username.

char hostname[100];

CLIENT * binder() {
    CLIENT *cl; // Pointer to the client handle.

    cl = clnt_create(hostname, SSNFSPROG, SSNFSVER, "tcp");
    if (cl == NULL) { // If there was an error creating the client...
        fprintf(stderr, "getting client handle failed\n"); // Report an error message.
        exit(2); // And exit with an error code.
    }

    return cl;
}

int Open(char *filename){
    CLIENT *cl;
    open_input in; // Input struct for the RPC call.
    int fd;
    void *outp;

    cl = binder();

    char * username = getpwuid(geteuid())->pw_name;
    strcpy(in.user_name, username); // Copy user name.
    strcpy(in.file_name, filename); // Copy file name.
    outp = open_file_1(&in, cl); // And call the create file RPC
    fd = ((open_output*)outp)->fd;

    if(fd == -1){
        printf("ERROR OPENING %s\n", filename);
    } else {
        printf("%s OPENED SUCCESSFULLY\n", filename);
    };
    printf("\n");
    return ((open_output*)outp)->fd;
}

write_output Write(int fd, char *buffer, int numbytes){
    CLIENT *cl;
    write_input in; // Input struct for the RPC call.
    void *outp;

    cl = binder();

    char * username = getpwuid(geteuid())->pw_name;

    strcpy(in.user_name, username); // Copy user name.
    in.fd = fd; // Copy fd number.
    in.numbytes = numbytes; // The number of bytes to write.
    in.buffer.buffer_val = strdup(buffer);
    in.buffer.buffer_len = strlen(buffer);
    outp = write_file_1(&in, cl); // And call the create file RPC.

    char * output = malloc(((write_output*)outp)->out_msg.out_msg_len + 1);
    memset(output, 0, ((write_output*)outp)->out_msg.out_msg_len + 1);
    strncpy(output, ((write_output*)outp)->out_msg.out_msg_val, ((write_output*)outp)->out_msg.out_msg_len + 1);
    printf("%s\n", output); // Print the result of the RPC call.
}

read_output Read(int fd, char *buffer, int numbytes){

    CLIENT *cl;
    void *outp;

    cl = binder();

    char * username = getpwuid(geteuid())->pw_name;
    read_input in; // Input struct for the RPC call.
    strcpy(in.user_name, username); // Copy user name.
    in.fd = fd; // Copy fd number.
    in.numbytes = numbytes; // The number of bytes to read.
    outp = read_file_1(&in, cl); // And call the read file RPC.

    if (((read_output*)outp)->out_msg.out_msg_val == 0 ){
        buffer = "0";
    }

    strncpy(buffer, ((read_output*)outp)->out_msg.out_msg_val, ((write_output*)outp)->out_msg.out_msg_len + 1);

}

close_output Close(int fd){
    CLIENT *cl;
    void *outp;

    cl = binder();

    char * username = getpwuid(geteuid())->pw_name;
    close_input in; // Input struct for the RPC call.
    strcpy(in.user_name, username); // Copy user name.
    in.fd = fd; // Copy fd number.

    outp = close_file_1(&in, cl); // And call the read file RPC.

    char * output = malloc(((write_output*)outp)->out_msg.out_msg_len + 1);
    memset(output, 0, ((write_output*)outp)->out_msg.out_msg_len + 1);
    strncpy(output, ((write_output*)outp)->out_msg.out_msg_val, ((write_output*)outp)->out_msg.out_msg_len + 1);
    printf("\n\n%s\n\n", output); // Print the result of the RPC call.

}

list_output List(){
    list_input in; // Input struct for the RPC call.
    void *outp;
    CLIENT *cl; // Pointer to the client handle.

    cl = binder();

    char * username = getpwuid(geteuid())->pw_name; // Get the user's username.
    strcpy(in.user_name, username); // Copy user name.
    outp = list_files_1(&in, cl); // And call the create file RPC.

    char * output = malloc(((write_output*)outp)->out_msg.out_msg_len + 1);
    memset(output, 0, ((write_output*)outp)->out_msg.out_msg_len + 1);
    strncpy(output, ((write_output*)outp)->out_msg.out_msg_val, ((write_output*)outp)->out_msg.out_msg_len + 1);
    printf("%s\n\n", output); // Print the result of the RPC call.
}

delete_output Delete(char * filename){
    delete_input in; // Input struct for the RPC call.
    void *outp;
    CLIENT *cl; // Pointer to the client handle.

    cl = binder();

    char * username = getpwuid(geteuid())->pw_name; // Get the user's username.

    strcpy(in.user_name, username); // Copy user name.
    strcpy(in.file_name, filename); // Copy file name.
    outp = delete_file_1(&in, cl); // And call the create file RPC.

    char * output = malloc(((write_output*)outp)->out_msg.out_msg_len + 1);
    memset(output, 0, ((write_output*)outp)->out_msg.out_msg_len + 1);
    strncpy(output, ((write_output*)outp)->out_msg.out_msg_val, ((write_output*)outp)->out_msg.out_msg_len + 1);
    printf("%s\n\n", output); // Print the result of the RPC call.
}

//Main fuction

int main(int argc, char **argv){

    // To give the user the option to select server location
    if (argc != 2) { // If the user doesn't enter the correct number of parameters...
        fprintf(stderr, "usage: %s <hostname>\n", argv[0]); // Print out usage information.
        exit(1); // And exit with an error code.
    }

    strcpy(hostname, argv[1]);

    int i,j,k;
    int fd1,fd2,fd3;
    char buffer[100];
    fd1=Open("File1"); // opens the file
    for (i=0; i< 20;i++){
        Write(fd1, "funny contents in the file 1", 15);
    }
    Close(fd1);
    fd2=Open("File1");
    for (j=0; j< 20;j++){
        Read(fd2, buffer, 10);
        printf("%s",buffer);
    }
    Close(fd2);
    List();
    Delete("File1");
    List();

}