// Uses the source code from the LibXDiff library used by git with modifications so that it can be used for image comparison
#pragma once

#include <vector>
#include <cstdlib>
#include <cassert>

template <class Data> class Diff
{
/** xmacros.h begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(XMACROS_H)
#define XMACROS_H




#define XDL_MIN(a, b) ((a) < (b) ? (a): (b))
#define XDL_MAX(a, b) ((a) > (b) ? (a): (b))
#define XDL_ADDBITS(v,b)	((v) + ((v) >> (b)))
#define XDL_MASKBITS(b)		((1UL << (b)) - 1)
#define XDL_HASHLONG(v,b)	(XDL_ADDBITS((unsigned long)(v), b) & XDL_MASKBITS(b))

#endif /* #if !defined(XMACROS_H) */

/** xmacros.h end */

/** xdiff.h begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(XDIFF_H)
#define XDIFF_H

/* xpparm_t.flags */
#define XDF_NEED_MINIMAL (1 << 0)

#define XDF_PATIENCE_DIFF (1 << 14)
#define XDF_HISTOGRAM_DIFF (1 << 15)
#define XDF_NONE_DIFF (1 << 16)
#define XDF_DIFF_ALGORITHM_MASK (XDF_PATIENCE_DIFF | XDF_HISTOGRAM_DIFF | XDF_NONE_DIFF)
#define XDF_DIFF_ALG(x) ((x) & XDF_DIFF_ALGORITHM_MASK)


typedef struct s_mmfile {
	char *ptr;
	long size;
} mmfile_t;

typedef struct s_xpparam {
	unsigned long flags;

	/* See Documentation/diff-options.txt. */
	char **anchors;
	size_t anchors_nr;
} xpparam_t;

#define xdl_malloc(x) malloc(x)
#define xdl_free(ptr) free(ptr)
#define xdl_realloc(ptr,x) realloc(ptr,x)


#endif /* #if !defined(XDIFF_H) */

/** xdiff.h end */

/** xtypes.h begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(XTYPES_H)
#define XTYPES_H



typedef struct s_chanode {
	struct s_chanode *next;
	long icurr;
} chanode_t;

typedef struct s_chastore {
	chanode_t *head, *tail;
	long isize, nsize;
	chanode_t *ancur;
	chanode_t *sncur;
	long scurr;
} chastore_t;

typedef struct s_xrecord {
	struct s_xrecord *next;
	char const *ptr;
	long size;
	unsigned long ha;
} xrecord_t;

typedef struct s_xdfile {
	chastore_t rcha;
	long nrec;
	unsigned int hbits;
	xrecord_t **rhash;
	long dstart, dend;
	xrecord_t **recs;
	char *rchg;
	long *rindex;
	long nreff;
	unsigned long *ha;
} xdfile_t;

typedef struct s_xdfenv {
	xdfile_t xdf1, xdf2;
} xdfenv_t;



#endif /* #if !defined(XTYPES_H) */

/** xtypes.h end */

/** xdiffi.h begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(XDIFFI_H)
#define XDIFFI_H


typedef struct s_diffdata {
	long nrec;
	unsigned long const *ha;
	long *rindex;
	char *rchg;
} diffdata_t;

typedef struct s_xdalgoenv {
	long mxcost;
	long snake_cnt;
	long heur_min;
} xdalgoenv_t;

#endif /* #if !defined(XDIFFI_H) */

/** xdiffi.h end */

/** xpatience.c begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003-2016 Davide Libenzi, Johannes E. Schindelin
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

/*
 * The basic idea of patience diff is to find lines that are unique in
 * both files.  These are intuitively the ones that we want to see as
 * common lines.
 *
 * The maximal ordered sequence of such line pairs (where ordered means
 * that the order in the sequence agrees with the order of the lines in
 * both files) naturally defines an initial set of common lines.
 *
 * Now, the algorithm tries to extend the set of common lines by growing
 * the line ranges where the files have identical lines.
 *
 * Between those common lines, the patience diff algorithm is applied
 * recursively, until no unique line pairs can be found; these line ranges
 * are handled by the well-known Myers algorithm.
 */

#define NON_UNIQUE ULONG_MAX

struct entry {
	unsigned long hash;
	/*
	 * 0 = unused entry, 1 = first line, 2 = second, etc.
	 * line2 is NON_UNIQUE if the line is not unique
	 * in either the first or the second file.
	 */
	unsigned long line1, line2;
	/*
	 * "next" & "previous" are used for the longest common
	 * sequence;
	 * initially, "next" reflects only the order in file1.
	 */
	struct entry *next, *previous;

	/*
	 * If 1, this entry can serve as an anchor. See
	 * Documentation/diff-options.txt for more information.
	 */
	unsigned anchor : 1;
};

/*
 * This is a hash mapping from line hash to line numbers in the first and
 * second file.
 */
struct hashmap {
	int nr, alloc;
	struct entry *entries, *first, *last;
	/* were common records found? */
	unsigned long has_matches;
	mmfile_t *file1, *file2;
	xdfenv_t *env;
	xpparam_t const *xpp;
};

static int is_anchor(xpparam_t const *xpp, const char *line)
{
	int i;
	for (i = 0; i < xpp->anchors_nr; i++) {
		if (!strncmp(line, xpp->anchors[i], strlen(xpp->anchors[i])))
			return 1;
	}
	return 0;
}

/* The argument "pass" is 1 for the first file, 2 for the second. */
void insert_record(xpparam_t const *xpp, int line, struct hashmap *map,
			  int pass)
{
	xrecord_t **records = pass == 1 ?
		map->env->xdf1.recs : map->env->xdf2.recs;
	xrecord_t *record = records[line - 1], *other;
	/*
	 * After xdl_prepare_env() (or more precisely, due to
	 * xdl_classify_record()), the "ha" member of the records (AKA lines)
	 * is _not_ the hash anymore, but a linearized version of it.  In
	 * other words, the "ha" member is guaranteed to start with 0 and
	 * the second record's ha can only be 0 or 1, etc.
	 *
	 * So we multiply ha by 2 in the hope that the hashing was
	 * "unique enough".
	 */
	int index = (int)((record->ha << 1) % map->alloc);

	while (map->entries[index].line1) {
		other = map->env->xdf1.recs[map->entries[index].line1 - 1];
		if (map->entries[index].hash != record->ha ||
				!xdl_recmatch(record->ptr, record->size,
					other->ptr, other->size,
					map->xpp->flags)) {
			if (++index >= map->alloc)
				index = 0;
			continue;
		}
		if (pass == 2)
			map->has_matches = 1;
		if (pass == 1 || map->entries[index].line2)
			map->entries[index].line2 = NON_UNIQUE;
		else
			map->entries[index].line2 = line;
		return;
	}
	if (pass == 2)
		return;
	map->entries[index].line1 = line;
	map->entries[index].hash = record->ha;
	map->entries[index].anchor = is_anchor(xpp, map->env->xdf1.recs[line - 1]->ptr);
	if (!map->first)
		map->first = map->entries + index;
	if (map->last) {
		map->last->next = map->entries + index;
		map->entries[index].previous = map->last;
	}
	map->last = map->entries + index;
	map->nr++;
}

/*
 * This function has to be called for each recursion into the inter-hunk
 * parts, as previously non-unique lines can become unique when being
 * restricted to a smaller part of the files.
 *
 * It is assumed that env has been prepared using xdl_prepare().
 */
int fill_hashmap(mmfile_t *file1, mmfile_t *file2,
		xpparam_t const *xpp, xdfenv_t *env,
		struct hashmap *result,
		int line1, int count1, int line2, int count2)
{
	result->file1 = file1;
	result->file2 = file2;
	result->xpp = xpp;
	result->env = env;

	/* We know exactly how large we want the hash map */
	result->alloc = count1 * 2;
	result->entries = (struct entry *)
		xdl_malloc(result->alloc * sizeof(struct entry));
	if (!result->entries)
		return -1;
	memset(result->entries, 0, result->alloc * sizeof(struct entry));

	/* First, fill with entries from the first file */
	while (count1--)
		insert_record(xpp, line1++, result, 1);

	/* Then search for matches in the second file */
	while (count2--)
		insert_record(xpp, line2++, result, 2);

	return 0;
}

/*
 * Find the longest sequence with a smaller last element (meaning a smaller
 * line2, as we construct the sequence with entries ordered by line1).
 */
static int binary_search(struct entry **sequence, int longest,
		struct entry *entry)
{
	int left = -1, right = longest;

	while (left + 1 < right) {
		int middle = left + (right - left) / 2;
		/* by construction, no two entries can be equal */
		if (sequence[middle]->line2 > entry->line2)
			right = middle;
		else
			left = middle;
	}
	/* return the index in "sequence", _not_ the sequence length */
	return left;
}

/*
 * The idea is to start with the list of common unique lines sorted by
 * the order in file1.  For each of these pairs, the longest (partial)
 * sequence whose last element's line2 is smaller is determined.
 *
 * For efficiency, the sequences are kept in a list containing exactly one
 * item per sequence length: the sequence with the smallest last
 * element (in terms of line2).
 */
static struct entry *find_longest_common_sequence(struct hashmap *map)
{
	struct entry **sequence = (struct entry **)xdl_malloc(map->nr * sizeof(struct entry *));
	int longest = 0, i;
	struct entry *entry;

	/*
	 * If not -1, this entry in sequence must never be overridden.
	 * Therefore, overriding entries before this has no effect, so
	 * do not do that either.
	 */
	int anchor_i = -1;

	for (entry = map->first; entry; entry = entry->next) {
		if (!entry->line2 || entry->line2 == NON_UNIQUE)
			continue;
		i = binary_search(sequence, longest, entry);
		entry->previous = i < 0 ? NULL : sequence[i];
		++i;
		if (i <= anchor_i)
			continue;
		sequence[i] = entry;
		if (entry->anchor) {
			anchor_i = i;
			longest = anchor_i + 1;
		} else if (i == longest) {
			longest++;
		}
	}

	/* No common unique lines were found */
	if (!longest) {
		xdl_free(sequence);
		return NULL;
	}

	/* Iterate starting at the last element, adjusting the "next" members */
	entry = sequence[longest - 1];
	entry->next = NULL;
	while (entry->previous) {
		entry->previous->next = entry;
		entry = entry->previous;
	}
	xdl_free(sequence);
	return entry;
}

int match(struct hashmap *map, int line1, int line2)
{
	xrecord_t *record1 = map->env->xdf1.recs[line1 - 1];
	xrecord_t *record2 = map->env->xdf2.recs[line2 - 1];
	return xdl_recmatch(record1->ptr, record1->size,
		record2->ptr, record2->size, map->xpp->flags);
}


int walk_common_sequence(struct hashmap *map, struct entry *first,
		int line1, int count1, int line2, int count2)
{
	int end1 = line1 + count1, end2 = line2 + count2;
	int next1, next2;

	for (;;) {
		/* Try to grow the line ranges of common lines */
		if (first) {
			next1 = first->line1;
			next2 = first->line2;
			while (next1 > line1 && next2 > line2 &&
					match(map, next1 - 1, next2 - 1)) {
				next1--;
				next2--;
			}
		} else {
			next1 = end1;
			next2 = end2;
		}
		while (line1 < next1 && line2 < next2 &&
				match(map, line1, line2)) {
			line1++;
			line2++;
		}

		/* Recurse */
		if (next1 > line1 || next2 > line2) {
			struct hashmap submap;

			memset(&submap, 0, sizeof(submap));
			if (patience_diff(map->file1, map->file2,
					map->xpp, map->env,
					line1, next1 - line1,
					line2, next2 - line2))
				return -1;
		}

		if (!first)
			return 0;

		while (first->next &&
				first->next->line1 == first->line1 + 1 &&
				first->next->line2 == first->line2 + 1)
			first = first->next;

		line1 = first->line1 + 1;
		line2 = first->line2 + 1;

		first = first->next;
	}
}

int fall_back_to_classic_diff(struct hashmap *map,
		int line1, int count1, int line2, int count2)
{
	xpparam_t xpp;
	xpp.flags = map->xpp->flags & ~XDF_DIFF_ALGORITHM_MASK;

	return xdl_fall_back_diff(map->env, &xpp,
				  line1, count1, line2, count2);
}

/*
 * Recursively find the longest common sequence of unique lines,
 * and if none was found, ask xdl_do_diff() to do the job.
 *
 * This function assumes that env was prepared with xdl_prepare_env().
 */
int patience_diff(mmfile_t *file1, mmfile_t *file2,
		xpparam_t const *xpp, xdfenv_t *env,
		int line1, int count1, int line2, int count2)
{
	struct hashmap map;
	struct entry *first;
	int result = 0;

	/* trivial case: one side is empty */
	if (!count1) {
		while(count2--)
			env->xdf2.rchg[line2++ - 1] = 1;
		return 0;
	} else if (!count2) {
		while(count1--)
			env->xdf1.rchg[line1++ - 1] = 1;
		return 0;
	}

	memset(&map, 0, sizeof(map));
	if (fill_hashmap(file1, file2, xpp, env, &map,
			line1, count1, line2, count2))
		return -1;

	/* are there any matching lines at all? */
	if (!map.has_matches) {
		while(count1--)
			env->xdf1.rchg[line1++ - 1] = 1;
		while(count2--)
			env->xdf2.rchg[line2++ - 1] = 1;
		xdl_free(map.entries);
		return 0;
	}

	first = find_longest_common_sequence(&map);
	if (first)
		result = walk_common_sequence(&map, first,
			line1, count1, line2, count2);
	else
		result = fall_back_to_classic_diff(&map,
			line1, count1, line2, count2);

	xdl_free(map.entries);
	return result;
}

int xdl_do_patience_diff(mmfile_t *file1, mmfile_t *file2,
		xpparam_t const *xpp, xdfenv_t *env)
{
	if (xdl_prepare_env(file1, file2, xpp, env) < 0)
		return -1;

	/* environment is cleaned up in xdl_diff() */
	return patience_diff(file1, file2, xpp, env,
			1, env->xdf1.nrec, 1, env->xdf2.nrec);
}

/** xpatience.c end */

/** xhistogram.c begin */

/*
 * Copyright (C) 2010, Google Inc.
 * and other copyright owners as documented in JGit's IP log.
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Distribution License v1.0 which
 * accompanies this distribution, is reproduced below, and is
 * available at http://www.eclipse.org/org/documents/edl-v10.php
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 * - Neither the name of the Eclipse Foundation, Inc. nor the
 *   names of its contributors may be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define MAX_PTR	UINT_MAX
#define MAX_CNT	UINT_MAX

#define LINE_END(n) (line##n + count##n - 1)
#define LINE_END_PTR(n) (*line##n + *count##n - 1)

struct record {
	unsigned int ptr, cnt;
	struct record* next;
};

struct histindex {
	struct record **records, /* an occurrence */
		** line_map; /* map of line to record chain */
	chastore_t rcha;
	unsigned int *next_ptrs;
	unsigned int table_bits,
		     records_size,
		     line_map_size;

	unsigned int max_chain_length,
		     key_shift,
		     ptr_shift;

	unsigned int cnt,
		     has_common;

	xdfenv_t *env;
	xpparam_t const *xpp;
};

