


#include<string.h>

#define PROP_NAME_MAX   32
#define PROP_VALUE_MAX  92
typedef struct prop_area prop_area;
typedef struct prop_info prop_info;
struct prop_info {
    char name[PROP_NAME_MAX];
    unsigned volatile serial;
    char value[PROP_VALUE_MAX];
};

struct prop_area {
    unsigned volatile count;
    unsigned volatile serial;
    unsigned magic;
    unsigned version;
    unsigned reserved[4];
    unsigned toc[1];
};

#define SERIAL_VALUE_LEN(serial) ((serial) >> 24)
#define SERIAL_DIRTY(serial) ((serial) & 1)
#define TOC_NAME_LEN(toc)       ((toc) >> 24)
#define TOC_TO_INFO(area, toc)  ((prop_info*) (((char*) area) + ((toc) & 0xFFFFFF)))
#define FUTEX_PRIVATE_FLAG  128
#define FUTEX_WAIT 0
#define FUTEX_WAIT_PRIVATE  (FUTEX_WAIT|FUTEX_PRIVATE_FLAG)

static unsigned dummy_props = 0;
prop_area *__system_property_area__ = (prop_area*) &dummy_props;
#if 0
int __futex_syscall4(void *ftx, int op, int val, const struct timespec *timeout)
{
    return futex(ftx, op, val, (void *)timeout, 0, 0);
}

int  __futex_wait_ex(void *ftx, int pshared, int val, const struct timespec *timeout)
{
    return __futex_syscall4(ftx, pshared ? FUTEX_WAIT : FUTEX_WAIT_PRIVATE, val, timeout);
}
#endif
const prop_info *__system_property_find(const char *name)
{
    prop_area *pa = __system_property_area__;
    unsigned count = pa->count;
    unsigned *toc = pa->toc;
    unsigned len = strlen(name);
    prop_info *pi;

    while(count--) {
        unsigned entry = *toc++;
        if(TOC_NAME_LEN(entry) != len) continue;

        pi = TOC_TO_INFO(pa, entry);
        if(memcmp(name, pi->name, len)) continue;

        return pi;
    }

    return 0;
}


int __system_property_read(const prop_info *pi, char *name, char *value)
{
    unsigned serial, len;

    for(;;) {
        serial = pi->serial;
		#if 0
        while(SERIAL_DIRTY(serial)) {
            __futex_wait((void *)&pi->serial, serial, 0);
            serial = pi->serial;
        }
		#endif
        len = SERIAL_VALUE_LEN(serial);
        memcpy(value, pi->value, len + 1);
        if(serial == pi->serial) {
            if(name != 0) {
                strcpy(name, pi->name);
            }
            return len;
        }
    }
}

int __system_property_get(const char* name, char* value) {
  const prop_info* pi = __system_property_find(name);

  if (pi != 0) {
    return __system_property_read(pi, nullptr, value);
  } else {
    value[0] = 0;
    return 0;
  }
}

int property_get(const char *key, char *value, const char *default_value)
{
    int len;

    len = __system_property_get(key, value);
    if(len > 0) {
        return len;
    }

    if(default_value) {
        len = strlen(default_value);
        memcpy(value, default_value, len + 1);
    }
    return len;
}



