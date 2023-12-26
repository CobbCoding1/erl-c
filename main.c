#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

typedef uint32_t word;
typedef uint8_t byte;

typedef struct {
    char ifheader[sizeof(word)];
    byte size_buf[sizeof(word)];
    char formtype[sizeof(word)];
} File_Header;

typedef struct {
    char name[sizeof(word)];
    byte size_buf[sizeof(word)];
    byte sub_size_buf[sizeof(word)];
} Block_Header;

typedef struct {
    int size[32];
    size_t size_len;
    char name[32][32];
    size_t name_len;
    size_t total_read;
} Atom_Body;

typedef struct {
    byte opcode_max[sizeof(word)];
    byte label_count[sizeof(word)];
    byte function_count[sizeof(word)];
} Code_Body;

typedef struct {
    byte mod_name[sizeof(word)];
    byte function_name[sizeof(word)];
    byte arity[sizeof(word)];
} Import;

int decode_big_endian(byte *buf, size_t buf_s) {
    size_t shift_index = 0;
    int size = 0;
    for(int i = buf_s - 1; i >= 0; i--) {
        size |= buf[i] << (8 * shift_index++);
    }
    return size;
}

int align_by_four(int n) {
    return 4 * ((n + 3)/4);
}

int main(void) {
    FILE *file = fopen("hello.beam", "rb");
    if(file == NULL) assert(0);

    File_Header f_header;
    fread(&f_header, sizeof(File_Header), 1, file);

    Block_Header atu8_header;
    Atom_Body atom_body = {0};
    fread(&atu8_header, sizeof(Block_Header), 1, file);
    atom_body.total_read = 4; // initialize total_read to 4, because that's what we've read

    for(size_t i = 0; (int)i < decode_big_endian(atu8_header.sub_size_buf, sizeof(word)); i++) {
        fread(&atom_body.size[atom_body.size_len++], sizeof(byte), 1, file);
        atom_body.total_read += sizeof(byte);
        fread(atom_body.name[atom_body.name_len++], sizeof(byte) * atom_body.size[atom_body.size_len - 1], 1, file);
        atom_body.total_read += sizeof(byte) * atom_body.size[atom_body.size_len - 1];
    }
    int chunk_size = align_by_four(decode_big_endian(atu8_header.size_buf, sizeof(word)));
    int nothing = 0;
    fread(&nothing, sizeof(byte) * (chunk_size-atom_body.total_read), 1, file);

    Block_Header code_header = {0};
    Code_Body code_body = {0};
    fread(&code_header, sizeof(Block_Header), 1, file);
    fread(&code_body, sizeof(Code_Body), 1, file);

    int code_chunk_size = align_by_four(decode_big_endian(code_header.size_buf, sizeof(word)));
    int code_sub_size = decode_big_endian(code_header.sub_size_buf, sizeof(word));
    int code_size = code_chunk_size - code_sub_size;
    printf("%d %d\n", code_chunk_size, code_sub_size);
    printf("%d\n", code_size);
    byte code[code_size];
    fread(code, sizeof(byte) * code_size, 1, file);

    char *str_t_name = malloc(sizeof(word));
    byte str_t_size_buf[sizeof(word)];
    fread(str_t_name, sizeof(word), 1, file);
    fread(str_t_size_buf, sizeof(word), 1, file);

    char *imp_t_name = malloc(sizeof(word));
    byte imp_t_size_buf[sizeof(word)];
    byte imp_t_count_buf[sizeof(word)];
    fread(imp_t_name, sizeof(word), 1, file);
    fread(imp_t_size_buf, sizeof(word), 1, file);
    size_t count = 0;
    fread(imp_t_count_buf, sizeof(word), 1, file);
    count += 4;
    size_t imp_count = decode_big_endian(imp_t_count_buf, sizeof(word));
    Import imports[imp_count];
    for(size_t i = 0; i < imp_count; i++) {
        fread(&imports[i], sizeof(Import), 1, file);
        count += sizeof(Import);
    }

    int diff = align_by_four(decode_big_endian(imp_t_size_buf, sizeof(word)) - count);
    fread(&nothing, sizeof(byte) * diff, 1, file);

    fclose(file);
    return 0;
}
