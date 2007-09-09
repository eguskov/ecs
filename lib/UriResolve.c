/*
 * uriparser - RFC 3986 URI parsing library
 *
 * Copyright (C) 2007, Weijia Song <songweijia@gmail.com>
 * Copyright (C) 2007, Sebastian Pipping <webmaster@hartwork.org>
 * All rights reserved.
 *
 * Redistribution  and use in source and binary forms, with or without
 * modification,  are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions   of  source  code  must  retain  the   above
 *       copyright  notice, this list of conditions and the  following
 *       disclaimer.
 *
 *     * Redistributions  in  binary  form must  reproduce  the  above
 *       copyright  notice, this list of conditions and the  following
 *       disclaimer   in  the  documentation  and/or  other  materials
 *       provided with the distribution.
 *
 *     * Neither  the name of the <ORGANIZATION> nor the names of  its
 *       contributors  may  be  used to endorse  or  promote  products
 *       derived  from  this software without specific  prior  written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT  NOT
 * LIMITED  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS
 * FOR  A  PARTICULAR  PURPOSE ARE DISCLAIMED. IN NO EVENT  SHALL  THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL,    SPECIAL,   EXEMPLARY,   OR   CONSEQUENTIAL   DAMAGES
 * (INCLUDING,  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES;  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT  LIABILITY,  OR  TORT (INCLUDING  NEGLIGENCE  OR  OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* What encodings are enabled? */
#include <uriparser/UriDefsConfig.h>
#if (!defined(URI_PASS_ANSI) && !defined(URI_PASS_UNICODE))
/* Include SELF twice */
# define URI_PASS_ANSI 1
# include "UriResolve.c"
# undef URI_PASS_ANSI
# define URI_PASS_UNICODE 1
# include "UriResolve.c"
# undef URI_PASS_UNICODE
#else
# ifdef URI_PASS_ANSI
#  include <uriparser/UriDefsAnsi.h>
# else
#  include <uriparser/UriDefsUnicode.h>
#  include <wchar.h>
# endif



#ifndef URI_DOXYGEN
# include <uriparser/Uri.h>
# include "UriCommon.h"
#endif



static UriBool URI_FUNC(CopyPath)(URI_TYPE(Uri) * dest, const URI_TYPE(Uri) * source);
static UriBool URI_FUNC(CopyAuthority)(URI_TYPE(Uri) * dest, const URI_TYPE(Uri) * source);
static UriBool URI_FUNC(MergePath)(URI_TYPE(Uri) * absWork, const URI_TYPE(Uri) * relAppend);
static UriBool URI_FUNC(FixAmbiguity)(URI_TYPE(Uri) * uri);



static const URI_CHAR * const URI_FUNC(ConstDot) = _UT(".");



/* Copies the path segment list from one URI to another. */
static URI_INLINE UriBool URI_FUNC(CopyPath)(URI_TYPE(Uri) * dest,
		const URI_TYPE(Uri) * source) {
	if (source->pathHead == NULL) {
		/* No path component */
		dest->pathHead = NULL;
		dest->pathTail = NULL;
	} else {
		/* Copy list but not the text contained */
		URI_TYPE(PathSegment) * sourceWalker = source->pathHead;
		URI_TYPE(PathSegment) * destPrev = NULL;
		do {
			URI_TYPE(PathSegment) * cur = malloc(sizeof(URI_TYPE(PathSegment)));
			if (cur == NULL) {
				/* Fix broken list */
				if (destPrev != NULL) {
					destPrev->next = NULL;
				}
				return URI_FALSE; /* Raises malloc error */
			}
			
			/* From this functions usage we know that *
			 * the dest URI cannot be uri->owner      */
			cur->text = sourceWalker->text;
			if (destPrev == NULL) {
				/* First segment ever */
				dest->pathHead = cur;
			} else {
				destPrev->next = cur;
			}
			destPrev = cur;
			sourceWalker = sourceWalker->next;
		} while (sourceWalker != NULL);
		dest->pathTail = destPrev;
		dest->pathTail->next = NULL;
	}

	dest->absolutePath = source->absolutePath;
	return URI_TRUE;
}



