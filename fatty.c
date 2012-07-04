/* * For simplicity, we do not write the backup FAT.
 * * Note that a FAT can contain more clusters than the physical device,
 *   thus we must use the logical sectors per FAT.
 */

// we are going to work with sdcards larger than 4G
#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUG 1
#define Log(...) do { if (DEBUG) fprintf(stderr, __VA_ARGS__); } while (0)

int       device_fd;

uint16_t  bytes_per_sector;
uint8_t   sectors_per_cluster;
uint32_t  bytes_per_cluster; // bytes_per_sector * sectors_per_cluster
uint32_t  max_dir_entries;
uint32_t  max_fat_entries;
uint16_t  reserved_sector_num;
uint32_t  total_logical_sectors;
uint8_t   fat_num;
uint16_t  signature;

uint32_t  partition_begin_sector;
uint32_t  fat_begin_sector;
uint32_t  cluster_begin_sector;
uint32_t  root_begin_sector;

uint16_t *fat16_buffer;
uint32_t *fat32_buffer;
uint32_t  fat_buffer_size;

uint32_t sector_of_cluster(uint32_t cluster_number)
{
    return cluster_begin_sector +
           (cluster_number - 2) * sectors_per_cluster;
}

ssize_t seek_read(off_t offset, void *ptr, size_t count)
{
    ssize_t s;
    lseek(device_fd, offset, SEEK_SET);
    s = read(device_fd, ptr, count);
    return s;
}

void seek_write(off_t offset, void *ptr, size_t count)
{
    lseek(device_fd, offset, SEEK_SET);
    write(device_fd, ptr, count);
}

void increase_filename(char *filename)
{
    int i;
    for (i = 7; i >= 0; i--)
    {
        if (filename[i] == '9')
            filename[i] = '0';
        else
        {
            filename[i]++;
            break;
        }
    }
}

// filesize == 0 for directory
void entry_write_16(off_t     offset,
                    char     *name,
                    uint16_t  cluster,
                    uint32_t  size)
{
    uint8_t entry[32] = {
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // filename
        0x20, 0x20, 0x20,      // extention
        0x10,                  // 00010000, bit is set for directory
         0x0,                  // reserved for NT
         0x0,                  // creation ms
         0x0,  0x0,            // creation time
         0x0,  0x0,            // creation date
         0x0,  0x0,            // last access date
         0x0,  0x0,            // reserved for 32
         0x0,  0x0,            // last write time
         0x0,  0x0,            // last write date
         0x0,  0x0,            // starting cluster
         0x0,  0x0,  0x0,  0x0 // filesize
    };
    size_t len = strlen(name);
    assert(len >= 1 && len <= 8);

    memcpy(entry, name, len);
    memcpy(entry + 0x1A, &cluster, sizeof cluster);
    if (size > 0)
    {
        entry[11] = 0x0; // not directory
        memcpy(entry + 0x08, "DAT", 3);
        memcpy(entry + 0x1C, &size, sizeof size);
    }

    seek_write(offset, entry, sizeof entry);
}

void entry_write_32(off_t     offset,
                    char     *name,
                    uint32_t  cluster,
                    uint32_t  size)
{
    uint8_t entry[32] = {
        0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, // filename
        0x20, 0x20, 0x20,      // extention
        0x10,                  // 00010000, bit is set for directory
         0x0,                  // reserved for NT
         0x0,                  // creation ms
         0x0,  0x0,            // creation time
         0x0,  0x0,            // creation date
         0x0,  0x0,            // last access date
         0x0,  0x0,            // reserved for 32
         0x0,  0x0,            // last write time
         0x0,  0x0,            // last write date
         0x0,  0x0,            // starting cluster
         0x0,  0x0,  0x0,  0x0 // filesize
    };
    size_t len = strlen(name);
    assert(len >= 1 && len <= 8);

    memcpy(entry, name, len);
    memcpy(entry + 0x1A, &cluster, sizeof(uint16_t));
    memcpy(entry + 0x14, (uint16_t *) &cluster + 1, sizeof(uint16_t));
    if (size > 0)
    {
        entry[11] = 0x0; // not directory
        memcpy(entry + 0x08, "DAT", 3);
        memcpy(entry + 0x1C, &size, sizeof size);
    }

    seek_write(offset, entry, sizeof entry);
}

int fat_find_free_cluster_16(off_t    start_offset,
                             uint16_t sectors_per_fat)
{
    off_t     offset;
    uint16_t  i, acc;
    uint16_t *buffer;

    assert(start_offset >= fat_begin_sector*bytes_per_sector);
    assert((start_offset - fat_begin_sector*bytes_per_sector) % 2 == 0);

    if (start_offset + fat_buffer_size >
       (fat_begin_sector * bytes_per_sector + (max_fat_entries + 2) * 2))
    {
        Log("Warning (harmless): no free clusters available\n");
        return 0;
    }

    buffer = malloc(fat_buffer_size);
    offset = start_offset;
    acc    = (start_offset - fat_begin_sector * bytes_per_sector) / 2;

find:
    seek_read(offset, buffer, fat_buffer_size);
    for (i = 0; i < fat_buffer_size / 2; i++)
    {
        if (buffer[i] == 0) // a free cluster!
        {
            // occupy this cluster
            buffer[i] = 0xFFFF;
            seek_write(offset, buffer, fat_buffer_size);

            acc += i; // done
            break;
        }
    }

    if (i == fat_buffer_size / 2)
    {
        acc    += i;
        offset += fat_buffer_size;

        if (offset + fat_buffer_size >
           (fat_begin_sector * bytes_per_sector +
           (max_fat_entries + 2) * 2))
        {
            Log("Warning (harmless): no free clusters available\n");
            return 0;
        }

        goto find;
    }

    free(buffer);
    return acc;
}

