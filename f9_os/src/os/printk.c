#include <os\printk.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stm32f7xx.h>
#include <assert.h>

//#include "buf.h"
//#include "user.h"
//#include "tty.h"
struct tty;

#define LOGTIME
#ifdef LOGTIME
#include <time.h>
#include <sys\time.h>
#include <sys\times.h>

#endif
#define QUEUE_LEN 512
#define QUEUE_OK		0x0
#define	QUEUE_OVERFLOW		0x1
#define	QUEUE_EMPTY		0x2

/**
 * Queue control block
 */
struct queue_t {
	uint32_t top;		/*!< queue top */
	uint32_t end;		/*!< queue end */
	size_t size;		/*!< the size of the allocated queue */
	uint8_t data[QUEUE_LEN];		/*!< pointer to the starting position of buffer */
};
struct queue_t kqueue = { 0 ,0 , QUEUE_LEN };
static uint32_t queue_length(struct queue_t *queue)
{
	return (queue->end >= queue->top) ?
	       (queue->end - queue->top) :
	       (queue->size + queue->top - queue->end);
}

uint32_t queue_init(struct queue_t *queue, uint8_t *addr, size_t size)
{
	queue->top = 0;
	queue->end = 0;
	queue->size = size;
//	queue->data = addr;

	return QUEUE_OK;
}

uint32_t queue_push(struct queue_t *queue, uint8_t element)
{
	if (queue_length(queue) == queue->size)
		return QUEUE_OVERFLOW;

	++queue->end;

	if (queue->end == (queue->size - 1))
		queue->end = 0;

	queue->data[queue->end] = element;

	return QUEUE_OK;
}

int queue_is_empty(struct queue_t *queue)
{
	return queue_length(queue) == 0;
}

uint32_t queue_pop(struct queue_t *queue, uint8_t *element)
{
	if (queue_length(queue) == 0)
		return QUEUE_EMPTY;

	++queue->top;

	if (queue->top == (queue->size - 1))
		queue->top = 0;

	*element = queue->data[queue->top];

	return QUEUE_OK;
}


static void empty_flush() {
	//fflush(stdout);
}
static int empty_putchar(int c) {
	assert(queue_push(&kqueue,c)==QUEUE_OK);
	return c;
}
static int debug_putchar(int c) { return ITM_SendChar(c); }

static printk_options_t printk_options = 0;
static int (*_printk_putchar)(int) = empty_putchar;
static void (*_printk_flush)() = empty_flush;
#define HAS_OPTION(O) (printk_options & (O))

static void printk_flush() {
	if(_printk_putchar != empty_putchar && !queue_is_empty(&kqueue)){
		uint8_t ch;
		while(queue_pop(&kqueue,&ch)== QUEUE_OK) _printk_putchar(ch);
	}
	if(_printk_flush != NULL) _printk_flush();
}
static void push_char(int c) {
	while(queue_push(&kqueue,c)!=QUEUE_OK) printk_flush();
}
static int printk_putchar(int c){
	switch(c) {
	case '\r': break; // ignore
	case '\n':
		push_char('\r');
		push_char('\n');
		printk_flush();
		break;
	default:
		push_char(c);
		break;
	}
	return c;
}

typedef struct params_s {
    int len;
    int num1;
    int num2;
    char pad_character;
    int do_padding;
    int left_flag;
} params_t;

void printk_setup(int (*outchar)(int), void(*flush)(),printk_options_t options){
	if(empty_putchar == _printk_putchar && !queue_is_empty(&kqueue)){
		uint8_t ch;
		while(queue_pop(&kqueue,&ch)== QUEUE_OK) outchar(ch);
	} else {
		printk_flush();
	}
	_printk_flush = flush;
	_printk_putchar = outchar;
	printk_options = options;
}


void putsk(const char* str){
	while(*str)printk_putchar(*str++);
}
void writek(const uint8_t* data, size_t len){
	// ignores line options
	while(len--) printk_putchar((char)*data++);
}
#if 0

static void padding( const int l_flag, params_t *par)
{
    if (par->do_padding && l_flag && (par->len < par->num1))
        for (int i=par->len; i<par->num1; i++)
        	printk_putchar( par->pad_character);
}

static void outs( const char* lp, params_t *par)
{
    /* pad on left if needed                         */
    par->len = strlen( lp);
    padding( !(par->left_flag), par);
    /* Move string to the buffer                     */
    while (*lp && (par->num2)--)     putck( *lp++);

    /* Pad on right if needed                        */
    /* CR 439175 - elided next stmt. Seemed bogus.   */
    /* par->len = strlen( lp);                       */
    padding( par->left_flag, par);
}

/*---------------------------------------------------*/
/*                                                   */
/* This routine moves a number to the output buffer  */
/* as directed by the padding and positioning flags. */
/*                                                   */

