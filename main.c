#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "view.c"

typedef uint32_t word;
typedef uint8_t byte;

typedef struct {
    String_View name;
    int value;
} Function;

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

typedef struct {
    byte function_name[sizeof(word)];
    byte arity[sizeof(word)];
    byte label[sizeof(word)];
} Export;

typedef struct {
    char name[sizeof(word)];
    byte size_buf[sizeof(word)];
    byte count_buf[sizeof(word)];
} Function_Header;

typedef enum {
    Label = 1,
    FuncInfo = 2,
    Return = 19,
    Move = 64,
} Opcodes;

int decode_big_endian(byte *buf, size_t buf_s) {
    size_t shift_index = 0;
    int size = 0;
    for(int i = buf_s - 1; i >= 0; i--) {
        size |= buf[i] << (8 * shift_index++);
    }
    return size;
}

byte *encode_big_endian(int n) {
    byte *buf = malloc(sizeof(word));
    for(size_t i = 0; i < sizeof(word); i++) {
        buf[i] = (byte)((n >> (8 * (sizeof(word) - 1 - i))) & 0xFF);
    }
    return buf;
}

int align_by_four(int n) {
    return 4 * ((n + 3)/4);
}

typedef struct {
    byte a;
    byte b;
} Encoded;

Encoded encode(int tag, int n) {
    if(n < 16) {
        Encoded result = {0};
        result.a = (byte)((n << 4)|tag);
        return result;
    } else if(n < 0x800) {
        //[((N bsr 3) band 2#11100000) bor Tag bor 2#00001000, N band 16#ff];
        Encoded result = {0};
        result.a = (byte)(((n >> 3) & 0xE0) | tag | 0x8);
        result.b = n & 0xFF;
        return result;
    }
    return (Encoded){0};
}