int fat_find_free_cluster_32(off_t    start_offset,
                             uint32_t sectors_per_fat)
{
    off_t     offset;
    uint32_t  i, acc;
    uint32_t *buffer;

    assert(start_offset >= fat_begin_sector*bytes_per_sector);
    assert((start_offset - fat_begin_sector*bytes_per_sector) % 4 == 0);

    if (start_offset + fat_buffer_size >
       (fat_begin_sector * bytes_per_sector + (max_fat_entries + 2) * 4))
    {
        Log("Warning (harmless): no free clusters available\n");
        return 0;
    }

    buffer = malloc(fat_buffer_size);
    offset = start_offset;
    acc    = (start_offset - fat_begin_sector * bytes_per_sector) / 4;

find:
    seek_read(offset, buffer, fat_buffer_size);
    for (i = 0; i < fat_buffer_size / 4; i++)
    {
        if ((buffer[i] & 0xFFFFFFF) == 0) // a free cluster!
        {
            // occupy this cluster
            buffer[i] = 0xFFFFFFFF;
            seek_write(offset, buffer, fat_buffer_size);

            acc += i; // done
            break;
        }
    }

    if (i == fat_buffer_size / 4)
    {
        acc    += i;
        offset += fat_buffer_size;

        if (offset + fat_buffer_size >
           (fat_begin_sector * bytes_per_sector +
           (max_fat_entries + 2) * 4))
        {
            Log("Warning (harmless): no free clusters available\n");
            return 0;
        }

        goto find;
    }

    free(buffer);
    return acc;
}

// return 1 if "IPCAM" directory exists
int check_ipcam_dir_16(uint16_t  max_root_entries,
                       uint16_t  sectors_per_fat,
                       uint16_t *ipcam_begin_cluster)
{
    uint16_t i;
    off_t    offset;
    uint8_t  dir_entry[32];
    char     filename[9];

    offset = (uint64_t) root_begin_sector * bytes_per_sector;
    for (i = 0; i < max_root_entries; i++)
    {
        seek_read(offset, dir_entry, sizeof dir_entry);
        if (dir_entry[0] == 0) // stop the search
            break;
        else if (dir_entry[0] != 0xE5 && dir_entry[0] != 0x05)
        {
            memcpy(filename, dir_entry, 8);
            filename[8] = '\0';
            seek_read(offset + 0x1A, ipcam_begin_cluster,
                      sizeof(uint16_t));

            if (!strcasecmp(filename, "IPCAM   ") &&
                *ipcam_begin_cluster >= 2)
            {
                Log("\n==> IPCAM exists\n");
                return 1;
            }
        }
        offset += 32;
    }
    Log("==> IPCAM doesn't exist\n\n");

    return 0;
}

int check_ipcam_dir_32(uint32_t  sectors_per_fat,
                       uint32_t  root_begin_cluster,
                       uint32_t *ipcam_begin_cluster)
{
    uint32_t i;
    off_t    offset;
    uint8_t  dir_entry[32];
    char     filename[9];
    uint32_t next_cluster = root_begin_cluster;

check:
    offset = (uint64_t) sector_of_cluster(next_cluster) *
                        bytes_per_sector;
    for (i = 0; i < max_dir_entries; i++)
    {
        seek_read(offset, dir_entry, sizeof dir_entry);
        if (dir_entry[0] == 0) // stop the search
            break;
        else if (dir_entry[0] != 0xE5 && dir_entry[0] != 0x05)
        {
            memcpy(filename, dir_entry, 8);
            filename[8] = '\0';
            seek_read(offset + 0x1A, ipcam_begin_cluster,
                      sizeof(uint16_t));
            seek_read(offset + 0x14, // high two bytes
                     ((uint16_t *)ipcam_begin_cluster + 1),
                     sizeof(uint16_t));

            if (!strcasecmp(filename, "IPCAM   ") &&
                *ipcam_begin_cluster >= 2)
            {
                Log("\n==> IPCAM exists\n");
                return 1;
            }
        }
        offset += 32;
    }

    // if the root directory is quite large
    if (i == max_dir_entries)
    {
        // look up FAT for the next cluster of the directory
        offset = (uint64_t) fat_begin_sector * bytes_per_sector +
                            next_cluster * 4;
        seek_read(offset, &next_cluster, sizeof next_cluster);
        goto check;
    }
    Log("==> IPCAM doesn't exist\n\n");

    return 0;
}

