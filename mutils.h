/* $Id: mutils.h,v 1.2 2001/08/16 14:57:17 dsanta Exp $ */

/*
 * mutils: various utilities for Metro
 */

#ifndef _MUTILS_PROTO
#define _MUTILS_PROTO

#include <stddef.h>
#include <3dmodel.h>

#ifdef __cplusplus
#define BEGIN_DECL extern "C" {
#define END_DECL }
#else
#define BEGIN_DECL
#define END_DECL
#endif

BEGIN_DECL
#undef BEGIN_DECL

/* Same as malloc, but exits if out of memory. */
void * xmalloc(size_t size);

/* Same as calloc, but exits if out of memory. */
void * xcalloc(size_t nmemb, size_t size);

/* Same as realloc, but exits if out of memory. */
void * xrealloc(void *ptr, size_t size);

/* Same as free, but should be used to free every xmalloc, xcalloc and
 * xrealloc allocation (compatibility with some other versions of these
 * functions, such as mpatrol's). */
void xfree(void *ptr);

END_DECL
#undef END_DECL

#endif /* _MUTILS_PROTO */
