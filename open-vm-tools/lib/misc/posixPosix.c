/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

#define UNICODE_BUILDING_POSIX_WRAPPERS
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include "su.h"
#include <utime.h>
#include <sys/time.h>
#include <stdarg.h>

#if defined(__APPLE__)
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/kauth.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>
#include <pwd.h>
#else
#if defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#else
#if !defined(N_PLAT_NLM)
#include <sys/statfs.h>
#if !defined(sun)
#include <mntent.h>
#include <pwd.h>
#endif
#endif
#endif
#endif

# if defined(__FreeBSD__) && BSD_VERSION >= 53
#  include <syslimits.h>  // PATH_MAX
# else
#  include <limits.h>  // PATH_MAX
# endif

#include "vmware.h"
#include "str.h"
#include "posix.h"
#include "unicode.h"
#if !defined(N_PLAT_NLM)
#include "hashTable.h"
#include "vm_atomic.h"
#endif


#if !defined(N_PLAT_NLM) && !defined(__FreeBSD__) && !defined(sun)
static struct passwd *GetpwInternal(struct passwd *pw);
static int GetpwInternal_r(struct passwd *pw, char *buf, size_t size,
                           struct passwd **ppw);
#endif


/*
 *----------------------------------------------------------------------
 *
 * Posix_Open --
 *
 *      Open a file using POSIX open.
 *
 * Results:
 *      -1	error
 *      >= 0	success (file descriptor)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Open(ConstUnicode pathName,  // IN:
           int flags,              // IN:
           ...)                    // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   mode_t mode = 0;
   int fd;

   if ((flags & O_CREAT) != 0) {
      va_list a;

      /*
       * The FreeBSD tools compiler
       * (toolchain/lin32/gcc-4.1.2-5/bin/i686-freebsd5.0-gcc)
       * wants us to use va_arg(a, int) instead of va_arg(a, mode_t),
       * so oblige.  -- edward
       */

      va_start(a, flags);
      ASSERT_ON_COMPILE(sizeof (int) >= sizeof(mode_t));
      mode = va_arg(a, int);
      va_end(a);
   }

   fd = open(path, flags, mode);

   free(path);
   return fd;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Creat --
 *
 *      Create a file via POSIX creat()
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Creat(ConstUnicode pathName, // IN:
            mode_t mode)           // IN:
{
   return Posix_Open(pathName, O_CREAT | O_WRONLY | O_TRUNC, mode);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Fopen --
 *
 *      Open a file via POSIX fopen()
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Fopen(ConstUnicode pathName, // IN:
            const char *mode)      // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   FILE *stream;

   ASSERT(mode);

   stream = fopen(path, mode);

   free(path);
   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Stat --
 *
 *      POSIX stat()
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Stat(ConstUnicode pathName,  // IN:
           struct stat *statbuf)   // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = stat(path, statbuf);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Chmod --
 *
 *      POSIX chmod()
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Chmod(ConstUnicode pathName,  // IN:
            mode_t mode)            // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = chmod(path, mode);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Rename --
 *
 *      POSIX rename().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Rename(ConstUnicode fromPathName,  // IN:
             ConstUnicode toPathName)    // IN:
{
   char *toPath = Unicode_GetAllocBytes(toPathName, STRING_ENCODING_DEFAULT);
   char *fromPath = Unicode_GetAllocBytes(fromPathName,
					  STRING_ENCODING_DEFAULT);
   int result = rename(fromPath, toPath);

   free(toPath);
   free(fromPath);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Unlink --
 *
 *      POSIX unlink().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Unlink(ConstUnicode pathName)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = unlink(path);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Rmdir --
 *
 *      POSIX rmdir().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Rmdir(ConstUnicode pathName)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = rmdir(path);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Freopen --
 *
 *      Open a file via POSIX freopen()
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Freopen(ConstUnicode pathName, // IN:
              const char *mode,      // IN:
              FILE *input_stream)    // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   FILE *stream;

   ASSERT(mode);

   stream = freopen(path, mode, input_stream);

   free(path);
   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Access --
 *
 *      POSIX access().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Access(ConstUnicode pathName,  // IN:
             int mode)               // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = access(path, mode);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Utime --
 *
 *      POSIX utime().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Utime(ConstUnicode pathName,        // IN:
            const struct utimbuf *times)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = utime(path, times);

   free(path);
   return ret;
}


#if !defined(N_PLAT_NLM) // {
/*
 *----------------------------------------------------------------------
 *
 * Posix_Popen --
 *
 *      Open a file using POSIX popen().
 *
 * Results:
 *      -1	error
 *      >= 0	success (file descriptor)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Popen(ConstUnicode pathName,  // IN:
            const char *mode)       // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   FILE *stream;

   ASSERT(mode);

   stream = popen(path, mode);

   free(path);
   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Mknod --
 *
 *      POSIX mknod().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Mknod(ConstUnicode pathName,  // IN:
            mode_t mode,            // IN:
            dev_t dev)              // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = mknod(path, mode, dev);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Chown --
 *
 *      POSIX chown().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Chown(ConstUnicode pathName,  // IN:
            uid_t owner,            // IN:
            gid_t group)            // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = chown(path, owner, group);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Lchown --
 *
 *      POSIX lchown().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Lchown(ConstUnicode pathName,  // IN:
             uid_t owner,            // IN:
             gid_t group)            // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = lchown(path, owner, group);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Link --
 *
 *      POSIX link().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Link(ConstUnicode pathName1,  // IN:
           ConstUnicode pathName2)  // IN:
{
   char *path1 = Unicode_GetAllocBytes(pathName1, STRING_ENCODING_DEFAULT);
   char *path2 = Unicode_GetAllocBytes(pathName2, STRING_ENCODING_DEFAULT);
   int ret = link(path1, path2);

   free(path1);
   free(path2);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Symlink --
 *
 *      POSIX symlink().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Symlink(ConstUnicode pathName1,  // IN:
              ConstUnicode pathName2)  // IN:
{
   char *path1 = Unicode_GetAllocBytes(pathName1, STRING_ENCODING_DEFAULT);
   char *path2 = Unicode_GetAllocBytes(pathName2, STRING_ENCODING_DEFAULT);
   int ret = symlink(path1, path2);

   free(path1);
   free(path2);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Mkfifo --
 *
 *      POSIX mkfifo().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Mkfifo(ConstUnicode pathName,  // IN:
             mode_t mode)            // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = mkfifo(path, mode);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Truncate --
 *
 *      POSIX truncate().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Truncate(ConstUnicode pathName,  // IN:
               off_t length)           // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = truncate(path, length);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Utimes --
 *
 *      POSIX utimes().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Utimes(ConstUnicode pathName,        // IN:
             const struct timeval *times)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = utimes(path, times);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execl --
 *
 *      POSIX execl().
 *
 * Results:
 *      -1      Error
 *      0       Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execl(ConstUnicode pathName,   // IN:
            ConstUnicode arg0, ...)  // IN:
{
   int ret = -1;
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   va_list vl;
   char **argv = NULL;
   int i, count = 0;

   if (arg0) {
      count = 1;
      va_start(vl, arg0);
      while (va_arg(vl, char *)) {
         count ++;
      }   
      va_end(vl);
   }

   argv = (char **) malloc(sizeof(char *) * (count + 1));
   if (argv) {
      if (count > 0) {
         argv[0] = Unicode_GetAllocBytes(arg0, STRING_ENCODING_DEFAULT);
         va_start(vl, arg0);
         for (i = 1; i < count; i++) {
            argv[i] = Unicode_GetAllocBytes(va_arg(vl, char *),
                                            STRING_ENCODING_DEFAULT);
         }
         va_end(vl);
      }
      argv[count] = NULL;
   } else {
      errno = ENOMEM;
      goto exit;
   }

   ret = execv(path, argv);

exit:
   if (argv) {
      for (i = 0; i < count; i++) {
         free(argv[i]);
      }
      free(argv);
   }
   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execv --
 *
 *      POSIX execv().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execv(ConstUnicode pathName,        // IN:
            Unicode const argVal[])       // IN:
{
   int ret = -1;
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   char **argv = NULL;
   int i, count = 0;

   if (argVal) {
      while (argVal[count]) {
         count++;
      }
      argv = (char **) malloc(sizeof(char *) * (count + 1));
      if (argv) {
         for (i = 0; i < count; i++) {
            argv[i] = Unicode_GetAllocBytes(argVal[i],
                                            STRING_ENCODING_DEFAULT);
         }
         argv[count] = NULL;
      } else {
         errno = ENOMEM;
         goto exit;
      }
   }

   ret = execv(path, argv);

exit:
   if (argv) {
      for (i = 0; i < count; i++) {
         free(argv[i]);
      }
      free(argv);
   }
   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execvp --
 *
 *      POSIX execvp().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execvp(ConstUnicode fileName,        // IN:
             Unicode const argVal[])       // IN:
{
   int ret = -1;
   char *file = Unicode_GetAllocBytes(fileName, STRING_ENCODING_DEFAULT);
   char **argv = NULL;
   int i, count = 0;

   if (argVal) {
      while (argVal[count]) {
         count++;
      }
      argv = (char **) malloc(sizeof(char *) * (count + 1));
      if (argv) {
         for (i = 0; i < count; i++) {
            argv[i] = Unicode_GetAllocBytes(argVal[i],
                                            STRING_ENCODING_DEFAULT);
         }
         argv[count] = NULL;
      } else {
         errno = ENOMEM;
         goto exit;
      }
   }

   ret = execvp(file, argv);

exit:
   if (argv) {
      for (i = 0; i < count; i++) {
         free(argv[i]);
      }
      free(argv);
   }
   free(file);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Mkdir --
 *
 *      POSIX mkdir().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Mkdir(ConstUnicode pathName,  // IN:
            mode_t mode)            // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = mkdir(path, mode);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Chdir --
 *
 *      POSIX chdir().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Chdir(ConstUnicode pathName)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = chdir(path);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_RealPath --
 *
 *      POSIX realpath().
 *
 * Results:
 *      NULL	Error
 *      !NULL	Success (result must be freed by the caller)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

Unicode
Posix_RealPath(ConstUnicode pathName)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   char rpath[PATH_MAX];
   char *p = realpath(path, rpath);

   free(path);
   return p == NULL ? NULL : Unicode_Alloc(rpath, STRING_ENCODING_DEFAULT);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_ReadLink --
 *
 *      POSIX readlink().
 *
 * Results:
 *      NULL	Error
 *      !NULL	Success (result must be freed by the caller)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

Unicode
Posix_ReadLink(ConstUnicode pathName)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   ssize_t bytes;
   Unicode result = NULL;
   char link[PATH_MAX];

   bytes = readlink(path, link, sizeof link);
   ASSERT_NOT_IMPLEMENTED(bytes < (ssize_t) sizeof link);

   free(path);

   if (bytes != -1) {
      /* add the missing NUL character to path */
      link[bytes] = '\0';
      result = Unicode_Alloc(link, STRING_ENCODING_DEFAULT);
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Lstat --
 *
 *      POSIX lstat()
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Lstat(ConstUnicode pathName,  // IN:
            struct stat *statbuf)   // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = lstat(path, statbuf);

   free(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_OpenDir --
 *
 *      POSIX opendir()
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

DIR *
Posix_OpenDir(ConstUnicode pathName)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   DIR *ret = opendir(path);

   free(path);
   return ret;
}


#if !defined(sun) // {
/*
 *----------------------------------------------------------------------
 *
 * Posix_Getenv --
 *
 *      POSIX getenv().
 *
 * Results:
 *      NULL    The name was not found or an error occurred
 *      !NULL   The value associated with the name in UTF8. This does not
 *              need to be freed.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static void
PosixEnvFree(void *v)  // IN:
{
   Unicode_Free((Unicode) v);
}

Unicode
Posix_Getenv(ConstUnicode name)  // IN:
{
   char *rawData;
   char *rawName;
   Unicode newValue;
   HashTable *posixHashTable;

   static Atomic_Ptr posixEnvPtr; // Implicitly initialized to NULL. --mbellon

   posixHashTable = HashTable_AllocOnce(&posixEnvPtr, 128,
                                        HASH_FLAG_ATOMIC | HASH_STRING_KEY,
                                        PosixEnvFree);

   rawName = Unicode_GetAllocBytes(name, STRING_ENCODING_DEFAULT);
   rawData = getenv(rawName);
   free(rawName);

   if (rawData == NULL) {
     return NULL;
   }

   newValue = Unicode_Alloc(rawData, STRING_ENCODING_DEFAULT);

   if (newValue != NULL) {
      HashTable_ReplaceOrInsert(posixHashTable, name, newValue);
   }

   return newValue;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Setenv --
 *
 *      POSIX setenv().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      Environment may be changed.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Setenv(ConstUnicode name,   // IN:
             ConstUnicode value,  // IN:
             int overWrite)       // IN:
{
   int res;
   char *rawData;
   char *rawName;

   rawData = Unicode_GetAllocBytes(value, STRING_ENCODING_DEFAULT);
   rawName = Unicode_GetAllocBytes(name, STRING_ENCODING_DEFAULT);

   res = setenv(rawName, rawData, overWrite);

   free(rawData);
   free(rawName);

   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Statfs --
 *
 *      POSIX statfs()
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Statfs(ConstUnicode pathName,     // IN:
             struct statfs *statfsbuf)  // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   int ret = statfs(path, statfsbuf);

   free(path);
   return ret;
}


#if !defined(__FreeBSD__) // {

/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwnam --
 *
 *      POSIX getpwnam()
 *
 * Results:
 *      Pointer to updated passwd struct on NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct passwd *
Posix_Getpwnam(ConstUnicode name)  // IN:
{
   struct passwd *pw;
   char *tmpname;

   tmpname = Unicode_GetAllocBytes(name, STRING_ENCODING_DEFAULT);
   pw = getpwnam(tmpname);
   free(tmpname);

   return GetpwInternal(pw);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwuid --
 *
 *      POSIX getpwuid()
 *
 * Results:
 *      Pointer to updated passwd struct on NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct passwd *
Posix_Getpwuid(uid_t uid)  // IN:
{
   struct passwd *pw;
   
   pw = getpwuid(uid);
   return GetpwInternal(pw);
}


/*
 *----------------------------------------------------------------------
 *
 * GetpwInternal --
 *
 *      Helper function for Posix_Getpwnam and Posix_Getpwuid
 *
 * Results:
 *      Pointer to updated passwd struct on NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

static struct passwd *
GetpwInternal(struct passwd *pw)  // IN:
{
   int ret;
   static struct passwd spw = {0};

   if (!pw) {
      return NULL;
   }

   /* Free static structure string pointers before reuse. */
   free(spw.pw_passwd);
   spw.pw_passwd = NULL;
   free(spw.pw_dir);
   spw.pw_dir = NULL;
   free(spw.pw_name);
   spw.pw_name = NULL;
   free(spw.pw_gecos);
   spw.pw_gecos = NULL;
   free(spw.pw_shell);
   spw.pw_shell = NULL;

   /* Fill out structure with new values. */
   spw.pw_uid = pw->pw_uid;
   spw.pw_gid = pw->pw_gid;

   ret = ENOMEM;
   if (pw->pw_passwd &&
       (spw.pw_passwd = Unicode_Alloc(pw->pw_passwd, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_dir &&
       (spw.pw_dir = Unicode_Alloc(pw->pw_dir, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_name &&
       (spw.pw_name = Unicode_Alloc(pw->pw_name, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_gecos &&
       (spw.pw_gecos = Unicode_Alloc(pw->pw_gecos, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_shell &&
       (spw.pw_shell = Unicode_Alloc(pw->pw_shell, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   ret = 0;

exit:
   if (ret != 0) {
      errno = ret;
      return NULL;
   }
   return &spw;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwnam_r --
 *
 *      POSIX getpwnam_r()
 *
 * Results:
 *      Returns 0 with success and pointer to updated passwd struct
 *      or returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Getpwnam_r(ConstUnicode name,    // IN:
                 struct passwd *pw,    // IN:
                 char *buf,            // IN:
                 size_t size,          // IN:
                 struct passwd **ppw)  // OUT:
{
   int ret;
   char *tmpname;

   tmpname = Unicode_GetAllocBytes(name, STRING_ENCODING_DEFAULT);
   ret = getpwnam_r(tmpname, pw, buf, size, ppw);
   free(tmpname);

   if (ret != 0) {
      return ret;
   }

   return GetpwInternal_r(pw, buf, size, ppw);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getpwuid_r --
 *
 *      POSIX getpwuid_r()
 *
 * Results:
 *      Returns 0 with success and pointer to updated passwd struct
 *      or returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Getpwuid_r(uid_t uid,            // IN:
                 struct passwd *pw,    // IN:
                 char *buf,            // IN:
                 size_t size,          // IN:
                 struct passwd **ppw)  // OUT:
{
   int ret;

   ret = getpwuid_r(uid, pw, buf, size, ppw);
   if (ret != 0) {
      return ret;
   }

   return GetpwInternal_r(pw, buf, size, ppw);
}


/*
 *----------------------------------------------------------------------
 *
 * GetpwInternal_r --
 *
 *      Helper function for Posix_Getpwnam_r and Posix_Getpwuid_r
 *
 * Results:
 *      Returns 0 with success and pointer to updated passwd struct
 *      or returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
GetpwInternal_r(struct passwd *pw,    // IN:
                char *buf,            // IN:
                size_t size,          // IN:
                struct passwd **ppw)  // OUT:
{
   int ret;
   char *pwname = NULL;
   char *passwd = NULL;
   char *gecos = NULL;
   char *dir = NULL;
   char *shell = NULL;
   char *p;

   /*
    * Maybe getpwnam_r didn't use supplied struct, but we don't care.
    * We just fix up the one it gives us.
    */

   pw = *ppw;

   /*
    * Convert strings to UTF-8
    */

   ret = ENOMEM;
   if (pw->pw_name &&
       (pwname = Unicode_Alloc(pw->pw_name, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_passwd &&
       (passwd = Unicode_Alloc(pw->pw_passwd, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_gecos &&
       (gecos = Unicode_Alloc(pw->pw_gecos, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_dir &&
       (dir = Unicode_Alloc(pw->pw_dir, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (pw->pw_shell &&
       (shell = Unicode_Alloc(pw->pw_shell, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }

   /*
    * Put UTF-8 strings into the structure.
    */

   ret = ERANGE;
   p = buf;

   if (pwname) {
      size_t len = strlen(pwname) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      pw->pw_name = memcpy(p, pwname, len);
      p += len;
   }

   if (passwd != NULL) {
      size_t len = strlen(passwd) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      pw->pw_passwd = memcpy(p, passwd, len);
      p += len;
   }

   if (gecos) {
      size_t len = strlen(gecos) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      pw->pw_gecos = memcpy(p, gecos, len);
      p += len;
   }

   if (dir) {
      size_t len = strlen(dir) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      pw->pw_dir = memcpy(p, dir, len);
      p += len;
   }

   if (shell) {
      size_t len = strlen(shell) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      pw->pw_shell = memcpy(p, shell, len);
      p += len;
   }
   ret = 0;

exit:
   free(passwd);
   free(dir);
   free(pwname);
   free(gecos);
   free(shell);
   return ret;
}


#if !defined(__APPLE__) // {

/*
 *----------------------------------------------------------------------
 *
 * Posix_Setmntent --
 *
 *      Open a file via POSIX setmntent()
 *
 * Results:
 *      NULL	Error
 *      !NULL	File stream
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Setmntent(ConstUnicode pathName, // IN:
                const char *mode)      // IN:
{
   char *path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   FILE *stream;

   ASSERT(mode != NULL);

   stream = setmntent(path, mode);

   free(path);
   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getmntent --
 *
 *      POSIX getmntent()
 *
 * Results:
 *      Pointer to updated mntent struct or NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct mntent *
Posix_Getmntent(FILE *fp)  // IN:
{
   int ret;
   struct mntent *m;
   static struct mntent sm = {0};

   m = getmntent(fp);
   if (!m) {
      return NULL;
   }

   /* Free static structure string pointers before reuse. */
   free(sm.mnt_fsname);
   sm.mnt_fsname = NULL;
   free(sm.mnt_dir);
   sm.mnt_dir = NULL;
   free(sm.mnt_type);
   sm.mnt_type = NULL;
   free(sm.mnt_opts);
   sm.mnt_opts = NULL;

   /* Fill out structure with new values. */
   sm.mnt_freq = m->mnt_freq;
   sm.mnt_passno = m->mnt_passno;

   ret = ENOMEM;
   if (m->mnt_fsname &&
       (sm.mnt_fsname = Unicode_Alloc(m->mnt_fsname, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_dir &&
       (sm.mnt_dir = Unicode_Alloc(m->mnt_dir, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_type &&
       (sm.mnt_type = Unicode_Alloc(m->mnt_type, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_opts &&
       (sm.mnt_opts = Unicode_Alloc(m->mnt_opts, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   ret = 0;

exit:
   if (ret != 0) {
      errno = ret;
      return NULL;
   }
   return &sm;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getmntent_r --
 *
 *      POSIX getmntent_r()
 *
 * Results:
 *      Pointer to updated mntent struct or NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct mntent *
Posix_Getmntent_r(FILE *fp,          // IN:
                  struct mntent *m,  // IN:
                  char *buf,         // IN:
                  int size)          // IN:
{
   int ret;
   char *fsname = NULL;
   char *dir = NULL;
   char *type = NULL;
   char *opts = NULL;
   char *p;

   if (!getmntent_r(fp, m, buf, size)) {
      return NULL;
   }

   /*
    * Convert strings to UTF-8
    */

   ret = ENOMEM;
   if (m->mnt_fsname &&
       (fsname = Unicode_Alloc(m->mnt_fsname, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_dir &&
       (dir = Unicode_Alloc(m->mnt_dir, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_type &&
       (type = Unicode_Alloc(m->mnt_type, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_opts &&
       (opts = Unicode_Alloc(m->mnt_opts, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }

   /*
    * Put UTF-8 strings into the structure.
    */

   ret = ERANGE;
   p = buf;

   if (fsname) {
      int len = strlen(fsname) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      m->mnt_fsname = memcpy(p, fsname, len);
      p += len;
   }

   if (dir != NULL) {
      int len = strlen(dir) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      m->mnt_dir = memcpy(p, dir, len);
      p += len;
   }

   if (type) {
      int len = strlen(type) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      m->mnt_type = memcpy(p, type, len);
      p += len;
   }

   if (opts) {
      size_t len = strlen(opts) + 1;
      if (p + len > buf + size) {
         goto exit;
      }
      m->mnt_opts = memcpy(p, opts, len);
      p += len;
   }
   ret = 0;

exit:

   free(fsname);
   free(dir);
   free(type);
   free(opts);

   if (ret != 0) {
      errno = ret;
      return NULL;
   }
   return m;
}

#endif // } !defined(__APPLE__)
#endif // } !defined(__FreeBSD__)
#endif // } !defined(sun)
#endif // } !defined(N_PLAT_NLM)