#define FUNC_COUNT 3
void generate_bytecode(Function *functions, size_t functions_s, char *filename) {
    char new_filename[128];
    sprintf(new_filename, "%s.beam", filename);
    FILE *file = fopen(new_filename, "wb");
    if(file == NULL) assert(0);

    File_Header f_header = {
        .ifheader = "FOR1",
        .formtype = "BEAM",
    };

    memcpy(f_header.size_buf, encode_big_endian(0), sizeof(word));
    fwrite(&f_header, sizeof(File_Header), 1, file);

    Block_Header atom_header = {
        .name = "AtU8",
    };

    size_t name_sizes = strlen(filename)+1;
    for(size_t i = 0; i < functions_s; i++) {
        name_sizes += functions[i].name.len + 1;
    }
    memcpy(atom_header.size_buf, encode_big_endian(4 + (name_sizes)), sizeof(word));
    memcpy(atom_header.sub_size_buf, encode_big_endian(1 + (functions_s)), sizeof(word));

    fwrite(&atom_header, sizeof(Block_Header), 1, file);

    char *atom_str = filename;
    byte atom_len = (byte)strlen(atom_str);
    fwrite(&atom_len, sizeof(byte), 1, file);
    fwrite(atom_str, sizeof(byte) * atom_len, 1, file);

    String_View atom_view;
    for(size_t i = 0; i < functions_s; i++) {
        atom_view = functions[i].name;
        atom_len = (byte)atom_view.len;
        fwrite(&atom_len, sizeof(byte), 1, file);
        fwrite(atom_view.data, sizeof(byte) * atom_len, 1, file);
    }

    int atom_size = decode_big_endian(atom_header.size_buf, sizeof(word));
    fwrite(encode_big_endian(0), align_by_four(atom_size) - atom_size, 1, file);


    Block_Header code_header = {
        .name = "Code",
    };

    #define FUNC_SIZE 12
    size_t offset = 0;
    for(size_t i = 0; i < functions_s; i++) {
        if(functions[i].value >= 16) {
            offset += 1;
        }
    }
    memcpy(code_header.size_buf, encode_big_endian(21 + (functions_s * FUNC_SIZE) + offset), sizeof(word));
    memcpy(code_header.sub_size_buf, encode_big_endian(16), sizeof(word));

    fwrite(&code_header, sizeof(Block_Header), 1, file);

    Code_Body code_body;

    byte inst_set[sizeof(word)]; 
    memcpy(inst_set, encode_big_endian(0), sizeof(word));
    memcpy(code_body.opcode_max, encode_big_endian(169), sizeof(word));
    memcpy(code_body.label_count, encode_big_endian(1+(functions_s * 2)), sizeof(word));
    memcpy(code_body.function_count, encode_big_endian(functions_s), sizeof(word));

    fwrite(inst_set, sizeof(word), 1, file);
    fwrite(&code_body, sizeof(Code_Body), 1, file);

    Encoded value = {0};

    int label_count = 1;
    for(size_t i = 0; i < functions_s; i++) {
        byte values = (byte)Label;
        fwrite(&values, sizeof(byte), 1, file);
        value = encode(0, label_count++);
        fwrite(&value.a, sizeof(byte), 1, file);

        values = (byte)FuncInfo;
        fwrite(&values, sizeof(byte), 1, file);
        value = encode(2, 1);
        fwrite(&value.a, sizeof(byte), 1, file);
        value = encode(2, i+2);
        fwrite(&value.a, sizeof(byte), 1, file);
        value = encode(0, 0);
        fwrite(&value.a, sizeof(byte), 1, file);

        values = (byte)Label;
        fwrite(&values, sizeof(byte), 1, file);
        value = encode(0, label_count++);
        fwrite(&value.a, sizeof(byte), 1, file);

        values = (byte)Move;
        fwrite(&values, sizeof(byte), 1, file);
        Encoded ivalue = encode(1, functions[i].value);
        fwrite(&ivalue.a, sizeof(byte), 1, file);
        if(functions[i].value >= 16) {
            fwrite(&ivalue.b, sizeof(byte), 1, file);
        }
        value = encode(3, 0);
        fwrite(&value.a, sizeof(byte), 1, file);

        values = (byte)Return;
        fwrite(&values, sizeof(byte), 1, file);
    }

    byte values = (byte)3;
    fwrite(&values, sizeof(byte), 1, file);

    int code_size = decode_big_endian(code_header.size_buf, sizeof(word));
    fwrite(encode_big_endian(0), align_by_four(code_size)-code_size, 1, file);

    fwrite("StrT", sizeof(word), 1, file);
    fwrite(encode_big_endian(0), sizeof(word), 1, file);

    Block_Header imp_t_header = {
        .name = "ImpT",
    };

    memcpy(imp_t_header.size_buf, encode_big_endian(sizeof(word)), sizeof(word));
    memcpy(imp_t_header.sub_size_buf, encode_big_endian(0), sizeof(word));

    fwrite(&imp_t_header, sizeof(Block_Header), 1, file);

    Block_Header exp_t_header = {
        .name = "ExpT",
    };

    memcpy(exp_t_header.size_buf, encode_big_endian((sizeof(word) * 1) + ((sizeof(word)*3)*functions_s)), sizeof(word));
    memcpy(exp_t_header.sub_size_buf, encode_big_endian(functions_s), sizeof(word));

    fwrite(&exp_t_header, sizeof(Block_Header), 1, file);

    for(size_t i = 0; i < functions_s; i++) {
        fwrite(encode_big_endian(i+2), sizeof(word), 1, file);
        fwrite(encode_big_endian(0), sizeof(word), 1, file);
        fwrite(encode_big_endian((i+1)*2), sizeof(word), 1, file);
    }

    int export_size = decode_big_endian(exp_t_header.size_buf, sizeof(word));
    fwrite(encode_big_endian(0), align_by_four(export_size) - export_size, 1, file);

    int flen = ftell(file);
    fseek(file, 0, SEEK_SET);

    memcpy(f_header.size_buf, encode_big_endian(flen-8), sizeof(word));
    fwrite(&f_header, sizeof(File_Header), 1, file);

    fclose(file);
}


int main(int argc, char *argv[]) {
    char *program = argv[0];
    if(argc < 2) {
        fprintf(stderr, "usage: %s <file_name.erc>\n", program);
        exit(1);
    }
    char *filename = argv[1];
    size_t filename_len = strlen(filename);
    FILE *file = fopen(filename, "r");
    if(file == NULL) assert(0);

    for(size_t i = filename_len; i > 0; i--) {
        if(filename[i] == '.') {
            filename[i] = '\0';
            break;
        }
    }
    
    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buf = malloc(sizeof(char) * length);
    fread(buf, length, 1, file);
    fclose(file);
    buf[length] = '\0';

    String_View contents = view_create(buf, length);
    #define ARR_S 128
    String_View arr[ARR_S];

    contents = view_trim_right(contents);
    int arr_len = view_split_whitespace(contents, arr, ARR_S);

    assert(arr_len % 3 == 0 && "Incorrect syntax\n");

    Function functions[arr_len/3];
    size_t functions_len = 0;
    for(size_t i = 0; (int)i < arr_len; i++) {
        Function func = {0};
        func.name = arr[i];
        i += 2;
        func.value = view_to_int(arr[i]);
        functions[functions_len++] = func;
    }

    generate_bytecode(functions, functions_len, filename);
}