void read_ipcam_dir_16(char *max_filename, uint16_t ipcam_begin_cluster)
{
    uint16_t i;
    ssize_t  s;
    off_t    offset;
    uint8_t  dir_entry[32];
    char     filename[9];
    uint16_t cluster;
    uint32_t file_size;
    uint16_t next_cluster = ipcam_begin_cluster;

    Log("\nA list of IPCAM:\n\n");

read:
    offset = (uint64_t) sector_of_cluster(next_cluster) *
                        bytes_per_sector;
    for (i = 0; i < max_dir_entries; i++)
    {
        s = seek_read(offset, dir_entry, sizeof dir_entry);
        if (s < sizeof dir_entry)
            break;
        else if (dir_entry[0] == 0) // stop the search
            break;
        else if (dir_entry[0] != 0xE5 && dir_entry[0] != 0x05)
        {
            memcpy(filename, dir_entry, 8);
            filename[8] = '\0';
            seek_read(offset + 0x1A, &cluster, sizeof cluster);
            seek_read(offset + 0x1C, &file_size, sizeof file_size);

            // yes, strcmp can do this!
            if (strcmp(filename, max_filename) > 0)
                strcpy(max_filename, filename);

            Log("%s:\n", filename);
            Log("\tstarting_cluster: %i, ", cluster);
            Log("filesize: %i\n", file_size);
        }
        offset += 32;
    }

    // if the ipcam directory is quite large
    if (i == max_dir_entries)
    {
        // look up FAT for the next cluster of the directory
        offset = (uint64_t) fat_begin_sector * bytes_per_sector +
                            next_cluster * 2;
        seek_read(offset, &next_cluster, sizeof next_cluster);
        goto read;
    }

    // the filename to start with
    increase_filename(max_filename);
    Log("\n");
}

void read_ipcam_dir_32(char *max_filename, uint32_t ipcam_begin_cluster)
{
    uint32_t i;
    ssize_t  s;
    off_t    offset;
    uint8_t  dir_entry[32];
    char     filename[9];
    uint32_t cluster;
    uint32_t file_size;
    uint32_t next_cluster = ipcam_begin_cluster;

    Log("\nA list of IPCAM:\n\n");

read:
    offset = (uint64_t) sector_of_cluster(next_cluster) *
                        bytes_per_sector;
    for (i = 0; i < max_dir_entries; i++)
    {
        s = seek_read(offset, dir_entry, sizeof dir_entry);
        if (s < sizeof dir_entry)
            break;
        else if (dir_entry[0] == 0) // stop the search
            break;
        else if (dir_entry[0] != 0xE5 && dir_entry[0] != 0x05)
        {
            memcpy(filename, dir_entry, 8);
            filename[8] = '\0';
            seek_read(offset + 0x1A, &cluster, sizeof(uint16_t));
            seek_read(offset + 0x14, // high two bytes
                     ((uint16_t *)&cluster + 1), sizeof(uint16_t));
            seek_read(offset + 0x1C, &file_size, sizeof file_size);

            // yes, strcmp can do this!
            if (strcmp(filename, max_filename) > 0)
                strcpy(max_filename, filename);

            Log("%s:\n", filename);
            Log("\tstarting_cluster: %i, ", cluster);
            Log("filesize: %i\n", file_size);
        }
        offset += 32;
    }

    // if the ipcam directory is quite large
    if (i == max_dir_entries)
    {
        // look up FAT for the next cluster of the directory
        offset = (uint64_t) fat_begin_sector * bytes_per_sector +
                            next_cluster * 4;
        seek_read(offset, &next_cluster, sizeof next_cluster);
        goto read;
    }

    // the filename to start with
    increase_filename(max_filename);
    Log("\n");
}

// return the "IPCAM" directory begin cluster
int write_ipcam_dir_16(uint16_t max_root_entries,
                       uint16_t sectors_per_fat)
{
    uint16_t  i;
    off_t     offset;
    uint8_t   dir_entry[32];
    uint8_t  *cluster_zero;
    uint16_t  cluster_free;

    cluster_zero = calloc(bytes_per_cluster, 1);

    // find the free cluster to place the "IPCAM" directory
    cluster_free = fat_find_free_cluster_16(
                   fat_begin_sector * bytes_per_sector, sectors_per_fat);
    if (cluster_free == 0)
        goto end;

    // write the "IPCAM" in root directory
    offset = (uint64_t) root_begin_sector * bytes_per_sector;
    for (i = 0; i < max_root_entries; i++)
    {
        seek_read(offset, dir_entry, sizeof dir_entry);
        if (dir_entry[0] == 0x0 || dir_entry[0] == 0xE5 ||
            dir_entry[0] == 0x05)
        {
            entry_write_16(offset, "IPCAM", cluster_free, 0);
            break;
        }
        offset += 32;
    }

    if (i == max_root_entries)
    {
        cluster_free = 0;
        goto end;
    }

    // sub-directories are created by allocating a cluster which
    // then are cleared so it doesn't contain any directory entries
    offset = (uint64_t) sector_of_cluster(cluster_free) *
                        bytes_per_sector;
    seek_write(offset, cluster_zero, bytes_per_cluster);

    // write the "." ".." special entry in the "IPCAM" directory
    entry_write_16(offset, ".", cluster_free, 0);
    entry_write_16(offset + 32, "..", 0, 0);

end:
    free(cluster_zero);

    // return "IPCAM" directory entry cluster
    return cluster_free;
}