struct region {
	unsigned int begin1, end1;
	unsigned int begin2, end2;
};

#define LINE_MAP(i, a) (i->line_map[(a) - i->ptr_shift])

#define NEXT_PTR(index, ptr) \
	(index->next_ptrs[(ptr) - index->ptr_shift])

#define CNT(index, ptr) \
	((LINE_MAP(index, ptr))->cnt)

#define REC(env, s, l) \
	(env->xdf##s.recs[l - 1])

int cmp_recs(xpparam_t const *xpp,
	xrecord_t *r1, xrecord_t *r2)
{
	return r1->ha == r2->ha &&
		xdl_recmatch(r1->ptr, r1->size, r2->ptr, r2->size,
			    xpp->flags);
}

#define CMP_ENV(xpp, env, s1, l1, s2, l2) \
	(cmp_recs(xpp, REC(env, s1, l1), REC(env, s2, l2)))

#define CMP(i, s1, l1, s2, l2) \
	(cmp_recs(i->xpp, REC(i->env, s1, l1), REC(i->env, s2, l2)))

#define TABLE_HASH(index, side, line) \
	XDL_HASHLONG((REC(index->env, side, line))->ha, index->table_bits)

int scanA(struct histindex *index, int line1, int count1)
{
	unsigned int ptr, tbl_idx;
	unsigned int chain_len;
	struct record **rec_chain, *rec;

	for (ptr = LINE_END(1); line1 <= static_cast<int>(ptr); ptr--) {
		tbl_idx = TABLE_HASH(index, 1, ptr);
		rec_chain = index->records + tbl_idx;
		rec = *rec_chain;

		chain_len = 0;
		while (rec) {
			if (CMP(index, 1, rec->ptr, 1, ptr)) {
				/*
				 * ptr is identical to another element. Insert
				 * it onto the front of the existing element
				 * chain.
				 */
				NEXT_PTR(index, ptr) = rec->ptr;
				rec->ptr = ptr;
				/* cap rec->cnt at MAX_CNT */
				rec->cnt = XDL_MIN(MAX_CNT, rec->cnt + 1);
				LINE_MAP(index, ptr) = rec;
				goto continue_scan;
			}

			rec = rec->next;
			chain_len++;
		}

		if (chain_len == index->max_chain_length)
			return -1;

		/*
		 * This is the first time we have ever seen this particular
		 * element in the sequence. Construct a new chain for it.
		 */
		if (!(rec = reinterpret_cast<struct record *>(xdl_cha_alloc(&index->rcha))))
			return -1;
		rec->ptr = ptr;
		rec->cnt = 1;
		rec->next = *rec_chain;
		*rec_chain = rec;
		LINE_MAP(index, ptr) = rec;

continue_scan:
		; /* no op */
	}

	return 0;
}

int try_lcs(struct histindex *index, struct region *lcs, int b_ptr,
	int line1, int count1, int line2, int count2)
{
	unsigned int b_next = b_ptr + 1;
	struct record *rec = index->records[TABLE_HASH(index, 2, b_ptr)];
	unsigned int as, ae, bs, be, np, rc;
	int should_break;

	for (; rec; rec = rec->next) {
		if (rec->cnt > index->cnt) {
			if (!index->has_common)
				index->has_common = CMP(index, 1, rec->ptr, 2, b_ptr);
			continue;
		}

		as = rec->ptr;
		if (!CMP(index, 1, as, 2, b_ptr))
			continue;

		index->has_common = 1;
		for (;;) {
			should_break = 0;
			np = NEXT_PTR(index, as);
			bs = b_ptr;
			ae = as;
			be = bs;
			rc = rec->cnt;

			while (line1 < static_cast<int>(as) && line2 < static_cast<int>(bs)
				&& CMP(index, 1, as - 1, 2, bs - 1)) {
				as--;
				bs--;
				if (1 < rc)
					rc = XDL_MIN(rc, CNT(index, as));
			}
			while (static_cast<int>(ae) < LINE_END(1) && static_cast<int>(be) < LINE_END(2)
				&& CMP(index, 1, ae + 1, 2, be + 1)) {
				ae++;
				be++;
				if (1 < rc)
					rc = XDL_MIN(rc, CNT(index, ae));
			}

			if (b_next <= be)
				b_next = be + 1;
			if (lcs->end1 - lcs->begin1 < ae - as || rc < index->cnt) {
				lcs->begin1 = as;
				lcs->begin2 = bs;
				lcs->end1 = ae;
				lcs->end2 = be;
				index->cnt = rc;
			}

			if (np == 0)
				break;

			while (np <= ae) {
				np = NEXT_PTR(index, np);
				if (np == 0) {
					should_break = 1;
					break;
				}
			}

			if (should_break)
				break;

			as = np;
		}
	}
	return b_next;
}

int fall_back_to_classic_diff(xpparam_t const *xpp, xdfenv_t *env,
		int line1, int count1, int line2, int count2)
{
	xpparam_t xpparam;
	xpparam.flags = xpp->flags & ~XDF_DIFF_ALGORITHM_MASK;

	return xdl_fall_back_diff(env, &xpparam,
				  line1, count1, line2, count2);
}

static inline void free_index(struct histindex *index)
{
	xdl_free(index->records);
	xdl_free(index->line_map);
	xdl_free(index->next_ptrs);
	xdl_cha_free(&index->rcha);
}

int find_lcs(xpparam_t const *xpp, xdfenv_t *env,
		    struct region *lcs,
		    int line1, int count1, int line2, int count2)
{
	int b_ptr;
	int sz, ret = -1;
	struct histindex index;

	memset(&index, 0, sizeof(index));

	index.env = env;
	index.xpp = xpp;

	index.records = NULL;
	index.line_map = NULL;
	/* in case of early xdl_cha_free() */
	index.rcha.head = NULL;

	index.table_bits = xdl_hashbits(count1);
	sz = index.records_size = 1 << index.table_bits;
	sz *= sizeof(struct record *);
	if (!(index.records = (struct record **) xdl_malloc(sz)))
		goto cleanup;
	memset(index.records, 0, sz);

	sz = index.line_map_size = count1;
	sz *= sizeof(struct record *);
	if (!(index.line_map = (struct record **) xdl_malloc(sz)))
		goto cleanup;
	memset(index.line_map, 0, sz);

	sz = index.line_map_size;
	sz *= sizeof(unsigned int);
	if (!(index.next_ptrs = (unsigned int *) xdl_malloc(sz)))
		goto cleanup;
	memset(index.next_ptrs, 0, sz);

	/* lines / 4 + 1 comes from xprepare.c:xdl_prepare_ctx() */
	if (xdl_cha_init(&index.rcha, sizeof(struct record), count1 / 4 + 1) < 0)
		goto cleanup;

	index.ptr_shift = line1;
	index.max_chain_length = 64;

	if (scanA(&index, line1, count1))
		goto cleanup;

	index.cnt = index.max_chain_length + 1;

	for (b_ptr = line2; b_ptr <= LINE_END(2); )
		b_ptr = try_lcs(&index, lcs, b_ptr, line1, count1, line2, count2);

	if (index.has_common && index.max_chain_length < index.cnt)
		ret = 1;
	else
		ret = 0;

cleanup:
	free_index(&index);
	return ret;
}

int histogram_diff(xpparam_t const *xpp, xdfenv_t *env,
	int line1, int count1, int line2, int count2)
{
	struct region lcs;
	int lcs_found;
	int result;
redo:
	result = -1;

	if (count1 <= 0 && count2 <= 0)
		return 0;

	if (LINE_END(1) >= MAX_PTR)
		return -1;

	if (!count1) {
		while(count2--)
			env->xdf2.rchg[line2++ - 1] = 1;
		return 0;
	} else if (!count2) {
		while(count1--)
			env->xdf1.rchg[line1++ - 1] = 1;
		return 0;
	}

	memset(&lcs, 0, sizeof(lcs));
	lcs_found = find_lcs(xpp, env, &lcs, line1, count1, line2, count2);
	if (lcs_found < 0)
		goto out;
	else if (lcs_found)
		result = fall_back_to_classic_diff(xpp, env, line1, count1, line2, count2);
	else {
		if (lcs.begin1 == 0 && lcs.begin2 == 0) {
			while (count1--)
				env->xdf1.rchg[line1++ - 1] = 1;
			while (count2--)
				env->xdf2.rchg[line2++ - 1] = 1;
			result = 0;
		} else {
			result = histogram_diff(xpp, env,
						line1, lcs.begin1 - line1,
						line2, lcs.begin2 - line2);
			if (result)
				goto out;
			/*
			 * result = histogram_diff(xpp, env,
			 *            lcs.end1 + 1, LINE_END(1) - lcs.end1,
			 *            lcs.end2 + 1, LINE_END(2) - lcs.end2);
			 * but let's optimize tail recursion ourself:
			*/
			count1 = LINE_END(1) - lcs.end1;
			line1 = lcs.end1 + 1;
			count2 = LINE_END(2) - lcs.end2;
			line2 = lcs.end2 + 1;
			goto redo;
		}
	}
out:
	return result;
}

int xdl_do_histogram_diff(mmfile_t *file1, mmfile_t *file2,
	xpparam_t const *xpp, xdfenv_t *env)
{
	if (xdl_prepare_env(file1, file2, xpp, env) < 0)
		return -1;

	return histogram_diff(xpp, env,
		env->xdf1.dstart + 1, env->xdf1.dend - env->xdf1.dstart + 1,
		env->xdf2.dstart + 1, env->xdf2.dend - env->xdf2.dstart + 1);
}

/** xhistogram.c end */

/** xnone.c end */

int none_diff(mmfile_t *file1, mmfile_t *file2,
		xpparam_t const *xpp, xdfenv_t *env,
		int line1, int count1, int line2, int count2)
{
	while(count1 != 0 && count2 != 0) {
		xrecord_t *rec1 = env->xdf1.recs[line1 - 1];
		xrecord_t *rec2 = env->xdf2.recs[line2 - 1];
		int match = xdl_recmatch(
			rec1->ptr, rec1->size,
			rec2->ptr, rec2->size,
			xpp->flags);
		env->xdf1.rchg[line1++ - 1] = !match;
		env->xdf2.rchg[line2++ - 1] = !match;
		count1--;
		count2--;
	}

	if (!count1) {
		while(count2--)
			env->xdf2.rchg[line2++ - 1] = 1;
	} else if (!count2) {
		while(count1--)
			env->xdf1.rchg[line1++ - 1] = 1;
	}
	return 0;
}

int xdl_do_none_diff(mmfile_t *file1, mmfile_t *file2,
		xpparam_t const *xpp, xdfenv_t *env)
{
	if (xdl_prepare_env(file1, file2, xpp, env) < 0)
		return -1;

	/* environment is cleaned up in xdl_diff() */
	return none_diff(file1, file2, xpp, env,
			1, env->xdf1.nrec, 1, env->xdf2.nrec);
}

/** xnone.c end */

/** xutils.c begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003	Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

long xdl_bogosqrt(long n) {
	long i;

	/*
	 * Classical integer square root approximation using shifts.
	 */
	for (i = 1; n > 0; n >>= 2)
		i <<= 1;

	return i;
}


