/*
 * security/tomoyo/file.c
 *
 * Pathname restriction functions.
 *
 * Copyright (C) 2005-2010  NTT DATA CORPORATION
 */

#include "common.h"
#include <linux/slab.h>

/* Keyword array for operations with one pathname. */
const char *tomoyo_path_keyword[TOMOYO_MAX_PATH_OPERATION] = {
	[TOMOYO_TYPE_EXECUTE]    = "execute",
	[TOMOYO_TYPE_READ]       = "read",
	[TOMOYO_TYPE_WRITE]      = "write",
	[TOMOYO_TYPE_APPEND]     = "append",
	[TOMOYO_TYPE_UNLINK]     = "unlink",
	[TOMOYO_TYPE_GETATTR]    = "getattr",
	[TOMOYO_TYPE_RMDIR]      = "rmdir",
	[TOMOYO_TYPE_TRUNCATE]   = "truncate",
	[TOMOYO_TYPE_SYMLINK]    = "symlink",
	[TOMOYO_TYPE_CHROOT]     = "chroot",
	[TOMOYO_TYPE_UMOUNT]     = "unmount",
};

/* Keyword array for operations with one pathname and three numbers. */
const char *tomoyo_mkdev_keyword[TOMOYO_MAX_MKDEV_OPERATION] = {
	[TOMOYO_TYPE_MKBLOCK]    = "mkblock",
	[TOMOYO_TYPE_MKCHAR]     = "mkchar",
};

/* Keyword array for operations with two pathnames. */
const char *tomoyo_path2_keyword[TOMOYO_MAX_PATH2_OPERATION] = {
	[TOMOYO_TYPE_LINK]       = "link",
	[TOMOYO_TYPE_RENAME]     = "rename",
	[TOMOYO_TYPE_PIVOT_ROOT] = "pivot_root",
};

/* Keyword array for operations with one pathname and one number. */
const char *tomoyo_path_number_keyword[TOMOYO_MAX_PATH_NUMBER_OPERATION] = {
	[TOMOYO_TYPE_CREATE]     = "create",
	[TOMOYO_TYPE_MKDIR]      = "mkdir",
	[TOMOYO_TYPE_MKFIFO]     = "mkfifo",
	[TOMOYO_TYPE_MKSOCK]     = "mksock",
	[TOMOYO_TYPE_IOCTL]      = "ioctl",
	[TOMOYO_TYPE_CHMOD]      = "chmod",
	[TOMOYO_TYPE_CHOWN]      = "chown",
	[TOMOYO_TYPE_CHGRP]      = "chgrp",
};

/*
 * Mapping table from "enum tomoyo_path_acl_index" to "enum tomoyo_mac_index".
 */
static const u8 tomoyo_p2mac[TOMOYO_MAX_PATH_OPERATION] = {
	[TOMOYO_TYPE_EXECUTE]    = TOMOYO_MAC_FILE_EXECUTE,
	[TOMOYO_TYPE_READ]       = TOMOYO_MAC_FILE_OPEN,
	[TOMOYO_TYPE_WRITE]      = TOMOYO_MAC_FILE_OPEN,
	[TOMOYO_TYPE_APPEND]     = TOMOYO_MAC_FILE_OPEN,
	[TOMOYO_TYPE_UNLINK]     = TOMOYO_MAC_FILE_UNLINK,
	[TOMOYO_TYPE_GETATTR]    = TOMOYO_MAC_FILE_GETATTR,
	[TOMOYO_TYPE_RMDIR]      = TOMOYO_MAC_FILE_RMDIR,
	[TOMOYO_TYPE_TRUNCATE]   = TOMOYO_MAC_FILE_TRUNCATE,
	[TOMOYO_TYPE_SYMLINK]    = TOMOYO_MAC_FILE_SYMLINK,
	[TOMOYO_TYPE_CHROOT]     = TOMOYO_MAC_FILE_CHROOT,
	[TOMOYO_TYPE_UMOUNT]     = TOMOYO_MAC_FILE_UMOUNT,
};

/*
 * Mapping table from "enum tomoyo_mkdev_acl_index" to "enum tomoyo_mac_index".
 */
static const u8 tomoyo_pnnn2mac[TOMOYO_MAX_MKDEV_OPERATION] = {
	[TOMOYO_TYPE_MKBLOCK] = TOMOYO_MAC_FILE_MKBLOCK,
	[TOMOYO_TYPE_MKCHAR]  = TOMOYO_MAC_FILE_MKCHAR,
};

