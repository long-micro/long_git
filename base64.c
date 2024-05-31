#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define padding '='

static const char base64_table[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int char_to_base64(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return 26 + c -'a';
    if (c >= '0' && c <= '9') return 52 + c - '0';
    if ('+' == c) return 62;
    if ('/' == c) return 63;
    return -1;
}

static int base64_encode(const char *unencrypted, const unsigned int bytes, char *encrypted)
{   
    int i,j;
    for (i = 0, j = 0; i < bytes; i+=3) {
        
        uint8_t byte1 = i < bytes ? unencrypted[i] : 0;
        uint8_t byte2 = (i+1) < bytes ? unencrypted[i+1] : 0;
        uint8_t byte3 = (i+2) < bytes ? unencrypted[i+2] : 0;

        encrypted[j++] = base64_table[byte1 >> 2];
        encrypted[j++] = base64_table[((byte1 & 0x03) << 4) + (byte2 >> 4)];
        encrypted[j++] = base64_table[((byte2 & 0x0F) << 2) + (byte3 >> 6)];
        encrypted[j++] = base64_table[byte3 & 0x3F];
    }

    if(2 == (bytes %3)) {
        encrypted[j-1] = padding;
    }

    if(1 == (bytes %3)) {
        encrypted[j-1] = padding;
        encrypted[j-2] = padding;
    }

    encrypted[j] = '\0';
    
    return 0;
}

static int base64_decode(const char *encrypted, const unsigned int bytes, char *unencrypted)
{
    uint8_t base64_num[4] = {0};
    uint8_t asccii_num[3] = {0};
    int padding_num = 0;

    if ('=' == encrypted[bytes-2]) {
        padding_num = 2;
    }
    else if ('=' == encrypted[bytes-1]) {
        padding_num = 1;
    }

    int i = 0, j = 0;
    for (i = 0; i < bytes; i+=4) {
        for (int k = 0; k < 4 && (i + k) < bytes - padding_num; k++) {
            base64_num[k] = char_to_base64(encrypted[i+k]);
            if (-1 == base64_num[k]) {
                printf("非法字符\n");
                return -1;
            }
        }

        asccii_num[0] = (base64_num[0] << 2) + (base64_num[1] >> 4);
        asccii_num[1] = (base64_num[1] << 4) + (base64_num[2] >> 2);
        asccii_num[2] = (base64_num[2] << 6) + (base64_num[3]);

        unencrypted[j++] = asccii_num[0];
        unencrypted[j++] = asccii_num[1];
        unencrypted[j++] = asccii_num[2];
    }

    if (2 == padding_num) {
        unencrypted[j - 2] = '\0';
    }

    if (1 == padding_num) {
        unencrypted[j - 1] = '\0';
    }

    unencrypted[j] = '\0';

    return 0;
}

int main(int argc, char *argv[])
{
    char original[] = "hello, world!";
    char encrypted[100] ={0};
    char unencrypted[100] ={0};

    base64_encode(original, strlen(original), encrypted);

    printf("%s\n", encrypted);

    base64_decode(encrypted, strlen(encrypted), unencrypted);

    printf("%s\n", unencrypted);

    return 0;
}