int write_ipcam_dir_32(uint32_t sectors_per_fat,
                       uint32_t root_begin_cluster)
{
    uint32_t  i;
    off_t     offset;
    uint8_t   dir_entry[32];
    uint8_t  *cluster_zero;
    uint32_t  cluster_free;
    uint32_t  next_cluster = root_begin_cluster;

    cluster_zero = calloc(bytes_per_cluster, 1);

    // free cluster for "IPCAM"
    cluster_free = fat_find_free_cluster_32(
                   fat_begin_sector * bytes_per_sector, sectors_per_fat);
    if (cluster_free == 0)
        goto end;

write:
    // write the "IPCAM" in root directory
    offset = (uint64_t) sector_of_cluster(next_cluster) *
                        bytes_per_sector;
    for (i = 0; i < max_dir_entries; i++)
    {
        seek_read(offset, dir_entry, sizeof dir_entry);
        if (dir_entry[0] == 0x0 || dir_entry[0] == 0xE5 ||
            dir_entry[0] == 0x05)
        {
            entry_write_32(offset, "IPCAM", cluster_free, 0);
            break;
        }
        offset += 32;
    }

    // if the root directory is full
    if (i == max_dir_entries)
    {
        offset = (uint64_t) fat_begin_sector * bytes_per_sector +
                            next_cluster * 4;
        seek_read(offset, &next_cluster, sizeof next_cluster);
        if (next_cluster == 0xFFFFFFFF) // must expand root directory
        {
            next_cluster = fat_find_free_cluster_32(
                           fat_begin_sector * bytes_per_sector,
                           sectors_per_fat); // 0XFF
            if (next_cluster == 0)
                goto end;

            seek_write(offset, &next_cluster, sizeof next_cluster);
            // clear the new cluster
            offset = (uint64_t) sector_of_cluster(next_cluster) *
                                bytes_per_sector;
            seek_write(offset, cluster_zero, bytes_per_cluster);
        }

        // write the "IPCAM" directory entry
        goto write;
    }

    // sub-directories are created by allocating a cluster which
    // then are cleared so it doesn't contain any directory entries
    offset = (uint64_t) sector_of_cluster(cluster_free) *
                        bytes_per_sector;
    seek_write(offset, cluster_zero, bytes_per_cluster);

    // write the "." ".." special entry in the "IPCAM" directory
    entry_write_32(offset, ".", cluster_free, 0);
    entry_write_32(offset + 32, "..", 0, 0);

end:
    free(cluster_zero);

    // return "IPCAM" directory entry cluster
    return cluster_free;
}