/*
 * Mapping table from "enum tomoyo_path2_acl_index" to "enum tomoyo_mac_index".
 */
static const u8 tomoyo_pp2mac[TOMOYO_MAX_PATH2_OPERATION] = {
	[TOMOYO_TYPE_LINK]       = TOMOYO_MAC_FILE_LINK,
	[TOMOYO_TYPE_RENAME]     = TOMOYO_MAC_FILE_RENAME,
	[TOMOYO_TYPE_PIVOT_ROOT] = TOMOYO_MAC_FILE_PIVOT_ROOT,
};

/*
 * Mapping table from "enum tomoyo_path_number_acl_index" to
 * "enum tomoyo_mac_index".
 */
static const u8 tomoyo_pn2mac[TOMOYO_MAX_PATH_NUMBER_OPERATION] = {
	[TOMOYO_TYPE_CREATE] = TOMOYO_MAC_FILE_CREATE,
	[TOMOYO_TYPE_MKDIR]  = TOMOYO_MAC_FILE_MKDIR,
	[TOMOYO_TYPE_MKFIFO] = TOMOYO_MAC_FILE_MKFIFO,
	[TOMOYO_TYPE_MKSOCK] = TOMOYO_MAC_FILE_MKSOCK,
	[TOMOYO_TYPE_IOCTL]  = TOMOYO_MAC_FILE_IOCTL,
	[TOMOYO_TYPE_CHMOD]  = TOMOYO_MAC_FILE_CHMOD,
	[TOMOYO_TYPE_CHOWN]  = TOMOYO_MAC_FILE_CHOWN,
	[TOMOYO_TYPE_CHGRP]  = TOMOYO_MAC_FILE_CHGRP,
};

/**
 * tomoyo_put_name_union - Drop reference on "struct tomoyo_name_union".
 *
 * @ptr: Pointer to "struct tomoyo_name_union".
 *
 * Returns nothing.
 */
void tomoyo_put_name_union(struct tomoyo_name_union *ptr)
{
	tomoyo_put_group(ptr->group);
	tomoyo_put_name(ptr->filename);
}

/**
 * tomoyo_compare_name_union - Check whether a name matches "struct tomoyo_name_union" or not.
 *
 * @name: Pointer to "struct tomoyo_path_info".
 * @ptr:  Pointer to "struct tomoyo_name_union".
 *
 * Returns "struct tomoyo_path_info" if @name matches @ptr, NULL otherwise.
 */
const struct tomoyo_path_info *
tomoyo_compare_name_union(const struct tomoyo_path_info *name,
			  const struct tomoyo_name_union *ptr)
{
	if (ptr->group)
		return tomoyo_path_matches_group(name, ptr->group);
	if (tomoyo_path_matches_pattern(name, ptr->filename))
		return ptr->filename;
	return NULL;
}

/**
 * tomoyo_put_number_union - Drop reference on "struct tomoyo_number_union".
 *
 * @ptr: Pointer to "struct tomoyo_number_union".
 *
 * Returns nothing.
 */
void tomoyo_put_number_union(struct tomoyo_number_union *ptr)
{
	tomoyo_put_group(ptr->group);
}

/**
 * tomoyo_compare_number_union - Check whether a value matches "struct tomoyo_number_union" or not.
 *
 * @value: Number to check.
 * @ptr:   Pointer to "struct tomoyo_number_union".
 *
 * Returns true if @value matches @ptr, false otherwise.
 */
bool tomoyo_compare_number_union(const unsigned long value,
				 const struct tomoyo_number_union *ptr)
{
	if (ptr->group)
		return tomoyo_number_matches_group(value, value, ptr->group);
	return value >= ptr->values[0] && value <= ptr->values[1];
}

/**
 * tomoyo_add_slash - Add trailing '/' if needed.
 *
 * @buf: Pointer to "struct tomoyo_path_info".
 *
 * Returns nothing.
 *
 * @buf must be generated by tomoyo_encode() because this function does not
 * allocate memory for adding '/'.
 */
static void tomoyo_add_slash(struct tomoyo_path_info *buf)
{
	if (buf->is_dir)
		return;
	/*
	 * This is OK because tomoyo_encode() reserves space for appending "/".
	 */
	strcat((char *) buf->name, "/");
	tomoyo_fill_path_info(buf);
}

/**
 * tomoyo_get_realpath - Get realpath.
 *
 * @buf:  Pointer to "struct tomoyo_path_info".
 * @path: Pointer to "struct path".
 *
 * Returns true on success, false otherwise.
 */
