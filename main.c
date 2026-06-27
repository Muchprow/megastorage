#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "compressor.h"


typedef struct {
    char file_name[256];
    uint32_t orig_size;
    uint32_t packed_size;
    int is_compressed;
} FileMeta;

typedef struct {
    char signature[4];
    char original_name[256];
} ArchiveHeader;

void set_theme_colors() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    system("color 1F");
}

void draw_header(const char *title) {
    printf("\n\t==========================================================\n");
    printf("\t                    %s \n", title);
    printf("\t==========================================================\n\n");
}

void print_progress(uint64_t current, uint64_t total) {
    if (total == 0) return;
    int width = 40;
    int percent = (int)((current * 100) / total);
    int pos = (int)((current * width) / total);

    printf("\r\tProgress: [");
    for (int i = 0; i < width; ++i) {
        if (i < pos) printf("=");
        else if (i == pos) printf(">");
        else printf(".");
    }
    printf("] %d%%", percent);
    fflush(stdout);
}

void pack_smart() {
    char input_paths[2048];
    char archive_name[512];

    system("cls");
    draw_header("MEGASTORAGE - SMART PACKER");

    printf("\tEnter file(s) to pack (separated by space or comma):\n\t-> ");
    if (fgets(input_paths, sizeof(input_paths), stdin) == NULL) return;
    input_paths[strcspn(input_paths, "\r\n")] = 0;

    printf("\n\tEnter output archive name (without extension):\n\t-> ");
    if (fgets(archive_name, sizeof(archive_name), stdin) == NULL) return;
    archive_name[strcspn(archive_name, "\r\n")] = 0;

    CreateDirectoryA("MGS_Archives", NULL);

    char *files[64];
    int file_count = 0;
    char *token = strtok(input_paths, " ,\"");
    while (token != NULL && file_count < 64) {
        files[file_count++] = token;
        token = strtok(NULL, " ,\"");
    }

    if (file_count == 0) {
        printf("\n\t[ERROR] No input files specified! Press Enter...");
        getchar();
        return;
    }

    uint8_t *src_buf = (uint8_t *)malloc(CHUNK_SIZE);
    uint8_t *dst_buf = (uint8_t *)malloc(CHUNK_SIZE * 3);

    uint64_t total_original_size = 0;
    uint64_t total_packed_size = 0;

    // ==========================================
    // SCENARIO 1: Single file -> Produce .mgs
    // ==========================================
    if (file_count == 1) {
        char full_archive_path[1024];
        sprintf(full_archive_path, "MGS_Archives\\%s.mgs", archive_name);

        FILE *in = fopen(files[0], "rb");
        if (!in) {
            printf("\n\t[ERROR] File '%s' not found! Press Enter...", files[0]);
            free(src_buf); free(dst_buf);
            getchar(); return;
        }

        FILE *arch = fopen(full_archive_path, "wb");
        if (!arch) {
            printf("\n\t[ERROR] Can't create archive! Press Enter...");
            fclose(in); free(src_buf); free(dst_buf);
            getchar(); return;
        }

        char *pure_name = strrchr(files[0], '\\');
        if (!pure_name) pure_name = strrchr(files[0], '/');
        if (!pure_name) pure_name = files[0]; else pure_name++;

        ArchiveHeader header;
        memcpy(header.signature, "MGS!", 4);
        strncpy(header.original_name, pure_name, 256);
        fwrite(&header, sizeof(ArchiveHeader), 1, arch);
        total_packed_size += sizeof(ArchiveHeader);

        fseek(in, 0, SEEK_END);
        uint64_t file_size = _ftelli64(in);
        fseek(in, 0, SEEK_SET);

        total_original_size = file_size;

        printf("\n\tPacking single file into .mgs archive:\n");
        printf("\t-> %s (%.2f MB) ", pure_name, (double)file_size / (1024.0 * 1024.0));

        uint64_t bytes_processed = 0;
        while (bytes_processed < file_size) {
            uint32_t to_read = (file_size - bytes_processed > CHUNK_SIZE) ? CHUNK_SIZE : (uint32_t)(file_size - bytes_processed);
            fread(src_buf, 1, to_read, in);

            int block_compressed = 0;
            uint32_t packed_chunk_size = compress_block(src_buf, to_read, dst_buf, &block_compressed);

            uint8_t flag = (uint8_t)block_compressed;
            fwrite(&flag, sizeof(uint8_t), 1, arch);
            fwrite(&packed_chunk_size, sizeof(uint32_t), 1, arch);
            fwrite(&to_read, sizeof(uint32_t), 1, arch);
            fwrite(dst_buf, 1, packed_chunk_size, arch);

            total_packed_size += sizeof(uint8_t) + sizeof(uint32_t) + sizeof(uint32_t) + packed_chunk_size;

            bytes_processed += to_read;
            print_progress(bytes_processed, file_size);
        }
        printf(" [DONE]\n");
        fclose(in); fclose(arch);
    }
    // ==========================================
    // SCENARIO 2: Multiple files -> Produce .mgsa
    // ==========================================
    else {
        char full_archive_path[1024];
        sprintf(full_archive_path, "MGS_Archives\\%s.mgsa", archive_name);

        FILE *arch = fopen(full_archive_path, "wb");
        if (!arch) {
            printf("\n\t[ERROR] Can't create archive! Press Enter...");
            free(src_buf); free(dst_buf);
            getchar(); return;
        }

        fwrite(&file_count, sizeof(int), 1, arch);
        total_packed_size += sizeof(int);

        long header_pos = ftell(arch);
        FileMeta *metadata = (FileMeta *)malloc(file_count * sizeof(FileMeta));
        fwrite(metadata, sizeof(FileMeta), file_count, arch);
        total_packed_size += file_count * sizeof(FileMeta);

        printf("\n\tPacking multiple files into .mgsa container:\n");
        for (int f = 0; f < file_count; f++) {
            FILE *in = fopen(files[f], "rb");
            if (!in) {
                printf("\t[SKIP] File '%s' not found!\n", files[f]);
                metadata[f].orig_size = 0;
                metadata[f].packed_size = 0;
                continue;
            }

            char *pure_name = strrchr(files[f], '\\');
            if (!pure_name) pure_name = strrchr(files[f], '/');
            if (!pure_name) pure_name = files[f]; else pure_name++;

            strncpy(metadata[f].file_name, pure_name, 255);

            fseek(in, 0, SEEK_END);
            uint64_t file_size = _ftelli64(in);
            fseek(in, 0, SEEK_SET);

            metadata[f].orig_size = (uint32_t)file_size;
            total_original_size += file_size;

            printf("\t-> %s (%.2f MB) ", pure_name, (double)file_size / (1024.0 * 1024.0));

            uint64_t bytes_processed = 0;
            uint32_t total_packed_for_file = 0;
            int file_was_compressed = 0;

            while (bytes_processed < file_size) {
                uint32_t to_read = (file_size - bytes_processed > CHUNK_SIZE) ? CHUNK_SIZE : (uint32_t)(file_size - bytes_processed);
                fread(src_buf, 1, to_read, in);

                int block_compressed = 0;
                uint32_t packed_chunk_size = compress_block(src_buf, to_read, dst_buf, &block_compressed);

                fwrite(&packed_chunk_size, sizeof(uint32_t), 1, arch);
                fwrite(&block_compressed, sizeof(int), 1, arch);
                fwrite(dst_buf, 1, packed_chunk_size, arch);

                total_packed_for_file += packed_chunk_size + sizeof(uint32_t) + sizeof(int);
                if (block_compressed) file_was_compressed = 1;

                bytes_processed += to_read;
                print_progress(bytes_processed, file_size);
            }
            fclose(in);

            metadata[f].packed_size = total_packed_for_file;
            metadata[f].is_compressed = file_was_compressed;
            total_packed_size += total_packed_for_file;
            printf(" [DONE]\n");
        }

        fseek(arch, header_pos, SEEK_SET);
        fwrite(metadata, sizeof(FileMeta), file_count, arch);
        fclose(arch);
        free(metadata);
    }

    free(src_buf); free(dst_buf);

    double ratio = 0.0;
    if (total_original_size > 0) {
        ratio = (1.0 - ((double)total_packed_size / (double)total_original_size)) * 100.0;
    }

    printf("\n\t----------------------------------------------------------\n");
    printf("\t  Original Total Size:   %.2f MB\n", (double)total_original_size / (1024.0 * 1024.0));
    printf("\t  Packed Archive Size:   %.2f MB\n", (double)total_packed_size / (1024.0 * 1024.0));
    printf("\t  Storage Saved:         %.1f %%\n", ratio < 0 ? 0.0 : ratio);
    printf("\t----------------------------------------------------------\n");

    printf("\n\tSUCCESS! Archive operation completed.\n\tPress Enter to return to menu...");
    getchar();
}