void outnum( const long n, const long base, params_t *par)
{
	caddr_t cp;
    int negative;
    char outbuf[32];
    const char digits[] = "0123456789ABCDEF";
    unsigned long num;

    /* Check if number is negative                   */
    if (base == 10 && n < 0L) {
        negative = 1;
        num = -(n);
    }
    else{
        num = (n);
        negative = 0;
    }

    /* Build number (backwards) in outbuf            */
    cp = outbuf;
    par->len=0;
    do {
        *cp++ = digits[(int)(num % base)];
        ++par->len;
    } while ((num /= base) > 0);
    if (negative){
    	++par->len;
    	 *cp++ = '-';
    }
    *cp-- = 0;

    /* Move the converted number to the buffer and   */
    /* add in the padding where needed.              */
    padding( !(par->left_flag), par);
    while (cp >= outbuf)
    	printk_putchar( *cp--);
    padding( par->left_flag, par);
}
/*---------------------------------------------------*/
/*                                                   */
/* This routine gets a number from the format        */
/* string.                                           */


#define ISDIGIT(X)  ((X) >= '0' && (X) <= '9' )
static int getnum( const char** linep)
{
    int n;
    const char*  cp;

    n = 0;
    cp = *linep;
    while (ISDIGIT(*cp))
        n = n*10 + ((*cp++) - '0');
    *linep = cp;
    return(n);
}

/*
 * Print a character on console or users terminal.  If destination is
 * the console then the last MSGBUFS characters are saved in msgbuf for
 * inspection later.
 */
static void kputchar(int c, int flags, struct tty *tp)
{
	printk_putchar(c);

    extern int msgbufmapped;
    register struct msgbuf *mbp;

    if (panicstr)
        constty = NULL;
    if ((flags & TOCONS) && tp == NULL && constty) {
        tp = constty;
        flags |= TOTTY;
    }
    if ((flags & TOTTY) && tp && tputchar(c, tp) < 0 &&
        (flags & TOCONS) && tp == constty)
        constty = NULL;
    if ((flags & TOLOG) &&
        c != '\0' && c != '\r' && c != 0177 && msgbufmapped) {
        mbp = msgbufp;
        if (mbp->msg_magic != MSG_MAGIC) {
            bzero((caddr_t)mbp, sizeof(*mbp));
            mbp->msg_magic = MSG_MAGIC;
        }
        mbp->msg_bufc[mbp->msg_bufx++] = c;
        if (mbp->msg_bufx < 0 || mbp->msg_bufx >= MSG_BSIZE)
            mbp->msg_bufx = 0;
        /* If the buffer is full, keep the most recent data. */
        if (mbp->msg_bufr == mbp->msg_bufx) {
            if (++mbp->msg_bufr >= MSG_BSIZE)
                mbp->msg_bufr = 0;
        }
    }
    if ((flags & TOCONS) && constty == NULL && c != '\0')
        (*v_putc)(c);

}
#endif
/*
 * Put a number (base <= 16) in a buffer in reverse order; return an
 * optional length and a pointer to the NULL terminated (preceded?)
 * buffer.
 */
static char * ksprintn(uint32_t ul, int base,int* lenp)
{                   /* A long in base 8, plus NULL. */
    static char buf[sizeof(long) * 8 / 3 + 2];
    register char *p;

    p = buf;
    do {
        *++p = "0123456789abcdef"[ul % base];
    } while (ul /= base);
    if (lenp)
        *lenp = p - buf;
    return (p);
}
static void kprintnumber(uint32_t ul, int base, int width, int max, int pad_begin,int pad_end) {
	int tmp;
	const char* p = ksprintn(ul, base, &tmp);
    if (width && (width -= tmp) > 0) while (width-- > 0 && max-- > 0) printk_putchar(pad_begin);
    while (max-- > 0){
    	if(*p) printk_putchar(*p--);
    	else printk_putchar(pad_end);
    }
}

#ifdef LOGTIME
void print_timestamp(struct timeval * tv) {
	printk_putchar('[');
	kprintnumber(tv->tv_sec,10,4,4,' ',' ');
	printk_putchar('.');
	kprintnumber(tv->tv_usec,10,0,4,' ','0');
	printk_putchar(']');
	printk_putchar(' ');
}
#else
#define print_timestamp(TV)
#endif
/*
 * Scaled down version of printf(3).
 *
 * Two additional formats:
 *
 * The format %b is supported to decode error registers.
 * Its usage is:
 *
 *  printf("reg=%b\n", regval, "<base><arg>*");
 *
 * where <base> is the output base expressed as a control character, e.g.
 * \10 gives octal; \20 gives hex.  Each arg is a sequence of characters,
 * the first of which gives the bit number to be inspected (origin 1), and
 * the next characters (up to a control character, i.e. a character <= 32),
 * give the name of the register.  Thus:
 *
 *  kprintf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 *
 * would produce output:
 *
 *  reg=3<BITTWO,BITONE>
 *
 * The format %r passes an additional format string and argument list
 * recursively.  Its usage is:
 *
 * fn(char *fmt, ...)
 * {
 *  va_list ap;
 *  va_start(ap, fmt);
 *  printf("prefix: %r: suffix\n", fmt, ap);
 *  va_end(ap);
 * }
 *
 * Space or zero padding and a field width are supported for the numeric
 * formats only.
 */