static bool tomoyo_get_realpath(struct tomoyo_path_info *buf, struct path *path)
{
	buf->name = tomoyo_realpath_from_path(path);
	if (buf->name) {
		tomoyo_fill_path_info(buf);
		return true;
	}
        return false;
}

/**
 * tomoyo_audit_path_log - Audit path request log.
 *
 * @r: Pointer to "struct tomoyo_request_info".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int tomoyo_audit_path_log(struct tomoyo_request_info *r)
{
	const char *operation = tomoyo_path_keyword[r->param.path.operation];
	const struct tomoyo_path_info *filename = r->param.path.filename;
	if (r->granted)
		return 0;
	tomoyo_warn_log(r, "%s %s", operation, filename->name);
	return tomoyo_supervisor(r, "allow_%s %s\n", operation,
				 filename->name);
}

/**
 * tomoyo_audit_path2_log - Audit path/path request log.
 *
 * @r: Pointer to "struct tomoyo_request_info".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int tomoyo_audit_path2_log(struct tomoyo_request_info *r)
{
	const char *operation = tomoyo_path2_keyword[r->param.path2.operation];
	const struct tomoyo_path_info *filename1 = r->param.path2.filename1;
	const struct tomoyo_path_info *filename2 = r->param.path2.filename2;
	if (r->granted)
		return 0;
	tomoyo_warn_log(r, "%s %s %s", operation, filename1->name,
			filename2->name);
	return tomoyo_supervisor(r, "allow_%s %s %s\n", operation,
				 filename1->name, filename2->name);
}

/**
 * tomoyo_audit_mkdev_log - Audit path/number/number/number request log.
 *
 * @r: Pointer to "struct tomoyo_request_info".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int tomoyo_audit_mkdev_log(struct tomoyo_request_info *r)
{
	const char *operation = tomoyo_mkdev_keyword[r->param.mkdev.operation];
	const struct tomoyo_path_info *filename = r->param.mkdev.filename;
	const unsigned int major = r->param.mkdev.major;
	const unsigned int minor = r->param.mkdev.minor;
	const unsigned int mode = r->param.mkdev.mode;
	if (r->granted)
		return 0;
	tomoyo_warn_log(r, "%s %s 0%o %u %u", operation, filename->name, mode,
			major, minor);
	return tomoyo_supervisor(r, "allow_%s %s 0%o %u %u\n", operation,
				 filename->name, mode, major, minor);
}

/**
 * tomoyo_audit_path_number_log - Audit path/number request log.
 *
 * @r: Pointer to "struct tomoyo_request_info".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int tomoyo_audit_path_number_log(struct tomoyo_request_info *r)
{
	const u8 type = r->param.path_number.operation;
	u8 radix;
	const struct tomoyo_path_info *filename = r->param.path_number.filename;
	const char *operation = tomoyo_path_number_keyword[type];
	char buffer[64];
	if (r->granted)
		return 0;
	switch (type) {
	case TOMOYO_TYPE_CREATE:
	case TOMOYO_TYPE_MKDIR:
	case TOMOYO_TYPE_MKFIFO:
	case TOMOYO_TYPE_MKSOCK:
	case TOMOYO_TYPE_CHMOD:
		radix = TOMOYO_VALUE_TYPE_OCTAL;
		break;
	case TOMOYO_TYPE_IOCTL:
		radix = TOMOYO_VALUE_TYPE_HEXADECIMAL;
		break;
	default:
		radix = TOMOYO_VALUE_TYPE_DECIMAL;
		break;
	}
	tomoyo_print_ulong(buffer, sizeof(buffer), r->param.path_number.number,
			   radix);
	tomoyo_warn_log(r, "%s %s %s", operation, filename->name, buffer);
	return tomoyo_supervisor(r, "allow_%s %s %s\n", operation,
				 filename->name, buffer);
}

/**
 * tomoyo_check_path_acl - Check permission for path operation.
 *
 * @r:   Pointer to "struct tomoyo_request_info".
 * @ptr: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if granted, false otherwise.
 *
 * To be able to use wildcard for domain transition, this function sets
 * matching entry on success. Since the caller holds tomoyo_read_lock(),
 * it is safe to set matching entry.
 */
static bool tomoyo_check_path_acl(struct tomoyo_request_info *r,
				  const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_path_acl *acl = container_of(ptr, typeof(*acl),
							 head);
	if (acl->perm & (1 << r->param.path.operation)) {
		r->param.path.matched_path =
			tomoyo_compare_name_union(r->param.path.filename,
						  &acl->name);
		return r->param.path.matched_path != NULL;
	}
	return false;
}

