#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))

#define MAX_FD 20

ssize_t bulk_read(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(read(fd, buf, count));
        if (c < 0)
            return c;
        if (c == 0)
            return len;  // EOF
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    return len;
}

ssize_t bulk_write(int fd, char *buf, size_t count)
{
    ssize_t c;
    ssize_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd, buf, count));
        if (c < 0)
            return c;
        buf += c;
        len += c;
        count -= c;
    } while (count > 0);
    printf("\n");
    return len;
}

void dir_stage2(const char *const path)
{
    DIR *dirp;
    struct dirent *dp;
    if (NULL == (dirp = opendir(path)))
        ERR("opendir");
    do
    {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL)
            printf("%s\n", dp->d_name);
    } while (dp != NULL);
    printf("\n");
    if (errno != 0)
        ERR("readdir");
    if (closedir(dirp))
        ERR("closedir");
}

void reg_stage2(const char *const path, const struct stat *const stat_buf)
{
    int fd;
    char *buf = (char *)malloc(stat_buf->st_size);
    if ((fd = open(path, O_RDONLY)) == -1)
        ERR("open");
    printf("Size: %ld\nContents:\n", stat_buf->st_size);
    if (bulk_read(fd, buf, stat_buf->st_size) == -1)
        ERR("bulk_read");
    printf("%s\n", buf);
    if (close(fd) == -1)
        ERR("close");
    free(buf);
}

void show_stage2(const char *const path, const struct stat *const stat_buf)
{
    if (S_ISDIR(stat_buf->st_mode))
        dir_stage2(path);
    else if (S_ISREG(stat_buf->st_mode))
        reg_stage2(path, stat_buf);
    else
        fprintf(stderr, "Unknown filetype");
}

void write_stage3(const char *const path, const struct stat *const stat_buf)
{
    int fd;
    size_t n;
    char *buf = (char *)malloc(stat_buf->st_size);
    if ((fd = open(path, O_RDONLY)) == -1)
        ERR("open");
    printf("Contents:\n");
    if (bulk_read(fd, buf, stat_buf->st_size) == -1)
        ERR("bulk_read");
    printf("%s\n", buf);
    if (close(fd) == -1)
        ERR("close");
    if ((fd = open(path, O_APPEND | O_WRONLY)) == -1)
        ERR("open");
    free(buf);
    buf = NULL;
    n = 0;
    while (getline(&buf, &n, stdin) > 1)
    {
        if (bulk_write(fd, buf, n) == -1)
            ERR("bulk_write");
    }
    if (close(fd) == -1)
        ERR("close");
    free(buf);
}

int disp_stage4(const char *path, const struct stat *filestat, int type, struct FTW *f)
{
    filestat = filestat;
    const char *name = path + f->base;
    if (type == FTW_D)
        printf("Directory: ");
    else if (type == FTW_F)
        printf("File: ");
    else
        printf("Unknown: ");
    printf("%s\n", name);
    return 0;
}

void walk_stage4(const char *const path)
{
    if (nftw(path, disp_stage4, MAX_FD, FTW_PHYS) == -1)
        ERR("nftw");
}

int interface_stage1()
{
    const char *correct_answers = "1234";
    char *answer = NULL;
    size_t an = 0;
    char *path = NULL;
    struct stat filestat;
    size_t n = 0;
    printf("1. show\n2. write\n3. walk\n4.exit\n");
    if (getline(&answer, &an, stdin) == -1)
        ERR("getline");
    if (strlen(answer) > 2 || strchr(correct_answers, answer[0]) == NULL)
    {
        free(answer);
        fprintf(stderr, "Invalid command\n");
        return 1;
    }
    printf("Your choice:\n%s\n", answer);
    if (answer[0] == '4')
    {
        free(path);
        free(answer);
        return 0;
    }
    if (getline(&path, &n, stdin) == -1)
        ERR("getline");
    path[strlen(path) - 1] = '\0';
    if (stat(path, &filestat))
    {
        free(path);
        free(answer);
        if (errno == ENOENT)
        {
            fprintf(stderr, "Invalid path\n");
            return 1;
        }
        else
            ERR("stat");
    }
    switch (answer[0])
    {
        case '1':
            show_stage2(path, &filestat);
            return 1;
        case '2':
            write_stage3(path, &filestat);
            return 1;
        case '3':
            walk_stage4(path);
            return 1;
        default:
            fprintf(stderr, "Invalid command\n");
            free(path);
            free(answer);
            return 1;
    }
    free(path);
    free(answer);
    return 1;
}

int main()
{
    while (interface_stage1())
        ;
    return EXIT_SUCCESS;
}