void *xdl_mmfile_first(mmfile_t *mmf, long *size)
{
	*size = mmf->size;
	return mmf->ptr;
}


long xdl_mmfile_size(mmfile_t *mmf)
{
	return mmf->size;
}


int xdl_cha_init(chastore_t *cha, long isize, long icount) {

	cha->head = cha->tail = NULL;
	cha->isize = isize;
	cha->nsize = icount * isize;
	cha->ancur = cha->sncur = NULL;
	cha->scurr = 0;

	return 0;
}


static void xdl_cha_free(chastore_t *cha) {
	chanode_t *cur, *tmp;

	for (cur = cha->head; (tmp = cur) != NULL;) {
		cur = cur->next;
		xdl_free(tmp);
	}
}


void *xdl_cha_alloc(chastore_t *cha) {
	chanode_t *ancur;
	void *data;

	if (!(ancur = cha->ancur) || ancur->icurr == cha->nsize) {
		if (!(ancur = (chanode_t *) xdl_malloc(sizeof(chanode_t) + cha->nsize))) {

			return NULL;
		}
		ancur->icurr = 0;
		ancur->next = NULL;
		if (cha->tail)
			cha->tail->next = ancur;
		if (!cha->head)
			cha->head = ancur;
		cha->tail = ancur;
		cha->ancur = ancur;
	}

	data = (char *) ancur + sizeof(chanode_t) + ancur->icurr;
	ancur->icurr += cha->isize;

	return data;
}