/**
 * tomoyo_check_path_number_acl - Check permission for path number operation.
 *
 * @r:   Pointer to "struct tomoyo_request_info".
 * @ptr: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if granted, false otherwise.
 */
static bool tomoyo_check_path_number_acl(struct tomoyo_request_info *r,
					 const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_path_number_acl *acl =
		container_of(ptr, typeof(*acl), head);
	return (acl->perm & (1 << r->param.path_number.operation)) &&
		tomoyo_compare_number_union(r->param.path_number.number,
					    &acl->number) &&
		tomoyo_compare_name_union(r->param.path_number.filename,
					  &acl->name);
}

/**
 * tomoyo_check_path2_acl - Check permission for path path operation.
 *
 * @r:   Pointer to "struct tomoyo_request_info".
 * @ptr: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if granted, false otherwise.
 */
static bool tomoyo_check_path2_acl(struct tomoyo_request_info *r,
				   const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_path2_acl *acl =
		container_of(ptr, typeof(*acl), head);
	return (acl->perm & (1 << r->param.path2.operation)) &&
		tomoyo_compare_name_union(r->param.path2.filename1, &acl->name1)
		&& tomoyo_compare_name_union(r->param.path2.filename2,
					     &acl->name2);
}

/**
 * tomoyo_check_mkdev_acl - Check permission for path number number number operation.
 *
 * @r:   Pointer to "struct tomoyo_request_info".
 * @ptr: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if granted, false otherwise.
 */
static bool tomoyo_check_mkdev_acl(struct tomoyo_request_info *r,
				   const struct tomoyo_acl_info *ptr)
{
	const struct tomoyo_mkdev_acl *acl =
		container_of(ptr, typeof(*acl), head);
	return (acl->perm & (1 << r->param.mkdev.operation)) &&
		tomoyo_compare_number_union(r->param.mkdev.mode,
					    &acl->mode) &&
		tomoyo_compare_number_union(r->param.mkdev.major,
					    &acl->major) &&
		tomoyo_compare_number_union(r->param.mkdev.minor,
					    &acl->minor) &&
		tomoyo_compare_name_union(r->param.mkdev.filename,
					  &acl->name);
}

/**
 * tomoyo_same_path_acl - Check for duplicated "struct tomoyo_path_acl" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_info".
 * @b: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if @a == @b except permission bits, false otherwise.
 */
static bool tomoyo_same_path_acl(const struct tomoyo_acl_info *a,
				 const struct tomoyo_acl_info *b)
{
	const struct tomoyo_path_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_path_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->name, &p2->name);
}

/**
 * tomoyo_merge_path_acl - Merge duplicated "struct tomoyo_path_acl" entry.
 *
 * @a:         Pointer to "struct tomoyo_acl_info".
 * @b:         Pointer to "struct tomoyo_acl_info".
 * @is_delete: True for @a &= ~@b, false for @a |= @b.
 *
 * Returns true if @a is empty, false otherwise.
 */
static bool tomoyo_merge_path_acl(struct tomoyo_acl_info *a,
				  struct tomoyo_acl_info *b,
				  const bool is_delete)
{
	u16 * const a_perm = &container_of(a, struct tomoyo_path_acl, head)
		->perm;
	u16 perm = *a_perm;
	const u16 b_perm = container_of(b, struct tomoyo_path_acl, head)->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

/**
 * tomoyo_update_path_acl - Update "struct tomoyo_path_acl" list.
 *
 * @perm:  Permission.
 * @param: Pointer to "struct tomoyo_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_path_acl(const u16 perm,
				  struct tomoyo_acl_param *param)
{
	struct tomoyo_path_acl e = {
		.head.type = TOMOYO_TYPE_PATH_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_path_acl,
					     tomoyo_merge_path_acl);
	tomoyo_put_name_union(&e.name);
	return error;
}

/**
 * tomoyo_same_mkdev_acl - Check for duplicated "struct tomoyo_mkdev_acl" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_info".
 * @b: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if @a == @b except permission bits, false otherwise.
 */
static bool tomoyo_same_mkdev_acl(const struct tomoyo_acl_info *a,
					 const struct tomoyo_acl_info *b)
{
	const struct tomoyo_mkdev_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_mkdev_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->name, &p2->name) &&
		tomoyo_same_number_union(&p1->mode, &p2->mode) &&
		tomoyo_same_number_union(&p1->major, &p2->major) &&
		tomoyo_same_number_union(&p1->minor, &p2->minor);
}