void write_ipcam_file_16(uint16_t  sectors_per_fat,
                         uint16_t  ipcam_begin_cluster,
                         char     *filename,
                         uint32_t  file_size,
                         uint16_t  file_clusters)
{
    uint16_t  i, j;
    uint16_t  acc_clusters     = 0;
    uint16_t  start_cluster    = 0;
    uint16_t  written_cluster  = 0;
    uint16_t  prev_cluster     = 0;
    uint16_t  next_cluster     = ipcam_begin_cluster;
    int       buffer_refreshed = 0; // bool
    uint8_t   dir_entry[32];
    uint8_t  *cluster_zero;
    off_t     offset;
    off_t     offset_entry;
    off_t     offset_util;

    cluster_zero = calloc(bytes_per_cluster, 1);

    // cast to int32_t since it might be negative, see comments in the
    // 32-bit version
    offset       = (int32_t) (fat_begin_sector * bytes_per_sector -
                              fat_buffer_size);
    offset_entry = (uint64_t) sector_of_cluster(next_cluster) *
                              bytes_per_sector;

    for (i = 0, j = 0; i < max_fat_entries + 2; i++)
    {
        // time to read next segment
        if (i % (fat_buffer_size / 2) == 0)
        {
            // write previous buffer
            if (i != 0)
                seek_write(offset, fat16_buffer, fat_buffer_size);
            // read new buffer
            offset += fat_buffer_size;
            seek_read(offset, fat16_buffer, fat_buffer_size);

            buffer_refreshed = 1;
        }

        // a free cluster
        if (fat16_buffer[i % (fat_buffer_size / 2)] == 0)
        {
            // not enough clusters
            if ((max_fat_entries + 2) - i < file_clusters)
            {
                if (buffer_refreshed)
                {
                    seek_write(fat_begin_sector * bytes_per_sector +
                               prev_cluster * 2, &i, sizeof i);
                }
                else
                    fat16_buffer[prev_cluster % (fat_buffer_size/2)] = i;

                fat16_buffer[i % (fat_buffer_size / 2)] = 0xFFFF;
                goto end;
            }

            // if buffer refreshed
            if (buffer_refreshed)
            {
                // previous buffer happen to complete a file
                if (acc_clusters == 0)
                    start_cluster = i;
                else // write the "previous buffer"
                {
                    seek_write(fat_begin_sector * bytes_per_sector +
                               prev_cluster * 2, &i, sizeof i);
                }
                buffer_refreshed = 0;
            }
            else
            {
                if (acc_clusters == 0)
                    start_cluster = i;
                else
                    fat16_buffer[prev_cluster % (fat_buffer_size/2)] = i;
            }

            prev_cluster = i;
            acc_clusters++;

            if (acc_clusters == file_clusters)
            {
                fat16_buffer[i % (fat_buffer_size / 2)] = 0xFFFF;

write_entry:
                for (; j < max_dir_entries; j++)
                {
                    seek_read(offset_entry, dir_entry, sizeof dir_entry);
                    if (dir_entry[0] == 0x0 || dir_entry[0] == 0xE5 ||
                        dir_entry[0] == 0x05)
                    {
                        Log("start_cluster: %i\n", start_cluster);
                        entry_write_16(offset_entry, filename,
                                       start_cluster, file_size);
                        written_cluster = start_cluster;
                        break;
                    }
                    offset_entry += 32;
                }

                // if the IPCAM directory is full
                if (j == max_dir_entries)
                {
                    uint16_t prev_next;
                    offset_util = (uint64_t)
                                  fat_begin_sector * bytes_per_sector +
                                  next_cluster * 2;
                    prev_next   = next_cluster;

                    seek_read(offset_util, &next_cluster,
                              sizeof next_cluster);

                    // very subtle here!
                    if (next_cluster == 0xFFFF) // must expand IPCAM
                    {
                        // subtlety 0: we must start looking for free
                        // cluster from next "fat16_buffer", since the
                        // previous ones are obviously occupied, and
                        // the current one is used by the file writing
                        // procedure, which will overwrite data on disk.
                        next_cluster = fat_find_free_cluster_16(
                                       offset + fat_buffer_size,
                                       sectors_per_fat); // 0XFF
                        if (next_cluster == 0)
                            goto end;

                        // subtlety 1: if offset_util happens to be in
                        // the current fat16_buffer, we must write into
                        // the buffer instead of the disk
                        if (offset_util >= offset &&
                            offset_util <  offset + fat_buffer_size)
                        {
                            fat16_buffer[prev_next %
                                (fat_buffer_size / 2)] = next_cluster;
                        }
                        else
                        {
                            seek_write(offset_util, &next_cluster,
                                       sizeof next_cluster);
                        }

                        // clear the new cluster
                        offset_entry = (uint64_t)
                                       sector_of_cluster(next_cluster)
                                       * bytes_per_sector;
                        seek_write(offset_entry, cluster_zero,
                                   bytes_per_cluster);
                    }

                    offset_entry = (uint64_t)
                                   sector_of_cluster(next_cluster) *
                                   bytes_per_sector;
                    j            = 0;

                    goto write_entry;
                }

                // reset for a new file
                acc_clusters  = 0;
                increase_filename(filename);
            }
        }
    }

end:
    // this is one tricky thing we keep forgetting
#if 0
    // Wrong! What if the last file spans multiple buffers?
    if (start_cluster > written_cluster) // clear the last file
    {
        uint16_t c = start_cluster;
        uint16_t p;

        do {
            p = fat16_buffer[c % (fat_buffer_size / 2)];
            fat16_buffer[c % (fat_buffer_size / 2)] = 0;
            c = p;
        } while (c != 0xFFFF);
    }
#endif
    seek_write(offset, fat16_buffer, fat_buffer_size);

    if (start_cluster > written_cluster) // clear the last file
    {
        uint16_t c = start_cluster;
        uint16_t p;
        uint16_t start;

        offset = (int32_t) (fat_begin_sector * bytes_per_sector -
                            fat_buffer_size);
        for (i = 0; i < max_fat_entries + 2; i++)
        {
            // time to read next segment
            if (i % (fat_buffer_size / 2) == 0)
            {
                start = i;
                // write previous buffer
                if (i != 0)
                    seek_write(offset, fat16_buffer, fat_buffer_size);
                // read new buffer
                offset += fat_buffer_size;
                seek_read(offset, fat16_buffer, fat_buffer_size);
            }

            if (i == c)
            {
                do {
                    p = fat16_buffer[c % (fat_buffer_size / 2)];
                    fat16_buffer[c % (fat_buffer_size / 2)] = 0;
                    c = p;
                } while (c < start + (fat_buffer_size / 2) &&
                         c != 0xFFFF);
            }
        }
        // this is one tricky thing we keep forgetting
        seek_write(offset, fat16_buffer, fat_buffer_size);
    }

    free(cluster_zero);
}

