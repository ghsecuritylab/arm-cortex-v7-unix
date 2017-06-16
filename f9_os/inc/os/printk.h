#ifndef _PRINTK_K_
#define _PRINTK_K_

#include <stdint.h>
#include <stddef.h>

// generic simple print k that uses a function for char output

#include <stdint.h>
#include <stdarg.h>
#include <sys\types.h>



#ifdef __cplusplus
extern "C" {
#endif

typedef int (*printk_write_t)(const char*,size_t);
typedef enum printk_options_s {
	PRINTK_IGNORENEWLINE		= 0x01,	// any \n it sees it will not print
	PRINTK_IGNORERETURN			= 0x02,	// any \r it sees it will not print
	PRINTK_NEWLINEAFTERRETURN	= 0x04,	// newline it sees it will print \r
	PRINTK_RETURNAFTERNEWLINE	= 0x08,	// any \r it sees it will print \n
	PRINTK_FLUSHONNEWLINE		= 0x10,
} printk_options_t;

#define SERIAL_OPTIONS  (printk_options_t)(PRINTK_IGNORERETURN | PRINTK_NEWLINEAFTERRETURN | PRINTK_FLUSHONNEWLINE)
void printk(const char* msg, ...);
void putck(int c);
void putsk(const char* str);
void writek(const uint8_t* data, size_t len);
void vprintk(const char* msg, va_list va);

void printk_setup(printk_write_t out_write, printk_options_t options);
void kpanic(const char*fmt,...);
void panic_mode(); // sets the serial port to do blocking write, disable irq on evey printk

#ifdef __cplusplus
};
#endif

#endif
