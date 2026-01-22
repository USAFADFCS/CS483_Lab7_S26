/* ============================================
 * SHARED MEM - shared_sync.c
 * ============================================
 * This program creates two children that send and
 * read messages from a shared memory region
 * 
 * To compile using gcc:
 *   gcc -o shared_sync shared_sync.c -lrt
 * 
 * USAGE: 
 *   ./shared_sync
 */#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>

#define SHM_NAME "/my_shared_memory"
#define SHM_SIZE 1024
#define MAX_MSG_LEN 256

// Structure for the shared message
typedef struct {
    int message_id;
    char text_message[MAX_MSG_LEN];
    int ready;  // Synchronization flag: 0 = can write, 1 = can read
} SharedMessage;

int main() {
    int shm_fd;
    SharedMessage *shared_mem;
    pid_t pid1, pid2;

    // shm_open() - Create and open a POSIX shared memory object
    // Parameters:
    //   SHM_NAME ("/my_shared_memory") - name of shared memory object (like a file path)
    //                                    visible in /dev/shm/ on Linux
    //   O_CREAT | O_RDWR - flags:
    //     O_CREAT: create the object if it doesn't exist
    //     O_RDWR: open for reading and writing
    //   0666 - permissions (read/write for owner, group, others)
    // Returns: file descriptor for the shared memory object, or -1 on error
    // Note: Creates object in /dev/shm/ filesystem on Linux
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }

    // ftruncate() - Set the size of the shared memory object
    // Parameters:
    //   shm_fd - file descriptor from shm_open()
    //   SHM_SIZE (1024) - size to set in bytes
    // Returns: 0 on success, -1 on error
    // Purpose: Newly created shared memory objects have size 0, must be resized
    // Note: This is like resizing a file to the desired length
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        exit(1);
    }

    // mmap() - Map shared memory object into process address space
    // Parameters:
    //   NULL - let kernel choose the address where to map (recommended)
    //   SHM_SIZE (1024) - number of bytes to map
    //   PROT_READ | PROT_WRITE - memory protection flags:
    //     PROT_READ: pages may be read
    //     PROT_WRITE: pages may be written
    //   MAP_SHARED - changes are shared with other processes mapping same object
    //                (MAP_PRIVATE would create copy-on-write private mapping)
    //   shm_fd - file descriptor of shared memory object
    //   0 - offset into the file (0 means start from beginning)
    // Returns: pointer to mapped memory, or MAP_FAILED on error
    // Purpose: Maps the shared memory into this process's virtual address space
    shared_mem = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    // Initialize shared memory
    shared_mem->message_id = 0;
    shared_mem->ready = 0;  // Start with ready=0 (writer can write)
    memset(shared_mem->text_message, 0, MAX_MSG_LEN);

    printf("Parent: Created shared memory object '%s' (fd: %d)\n", SHM_NAME, shm_fd);
    printf("Parent: Mapped at address %p\n", (void *)shared_mem);
    printf("Parent: Using ready flag for synchronization\n\n");

    // Note: Child inherits the mapping, so both parent and child share same physical memory
    pid1 = fork();
    
    if (pid1 < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid1 == 0) {
        // Child 1 - Writer process
        printf("Child 1 (PID %d): Started as WRITER\n", getpid());
        
        for (int i = 1; i <= 5; i++) {
            // TODO - Wait until ready flag is 0 (reader has read previous message)
            // Busy-wait loop - keeps checking until ready becomes 0
            // Note: This is a spin lock - inefficient but simple for demonstration
 
            
            // Now we can safely write - ready=0 means no reader is reading
            shared_mem->message_id = 100 + i;
            snprintf(shared_mem->text_message, MAX_MSG_LEN, 
                    "Message from Child 1 (writer), iteration %d", i);
            
            printf("Child 1: WROTE message ID %d: '%s'\n", 
                   shared_mem->message_id, shared_mem->text_message);
            
            // TODO - Set ready=1 to signal reader that message is ready

            
            usleep(100000);  // Sleep 100ms between writes
        }
        
        printf("Child 1: Finished writing\n");
        munmap(shared_mem, SHM_SIZE);
        close(shm_fd);
        exit(0);
    }

    // Both children inherit the memory mapping to same physical shared memory
    pid2 = fork();
    
    if (pid2 < 0) {
        perror("fork");
        exit(1);
    }
    
    if (pid2 == 0) {
        // Child 2 - Reader process
        printf("Child 2 (PID %d): Started as READER\n", getpid());
        
        for (int i = 1; i <= 5; i++) {
            // TODO - Wait until ready flag is 1 (writer has written a message)
            // Busy-wait loop - keeps checking until ready becomes 1

            
            // Now we can safely read - ready=1 means writer has finished writing
            printf("Child 2: READ message ID %d: '%s'\n", 
                   shared_mem->message_id, shared_mem->text_message);
            
            // TODO - Set ready=0 to signal writer that we've read the message
            // and it's safe to write the next one

            
            usleep(100000);  // Sleep 100ms between reads
        }
        
        printf("Child 2: Finished reading\n");
        
        // munmap() - Unmap shared memory from process address space
        // Parameters:
        //   shared_mem - pointer returned by mmap()
        //   SHM_SIZE - size of mapped region in bytes
        // Returns: 0 on success, -1 on error
        // Note: Doesn't delete the shared memory object, just unmaps from this process
        munmap(shared_mem, SHM_SIZE);
        
        close(shm_fd);
        exit(0);
    }

    // Parent process waits for both children
    printf("Parent: Waiting for children to complete...\n");
    
    waitpid(pid1, NULL, 0);
    printf("Parent: Child 1 (PID %d) completed\n", pid1);
    
    waitpid(pid2, NULL, 0);
    printf("Parent: Child 2 (PID %d) completed\n", pid2);

    // Cleanup: unmap and remove shared memory
    munmap(shared_mem, SHM_SIZE);  // Unmap from parent's address space
    close(shm_fd);  // Close file descriptor
    
    // shm_unlink() - Remove shared memory object
    // Parameters: SHM_NAME - name of shared memory object to remove
    // Returns: 0 on success, -1 on error
    // Effect: Removes the shared memory object from /dev/shm/
    // Note: Like unlink() for files - object destroyed when last process unmaps it
    shm_unlink(SHM_NAME);
    
    printf("Parent: Cleaned up shared memory. Exiting.\n");
    
    return 0;
}