/* Copies the authority part of an URI over to another. */
static URI_INLINE UriBool URI_FUNC(CopyAuthority)(URI_TYPE(Uri) * dest,
		const URI_TYPE(Uri) * source) {
	/* From this functions usage we know that *
	 * the dest URI cannot be uri->owner      */

	/* Copy userInfo */
	dest->userInfo = source->userInfo;

	/* Copy hostText */
	dest->hostText = source->hostText;

	/* Copy hostData */
	if (source->hostData.ip4 != NULL) {
		dest->hostData.ip4 = malloc(sizeof(UriIp4));
		if (dest->hostData.ip4 == NULL) {
			return URI_FALSE; /* Raises malloc error */
		}
		*(dest->hostData.ip4) = *(source->hostData.ip4);
		dest->hostData.ip6 = NULL;
		dest->hostData.ipFuture.first = NULL;
		dest->hostData.ipFuture.afterLast = NULL;
	} else if (source->hostData.ip6 != NULL) {
		dest->hostData.ip4 = NULL;
		dest->hostData.ip6 = malloc(sizeof(UriIp6));
		if (dest->hostData.ip6 == NULL) {
			return URI_FALSE; /* Raises malloc error */
		}
		*(dest->hostData.ip6) = *(source->hostData.ip6);
		dest->hostData.ipFuture.first = NULL;
		dest->hostData.ipFuture.afterLast = NULL;
	} else {
		dest->hostData.ip4 = NULL;
		dest->hostData.ip6 = NULL;
		dest->hostData.ipFuture = source->hostData.ipFuture;
	}

	/* Copy portText */
	dest->portText = source->portText;

	return URI_TRUE;
}



/* Appends a relative URI to an absolute. The last path segement of
 * the absolute URI is replaced. */
static URI_INLINE UriBool URI_FUNC(MergePath)(URI_TYPE(Uri) * absWork,
		const URI_TYPE(Uri) * relAppend) {
	URI_TYPE(PathSegment) * sourceWalker;
	URI_TYPE(PathSegment) * destPrev;
	if (relAppend->pathHead == NULL) {
		return URI_TRUE;
	}

	/* Replace last segment ("" if trailing slash) with first of append chain */
	if (absWork->pathHead == NULL) {
		URI_TYPE(PathSegment) * const dup = malloc(sizeof(URI_TYPE(PathSegment)));
		if (dup == NULL) {
			return URI_FALSE; /* Raises malloc error */
		}
		dup->next = NULL;
		absWork->pathHead = dup;
		absWork->pathTail = dup;
	}
	absWork->pathTail->text.first = relAppend->pathHead->text.first;
	absWork->pathTail->text.afterLast = relAppend->pathHead->text.afterLast;

	/* Append all the others */
	sourceWalker = relAppend->pathHead->next;
	if (sourceWalker == NULL) {
		return URI_TRUE;
	}
	destPrev = absWork->pathTail;

	for (;;) {
		URI_TYPE(PathSegment) * const dup = malloc(sizeof(URI_TYPE(PathSegment)));
		if (dup == NULL) {
			destPrev->next = NULL;
			absWork->pathTail = destPrev;
			return URI_FALSE; /* Raises malloc error */
		}
		dup->text = sourceWalker->text;
		destPrev->next = dup;

		if (sourceWalker->next == NULL) {
			absWork->pathTail = dup;
			absWork->pathTail->next = NULL;
			break;
		}
		destPrev = dup;
		sourceWalker = sourceWalker->next;
	}

	return URI_TRUE;
}



static URI_INLINE UriBool URI_FUNC(FixAmbiguity)(URI_TYPE(Uri) * uri) {
	URI_TYPE(PathSegment) * segment;

	if ((!uri->absolutePath)
			|| (uri->pathHead == NULL)
			|| (uri->pathHead->text.afterLast != uri->pathHead->text.first)) {
		return URI_TRUE;
	}

	/* Insert "." segment in front */
	segment = malloc(1 * sizeof(URI_TYPE(PathSegment)));
	if (segment == NULL) {
		return URI_FALSE; /* Raises malloc error */
	}
	segment->next = uri->pathHead;
	segment->text.first = URI_FUNC(ConstDot);
	segment->text.afterLast = URI_FUNC(ConstDot) + 1;
	uri->pathHead = segment;

	return URI_TRUE;
}