long xdl_guess_lines(int pass, mmfile_t *mf, long sample) {
	long nl = 0, size, tsize = 0;
	char const *data, *cur, *top;
	const Data& dat = pass == 1 ? m_data1 : m_data2;

	if ((cur = data = reinterpret_cast<const char *>(xdl_mmfile_first(mf, &size))) != NULL) {
		for (top = data + size; nl < sample && cur < top; ) {
			nl++;
			cur = dat.next(cur);
		}
		tsize += (long) (cur - data);
	}

	if (nl && tsize)
		nl = xdl_mmfile_size(mf) / (tsize / nl);

	return nl + 1;
}


int xdl_recmatch(const char *l1, long s1, const char *l2, long s2, long flags)
{
	return m_data1.equals(l1, s1, l2, s2);
}


unsigned int xdl_hashbits(unsigned int size) {
	unsigned int val = 1, bits = 0;

	for (; val < size && bits < CHAR_BIT * sizeof(unsigned int); val <<= 1, bits++);
	return bits ? bits: 1;
}

int xdl_fall_back_diff(xdfenv_t *diff_env, xpparam_t const *xpp,
		int line1, int count1, int line2, int count2)
{
	/*
	 * This probably does not work outside Git, since
	 * we have a very simple mmfile structure.
	 *
	 * Note: ideally, we would reuse the prepared environment, but
	 * the libxdiff interface does not (yet) allow for diffing only
	 * ranges of lines instead of the whole files.
	 */
	mmfile_t subfile1, subfile2;
	xdfenv_t env;

	subfile1.ptr = (char *)diff_env->xdf1.recs[line1 - 1]->ptr;
	subfile1.size = static_cast<long>(diff_env->xdf1.recs[line1 + count1 - 2]->ptr +
		diff_env->xdf1.recs[line1 + count1 - 2]->size - subfile1.ptr);
	subfile2.ptr = (char *)diff_env->xdf2.recs[line2 - 1]->ptr;
	subfile2.size = static_cast<long>(diff_env->xdf2.recs[line2 + count2 - 2]->ptr +
		diff_env->xdf2.recs[line2 + count2 - 2]->size - subfile2.ptr);
	if (xdl_do_diff(&subfile1, &subfile2, xpp, &env) < 0)
		return -1;

	memcpy(diff_env->xdf1.rchg + line1 - 1, env.xdf1.rchg, count1);
	memcpy(diff_env->xdf2.rchg + line2 - 1, env.xdf2.rchg, count2);

	xdl_free_env(&env);

	return 0;
}

/** xutils.c end */