/**
 * tomoyo_merge_mkdev_acl - Merge duplicated "struct tomoyo_mkdev_acl" entry.
 *
 * @a:         Pointer to "struct tomoyo_acl_info".
 * @b:         Pointer to "struct tomoyo_acl_info".
 * @is_delete: True for @a &= ~@b, false for @a |= @b.
 *
 * Returns true if @a is empty, false otherwise.
 */
static bool tomoyo_merge_mkdev_acl(struct tomoyo_acl_info *a,
				   struct tomoyo_acl_info *b,
				   const bool is_delete)
{
	u8 *const a_perm = &container_of(a, struct tomoyo_mkdev_acl,
					 head)->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_mkdev_acl, head)
		->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

/**
 * tomoyo_update_mkdev_acl - Update "struct tomoyo_mkdev_acl" list.
 *
 * @perm:  Permission.
 * @param: Pointer to "struct tomoyo_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_mkdev_acl(const u8 perm,
				   struct tomoyo_acl_param *param)
{
	struct tomoyo_mkdev_acl e = {
		.head.type = TOMOYO_TYPE_MKDEV_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name) ||
	    !tomoyo_parse_number_union(param, &e.mode) ||
	    !tomoyo_parse_number_union(param, &e.major) ||
	    !tomoyo_parse_number_union(param, &e.minor))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_mkdev_acl,
					     tomoyo_merge_mkdev_acl);
	tomoyo_put_name_union(&e.name);
	tomoyo_put_number_union(&e.mode);
	tomoyo_put_number_union(&e.major);
	tomoyo_put_number_union(&e.minor);
	return error;
}

/**
 * tomoyo_same_path2_acl - Check for duplicated "struct tomoyo_path2_acl" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_info".
 * @b: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if @a == @b except permission bits, false otherwise.
 */
static bool tomoyo_same_path2_acl(const struct tomoyo_acl_info *a,
				  const struct tomoyo_acl_info *b)
{
	const struct tomoyo_path2_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_path2_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->name1, &p2->name1) &&
		tomoyo_same_name_union(&p1->name2, &p2->name2);
}

/**
 * tomoyo_merge_path2_acl - Merge duplicated "struct tomoyo_path2_acl" entry.
 *
 * @a:         Pointer to "struct tomoyo_acl_info".
 * @b:         Pointer to "struct tomoyo_acl_info".
 * @is_delete: True for @a &= ~@b, false for @a |= @b.
 *
 * Returns true if @a is empty, false otherwise.
 */
static bool tomoyo_merge_path2_acl(struct tomoyo_acl_info *a,
				   struct tomoyo_acl_info *b,
				   const bool is_delete)
{
	u8 * const a_perm = &container_of(a, struct tomoyo_path2_acl, head)
		->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_path2_acl, head)->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

/**
 * tomoyo_update_path2_acl - Update "struct tomoyo_path2_acl" list.
 *
 * @perm:  Permission.
 * @param: Pointer to "struct tomoyo_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_path2_acl(const u8 perm,
				   struct tomoyo_acl_param *param)
{
	struct tomoyo_path2_acl e = {
		.head.type = TOMOYO_TYPE_PATH2_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name1) ||
	    !tomoyo_parse_name_union(param, &e.name2))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_path2_acl,
					     tomoyo_merge_path2_acl);
	tomoyo_put_name_union(&e.name1);
	tomoyo_put_name_union(&e.name2);
	return error;
}

/**
 * tomoyo_path_permission - Check permission for single path operation.
 *
 * @r:         Pointer to "struct tomoyo_request_info".
 * @operation: Type of operation.
 * @filename:  Filename to check.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_path_permission(struct tomoyo_request_info *r, u8 operation,
			   const struct tomoyo_path_info *filename)
{
	int error;

	r->type = tomoyo_p2mac[operation];
	r->mode = tomoyo_get_mode(r->profile, r->type);
	if (r->mode == TOMOYO_CONFIG_DISABLED)
		return 0;
	r->param_type = TOMOYO_TYPE_PATH_ACL;
	r->param.path.filename = filename;
	r->param.path.operation = operation;
	do {
		tomoyo_check_acl(r, tomoyo_check_path_acl);
		error = tomoyo_audit_path_log(r);
		/*
		 * Do not retry for execute request, for alias may have
		 * changed.
		 */
	} while (error == TOMOYO_RETRY_REQUEST &&
		 operation != TOMOYO_TYPE_EXECUTE);
	return error;
}

/**
 * tomoyo_same_path_number_acl - Check for duplicated "struct tomoyo_path_number_acl" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_info".
 * @b: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if @a == @b except permission bits, false otherwise.
 */
