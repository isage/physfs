/*
 * Posix-esque support routines for PhysicsFS.
 *
 * Please see the file LICENSE in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#if (defined __STRICT_ANSI__)
#define __PHYSFS_DOING_STRICT_ANSI__
#endif

/*
 * We cheat a little: I want the symlink version of stat() (lstat), and
 *  GCC/Linux will not declare it if compiled with the -ansi flag.
 *  If you are really lacking symlink support on your platform,
 *  you should #define __PHYSFS_NO_SYMLINKS__ before compiling this
 *  file. That will open a security hole, though, if you really DO have
 *  symlinks on your platform; it renders PHYSFS_permitSymbolicLinks(0)
 *  useless, since every symlink will be reported as a regular file/dir.
 */
#if (defined __PHYSFS_DOING_STRICT_ANSI__)
#undef __STRICT_ANSI__
#endif
#include <stdio.h>
#if (defined __PHYSFS_DOING_STRICT_ANSI__)
#define __STRICT_ANSI__
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#define __PHYSICSFS_INTERNAL__
#include "physfs_internal.h"


char *__PHYSFS_platformCopyEnvironmentVariable(const char *varname)
{
    const char *envr = getenv(varname);
    char *retval = NULL;

    if (envr != NULL)
    {
        retval = malloc(strlen(envr) + 1);
        if (retval != NULL)
            strcpy(retval, envr);
    } /* if */

    return(retval);
} /* __PHYSFS_platformCopyEnvironmentVariable */


static char *getUserNameByUID(void)
{
    uid_t uid = getuid();
    struct passwd *pw;
    char *retval = NULL;

    pw = getpwuid(uid);
    if ((pw != NULL) && (pw->pw_name != NULL))
    {
        retval = malloc(strlen(pw->pw_name) + 1);
        if (retval != NULL)
            strcpy(retval, pw->pw_name);
    } /* if */
    
    return(retval);
} /* getUserNameByUID */


static char *getUserDirByUID(void)
{
    uid_t uid = getuid();
    struct passwd *pw;
    char *retval = NULL;

    pw = getpwuid(uid);
    if ((pw != NULL) && (pw->pw_dir != NULL))
    {
        retval = malloc(strlen(pw->pw_dir) + 1);
        if (retval != NULL)
            strcpy(retval, pw->pw_dir);
    } /* if */
    
    return(retval);
} /* getUserDirByUID */


char *__PHYSFS_platformGetUserName(void)
{
    char *retval = getUserNameByUID();
    if (retval == NULL)
        retval = __PHYSFS_platformCopyEnvironmentVariable("USER");
    return(retval);
} /* __PHYSFS_platformGetUserName */


char *__PHYSFS_platformGetUserDir(void)
{
    char *retval = __PHYSFS_platformCopyEnvironmentVariable("HOME");
    if (retval == NULL)
        retval = getUserDirByUID();
    return(retval);
} /* __PHYSFS_platformGetUserDir */


/* -ansi and -pedantic flags prevent use of strcasecmp() on Linux. */
int __PHYSFS_platformStricmp(const char *x, const char *y)
{
    int ux, uy;

    do
    {
        ux = toupper((int) *x);
        uy = toupper((int) *y);
        if (ux > uy)
            return(1);
        else if (ux < uy)
            return(-1);
        x++;
        y++;
    } while ((ux) && (uy));

    return(0);
} /* __PHYSFS_platformStricmp */


int __PHYSFS_platformExists(const char *fname)
{
    struct stat statbuf;
    return(stat(fname, &statbuf) == 0);
} /* __PHYSFS_platformExists */


int __PHYSFS_platformIsSymLink(const char *fname)
{
#if (defined __PHYSFS_NO_SYMLINKS__)
    return(0);
#else

    struct stat statbuf;
    int retval = 0;

    if (lstat(fname, &statbuf) == 0)
    {
        if (S_ISLNK(statbuf.st_mode))
            retval = 1;
    } /* if */
    
    return(retval);

#endif
} /* __PHYSFS_platformIsSymlink */