/** xdiffi.c begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003	Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#define XDL_MAX_COST_MIN 256
#define XDL_HEUR_MIN_COST 256
#define XDL_LINE_MAX (long)((1UL << (CHAR_BIT * sizeof(long) - 1)) - 1)
#define XDL_SNAKE_CNT 20
#define XDL_K_HEUR 4

typedef struct s_xdpsplit {
	long i1, i2;
	int min_lo, min_hi;
} xdpsplit_t;

/*
 * See "An O(ND) Difference Algorithm and its Variations", by Eugene Myers.
 * Basically considers a "box" (off1, off2, lim1, lim2) and scan from both
 * the forward diagonal starting from (off1, off2) and the backward diagonal
 * starting from (lim1, lim2). If the K values on the same diagonal crosses
 * returns the furthest point of reach. We might end up having to expensive
 * cases using this algorithm is full, so a little bit of heuristic is needed
 * to cut the search and to return a suboptimal point.
 */
static long xdl_split(unsigned long const *ha1, long off1, long lim1,
		      unsigned long const *ha2, long off2, long lim2,
		      long *kvdf, long *kvdb, int need_min, xdpsplit_t *spl,
		      xdalgoenv_t *xenv) {
	long dmin = off1 - lim2, dmax = lim1 - off2;
	long fmid = off1 - off2, bmid = lim1 - lim2;
	long odd = (fmid - bmid) & 1;
	long fmin = fmid, fmax = fmid;
	long bmin = bmid, bmax = bmid;
	long ec, d, i1, i2, prev1, best, dd, v, k;

	/*
	 * Set initial diagonal values for both forward and backward path.
	 */
	kvdf[fmid] = off1;
	kvdb[bmid] = lim1;

	for (ec = 1;; ec++) {
		int got_snake = 0;

		/*
		 * We need to extent the diagonal "domain" by one. If the next
		 * values exits the box boundaries we need to change it in the
		 * opposite direction because (max - min) must be a power of two.
		 * Also we initialize the external K value to -1 so that we can
		 * avoid extra conditions check inside the core loop.
		 */
		if (fmin > dmin)
			kvdf[--fmin - 1] = -1;
		else
			++fmin;
		if (fmax < dmax)
			kvdf[++fmax + 1] = -1;
		else
			--fmax;

		for (d = fmax; d >= fmin; d -= 2) {
			if (kvdf[d - 1] >= kvdf[d + 1])
				i1 = kvdf[d - 1] + 1;
			else
				i1 = kvdf[d + 1];
			prev1 = i1;
			i2 = i1 - d;
			for (; i1 < lim1 && i2 < lim2 && ha1[i1] == ha2[i2]; i1++, i2++);
			if (i1 - prev1 > xenv->snake_cnt)
				got_snake = 1;
			kvdf[d] = i1;
			if (odd && bmin <= d && d <= bmax && kvdb[d] <= i1) {
				spl->i1 = i1;
				spl->i2 = i2;
				spl->min_lo = spl->min_hi = 1;
				return ec;
			}
		}

		/*
		 * We need to extent the diagonal "domain" by one. If the next
		 * values exits the box boundaries we need to change it in the
		 * opposite direction because (max - min) must be a power of two.
		 * Also we initialize the external K value to -1 so that we can
		 * avoid extra conditions check inside the core loop.
		 */
		if (bmin > dmin)
			kvdb[--bmin - 1] = XDL_LINE_MAX;
		else
			++bmin;
		if (bmax < dmax)
			kvdb[++bmax + 1] = XDL_LINE_MAX;
		else
			--bmax;

		for (d = bmax; d >= bmin; d -= 2) {
			if (kvdb[d - 1] < kvdb[d + 1])
				i1 = kvdb[d - 1];
			else
				i1 = kvdb[d + 1] - 1;
			prev1 = i1;
			i2 = i1 - d;
			for (; i1 > off1 && i2 > off2 && ha1[i1 - 1] == ha2[i2 - 1]; i1--, i2--);
			if (prev1 - i1 > xenv->snake_cnt)
				got_snake = 1;
			kvdb[d] = i1;
			if (!odd && fmin <= d && d <= fmax && i1 <= kvdf[d]) {
				spl->i1 = i1;
				spl->i2 = i2;
				spl->min_lo = spl->min_hi = 1;
				return ec;
			}
		}

		if (need_min)
			continue;

		/*
		 * If the edit cost is above the heuristic trigger and if
		 * we got a good snake, we sample current diagonals to see
		 * if some of the, have reached an "interesting" path. Our
		 * measure is a function of the distance from the diagonal
		 * corner (i1 + i2) penalized with the distance from the
		 * mid diagonal itself. If this value is above the current
		 * edit cost times a magic factor (XDL_K_HEUR) we consider
		 * it interesting.
		 */
		if (got_snake && ec > xenv->heur_min) {
			for (best = 0, d = fmax; d >= fmin; d -= 2) {
				dd = d > fmid ? d - fmid: fmid - d;
				i1 = kvdf[d];
				i2 = i1 - d;
				v = (i1 - off1) + (i2 - off2) - dd;

				if (v > XDL_K_HEUR * ec && v > best &&
				    off1 + xenv->snake_cnt <= i1 && i1 < lim1 &&
				    off2 + xenv->snake_cnt <= i2 && i2 < lim2) {
					for (k = 1; ha1[i1 - k] == ha2[i2 - k]; k++)
						if (k == xenv->snake_cnt) {
							best = v;
							spl->i1 = i1;
							spl->i2 = i2;
							break;
						}
				}
			}
			if (best > 0) {
				spl->min_lo = 1;
				spl->min_hi = 0;
				return ec;
			}

			for (best = 0, d = bmax; d >= bmin; d -= 2) {
				dd = d > bmid ? d - bmid: bmid - d;
				i1 = kvdb[d];
				i2 = i1 - d;
				v = (lim1 - i1) + (lim2 - i2) - dd;

				if (v > XDL_K_HEUR * ec && v > best &&
				    off1 < i1 && i1 <= lim1 - xenv->snake_cnt &&
				    off2 < i2 && i2 <= lim2 - xenv->snake_cnt) {
					for (k = 0; ha1[i1 + k] == ha2[i2 + k]; k++)
						if (k == xenv->snake_cnt - 1) {
							best = v;
							spl->i1 = i1;
							spl->i2 = i2;
							break;
						}
				}
			}
			if (best > 0) {
				spl->min_lo = 0;
				spl->min_hi = 1;
				return ec;
			}
		}

		/*
		 * Enough is enough. We spent too much time here and now we collect
		 * the furthest reaching path using the (i1 + i2) measure.
		 */
		if (ec >= xenv->mxcost) {
			long fbest, fbest1, bbest, bbest1;

			fbest = fbest1 = -1;
			for (d = fmax; d >= fmin; d -= 2) {
				i1 = XDL_MIN(kvdf[d], lim1);
				i2 = i1 - d;
				if (lim2 < i2)
					i1 = lim2 + d, i2 = lim2;
				if (fbest < i1 + i2) {
					fbest = i1 + i2;
					fbest1 = i1;
				}
			}

			bbest = bbest1 = XDL_LINE_MAX;
			for (d = bmax; d >= bmin; d -= 2) {
				i1 = XDL_MAX(off1, kvdb[d]);
				i2 = i1 - d;
				if (i2 < off2)
					i1 = off2 + d, i2 = off2;
				if (i1 + i2 < bbest) {
					bbest = i1 + i2;
					bbest1 = i1;
				}
			}

			if ((lim1 + lim2) - bbest < fbest - (off1 + off2)) {
				spl->i1 = fbest1;
				spl->i2 = fbest - fbest1;
				spl->min_lo = 1;
				spl->min_hi = 0;
			} else {
				spl->i1 = bbest1;
				spl->i2 = bbest - bbest1;
				spl->min_lo = 0;
				spl->min_hi = 1;
			}
			return ec;
		}
	}
}


/*
 * Rule: "Divide et Impera". Recursively split the box in sub-boxes by calling
 * the box splitting function. Note that the real job (marking changed lines)
 * is done in the two boundary reaching checks.
 */