int URI_FUNC(AddBaseUri)(URI_TYPE(Uri) * absDest,
		const URI_TYPE(Uri) * relSource,
		const URI_TYPE(Uri) * absBase) {
	if ((absDest == NULL) || (relSource == NULL) || (absBase == NULL)) {
		return URI_ERROR_NULL;
	}

	/* absBase absolute? */
	if (absBase->scheme.first == NULL) {
		return URI_ERROR_ADDBASE_REL_BASE;
	}

	URI_FUNC(ResetUri)(absDest);

	/* [01/32]	if defined(R.scheme) then */
				if (relSource->scheme.first != NULL) {
	/* [02/32]		T.scheme = R.scheme; */
					absDest->scheme = relSource->scheme;
	/* [03/32]		T.authority = R.authority; */
					if (!URI_FUNC(CopyAuthority)(absDest, relSource)) {
						return URI_ERROR_MALLOC;
					}
	/* [04/32]		T.path = remove_dot_segments(R.path); */
					if (!URI_FUNC(CopyPath)(absDest, relSource)) {
						return URI_ERROR_MALLOC;
					}
					if (!URI_FUNC(RemoveDotSegments)(absDest)) {
						return URI_ERROR_MALLOC;
					}
	/* [05/32]		T.query = R.query; */
					absDest->query = relSource->query;
	/* [06/32]	else */
				} else {
	/* [07/32]		if defined(R.authority) then */
					if (URI_FUNC(IsHostSet)(relSource)) {
	/* [08/32]			T.authority = R.authority; */
						if (!URI_FUNC(CopyAuthority)(absDest, relSource)) {
							return URI_ERROR_MALLOC;
						}
	/* [09/32]			T.path = remove_dot_segments(R.path); */
						if (!URI_FUNC(CopyPath)(absDest, relSource)) {
							return URI_ERROR_MALLOC;
						}
						if (!URI_FUNC(RemoveDotSegments)(absDest)) {
							return URI_ERROR_MALLOC;
						}
	/* [10/32]			T.query = R.query; */
						absDest->query = relSource->query;
	/* [11/32]		else */
					} else {
	/* [12/32]			if (R.path == "") then */
						if (relSource->pathHead == NULL) {
	/* [13/32]				T.path = Base.path; */
							if (!URI_FUNC(CopyPath)(absDest, absBase)) {
								return URI_ERROR_MALLOC;
							}
	/* [14/32]				if defined(R.query) then */
							if (relSource->query.first != NULL) {
	/* [15/32]					T.query = R.query; */
								absDest->query = relSource->query;
	/* [16/32]				else */
							} else {
	/* [17/32]					T.query = Base.query; */
								absDest->query = absBase->query;
	/* [18/32]				endif; */
							}
	/* [19/32]			else */
						} else {
	/* [20/32]				if (R.path starts-with "/") then */
							if (relSource->absolutePath) {
	/* [21/32]					T.path = remove_dot_segments(R.path); */
								if (!URI_FUNC(CopyPath)(absDest, relSource)) {
									return URI_ERROR_MALLOC;
								}
								if (!URI_FUNC(RemoveDotSegments)(absDest)) {
									return URI_ERROR_MALLOC;
								}
	/* [22/32]				else */
							} else {
	/* [23/32]					T.path = merge(Base.path, R.path); */
								if (!URI_FUNC(CopyPath)(absDest, absBase)) {
									return URI_ERROR_MALLOC;
								}
								if (!URI_FUNC(MergePath)(absDest, relSource)) {
									return URI_ERROR_MALLOC;
								}
	/* [24/32]					T.path = remove_dot_segments(T.path); */
								if (!URI_FUNC(RemoveDotSegments)(absDest)) {
									return URI_ERROR_MALLOC;
								}

								if (!URI_FUNC(FixAmbiguity)(absDest)) {
									return URI_ERROR_MALLOC;
								}
	/* [25/32]				endif; */
							}
	/* [26/32]				T.query = R.query; */
							absDest->query = relSource->query;
	/* [27/32]			endif; */
						}
	/* [28/32]			T.authority = Base.authority; */
					if (!URI_FUNC(CopyAuthority)(absDest, absBase)) {
						return URI_ERROR_MALLOC;
					}
	/* [29/32]		endif; */
					}
	/* [30/32]		T.scheme = Base.scheme; */
					absDest->scheme = absBase->scheme;
	/* [31/32]	endif; */
				}
	/* [32/32]	T.fragment = R.fragment; */
				absDest->fragment = relSource->fragment;

	return URI_SUCCESS;
}



#endif
