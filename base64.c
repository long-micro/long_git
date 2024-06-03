#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define padding '='

int max_encrypted = 2;
int max_unencrypted = 2;

struct three_bytes {
    uint8_t byte1;
    uint8_t byte2;
    uint8_t byte3;
};

static const char base64_table[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *no_overflow(char *array, int cur_index, int add_num, int *max_num)
{
    bool overflowed = ((cur_index + add_num) > (*max_num - 1));
    if (true == overflowed) {
        *max_num = cur_index + add_num + 1;
        array = (char *)realloc(array, (*max_num) * sizeof(char));
        if (NULL == array) {
            printf("realloc error, err(%d:%s)", errno, strerror(errno));
            return NULL;
        }
    }

    return array;
}

static int char_to_base64(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return 26 + c -'a';
    if (c >= '0' && c <= '9') return 52 + c - '0';
    if ('+' == c) return 62;
    if ('/' == c) return 63;
    return -1;
}

static uint8_t *generate_base64(struct three_bytes three_bytes)
{
    static uint8_t base64[4]; 

    base64[0] = three_bytes.byte1 >> 2;
    base64[1] = ((three_bytes.byte1 & 0x03) << 4) + (three_bytes.byte2 >> 4);
    base64[2] = ((three_bytes.byte2 & 0x0F) << 2) + (three_bytes.byte3 >> 6);
    base64[3] = three_bytes.byte3 & 0x3F;

    return base64;
}

static uint8_t *generate_ascii(uint8_t *base64)
{
    static uint8_t ascii[3];

    ascii[0] = (base64[0] << 2) + (base64[1] >> 4);
    ascii[1] = (base64[1] << 4) + (base64[2] >> 2);
    ascii[2] = (base64[2] << 6) + (base64[3]);

    return ascii;
}

static void padding_append(const unsigned int bytes, char *encrypted, int index)
{
    if(2 == (bytes %3)) {
        encrypted[index-1] = padding;
    }

    if(1 == (bytes %3)) {
        encrypted[index-1] = padding;
        encrypted[index-2] = padding;
    }
}

static void padding_remove(int padding_num, char *unencrypted, int index)
{
    if (2 == padding_num) {
        unencrypted[index - 2] = '\0';
    }

    if (1 == padding_num) {
        unencrypted[index - 1] = '\0';
    }
}

static char *base64_encode(const char *unencrypted, const unsigned int bytes)
{   
    int index = 0;
    struct three_bytes three_bytes = {0};

    char *encrypted = (char *)malloc(max_encrypted * sizeof(char));
    if(NULL == encrypted) {
        printf("malloc error err(%d:%s)", errno, strerror(errno));
        return NULL;
    }

    for (int i = 0; i < bytes; i+=3) {
        three_bytes.byte1 = i < bytes ? unencrypted[i] : 0;
        three_bytes.byte2 = (i+1) < bytes ? unencrypted[i+1] : 0;
        three_bytes.byte3 = (i+2) < bytes ? unencrypted[i+2] : 0;

        uint8_t *base64 = generate_base64(three_bytes);
        encrypted = no_overflow(encrypted, 4, index, &max_encrypted);
        encrypted[index++] = base64_table[base64[0]];
        encrypted[index++] = base64_table[base64[1]];
        encrypted[index++] = base64_table[base64[2]];
        encrypted[index++] = base64_table[base64[3]];
    }

    padding_append(bytes, encrypted, index);

    encrypted[index] = '\0';

    return encrypted;
}

static char *base64_decode(const char *encrypted, const unsigned int bytes)
{
    int index = 0;
    int padding_num = 0;
    uint8_t base64[4] = {0};

    char *unencrypted = (char *)malloc(max_unencrypted * sizeof(char));
    if(NULL == unencrypted) {
        printf("malloc error err(%d:%s)", errno, strerror(errno));
        return NULL;
    }

    if ('=' == encrypted[bytes-2]) {
        padding_num = 2;
    }
    else if ('=' == encrypted[bytes-1]) {
        padding_num = 1;
    }

    for (int i = 0; i < bytes; i+=4) {
        for (int k = 0; k < 4 && (i + k) < (bytes - padding_num); k++) {
            base64[k] = char_to_base64(encrypted[i+k]);
            if (-1 == base64[k]) {
                printf("非法字符\n");
                return NULL;
            }
        }

        uint8_t *ascii = generate_ascii(base64);
        unencrypted = no_overflow(unencrypted, 3, index, &max_unencrypted);
        unencrypted[index++] = ascii[0];
        unencrypted[index++] = ascii[1];
        unencrypted[index++] = ascii[2];
    }

    padding_remove(padding_num, unencrypted, index);

    unencrypted[index] = '\0';

    return unencrypted;
}

int main(int argc, char *argv[])
{
    if (2 != argc) {
        printf("请输入要加密的明文！");
        return -1;
    }

    char *encrypted = base64_encode(argv[1], strlen(argv[1]));
    printf("base64加密: %s\n", encrypted);

    char *unencrypted = base64_decode(encrypted, strlen(encrypted));
    printf("base64解密: %s\n", unencrypted);

    free(encrypted);
    encrypted = NULL;
    free(unencrypted);
    unencrypted = NULL;

    return 0;
}