// Universal TUI Unpacker for .mgs and .mgsa
void unpack_smart() {
    char archive_path[512];
    system("cls");
    draw_header("MEGASTORAGE - UNPACKER");

    printf("\tEnter full path to archive (.mgs or .mgsa):\n\t-> ");
    if (fgets(archive_path, sizeof(archive_path), stdin) == NULL) return;
    archive_path[strcspn(archive_path, "\r\n")] = 0;

    FILE *arch = fopen(archive_path, "rb");
    if (!arch) {
        printf("\n\t[ERROR] Archive not found! Press Enter...");
        getchar(); return;
    }

    uint8_t *src_buf = (uint8_t *)malloc(CHUNK_SIZE * 3);
    uint8_t *dst_buf = (uint8_t *)malloc(CHUNK_SIZE);

    CreateDirectoryA("archive_root", NULL);

    // ==========================================
    // INTERACTIVE TUI MODE FOR .MGSA CONTAINER
    // ==========================================
    if (strstr(archive_path, ".mgsa") != NULL) {
        int file_count = 0;
        fread(&file_count, sizeof(int), 1, arch);

        FileMeta *metadata = (FileMeta *)malloc(file_count * sizeof(FileMeta));
        fread(metadata, sizeof(FileMeta), file_count, arch);

        long data_start_pos = ftell(arch);

        while (1) {
            system("cls");
            draw_header("MEGASTORAGE - CONTAINER EXPLORER (.mgsa)");
            printf("\tArchive path: %s\n", archive_path);
            printf("\tFiles inside container: %d\n\n", file_count);

            // Fixed Table Header in pure English
            printf("\tID  | %-30s | %-12s | %-12s | Method\n", "File Name", "Orig Size", "Packed Size");
            printf("\t----+--------------------------------+--------------+--------------+---------\n");
            for (int i = 0; i < file_count; i++) {
                if (metadata[i].orig_size == 0) continue;
                printf("\t%-3d | %-30.30s | %-10u B | %-10u B | %s\n",
                       i + 1,
                       metadata[i].file_name,
                       metadata[i].orig_size,
                       metadata[i].packed_size,
                       metadata[i].is_compressed ? "LZ-OK" : "STORE");
            }
            printf("\t----+--------------------------------+--------------+--------------+---------\n\n");

            printf("\t[Container Management]:\n");
            printf("\t1. Extract ALL files\n");
            printf("\t2. Extract SINGLE file by ID\n");
            printf("\t0. Back to Main Menu\n\n");
            printf("\tSelect action -> ");

            char tui_choice[10];
            if (fgets(tui_choice, sizeof(tui_choice), stdin) == NULL) break;

            if (tui_choice[0] == '0') {
                break;
            }
            // Extract All
            else if (tui_choice[0] == '1') {
                fseek(arch, data_start_pos, SEEK_SET);
                printf("\n\tExtracting all files to 'archive_root' folder...\n");
                for (int f = 0; f < file_count; f++) {
                    if (metadata[f].orig_size == 0) continue;

                    char out_path[1024];
                    sprintf(out_path, "archive_root\\%s", metadata[f].file_name);
                    FILE *out = fopen(out_path, "wb");
                    if (!out) {
                        fseek(arch, metadata[f].packed_size, SEEK_CUR);
                        continue;
                    }

                    printf("\t-> %s ", metadata[f].file_name);
                    uint32_t bytes_written = 0;
                    while (bytes_written < metadata[f].orig_size) {
                        uint32_t packed_chunk_size = 0;
                        int is_chunk_compressed = 0;

                        fread(&packed_chunk_size, sizeof(uint32_t), 1, arch);
                        fread(&is_chunk_compressed, sizeof(int), 1, arch);
                        fread(src_buf, 1, packed_chunk_size, arch);

                        uint32_t expected = (metadata[f].orig_size - bytes_written > CHUNK_SIZE) ? CHUNK_SIZE : (metadata[f].orig_size - bytes_written);

                        if (is_chunk_compressed) {
                            decompress_block(src_buf, packed_chunk_size, dst_buf, expected);
                            fwrite(dst_buf, 1, expected, out);
                        } else {
                            fwrite(src_buf, 1, packed_chunk_size, out);
                        }
                        bytes_written += expected;
                        print_progress(bytes_written, metadata[f].orig_size);
                    }
                    fclose(out);
                    printf(" [OK]\n");
                }
                printf("\n\tAll files extracted successfully! Press Enter...");
                getchar();
            }
            // Extract Single File by ID
            else if (tui_choice[0] == '2') {
                printf("\tEnter target File ID: ");
                char id_str[10];
                fgets(id_str, sizeof(id_str), stdin);
                int target_id = atoi(id_str) - 1;

                if (target_id < 0 || target_id >= file_count || metadata[target_id].orig_size == 0) {
                    printf("\t[ERROR] Invalid File ID! Press Enter...");
                    getchar(); continue;
                }

                fseek(arch, data_start_pos, SEEK_SET);
                for (int f = 0; f < target_id; f++) {
                    fseek(arch, metadata[f].packed_size, SEEK_CUR);
                }

                char out_path[1024];
                sprintf(out_path, "archive_root\\%s", metadata[target_id].file_name);
                FILE *out = fopen(out_path, "wb");
                if (!out) {
                    printf("\t[ERROR] Failed to create output file! Press Enter...");
                    getchar(); continue;
                }

                printf("\n\tExtracting target file: %s\n", metadata[target_id].file_name);
                uint32_t bytes_written = 0;
                while (bytes_written < metadata[target_id].orig_size) {
                    uint32_t packed_chunk_size = 0;
                    int is_chunk_compressed = 0;

                    fread(&packed_chunk_size, sizeof(uint32_t), 1, arch);
                    fread(&is_chunk_compressed, sizeof(int), 1, arch);
                    fread(src_buf, 1, packed_chunk_size, arch);

                    uint32_t expected = (metadata[target_id].orig_size - bytes_written > CHUNK_SIZE) ? CHUNK_SIZE : (metadata[target_id].orig_size - bytes_written);

                    if (is_chunk_compressed) {
                        decompress_block(src_buf, packed_chunk_size, dst_buf, expected);
                        fwrite(dst_buf, 1, expected, out);
                    } else {
                        fwrite(src_buf, 1, packed_chunk_size, out);
                    }
                    bytes_written += expected;
                    print_progress(bytes_written, metadata[target_id].orig_size);
                }
                fclose(out);
                printf(" [EXTRACTED]\n");
                printf("\n\tFile extracted successfully! Press Enter...");
                getchar();
            }
        }
        free(metadata);
    }
    // ==========================================
    // STANDARD SINGLE .MGS EXTRACTION
    // ==========================================
    else {
        ArchiveHeader header;
        if (fread(&header, sizeof(ArchiveHeader), 1, arch) != 1 || memcmp(header.signature, "MGS!", 4) != 0) {
            printf("\n\t[ERROR] Invalid .mgs format! Press Enter...");
            fclose(arch); free(src_buf); free(dst_buf);
            getchar(); return;
        }

        char out_path[1024];
        sprintf(out_path, "archive_root\\%s", header.original_name);
        FILE *out = fopen(out_path, "wb");

        printf("\n\tExtracting single .mgs file to 'archive_root':\n");
        printf("\t-> %s ", header.original_name);

        uint8_t flag = 0;
        uint32_t packed_size = 0, orig_size = 0;

        while (fread(&flag, sizeof(uint8_t), 1, arch) == 1) {
            fread(&packed_size, sizeof(uint32_t), 1, arch);
            fread(&orig_size, sizeof(uint32_t), 1, arch);
            fread(src_buf, 1, packed_size, arch);

            if (flag == 1) {
                decompress_block(src_buf, packed_size, dst_buf, orig_size);
                fwrite(dst_buf, 1, orig_size, out);
            } else {
                fwrite(src_buf, 1, packed_size, out);
            }
            printf(".");
            fflush(stdout);
        }
        fclose(out);
        printf(" [EXTRACTED]\n");
        printf("\n\tSUCCESS! Data extracted to 'archive_root'.\n\tPress Enter...");
        getchar();
    }

    fclose(arch);
    free(src_buf); free(dst_buf);
}

int main() {
    set_theme_colors();
    char choice[10];

    while (1) {
        system("cls");
        draw_header("MEGASTORAGE");
        printf("\t========================================\n");
        printf("\t        1. COMPRESS DATA (.MGS/.MGSA)   \n");
        printf("\t        2. OPEN / UNPACK ARCHIVE        \n");
        printf("\t        3. EXIT SYSTEM                  \n");
        printf("\t========================================\n\n");
        printf("\tSelect option -> ");

        if (fgets(choice, sizeof(choice), stdin) == NULL) break;

        if (choice[0] == '1') {
            pack_smart();
        } else if (choice[0] == '2') {
            unpack_smart();
        } else if (choice[0] == '3') {
            break;
        }
    }
    return 0;
}