int __PHYSFS_platformIsDirectory(const char *fname)
{
    struct stat statbuf;
    int retval = 0;

    if (stat(fname, &statbuf) == 0)
    {
        if (S_ISDIR(statbuf.st_mode))
            retval = 1;
    } /* if */
    
    return(retval);
} /* __PHYSFS_platformIsDirectory */


char *__PHYSFS_platformCvtToDependent(const char *prepend,
                                      const char *dirName,
                                      const char *append)
{
    int len = ((prepend) ? strlen(prepend) : 0) +
              ((append) ? strlen(append) : 0) +
              strlen(dirName) + 1;
    char *retval = malloc(len);

    BAIL_IF_MACRO(retval == NULL, ERR_OUT_OF_MEMORY, NULL);

    /* platform-independent notation is Unix-style already.  :)  */

    if (prepend)
        strcpy(retval, prepend);
    else
        retval[0] = '\0';

    strcat(retval, dirName);

    if (append)
        strcat(retval, append);

    return(retval);
} /* __PHYSFS_platformCvtToDependent */



LinkedStringList *__PHYSFS_platformEnumerateFiles(const char *dirname,
                                                  int omitSymLinks)
{
    LinkedStringList *retval = NULL;
    LinkedStringList *l = NULL;
    LinkedStringList *prev = NULL;
    DIR *dir;
    struct dirent *ent;
    int bufsize = 0;
    char *buf = NULL;
    int dlen = 0;

    if (omitSymLinks)
    {
        dlen = strlen(dirname);
        bufsize = dlen + 256;
        buf = malloc(bufsize);
        BAIL_IF_MACRO(buf == NULL, ERR_OUT_OF_MEMORY, NULL);
        strcpy(buf, dirname);
        if (buf[dlen - 1] != '/')
        {
            buf[dlen++] = '/';
            buf[dlen] = '\0';
        } /* if */
    } /* if */

    errno = 0;
    dir = opendir(dirname);
    if (dir == NULL)
    {
        if (buf != NULL)
            free(buf);
        BAIL_IF_MACRO(1, strerror(errno), NULL);
    } /* if */

    while (1)
    {
        ent = readdir(dir);
        if (ent == NULL)   /* we're done. */
            break;

        if (strcmp(ent->d_name, ".") == 0)
            continue;

        if (strcmp(ent->d_name, "..") == 0)
            continue;

        if (omitSymLinks)
        {
            char *p;
            int len = strlen(ent->d_name) + dlen + 1;
            if (len > bufsize)
            {
                p = realloc(buf, len);
                if (p == NULL)
                    continue;
                buf = p;
                bufsize = len;
            } /* if */

            strcpy(buf + dlen, ent->d_name);
            if (__PHYSFS_platformIsSymLink(buf))
                continue;
        } /* if */

        l = (LinkedStringList *) malloc(sizeof (LinkedStringList));
        if (l == NULL)
            break;

        l->str = (char *) malloc(strlen(ent->d_name) + 1);
        if (l->str == NULL)
        {
            free(l);
            break;
        } /* if */

        strcpy(l->str, ent->d_name);

        if (retval == NULL)
            retval = l;
        else
            prev->next = l;

        prev = l;
        l->next = NULL;
    } /* while */

    if (buf != NULL)
        free(buf);

    closedir(dir);
    return(retval);
} /* __PHYSFS_platformEnumerateFiles */


char *__PHYSFS_platformCurrentDir(void)
{
    int allocSize = 0;
    char *retval = NULL;
    char *ptr;

    do
    {
        allocSize += 100;
        ptr = (char *) realloc(retval, allocSize);
        if (ptr == NULL)
        {
            if (retval != NULL)
                free(retval);
            BAIL_MACRO(ERR_OUT_OF_MEMORY, NULL);
        } /* if */

        retval = ptr;
        ptr = getcwd(retval, allocSize);
    } while (ptr == NULL && errno == ERANGE);

    if (ptr == NULL && errno)
    {
            /*
             * getcwd() failed for some reason, for example current
             * directory not existing.
             */
        if (retval != NULL)
            free(retval);
        BAIL_MACRO(ERR_NO_SUCH_FILE, NULL);
    } /* if */

    return(retval);
} /* __PHYSFS_platformCurrentDir */



