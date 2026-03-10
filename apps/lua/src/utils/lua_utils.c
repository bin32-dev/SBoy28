#include <stddef.h>

int lua_utils_normalize_path(const char* in, char* out, size_t out_size)
{
    size_t i = 0;
    size_t j = 0;

    if (!in || !out || out_size == 0) {
        return -1;
    }

    while (in[i] != '\0' && j + 1 < out_size) {
        char c = in[i++];
        if (c == '\\') {
            c = '/';
        }

        if (j > 0 && out[j - 1] == '/' && c == '/') {
            continue;
        }

        out[j++] = c;
    }

    out[j] = '\0';
    return (in[i] == '\0') ? 0 : -1;
}