void write_ipcam_file_32(uint32_t  sectors_per_fat,
                         uint32_t  ipcam_begin_cluster,
                         char     *filename,
                         uint32_t  file_size,
                         uint32_t  file_clusters)
{
    uint32_t  i, j;
    uint32_t  acc_clusters     = 0;
    uint32_t  start_cluster    = 0;
    uint32_t  written_cluster  = 0;
    uint32_t  prev_cluster     = 0;
    uint32_t  next_cluster     = ipcam_begin_cluster;
    int       buffer_refreshed = 0; // bool
    uint8_t   dir_entry[32];
    uint8_t  *cluster_zero;
    off_t     offset;
    off_t     offset_entry;
    off_t     offset_util;

    cluster_zero = calloc(bytes_per_cluster, 1);

    /** offset might be negative: when off_t is 32 bits, this is fine,
     *  when it is 64 bits, this goes wrong.
     *
     *      int32_t test  = fat_begin_sector * bytes_per_sector -
     *                      fat_buffer_size;
     *      int64_t test2 = fat_begin_sector * bytes_per_sector -
     *                      fat_buffer_size;
     *      printf("test = %i, test2 = %lli\n", test, test2);
     *
     *  This will print "test = -29696, test2 = 4294937600".
     */
    offset       = (int32_t) (fat_begin_sector * bytes_per_sector -
                              fat_buffer_size);
    offset_entry = (uint64_t) sector_of_cluster(next_cluster) *
                              bytes_per_sector;

    for (i = 0, j = 0; i < max_fat_entries + 2; i++)
    {
        // time to read next segment
        if (i % (fat_buffer_size / 4) == 0)
        {
            // write previous buffer
            if (i != 0)
                seek_write(offset, fat32_buffer, fat_buffer_size);
            // read new buffer
            offset += fat_buffer_size;
            seek_read(offset, fat32_buffer, fat_buffer_size);

            buffer_refreshed = 1;
        }

        // a free cluster
        if ((fat32_buffer[i % (fat_buffer_size / 4)] & 0xFFFFFFF) == 0)
        {
            // not enough clusters
            if ((max_fat_entries + 2) - i < file_clusters)
            {
                if (buffer_refreshed)
                {
                    seek_write(fat_begin_sector * bytes_per_sector +
                               prev_cluster * 4, &i, sizeof i);
                }
                else
                    fat32_buffer[prev_cluster % (fat_buffer_size/4)] = i;

                fat32_buffer[i % (fat_buffer_size / 4)] = 0xFFFFFFFF;
                goto end;
            }

            // if buffer refreshed
            if (buffer_refreshed)
            {
                // previous buffer happen to complete a file
                if (acc_clusters == 0)
                    start_cluster = i;
                else // write the "previous buffer"
                {
                    seek_write(fat_begin_sector * bytes_per_sector +
                               prev_cluster * 4, &i, sizeof i);
                }
                buffer_refreshed = 0;
            }
            else
            {
                if (acc_clusters == 0)
                    start_cluster = i;
                else
                    fat32_buffer[prev_cluster % (fat_buffer_size/4)] = i;
            }

            prev_cluster = i;
            acc_clusters++;

            if (acc_clusters == file_clusters)
            {
                fat32_buffer[i % (fat_buffer_size / 4)] = 0xFFFFFFFF;

write_entry:
                for (; j < max_dir_entries; j++)
                {
                    seek_read(offset_entry, dir_entry, sizeof dir_entry);
                    if (dir_entry[0] == 0x0 || dir_entry[0] == 0xE5 ||
                        dir_entry[0] == 0x05)
                    {
                        Log("start_cluster: %i - %s\n",
                            start_cluster, filename);
                        entry_write_32(offset_entry, filename,
                                       start_cluster, file_size);
                        written_cluster = start_cluster;
                        break;
                    }
                    offset_entry += 32;
                }

                // if the IPCAM directory is full
                if (j == max_dir_entries)
                {
                    uint32_t prev_next;
                    offset_util = (uint64_t)
                                  fat_begin_sector * bytes_per_sector +
                                  next_cluster * 4;
                    prev_next   = next_cluster;

                    seek_read(offset_util, &next_cluster,
                              sizeof next_cluster);

                    // very subtle here!
                    if (next_cluster == 0xFFFFFFFF) // must expand IPCAM
                    {
                        // subtlety 0: we must start looking for free
                        // cluster from next "fat32_buffer", since the
                        // previous ones are obviously occupied, and
                        // the current one is used by the file writing
                        // procedure, which will overwrite data on disk.
                        next_cluster = fat_find_free_cluster_32(
                                       offset + fat_buffer_size,
                                       sectors_per_fat); // 0XFF
                        if (next_cluster == 0)
                            goto end;

                        // subtlety 1: if offset_util happens to be in
                        // the current fat32_buffer, we must write into
                        // the buffer instead of the disk
                        if (offset_util >= offset &&
                            offset_util <  offset + fat_buffer_size)
                        {
                            fat32_buffer[prev_next %
                                (fat_buffer_size / 4)] = next_cluster;
                        }
                        else
                        {
                            seek_write(offset_util, &next_cluster,
                                       sizeof next_cluster);
                        }

                        /**
                         * The (uint64_t) conversion is also mandatory
                         * here.
                         */
                        // clear the new cluster
                        offset_entry = (uint64_t)
                                       sector_of_cluster(next_cluster) *
                                       bytes_per_sector;
                        seek_write(offset_entry, cluster_zero,
                                   bytes_per_cluster);
                    }

                    offset_entry = (uint64_t)
                                   sector_of_cluster(next_cluster) *
                                   bytes_per_sector;
                    j            = 0;

                    goto write_entry;
                }

                // reset for a new file
                acc_clusters  = 0;
                increase_filename(filename);
            }
        }
    }

end:
    // this is one tricky thing we keep forgetting
#if 0
    // Wrong! What if the last file spans multiple buffers?
    if (start_cluster > written_cluster) // clear the last file
    {
        uint32_t c = start_cluster;
        uint32_t p;

        do {
            p = fat32_buffer[c % (fat_buffer_size / 4)];
            fat32_buffer[c % (fat_buffer_size / 4)] = 0;
            c = p;
        } while (c != 0xFFFFFFFF);
    }
#endif
    seek_write(offset, fat32_buffer, fat_buffer_size);

    if (start_cluster > written_cluster) // clear the last file
    {
        uint32_t c = start_cluster;
        uint32_t p;
        uint32_t start;

        offset = (int32_t) (fat_begin_sector * bytes_per_sector -
                            fat_buffer_size);
        for (i = 0; i < max_fat_entries + 2; i++)
        {
            // time to read next segment
            if (i % (fat_buffer_size / 4) == 0)
            {
                start = i;
                // write previous buffer
                if (i != 0)
                    seek_write(offset, fat32_buffer, fat_buffer_size);
                // read new buffer
                offset += fat_buffer_size;
                seek_read(offset, fat32_buffer, fat_buffer_size);
            }

            if (i == c)
            {
                do {
                    p = fat32_buffer[c % (fat_buffer_size / 4)];
                    fat32_buffer[c % (fat_buffer_size / 4)] = 0;
                    c = p;
                } while (c < start + (fat_buffer_size / 4) &&
                         c != 0xFFFFFFFF);
            }
        }
        // this is one tricky thing we keep forgetting
        seek_write(offset, fat32_buffer, fat_buffer_size);
    }

    free(cluster_zero);
}