#define ISEOL(c) ((c) == '\n' || (c) == '\r')
void kprintf(const char *fmt, int flags, struct tty *tp, va_list ap)
{
    register char *p, *q;
    register int ch, n;
    uint32_t ul;
    int base, lflag, tmp, width;
    char padc=' ';
#ifdef LOGTIME
    struct timeval time_stamp;
    gettimeofday(&time_stamp, NULL);
#endif
    for (;;) {
        padc = ' ';
        width = 0;
        while ((ch = *(uint8_t *)fmt++) != '%') {
        	if(ch == '\0') return;
        	if(ch == '\n') {
            	printk_putchar('\n');
            	print_timestamp(&time_stamp);
        	} else printk_putchar(ch);
        }
        lflag = 0;
reswitch:
	switch (ch = *(uint8_t *)fmt++) {
        case '0':
            padc = '0';
            goto reswitch;
        case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            for (width = 0;; ++fmt) {
                width = width * 10 + ch - '0';
                ch = *fmt;
                if (ch < '0' || ch > '9')
                    break;
            }
            goto reswitch;
        case 'l':
            lflag = 1;
            goto reswitch;
        case 'b':
            ul = va_arg(ap, int);
            p = va_arg(ap, char *);
            q = ksprintn(ul, *p++, NULL);
            while ((ch = *q--))
            	printk_putchar(ch);

            if (!ul)
                break;

            for (tmp = 0; (n = *p++); ) {
                if (ul & (1 << (n - 1))) {
                	printk_putchar(tmp ? ',' : '<');
                    for (; (n = *p) > ' '; ++p)
                    	printk_putchar(n);
                    tmp = 1;
                } else
                    for (; *p > ' '; ++p)
                        continue;
            }
            if (tmp)
            	printk_putchar('>');
            break;
        case 'c':
        	printk_putchar(va_arg(ap, int));
            break;
        case 'r':
            p = va_arg(ap, char *);
            kprintf(p, flags, tp, va_arg(ap, va_list));
            break;
        case 't': // time stamp format, its two digts, first and second
        	printk_putchar('[');
        	kprintnumber(va_arg(ap, int),10,4,4,' ',' ');
        	printk_putchar('.');
        	kprintnumber(va_arg(ap, int),10,0,4,' ','0');
        	printk_putchar(']');
        	break;
        case 's':
            p = va_arg(ap, char *);
            while ((ch = *p++))
            	printk_putchar(ch);
            break;
        case 'd':
            ul = lflag ? va_arg(ap, long) : va_arg(ap, int);
            if ((long)ul < 0) {
            	printk_putchar('-');
                ul = -(long)ul;
            }
            base = 10;
            goto number;
        case 'o':
            ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            base = 8;
            goto number;
        case 'u':
            ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            base = 10;
            goto number;
        case 'p': // for pointers
            ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            base = 16;
            width = 8;
            padc = '0';
            printk_putchar('0');
            printk_putchar('x');
            goto number;
        case 'x':
            ul = lflag ? va_arg(ap, unsigned long) : va_arg(ap, unsigned int);
            base = 16;
            //no break
number:
		p = ksprintn(ul, base, &tmp);
            if (width && (width -= tmp) > 0)
                while (width--)
                	printk_putchar(padc);
            while ((ch = *p--))
            	printk_putchar(ch);
            break;
        default:
        	printk_putchar('%');
            if (lflag)
            	printk_putchar('l');
            /* FALLTHROUGH */
            // no break
        case '%':
        	printk_putchar(ch);
        	break;
        }
    }
}
void  MX_USART1_UART_Init();
extern int panic_usart(int c);

void panic(const char*fmt,...){
	printk_setup(panic_usart,NULL,0);
	//_printk_putchar = panic_usart;
	putsk("\r\n\r\n");
#ifdef LOGTIME
    struct timeval time_stamp;
    gettimeofday(&time_stamp, NULL);
    print_timestamp(&time_stamp);
#endif
	putsk("PANIC!\r\n");
    va_list ap;
	va_start(ap,fmt);
	kprintf(fmt,0,NULL,ap);
	va_end(ap);
}

void printk(const char* fmt, ...){
	va_list ap;
	va_start(ap,fmt);
	kprintf(fmt,0,NULL,ap);
	//vprintk(fmt,ap);
	va_end(ap);
}

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.
 */






