/* I can be compiled with the command "gcc -o dentls dentls.c" */

#define _GNU_SOURCE
#include <search.h>     /* Defines tree functions */
#include <dirent.h>     /* Defines DT_* constants */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <string.h>

/* Because most filesystems use btree to store dents
 * its very important to perform an in-order removal
 * of the file contents. Performing an 'as-is read' of
 * the contents causes lots of btree rebalancing
 * that has significantly negative effect on unlink performance
 */

/* Tests indicate that performing a ascending order traversal
 * is about 1/3 faster than a descending order traversal */
int compare_fnames(const void *key1, const void *key2) {
  return strcmp((char *)key1, (char *)key2);
}

void walk_tree(const void *node, VISIT val, int lvl) {
  int rc = 0;
  switch(val) {
  case leaf:
    printf("%s\n", *(char **)node);
    // rc = unlink(*(char **)node);
    break;
  /* End order is deliberate here as it offers the best btree
   * rebalancing avoidance.
   */
  case endorder:
    printf("%s\n", *(char **)node);
    // rc = unlink(*(char **)node);
  break;
  default:
    return;
    break;
  }

  if (rc < 0) {
    perror("unlink problem");
    exit(1);
  }

}

void dummy_destroy(void *nil) {
  return;
}

void *tree = NULL;

struct linux_dirent {
        long           d_ino;
        off_t          d_off;
        unsigned short d_reclen;
        char           d_name[256];
        char           d_type;
};

int main(const int argc, const char** argv) {
    int totalfiles = 0;
    int dirfd = -1;
    int offset = 0;
    int bufcount = 0;
    void *buffer = NULL;
    char *d_type;
    struct linux_dirent *dent = NULL;
    struct stat dstat;

    /* Test we have a directory path */
    if (argc < 2) {
        fprintf(stderr, "You must supply a valid directory path.\n");
        exit(1);
    }

    const char *path = argv[1];

    /* Standard sanity checking stuff */
    if (access(path, R_OK) < 0) {
        perror("Could not access directory");
        exit(1);
    }

    if (lstat(path, &dstat) < 0) {
        perror("Unable to lstat path");
        exit(1);
    }

    if (!S_ISDIR(dstat.st_mode)) {
        fprintf(stderr, "The path %s is not a directory.\n", path);
        exit(1);
    }

    /* Allocate a buffer of equal size to the directory to store dents */
    if ((buffer = malloc(dstat.st_size+10240)) == NULL) {
        perror("malloc failed");
        exit(1);
    }

    /* Open the directory */
    if ((dirfd = open(path, O_RDONLY)) < 0) {
        perror("Open error");
        exit(1);
    }

    /* Switch directories */
    fchdir(dirfd);

    while (bufcount = syscall(SYS_getdents, dirfd, buffer, dstat.st_size+10240)) {
        offset = 0;
        dent = buffer;
        while (offset < bufcount) {
            /* Dont print thisdir and parent dir */
            if (!((strcmp(".",dent->d_name) == 0) || (strcmp("..",dent->d_name) == 0))) {
                d_type = (char *)dent + dent->d_reclen-1;
                /* Only print files */
                if (*d_type == DT_REG) {
                    /* Sort all our files into a binary tree */
            if (!tsearch(dent->d_name, &tree, compare_fnames)) {
                      fprintf(stderr, "Cannot acquire resources for tree!\n");
                      exit(1);
                    }
                    totalfiles++;
                }
            }
            offset += dent->d_reclen;
            dent = buffer + offset;
        }
    }
    fprintf(stderr, "Total files: %d\n", totalfiles);
    printf("Performing delete..\n");

    twalk(tree, walk_tree);
    printf("Done\n");
    close(dirfd);
    free(buffer);
    tdestroy(tree, dummy_destroy);
}
