/*
Small program to blockwise compare two files and write different
blocks from file1 to file2.

Arguments: file1, file2, blocksize in bytes
If blocksize is not given, it is set to 512 (minimum)

No error checking, no intensive tests run - use at your own risk!

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>

static int readfile(int fd, void *buf, int bufsize)
{
        int read_off = 0;
        int ret;

        do {
                ret = read(fd, buf + read_off, bufsize - read_off);
                if (ret <= 0)
                        break;
                read_off += ret;
        } while (read_off < bufsize);

        return read_off;
}

static void print_help(char *argv[])
{
        fprintf(stderr, "Usage: %s -o outfile [-h] [-b blocksize]\n\n", argv[0]);
        fprintf(stderr, "\tReads from stdin and compares data within outfile.\n");
        fprintf(stderr, "\tWrites modified blocks to outfile.\n\n");
        fprintf(stderr, "\t-h       : Prints help message\n");
        fprintf(stderr, "\t--help   : Prints help message\n");
        fprintf(stderr, "\t-o       : The file to write to\n");
        fprintf(stderr, "\t--out    : The file to write to\n");
        fprintf(stderr, "\t-b       : Block size when operating on files (min 512)\n");
        fprintf(stderr, "\t--block  : Block size when operating on files (min 512)\n");

        exit(1);
}

static int get_proc_cpu_count(void)
{
        int ret = -1;
        int fd = 0;
        off_t filesize = 0;
        char *buf = NULL;
        char *l = NULL;

        fd = open("/proc/cpuinfo", O_RDONLY);
        if (fd < 0)
                goto out;
        buf = malloc(1024);
        if (!buf)
                goto out;
        for (;;) {
                int i = read(fd, &buf[filesize], 1024);
                if (i <= 0)
                        break;
                filesize += i;
                buf = realloc(buf, filesize + 1024);
                if (!buf)
                        goto out;
        };
        l = buf;
        ret = 0;
        for (;;) {
                l = strstr(l, "processor");
                if (!l)
                        break;
                l+=9;
                ret ++;
        }
out:
        if (fd > 0)
                close(fd);
        if (buf)
                free (buf);
        return ret;
}

static int get_proc_cache_size(void)
{
        int ret = -1;
        int fd = 0;
        off_t filesize = 0;
        char *buf = NULL;
        char *l = NULL;
        char unit[64];

        fd = open("/proc/cpuinfo", O_RDONLY);
        if (fd < 0)
                goto out;
        buf = malloc(1024);
        if (!buf)
                goto out;
        for (;;) {
                int i = read(fd, &buf[filesize], 1024);
                if (i <= 0)
                        break;
                filesize += i;
                buf = realloc(buf, filesize + 1024);
                if (!buf)
                        goto out;
        };
        l = strstr(buf, "cache size");
        if (!l)
                goto out;
        l = strstr(l, ":");
        if (!l)
                goto out;
        l+=2;        /* ':', ' ' */

        sscanf (l, "%d %s\n", &ret, &unit);

        if (unit != NULL && strcmp(unit, "KB") == 0) {
                ret *= 1024;
        } else if (unit != NULL && strcmp(unit, "MB") == 0) {
                ret *= 1024 * 1024;
        }
out:
        if (fd > 0)
                close(fd);
        if (buf)
                free (buf);
        return ret;
}

int main(int argc, char *argv[])
{

        char *fnameout;                 /* Output file name */
        char *bufin;                    /* Input buffer */
        char *bufout;                   /* Output buffer */
        int bufsize;                    /* Buffer size (blocksize) */
        int fdin;                       /* Input file descriptor*/
        int fdout;                      /* Output file descriptor*/
        int cnt;                        /* Current block # */
        int dirty;                      /* Dirty blocks # */
        int opt;                        /* getopt return value */
        int option_index;               /* getopt return value */

        int bufsize_read;               /* Read ofsset in block */
        int ret;

        /* Argument processing */
        bufsize = 0x10000;
        /* Get L3 cache per CPU core and divide by two to find optimal buffer size */
        if (get_proc_cache_size() > 4096) {
                int cpus = get_proc_cpu_count();
                if (cpus <= 0)
                        cpus = 1;

                bufsize = get_proc_cache_size() / (cpus * 2);
                bufsize = ((bufsize + 0x10000 - 1) / 0x10000) * 0x10000;
        }
        fnameout = NULL;

        static struct option long_options[] = {
          /* These options set a flag. */
          {"help", no_argument,       0, 'h'},
          {"out",  required_argument, 0, 'o'},
          {"block",  required_argument, 0, 'b'},
          {0, 0, 0, 0}
        };

        while((opt = getopt_long(argc, argv, "o:b:h", long_options, &option_index)) != -1)
        {
                switch(opt)
                {
                        case 'o':
                                fnameout = strdup(optarg);
                                break;
                        case 'b':
                                bufsize = strtoul(optarg, NULL, 0);
                                break;
                        case '?':
                        case 'h':
                                print_help(argv);
                                break;
                        default:
                                break;
                }
        }

        if (bufsize < 512) {
                fprintf(stderr,"Error: Illegal value for [bufsize]\n");
                exit(1);
        }
        if (!fnameout) {
                print_help(argv);
        }

        if (posix_memalign((void **)&bufin, 4 * 1024 * 1024, bufsize)) {
                fprintf(stderr,"Error: Can't allocate buffers: %i\n", bufsize);
                exit(1);
        }
        if (posix_memalign((void **)&bufout, 4 * 1024 * 1024, bufsize)) {
                fprintf(stderr,"Error: Can't allocate buffers: %i\n", bufsize);
                exit(1);
        }

        fdin = dup(STDIN_FILENO);
        if (fdin < 0) {
                fprintf(stderr,"Error: Can't open stdin\n");
                free(bufin);
                free(bufout);
                exit(1);
        }

        fdout = open(fnameout, O_RDWR | O_SYNC);
        if (fdout < 0) {
                fprintf(stderr,"Error: Can't open ouput file: %s\n", fnameout);
                free(fnameout);
                free(bufin);
                free(bufout);
                exit(1);
        }

        cnt = 0;
        dirty = 0;
        while (1) {
                ret = readfile(fdin, bufin, bufsize);
                if (ret <= 0)
                        break;

                bufsize_read = ret;
                ret = readfile(fdout, bufout, bufsize_read);
                if (ret <= 0)
                        break;
                if (ret != bufsize_read) {
                        fprintf(stderr, "Failed to read from output file %s\n", fnameout);
                }

                if (memcmp(bufin, bufout, ret) != 0) {
                        if (lseek(fdout, -bufsize_read, SEEK_CUR) > -1) {
                                if (write(fdout, bufin, bufsize_read) != bufsize_read) {
                                        fprintf(stderr,"Error: Unable to write to output file %s block # %i\n", fnameout, cnt);
                                        free(fnameout);
                                        free(bufin);
                                        free(bufout);
                                        exit(1);
                                }
                        } else {
                                fprintf(stderr,"Error: Unable to seek to output file %s block # %i\n", fnameout, cnt);
                                free(fnameout);
                                free(bufin);
                                free(bufout);
                                exit(1);
                        }
                        dirty++;
                }
                cnt++;
        }
        printf("Scanned %d blocks with size 0x%x, dirty %d\n", cnt, bufsize, dirty);
        free(fnameout);
        free(bufin);
        free(bufout);
        exit(0);
}