int xdl_recs_cmp(diffdata_t *dd1, long off1, long lim1,
		 diffdata_t *dd2, long off2, long lim2,
		 long *kvdf, long *kvdb, int need_min, xdalgoenv_t *xenv) {
	unsigned long const *ha1 = dd1->ha, *ha2 = dd2->ha;

	/*
	 * Shrink the box by walking through each diagonal snake (SW and NE).
	 */
	for (; off1 < lim1 && off2 < lim2 && ha1[off1] == ha2[off2]; off1++, off2++);
	for (; off1 < lim1 && off2 < lim2 && ha1[lim1 - 1] == ha2[lim2 - 1]; lim1--, lim2--);

	/*
	 * If one dimension is empty, then all records on the other one must
	 * be obviously changed.
	 */
	if (off1 == lim1) {
		char *rchg2 = dd2->rchg;
		long *rindex2 = dd2->rindex;

		for (; off2 < lim2; off2++)
			rchg2[rindex2[off2]] = 1;
	} else if (off2 == lim2) {
		char *rchg1 = dd1->rchg;
		long *rindex1 = dd1->rindex;

		for (; off1 < lim1; off1++)
			rchg1[rindex1[off1]] = 1;
	} else {
		xdpsplit_t spl;
		spl.i1 = spl.i2 = 0;

		/*
		 * Divide ...
		 */
		if (xdl_split(ha1, off1, lim1, ha2, off2, lim2, kvdf, kvdb,
			      need_min, &spl, xenv) < 0) {

			return -1;
		}

		/*
		 * ... et Impera.
		 */
		if (xdl_recs_cmp(dd1, off1, spl.i1, dd2, off2, spl.i2,
				 kvdf, kvdb, spl.min_lo, xenv) < 0 ||
		    xdl_recs_cmp(dd1, spl.i1, lim1, dd2, spl.i2, lim2,
				 kvdf, kvdb, spl.min_hi, xenv) < 0) {

			return -1;
		}
	}

	return 0;
}


int xdl_do_diff(mmfile_t *mf1, mmfile_t *mf2, xpparam_t const *xpp,
		xdfenv_t *xe) {
	long ndiags;
	long *kvd, *kvdf, *kvdb;
	xdalgoenv_t xenv;
	diffdata_t dd1, dd2;

	if (XDF_DIFF_ALG(xpp->flags) == XDF_PATIENCE_DIFF)
		return xdl_do_patience_diff(mf1, mf2, xpp, xe);

	if (XDF_DIFF_ALG(xpp->flags) == XDF_HISTOGRAM_DIFF)
		return xdl_do_histogram_diff(mf1, mf2, xpp, xe);

	if (XDF_DIFF_ALG(xpp->flags) == XDF_NONE_DIFF)
		return xdl_do_none_diff(mf1, mf2, xpp, xe);

	if (xdl_prepare_env(mf1, mf2, xpp, xe) < 0) {

		return -1;
	}

	/*
	 * Allocate and setup K vectors to be used by the differential algorithm.
	 * One is to store the forward path and one to store the backward path.
	 */
	ndiags = xe->xdf1.nreff + xe->xdf2.nreff + 3;
	if (!(kvd = (long *) xdl_malloc((2 * ndiags + 2) * sizeof(long)))) {

		xdl_free_env(xe);
		return -1;
	}
	kvdf = kvd;
	kvdb = kvdf + ndiags;
	kvdf += xe->xdf2.nreff + 1;
	kvdb += xe->xdf2.nreff + 1;

	xenv.mxcost = xdl_bogosqrt(ndiags);
	if (xenv.mxcost < XDL_MAX_COST_MIN)
		xenv.mxcost = XDL_MAX_COST_MIN;
	xenv.snake_cnt = XDL_SNAKE_CNT;
	xenv.heur_min = XDL_HEUR_MIN_COST;

	dd1.nrec = xe->xdf1.nreff;
	dd1.ha = xe->xdf1.ha;
	dd1.rchg = xe->xdf1.rchg;
	dd1.rindex = xe->xdf1.rindex;
	dd2.nrec = xe->xdf2.nreff;
	dd2.ha = xe->xdf2.ha;
	dd2.rchg = xe->xdf2.rchg;
	dd2.rindex = xe->xdf2.rindex;

	if (xdl_recs_cmp(&dd1, 0, dd1.nrec, &dd2, 0, dd2.nrec,
			 kvdf, kvdb, (xpp->flags & XDF_NEED_MINIMAL) != 0, &xenv) < 0) {

		xdl_free(kvd);
		xdl_free_env(xe);
		return -1;
	}

	xdl_free(kvd);

	return 0;
}

/** xdiffi.c end */

/** xprepare.c begin */

/*
 *  LibXDiff by Davide Libenzi ( File Differential Library )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, see
 *  <http://www.gnu.org/licenses/>.
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */



#define XDL_KPDIS_RUN 4
#define XDL_MAX_EQLIMIT 1024
#define XDL_SIMSCAN_WINDOW 100
#define XDL_GUESS_NLINES1 256
#define XDL_GUESS_NLINES2 20


typedef struct s_xdlclass {
	struct s_xdlclass *next;
	unsigned long ha;
	char const *line;
	long size;
	long idx;
	long len1, len2;
} xdlclass_t;

typedef struct s_xdlclassifier {
	unsigned int hbits;
	long hsize;
	xdlclass_t **rchash;
	chastore_t ncha;
	xdlclass_t **rcrecs;
	long alloc;
	long count;
	long flags;
} xdlclassifier_t;




int xdl_init_classifier(xdlclassifier_t *cf, long size, long flags) {
	cf->flags = flags;

	cf->hbits = xdl_hashbits((unsigned int) size);
	cf->hsize = 1 << cf->hbits;

	if (xdl_cha_init(&cf->ncha, sizeof(xdlclass_t), size / 4 + 1) < 0) {

		return -1;
	}
	if (!(cf->rchash = (xdlclass_t **) xdl_malloc(cf->hsize * sizeof(xdlclass_t *)))) {

		xdl_cha_free(&cf->ncha);
		return -1;
	}
	memset(cf->rchash, 0, cf->hsize * sizeof(xdlclass_t *));

	cf->alloc = size;
	if (!(cf->rcrecs = (xdlclass_t **) xdl_malloc(cf->alloc * sizeof(xdlclass_t *)))) {

		xdl_free(cf->rchash);
		xdl_cha_free(&cf->ncha);
		return -1;
	}

	cf->count = 0;

	return 0;
}


static void xdl_free_classifier(xdlclassifier_t *cf) {

	xdl_free(cf->rcrecs);
	xdl_free(cf->rchash);
	xdl_cha_free(&cf->ncha);
}


int xdl_classify_record(unsigned int pass, xdlclassifier_t *cf, xrecord_t **rhash,
			       unsigned int hbits, xrecord_t *rec) {
	long hi;
	char const *line;
	xdlclass_t *rcrec;
	xdlclass_t **rcrecs;

	line = rec->ptr;
	hi = (long) XDL_HASHLONG(rec->ha, cf->hbits);
	for (rcrec = cf->rchash[hi]; rcrec; rcrec = rcrec->next)
		if (rcrec->ha == rec->ha &&
				xdl_recmatch(rcrec->line, rcrec->size,
					rec->ptr, rec->size, cf->flags))
			break;

	if (!rcrec) {
		if (!(rcrec = reinterpret_cast<xdlclass_t*>(xdl_cha_alloc(&cf->ncha)))) {

			return -1;
		}
		rcrec->idx = cf->count++;
		if (cf->count > cf->alloc) {
			cf->alloc *= 2;
			if (!(rcrecs = (xdlclass_t **) xdl_realloc(cf->rcrecs, cf->alloc * sizeof(xdlclass_t *)))) {

				return -1;
			}
			cf->rcrecs = rcrecs;
		}
		cf->rcrecs[rcrec->idx] = rcrec;
		rcrec->line = line;
		rcrec->size = rec->size;
		rcrec->ha = rec->ha;
		rcrec->len1 = rcrec->len2 = 0;
		rcrec->next = cf->rchash[hi];
		cf->rchash[hi] = rcrec;
	}

	(pass == 1) ? rcrec->len1++ : rcrec->len2++;

	rec->ha = (unsigned long) rcrec->idx;

	hi = (long) XDL_HASHLONG(rec->ha, hbits);
	rec->next = rhash[hi];
	rhash[hi] = rec;

	return 0;
}