static bool tomoyo_same_path_number_acl(const struct tomoyo_acl_info *a,
					const struct tomoyo_acl_info *b)
{
	const struct tomoyo_path_number_acl *p1 = container_of(a, typeof(*p1),
							       head);
	const struct tomoyo_path_number_acl *p2 = container_of(b, typeof(*p2),
							       head);
	return tomoyo_same_name_union(&p1->name, &p2->name) &&
		tomoyo_same_number_union(&p1->number, &p2->number);
}

/**
 * tomoyo_merge_path_number_acl - Merge duplicated "struct tomoyo_path_number_acl" entry.
 *
 * @a:         Pointer to "struct tomoyo_acl_info".
 * @b:         Pointer to "struct tomoyo_acl_info".
 * @is_delete: True for @a &= ~@b, false for @a |= @b.
 *
 * Returns true if @a is empty, false otherwise.
 */
static bool tomoyo_merge_path_number_acl(struct tomoyo_acl_info *a,
					 struct tomoyo_acl_info *b,
					 const bool is_delete)
{
	u8 * const a_perm = &container_of(a, struct tomoyo_path_number_acl,
					  head)->perm;
	u8 perm = *a_perm;
	const u8 b_perm = container_of(b, struct tomoyo_path_number_acl, head)
		->perm;
	if (is_delete)
		perm &= ~b_perm;
	else
		perm |= b_perm;
	*a_perm = perm;
	return !perm;
}

/**
 * tomoyo_update_path_number_acl - Update ioctl/chmod/chown/chgrp ACL.
 *
 * @perm:  Permission.
 * @param: Pointer to "struct tomoyo_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 */
static int tomoyo_update_path_number_acl(const u8 perm,
					 struct tomoyo_acl_param *param)
{
	struct tomoyo_path_number_acl e = {
		.head.type = TOMOYO_TYPE_PATH_NUMBER_ACL,
		.perm = perm
	};
	int error;
	if (!tomoyo_parse_name_union(param, &e.name) ||
	    !tomoyo_parse_number_union(param, &e.number))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_path_number_acl,
					     tomoyo_merge_path_number_acl);
	tomoyo_put_name_union(&e.name);
	tomoyo_put_number_union(&e.number);
	return error;
}

/**
 * tomoyo_path_number_perm - Check permission for "create", "mkdir", "mkfifo", "mksock", "ioctl", "chmod", "chown", "chgrp".
 *
 * @type:   Type of operation.
 * @path:   Pointer to "struct path".
 * @number: Number.
 *
 * Returns 0 on success, negative value otherwise.
 */
