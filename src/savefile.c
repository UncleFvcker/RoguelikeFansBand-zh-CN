#include "angband.h"
#include "savefile.h"

#include <assert.h>

int version_compare(version_ptr left, version_ptr right)
{
    if (left->major < right->major) return -1;
    else if (left->major > right->major) return 1;

    if (left->minor < right->minor) return -1;
    else if (left->minor > right->minor) return 1;

    if (left->patch < right->patch) return -1;
    else if (left->patch > right->patch) return 1;

    if (left->extra < right->extra) return -1;
    else if (left->extra > right->extra) return 1;

    return 0;
}

void savefile_normalize_version(version_ptr version)
{
    if (!version) return;

    /* Treat early 1.0.x headers as the original RoguelikeFansBand baseline. */
    if (version->major == 1 && version->minor == 0)
    {
        version->major = 1;
        version->minor = 0;
        version->patch = 0;
        version->extra = 0;
    }
}

bool savefile_is_older_than(savefile_ptr file, byte major, byte minor, byte patch, byte extra)
{
    version_t v;
    v.major = major;
    v.minor = minor;
    v.patch = patch;
    v.extra = extra;

    if (version_compare(&file->version, &v) < 0) 
        return TRUE;

    return FALSE;
}

savefile_ptr savefile_open_read(const char *name)
{
    savefile_ptr result = NULL;
    FILE *fff;
    int version[4];
    int i;
    
    safe_setuid_grab();
    fff = my_fopen(name, "rb");
    safe_setuid_drop();

    if (!fff) return NULL;

    for (i = 0; i < 4; i++)
    {
        version[i] = getc(fff);
        if (version[i] == EOF)
        {
            my_fclose(fff);
            return NULL;
        }
    }

    result = malloc(sizeof(savefile_t));
    if (!result)
    {
        my_fclose(fff);
        return NULL;
    }
    memset(result, 0, sizeof(savefile_t));
    result->file = fff;
    result->type = SAVEFILE_READ;

    result->version.major = (byte)version[0];
    result->version.minor = (byte)version[1];
    result->version.patch = (byte)version[2];
    result->version.extra = (byte)version[3];
    savefile_normalize_version(&result->version);
    result->xor_byte = 0;
    result->pos = 3;
    savefile_read_byte(result);
    if (savefile_is_error(result))
    {
        savefile_close(result);
        return NULL;
    }
    result->pos = 4;
    result->v_check = 0;
    result->x_check = 0;

    return result;
}

savefile_ptr savefile_open_write(const char *name)
{
    savefile_ptr result = NULL;
    FILE *fff = NULL;
    int   fd = -1;
    
    safe_setuid_grab();
    fd_kill(name);
    safe_setuid_drop();

    FILE_TYPE(FILE_TYPE_SAVE);
    safe_setuid_grab();
    fd = fd_make(name, 0644); /* WTF is 0644? */
    safe_setuid_drop();
    if (fd < 0) return NULL;

    fd_close(fd);

    safe_setuid_grab();
    fff = my_fopen(name, "wb");
    safe_setuid_drop();

    if (!fff) return NULL;

    result = malloc(sizeof(savefile_t));
    if (!result)
    {
        my_fclose(fff);
        return NULL;
    }
    memset(result, 0, sizeof(savefile_t));
    result->file = fff;
    result->type = SAVEFILE_WRITE;

    /* Dump the file header */
    putc(SAVEFILE_VER_MAJOR, result->file);
    putc(SAVEFILE_VER_MINOR, result->file);
    putc(SAVEFILE_VER_PATCH_ID, result->file);
    putc(SAVEFILE_VER_EXTRA, result->file);

    result->xor_byte = 0;
    result->pos = 3;
    savefile_write_byte(result, randint0(256));

    /* Reset the checksum */
    result->v_check = 0;
    result->x_check = 0;

    return result;
}

bool savefile_close(savefile_ptr file)
{
    errr err = -1;
    if (file->file)
        err = my_fclose(file->file);

    free(file);
    return err ? FALSE : TRUE;
}

bool savefile_is_error(savefile_ptr file)
{
    return file && file->error;
}

byte savefile_read_byte(savefile_ptr file)
{
    int c;
    byte b, v;

    assert(file->type == SAVEFILE_READ);

    c = getc(file->file);
    if (c == EOF)
    {
        file->error = TRUE;
        return 0;
    }

    b = (byte)(c & 0xFF);
    v = b ^ file->xor_byte;
    file->xor_byte = b;
    file->v_check += v;
    file->x_check += file->xor_byte;
    file->pos++;

    return v;
}

void savefile_write_byte(savefile_ptr file, byte v)
{
    assert(file->type == SAVEFILE_WRITE);

    file->xor_byte ^= v;
    putc(file->xor_byte, file->file);
    file->v_check += v;
    file->x_check += file->xor_byte;
    file->pos++;
}

bool savefile_read_bool(savefile_ptr file)
{
    return savefile_read_byte(file);
}

void savefile_write_bool(savefile_ptr file, bool v)
{
    savefile_write_byte(file, v);
}

u16b savefile_read_u16b(savefile_ptr file)
{
    u16b result;

    result = savefile_read_byte(file);
    result |= ((u16b)savefile_read_byte(file)) << 8;

    return result;
}

void savefile_write_u16b(savefile_ptr file, u16b v)
{
    savefile_write_byte(file, v & 0xFF);
    savefile_write_byte(file, (v >> 8) & 0xFF);
}

s16b savefile_read_s16b(savefile_ptr file)
{
    return (s16b)savefile_read_u16b(file);
}

void savefile_write_s16b(savefile_ptr file, s16b v)
{
    savefile_write_u16b(file, (u16b)v);
}

u32b savefile_read_u32b(savefile_ptr file)
{
    u32b result;

    result = savefile_read_byte(file);
    result |= ((u32b)savefile_read_byte(file)) << 8;
    result |= ((u32b)savefile_read_byte(file)) << 16;
    result |= ((u32b)savefile_read_byte(file)) << 24;

    return result;
}

void savefile_write_u32b(savefile_ptr file, u32b v)
{
    savefile_write_byte(file, v & 0xFF);
    savefile_write_byte(file, (v >> 8) & 0xFF);
    savefile_write_byte(file, (v >> 16) & 0xFF);
    savefile_write_byte(file, (v >> 24) & 0xFF);
}

s32b savefile_read_s32b(savefile_ptr file)
{
    return (s32b)savefile_read_u32b(file);
}

void savefile_write_s32b(savefile_ptr file, s32b v)
{
    savefile_write_u32b(file, (u32b)v);
}

void savefile_read_cptr(savefile_ptr file, char *buf, int max)
{
    int i = 0;

    if (max <= 0)
    {
        while (!savefile_is_error(file) && savefile_read_byte(file)) ;
        return;
    }

    for (i = 0; !savefile_is_error(file); i++)
    {
        byte c = savefile_read_byte(file);
        if (i < max - 1) buf[i] = c;
        if (!c) break;
    }
    buf[MIN(i, max - 1)] = '\0';
}

string_ptr savefile_read_string(savefile_ptr file)
{
    string_ptr s = string_alloc();
    while (!savefile_is_error(file))
    {
        byte c = savefile_read_byte(file);
        if (!c) break;
        string_append_c(s, c);
    }
    return s;
}

void savefile_write_cptr(savefile_ptr file, const char *buf)
{
    while (*buf)
        savefile_write_byte(file, *buf++);
    savefile_write_byte(file, 0);
}

void savefile_read_skip(savefile_ptr file, int cb)
{
    for (; cb > 0; cb--)
        savefile_read_byte(file);
}