void fat16(int mb)
{
    int       ipcam_exists = 0;
    uint16_t  ipcam_begin_cluster;

    uint16_t  max_root_entries;
    uint16_t  sectors_per_fat;

    // short filenames have a fixed length of 8 (padded with ' ')
    char      filename[9];
    uint32_t  file_size;
    uint16_t  file_clusters;

    seek_read(0x11, &max_root_entries, sizeof max_root_entries);
    seek_read(0x16, &sectors_per_fat, sizeof sectors_per_fat);

    fat_begin_sector     = partition_begin_sector + reserved_sector_num;
    cluster_begin_sector = partition_begin_sector + reserved_sector_num +
                           fat_num * sectors_per_fat +
                           max_root_entries * 32 / bytes_per_sector;
    root_begin_sector    = partition_begin_sector + reserved_sector_num +
                           fat_num * sectors_per_fat;
    max_fat_entries      = (total_logical_sectors - reserved_sector_num -
                           fat_num * sectors_per_fat -
                           max_root_entries * 32 / bytes_per_sector) /
                           sectors_per_cluster;

    // calculate the number of clusters needed by each file
    file_size            = 1024 * 1024 * mb;
    file_clusters        = (file_size % bytes_per_cluster) == 0 ?
                            file_size / bytes_per_cluster :
                            file_size / bytes_per_cluster + 1;

    fat_buffer_size      = file_clusters * 2 * 4;
    fat16_buffer         = malloc(fat_buffer_size);

    Log("\n*** FAT16 infomation:\n");
    Log("max_root_entries: %i\n", max_root_entries);
    Log("max_fat_entries: %i\n", max_fat_entries);
    Log("sectors_per_fat: %i\n", sectors_per_fat);
    Log("fat_begin_sector: %i\n", fat_begin_sector);
    Log("root_begin_sector: %i\n", root_begin_sector);
    Log("cluster_begin_sector: %i\n", cluster_begin_sector);

// read

    ipcam_exists = check_ipcam_dir_16(max_root_entries, sectors_per_fat,
                                      &ipcam_begin_cluster);

// write

    strcpy(filename, "00000000");
    if (ipcam_exists) // get the max filename
        read_ipcam_dir_16(filename, ipcam_begin_cluster);
    else // write the "IPCAM" directory
    {
        ipcam_begin_cluster = write_ipcam_dir_16(max_root_entries,
                                                 sectors_per_fat);
        if (ipcam_begin_cluster == 0)
            goto end;
    }

    Log("==> Start writing with filename: %s and file_clusters: %i\n\n",
        filename, file_clusters);
    write_ipcam_file_16(sectors_per_fat,
                        ipcam_begin_cluster,
                        filename, file_size, file_clusters);
end:
    free(fat16_buffer);
}