int xdl_prepare_ctx(unsigned int pass, mmfile_t *mf, long narec, xpparam_t const *xpp,
			   xdlclassifier_t *cf, xdfile_t *xdf) {
	unsigned int hbits;
	long nrec, hsize, bsize;
	unsigned long hav;
	char const *blk, *cur, *top, *prev;
	xrecord_t *crec;
	xrecord_t **recs, **rrecs;
	xrecord_t **rhash;
	unsigned long *ha;
	char *rchg;
	long *rindex;

	ha = NULL;
	rindex = NULL;
	rchg = NULL;
	rhash = NULL;
	recs = NULL;

	if (xdl_cha_init(&xdf->rcha, sizeof(xrecord_t), narec / 4 + 1) < 0)
		goto abort;
	if (!(recs = (xrecord_t **) xdl_malloc(narec * sizeof(xrecord_t *))))
		goto abort;

	if (XDF_DIFF_ALG(xpp->flags) == XDF_HISTOGRAM_DIFF)
		hbits = hsize = 0;
	else {
		hbits = xdl_hashbits((unsigned int) narec);
		hsize = 1 << hbits;
		if (!(rhash = (xrecord_t **) xdl_malloc(hsize * sizeof(xrecord_t *))))
			goto abort;
		memset(rhash, 0, hsize * sizeof(xrecord_t *));
	}

	const Data& data = ((pass == 1) ? m_data1 : m_data2);

	nrec = 0;
	if ((cur = blk = reinterpret_cast<const char *>(xdl_mmfile_first(mf, &bsize))) != NULL) {
		for (top = blk + bsize; cur < top; ) {
			prev = cur;
			hav = data.hash(cur);
			cur = data.next(cur);
			if (nrec >= narec) {
				narec *= 2;
				if (!(rrecs = (xrecord_t **) xdl_realloc(recs, narec * sizeof(xrecord_t *))))
					goto abort;
				recs = rrecs;
			}
			if (!(crec = reinterpret_cast<s_xrecord *>(xdl_cha_alloc(&xdf->rcha))))
				goto abort;
			crec->ptr = prev;
			crec->size = (long) (cur - prev);
			crec->ha = hav;
			recs[nrec++] = crec;

			if ((XDF_DIFF_ALG(xpp->flags) != XDF_HISTOGRAM_DIFF) &&
			    xdl_classify_record(pass, cf, rhash, hbits, crec) < 0)
				goto abort;
		}
	}

	if (!(rchg = (char *) xdl_malloc((nrec + 2) * sizeof(char))))
		goto abort;
	memset(rchg, 0, (nrec + 2) * sizeof(char));

	if (!(rindex = (long *) xdl_malloc((nrec + 1) * sizeof(long))))
		goto abort;
	if (!(ha = (unsigned long *) xdl_malloc((nrec + 1) * sizeof(unsigned long))))
		goto abort;

	xdf->nrec = nrec;
	xdf->recs = recs;
	xdf->hbits = hbits;
	xdf->rhash = rhash;
	xdf->rchg = rchg + 1;
	xdf->rindex = rindex;
	xdf->nreff = 0;
	xdf->ha = ha;
	xdf->dstart = 0;
	xdf->dend = nrec - 1;

	return 0;

abort:
	xdl_free(ha);
	xdl_free(rindex);
	xdl_free(rchg);
	xdl_free(rhash);
	xdl_free(recs);
	xdl_cha_free(&xdf->rcha);
	return -1;
}


static void xdl_free_ctx(xdfile_t *xdf) {

	xdl_free(xdf->rhash);
	xdl_free(xdf->rindex);
	xdl_free(xdf->rchg - 1);
	xdl_free(xdf->ha);
	xdl_free(xdf->recs);
	xdl_cha_free(&xdf->rcha);
}


int xdl_prepare_env(mmfile_t *mf1, mmfile_t *mf2, xpparam_t const *xpp,
		    xdfenv_t *xe) {
	long enl1, enl2, sample;
	xdlclassifier_t cf;

	memset(&cf, 0, sizeof(cf));

	/*
	 * For histogram diff, we can afford a smaller sample size and
	 * thus a poorer estimate of the number of lines, as the hash
	 * table (rhash) won't be filled up/grown. The number of lines
	 * (nrecs) will be updated correctly anyway by
	 * xdl_prepare_ctx().
	 */
	sample = (XDF_DIFF_ALG(xpp->flags) == XDF_HISTOGRAM_DIFF
		  ? XDL_GUESS_NLINES2 : XDL_GUESS_NLINES1);

	enl1 = xdl_guess_lines(1, mf1, sample) + 1;
	enl2 = xdl_guess_lines(2, mf2, sample) + 1;

	if (XDF_DIFF_ALG(xpp->flags) != XDF_HISTOGRAM_DIFF &&
	    xdl_init_classifier(&cf, enl1 + enl2 + 1, xpp->flags) < 0)
		return -1;

	if (xdl_prepare_ctx(1, mf1, enl1, xpp, &cf, &xe->xdf1) < 0) {

		xdl_free_classifier(&cf);
		return -1;
	}
	if (xdl_prepare_ctx(2, mf2, enl2, xpp, &cf, &xe->xdf2) < 0) {

		xdl_free_ctx(&xe->xdf1);
		xdl_free_classifier(&cf);
		return -1;
	}

	if ((XDF_DIFF_ALG(xpp->flags) != XDF_PATIENCE_DIFF) &&
	    (XDF_DIFF_ALG(xpp->flags) != XDF_HISTOGRAM_DIFF) &&
	    (XDF_DIFF_ALG(xpp->flags) != XDF_NONE_DIFF) &&
	    xdl_optimize_ctxs(&cf, &xe->xdf1, &xe->xdf2) < 0) {

		xdl_free_ctx(&xe->xdf2);
		xdl_free_ctx(&xe->xdf1);
		xdl_free_classifier(&cf);
		return -1;
	}

	if (XDF_DIFF_ALG(xpp->flags) != XDF_HISTOGRAM_DIFF)
		xdl_free_classifier(&cf);

	return 0;
}


void xdl_free_env(xdfenv_t *xe) {

	xdl_free_ctx(&xe->xdf2);
	xdl_free_ctx(&xe->xdf1);
}