int tomoyo_path_number_perm(const u8 type, struct path *path,
			    unsigned long number)
{
	struct tomoyo_request_info r;
	int error = -ENOMEM;
	struct tomoyo_path_info buf;
	int idx;

	if (tomoyo_init_request_info(&r, NULL, tomoyo_pn2mac[type])
	    == TOMOYO_CONFIG_DISABLED || !path->mnt || !path->dentry)
		return 0;
	idx = tomoyo_read_lock();
	if (!tomoyo_get_realpath(&buf, path))
		goto out;
	if (type == TOMOYO_TYPE_MKDIR)
		tomoyo_add_slash(&buf);
	r.param_type = TOMOYO_TYPE_PATH_NUMBER_ACL;
	r.param.path_number.operation = type;
	r.param.path_number.filename = &buf;
	r.param.path_number.number = number;
	do {
		tomoyo_check_acl(&r, tomoyo_check_path_number_acl);
		error = tomoyo_audit_path_number_log(&r);
	} while (error == TOMOYO_RETRY_REQUEST);
	kfree(buf.name);
 out:
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

/**
 * tomoyo_check_open_permission - Check permission for "read" and "write".
 *
 * @domain: Pointer to "struct tomoyo_domain_info".
 * @path:   Pointer to "struct path".
 * @flag:   Flags for open().
 *
 * Returns 0 on success, negative value otherwise.
 */
int tomoyo_check_open_permission(struct tomoyo_domain_info *domain,
				 struct path *path, const int flag)
{
	const u8 acc_mode = ACC_MODE(flag);
	int error = 0;
	struct tomoyo_path_info buf;
	struct tomoyo_request_info r;
	int idx;

	if (!path->mnt)
		return 0;
	buf.name = NULL;
	r.mode = TOMOYO_CONFIG_DISABLED;
	idx = tomoyo_read_lock();
	if (acc_mode &&
	    tomoyo_init_request_info(&r, domain, TOMOYO_MAC_FILE_OPEN)
	    != TOMOYO_CONFIG_DISABLED) {
		if (!tomoyo_get_realpath(&buf, path)) {
			error = -ENOMEM;
			goto out;
		}
		if (acc_mode & MAY_READ)
			error = tomoyo_path_permission(&r, TOMOYO_TYPE_READ,
						       &buf);
		if (!error && (acc_mode & MAY_WRITE))
			error = tomoyo_path_permission(&r, (flag & O_APPEND) ?
						       TOMOYO_TYPE_APPEND :
						       TOMOYO_TYPE_WRITE,
						       &buf);
	}
 out:
	kfree(buf.name);
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

/**
 * tomoyo_path_perm - Check permission for "unlink", "rmdir", "truncate", "symlink", "append", "chroot" and "unmount".
 *
 * @operation: Type of operation.
 * @path:      Pointer to "struct path".
 *
 * Returns 0 on success, negative value otherwise.
 */
int tomoyo_path_perm(const u8 operation, struct path *path)
{
	struct tomoyo_request_info r;
	int error;
	struct tomoyo_path_info buf;
	bool is_enforce;
	int idx;

	if (!path->mnt)
		return 0;
	if (tomoyo_init_request_info(&r, NULL, tomoyo_p2mac[operation])
	    == TOMOYO_CONFIG_DISABLED)
		return 0;
	is_enforce = (r.mode == TOMOYO_CONFIG_ENFORCING);
	error = -ENOMEM;
	buf.name = NULL;
	idx = tomoyo_read_lock();
	if (!tomoyo_get_realpath(&buf, path))
		goto out;
	switch (operation) {
	case TOMOYO_TYPE_RMDIR:
	case TOMOYO_TYPE_CHROOT:
		tomoyo_add_slash(&buf);
		break;
	}
	error = tomoyo_path_permission(&r, operation, &buf);
 out:
	kfree(buf.name);
	tomoyo_read_unlock(idx);
	if (!is_enforce)
		error = 0;
	return error;
}

/**
 * tomoyo_mkdev_perm - Check permission for "mkblock" and "mkchar".
 *
 * @operation: Type of operation. (TOMOYO_TYPE_MKCHAR or TOMOYO_TYPE_MKBLOCK)
 * @path:      Pointer to "struct path".
 * @mode:      Create mode.
 * @dev:       Device number.
 *
 * Returns 0 on success, negative value otherwise.
 */
int tomoyo_mkdev_perm(const u8 operation, struct path *path,
		      const unsigned int mode, unsigned int dev)
{
	struct tomoyo_request_info r;
	int error = -ENOMEM;
	struct tomoyo_path_info buf;
	int idx;

	if (!path->mnt ||
	    tomoyo_init_request_info(&r, NULL, tomoyo_pnnn2mac[operation])
	    == TOMOYO_CONFIG_DISABLED)
		return 0;
	idx = tomoyo_read_lock();
	error = -ENOMEM;
	if (tomoyo_get_realpath(&buf, path)) {
		dev = new_decode_dev(dev);
		r.param_type = TOMOYO_TYPE_MKDEV_ACL;
		r.param.mkdev.filename = &buf;
		r.param.mkdev.operation = operation;
		r.param.mkdev.mode = mode;
		r.param.mkdev.major = MAJOR(dev);
		r.param.mkdev.minor = MINOR(dev);
		tomoyo_check_acl(&r, tomoyo_check_mkdev_acl);
		error = tomoyo_audit_mkdev_log(&r);
		kfree(buf.name);
	}
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

/**
 * tomoyo_path2_perm - Check permission for "rename", "link" and "pivot_root".
 *
 * @operation: Type of operation.
 * @path1:      Pointer to "struct path".
 * @path2:      Pointer to "struct path".
 *
 * Returns 0 on success, negative value otherwise.
 */
int tomoyo_path2_perm(const u8 operation, struct path *path1,
		      struct path *path2)
{
	int error = -ENOMEM;
	struct tomoyo_path_info buf1;
	struct tomoyo_path_info buf2;
	struct tomoyo_request_info r;
	int idx;

	if (!path1->mnt || !path2->mnt ||
	    tomoyo_init_request_info(&r, NULL, tomoyo_pp2mac[operation])
	    == TOMOYO_CONFIG_DISABLED)
		return 0;
	buf1.name = NULL;
	buf2.name = NULL;
	idx = tomoyo_read_lock();
	if (!tomoyo_get_realpath(&buf1, path1) ||
	    !tomoyo_get_realpath(&buf2, path2))
		goto out;
	switch (operation) {
		struct dentry *dentry;
	case TOMOYO_TYPE_RENAME:
        case TOMOYO_TYPE_LINK:
		dentry = path1->dentry;
	        if (!dentry->d_inode || !S_ISDIR(dentry->d_inode->i_mode))
                        break;
                /* fall through */
        case TOMOYO_TYPE_PIVOT_ROOT:
                tomoyo_add_slash(&buf1);
                tomoyo_add_slash(&buf2);
		break;
        }
	r.param_type = TOMOYO_TYPE_PATH2_ACL;
	r.param.path2.operation = operation;
	r.param.path2.filename1 = &buf1;
	r.param.path2.filename2 = &buf2;
	do {
		tomoyo_check_acl(&r, tomoyo_check_path2_acl);
		error = tomoyo_audit_path2_log(&r);
	} while (error == TOMOYO_RETRY_REQUEST);
 out:
	kfree(buf1.name);
	kfree(buf2.name);
	tomoyo_read_unlock(idx);
	if (r.mode != TOMOYO_CONFIG_ENFORCING)
		error = 0;
	return error;
}

/**
 * tomoyo_same_mount_acl - Check for duplicated "struct tomoyo_mount_acl" entry.
 *
 * @a: Pointer to "struct tomoyo_acl_info".
 * @b: Pointer to "struct tomoyo_acl_info".
 *
 * Returns true if @a == @b, false otherwise.
 */
static bool tomoyo_same_mount_acl(const struct tomoyo_acl_info *a,
				  const struct tomoyo_acl_info *b)
{
	const struct tomoyo_mount_acl *p1 = container_of(a, typeof(*p1), head);
	const struct tomoyo_mount_acl *p2 = container_of(b, typeof(*p2), head);
	return tomoyo_same_name_union(&p1->dev_name, &p2->dev_name) &&
		tomoyo_same_name_union(&p1->dir_name, &p2->dir_name) &&
		tomoyo_same_name_union(&p1->fs_type, &p2->fs_type) &&
		tomoyo_same_number_union(&p1->flags, &p2->flags);
}

/**
 * tomoyo_update_mount_acl - Write "struct tomoyo_mount_acl" list.
 *
 * @param: Pointer to "struct tomoyo_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_mount_acl(struct tomoyo_acl_param *param)
{
	struct tomoyo_mount_acl e = { .head.type = TOMOYO_TYPE_MOUNT_ACL };
	int error;
	if (!tomoyo_parse_name_union(param, &e.dev_name) ||
	    !tomoyo_parse_name_union(param, &e.dir_name) ||
	    !tomoyo_parse_name_union(param, &e.fs_type) ||
	    !tomoyo_parse_number_union(param, &e.flags))
		error = -EINVAL;
	else
		error = tomoyo_update_domain(&e.head, sizeof(e), param,
					     tomoyo_same_mount_acl, NULL);
	tomoyo_put_name_union(&e.dev_name);
	tomoyo_put_name_union(&e.dir_name);
	tomoyo_put_name_union(&e.fs_type);
	tomoyo_put_number_union(&e.flags);
	return error;
}

/**
 * tomoyo_write_file - Update file related list.
 *
 * @param: Pointer to "struct tomoyo_acl_param".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_write_file(struct tomoyo_acl_param *param)
{
	u16 perm = 0;
	u8 type;
	const char *operation = tomoyo_read_token(param);
	for (type = 0; type < TOMOYO_MAX_PATH_OPERATION; type++)
		if (tomoyo_permstr(operation, tomoyo_path_keyword[type]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_path_acl(perm, param);
	for (type = 0; type < TOMOYO_MAX_PATH2_OPERATION; type++)
		if (tomoyo_permstr(operation, tomoyo_path2_keyword[type]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_path2_acl(perm, param);
	for (type = 0; type < TOMOYO_MAX_PATH_NUMBER_OPERATION; type++)
		if (tomoyo_permstr(operation,
				   tomoyo_path_number_keyword[type]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_path_number_acl(perm, param);
	for (type = 0; type < TOMOYO_MAX_MKDEV_OPERATION; type++)
		if (tomoyo_permstr(operation, tomoyo_mkdev_keyword[type]))
			perm |= 1 << type;
	if (perm)
		return tomoyo_update_mkdev_acl(perm, param);
	if (tomoyo_permstr(operation, "mount"))
		return tomoyo_update_mount_acl(param);
	return -EINVAL;
}