int __PHYSFS_platformMkDir(const char *path)
{
    int rc;
    errno = 0;
    rc = mkdir(path, S_IRWXU);
    BAIL_IF_MACRO(rc == -1, strerror(errno), 0);
    return(1);
} /* __PHYSFS_platformMkDir */


static void *doOpen(const char *filename, const char *mode)
{
    FILE *retval;
    errno = 0;

    retval = fopen(filename, mode);
    if (retval == NULL)
        __PHYSFS_setError(strerror(errno));

    return((void *) retval);
} /* doOpen */


void *__PHYSFS_platformOpenRead(const char *filename)
{
    return(doOpen(filename, "rb"));
} /* __PHYSFS_platformOpenRead */


void *__PHYSFS_platformOpenWrite(const char *filename)
{
    return(doOpen(filename, "wb"));
} /* __PHYSFS_platformOpenWrite */


void *__PHYSFS_platformOpenAppend(const char *filename)
{
    return(doOpen(filename, "ab"));
} /* __PHYSFS_platformOpenAppend */


PHYSFS_sint64 __PHYSFS_platformRead(void *opaque, void *buffer,
                                    PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    FILE *io = (FILE *) opaque;
    int rc = fread(buffer, size, count, io);
    if (rc < count)
    {
        int err = errno;
        BAIL_IF_MACRO(ferror(io), strerror(err), rc);
        BAIL_MACRO(ERR_PAST_EOF, rc);
    } /* if */

    return(rc);
} /* __PHYSFS_platformRead */


PHYSFS_sint64 __PHYSFS_platformWrite(void *opaque, const void *buffer,
                                     PHYSFS_uint32 size, PHYSFS_uint32 count)
{
    FILE *io = (FILE *) opaque;
    int rc = fwrite((void *) buffer, size, count, io);
    if (rc < count)
        __PHYSFS_setError(strerror(errno));

    return(rc);
} /* __PHYSFS_platformWrite */


int __PHYSFS_platformSeek(void *opaque, PHYSFS_uint64 pos)
{
    FILE *io = (FILE *) opaque;

    /* !!! FIXME: Use llseek where available. */
    errno = 0;
    BAIL_IF_MACRO(fseek(io, pos, SEEK_SET) != 0, strerror(errno), 0);

    return(1);
} /* __PHYSFS_platformSeek */


PHYSFS_sint64 __PHYSFS_platformTell(void *opaque)
{
    FILE *io = (FILE *) opaque;
    PHYSFS_sint64 retval = ftell(io);
    BAIL_IF_MACRO(retval == -1, strerror(errno), -1);
    return(retval);
} /* __PHYSFS_platformTell */


PHYSFS_sint64 __PHYSFS_platformFileLength(void *opaque)
{
    FILE *io = (FILE *) opaque;
    struct stat statbuf;
    errno = 0;
    BAIL_IF_MACRO(fstat(fileno(io), &statbuf) == -1, strerror(errno), -1);
    return((PHYSFS_sint64) statbuf.st_size);
} /* __PHYSFS_platformFileLength */


int __PHYSFS_platformEOF(void *opaque)
{
    return(feof((FILE *) opaque));
} /* __PHYSFS_platformEOF */


int __PHYSFS_platformFlush(void *opaque)
{
    errno = 0;
    BAIL_IF_MACRO(fflush((FILE *) opaque) == EOF, strerror(errno), 0);
    return(1);
} /* __PHYSFS_platformFlush */


int __PHYSFS_platformClose(void *opaque)
{
    errno = 0;
    BAIL_IF_MACRO(fclose((FILE *) opaque) == EOF, strerror(errno), 0);
    return(1);
} /* __PHYSFS_platformClose */


int __PHYSFS_platformDelete(const char *path)
{
    errno = 0;
    BAIL_IF_MACRO(remove(path) == -1, strerror(errno), 0);
    return(1);
} /* __PHYSFS_platformDelete */


PHYSFS_sint64 __PHYSFS_platformGetLastModTime(const char *fname)
{
    struct stat statbuf;
    BAIL_IF_MACRO(stat(fname, &statbuf) < 0, strerror(errno), -1);
    return statbuf.st_mtime;
} /* __PHYSFS_platformGetLastModTime */

/* end of posix.c ... */