int xdl_clean_mmatch(char const *dis, long i, long s, long e) {
	long r, rdis0, rpdis0, rdis1, rpdis1;

	/*
	 * Limits the window the is examined during the similar-lines
	 * scan. The loops below stops when dis[i - r] == 1 (line that
	 * has no match), but there are corner cases where the loop
	 * proceed all the way to the extremities by causing huge
	 * performance penalties in case of big files.
	 */
	if (i - s > XDL_SIMSCAN_WINDOW)
		s = i - XDL_SIMSCAN_WINDOW;
	if (e - i > XDL_SIMSCAN_WINDOW)
		e = i + XDL_SIMSCAN_WINDOW;

	/*
	 * Scans the lines before 'i' to find a run of lines that either
	 * have no match (dis[j] == 0) or have multiple matches (dis[j] > 1).
	 * Note that we always call this function with dis[i] > 1, so the
	 * current line (i) is already a multimatch line.
	 */
	for (r = 1, rdis0 = 0, rpdis0 = 1; (i - r) >= s; r++) {
		if (!dis[i - r])
			rdis0++;
		else if (dis[i - r] == 2)
			rpdis0++;
		else
			break;
	}
	/*
	 * If the run before the line 'i' found only multimatch lines, we
	 * return 0 and hence we don't make the current line (i) discarded.
	 * We want to discard multimatch lines only when they appear in the
	 * middle of runs with nomatch lines (dis[j] == 0).
	 */
	if (rdis0 == 0)
		return 0;
	for (r = 1, rdis1 = 0, rpdis1 = 1; (i + r) <= e; r++) {
		if (!dis[i + r])
			rdis1++;
		else if (dis[i + r] == 2)
			rpdis1++;
		else
			break;
	}
	/*
	 * If the run after the line 'i' found only multimatch lines, we
	 * return 0 and hence we don't make the current line (i) discarded.
	 */
	if (rdis1 == 0)
		return 0;
	rdis1 += rdis0;
	rpdis1 += rpdis0;

	return rpdis1 * XDL_KPDIS_RUN < (rpdis1 + rdis1);
}


/*
 * Try to reduce the problem complexity, discard records that have no
 * matches on the other file. Also, lines that have multiple matches
 * might be potentially discarded if they happear in a run of discardable.
 */
int xdl_cleanup_records(xdlclassifier_t *cf, xdfile_t *xdf1, xdfile_t *xdf2) {
	long i, nm, nreff, mlim;
	xrecord_t **recs;
	xdlclass_t *rcrec;
	char *dis, *dis1, *dis2;

	if (!(dis = (char *) xdl_malloc(xdf1->nrec + xdf2->nrec + 2))) {

		return -1;
	}
	memset(dis, 0, xdf1->nrec + xdf2->nrec + 2);
	dis1 = dis;
	dis2 = dis1 + xdf1->nrec + 1;

	if ((mlim = xdl_bogosqrt(xdf1->nrec)) > XDL_MAX_EQLIMIT)
		mlim = XDL_MAX_EQLIMIT;
	for (i = xdf1->dstart, recs = &xdf1->recs[xdf1->dstart]; i <= xdf1->dend; i++, recs++) {
		rcrec = cf->rcrecs[(*recs)->ha];
		nm = rcrec ? rcrec->len2 : 0;
		dis1[i] = (nm == 0) ? 0: (nm >= mlim) ? 2: 1;
	}

	if ((mlim = xdl_bogosqrt(xdf2->nrec)) > XDL_MAX_EQLIMIT)
		mlim = XDL_MAX_EQLIMIT;
	for (i = xdf2->dstart, recs = &xdf2->recs[xdf2->dstart]; i <= xdf2->dend; i++, recs++) {
		rcrec = cf->rcrecs[(*recs)->ha];
		nm = rcrec ? rcrec->len1 : 0;
		dis2[i] = (nm == 0) ? 0: (nm >= mlim) ? 2: 1;
	}

	for (nreff = 0, i = xdf1->dstart, recs = &xdf1->recs[xdf1->dstart];
	     i <= xdf1->dend; i++, recs++) {
		if (dis1[i] == 1 ||
		    (dis1[i] == 2 && !xdl_clean_mmatch(dis1, i, xdf1->dstart, xdf1->dend))) {
			xdf1->rindex[nreff] = i;
			xdf1->ha[nreff] = (*recs)->ha;
			nreff++;
		} else
			xdf1->rchg[i] = 1;
	}
	xdf1->nreff = nreff;

	for (nreff = 0, i = xdf2->dstart, recs = &xdf2->recs[xdf2->dstart];
	     i <= xdf2->dend; i++, recs++) {
		if (dis2[i] == 1 ||
		    (dis2[i] == 2 && !xdl_clean_mmatch(dis2, i, xdf2->dstart, xdf2->dend))) {
			xdf2->rindex[nreff] = i;
			xdf2->ha[nreff] = (*recs)->ha;
			nreff++;
		} else
			xdf2->rchg[i] = 1;
	}
	xdf2->nreff = nreff;

	xdl_free(dis);

	return 0;
}


/*
 * Early trim initial and terminal matching records.
 */
static int xdl_trim_ends(xdfile_t *xdf1, xdfile_t *xdf2) {
	long i, lim;
	xrecord_t **recs1, **recs2;

	recs1 = xdf1->recs;
	recs2 = xdf2->recs;
	for (i = 0, lim = XDL_MIN(xdf1->nrec, xdf2->nrec); i < lim;
	     i++, recs1++, recs2++)
		if ((*recs1)->ha != (*recs2)->ha)
			break;

	xdf1->dstart = xdf2->dstart = i;

	recs1 = xdf1->recs + xdf1->nrec - 1;
	recs2 = xdf2->recs + xdf2->nrec - 1;
	for (lim -= i, i = 0; i < lim; i++, recs1--, recs2--)
		if ((*recs1)->ha != (*recs2)->ha)
			break;

	xdf1->dend = xdf1->nrec - i - 1;
	xdf2->dend = xdf2->nrec - i - 1;

	return 0;
}


int xdl_optimize_ctxs(xdlclassifier_t *cf, xdfile_t *xdf1, xdfile_t *xdf2) {

	if (xdl_trim_ends(xdf1, xdf2) < 0 ||
	    xdl_cleanup_records(cf, xdf1, xdf2) < 0) {

		return -1;
	}

	return 0;
}

/** xprepare.c end */

public:
	enum Algorithm { MYERS, MINIMAL, PATIENCE, HISTOGRAM, NONE };

	Diff(const Data& data1, const Data& data2)
		: m_data1(data1), m_data2(data2) { }

	int diff(Algorithm algo, std::vector<char>& edscript)
	{
		mmfile_t file1{}, file2{};
		xdfenv_t env;
		xpparam_t xpp{};

		file1.ptr = (char*)m_data1.data();
		file1.size = m_data1.size();
		file2.ptr = (char*)m_data2.data();
		file2.size = m_data2.size();

		switch (algo)
		{	
		case MINIMAL:
			xpp.flags = XDF_NEED_MINIMAL;
			break;
		case PATIENCE:
			xpp.flags = XDF_PATIENCE_DIFF;
			break;
		case HISTOGRAM:
			xpp.flags = XDF_HISTOGRAM_DIFF;
			break;
		case NONE:
			xpp.flags = XDF_NONE_DIFF;
			break;
		default:
			xpp.flags = 0;
		}

		xdl_do_diff(&file1, &file2, &xpp, &env);

		edscript.clear();

		char* rchg1 = env.xdf1.rchg, * rchg2 = env.xdf2.rchg;
		long nrec1 = env.xdf1.nrec, nrec2 = env.xdf2.nrec;
		long i1 = 0, i2 = 0;
		int D = 0;
		while (i1 < nrec1 || i2 < nrec2)
		{
			if ((i1 < nrec1 && rchg1[i1]) && (i2 < nrec2 && rchg2[i2]))
			{
				edscript.push_back('!');
				D++;
				i1++;
				i2++;
			}
			else if ((i1 >= nrec1 || !rchg1[i1]) && (i2 < nrec2 && rchg2[i2]))
			{
				edscript.push_back('+');
				D++;
				i2++;
			}
			else if ((i1 < nrec1 && rchg1[i1]) && (i2 >= nrec2 || !rchg2[i2]))
			{
				edscript.push_back('-');
				D++;
				i1++;
			}
			else if ((i1 < nrec1 && !rchg1[i1]) && (i2 < nrec2 && !rchg2[i2]))
			{
				edscript.push_back('=');
				D++;
				i1++;
				i2++;
			}
			else
			{
				assert(true);
			}
		}

		xdl_free_env(&env);

		return D;
	}

private:
	const Data& m_data1;
	const Data& m_data2;
};