void fat32(int mb)
{
    int       ipcam_exists = 0;
    uint32_t  ipcam_begin_cluster;

    uint32_t  sectors_per_fat;
    uint32_t  root_begin_cluster;

    // short filenames have a fixed length of 8 (padded with ' ')
    char      filename[9];
    uint32_t  file_size;
    uint32_t  file_clusters;

    seek_read(0x24, &sectors_per_fat, sizeof sectors_per_fat);
    seek_read(0x2C, &root_begin_cluster, sizeof root_begin_cluster);

    // calculate cluster_begin_sector before we can use sector_of_cluster
    fat_begin_sector     = partition_begin_sector + reserved_sector_num;
    cluster_begin_sector = partition_begin_sector + reserved_sector_num +
                           fat_num * sectors_per_fat;
    root_begin_sector    = sector_of_cluster(root_begin_cluster);
    max_fat_entries      = (total_logical_sectors - reserved_sector_num -
                           fat_num * sectors_per_fat) /
                           sectors_per_cluster;

    // calculate the number of clusters needed by each file
    file_size            = 1024 * 1024 * mb;
    file_clusters        = (file_size % bytes_per_cluster) == 0 ?
                            file_size / bytes_per_cluster :
                            file_size / bytes_per_cluster + 1;

    fat_buffer_size      = file_clusters * 4 * 4;
    fat32_buffer         = malloc(fat_buffer_size);

    Log("\n*** FAT32 infomation:\n");
    Log("max_fat_entries: %i\n", max_fat_entries);
    Log("sectors_per_fat: %i\n", sectors_per_fat);
    Log("fat_begin_sector: %i\n", fat_begin_sector);
    Log("cluster_begin_sector: %i\n", cluster_begin_sector);
    Log("root_begin_sector: %i\n", root_begin_sector);
    Log("root_begin_cluster: %i\n", root_begin_cluster);

// read

    ipcam_exists = check_ipcam_dir_32(sectors_per_fat,
                                      root_begin_cluster,
                                      &ipcam_begin_cluster);

// write

    strcpy(filename, "00000000");
    if (ipcam_exists) // get the max filename
        read_ipcam_dir_32(filename, ipcam_begin_cluster);
    else // write the "IPCAM" directory
    {
        ipcam_begin_cluster = write_ipcam_dir_32(sectors_per_fat,
                                                 root_begin_cluster);
        if (ipcam_begin_cluster == 0)
            goto end;
    }

    Log("==> Start writing with filename: %s and file_clusters: %i\n\n",
        filename, file_clusters);
    write_ipcam_file_32(sectors_per_fat,
                        ipcam_begin_cluster,
                        filename, file_size, file_clusters);
end:
    free(fat32_buffer);
}

int creat_record_file(int argc, char **argv)
{
    char     fat_type[8];
    uint8_t *byte;

    if (argc != 3)
    {
        Log("./fatty 256 fat.img\n");
        return 1;
    }
    Log("argv[0] = %s\nargv[1] = %s\nargv[2]=%s\n",argv[0],argv[1],argv[2]);
    device_fd = open(argv[2], O_RDWR);
    if (device_fd < 0)
    {
        Log("open() failed\n");
	Log("dev name = %s\n",argv[2]);
        return 1;
    }

    // http://www.maverick-os.dk/FileSystemFormats/FAT16_FileSystem.html
    // http://www.pjrc.com/tech/8051/ide/fat32.html
    seek_read(0x0B, &bytes_per_sector, sizeof bytes_per_sector);
    if (bytes_per_sector != 512)
    {
        Log("error: bytes_per_sector != 512\n");
        goto end;
    }
    seek_read(0x0D, &sectors_per_cluster, sizeof sectors_per_cluster);
    seek_read(0x0E, &reserved_sector_num, sizeof reserved_sector_num);
    seek_read(0x10, &fat_num, sizeof fat_num);
    if (fat_num != 2)
    {
        Log("error: fat_num != 2\n");
        goto end;
    }

    // 0. If greater than 65535; otherwise, see offset 0x013 (which we
    //    don't bother).
    // 1. This DOS 3.31 entry is incompatible with a
    //    similar entry at offset 0x01E in DOS 3.2-3.3 BPBs (which we
    //    don't bother either).
    seek_read(0x20, &total_logical_sectors,
              sizeof total_logical_sectors);

    seek_read(0x36, &fat_type, sizeof fat_type);
    seek_read(0x1FE, &signature, sizeof signature);
    byte = (uint8_t *)&signature;
    if (*byte != 0x55 && *(byte + 1) != 0xAA)
    {
        Log("error: wrong signature\n");
        goto end;
    }

    Log("*** FAT infomation:\n");
    Log("bytes_per_sector: %i\n", bytes_per_sector);       // always 512
    Log("sectors_per_cluster: %i\n", sectors_per_cluster);
    Log("reserved_sector_num: %i\n", reserved_sector_num);
    Log("total_logical_sectors: %i\n", total_logical_sectors);
    Log("fat_num: %i\n", fat_num);                         // 2
    Log("fat_type: %s\n", fat_type);

    bytes_per_cluster = bytes_per_sector * sectors_per_cluster;
    max_dir_entries   = bytes_per_cluster / 32;

    // This is a very dirty way to determine FAT type, as indicated by
    // wikipedia: 
    //
    // This entry is meant for display purposes only and must not be
    // used by the operating system to identify the type of the file
    // system. Nevertheless, it is sometimes used for identification
    // purposes by third-party software and therefore the values should
    // not differ from those officially used.
    //
    // For a more elaborate solution:
    //
    //   http://homepage.ntlworld.com/jonathan.deboynepollard/FGA/
    //   determining-fat-widths.html
    //
    if (strcmp(fat_type, "FAT16   ") == 0)
        fat16(atoi(argv[1]));
    else
        fat32(atoi(argv[1]));

end:
    printf("Done.\n");
    close(device_fd);
    return 0;
}

