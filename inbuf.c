#include <stdlib.h>

#include "inbuf.h"

#define BUFSIZE 16384
#define ALLOCADD 4096



struct inbuf *
inbuf_new(FILE *f, long length)
{
    struct inbuf *inb;

    if ((inb=(struct inbuf *)malloc(sizeof(struct inbuf))) == NULL)
	return NULL;

    inb->bsize = BUFSIZE;
    inb->allocsize = inb->bsize + ALLOCADD;
    if ((inb->b=(char *)malloc(inb->allocsize)) == NULL) {
	free(inb);
	return NULL;
    }

    inb->f = f;
    inb->cur = inb->first = inb->last = 0;
    inb->keep = -1;
    inb->length = length;
    inb->eof = 0;

    return inb;
}



void
inbuf_free(struct inbuf *inb)
{
    free(inb->b);
    free(inb);
}



int
inbuf_fgetc(long pos, struct inbuf *inb)
{
    int nskip, nread, first;
    int n, k, off, n2;

    if (pos < inb->first)
	return -2; /* data no longer available */
    else if (pos < inb->last)
	return inb->b[pos % inb->bsize];

    if (inb->eof)
	return EOF;
    if (inb->length != -1 && pos >= inb->length)
	return EOF; /* logical EOF */

    inb->first = ((inb->keep != -1 && inb->keep < pos)
		  ? inb->keep : pos);
    if (inb->first + inb->bsize < pos)
	return -3; /* buffer overflow */
    
    if (inb->first < inb->last) {
	first = inb->last;
	nskip = 0;
	nread = inb->first+inb->bsize - first;
    }
    else {
	first = inb->first;
	nskip = inb->first-inb->last;
	nread = inb->bsize;
    }
    if (inb->length != -1 && first + nread > inb->length)
	nread = inb->length - first;

    while (nskip > inb->bsize) {
	for (n=0; ((k=fread(inb->b, 1, inb->bsize, inb->f)) > 0
		   && (n+=k) < inb->bsize);)
	    ;
	if (k == 0) { /* physical EOF */
	    inb->eof = 1;
	    return EOF;
	}
	nskip -= inb->bsize;
    }

    first -= nskip;
    nread += nskip;

    off = first % inb->bsize;
    n = inb->bsize - off;
    if (n > nread)
	n = nread;
    for (n2=0; ((k=fread(inb->b+off+n2, 1, n-n2, inb->f)) > 0
		&& (n2+=k) < n);)
	;
    if (k == 0) { /* physical EOF */
	inb->eof = 1;
	nread = n = n2;
    }
    inb->last = first + n2;

    if (nread-n > 0) {
	n = nread-n;
	
	for (n2=0; ((k=fread(inb->b+n2, 1, n-n2, inb->f)) > 0
		    && (n2+=k) < n);)
	    ;
	if (k == 0) /* physical EOF */
	    inb->eof = 1;
	inb->last += n2;
    }
    if (pos > inb->last)
	return EOF;
    return inb->b[pos % inb->bsize];
}



#if 0
/* is a macro now */
int
inbuf_keep(long pos, struct inbuf *inb)
{
    inb->keep = pos;
}
#endif



#if 0
/* is a macro now */
int
inbuf_unkeep(struct inbuf *inb)
{
    inb->keep = -1;
}
#endif



int
inbuf_getlong(unsigned long *lp, long pos, struct inbuf *inb)
{
    int c;
    unsigned long l;

    if ((c = inbuf_getc(pos, inb)) < 0)
	return c;
    l = (c & 0xff) << 24;
    if ((c = inbuf_getc(pos+1, inb)) < 0)
	return c;
    l |= (c & 0xff) << 16;
    if ((c = inbuf_getc(pos+2, inb)) < 0)
	return c;
    l |= (c & 0xff) << 8;
    if ((c = inbuf_getc(pos+3, inb)) < 0)
	return c;
    l |= (c & 0xff);

    *lp = l;
    return 0;
}



int
inbuf_copy(unsigned char **b, long pos, long len, struct inbuf *inb)
{
    long keep;
    int n;

    if (pos < inb->first)
	return -2;

    keep = inb->keep;
    if (inb->keep == -1 || inb->keep > pos)
	inb->keep = pos;

    if (inbuf_getc(pos+len-1, inb) < 0) {
	len = inbuf_length(inb) - pos;
	inbuf_getc(pos+len-1, inb);
    }

    if ((pos / inb->bsize) != ((pos+len-1) / inb->bsize)) {
	n = (pos+len) % inb->bsize;
	if (inb->allocsize < inb->bsize+n) {
	    inb->allocsize = inb->bsize+n;
	    if ((inb->b=(char *)realloc(inb->b, inb->allocsize)) == NULL)
		return -1;
	}
	memcpy(inb->b+inb->bsize, inb->b, n);
    }

    *b = inb->b + (pos%inb->bsize);
    inb->keep = keep;
    return len;
}



int
inbuf_length(struct inbuf *inb)
{
    if (inb->eof)
	return inb->last;
    else
	return inb->length;
}
